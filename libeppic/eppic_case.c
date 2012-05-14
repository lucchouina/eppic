/*
 * Copyright 2001 Silicon Graphics, Inc. All rights reserved.
 * Copyright 2002-2012 Luc Chouinard. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*
	Set of functions to handle the case construct.
*/
#include "eppic.h"

void
eppic_freecaseval(caseval_t*cv)
{
	eppic_free(cv);
}

node_t*
eppic_caseval(int isdef, node_t*val)
{
caseval_t*cv=eppic_alloc(sizeof(caseval_t));
node_t*n=eppic_newnode();
value_t *v;

	cv->isdef=isdef;
	if(val) {

		v=NODE_EXE(val);
		cv->val=unival(v);
		eppic_freeval(v);
		NODE_FREE(val);

	} else cv->val=0;

	eppic_setpos(&cv->pos);

	cv->next=0;
	n->data=cv;
	return n;
}

node_t*
eppic_addcaseval(node_t*n, node_t*n2)
{
caseval_t*cv=(caseval_t*)n->data;
caseval_t*ncv=(caseval_t*)n2->data;

	eppic_free(n);
	ncv->next=cv;
	return n2;
}

void
eppic_freecase(void *vcl)
{
caselist_t*cl=(caselist_t*)vcl;

	NODE_FREE(cl->stmt);
	eppic_free(cl);
}

node_t*
eppic_newcase(node_t*nc, node_t* n)
{
caseval_t*cv=(caseval_t*)nc->data;
caselist_t*cl=eppic_alloc(sizeof(caselist_t));
node_t*nn=eppic_newnode();


	nn->data=cl;
	nn->free=(ffct_t)eppic_freecase;

	cl->vals=cv;
	eppic_free(nc);

	cl->stmt=n;
	cl->next=0;

	eppic_setpos(&cl->pos);

	return nn;
}

node_t*
eppic_addcase(node_t*n, node_t*n2)
{
caselist_t*lcl;
caselist_t*ncl=(caselist_t*)n2->data;
caselist_t*cl=(caselist_t*)n->data;

	for(lcl=cl; lcl->next; lcl=lcl->next);

	/* we need to add case in the order they are listed */
	lcl->next=ncl;
	eppic_free(n2);
	ncl->next=0;

	eppic_setpos(&ncl->pos);

	return n;
}

int
eppic_docase(ull val, caselist_t*cl)
{
caselist_t*defclp=0, *clp;


	for(clp=cl;clp;clp=clp->next) {

	caseval_t*cvp;

		for(cvp=clp->vals; cvp; cvp=cvp->next) {

			if(cvp->val==val) goto out;
			else if(cvp->isdef) defclp=clp;
		}
	}
out:
	if(clp || (clp=defclp)) {

		for(;clp;clp=clp->next) {

			if(clp->stmt) NODE_EXE(clp->stmt);
		}
	}
	return 1;
}
