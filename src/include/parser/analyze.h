/*-------------------------------------------------------------------------
 *
 * analyze.h
 *		parse analysis for optimizable statements
 *
 *
<<<<<<< HEAD
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/analyze.h,v 1.34.2.1 2008/12/13 02:00:53 tgl Exp $
=======
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/analyze.h,v 1.38.2.1 2008/12/13 02:00:30 tgl Exp $
>>>>>>> 632e7b6353a99dd139b999efce4cb78db9a1e588
 *
 *-------------------------------------------------------------------------
 */
#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/parse_node.h"


<<<<<<< HEAD
/* fwd declarations */
struct GpPolicy;

extern List *parse_analyze(Node *parseTree, const char *sourceText,
=======
extern Query *parse_analyze(Node *parseTree, const char *sourceText,
>>>>>>> 632e7b6353a99dd139b999efce4cb78db9a1e588
			  Oid *paramTypes, int numParams);
extern Query *parse_analyze_varparams(Node *parseTree, const char *sourceText,
						Oid **paramTypes, int *numParams);
<<<<<<< HEAD
extern List *parse_sub_analyze(Node *parseTree, ParseState *parentParseState);
extern bool analyze_requires_snapshot(Node *parseTree);
extern List *analyzeCreateSchemaStmt(CreateSchemaStmt *stmt);
=======

extern Query *parse_sub_analyze(Node *parseTree, ParseState *parentParseState);
extern Query *transformStmt(ParseState *pstate, Node *parseTree);

extern bool analyze_requires_snapshot(Node *parseTree);

>>>>>>> 632e7b6353a99dd139b999efce4cb78db9a1e588
extern void CheckSelectLocking(Query *qry);
extern void applyLockingClause(Query *qry, Index rtindex,
				   bool forUpdate, bool noWait);

/* State shared by transformCreateStmt and its subroutines */
typedef struct
{
	const char *stmtType;		/* "CREATE TABLE" or "ALTER TABLE" */
	RangeVar   *relation;		/* relation to create */
	List	   *inhRelations;	/* relations to inherit from */
	bool		hasoids;		/* does relation have an OID column? */
	bool		isalter;		/* true if altering existing table */
	bool		isaddpart;		/* true if create in service of adding a part */
	List	   *columns;		/* ColumnDef items */
	List	   *ckconstraints;	/* CHECK constraints */
	List	   *fkconstraints;	/* FOREIGN KEY constraints */
	List	   *ixconstraints;	/* index-creating constraints */
	List	   *inh_indexes;	/* cloned indexes from INCLUDING INDEXES */
	List	   *blist;			/* "before list" of things to do before
								 * creating the table */
	List	   *alist;			/* "after list" of things to do after creating
								 * the table */
	List	   *dlist;			/* "deferred list" of utility statements to 
								 * transfer to the list CreateStmt->deferredStmts
								 * for later parse_analyze and dispatch */
	IndexStmt  *pkey;			/* PRIMARY KEY index, if any */
} CreateStmtContext;

Query *transformCreateStmt(ParseState *pstate, CreateStmt *stmt,
						   List **extras_before, List **extras_after);

int validate_partition_spec(ParseState 			*pstate,
							CreateStmtContext 	*cxt, 
							CreateStmt 			*stmt, 
							PartitionBy 		*partitionBy, 	
							char	   			*at_depth,
							int					 partNumber);

extern bool is_aocs(List *opts);

List *transformStorageEncodingClause(List *options);
List *TypeNameGetStorageDirective(TypeName *typname);
extern List * form_default_storage_directive(List *enc);

#endif   /* ANALYZE_H */
