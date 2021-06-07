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
#include "eppic.h"
#include "eppic.tab.h"
#include <stdarg.h>
#include <setjmp.h>

#define MAXPARMS 10

typedef struct stat {

    int stype;
    int np;
    struct stat *next;
    srcpos_t pos;
    node_t*n;
    node_t*parms[MAXPARMS];
    var_t*svs;  /* if statement block then these are the auto and static
               wars for it */
    var_t*avs;

} stat;

#define SETVS   value_t *v1=0,*v2=0,*v3=0,*v4=0
#define FV1 eppic_freeval(v1),v1=0
#define FV2 eppic_freeval(v2),v2=0
#define FV3 eppic_freeval(v3),v3=0
#define FV4 eppic_freeval(v4),v4=0
#define UNSETVS FV1,FV2,FV3,FV4

#define P1 (s->parms[0])
#define P2 (s->parms[1])
#define P3 (s->parms[2])
#define P4 (s->parms[3])

#define V1 (v1?v1:(v1=NODE_EXE(P1)))
#define V2 (v2?v2:(v2=NODE_EXE(P2)))
#define V3 (v3?v3:(v3=NODE_EXE(P3)))
#define V4 (v4?v4:(v4=NODE_EXE(P4)))

#define L1 (unival(V1))
#define L2 (unival(V2))
#define L3 (unival(V3))
#define L4 (unival(V4))

#define S1 (V1->v.data)
#define S2 (V2->v.data)
#define S3 (V3->v.data)
#define S4 (V4->v.data)

/* this is used to execute staement lists e.g. i=1,j=3; */
static value_t*
eppic_exeplist(node_t*n)
{
value_t *val=0;

    if(n) {

        do {

            if(val) eppic_freeval(val), val=0;
            val=NODE_EXE(n);
            n=n->next;

        } while(n);
    }
    return val;
}

static int
eppic_dofor(stat *s)
{
jmp_buf brkenv;
jmp_buf cntenv;
SETVS;

    if(!setjmp(brkenv)) {

        eppic_pushjmp(J_BREAK, &brkenv, 0);

        v1=eppic_exeplist(P1);
        FV1;

        while(1) {
            v2=eppic_exeplist(P2);
            if(!P2 || eppic_bool(V2)) {

                FV2;

                if(!setjmp(cntenv)) {

                    if(P4) {
                        eppic_pushjmp(J_CONTINUE, &cntenv, 0);
                        V4;
                        FV4;
                        eppic_popjmp(J_CONTINUE);
                    }
                }

                UNSETVS; /* make sure we re-execute everything each time */
                v3=eppic_exeplist(P3);
                FV3;
            }
            else break;
        }
        eppic_popjmp(J_BREAK);
    }
    UNSETVS;
    return 1;
}

static int
eppic_dowhile(stat *s)
{
jmp_buf brkenv;
jmp_buf cntenv;
SETVS;

    if(!setjmp(brkenv)) {

        eppic_pushjmp(J_BREAK, &brkenv, 0);

        while(eppic_bool(V1)) {

            FV1;

            if(!setjmp(cntenv)) {

                eppic_pushjmp(J_CONTINUE, &cntenv, 0);
                V2;
                FV2;
                eppic_popjmp(J_CONTINUE);

            }
            
            UNSETVS; /* make sure we re-execute everything each time */
        }
        FV1;
        eppic_popjmp(J_BREAK);
        
    }

    return 1;
}

static int
eppic_dodo(stat *s)
{
jmp_buf brkenv;
jmp_buf cntenv;
SETVS;

    if(!setjmp(brkenv)) {

        eppic_pushjmp(J_BREAK, &brkenv, 0);

        do {

            FV2;
            if(!setjmp(cntenv)) {

                eppic_pushjmp(J_CONTINUE, &cntenv, 0);
                V1;
                FV1;
                eppic_popjmp(J_CONTINUE);

            }
            
            UNSETVS; /* make sure we re-execute everything each time */

        } while (eppic_bool(V2));
        FV2;

        eppic_popjmp(J_BREAK);

    }

    UNSETVS;
    return 1;
}

static int
eppic_doif(stat *s)
{
SETVS;
ul b;

    b=eppic_bool(V1);
    FV1;

    if(s->np==3) {

        if (b) 
            V2;
        else 
            V3;

    } else {

        if (b) 
            V2;

    }

    UNSETVS;
    return 1;
}

static int
eppic_doswitch(stat *s)
{
jmp_buf brkenv;
ull cval;
SETVS;

    if(!setjmp(brkenv)) {

        eppic_pushjmp(J_BREAK, &brkenv, 0);
        cval=unival(V1);
        FV1;
        eppic_docase(cval, P2->data);
        eppic_popjmp(J_BREAK);

    }

    UNSETVS;
    return 1;
}

static void
eppic_exein(stat *s)
{
jmp_buf cntenv;
SETVS;

    if(!setjmp(cntenv)) {

        eppic_pushjmp(J_CONTINUE, &cntenv, 0);
        V3;
        eppic_popjmp(J_CONTINUE);

    }
    UNSETVS;
}

static int
eppic_doin(stat *s)
{
jmp_buf brkenv;
    if(!setjmp(brkenv)) {

        eppic_pushjmp(J_BREAK, &brkenv, 0);
        eppic_walkarray(P1, P2, (void (*)(void *))eppic_exein, s);
        eppic_popjmp(J_BREAK);
    }
    return 1;
}

/* this is where all of the flow control takes place */

static value_t*
eppic_exestat(stat *s)
{
srcpos_t p;
value_t *val=0;

    do {

        /* dump the val while looping */
        if(val) eppic_freeval(val);
        val=0;

        eppic_curpos(&s->pos, &p);


        switch(s->stype) {

        case FOR :  eppic_dofor(s); break;
        case WHILE:     eppic_dowhile(s); break;
        case IN:    eppic_doin(s); break;
        case IF:    eppic_doif(s); break;
        case DO:    eppic_dodo(s); break;
        case SWITCH:    eppic_doswitch(s); break;
        case DOBLK:
        {
        int lev;

            /* add any static variables to the current context */
            lev=eppic_addsvs(S_STAT, s->svs);
            eppic_addsvs(S_AUTO, eppic_dupvlist(s->avs));

            /* with the block statics inserted exeute the inside stmts */
            if(s->next) val=eppic_exestat(s->next);

            /* remove any static variables to the current context */
            if(s->svs) eppic_setsvlev(lev);

            eppic_curpos(&p, 0);

            return val;
        }

        case BREAK: eppic_dojmp(J_BREAK, 0); break;
        case CONTINUE:  eppic_dojmp(J_CONTINUE, 0); break;
        case RETURN: {


            if(s->parms[0]) {

                val=(s->parms[0]->exe)(s->parms[0]->data);
            }
            else val=eppic_newval();

            eppic_curpos(&p, 0);
            eppic_dojmp(J_RETURN, val);
        }
        break;
        case PATTERN:

            val=eppic_exeplist(s->parms[0]);

        }

        eppic_curpos(&p, 0);

    } while((s=s->next));

    /* we most return a type val no mather what it is */
    /* that's just the way it is...Somethings will never change...*/
    if(!val) val=eppic_newval();

    return val;
}

void
eppic_freestat(stat *s)
{
int i;

    if(s->next) eppic_freenode(s->next->n);

    for(i=0;i<s->np && s->parms[i];i++) {

        NODE_FREE(s->parms[i]);

    }
    eppic_free(s);
}

void
eppic_freestat_static(stat *s)
{

    if(s->next) eppic_freenode(s->next->n);

    /* free associated static var list */
    eppic_freesvs(s->svs);
    eppic_freesvs(s->avs);
    eppic_free(s);
}

var_t*eppic_getsgrp_avs(node_t*n) { return ((stat *)n->data)->avs; }
var_t*eppic_getsgrp_svs(node_t*n) { return ((stat *)n->data)->svs; }

/* add a set of static variable to a statement */
node_t*
eppic_stat_decl(node_t*n, var_t*svs)
{
node_t*nn;
stat *s;

    eppic_validate_vars(svs);

    nn=eppic_newnode();
    s=eppic_alloc(sizeof(stat));

    /* add statics and autos to this statement */
    s->svs=eppic_newvlist();
    s->avs=eppic_newvlist();
    eppic_addnewsvs(s->avs, s->svs, svs);

    if(n) s->next=(stat*)(n->data);
    else s->next=0;
    s->stype=DOBLK;
    s->n=nn;
    nn->exe=(xfct_t)eppic_exestat;
    nn->free=(ffct_t)eppic_freestat_static;
    nn->data=s;
    eppic_setpos(&s->pos);

    return nn;
}

node_t*
eppic_newstat(int type, int nargs, ...)
{
va_list ap;
node_t*n=eppic_newnode();
stat *s=eppic_alloc(sizeof(stat));
int i;

    s->stype=type;

    va_start(ap, nargs);

    for(i=0;i<nargs && i<MAXPARMS; i++) {

        s->parms[i]=va_arg(ap, node_t*);
    }

    s->np=i;
    s->n=n;
        s->next=0;
    n->exe=(xfct_t)eppic_exestat;
    n->free=(ffct_t)eppic_freestat;
    n->data=s;
    
    eppic_setpos(&s->pos);
    
    va_end(ap);
    return n;
}

node_t*
eppic_addstat(node_t*list, node_t*s)
{
    if(!s && list) return list;
    if(s && !list) return s;
    else {
        stat *sp=(stat*)(list->data);

        while(sp->next) sp=sp->next;
        sp->next=(stat*)(s->data);
        return list;

    }
}

