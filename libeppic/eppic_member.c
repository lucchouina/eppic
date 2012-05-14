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
#include "eppic.h"
#include "eppic.tab.h"
#include <endian.h>

/* these function are used to access and set members in structs */

/* define a member access */
typedef struct mem {
	char *name;	/* member name */
	int dir;	/* direct/indirect access */
	node_t*expr;	/* expression node_t*/
	stmember_t*stm;	/* associated member information */
	char *local;	/* local memory or ... */
	ull mem;	/* ... system memory access */
	srcpos_t p;
} mem;

void *
eppic_adrval(value_t *v)
{
	switch(v->type.size) {

		case 1: return &v->v.uc;
		case 2: return &v->v.us;
		case 4: return &v->v.ul;
		case 8: return &v->v.ull;
	}
	eppic_error("Oops eppic_adrval");
	return 0;
}

/* some API secondary entry points */
void eppic_member_soffset(member_t*m, int offset) { m->offset=offset; }
void eppic_member_ssize(member_t*m, int size) { m->size=size; }
void eppic_member_sfbit(member_t*m, int fbit) { m->fbit=fbit; }
void eppic_member_snbits(member_t*m, int nbits) { m->nbits=nbits; }
void eppic_member_sname(member_t*m, char *name) { m->name=eppic_strdup(name); }


void
eppic_setmem(mem *m, value_t *v)
{
stmember_t*stm=m->stm;

	/* check type compatibility. Ctypes should point to the same stinfo...*/
	if(stm->type.type != v->type.type
	   /* pointer most point to the same type of object */
	   || (v->type.type==V_REF && v->type.rtype != stm->type.rtype)
	   /* ctypes should point to the same stinfo */
	   || (is_ctype(v->type.type) && v->type.idx != stm->type.idx)) {

		eppic_error("Incompatible types for assignment");

	}

	if(stm->m.nbits) {

		ull dvalue_t=0;

		if(v->type.type!=V_BASE) {

			eppic_error("Invalid assignment to bit field");

		}

		/* do the bit gymnastic */
		/* we need to create a ull that contain the current
		   bit of teh destination */
		if(m->local) {

			memmove(m->local+stm->m.offset, ((char*)(&dvalue_t))+8-stm->m.size, stm->m.size);
			dvalue_t=set_bit_value_t(dvalue_t, v->v.ull, stm->m.nbits, stm->m.fbit);
			memmove(((char*)(&dvalue_t))+8-stm->m.size, m->local+stm->m.offset, stm->m.size);

		}

		if(m->mem) {

			API_GETMEM(m->mem+stm->m.offset, ((char*)(&dvalue_t))+8-stm->m.size, stm->m.size);
			dvalue_t=set_bit_value_t(dvalue_t, v->v.ull, stm->m.nbits, stm->m.fbit);
			API_PUTMEM(m->mem+stm->m.offset, ((char*)(&dvalue_t))+8-stm->m.size, stm->m.size);

		}
		

	} else {

		/* move the data */
		if(is_ctype(v->type.type)) {

			if(m->local) {

				memmove(m->local+stm->m.offset, v->v.data, stm->m.size);

			} 
			if(m->mem) {

				API_PUTMEM(m->mem+stm->m.offset, v->v.data, stm->m.size);
			}

		} else {

			eppic_transval(v->type.size, stm->m.size, v, eppic_issigned(v->type.typattr));

			if(m->local) {

				memmove(m->local+stm->m.offset, eppic_adrval(v), stm->m.size);

			}

			if(m->mem) {

				API_PUTMEM(m->mem+stm->m.offset, eppic_adrval(v), stm->m.size);
			}
		}
	}
}

#define vdata(p, t) ((t*)(p->v.data))

void
eppic_exememlocal(value_t *vp, stmember_t* stm, value_t *v)
{
	/* expression should be a ctype_t*/
	if(!is_ctype(vp->type.type)) {

		eppic_error("Invalid type for '.' expression");
	}
	/* get that value_t from the application memory */
	if(is_ctype(stm->type.type) && !stm->type.idxlst) {

		void *data=eppic_alloc(stm->m.size);

		memmove(data, vdata(vp, char)+stm->m.offset, stm->m.size);
		if(vp->mem) v->mem=vp->mem+stm->m.offset;
		v->v.data=data;

	}
	/* bit field gymnastic */
	else if(stm->m.nbits) {

		ull value=0;

		memmove(vdata(vp, char)+stm->m.offset, ((char*)&value)+(sizeof(value)-stm->m.size), stm->m.size);
		get_bit_value(value, stm->m.nbits, stm->m.fbit, stm->m.size, v);

	} 
	/* check if this is an array, if so then create a reference to it */
	else if(stm->type.idxlst) {

		ull mempos=vp->mem+stm->m.offset;
		if(eppic_defbsize()==8) v->v.ull=mempos;
		else v->v.ul=mempos;
		v->mem=mempos;

	} else {

		switch(TYPE_SIZE(&stm->type)) {
			case 1:
				memmove(&v->v.uc, vdata(vp, char)+stm->m.offset, 1);
			break;
			case 2:
				memmove(&v->v.us, vdata(vp, char)+stm->m.offset, 2);
			break;
			case 4:
				memmove(&v->v.ul, vdata(vp, char)+stm->m.offset, 4);
			break;
			case 8:
				memmove(&v->v.ull, vdata(vp, char)+stm->m.offset, 8);
			break;
			default:
				eppic_error("Oops exemem2[%d]", TYPE_SIZE(&stm->type));
			break;
		}
		if(vp->mem) v->mem=vp->mem+stm->m.offset;
	}
}

value_t *
eppic_exemem(mem *m)
{
value_t *v=eppic_newval();
value_t *vp=NODE_EXE(m->expr);
stmember_t*stm;
srcpos_t p;

	eppic_curpos(&m->p, &p);
    


	if(vp->type.type == V_REF) {

		if(vp->type.ref > 1) {

			eppic_error("Too many levels of indirection for access to [%s]", m->name);

		}
	}

	/* get the member information and attach it */
	stm=m->stm=(stmember_t*)eppic_member(m->name, &vp->type);
	if(!stm) {

		eppic_freeval(v);
		eppic_freeval(vp);
		eppic_error("Invalid member name specified : %s", m->name);

	}

	/* get a copy of the type of thise member and put it in v */
	eppic_duptype(&v->type, &stm->type);

	/* indirect i.e. (struct*)->member *most* be relative to the 
	   system image. This is a restriction of this language */
	if(m->dir==INDIRECT) {

		ull mempos;

		if(vp->type.type != V_REF  || !is_ctype(vp->type.rtype)) {

			eppic_error("Invalid type for '->' expression");
		}

		m->local=0;
		m->mem=eppic_defbsize()==8?vp->v.ull:vp->v.ul;
		mempos=m->mem+stm->m.offset;

		/* get that value_t from the system image */
		if(is_ctype(v->type.type) && !stm->type.idxlst) {

			v->v.data=eppic_alloc(stm->m.size);
			API_GETMEM(mempos, v->v.data, stm->m.size);
			v->mem=mempos;

		}
		/* bit field gymnastic */
		else if(stm->m.nbits) {
			ull value=0;
			void *target = &value;

			if (__BYTE_ORDER != __LITTLE_ENDIAN)
				target = target + (sizeof(value) - stm->m.size);

			API_GETMEM(m->mem+stm->m.offset, target, stm->m.size);
			get_bit_value(value, stm->m.nbits, stm->m.fbit, stm->m.size, v);
			/* no mempos for bit fields ... */

		} 
		/* check if this is an array, if so then create a reference to it */
		else if(stm->type.idxlst) {

			if(eppic_defbsize()==8) v->v.ull=mempos;
			else v->v.ul=mempos;
			v->mem=mempos;

		} else {

			v->mem=mempos;

			switch(TYPE_SIZE(&stm->type)) {
				case 1:
					API_GETMEM(mempos, &v->v.uc, 1);
				break;
				case 2:
					API_GETMEM(mempos, &v->v.us, 2);
				break;
				case 4:
					API_GETMEM(mempos, &v->v.ul, 4);
				break;
				case 8:
					API_GETMEM(mempos, &v->v.ull, 8);
				break;
				default:
					eppic_error("Oops exemem[%d]", TYPE_SIZE(&stm->type));
				break;
			}

		}
	}
	/* direct i.e. (struct).member *most* be in referance to a local 
	   structure. */
	else {

		m->mem=vp->mem;
		m->local=vp->v.data;

		/* extract the value from a local copy */
		eppic_exememlocal(vp, stm, v);
	}
	eppic_curpos(&p, 0);
	eppic_freeval(vp);
	v->setfct=(setfct_t)eppic_setmem;
	v->setval=(value_t*)m;
	v->set=1;
	return v;
}

void
eppic_freemem(mem *m)
{
	NODE_FREE(m->expr);
	eppic_free(m->name);
	eppic_free(m);
}

node_t*
eppic_newmem(int dir, node_t*expr, node_t*name)
{
char *nstr=NODE_NAME(name);
node_t*n=eppic_newnode();
mem *m=eppic_alloc(sizeof(mem));

	/* dicard nam  node_t*/
	NODE_FREE(name);
	m->name=nstr;
	m->dir=dir;
	m->expr=expr;
	eppic_setpos(&m->p);
	n->data=m;
	n->exe=(xfct_t)eppic_exemem;
	n->free=(ffct_t)eppic_freemem;
	return n;
}
