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
#include <string.h>
#include <ctype.h>
#include "eppic.h"
/*
	This set of function are used to print value_ts.
*/

/* utility that returns a string of '*' for a reference */
static
char *eppic_getref(int lev)
{
static char *ptrs="*******";

	return ptrs+strlen(ptrs)-lev;
}

static char *
eppic_getidx(type_t *t, char*buf, int len)
{
int pos=0;

	buf[0]='\0';
	if(t->idxlst) {

		int i;

		for(i=0; t->idxlst[i] && pos < len; i++) {

			pos += snprintf(buf+pos, len-pos, "[%d]", t->idxlst[i]);
		}
	}
	return buf;
}

#define INDENT		4	/* print indent at beginning of new line */
#define SPACER		16	/* space for type string */
#define NAMESPACE	16	/* space used for member/var names */
#define NBUNDLE		4	/* when printing arrays print this much before \n */

static void
eppic_indent(int level, int indent)
{
	if(!indent) return;
	eppic_msg("%*s", level*INDENT, "");
}

static void eppic_ptype2(type_t*t, value_t*v, int level, int indent, char *name, int ref, int justv);

/*
	Print a struct/union type or value
*/
static void
eppic_print_ctype(type_t *t, value_t *v, int level, int indent, char *name, int ref, int justv)
{
stinfo_t *st=eppic_getstbyindex(t->idx, t->type);
stmember_t *m;
char buf[100];

	if(!st) eppic_error("Oops eppic_print_ctype!");

	if(!st->all) {

		eppic_fillst(st);
		if(!st->all) eppic_error("Reference to a incomplete type");
	}

	eppic_indent(level, indent);

	if(!justv) {
		snprintf(buf, sizeof(buf)-1, "%s %s", eppic_ctypename(t->type), st->name?st->name:"");
		eppic_msg("%-*s ", SPACER, buf);

		/* is this is a pointer, bail out */
	}
	if(ref) return;

	if(v && !justv) eppic_msg(" = ");

	eppic_msg("{\n");

	for(m=st->stm; m; m=m->next) {

		value_t *vm=0;

		eppic_indent(level+1, 1);
		if(v) {
			vm=eppic_newval();
			eppic_duptype(&vm->type, &m->type);
			eppic_exememlocal(v, m, vm);
			eppic_ptype2(&vm->type, vm, level+1, 0, m->m.name, 0, 0);

		} else eppic_ptype2(&m->type, vm, level+1, 0, m->m.name, 0, 0);
		eppic_msg(";\n");
		if(vm) eppic_freeval(vm);
	}

	eppic_indent(level, 1);
	eppic_msg("}");
	if(name) eppic_msg(" %s", name);
	
}

static void
eppic_prbval(value_t *v)
{
	if(eppic_issigned(v->type.typattr)) eppic_msg("%8lld", eppic_getval(v));
	else eppic_msg("%8llu", eppic_getval(v));
}

static int 
eppic_prtstr(value_t *v, int justv)
{
value_t *vs;
char *s, *p;

	if(eppic_defbsize()==8) v->v.ull=v->mem;
	else v->v.ul=v->mem;
	vs=eppic_getstr(v);
	s=eppic_getptr(vs, char);
	for(p=s; *p; p++) if(!isprint(*p)) return 0;
	if(p==s) { eppic_freeval(vs); return 0; }
	if(!justv) eppic_msg("= ");
	eppic_msg("\"%s\"", s);
	eppic_freeval(vs);
	return 1;
}

static void
eppic_prtarray(type_t*t, ull mem, int level, int idx)
{
int i;
int j, size=1;

	for(j=idx+1; t->idxlst[j]; j++) size *= t->idxlst[j];
	size *= t->type==V_REF ? eppic_defbsize() : t->size;

	/* start printing */
	eppic_msg("{");
	eppic_msg("\n");
	eppic_indent(level+1, 1);

	for(i=0; i<t->idxlst[idx]; i++, mem += size) {

		if(t->idxlst[idx+1]) {

			eppic_msg("[%d] = ", i);
			eppic_prtarray(t, mem, level+1, idx+1);

		} else {

			/* time to deref and print final type */
			value_t *v=eppic_newval(), *vr=eppic_newval();
			int *pi=t->idxlst;

			t->idxlst=0;

			eppic_duptype(&vr->type, t);
			eppic_pushref(&vr->type, 1);
			if(eppic_defbsize()==8) vr->v.ull=mem;
			else vr->v.ul=(ul)mem;
			eppic_do_deref(1, v, vr);
			if(is_ctype(v->type.type) || !(i%NBUNDLE)) eppic_msg("[%2d] ", i);
			eppic_ptype2(&v->type, v, level+1, 0, 0, 0, 1);
			eppic_msg(", ");
			/* anything else then struct/unions, print in buddles */
			if(!is_ctype(v->type.type) && !((i+1)%NBUNDLE)) {

				eppic_msg("\n"); 
				eppic_indent(level+1, 1);
			}
			eppic_freeval(v);
			eppic_freeval(vr);
			t->idxlst=pi;
		}
	}
	eppic_msg("\n");
	eppic_indent(level, 1);
	eppic_msg("}");
}

/*
	Print a type.
	Typical output of the 'whatis' command.
*/
static
void eppic_ptype2(type_t*t, value_t*v, int level, int indent, char *name, int ref, int justv)
{
int type=t->type;

	eppic_indent(level, indent);
	switch(type) {

		case V_STRUCT: case V_UNION:

			/* make sure we have all the member info */
			eppic_print_ctype(t, v, level, 0, name, ref, justv);
		break;


		case V_TYPEDEF:
			/* no typedef should get here */
			eppic_warning("Typedef in print!");
		break;

		case V_ENUM:
			/* no enum should get here */
			eppic_warning("ENUM in print!");
		break;

		case V_REF:
		{
		int refi=t->ref, ref=refi;

			/* decrement ref if this was declared as a array */
			if(t->idxlst) ref--;

			/* print the referenced type */
			eppic_popref(t, t->ref);
			eppic_ptype2(t, 0, level, 0, 0, 1, justv);
			eppic_pushref(t, refi);

			if(!justv) {

				char buf[100], buf2[100];
				int pos=0, len=sizeof(buf);

				buf[0]='\0';
				if(t->fct) buf[pos++]='(';
				if(pos < len)
					pos += snprintf(buf+pos, len-pos, "%s%s", eppic_getref(ref), name?name:"");
				if(pos < len)
					pos += snprintf(buf+pos, len-pos, "%s", eppic_getidx(t, buf2, sizeof(buf2)));
				if(pos < len && t->fct)
					pos += snprintf(buf+pos, len-pos, "%s", ")()");

				eppic_msg("%*s ", NAMESPACE, buf);
			}

			/* arrays are ref with boundaries... */
			if(t->idxlst && v) {

				if(t->idxlst[1] || t->rtype!=V_BASE || t->size!=1 || !eppic_prtstr(v, justv)) 
				{
					if(!justv) eppic_msg("= ");
					eppic_popref(t, 1);
					eppic_prtarray(t, v->mem, level, 0);
					eppic_pushref(t, 1);
				}

			} else if(v) {

				if(!justv) eppic_msg("= ");
				if(!eppic_getval(v)) eppic_msg("(nil)");
				else {
					if(eppic_defbsize()==8) eppic_msg("0x%016llx", eppic_getval(v));
					else eppic_msg("0x%08x", eppic_getval(v));
				}
				if(t->ref==1 && t->rtype==V_BASE && t->size==1) {

					(void)eppic_prtstr(v, justv);
				}
			}
		}
		break;

		case V_BASE:
		{
			if(eppic_isenum(t->typattr)) {

				stinfo_t *st=eppic_getstbyindex(t->rtype, V_ENUM);
				if(!justv) {
					char buf[200];
					snprintf(buf, sizeof(buf), "enum %s", st->name?st->name:"");
					eppic_msg("%-*s ", SPACER, buf);
					eppic_msg("%*s ", NAMESPACE, (name&&v)?name:"");
				}
				if(v) {

					enum_t *e=st->enums;

					eppic_msg("= ");
					eppic_prbval(v);
					while(e) {

						if(e->value==eppic_getval(v)) {
							eppic_msg(" [%s]", e->name);
							break;
						}
						e=e->next;
					}
					if(!e) eppic_msg(" [???]");

				}else{

					enum_t *e=st->enums;
					int count=0;

					eppic_msg(" {");
					while(e) {

						if(!(count%4)) {
							eppic_msg("\n");
							eppic_indent(level+1, 1);
						}
						count ++;
						eppic_msg("%s = %d, ", e->name, e->value);
						e=e->next;

					}
					eppic_msg("\n");
					eppic_indent(level, 1);
					eppic_msg("%-*s ", SPACER, "}");
					if(ref) return;
					eppic_msg("%*s ", NAMESPACE, name?name:"");
				}

			} else {

				if(!justv) {
					eppic_msg("%-*s " , SPACER , eppic_getbtypename(t->typattr));
					if(ref) return;
					eppic_msg("%s%*s ", eppic_getref(t->ref), NAMESPACE, name?name:"");
				}
				if(v) { 

					if(!justv) eppic_msg("= ");
					eppic_prbval(v);
				}
			}
		}
		break;
		case V_STRING:
			if(!justv) {
				eppic_msg("%-*s " , SPACER , "string");
				eppic_msg("%*s ", NAMESPACE, name?name:"");
			}
			if(v) {

				if(!justv) eppic_msg("= ");
				eppic_msg("\"%s\"", v->v.data);
			}
		break;
	}
	if(indent) eppic_msg("\n");
}

static value_t*
eppic_ptype(value_t*v)
{
	eppic_ptype2(&v->type, 0, 0, 1, 0, 0, 0);
	eppic_msg("\n");
	return 0;
}

void
eppic_print_type(type_t *t)
{
	eppic_ptype2(t, 0, 0, 1, 0, 0, 0);
	eppic_msg("\n");
}

node_t*
eppic_newptype(var_t*v)
{
node_t*n=eppic_newnode();

	n->data=v->next->v;
	v->next->v=0; /* save value against freeing */
	eppic_freevar(v->next);
	eppic_freevar(v);
	n->exe=(xfct_t)eppic_ptype;
	n->free=(ffct_t)eppic_freeval;
	n->name=0;
	eppic_setpos(&n->pos);
	return n;
}

static value_t *
eppic_pval(node_t*n)
{
value_t *v=NODE_EXE(n);
char *name=NODE_NAME(n);

	eppic_ptype2(&v->type, v, 0, 1, name, 0, 0);
	eppic_free(name);
	eppic_freeval(v);
	return 0;
}

node_t*
eppic_newpval(node_t*vn, int fmt)
{
node_t*n=eppic_newnode();

	n->data=vn;
	n->exe=(xfct_t)eppic_pval;
	n->free=(ffct_t)eppic_freenode;
	n->name=0;
	eppic_setpos(&n->pos);
	return n;
}
