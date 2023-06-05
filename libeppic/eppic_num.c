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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eppic.h"

typedef struct {
    int type;
    ull val;
} num;

/*
        Numeric constants.
*/

static value_t*
eppic_exenum(num *n)
{
value_t *v=eppic_newval();

    v->type.type=V_BASE;
    v->type.idx=n->type;
    if(n->type==B_SLL) {

ll:
        v->v.sll=n->val;
        v->type.size=8;

    }else if(n->type==B_SC) {

        v->v.sc=n->val;
        v->type.size=1;

    } else {

        if(eppic_defbsize()==4) {

            v->v.sl=n->val;
            v->type.size=4;

        } else {

            v->type.idx=B_SLL;
            goto ll;
        }
    }
    v->type.typattr=eppic_idxtoattr(v->type.idx);
    v->set=0;
    return v;
}

void
eppic_freenumnode(num *n)
{
    eppic_free(n);
}

node_t*
eppic_makenum(int type, ull val)
{
node_t*n=eppic_newnode();
num *nu=eppic_alloc(sizeof(num));

    TAG(nu);

    nu->type=type;
    nu->val=val;
    n->exe=(xfct_t)eppic_exenum;
    n->free=(ffct_t)eppic_freenumnode;
    n->data=nu;

    eppic_setpos(&n->pos);
    return n;
}

/*
    Execution of the sizeof() operator.
    This sould be done at compile time, but I have not setup
    a 'type only' execution path for the nodes.
    Runtime is good enough to cover mos cases.
*/
#define SN_TYPE 1
#define SN_EXPR 2

typedef struct {
    int type;
    void *p;
    srcpos_t pos;
} snode_t;

static value_t *
eppic_exesnode(snode_t*sn)
{
srcpos_t pos;
type_t*t;
value_t *v=eppic_newval();
value_t *v2=0;
int size;

    eppic_curpos(&sn->pos, &pos);
    if(sn->type == SN_TYPE) {

        t=(type_t*)(sn->p);

    } else {

        eppic_setinsizeof(1);
        v2=NODE_EXE((node_t*)(sn->p));
        t=&v2->type;
        eppic_setinsizeof(0);
    }

    switch(t->type) {

        case V_REF:

            if(t->idxlst) {

                int i; 
                for(size=t->size,i=0;t->idxlst[i];i++) size *= t->idxlst[i];

            } else size=eppic_defbsize();

        break;
        case V_STRUCT: case V_UNION:

            if(eppic_ispartial(t)) {

                eppic_error("Invalid type specified");
            }
            size=t->size;

        break;
        case V_BASE: case V_STRING:
            size=t->size;
        break;
        
        default: size=0;
    }

    eppic_defbtype(v, (ull)size);

    eppic_curpos(&pos, 0);

    if(v2) eppic_freeval(v2);

    return v;
    
}

static void
eppic_freesnode(snode_t*sn)
{
    if(sn->type == SN_TYPE) eppic_free(sn->p);
    else NODE_FREE(sn->p);
    eppic_free(sn);  
}

type_t*
eppic_typeof(node_t *n)
{
type_t*t=eppic_newtype();
value_t *v2;

    eppic_setinsizeof(1);
    v2=NODE_EXE(n);
    eppic_setinsizeof(0);
    eppic_duptype(t, &v2->type);
    eppic_freeval(v2);
    eppic_freenode(n);
    return t;
}

node_t*
eppic_sizeof(void *p, int type)
{
node_t*n=eppic_newnode();
snode_t*sn=eppic_alloc(sizeof(snode_t));

    n->exe=(xfct_t)eppic_exesnode;
    n->free=(ffct_t)eppic_freesnode;
    n->data=sn;
    sn->type=type;
    sn->p=p;
    eppic_setpos(&sn->pos);
    return n;
}

node_t*
eppic_newnum(char *buf)
{
int type, idx, issigned=1, islonglong=0;
unsigned long long val;
char *endp;
int base;

    /* get the number base. Could be hex, octal or dec. */
    if (buf[0]=='0')
	if (buf[1]=='x') {
	    base=16;
	    buf+=2;
	} else
	    base=8;
    else
	base=10;

    /* treat the constant's suffix */
    for(idx=strlen(buf)-1;idx;idx--) {
	char c=buf[idx];

	if (c>='0' && c<='9')
	    break;

	if (c=='u' || c=='U') {
	    if (!issigned)
		goto error;
	    issigned=0;
	} else if (c=='l' || c=='L') {
	    if (islonglong)
		goto error;
	    /* ISO C does not permit mixed-case LL suffix. Clang gives
	     * an error, but GCC will happily parse them. Let's follow
	     * the robustness principle and be liberal in what we accept.
	     */
	    if (buf[idx-1]=='l' || buf[idx-1]=='L') {
		islonglong=2;
		idx--;
	    } else
		islonglong=1;
	}
    }

    /* get the value_t of this constant. */
    val = strtoull(buf,&endp,base);
    if(endp!=buf+idx+1) goto error;

    if(issigned) {
    
        if(eppic_defbsize()==8 || islonglong == 2)
            type=B_SLL;
        else 
            type=B_SL;
    
    }
    else {
    
        if(eppic_defbsize()==8 || islonglong == 2)
            type=B_ULL;
        else 
            type=B_UL;
    
    }
    {
        node_t*n=eppic_makenum(type, val);
        TAG(n->data);
        return n;
    }
error:
    eppic_error("Oops! NUMBER");
    return 0;
}
