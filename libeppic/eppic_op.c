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
#include <string.h>

#define MAXPARMS 10
#define DEBUG(fmt, ...) 
typedef struct {

    int op;         /* operator */
    int np;         /* number of operands */
    node_t*parms[MAXPARMS]; /* operands */

    srcpos_t pos;

} oper;

#define P1 (o->parms[0])
#define P2 (o->parms[1])
#define P3 (o->parms[2])
#define P4 (o->parms[3])

#define V1 (v1?v1:(v1=NODE_EXE(P1)))
#define V2 (v2?v2:(v2=NODE_EXE(P2)))
#define V3 (v3?v3:(v3=NODE_EXE(P3)))
#define V4 (v4?v4:(v4=NODE_EXE(P4)))

#define L1 (unival(V1))
#define L2 (unival(V2))
#define L3 (unival(V3))
#define L4 (unival(V4))

#define S1 ((V1)->v.data)
#define S2 ((V2)->v.data)
#define S3 ((V3)->v.data)
#define S4 ((V4)->v.data)

void eppic_do_deref(value_t *v, value_t *ref);
ul
eppic_bool(value_t *v)
{
    switch(v->type.type) {

    case V_BASE:
        switch(v->type.size) {
            case 1: return !(!(v->v.uc));
            case 2: return !(!(v->v.us));
            case 4: return !(!(v->v.ul));
            case 8: return !(!(v->v.ull));
            default: eppic_error("Oops eppic_bool()[%d]", v->type.size); break;
        }
    case V_STRING :     return !(!(*((char*)(v->v.data))));
    case V_REF:     return eppic_defbsize()==8?(!(!(v->v.ull))):(!(!(v->v.ul)));
    default :

        eppic_error("Invalid operand for boolean expression");
        return 0;
    }
}

static int cops[]={BAND,BOR,NOT,LT,LE,EQ,GE,GT,NE,CEXPR};
#define NCOPS (sizeof(cops)/sizeof(cops[0]))

static int
is_cond(int op)
{
int i;

    for(i=0;i<NCOPS;i++) {

        if(cops[i]==op) return 1;

    }
    return 0;
}

struct {
    int cur, equiv;
} cpls [] ={
    { ADDME, ADD },
    { SUBME, SUB },
    { DIVME, DIV },
    { MULME, MUL },
    { SHLME, SHL },
    { SHRME, SHR },
    { XORME, XOR },
    { ANDME, AND },
    { ORME,  OR  },
    { MODME, MOD },
};
#define NOPS (sizeof(cpls)/sizeof(cpls[0]))
/* get the equivalent operation ME type operators */
static int getop(int op)
{
int i;
    for(i=0;i<NOPS;i++) {

        if(cpls[i].cur==op) return cpls[i].equiv;

    }
    return op;
}

static void
eppic_transfer(value_t *v1, value_t *v2, ull rl)
{
    eppic_dupval(v1, v2);
    switch(TYPE_SIZE(&v1->type)) {

        case 1: v1->v.uc=rl; break;
        case 2: v1->v.us=rl; break;
        case 4: v1->v.ul=rl; break;
        case 8: v1->v.ull=rl; break;

    }
    /* the result of an assignment cannot be a lvalue_t */
    v1->set=0;
}

#define anyop(t) (V1->type.type==t || (o->np>1 && V2->type.type==t))

typedef struct {
    node_t*index;
    node_t*var;
    srcpos_t pos;
} index_t ;

static value_t *
eppic_exeindex(index_t  *i)
{
value_t *var;
value_t *vi=NODE_EXE(i->index);
value_t *v;
srcpos_t p;

    eppic_curpos(&i->pos, &p);

    /* we need to make believe it's been initiazed */
    eppic_setini(i->var);
    var=NODE_EXE(i->var);

    /* Anything with a vmcore adddress (saved in v->mem ? */
    if(var->mem) {

        int size;
        int n=eppic_getval(vi);
        value_t *ref;

        if(var->type.idxlst) {
        
            int size;
            
            if(var->type.ref) size=eppic_defbsize();
            else size=var->type.size;

            v=eppic_cloneval(var);

            if(var->type.idxlst[1]) {
                /* shift indexes left and reposition memory pointer accordingly */
                int i;

                v->type.idxlst[0]=0;
                for(i=1; var->type.idxlst[i]; i++) {

                    size *= var->type.idxlst[i];
                    v->type.idxlst[i]=var->type.idxlst[i+1];
                }
                v->mem+=size*n;
            }
            else {

                v->mem+=size*n;
                /* can free the last array of the replica */
                eppic_free(v->type.idxlst);
                v->type.idxlst=NULL;
                if(!eppic_type_isinvmcore(&v->type)) {
                    eppic_pushref(&v->type, 1);
                    eppic_do_deref(v, v);
                }
            }
        }
        else {

            v=eppic_newval();
            ref=eppic_cloneval(var);

            if(var->type.ref==1) size=var->type.size;
            else size=eppic_defbsize();

            ref->mem=ref->v.ull+size*n;
            eppic_do_deref(v, ref);
            eppic_freeval(ref);
        }

    } else {

        v=eppic_newval();

        /* use dynamic indexing aka awk indexing */
        eppic_valindex(var, vi, v);
    }

    /* discard expression results */
    eppic_freeval(var);
    eppic_freeval(vi);
    eppic_curpos(&p, 0);

    return v;
}

void
eppic_freeindex(index_t  *i)
{
    NODE_FREE(i->index);
    NODE_FREE(i->var);
    eppic_free(i);
}

node_t*
eppic_newindex(node_t*var, node_t*idx)
{
index_t  *i=eppic_alloc(sizeof(index_t ));
node_t*n=eppic_newnode();

    i->index=idx;
    i->var=var;
    n->exe=(xfct_t)eppic_exeindex;
    n->free=(ffct_t)eppic_freeindex;
    n->data=i;
    eppic_setpos(&i->pos);
    return n;
}

typedef struct {
    node_t*fname;
    node_t*parms;
    srcpos_t pos;
    void *file;
} call;

static value_t *
eppic_execall(call *c)
{
value_t *rv;
srcpos_t p;

    eppic_curpos(&c->pos, &p);
    rv=eppic_docall(c->fname, c->parms, c->file);
    eppic_curpos(&p, 0);
    return rv;
}

void
eppic_freecall(call *c)
{
    NODE_FREE(c->fname);
    eppic_free_siblings(c->parms);
    eppic_free(c);
}

node_t*
eppic_newcall(node_t* fname, node_t* parms)
{
node_t*n=eppic_newnode();
call *c=eppic_alloc(sizeof(call));

    c->fname=fname;
    c->file=eppic_getcurfile();
    c->parms=parms;
    n->exe=(xfct_t)eppic_execall;
    n->free=(ffct_t)eppic_freecall;
    n->data=c;
    eppic_setpos(&c->pos);
    return n;
}

typedef struct {
    node_t*expr;
    srcpos_t pos;
} adrof;

static value_t *
eppic_exeadrof(adrof *a)
{
value_t *rv, *v=NODE_EXE(a->expr);

    /* we can only do this op on something that came from system image
       Must not allow creation of references to local variable */
    if(!v->mem) {

        eppic_freeval(v);
        eppic_rerror(&a->pos, "Invalid operand to '&' operator");

    }
    /* create the reference */
    rv=eppic_newval();
    eppic_duptype(&rv->type, &v->type);
    eppic_pushref(&rv->type, 1);
    rv->mem=rv->v.ull=v->mem;
    eppic_freeval(v);

    return rv;
}

void
eppic_freeadrof(adrof *a)
{
    NODE_FREE(a->expr);
    eppic_free(a);
}

node_t*
eppic_newadrof(node_t* expr)
{
node_t*n=eppic_newnode();
adrof *a=eppic_alloc(sizeof(adrof));

    a->expr=expr;
    n->exe=(xfct_t)eppic_exeadrof;
    n->free=(ffct_t)eppic_freeadrof;
    n->data=a;
    eppic_setpos(&a->pos);
    return n;
}

static int
eppic_reftobase(value_t *v)
{
int idx= v->type.idx;

    if(v->type.type==V_REF) {

        if(eppic_defbsize()==4) 
            v->type.idx=B_UL;
        else 
            v->type.idx=B_ULL;
    }
    return idx;
}

static value_t*
eppic_docomp(int op, value_t *v1, value_t *v2)
{

    /* if one parameter is string then both must be */
    if(v1->type.type == V_STRING || v2->type.type == V_STRING) {

        if(v1->type.type != V_STRING || v2->type.type != V_STRING) {

            eppic_error("Invalid condition arguments");
        }
        else {

            switch(op) {

                case EQ: {  /* expr == expr */

                    return eppic_makebtype(!strcmp(v1->v.data, v2->v.data));

                }
                case GT: case GE: { /* expr > expr */

                    return eppic_makebtype(strcmp(v1->v.data, v2->v.data) > 0);

                }
                case LE: case LT: { /* expr <= expr */

                    return eppic_makebtype(strcmp(v1->v.data, v2->v.data) < 0);

                }
                case NE: {  /* expr != expr */

                    return eppic_makebtype(strcmp(v1->v.data, v2->v.data));

                }
                default: {

                    eppic_error("Oops conditional unknown 1");

                }
            }
        }

    }
    else {

        int idx1, idx2;
        value_t *v=eppic_newval();

        /* make sure pointers are forced to proper basetype
           before calling eppic_baseop()*/
        idx1=eppic_reftobase(v1);
        idx2=eppic_reftobase(v2);
        

        switch(op) {

            case EQ:
            case GT:
            case GE: 
            case LE:
            case LT:
            case NE:
                eppic_baseop(op, v1, v2, v);
            break;
            default: {

                eppic_error("Oops conditional unknown 2");

            }
        }
        v1->type.idx=idx1;
        v2->type.idx=idx2;
        return v;
    }
    return 0;
}

static value_t *
eppic_exeop(oper *o)
{
value_t *v=0, *v1=0, *v2=0, *v3=0, *v4=0;
int top;
srcpos_t p;

    eppic_curpos(&o->pos, &p);

    /* if ME (op on myself) operator, translate to normal operator
       we will re-assign onto self when done */

    top=getop(o->op);

    if(top == ASSIGN) {

        goto doop;

    } else if(top == IN) {

        /* the val in array[] test is valid for anything but struct/union */
        v=eppic_makebtype((ull)eppic_lookuparray(P1,P2));

    }
    else if(is_cond(top)) {

        /* the operands are eithr BASE (integer) or REF (pointer) */ 
        /* all conditional operators accept a mixture of pointers and integer */
        /* set the return as a basetype even if bool */

        switch(top) {

            case CEXPR: {   /* conditional expression expr ? : stmt : stmt */

                if(eppic_bool(V1)) {

                    v=eppic_cloneval(V2);

                } else {

                    v=eppic_cloneval(V3);

                }

            }
            break;
            case BOR: { /* a || b */

                v=eppic_makebtype((ull)(eppic_bool(V1) || eppic_bool(V2)));

            }
            break;
            case BAND: {    /* a && b */

                v=eppic_makebtype((ull)(eppic_bool(V1) && eppic_bool(V2)));

            }
            break;
            case NOT: { /* ! expr */

                v=eppic_makebtype((ull)(! eppic_bool(V1)));

            }
            break;
            default: {

                v=eppic_docomp(top, V1, V2);

            }
        }

    } else if(anyop(V_STRING)) {

        if(top == ADD) 
        {
        char *buf;

            if(V1->type.type != V_STRING || V2->type.type != V_STRING) {

                eppic_rerror(&P1->pos, "String concatenation needs two strings!");

            }
            buf=eppic_alloc(strlen(S1)+strlen(S2)+1);
            strcpy(buf, S1);
            strcat(buf, S2);
            v=eppic_makestr(buf);
            eppic_free(buf);
        }
        else {

            eppic_rerror(&P1->pos, "Invalid string operator");

        }
    }
    /* arithmetic operator */
    else if(anyop(V_REF)) { 

        int size;
        value_t *vt;

        /* make sure we have the base type second */
        if(V1->type.type != V_REF) { vt=V1; v1=V2; v2=vt; }


        if(V1->type.type == V_BASE) {
inval:
            eppic_error("Invalid operand on pointer operation");
        }

        /* get the size of whas we reference */
        size=V1->type.size;
    
        switch(top) {
            case ADD: { /* expr + expr */
                /* adding two pointers ? */
                if(V2->type.type == V_REF) goto inval;

                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) + L2 * size);
            }
            break;
            case SUB: { /* expr - expr */
                /* different results if mixed types.
                   if both are pointers then result is a V_BASE */
                if(V2->type.type == V_REF)
                    v=eppic_makebtype(L1 - L2);

                else {
                    V1;
                    eppic_transfer(v=eppic_newval(), v1,
                              unival(v1) - L2 * size);
                }
            }
            break;
            case PREDECR: { /* pre is easy */
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) - size);
                eppic_setval(v1, v);
            }
            break;
            case PREINCR: {
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) + size);
                eppic_setval(v1, v);
            }
            break;
            case POSTINCR: {
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) + size);
                eppic_setval(v1, v);
                eppic_transfer(v, v1, unival(v1));
            }
            break;
            case POSTDECR: {
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) - size);
                eppic_setval(v1, v);
                eppic_transfer(v, v1, unival(v1));
            }
            break;
            default:
                eppic_error("Invalid operation on pointer [%d]",top);
        }
    }
    else {

        /* both operands are V_BASE */
        switch(top) {

            /* for mod and div, we check for divide by zero */
            case MOD: case DIV:
                if(!L2) {
                    eppic_rerror(&P1->pos, "Mod by zero");
                }
            case ADD: case SUB: case MUL: case XOR: 
            case OR: case AND: case SHL: case SHR:
            {
                eppic_baseop(top, V1, V2, v=eppic_newval());
            }
            break;
            case UMINUS: {

                value_t *v0=eppic_newval();
                eppic_defbtype(v0, (ull)0);
                /* keep original type of v1 */
                v=eppic_newval();
                eppic_duptype(&v0->type, &V1->type);
                eppic_duptype(&v->type, &V1->type);
                eppic_baseop(SUB, v0, V1, v);
                eppic_freeval(v0);
                /* must make result signed */
                eppic_mkvsigned(v);
            }
            break;
            case FLIP: {

                value_t *v0=eppic_newval();
                eppic_defbtype(v0, (ull)0xffffffffffffffffll);
                /* keep original type of v1 */
                eppic_duptype(&v0->type, &V1->type);
                eppic_baseop(XOR, v0, V1, v=eppic_newval());
                eppic_freeval(v0);
            }
            break;
            case PREDECR: { /* pre is easy */
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) - 1);
                eppic_setval(v1, v);
            }
            break;
            case PREINCR: {
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) + 1);
                eppic_setval(v1, v);
            }
            break;
            case POSTINCR: {
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) + 1);
                eppic_setval(v1, v);
                eppic_transfer(v, v1, unival(v1));
            }
            break;
            case POSTDECR: {
                V1;
                eppic_transfer(v=eppic_newval(), v1,
                          unival(v1) - 1);
                eppic_setval(v1, v);
                eppic_transfer(v, v1, unival(v1));
            }
            break;
            default: eppic_rerror(&P1->pos, "Oops ops ! [%d]", top);
        }
    }
doop:
    /* need to assign the value_t back to P1 */
    if(top != o->op || top==ASSIGN) {

        /* in the case the Lvalue_t is a variable , bypass execution and set ini */
        if(P1->exe == eppic_exevar) {

            char *name=NODE_NAME(P1);
            var_t*va=eppic_getvarbyname(name, 0, 0);
            value_t *vp;

            eppic_free(name);
            
            if(!va) eppic_error("Variable is undefined '%s'", name);

            if(top != o->op) vp=v;
            else vp=V2;

	    if (va == NULL)
		    eppic_error("bad assigment error - undef var");
            eppic_chkandconvert(va->v, vp);

            eppic_freeval(v);
            v=eppic_cloneval(va->v);
            va->ini=1;

        } else {

            if(!(V1->set)) {

                eppic_rerror(&P1->pos, "Not Lvalue_t on assignment");

            }
            else {

                /* if it's a Me-op then v is already set */
                V1;
                if(top != o->op) {
                    eppic_setval(v1, v);
                } else {
                    eppic_setval(v1, V2);
                    v=eppic_cloneval(V2);
                }

            }
        }
        /* the result of a assignment if not an Lvalue_t */
        v->set=0;
    }
    eppic_freeval(v1);
    eppic_freeval(v2);
    eppic_freeval(v3);
    eppic_freeval(v4);
    eppic_setpos(&p);
    return v;
}

void
eppic_freeop(oper *o)
{
int i;

    for(i=0;i<o->np;i++) NODE_FREE(o->parms[i]);
    eppic_free(o);
}

node_t*
eppic_newop(int op, int nargs, ...)
{
va_list ap;
node_t*n=eppic_newnode();
oper *o=eppic_alloc(sizeof(oper));
int i;

    o->op=op;
    o->np=nargs;

    eppic_setpos(&o->pos);

    va_start(ap, nargs);

    for(i=0 ; i<MAXPARMS; i++) {

        if(!(o->parms[i]=va_arg(ap, node_t*))) break;;
    }

    n->exe=(xfct_t)eppic_exeop;
    n->free=(ffct_t)eppic_freeop;
    n->data=o;
    
    va_end(ap);
    return n;
}

/* mult is a special case since the parse always return a PTR token 
   for the '*' signed. The PTR token value_t is the number of '* found.
*/
node_t*
eppic_newmult(node_t*n1, node_t*n2, int n)
{
    if(n>1) {

        eppic_error("Syntax error");
    }
    return eppic_newop(MUL, 2, n1, n2);
}
/*
    This function is called when we want to set a value_t in live memory
    using a pointer to it.
*/
static void
eppic_setderef(value_t *v1, value_t *v2)
{
    void *eppic_adrval(value_t *);
    eppic_transval(v2->type.size, v1->type.size, v2, eppic_issigned(v2->type.typattr));
    API_PUTMEM(v1->mem, eppic_adrval(v2), v2->type.size);
}

/*
    Do a de-referencing from a pointer (ref) and put the result in v.
*/
void
eppic_do_deref(value_t *v, value_t *ref)
{
ull madr, new_madr;

    if(! ref->type.ref) {

        eppic_error("Too many levels of dereference");

    }else {
    

        madr=ref->mem;
        eppic_duptype(&v->type, &ref->type);
        eppic_popref(&v->type, 1);

        if(!v->type.ref) {

            /* for arrays and ctypes we do nothing. Indexing of member access will to the actual access */
            if(eppic_type_isinvmcore(&v->type)) {
            
                eppic_dbg(DBG_ALL, 1, "deference os array");
            }
            else {

                /* get the data from the system image */
                switch(TYPE_SIZE(&v->type)) {

                    case 1: eppic_getmem(madr, &v->v.uc, 1); 
                        break;
                    case 2: eppic_getmem(madr, &v->v.us, 2); 
                        break;
                    case 4: eppic_getmem(madr, &v->v.ul, 4);
                        break;
                    case 8: eppic_getmem(madr, &v->v.ull, 8); 
                        break;

                }
            }
        }
        else {
            eppic_getmem(madr, &v->v.ull, eppic_defbsize());
        }
    }

    /* we can always assign to a reference */
    v->set=1;
    v->setval=v;
    v->setfct=eppic_setderef;
}

static value_t *
eppic_exepto(node_t *n)
{
value_t *v=eppic_newval();
value_t *ref=NODE_EXE(n);

    eppic_do_deref(v, ref);
    eppic_freeval(ref);
    return v;
}

static void
eppic_freepto(node_t *n)
{
    NODE_FREE(n);
}
    

/* same thing for the ptrto operator */
node_t*
eppic_newptrto(int lev, node_t*n)
{
node_t*nn=eppic_newnode();

    nn->exe=(xfct_t)eppic_exepto;
    nn->free=(ffct_t)eppic_freepto;
    nn->data=n;
    return nn;
}
