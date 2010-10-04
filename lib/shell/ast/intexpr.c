/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <esc/common.h>
#include <stdio.h>
#include "intexpr.h"
#include "node.h"
#include "../mem.h"

sASTNode *ast_createIntExpr(tIntType val) {
	sASTNode *node = (sASTNode*)emalloc(sizeof(sASTNode));
	sIntExpr *expr = node->data = emalloc(sizeof(sIntExpr));
	expr->val = val;
	node->type = AST_INT_EXPR;
	return node;
}

sValue *ast_execIntExpr(sEnv *e,sIntExpr *n) {
	UNUSED(e);
	return val_createInt(n->val);
}

void ast_printIntExpr(sIntExpr *s,uint layer) {
	UNUSED(layer);
	printf("%d",s->val);
}

void ast_destroyIntExpr(sIntExpr *n) {
	/* nothing to do */
	UNUSED(n);
}
