/*-------------------------------------------------------------------------
 *
 * copy.h
 *	  Definitions for using the POSTGRES copy command.
 *
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/commands/copy.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef COPY_H
#define COPY_H

#include "nodes/execnodes.h"
#include "nodes/parsenodes.h"
#include "tcop/dest.h"
#include "executor/executor.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbcopy.h"

/*
 * Represents the different source/dest cases we need to worry about at
 * the bottom level
 */
typedef enum CopyDest
{
	COPY_FILE,					/* to/from file (or a piped program) */
	COPY_OLD_FE,				/* to/from frontend (2.0 protocol) */
	COPY_NEW_FE,				/* to/from frontend (3.0 protocol) */
	COPY_CALLBACK				/* to/from callback function (used for external tables) */
} CopyDest;

/* CopyStateData is private in commands/copy.c */
typedef struct CopyStateData *CopyState;
typedef int (*copy_data_source_cb) (void *outbuf, int datasize);

/*
 *	Represents the end-of-line terminator type of the input
 */
typedef enum EolType
{
	EOL_UNKNOWN,
	EOL_NL,
	EOL_CR,
	EOL_CRNL
} EolType;

/*
 * There are several ways to know the input row number where an error happened,
 * in order to report it reliably.
 *
 * ROWNUM_ORIGINAL - Used when the error happens in the same place that the
 * *whole* file or data is read, and input row numbers can be tracked reliably.
 * So, if an error happens in the n'th data row read, it's actually the n'th row
 * of the file (or data stream).
 * Using this type are - COPY in dispatch mode, COPY in utility mode, External
 * tables with 'file', and external web tables.
 *
 * ROWNUM_EMBEDDED - Used when the error happens in a place where a random
 * part of the file is read (given to us) and therefore the row numbers of the
 * original data are unknown.
 * So, If an error happens in the n'th row read it can actually be the (n+m)'th
 * row of the original file.
 * Using this type are - COPY in execute mode, and External tables getting data
 * from a gpfdist process in a CSV Format (gpfdist only parses CSV row by row,
 * while text format is parsed in chucks).
 * In this case we do some extra work to retrieve the original row number - the
 * distributor (COPY dispatcher, or gpfdist) embeds the original row number in
 * the beginning of each data row, and this number is extracted later on.
 *
 * BYTENUM_EMBEDDED - Original row isn't even known to the distributor, only
 * the byte offset of each chunk it sends. We report errors in byte offset
 * number, not row number. We keep track of byte counts. This is currently
 * only used by external tables with gpfdist in 'text' format.
 *
 */
typedef enum ErrLocType
{
	ROWNUM_ORIGINAL,
	ROWNUM_EMBEDDED,
	BYTENUM_EMBEDDED
} ErrLocType;


/*
 * The error handling mode for this data load.
 */
typedef enum CopyErrMode
{
	ALL_OR_NOTHING,	/* Either all rows or no rows get loaded (the default) */
	SREH_IGNORE,	/* Sreh - ignore errors (REJECT, but don't log errors) */
	SREH_LOG		/* Sreh - log errors */
} CopyErrMode;

typedef struct ProgramPipes
{
	char *shexec;
	int pipes[2];
	int pid;
} ProgramPipes;

/*
 *
 *
 **

COPY FROM modes (from file/client to table)

1. "normal" mode. This means ON SEGMENT, or utility mode, or non-distributed table in QD

2. Dispatcher mode. We are reading file/client, and forwarding all data to QEs
3. Executor mode. We are receiving pre-processed data from QD, and inserting to table.

COPY TO modes (table/query to file/client)

 */

typedef enum
{
	COPY_DIRECT,
	COPY_DISPATCH,
	COPY_EXECUTOR
} CopyDispatchMode;

/*
 * This struct contains all the state variables used throughout a COPY
 * operation. For simplicity, we use the same struct for all variants of COPY,
 * even though some fields are used in only some cases.
 *
 * Multi-byte encodings: all supported client-side encodings encode multi-byte
 * characters by having the first byte's high bit set. Subsequent bytes of the
 * character can have the high bit not set. When scanning data in such an
 * encoding to look for a match to a single-byte (ie ASCII) character, we must
 * use the full pg_encoding_mblen() machinery to skip over multibyte
 * characters, else we might find a false match to a trailing byte. In
 * supported server encodings, there is no possibility of a false match, and
 * it's faster to make useless comparisons to trailing bytes than it is to
 * invoke pg_encoding_mblen() to skip over them. encoding_embeds_ascii is TRUE
 * when we have to do it the hard way.
 */
typedef struct CopyStateData
{
	/* low-level state data */
	CopyDest	copy_dest;		/* type of copy source/destination */
	FILE	   *copy_file;		/* used if copy_dest == COPY_FILE */
	StringInfo	fe_msgbuf;		/* used for all dests during COPY TO, only for
								 * dest == COPY_NEW_FE in COPY FROM */
	bool		fe_eof;			/* true if detected end of copy data */
	EolType		eol_type;		/* EOL type of input */
	char	   *eol_str;		/* optional NEWLINE from command. before eol_type is defined */
	int			file_encoding;	/* file or remote side's character encoding */
	bool		need_transcoding;		/* file encoding diff from server? */
	bool		encoding_embeds_ascii;	/* ASCII can be non-first byte? */
	FmgrInfo   *enc_conversion_proc; /* conv proc from exttbl encoding to 
										server or the other way around */
	FmgrInfo   *custom_formatter_func; /* function to convert to custom format */
	char	   *custom_formatter_name; /* name of function to convert to custom format */
	List	   *custom_formatter_params; /* list of defelems that hold user's format parameters */
	size_t		bytesread;

	/* parameters from the COPY command */
	Relation	rel;			/* relation to copy to or from */
	GpPolicy	cdb_policy;		/* in ON SEGMENT mode, we received this from QD. Otherwise
								 * it's equal to rel->rd_cdbpolicy */
	QueryDesc  *queryDesc;		/* executable query to copy from */
	List	   *attnumlist;		/* integer list of attnums to copy */
	List	   *attnamelist;	/* list of attributes by name */
	char	   *filename;		/* filename, or NULL for STDIN/STDOUT */
	bool		is_program;		/* is 'filename' a program to popen? */
	copy_data_source_cb data_source_cb; /* function for reading data */
	bool		custom;			/* custom format? */
	bool		oids;			/* include OIDs? */
	bool        binary;         /* binary format */
	bool		csv_mode;		/* Comma Separated Value format? */
	bool		header_line;	/* CSV header line? */
	char	   *null_print;		/* NULL marker string (server encoding!) */
	int			null_print_len; /* length of same */
	char	   *null_print_client;		/* same converted to file encoding */
	char	   *delim;			/* column delimiter (must be 1 byte) */
	char	   *quote;			/* CSV quote char (must be 1 byte) */
	char	   *escape;			/* CSV escape char (must be 1 byte) */
	List	   *force_quote;	/* list of column names */
	bool		force_quote_all;	/* FORCE QUOTE *? */
	bool	   *force_quote_flags;		/* per-column CSV FQ flags */
	List	   *force_notnull;	/* list of column names */
	bool	   *force_notnull_flags;	/* per-column CSV FNN flags */
	bool		fill_missing;	/* missing attrs at end of line are NULL */

	CopyDispatchMode dispatch_mode;

	/* these are just for error messages, see CopyFromErrorCallback */
	const char *cur_relname;	/* table name for error messages */
	int64		cur_lineno;		/* line number for error messages.  Negative means it isn't available. */
	int64       cur_byteno;     /* number of bytes processed from input */
	const char *cur_attname;	/* current att for error messages */
	const char *cur_attval;		/* current att value for error messages */

	/*
	 * Working state for COPY TO/FROM
	 */
	MemoryContext copycontext;	/* per-copy execution context */

	/*
	 * Working state for COPY TO
	 */
	FmgrInfo   *out_functions;	/* lookup info for output functions */
	MemoryContext rowcontext;	/* per-row evaluation context */

	/*
	 * Working state for COPY FROM
	 */
	AttrNumber	num_defaults;
	bool		file_has_oids;
	FmgrInfo	oid_in_function;
	Oid			oid_typioparam;
	FmgrInfo   *in_functions;	/* array of input functions for each attrs */
	Oid		   *typioparams;	/* array of element types for in_functions */
	int		   *defmap;			/* array of default att numbers */
	ExprState **defexprs;		/* array of default att expressions */
	List	   *range_table;

	StringInfo	dispatch_msgbuf; /* used in COPY_DISPATCH mode, to construct message
								  * to send to QE. */
	
	/* Error handling options */
	CopyErrMode	errMode;
	struct CdbSreh *cdbsreh; /* single row error handler */
	int			num_consec_csv_err; /* # of consecutive csv invalid format errs */

	/*
	 * These variables are used to reduce overhead in textual COPY FROM.
	 *
	 * attribute_buf holds the separated, de-escaped text for each field of
	 * the current line.  The CopyReadAttributes functions return arrays of
	 * pointers into this buffer.  We avoid palloc/pfree overhead by re-using
	 * the buffer on each cycle.
	 */
	StringInfoData attribute_buf;

	/* field raw data pointers found by COPY FROM */

	int			max_fields;
	char	  **raw_fields;

	/*
	 * Similarly, line_buf holds the whole input line being processed. The
	 * input cycle is first to read the whole line into line_buf, convert it
	 * to server encoding there, and then extract the individual attribute
	 * fields into attribute_buf.  line_buf is preserved unmodified so that we
	 * can display it in error messages if appropriate.
	 */
	StringInfoData line_buf;

	int		   *attr_offsets;

	bool		line_buf_converted;		/* converted to server encoding? */

	/*
	 * Finally, raw_buf holds raw data read from the data source (file or
	 * client connection). CopyReadLine parses this data sufficiently to
	 * locate line boundaries, then transfers the data to line_buf and
	 * converts it.  Note: we guarantee that there is a \0 at
	 * raw_buf[raw_buf_len].
	 */

#define RAW_BUF_SIZE 65536		/* we palloc RAW_BUF_SIZE+1 bytes */
	char	   *raw_buf;
	int			raw_buf_index;	/* next byte to process */
	int			raw_buf_len;	/* total # of bytes stored */

	/* Greenplum Database specific variables */
	bool		is_copy_in;		/* copy in or out? */
	char		eol_ch[2];		/* The byte values of the 1 or 2 eol bytes */
	bool		escape_off;		/* treat backslashes as non-special? */
	bool		delimiter_off;  /* no delimiter. 1-column external tabs only */
	int			last_hash_field;
	bool		end_marker;
	char	   *begloc;
	char	   *endloc;
	bool		error_on_executor;		/* true if errors arrived from the
										 * executors COPY (QEs) */
	StringInfoData executor_err_context;		/* error context text from QE */


	/* for CSV format */
	bool		in_quote;
	bool		last_was_esc;

	/* for TEXT format */
	bool		esc_in_prevbuf; /* escape was last character of the data input
								 * buffer */
	bool		cr_in_prevbuf;	/* CR was last character of the data input
								 * buffer */

	/* Original row number tracking variables */
#define COPY_METADATA_DELIM '^'
	ErrLocType  err_loc_type;   /* see enum def for description */
	bool		md_error;
	
	PartitionNode *partitions; /* partitioning meta data from dispatcher */
	List		  *ao_segnos;  /* AO table meta data from dispatcher */
	bool          skip_ext_partition;  /* skip external partition */

	bool		on_segment; /* QE save data files locally */
	bool		ignore_extra_line; /* Don't count CSV header or binary trailer in
									  "processed" line number for on_segment mode*/
	ProgramPipes	*program_pipes; /* COPY PROGRAM pipes for data and stderr */
	/* end Greenplum Database specific variables */
} CopyStateData;

/*
 * Some platforms like macOS (since Yosemite) already define 64 bit versions
 * of htonl and nhohl so we need to guard against redefinition.
 */
#ifndef htonll
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#endif
#ifndef ntohll
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

extern uint64 DoCopy(const CopyStmt *stmt, const char *queryString);

extern void ProcessCopyOptions(CopyState cstate, bool is_from, List *options,
				   int num_columns, bool is_copy);
extern CopyState BeginCopyFrom(Relation rel, const char *filename,
			  bool is_program, copy_data_source_cb data_source_cb,
			  List *attnamelist, List *options, List *ao_segnos);
extern void EndCopyFrom(CopyState cstate);
extern bool NextCopyFrom(CopyState cstate, ExprContext *econtext,
						 Datum *values, bool *nulls, Oid *tupleOid, bool *got_error);
extern bool NextCopyFromRawFields(CopyState cstate,
					  char ***fields, int *nfields);

extern DestReceiver *CreateCopyDestReceiver(void);

extern List *CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist);
extern bool CopyReadLine(CopyState cstate);
extern void CopyOneRowTo(CopyState cstate, Oid tupleOid,
						 Datum *values, bool *nulls);
extern void CopyOneCustomRowTo(CopyState cstate, bytea *value);
extern void CopySendEndOfRow(CopyState cstate);
extern char *limit_printout_length(const char *str);
extern void truncateEol(StringInfo buf, EolType	eol_type);
extern void truncateEolStr(char *str, EolType eol_type);
extern void setEncodingConversionProc(CopyState cstate, int client_encoding, bool iswritable);
extern void CopyEolStrToType(CopyState cstate);

typedef struct GpDistributionData
{
	GpPolicy *policy;	/* the partitioning policy for this table */
	AttrNumber p_nattrs; /* num of attributes in the distribution policy */
	Oid *p_attr_types;   /* types for each policy attribute */
	CdbHash *cdbHash;
	HTAB *hashmap;
} GpDistributionData;

typedef struct PartitionData
{
	/* variables for partitioning */
	Datum *part_values ;
	Oid *part_attr_types; /* types for partitioning */
	Oid *part_typio ;
	FmgrInfo *part_infuncs ;
	AttrNumber *part_attnum ;
	int part_attnums ;
} PartitionData;

typedef struct GetAttrContext
{
	TupleDesc tupDesc;
	Form_pg_attribute *attr;
	AttrNumber num_phys_attrs;
	int *attr_offsets;
	bool *nulls;
	Datum *values;
	CdbCopy *cdbCopy;
	int original_lineno_for_qe;
} GetAttrContext;

typedef struct  cdbhashdata
{
	Oid relid;
	CdbHash *cdbHash; /* a CdbHash API object */
	GpPolicy *policy; /* policy for this cdb hash */
} cdbhashdata;


typedef struct
{
	/*
	 * target relation OID. Normally, the same as cstate->relid, but for
	 * a partitioned relation, it indicate the target partition.
	 */
	Oid			relid;

	Oid			loaded_oid;

	int64		lineno;

	int16		fld_count;

	/*
	 * Default values. For each default value:
	 * <data>
	 *
	 * The data is the raw Datum.
	 */

	/* data follows */
} copy_from_dispatch_frame;


#endif /* COPY_H */
