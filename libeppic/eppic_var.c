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
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include "eppic.h"

/*
	Get an existing variable from the current set.
*/

/* variable lists for the different scopes */
typedef struct {
	int type;
	var_t*svs;
} svlist;

typedef struct glo {
	struct glo *next;
	var_t*vv;
} glo;

/*
	Free indexes specifications.
*/
void
eppic_freeidx(idx_t *idx)
{
int i;

	for(i=0;i<idx->nidx;i++) {

		if(idx->idxs[i]) NODE_FREE(idx->idxs[i]);
	}
	eppic_free(idx);
}

/*
	Free a variable declaration structure.
*/
void
eppic_freedvar(dvar_t*dv)
{
	if(!dv) return;
	if(--dv->refcount) return;
	if(dv->name) eppic_free(dv->name);
	if(dv->idx) eppic_freeidx(dv->idx);
	if(dv->init) NODE_FREE(dv->init);
	if(dv->fargs) eppic_freesvs(dv->fargs);
	eppic_free(dv);
}

void
eppic_setarray(array_t**arpp)
{
array_t*arp=*arpp;

	if(!arp) {

		arp=eppic_calloc(sizeof(array_t));
		TAG(arp);
		arp->next=arp->prev=arp;
		arp->ref=1;
		*arpp=arp;
	}
}

/*
	this is the main variable declaration function.
	We support the global scope attribute that make the declared
	variable accessible to all function from all scripts.

	By default the scope of a variable either the statement block
	where it was declared (or first used):
	{
	int var;
	 ...
	}
	Then it's scope is the block itself.

	Or the file, if it was declared outside of a function.

	Storage is by default 'automatic' and can be made permanent
	by using the 'static' keywork in the declaration.
	'Volatile' and 'register' storage classes are supported but 
	have no effect.
*/
var_t*
eppic_vardecl(dvar_t*dv, type_t*t)
{
var_t*vlist=eppic_newvlist();
var_t*var;

	/* type *and* dv can have ref counts. First comes from typedef parsing
	   second comes from the declaration itself */
	dv->ref += t->ref;
    
	/* add one level of ref for arrays */
	if(dv->idx) dv->ref++;

	/* reset ref level for tests below */
	eppic_popref(t, t->ref);

	TAG(vlist);

	if(!t->type) {

		int sto=eppic_isstor(t->typattr);

		eppic_freetype(t);
		t=eppic_newbtype(0);
		t->typattr |= sto;
	}
	else if(t->type==V_BASE && !dv->ref) {

		eppic_chksign(t);
		eppic_chksize(t);
	}

	/* is this a new typedef declaration ? */
	/* typedef is considered just like any other storage class */
	if(eppic_istdef(t->typattr)) {

		eppic_tdef_decl(dv, t);
		return 0;
	}

	while(dv) {
        
                /* disalow var names that match against already defined vars */
                if(dv->name[0]) {
                    type_t *t=eppic_getctype(V_TYPEDEF, dv->name, 1);
                    if(t) {
                    
                        eppic_freetype(t);
                        eppic_warning("Variable '%s' already defined as typedef.\n");
                    }
                }
                /*
                else goto next;
                */

		/* 
		   some sanity checks here that apply to both var and struct 
		   declarations 
		*/
		if(is_ctype(t->type) && !dv->ref) {

			if(dv->name[0]) {

				if(!instruct) {

					if(!eppic_isxtern(t->typattr)) {

						eppic_freesvs(vlist);
						eppic_error("struct/union instances not supported, please use pointers");
					}

				} else if(eppic_ispartial(t)) {

					eppic_freesvs(vlist);
					eppic_error("Reference to incomplete type");
				}
			}
		}
		if(dv->nbits) { 

			if(t->type != V_BASE) {

				eppic_freesvs(vlist);
				eppic_error("Bit fields can only be of integer type");

			}
			if(dv->idx) {

				eppic_freesvs(vlist);
				eppic_error("An array of bits ? Come on...");
			}
		}

		var=eppic_newvar(dv->name);

		t->fct=dv->fct;
		eppic_duptype(&var->v->type, t);
		eppic_pushref(&var->v->type, dv->ref);

		var->dv=dv;

		TAG(var);

		if(t->type == V_STRING) {

			eppic_setstrval(var->v, "");

		} 

		eppic_setpos(&dv->pos);

        /* toff */
        /* add to parsing list */  
        if(var->name[0]) {
           var_t *plist=eppic_newvlist();
           eppic_enqueue(plist, var);
           eppic_addsvs(S_PARSE, eppic_dupvlist(plist));
        }
		eppic_enqueue(vlist, var);
next:
		dv=dv->next;
	}
	eppic_free(t);
	TAG(vlist);
	return vlist;
}

dvar_t*
eppic_newdvar(node_t*v)
{
dvar_t*dv;

	dv=eppic_alloc(sizeof(dvar_t));
	memset(dv, 0, sizeof(dvar_t));
	if(v) {
		dv->name=NODE_NAME(v);
		NODE_FREE(v);

	} else {

		dv->name=eppic_alloc(1);
		dv->name[0]='\0';
	}
	dv->refcount=1;
	eppic_setpos(&dv->pos);
	return dv;
}

dvar_t*
eppic_dvarini(dvar_t*dv, node_t*init)
{
	dv->init=init;
	return dv;
}

dvar_t*
eppic_dvarptr(int ref, dvar_t*dv)
{
	dv->ref+=ref;
	return dv;
}

dvar_t*
eppic_dvaridx(dvar_t*dv, node_t*n)
{
	if(!dv->idx) {

		dv->idx=eppic_alloc(sizeof(idx_t));
		dv->idx->nidx=0;
	}
	dv->idx->idxs[dv->idx->nidx++]=n;
	return dv;
}

dvar_t*
eppic_dvarfld(dvar_t*dv, node_t*n)
{

	if(n) {

		value_t *va=eppic_exenode(n);

		/* get the value_t for the bits */
		if(!va) dv->nbits=0;
		else {
			dv->nbits=unival(va);
			eppic_freeval(va);
		}
		NODE_FREE(n);

	} else dv->nbits=0;

	dv->bitfield=1;
	return dv;
}

dvar_t*
eppic_dvarfct(dvar_t*dv, var_t*fargs)
{
	dv->fct=1;
	dv->fargs=fargs;
	return dv;
}

dvar_t*
eppic_linkdvar(dvar_t*dvl, dvar_t*dv)
{
dvar_t*v;

	/* need to keep declaration order for variable initialization */
	if(dv) {

		for(v=dvl; v->next; v=v->next);
		dv->next=0;
		v->next=dv;
	}
	return dvl;
}

idx_t *
eppic_newidx(node_t*n)
{
idx_t *idx;

	if(!instruct) {

		eppic_error("Array supported only in struct/union declarations");
	}
	idx=eppic_alloc(sizeof(idx_t));
	idx->nidx=1;
	idx->idxs[0]=n;
	return idx;
}

idx_t *
eppic_addidx(idx_t *idx, node_t*n)
{
	if(idx->nidx==MAXIDX) {

		eppic_error("Maximum number of dimension is %d", MAXIDX);
	}
	idx->idxs[idx->nidx++]=n;
	return idx;
}

static svlist svs[S_MAXDEEP];
static glo *globs=0;
int svlev=0;

void
eppic_refarray(value_t *v, int inc)
{
array_t*ap, *na;

	if(!v->arr) return;
	v->arr->ref+=inc;
	if(v->arr->ref == 0) {

		/* free all array element. */
		for(ap=v->arr->next; ap!=v->arr; ap=na) {

			na=ap->next;
			eppic_freeval(ap->idx);
			eppic_freeval(ap->val);
			eppic_free(ap);
		}
		eppic_free(v->arr);
		v->arr=0;

	} else {

		/* do the same to all sub arrays */
		for(ap=v->arr->next; ap!=v->arr; ap=na) {

			na=ap->next;
			eppic_refarray(ap->val, inc);
		}
	}
		
}

void
eppic_freedata(value_t *v)
{
	
	if(is_ctype(v->type.type) || v->type.type == V_STRING) {

		if(v->v.data) eppic_free(v->v.data);
		v->v.data=0;

	}
	eppic_refarray(v, -1);
}

void
eppic_dupdata(value_t *v, value_t *vs)
{

	if(is_ctype(vs->type.type) || vs->type.type == V_STRING) {

		v->v.data=eppic_alloc(vs->type.size);
		memmove(v->v.data, vs->v.data, vs->type.size);
	}
}

void
eppic_freeval(value_t *v)
{
	if(!v) return;
	eppic_freedata(v);
	eppic_free(v);
}


void
eppic_freevar(var_t*v)
{

	if(v->name) eppic_free(v->name);
	eppic_freeval(v->v);
	eppic_freedvar(v->dv);
	eppic_free(v);
}

void 
eppic_enqueue(var_t*vl, var_t*v)
{
	v->prev=vl->prev;
	v->next=vl;
	vl->prev->next=v;
	vl->prev=v;
}

void
eppic_dequeue(var_t*v)
{
	v->prev->next=v->next;
	v->next->prev=v->prev;
	v->next=v->prev=v;
}

/*
	This function is called to validate variable declaration.
	No array decalration for variables (this can only be checked in 
	eppic_stat_decl() and eppic_file_decl() usingthe idx field ofthe var struct.
	Same comment for nbits. Only in struct declarations.
*/
void
eppic_validate_vars(var_t*svs)
{
var_t*v, *next;

	if(!svs) return;

	for(v=svs->next; v!=svs; v=next) {

		next=v->next;

		/* just remove extern variables */
		if(eppic_isxtern(v->v->type.typattr)) {

			eppic_dequeue(v);
			eppic_freevar(v);

		} else {

			if(v->dv->idx) {

				eppic_freesvs(svs);
				eppic_error("Array instanciations not supported.");

			} 
			if(v->dv->nbits) {

				eppic_freesvs(svs);
				eppic_error("Syntax error. Bit field unexpected.");
			}
		}
	}
}

var_t*
eppic_inlist(char *name, var_t*vl)
{
var_t*vp;

	if(vl) {


		for(vp=vl->next; vp!=vl; vp=vp->next) {

			if(!strcmp(name, vp->name)) {

				return vp;

			}

		}
	}
	return 0;
}

static var_t*apiglobs;

void
eppic_setapiglobs(void)
{
	apiglobs=eppic_newvlist();
	eppic_add_globals(apiglobs);
}

static var_t*
eppic_inglobs(char *name)
{
var_t*vp;
glo *g;

	for(g=globs; g; g=g->next) {

		if((vp=eppic_inlist(name, g->vv))) return vp;
	}
	return 0;
}


void
eppic_chkglobsforvardups(var_t*vl)
{
var_t*v;

	if(!vl) return;

	for(v=vl->next; v != vl; v=v->next) {

		var_t*vg;

		if(v->name[0] && (vg=eppic_inglobs(v->name))) {

			/* if this is a prototype declaration then skip it */
			if(v->dv && v->dv->fct) continue;

			eppic_rerror(&v->dv->pos, "Duplicate declaration of variable '%s', defined at %s:%d"
				, v->name, vg->dv->pos.file, vg->dv->pos.line);
		}
	}
}

/* used during parsing */
static int vlev=0;
void eppic_setvlev(int lev) { vlev=lev; }
static int sidx[S_MAXSDEEP];

/*
   This function scans a list of variable and looks for those that have not been initialized yet.
   Globals, statics and autos all get initialized through here.
*/
static void
eppic_inivars(var_t*sv, int parsing)
{
var_t*v;

	if(!sv) return;

	for(v=sv->next; v!=sv; v=v->next) {

        if(parsing && !eppic_isstatic(v->v->type.typattr)) continue;
        
		/* check if we need to initialize it */
		if(!v->ini && v->dv && v->dv->init) {

			value_t *val;
			srcpos_t pos;

			eppic_curpos(&v->dv->pos, &pos);

			if((val=eppic_exenode(v->dv->init))) {

				eppic_chkandconvert(v->v, val);
				eppic_freeval(val);
				if(!vlev) v->ini=1;

			} else {

				eppic_rwarning(&v->dv->pos, "Error initializing '%s'", v->name);
			}
			eppic_curpos(&pos, 0);
		}
	}
}

/* return the last set of globals */
var_t*
eppic_getcurgvar()
{
	if(!globs) return 0;
	return globs->vv;
}

void *
eppic_add_globals(var_t*vv)
{
glo *ng=eppic_alloc(sizeof(glo));

	eppic_inivars(vv, 0);
	ng->vv=vv;
	ng->next=globs;
	eppic_chkglobsforvardups(vv);
	globs=ng;
	return ng;
}

void
eppic_rm_globals(void *vg)
{
glo *g=(glo*)vg;

	if(globs) {

		if(globs==g) globs=g->next;
		else {

			glo *gp;

			for(gp=globs; gp; gp=gp->next) {

				if(gp->next==g) {

					gp->next=g->next;

				}

			}
		}
		eppic_free(g);
	}
}



/*
	This is where we implement the variable scoping.
*/
var_t*
eppic_getvarbyname(char *name, int silent, int local)
{
var_t*vp;
int i, aidx=0;
ull apiv, mem;

	for(i=svlev-1; i>=0; i--) {

		if((vp=eppic_inlist(name, svs[i].svs))) {

			return vp;
		}
		if(svs[i].type==S_AUTO && !aidx) aidx=i;

		/* when we get to the function we're finished */
		if(svs[i].type==S_FILE) break;
	}

	/* have'nt found any variable named like this one */
	/* first check the globals */
	if(!(vp=eppic_inglobs(name))) {

		int off=0;

		/* check the API for a corresponding symbol */
		/* Jump over possible leading "IMG_" prefix */
		if(!strncmp(name, "IMG_", 4)) off=4;
		if(!local) {
			vp=eppic_newvar(name);
            if(API_GETVAL(name+off, &apiv, vp->v)) {
			    vp->ini=1;
			    /* put this on the global list */
			    eppic_enqueue(apiglobs, vp);
            }
            else {
                eppic_freevar(vp);
                vp=0;
            }
		}
		else {

			if(silent) return 0;
			eppic_error("Unknown variable [%s]", name);
		}
	}
	return vp;
}

value_t *
eppic_exists(value_t *vname)
{
char *name=eppic_getptr(vname, char);

	return eppic_defbtype(eppic_newval(), (eppic_getvarbyname(name, 1, 0) || eppic_funcexists(name)));
}

/* get a new empty vlist */
var_t*
eppic_newvlist()
{
var_t*p=eppic_newvar("");
	TAG(p);
	TAG(p->name);
	return p;
}

/* this is called when we duplicate a list of automatic variables */
var_t*
eppic_dupvlist(var_t*vl)
{
var_t*nv=(var_t*)eppic_newvlist(); /* new root */
var_t*vp;

	for(vp=vl->next; vp !=vl; vp=vp->next) {

		var_t*v=eppic_newvar(vp->name); /* new var_t*/

		v->dv=vp->dv;
		v->dv->refcount++;
		v->ini=vp->ini;
		eppic_dupval(v->v, vp->v);

		/* we start with a new array for automatic variable */
		eppic_refarray(v->v, -1);
		v->v->arr=0;
		eppic_setarray(&v->v->arr);
		
		/* can't check ctypes for initialisation */
		if(is_ctype(v->v->type.type)) v->ini=1;
		eppic_enqueue(nv, v);

	}
	return nv;
}

void
eppic_addtolist(var_t*vl, var_t*v)
{
	if(!v->name[0] || !eppic_inlist(v->name, vl)) {

		eppic_enqueue(vl, v);

	} else {

		/* if this is a prototype declaration then skip it */
		if(v->dv && v->dv->fct) return;

		eppic_error("Duplicate declaration of variable %s", v->name);
	}
}

static void
eppic_chkforvardups(var_t*vl)
{
var_t*v;

	if(!vl) return;

	for(v=vl->next; v!=vl; v=v->next) {

		var_t*v2=v->next;

		for(v2=v->next; v2!=vl; v2=v2->next) {

			if(v2->name[0] && !strcmp(v->name, v2->name)) {

				eppic_rerror(&v2->dv->pos, "Duplicate declaration of variable '%s'", v->name);

			}
		}
	}
}

static int takeproto=0;
void eppic_settakeproto(int v) { takeproto=v; }


/* 
	This function scans a new list of declared variables
	searching for static variables.
*/
void
eppic_addnewsvs(var_t*avl, var_t*svl, var_t*nvl)
{
var_t*v;

	if(nvl) {

		for(v=nvl->next; v!=nvl; ) {

			var_t*next;

			/* save next before eppic_enqueue() trashes it ... */
			next=v->next;

			/* if this is a external variable or prototype function declaration 
			   skip it */
			if((!takeproto && v->dv->fct && !v->dv->ref) || eppic_isxtern(v->v->type.typattr)) {

				v=next;
				continue;
			}

			if(eppic_isstatic(v->v->type.typattr)) {

				eppic_addtolist(svl, v);

			} else {

				eppic_addtolist(avl, v);
			}
			/* with each new variables check for duplicate declarations */
			eppic_chkforvardups(avl);
			eppic_chkforvardups(svl);

			v=next;
		}
		/* discard nvl's root */
		eppic_freevar(nvl);
	}
}

/*
    Support for compile time variable tracking for purpose of
    evaluation of sizeof or typeof.
*/
int eppic_getvlev() { return vlev; }
eppic_vpush()
{
	if(vlev==S_MAXSDEEP) {

		eppic_error("Too many nested compound statements!");

	} else {

		sidx[vlev]=svlev;
		vlev++;
    }
}

eppic_vpop()
{
    if(vlev) {
        eppic_setsvlev(sidx[--vlev]);
    }
    else eppic_error("Too many parse var pops!");
}

int
eppic_addsvs(int type, var_t*sv)
{
int curlev=svlev;

	if(svlev==S_MAXDEEP) {

		eppic_error("Svars stack overflow");

	} else {

		svs[svlev].type=type;
		svs[svlev].svs=sv;
		eppic_setsvlev(eppic_getsvlev()+1);

		/* perform automatic initializations */
		eppic_inivars(sv, type==S_PARSE);
		
		/* if S_FILE then we are entering a function so start a newset of
		   stack variables */
		if(type == S_FILE ) {

			(void)eppic_addsvs(S_AUTO, (var_t*)eppic_newvlist());

		}
	}
	return curlev;
}

void
eppic_add_statics(var_t*var)
{
int i;

	for(i=svlev-1;i>=0;i--) {

		if(svs[i].type==S_FILE ) {

			if(svs[i].svs)
				eppic_enqueue(svs[i].svs, var);
			else
				svs[i].svs=var;
			return;

		}
	}
	eppic_rwarning(&var->dv->pos, "No static context for var %s.", var->name);
}

void eppic_freesvs(var_t*v)
{
var_t*vp;

	for(vp=v->next; vp != v; ) {

		var_t*vn=vp->next;

		eppic_freevar(vp);

		vp=vn;
	}
	eppic_freevar(v);
}

int
eppic_getsvlev() { return svlev; }

/* reset the current level of execution and free up any automatic
   variables. */
void
eppic_setsvlev(int newlev)
{
int lev;

    /*printf("svlev=%d newlev=%d\n", svlev, newlev);*/
	for(lev=svlev-1; lev>=newlev; lev--) {

			if(svs[lev].type==S_AUTO) {

				eppic_freesvs(svs[lev].svs);

			}

	}
	svlev=newlev;
}

/*
	called by the 'var in array' bool expression.
*/
int
eppic_lookuparray(node_t*vnode, node_t*arrnode)
{
value_t *varr=NODE_EXE(arrnode);
array_t*ap, *apr=varr->arr;
value_t *val;
int b=0;

	val=NODE_EXE(vnode);

	if(apr) {

		for(ap=apr->next; ap != apr; ap=ap->next) {

			if(VAL_TYPE(ap->idx) == VAL_TYPE(val)) {

				switch(VAL_TYPE(val)) {
				case V_STRING:	b=(!strcmp(ap->idx->v.data, val->v.data)); break;
				case V_BASE:	b=(unival(ap->idx)==unival(val)); break;
				case V_REF:	
					if(eppic_defbsize()==4) 
						b=(ap->idx->v.ul==val->v.ul);
					else
						b=(ap->idx->v.ull==val->v.ull);
				break;
				default:
					eppic_rerror(&vnode->pos, "Invalid indexing type %d", VAL_TYPE(val));
				}
				if(b) break;
			}

		}
	}
	eppic_freeval(val);
	eppic_freeval(varr);
	return b;
}

/*
	The actual for(i in array) core...
*/
void
eppic_walkarray(node_t*varnode, node_t*arrnode, void (*cb)(void *), void *data)
{
value_t *v;
value_t *av;
array_t*ap, *apr;

	eppic_setini(varnode);
	v=NODE_EXE(varnode);

	av=NODE_EXE(arrnode);

	if(av->arr) {

		for(apr=av->arr, ap=apr->next; ap != apr; ap=ap->next) {

			/* we set the value_t of the variable */
			eppic_setval(v,ap->idx);

			(cb)(data);

		}
	}
	eppic_freeval(v);
	eppic_freeval(av);
}

/* scan the current array for a specific index and return value_t 
   XXX should use some hashing tables here for speed and scalability */
array_t*
eppic_getarrval(array_t**app, value_t *idx)
{
array_t*ap, *apr;

	/* eppic_setarray(app); AAA comment out */
	apr=*app;

	for(ap=apr->next; ap != apr; ap=ap->next) {

		if(ap->idx->type.type == idx->type.type) {

		int b=0;

			switch(idx->type.type) {
			case V_STRING: b=(!strcmp(ap->idx->v.data, idx->v.data));
			break;
			case V_BASE: b=(unival(ap->idx)==unival(idx));
			break;
			case V_REF:	
				if(eppic_defbsize()==4) 
					b=(ap->idx->v.ul==idx->v.ul);
				else
					b=(ap->idx->v.ull==idx->v.ull);
			break;
			default:
				eppic_error("Invalid index type %d", idx->type.type);
			}

			if(b) {

				return ap;

			}
		}
	}

	/* we have not found this index, create one */
	ap=(array_t*)eppic_calloc(sizeof(array_t));
	ap->idx=eppic_makebtype(0);
	eppic_dupval(ap->idx, idx);

	/* just give it a int value_t of 0 for now */
	ap->val=eppic_makebtype(0);

	/* we must set the same refenrence number as the
	   upper level array_t*/
	ap->val->arr->ref=apr->ref;

	/* link it in */
	ap->prev=apr->prev;
	ap->next=apr;
	apr->prev->next=ap;
	apr->prev=ap;
	ap->ref=0;
	return ap;
}

value_t *
eppic_intindex(value_t *a, int idx)
{
value_t *v=eppic_makebtype(idx);
array_t*ap=eppic_getarrval(&a->arr, v);

	eppic_dupval(v, ap->val);
	return v;
}

value_t *
eppic_strindex(value_t *a, char *idx)
{
value_t *v=eppic_makestr(idx);
array_t*ap=eppic_getarrval(&a->arr, v);

	eppic_dupval(v, ap->val);
	return v;
}


void
eppic_setarrbval(array_t*a, int val)
{
	eppic_defbtype(a->val, (ull)val);
}

array_t*
eppic_addarrelem(array_t**arr, value_t *idx, value_t *val)
{
array_t*na;

	na=eppic_getarrval(arr, idx);

	/* copy new val over */
	eppic_freeval(na->val);
	na->val=val;

	return na;
}

/* insert a variable at the end of the list */
static void
eppic_varinsert(var_t*v)
{
int i;

	for(i=svlev-1;i>=0;i--) {

		if(svs[i].type==S_AUTO) {

			eppic_enqueue(svs[i].svs, v);
			break;
		}
	}
}

/* Dupicate and add a set of variables. Used to setup a function execution.
   The new veriables are the actual parameters of the function so we mark them
   As being initialized.
*/
void
eppic_add_auto(var_t*nv)
{
	nv->ini=1;
	eppic_varinsert(nv);
}

void
eppic_valindex(value_t *var, value_t *idx, value_t *ret)
{
	if(is_ctype(idx->type.type)) {

		eppic_error("Invalid indexing type");

	} else {

		array_t*a;

		a=eppic_getarrval(&var->arr, idx);

		/* this is the first level of indexing through a variable */
		eppic_dupval(ret, a->val);
		ret->set=1;
		ret->setval=a->val;
	}
}

void
eppic_addvalarray(value_t*v, value_t*idx, value_t*val)
{
	eppic_addarrelem(&v->arr, idx, val);
	eppic_freeval(idx);
}

static void
prtval(value_t*v)
{
value_t*fmt=eppic_makestr("%?");

	eppic_printf(fmt, v, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
	eppic_freeval(fmt);
}

static void
prlevel(char *name, value_t*root, int level)
{
ARRAY_S *arr;

	for(arr=root->arr->next; arr != root->arr; arr=arr->next) {

		printf("%*s%s[", level*3, "", name);
		prtval(arr->idx);
		printf("]=");
		prtval(arr->val);
		printf("\n");
		prlevel(name, arr->val, level+1);
	}
}

/* eppic_prarr builtin */
value_t*
eppic_prarr(value_t*vname, value_t*root)
{
char *name=eppic_getptr(vname, char);
	printf("%s=", name);
	prtval(root);
	printf("\n");
	prlevel(name, root, 1);
	return eppic_makebtype(0);
}

var_t*
eppic_newvar(char *name)
{
var_t*v=eppic_calloc(sizeof(var_t));
char *myname=eppic_alloc(strlen(name)+1);

	TAG(myname);
	strcpy(myname,name);
	v->name=myname;
	v->v=eppic_makebtype(0);
	v->v->setval=v->v;
	v->next=v->prev=v;
	return v;
}


typedef struct {
	node_t*n;
	char name[1];
} vnode_t ;

static int insizeof=0;
void eppic_setinsizeof(int v) { insizeof=v;}

value_t *
eppic_exevar(void *arg)
{
vnode_t *vn = arg;
value_t *nv;
var_t*curv;
srcpos_t pos;

	eppic_curpos(&vn->n->pos, &pos);

	if(!(curv=eppic_getvarbyname(vn->name, 0, 0))) {

		eppic_error("Oops! Var ref1.[%s]", vn->name);

	}
	if(!curv->ini && !insizeof && !vlev) {

		eppic_error("Variable [%s] used before being initialized", curv->name);

	}

	nv=eppic_newval();
	eppic_dupval(nv,curv->v);
	nv->set=1;
	nv->setval=curv->v;
	nv->setfct=eppic_setfct;

	eppic_curpos(&pos, 0);

	return nv;
}

/* make sure a variable is flaged as being inited */
void
eppic_setini(node_t*n)
{
	if((void*)n->exe == (void*)eppic_exevar) {

		var_t*v=eppic_getvarbyname(((vnode_t*)(n->data))->name, 0, 0);
		v->ini=1;
	}
}


/* get the name of a function through a variable */
char *
eppic_vartofunc(node_t*name)
{
char *vname=NODE_NAME(name);
value_t *val;

	/* if the nore is a general expression, then vname is 0 */
	if(!vname) {

		val=eppic_exenode(name);

	} else  {

		var_t*v;
		
		v=eppic_getvarbyname(vname, 1, 1);
		if(!v) return vname;
		val=v->v;
	}

	switch(val->type.type)
	{
		case V_STRING:
		{
		char *p=eppic_alloc(val->type.size+1);
			/* return the value_t of that string variable */
			strcpy(p, val->v.data);
			eppic_free(vname);
			return p;
		}
		default:
			/* return the name of the variable itself */
			eppic_error("Invalid type for function pointer, expected 'string'.");
			return vname;
	}
}

char *
eppic_namevar(vnode_t*vn)
{
char *p;

	p=eppic_strdup(vn->name);
	TAG(p);
	return p;
}

static void
eppic_freevnode(vnode_t*vn)
{
	eppic_free(vn);
}

/*
        create or return existing variable node.
*/      
node_t*
eppic_newvnode(char *name)
{
node_t*n=eppic_newnode();
vnode_t*vn=eppic_alloc(sizeof(vnode_t)+strlen(name)+1);

	TAG(vn);

	strcpy(vn->name, name);
	n->exe=(xfct_t)eppic_exevar;
	n->free=(ffct_t)eppic_freevnode;
	n->name=(nfct_t)eppic_namevar;
	n->data=vn;
	vn->n=n;

	eppic_setpos(&n->pos);

        return n;
}

#define TO (*to)
#define FRM (*frm)

void
eppic_cparrelems(array_t**to, array_t**frm)
{
array_t*ap;

	if(FRM) {

		eppic_setarray(to);
		for(ap=FRM->next; ap!=FRM; ap=ap->next) {

			array_t*na=eppic_calloc(sizeof(array_t));

			/* copy value_ts */
			eppic_dupval(na->idx, ap->idx);
			eppic_dupval(na->val, ap->val);

			/* link it in */
			na->prev=TO->prev;
			na->next=TO;
			TO->prev->next=na;
			TO->prev=na;
			na->ref=1;

			/* copy that branch */
			eppic_cparrelems(&na->val->arr, &ap->val->arr);
		}
	}
}

