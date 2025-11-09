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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <setjmp.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>

/* here we do some caching of the information. This can have a speedup effect
   since it limits the number of accesses we do the dwarf (or whatever type) db that
   drives the type and symbols information 
 */

static stinfo_t slist={"root"};

/* running key to new structures */
static ull nextidx=0, abitype=ABI_MIPS;
#define LOCALTYPESBASE 0x8000000000000000ll
static ull eppic_nextidx(void) { return LOCALTYPESBASE+nextidx++; }
int eppic_type_islocal(type_t *t) 
{ 
    return ((t->idx & LOCALTYPESBASE) != 0); 
}
/* this set of function is used to cleanup tdefs after their use.
   Trailing tdefs can be combersome. Trailing struct/union/enum get new idx
   each time and are not a trouble */
static stinfo_t*tag=0;
void
eppic_tagst(void)
{
    tag=slist.next;
}

void
eppic_flushtdefs(void)
{
stinfo_t*st=slist.next;
stinfo_t*last=&slist;

    while(st != tag) {

        stinfo_t*next=st->next;

        if(st->ctype.type==V_TYPEDEF && st->ctype.idx & LOCALTYPESBASE) {

            eppic_free(st->name);
            eppic_free(st);
            last->next=next;

        } else last=st;

        st=next;

    }
    tag=0;
}

static stinfo_t*
eppic_getst(char *name, int type)
{
stinfo_t*tst;

    for(tst=slist.next; tst; tst=tst->next) {

        if(tst->ctype.type == type && tst->name && ! strcmp(tst->name, name)) {

            return tst; 
        }
    }
    return 0;
}

void 
eppic_chktype(type_t *type, char *name)
{
type_t *t;

    if((t=eppic_getctype(type->rtype, name, 1))) {
        type->idx=t->idx;
        eppic_freetype(t);
    }
} 

#if 0
Not used yet.
static void
eppic_rmst(stinfo_t*rst)
{
stinfo_t*st=slist.next;
stinfo_t*last=&slist;

    while(st) {

        if(st==rst) {

            last->next=st->next;
            eppic_free(st->name);
            eppic_free(st);

            return;

        } 

        last=st;
        st=st->next;
    }
}
#endif

stinfo_t*
eppic_getstbyindex(ull idx, int type)
{
stinfo_t*tst;

    for(tst=slist.next; tst; tst=tst->next) {

        if(tst->ctype.type == type && tst->ctype.idx == idx) {

            return tst; 
        }
    }
    return 0;
}


static void
eppic_addst(stinfo_t*st)
{
stinfo_t*tst;
    eppic_dbg_named(DBG_STRUCT, st->name, 2, "Adding struct %s to cache\n", st->name);
    tst=slist.next;
    slist.next=st;
    st->next=tst;
}

stinfo_t *
eppic_partialctype(int type, char *name)
{
stinfo_t*st;

    /* check first if we have a partial of that type
       already in progress (after a forward declaration) */
    if((st=eppic_getst(name, type))) {

        /* if it's complete we need to start a new one */
        if(!st->all) return st;

    }
    st=eppic_calloc(sizeof(stinfo_t));
    st->name=eppic_strdup(name);
    st->ctype.type=type;
    st->all=0;
    st->ctype.idx=(ull)st;
    eppic_addst(st);
    eppic_dbg(DBG_ALL, 2, "Returning stinfo %p of type %d name %s", st,  type, name);
    return st;
}

void
eppic_type_setidxbyname(type_t *t, char *name)
{
stinfo_t *st;

    st=eppic_partialctype(t->type, name);
    eppic_dbg(DBG_ALL, 2, "Setting idxbyname name '%s' stinfo_t * %p", name, st);
    eppic_type_setidx(t, (ull)st);
}

typedef struct neg_s {
    struct neg_s *next;
    char *name;
} neg_t;

static neg_t *nlist=0;

void
eppic_addneg(char *name)
{
neg_t *neg;

    neg=eppic_alloc(sizeof *neg);
    neg->name=eppic_strdup(name);
    neg->next=nlist;
    nlist=neg;
}

int 
eppic_isneg(char *name)
{
neg_t *neg;

    for(neg=nlist; neg; neg=neg->next) 
        if(!strcmp(neg->name, name)) return 1;
    return 0;
}

/*
    This function is called by eppic_vardecl() when the typedef storage class
    as been specified. In which case we need to create new typedefs not variables.
*/
void
eppic_tdef_decl(dvar_t*dv, type_t*t)
{
    while(dv) {

        dvar_t*next;

        stinfo_t*st=eppic_calloc(sizeof(stinfo_t));

        if(dv->nbits) eppic_error("No bits fields for typedefs");
        if(dv->idx) {

            /* we change a 'typedef type var[n];' into a 'typedef type_t*var;' */
            eppic_freeidx(dv->idx);
            dv->idx=0;
            dv->ref++;
        }
#if 0
At this time we do not give any error messages or warnings.
If a type is redefined within a single file that will means
problem for the user put this is not a full blown C compiler.

        {
        type_t*t=eppic_newtype();

            if(API_GETCTYPE(V_TYPEDEF, dv->name, t)) {

                eppic_warning("Typedef %s already defined in image, redefinition ignored",
                    dv->name);
            }
            eppic_freetype(t);
        }
#endif
        t->typattr &= ~eppic_istdef(t->typattr);
        eppic_duptype(&st->rtype, t);
        eppic_pushref(&st->rtype, dv->ref);
        st->name=dv->name;
        dv->name=0;
        st->ctype.idx=eppic_nextidx();
        st->ctype.type=V_TYPEDEF;

        eppic_addst(st);
        
        next=dv->next;
        dv->next=0;
        eppic_freedvar(dv);
        dv=next;
    }
}

int
eppic_ispartial(type_t*t)
{
stinfo_t *st=(stinfo_t*)(t->idx);

    if(!st) {

        eppic_error("Oops eppic_ispartial");
    }
    return !st->all;
}

static int init=0;
static void
eppic_chkinit(void)
{
    if(!init) {

        eppic_error("Eppic Package not initialized");

    }
}

void
eppic_getmem(ull kp, void *p, int n)
{
    eppic_chkinit();
    if(!API_GETMEM(kp, p, n)) {

            memset(p, 0xff, n);
    }
}

void
eppic_putmem(ull kp, char *p, int n)
{
    eppic_chkinit();
    if(!API_PUTMEM(kp, p,n)) {

        eppic_error("Error on write at 0x%llx for %d", kp, n);

    }
}

void
eppic_startctype_named(int type, char *name)
{
stinfo_t*st;

    /* if no partial yet start one */
    if(!(st=eppic_getst(name, type)) || st->all)
        eppic_partialctype(type, name);
}

void
eppic_startctype(int type, node_t*namen)
{
    eppic_startctype_named(type, NODE_NAME(namen));
}

int
eppic_samectypename(int type, ull idx1, ull idx2)
{
stinfo_t *st1=(stinfo_t*)idx1;
stinfo_t *st2=(stinfo_t*)idx2;

    // check names
    if(!strcmp(st1->name, st2->name)) return 1;

    // check all members and sizes in order
    // unamed ctypes can end up here too...
    if(st1->stm) {
        stmember_t *m1=st1->stm, *m2=st2->stm;
        while(m1 && m2) {
            if(strcmp(m1->m.name, m2->m.name)) break;
            if(m1->m.offset != m2->m.offset ) break;
            if(m1->m.size != m2->m.size ) break;
            m1=m1->next;
            m2=m2->next;
        }
        if(!m1 && !m2) return 1;
    }
    else if(st1->enums) {

        enum_t *e1=st1->enums, *e2=st2->enums;
        while(e1 && e2) {
            if(strcmp(e1->name, e2->name)) break;
            if(e1->value != e2->value ) break;
            e1=e1->next;
            e2=e2->next;
        }
        if(!e1 && !e2) return 1;
    }
    return 0;
}

#define VOIDIDX 0xbabebabell
type_t*
eppic_getvoidstruct(int ctype)
{
type_t*bt=eppic_newtype();

    bt->type=ctype;
    bt->idx=VOIDIDX;
    bt->size=0;
    bt->ref=0;
    return bt;
}

void
eppic_fillst(stinfo_t *st)
{
    eppic_dbg_named(DBG_STRUCT, st->name, 2, "Fill St started '%s'.\n", st->name);
    API_MEMBER(st->name, "", (void**)&st->stm);
    if(st->stm) st->all=1;
}


type_t*
eppic_getctype(int ctype, char *name, int silent)
{
stinfo_t *st;
type_t *t=eppic_newtype();

    eppic_chkinit();
    eppic_dbg_named(DBG_TYPE, name, 1, "getctype [%d] [%s] [s=%d]\n", ctype, name, silent);
    if(!(st=eppic_getst(name, ctype))) {

        eppic_dbg_named(DBG_TYPE, name, 1, "getctype [%s] not found in cache - isneg %d\n", name, eppic_isneg(name));
        if(silent && eppic_isneg(name)) return 0;

        st=eppic_calloc(sizeof(stinfo_t));
        if(!API_GETCTYPE(ctype, name, &st->ctype)) {

            eppic_free(st);
            eppic_freetype(t);
            if(ctype == V_TYPEDEF) eppic_addneg(name);
            if(silent) return 0;

            eppic_dbg_named(DBG_TYPE, name, 1, "[%s] creating partial type\n", name);
            eppic_partialctype(ctype, name);
            return eppic_getctype(ctype, name, silent);
        }
        eppic_dbg_named(DBG_TYPE, name, 1, "getctype [%s] found in image\n", name);
        st->name=eppic_alloc(strlen(name)+1);
        strcpy(st->name, name);
        st->stm=0;
        st->ctype.idx=(ull)(unsigned long)st;
        eppic_addst(st);
        if(ctype == V_ENUM) {
            API_GETENUM(name, st->enums);
            eppic_pushenums(st->enums);
        }
    }
    else eppic_dbg_named(DBG_TYPE, name, 1, "getctype [%s] found in cache \n", name);

    if(ctype == V_ENUM || (ctype == V_TYPEDEF && st->rtype.type == V_ENUM)) {
        API_GETENUM(name, st->enums);
        eppic_pushenums(st->enums);
    }
    eppic_duptype(t, &st->ctype);
    eppic_type_setidx(t, (ull)st);
    eppic_dbg_named(DBG_TYPE, name, 1, "getctype [%s] idx=0x%llx ref=%d rtype=0x%llx\n", name, t->idx, t->ref, t->rtype);
    return t;
}

type_t*
eppic_newctype(int ctype, node_t*n)
{
type_t*t;
char *name;

    t=eppic_getctype(ctype, name=NODE_NAME(n), 0);
    NODE_FREE(n);
    eppic_free(name);
    return t;
}

/*
    We don't use the type to point back to get the typedef name.
    The type is now the real type not the type for the typedef.
    So we keep a running sting of the last name variable name
    the parser found and use that.
    5/23/00
*/
node_t*
eppic_tdeftovar(type_t*td)
{
char *eppic_lastvar(void);
char *name=eppic_lastvar();

    eppic_free(td);
    return eppic_newvnode(name);
}
/*
    Check to see if a cached member info is available
*/
static stmember_t*
eppic_getm_idx(char *name, ull idx, stinfo_t**sti, int offset)
{
stinfo_t*st=(stinfo_t*)idx;
stmember_t*stm;

    if(sti) *sti=st;

    /* checkl what we have right now */
    for(stm=st->stm; stm; stm=stm->next) {
        /* if the struct is unamed (unamed member), we try to match that member in that struct */
        if(!strlen(stm->m.name)) {
            stmember_t *ustm=eppic_getm_idx(name, stm->type.idx, sti, stm->m.offset);
            if(ustm) return ustm;
        }
        else if(!strcmp(stm->m.name, name)) return stm;
    }
    /* check if we can add this one */
    API_MEMBER(st->name, name, (void**)&st->stm);
    if(st->stm && !strcmp(st->stm->m.name, name)) return st->stm;
    return 0;
}

static stmember_t*
eppic_getm(char *name, type_t*tp, stinfo_t**sti)
{
    return eppic_getm_idx(name, tp->idx, sti, 0);
}


value_t *
eppic_ismember(value_t*vp, value_t*vm)
{
char *name=eppic_getptr(vm, char);
int ret=0;
stinfo_t*st;

    if(eppic_getm(name, &vp->type, &st)) ret=1;

    return eppic_defbtype(eppic_newval(), ret);
}

/* XXX this entire stuff could very well be machine specific ... */
static int
eppic_getalign(type_t*t)
{
    /* this is a custome type deal w/ it */
    if(t->type == V_BASE) {

        int n;

        /* Intel 386 ABI says that double values align on 4 bytes */
        if(abitype==ABI_X86) n=((t->size>4)?4:t->size);
        else  n=t->size;
        return n*8;
    }
    if(t->type == V_REF) {
        /*
         * This is an array but if there are additional references
         * (>1) it is an array of pointers. In that case the pointer
         * alignment has to be used.
         */
        if(t->idxlst && t->ref == 1) {
            int ret;

            eppic_popref(t, 1);
            ret=eppic_getalign(t);
            eppic_pushref(t, 1);
            return ret;
        }
        return eppic_defbsize()*8;
    }
    /* alignment of a struct/union is on the largest of it's member or
       largest allignment of sub structures */
    if(is_ctype(t->type)) {

        stinfo_t*st=(stinfo_t *)(t->idx);
        stmember_t*sm;
        int maxallign=0;

        for(sm=st->stm; sm; sm=sm->next) {

            int a=eppic_getalign(&sm->type);

            if(a > maxallign) maxallign=a;

        }

        return maxallign;

    }
    /* other types shoudl not be part of a ctype declaration ... */
    eppic_error("Oops eppic_getalign2!");
    return 0;
}

static stinfo_t*
eppic_chkctype(int ctype, char *name)
{
stinfo_t*sti;

    if(name) {

        /* we should already have a partial structure on the stack */
        sti=eppic_getst(name, ctype);

        if(sti->all) {

            eppic_error("Oops eppic_ctype_decl");
        }

        eppic_free(name);

    } else {

        sti=eppic_alloc(sizeof(stinfo_t));
        sti->name=0;
	sti->ctype.idx=(ull)(unsigned long)sti;
	sti->ctype.type=ctype;
        eppic_addst(sti);
    }
    return sti;
}

/*
    This function is used to create new enum types.
    The syntax for enum is:
    enum ident {
        ident [= int],
        [ident [= int] ] ...
    };
    So we check for an assign value and is it exists then 
    we reset the counter to it.
    This is the way the mips compiler does it. Which migt be
    the right way or not, although I fail to see why it's done
    that way.

    So enum foo {
        a,
        b,
        c=0,
        d
    };

    Wil yield values :

    a=0
    b=1
    c=0
    c=1
*/
enum_t*
eppic_add_enum(enum_t*ep, char *name, int val)
{
enum_t *epi, *nep=eppic_alloc(sizeof(enum_t));

    nep->name=name;
    nep->value=val;
    nep->next=0;
    if(!ep) return nep;
    epi=ep;
    while(ep->next) ep=ep->next;
    ep->next=nep;
    return epi;
}
    
type_t*
eppic_enum_decl(int ctype, node_t*n, dvar_t*dvl)
{
dvar_t*dv=dvl, *next;
int counter=0;
stinfo_t*sti;
enum_t *ep=0;
char *name=n?NODE_NAME(n):0;
type_t *t;

    if(name) eppic_startctype_named(ctype, name);
    sti=eppic_chkctype(ctype, name);

    while(dv) {

        int val;

        /* evaluate an assignment ? */
        if(dv->init) {

            value_t *v=eppic_exenode(dv->init);

            if(!v) {

                eppic_rerror(&dv->pos, "Syntax error in enum expression");

            } else if(v->type.type != V_BASE) {

                eppic_rerror(&dv->pos, "Integer expression needed");
            }

            val=eppic_getval(v);
            counter=val+1;
            eppic_freeval(v);

        } else {

            val=counter++;
        }

        ep=eppic_add_enum(ep, dv->name, val);

        next=dv->next;
        dv->next=0;
        dv->name=0;
        eppic_freedvar(dv);
        dv=next;
    }
    sti->enums=ep;

    /* now we push the values in the defines */
    eppic_pushenums(sti->enums);

    /* we return a simple basetype_t*/
    /* after stahing the idx in rtype */
    t=eppic_newbtype(INT);
    t->rtype=sti->ctype.idx;
    t->typattr |= eppic_isenum(-1);
        
    return t;
    
}

/*
    The next functions are used to produce a new type
    and make it available throught the local cache.
    This enables custom type definitions on top of the
    ctypes defined in the object symbol tables.

    There is one function per suported architechture.

*/
/* macro for alignment to a log2 boundary */
#define Alignto(v, a) (((v) + (a) -1) & ~((a)-1))
/*
    The algorith complies with the SysV mips ABI
*/
type_t*
eppic_ctype_decl(int ctype, node_t*n, var_t*list)
{
type_t*t;
stinfo_t*sti;
stmember_t **mpp;
var_t*v;
int bits_left, bit_alignment;
int maxbytes, alignment, nextbit;
char *name=n?NODE_NAME(n):0;

    if(list->next==list) {

        eppic_error("Empty struct/union/enum declaration");
    }

    t=eppic_newbtype(0);
    sti=eppic_chkctype(ctype, name);
    t->type=sti->ctype.type;
    t->idx=sti->ctype.idx;
    sti->stm=0;
    mpp=&sti->stm;

#if LDEBUG
printf("\n%s %s\n", ctype==V_STRUCT?"Structure":"Union", name ? name : "");
#endif

    /* these are the running position in the structure/union */
    nextbit=0;      /* next bit open for business */
    alignment=0;    /* keeps track of the structure alignment
                       Mips ABI says align to bigest alignment of
                       all members of the struct/union. Also
                       unamed bit fields do not participate here. */
    maxbytes=0;     /* tracking of the maximum member size for union */

    for(v=list->next; v!=list; v=v->next) {

        stmember_t*stm=eppic_calloc(sizeof(stmember_t));
        dvar_t*dv=v->dv;
        int nbits;

        stm->m.name=eppic_strdup(v->name);
        eppic_duptype(&stm->type, &v->v->type);

        /* if this member is a bit filed simply use that */
        if(dv->bitfield) {

            nbits=dv->nbits;

            /* aligment is the size of the declared base type size */
            bit_alignment=v->v->type.size*8;

            if(nbits > bit_alignment) {

                eppic_error("Too many bits for specified type");
            }

            /* For unamed bit field align to smallest entity */
            /* except for 0 bit bit fields */
            if(!dv->name[0] && nbits) {

                bit_alignment=((nbits+7)/8)*8;

            } 

            /* We compute the number of bits left in this entity */
            bits_left = bit_alignment - (nextbit%bit_alignment);

            /* 0 bits means, jump to next alignement unit anyway 
               if not already on such a boundary */
            if(!nbits && (bits_left != bit_alignment)) nbits=bits_left;

            /* Not enough space ? */
            if(nbits > bits_left) {

                /* jump to next start of entity */
                nextbit += bits_left;

            }

            /* update member information */
            stm->m.offset=(nextbit/bit_alignment)*v->v->type.size;
            stm->m.fbit=nextbit % bit_alignment;
            stm->m.nbits=nbits;
            stm->m.size=v->v->type.size;
#if LDEBUG
            printf("    [%s] Bit member offset=%d, fbit=%d, nbits=%d\n", stm->m.name, stm->m.offset,  stm->m.fbit, stm->m.nbits);
#endif
            /* an unamed bit field does not participate in the alignment value */
            if(!dv->name[0]) {
    
                bit_alignment=0;

                /* reset size so that it does not have affect in eppic_getalign() */
                stm->type.size=1;
            }

        } else {

            int nidx=1;

            if(dv->idx) {

                int i;

                /* flag it */
                stm->type.idxlst=eppic_calloc(sizeof(int)*(dv->idx->nidx+1));

                /* multiply all the [n][m][o]'s */
                for(i=0;i<dv->idx->nidx;i++) {

                    value_t *vidx;
                    ull idxv;

                    vidx=eppic_exenode(dv->idx->idxs[i]);
                    if(!vidx) {

                        eppic_error("Error while evaluating array size");
                    }
                    if(vidx->type.type != V_BASE) {

                        eppic_freeval(vidx);
                        eppic_error("Invalid index type");

                    }

                    idxv=eppic_getval(vidx);
                    eppic_freeval(vidx);

                    stm->type.idxlst[i]=idxv;

                    nidx *= idxv;
                }
            

            }

            /* the number of bits on which this item aligns itself */
            bit_alignment=eppic_getalign(&stm->type);

            /* jump to this boundary */
            nextbit = Alignto(nextbit,bit_alignment);


            if(stm->type.ref - (dv->idx?1:0)) {

                nbits=nidx*eppic_defbsize()*8;

            } else {

                nbits=nidx*stm->type.size*8;
            }

            if(abitype==ABI_X86) {

                int pos=nextbit/8;

                pos = (pos & 0xfffffffc) + 3 - (pos & 0x2);
                stm->m.offset=pos;

            } else {

                stm->m.offset=nextbit/8;
            }
            stm->m.nbits=0;
            stm->m.size=nbits/8;
#if LDEBUG
printf("    [%s] Mmember offset=%d, size=%d size1=%d nidx=%d\n", stm->m.name, stm->m.offset, stm->m.size, stm->type.size, nidx);
#endif

        }

        if(ctype==V_STRUCT) nextbit+=nbits;
             /* Union members overlap */
        else nextbit=0;

        /* keep track of the maximum alignment */
        if(bit_alignment>alignment) alignment=bit_alignment;

        /* keep track of maximum size for unions */
        if(stm->m.size > maxbytes) maxbytes=stm->m.size;

        stm->next=0;
        *mpp=stm;
        mpp=&stm->next;
    }

    /* pad the final structure according to it's most stricly aligned member */
    if(nextbit) nextbit = Alignto(nextbit, alignment);
    else nextbit=Alignto(maxbytes*8, alignment); /* --> it's the case for a union */

    t->size=sti->ctype.size=nextbit/8;

#if LDEBUG
printf("Final size = %d\n", t->size);
#endif

    sti->all=1;
    return t;
}

/*
   member access and caching.
   If the member name is empty then the caller wants us
   to populate the entire engregate. The apimember() should
   support a getfirst() (member name == "") and getnext()
   (member name != "") for this perpose.
 */
stmember_t*
eppic_member(char *mname, type_t*tp)
{
stinfo_t *sti;
stmember_t *stm;

    if(!is_ctype(tp->type) && ! (tp->type==V_REF && is_ctype(tp->rtype))) {

        eppic_error("Expression for member '%s' is not a struct/union", mname);
    
    }

    if(tp->idx == VOIDIDX) {

        eppic_error("Reference to member (%s) from unknown structure type", mname);
    }

    if(!(stm=eppic_getm(mname, tp, &sti))) {

        eppic_error("Unknown member name [%s]", mname);
    }
    return stm;
}

int
eppic_open()
{
    eppic_setofile(stderr);
    /* push an empty level for parsing allocation */
    eppic_pushjmp(0, 0, 0);
    eppic_setapiglobs();
    init=1;
    eppic_setbuiltins();
    return 1;
}

/* here is a set of api function that do nothing */
static int apigetmem(ull iaddr, void *p, int nbytes) { return 1; }
static int apiputmem(ull iaddr, void *p, int nbytes) { return 1; }
static int apimember(char *sname, char *mname, void **stmp) { return 0; }
static int apigetctype(int ctype, char *name, type_t*tout) { return 0; }
static int apigetval(char *name, ull *val, value_t *v) { return 0; }
static int apigetenum(char *name, enum_t *enums) { return 0; }
static def_t *apigetdefs(void) { return 0; }
static char* apifindsym(char *p) { return 0; }

static apiops nullops= {
    apigetmem, 
        apiputmem, 
        apimember, 
        apigetctype, 
        apigetval, 
        apigetenum, 
        apigetdefs, 
        0, 
        0, 
        0, 
        0, 
        apifindsym
};

apiops *eppic_ops=&nullops;;
int eppic_legacy=0;

void
eppic_apiset(apiops *o, int abi, int nbpw, int sign)
{
def_t *dt;

    eppic_ops=o?o:&nullops;
    // for now, use env var to control symbol lookups mode.
    if(getenv("EPPIC_LEGACY_MODE")) eppic_legacy=1;
    eppic_setdefbtype(nbpw, sign);
    /* get the pre defines and push them. */
    dt=API_GETDEFS();
    while(dt) {

        eppic_newmac(dt->name, dt->val, 0, 0, 1);
        dt=dt->next;
    }
    /* add the eppic define */
    eppic_newmac(eppic_strdup("eppic"), eppic_strdup("1"), 0, 0, 1);
}

/*
    Get and set path function.
    ipath is include file search path.
    mpath is macro search path
*/
static char *mpath="";
static char *ipath="";
void eppic_setmpath(char *p) { mpath=p; }
void eppic_setipath(char *p) { ipath=p; }
char *eppic_getmpath(void) { return mpath; }
char *eppic_getipath(void) { return ipath; }

static char *curp=0;
char *eppic_curp(char *p) { char *op=curp; curp=p; return op; }

static char*
eppic_cattry(char *first, char *second)
{
struct stat stats;
char *buf=eppic_alloc(strlen(first)+strlen(second)+2);

    strcpy(buf, first);
    strcat(buf, "/");
    strcat(buf, second);
    if(!stat(buf, &stats)) return buf;
    eppic_free(buf);
    return 0;
}

char *
eppic_filepath(char *fname, char *path)
{
    struct stat buf;
    /* valid file path, return immediatly */
    if(stat(fname,&buf) == 0) {
        /* must return a free'able name */
        char *name=eppic_strdup(fname);
        TAG(name);
        return name;

    } else if(fname[0]=='~') {

        if(strlen(fname)>1) {

            char *rname, *start;
            struct passwd *pwd;

            if(fname[1]=='/') {

                /* current user name */
                pwd=getpwuid(getuid());

                if(!pwd) {
                    eppic_msg("Who are you : uid=%d \n?", getuid());
                    return 0;
                }

                start=fname+1;

            } else {

                char *p, s;

                for(p=fname+1;*p;p++) if(*p=='/') break;
                s=*p;
                *p='\0';

                /* other user */
                pwd=getpwnam(fname+1);
                if(!pwd) {

                    eppic_msg("Who is this : %s ?\n", fname+1);
                    return 0;
                }
                if(s) *p=s;
                start=p;
            }
            rname=eppic_alloc(strlen(start+1)+strlen(pwd->pw_dir)+2);
            strcpy(rname, pwd->pw_dir);
            strcat(rname, start);
            return rname;
        }

    } else {

        char *p=eppic_strdup(path);
        char *tok, *curp;

        /* we check if the file is found relatively to the current
           position. I.e. the position of the running script */
        if((curp=eppic_curp(0)) && (curp=eppic_cattry(curp, fname))) {

            eppic_free(p);
            return curp;
        }

        tok=strtok(p, ":");
        while(tok) {

            if((curp=eppic_cattry(tok, fname))) {

                eppic_free(p);
                return curp;
            }
            tok=strtok(NULL, ":");

        }
        eppic_free(p);
    }
    return 0;
}

char*
eppic_filempath(char *fname) 
{
    return eppic_filepath(fname, mpath);
}

char *
eppic_fileipath(char *fname) 
{
    return eppic_filepath(fname, ipath);
}

/* load a file or a set of file */
int
eppic_loadunload(int load, char *name, int silent)
{
DIR *dirp;
int ret=1;
char *fname=eppic_filempath(name);

    if(!fname) {

        if(!silent) eppic_msg("File not found : %s\n", name);
        return 0;
    }

    if((dirp=opendir(fname))) {

        struct dirent *dp;
        char *buf;

        while ((dp = readdir(dirp)) != NULL) {

            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
                continue;

            buf=eppic_alloc(strlen(fname)+dp->d_reclen+2);
            sprintf(buf, "%s/%s", fname, dp->d_name);
            if(load) {
                eppic_msg("\t\t%s\n", buf);
                ret &= eppic_newfile(buf, silent);
            }else{
                eppic_deletefile(buf);
            }
            eppic_free(buf);
        }
        closedir(dirp);
    }
    else {

        if(load) {
            ret=eppic_newfile(fname, silent);
        }else{
            eppic_deletefile(fname);
        }
    }
    eppic_free(fname); 
    return ret;
}

/*
    Load conditionaly.
    If it's already load, return.
*/
ull
eppic_depend(char *name)
{
char *fname=eppic_filempath(name);
int ret=1 ;
void *fp;

    if(!fname) ret=0;
    else if(!(fp=eppic_findfile(fname,0)) || eppic_isnew(fp)) {

        ret=eppic_loadunload(1, name, 1);
        eppic_free(fname);
    }
    return ret;
}

value_t *
eppic_bdepend(value_t *vname)
{
    return eppic_makebtype(eppic_depend(eppic_getptr(vname, char)));
}

ull 
eppic_load(char *fname)
{
    return eppic_loadunload(1, fname, 0);
}

value_t*
eppic_bload(value_t *vfname)
{
char *fname=eppic_getptr(vfname, char);
value_t *v;

    v=eppic_makebtype(eppic_load(fname));
    return v;
}

ull
eppic_unload(char *fname)
{
    return eppic_loadunload(0, fname, 0);
}

value_t*
eppic_bunload(value_t *vfname)
{
char *fname=eppic_getptr(vfname, char);

    return eppic_defbtype(eppic_newval(), eppic_unload(fname));
}

void
eppic_loadall()
{
char *path=eppic_strdup(eppic_getmpath());
char *p, *pn;

    p=pn=path;
    while(*pn) {

        if(*pn == ':') {

            *pn++='\0';
            eppic_loadunload(1, p, 1);
            p=pn;

        } else pn++;
    }
    if(p!=pn) eppic_loadunload(1, p, 1);
    /* eppic_free(path); */
}

static void
add_flag(var_t*flags, int c)
{
char s[20];
var_t *v;

    sprintf(s, "%cflag", c);
    v=eppic_newvar(s);
    eppic_defbtype(v->v, (ull)0);
    v->ini=1;
    eppic_enqueue(flags, v);
}

int
eppic_cmd(char *fname, char **argv, int argc)
{
value_t *idx, *val;

    eppic_chkinit();

    if(eppic_chkfname(fname, 0)) {

        var_t*flags, *args, *narg;
        char *opts, *newn=eppic_alloc(strlen(fname)+sizeof("_usage")+1);
        int c, i;
        extern char *optarg;
        extern int optind;
        int dou;
        char *f=eppic_strdup("Xflag");

        flags=(var_t*)eppic_newvlist();

        /* build a complete list of option variables */
        for(c='a';c<='z';c++) add_flag(flags, c);
        for(c='A';c<='Z';c++) add_flag(flags, c);

        /* check if there is a getopt string associated with this command */
        /* there needs to be a fname_opt() and a fname_usage() function */
        sprintf(newn, "%s_opt", fname);

        if(eppic_chkfname(newn, 0)) opts=(char*)(unsigned long)eppic_exefunc(newn, 0);
        else opts="";

        sprintf(newn, "%s_usage", fname);
        dou=eppic_chkfname(newn, 0);

        /* build a set of variable from the given list of arguments */
        /* each options generate a conrresponding flag ex: -X sets Xflag to one
           end the corresponding argument of a ":" option is in ex. Xarg
           each additional arguments is keaped in the array args[] */

        if(opts[0]) {

#ifdef linux
            optind=0;
#else
            getoptreset();
#endif
            while ((c = getopt(argc, argv, opts)) != -1) {

                var_t*flag, *opt;
                char *a=eppic_strdup("Xarg");;

                if(c==':') {

                    eppic_warning("Missing argument(s)");
                    if(dou) eppic_exefunc(newn, 0);
                    eppic_free(a);
                    goto out;

                } else if(c=='?') {

                    if(dou) {

                        char *u=(char*)(unsigned long)eppic_exefunc(newn, 0);

                        if(u) eppic_msg("usage: %s %s\n", fname, u);
                    }
                    eppic_free(a);
                    goto out;
                }

    
                /* set the Xflag variable  to 1 */
                f[0]=c;
                flag=eppic_inlist(f, flags);
                eppic_defbtype(flag->v, (ull)1);
                flag->ini=1;

                /* create the Xarg variable */
                if(optarg && optarg[0]) {

                    char *p=eppic_alloc(strlen(optarg)+1);

                    a[0]=c;
                    strcpy(p, optarg);
                    opt=(var_t*)eppic_newvar(a);
                    eppic_setstrval(opt->v, p);
                    opt->ini=1;
                    eppic_enqueue(flags, opt);
                }
                eppic_free(a);
            }
            eppic_free(f);
        }
        else optind=1;

        /* put every other args into the argv[] array_t*/
        args=(var_t*)eppic_newvar("argv");
        args->ini=1;

        /* create a argv[0] with the name of the command */
        {

            val=eppic_makestr(fname);
            idx=eppic_makebtype(0);

            /* create the value's value */
            eppic_addarrelem(&args->v->arr, idx, val);
            eppic_freeval(idx);
        }

        for ( i=1; optind < argc; optind++, i++) {

            val=eppic_makestr(argv[optind]);
            idx=eppic_makebtype(i);

            /* create the value's value */
            eppic_addarrelem(&args->v->arr, idx, val);
            eppic_freeval(idx);
        }

        narg=(var_t*)eppic_newvar("argc");
        eppic_defbtype(narg->v, i);
        narg->ini=1;

        eppic_enqueue(flags, narg);

        /* add the args variable to the flags queue */
        eppic_enqueue(flags, args);

        /* now execute */
        eppic_runcmd(fname, flags);

out:
        /* free all arguments variables Xflag Xarg and argv[] */
        eppic_freesvs(flags);

        eppic_free(newn);
        return 0;
    }
    return 1;
}

