/*-------------------------------------------------------------------------
 *
 * nodeGroup.h
 *	  prototypes for nodeGroup.c
 *
 *
 * Copyright (c) 1994, Regents of the University of California
 *
 * $Id: nodeGroup.h,v 1.10 1999/05/25 16:13:55 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEGROUP_H
#define NODEGROUP_H

#include "executor/tuptable.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

extern TupleTableSlot *ExecGroup(Group *node);
extern bool ExecInitGroup(Group *node, EState *estate, Plan *parent);
extern int	ExecCountSlotsGroup(Group *node);
extern void ExecEndGroup(Group *node);
extern void ExecReScanGroup(Group *node, ExprContext *exprCtxt, Plan *parent);

/***S*I***/
extern void ExecReScanGroup(Group *node, ExprContext *exprCtxt, Plan *parent);


#endif	 /* NODEGROUP_H */
