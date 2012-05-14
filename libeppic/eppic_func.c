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
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include "eppic.h"

/*
	The next few functions manege the files and associated functions.
*/
struct fdata;

typedef struct fctype_t {
	int idx;
	struct fctype_t*next;

} fctype_t;

typedef struct func {

	char *name;	 	/* name of the function */
	var_t*varlist;		/* parameters information */
	var_t*rvar;		/* return value_t information */
	node_t*body;		/* execution node for body */
	int local;		/* load i.e. static ? */
	srcpos_t pos;		/* source position of function declaration */
	struct fdata *file;	/* back pointer to corresponding file */
	struct func *next;	/* linked list */

} func;

typedef struct fdata {

	char *fname;		/* name of the file */
	int  isdso;		/* is this from a loadable module ? 
				   `globs' becomes the handle */
	time_t time;		/* load time */
	var_t*fsvs;		/* associated list of static variables */
	var_t*fgvs;		/* associated list of global variables */
	void *globs;		/* handle for these globals */
	func *funcs;		/* chained list of functions */
	fctype_t *ctypes;	/* ctypes declared by this function */
	struct fdata *next; 	/* chained list of files */

} fdata;

static fdata *fall=0;
void eppicparse(void);
static func * eppic_getfbyname(char *name, fdata *thisfd);
value_t * eppic_execmcfunc(func *f, value_t **vp);

ull
eppic_getval(value_t*v)
{
ull ret=0;

	if(!v) return 0;

	/* need to cast properly here */
	if(v->type.type==V_BASE || v->type.type==V_REF) {

		if(v->type.type==V_REF || !eppic_issigned(v->type.typattr)) {

			switch(TYPE_SIZE(&v->type)) {
				case 1: ret= (ull) v->v.uc; break;
				case 2: ret= (ull) v->v.us; break;
				case 4: ret= (ull) v->v.ul; break;
				case 8: ret= (ull) v->v.ull; break;
				default: eppic_error("Oops getval base");
			}

		} else {

			switch(TYPE_SIZE(&v->type)) {
				case 1: ret= (ull) v->v.sc; break;
				case 2: ret= (ull) v->v.ss; break;
				case 4: ret= (ull) v->v.sl; break;
				case 8: ret= (ull) v->v.sll; break;
				default: eppic_error("Oops getval base");
			}
		}
	}
	/* in the case of a struct/union we pass a pointer to it */
	else ret = (unsigned long)v->v.data;
	return ret;
}

static int
eppic_dohelp(char *fname)
{
char buf[MAX_SYMNAMELEN+1];
char *hstr;

	sprintf(buf, "%s_help", fname);

	if(eppic_chkfname(buf, 0)) {

		char buf2[MAX_SYMNAMELEN+1];
		char *ustr;

		sprintf(buf2, "%s_usage", fname);
		ustr=(char*)(unsigned long)eppic_exefunc(buf2, 0);
		eppic_msg("COMMAND: %s %s\n\n", fname , ustr?ustr:"");
		hstr=(char*)(unsigned long)eppic_exefunc(buf, 0);
		eppic_format(1, hstr);
		eppic_format(0, "\n");
		eppic_msg("\n");
		return 1;
	}
	return 0;
}

void
eppic_showallhelp()
{
fdata *filep;

	for(filep=fall; filep; filep=filep->next) {

		func *funcp;

		for(funcp=filep->funcs;funcp;funcp=funcp->next) {

			(void)eppic_dohelp(funcp->name);
			
		}
	}
}

int
eppic_showhelp(char *fname)
{
	return eppic_dohelp(fname);
}

void*
eppic_getcurfile() { return fall; }

int
eppic_isnew(void *p)
{
fdata *fd=(fdata *)p;
struct stat stats;

	if(!stat(fd->fname, &stats)) {

		if(stats.st_mtime > fd->time) {

			return 1;
		}
	}
	return 0;
}

void *
eppic_findfile(char *name, int unlink)
{
fdata *fd;
fdata *last=0;

	for(fd=fall; fd; last=fd, fd=fd->next) {

		if(!strcmp(fd->fname, name)) {

			/* remove from the list ?*/
			if(unlink) {

				if(!last) fall=fd->next;
				else last->next=fd->next;
			
			}
			return fd;
		}

	}
	return 0;
}

void
eppic_freefunc(func *fn)
{
	eppic_free(fn->name);
	NODE_FREE(fn->body);
	if(fn->varlist) eppic_freesvs(fn->varlist);
	eppic_freevar(fn->rvar);
	eppic_free(fn);
}

static void
eppic_unloadso(fdata *fd)
{
typedef int (*fp_t)(void);
fp_t fp;
func *f;

	if((fp=(fp_t)dlsym(fd->globs, BT_ENDDSO_SYM))) {

		fp();
	}
	for(f=fd->funcs; f; ) {

		func *n=f->next;
		eppic_rmbuiltin(f->varlist);
		eppic_freevar(f->varlist);
		eppic_free(f);
		f=n;
	}

	dlclose(fd->globs);

	if(fall==fd) fall=fd->next;
	else {

		fdata *last=fall;

		while(last->next) {

			if(last->next==fd) {

				last->next=fd->next;
				break;
			}
			last=last->next;
		}
	}

	/* free the associated static and global variables */
	if(fd->fsvs) eppic_freesvs(fd->fsvs);
	if(fd->fgvs) eppic_freesvs(fd->fgvs);
	eppic_free(fd->fname);
	eppic_free(fd);
}

static void (*cb)(char *, int)=0;
void eppic_setcallback(void (*scb)(char *, int))
{ cb=scb; }
static void
eppic_docallback( fdata *fd, int load)
{
func *f;

	if(!cb) return;

	for(f=fd->funcs; f; f=f->next) {

		cb(f->name, load);
	}
}

void
eppic_freefile(fdata *fd)
{
	if(fd) {

		func *fct, *nxt;
		fctype_t *ct, *nct;

		if(fd->isdso) {

			eppic_unloadso(fd);
			return;
		}

		/* free the associated static and global variables */
		if(fd->fsvs) {
			eppic_freesvs(fd->fsvs);
			fd->fsvs = NULL;
		}
		if(fd->fgvs) {
			eppic_freesvs(fd->fgvs);
			fd->fgvs = NULL;
		}

		/* free all function nodes */
                // let debugger know ...
                eppic_docallback(fd, 0);
		for(fct=fd->funcs; fct; fct=nxt) {

			nxt=fct->next;
			eppic_freefunc(fct);
		}

		for(ct=fd->ctypes; ct; ct=nct) {

			nct=ct->next;
			eppic_free(ct);
		}
		eppic_free(fd->fname);
		if(fd->globs) eppic_rm_globals(fd->globs);
		eppic_free(fd);
	}
	else eppic_warning("Oops freefile!");
}

int
eppic_deletefile(char *name)
{
fdata *fd=eppic_findfile(name, 0);

	if(fd) {

		eppic_freefile(fd);
                (void)eppic_findfile(name, 1);
		return 1;

	}
	return 0;
}

static int parsing=0;
static jmp_buf parjmp;

void
eppic_parseback(void)
{
	if(parsing) {

		parsing=0;
		longjmp(parjmp, 1);
	}
}

/* link in a new set of static file variables */
int
eppic_file_decl(var_t*svs)
{
	eppic_validate_vars(svs);

	if(!fall->fsvs)
		fall->fsvs=(void*)eppic_newvlist();

	if(!fall->fgvs)
		fall->fgvs=(void*)eppic_newvlist();

	(void)eppic_addnewsvs(fall->fgvs, fall->fsvs, svs);
	
	return 1;
}

typedef struct sigaction sact;
/* __UNDO__ static int sigs[]={SIGSEGV, SIGILL, SIGTRAP, SIGINT, SIGPIPE}; */
static int sigs[]={SIGTRAP, SIGINT, SIGPIPE};
#define S_NSIG (sizeof(sigs)/sizeof(sigs[0]))

void
eppic_except_handler(int sig)
{
static int i=0;
	if(sig != SIGPIPE && sig != SIGINT) eppic_error("Exception caught!");
	eppic_dojmp(J_EXIT, &i);
}

void *
eppic_setexcept()
{
int i;
sact *osa=eppic_alloc(S_NSIG*sizeof(sact));
#if linux
sact na;

	memset(&na, 0, sizeof(na));
	na.sa_handler=eppic_except_handler;
	na.sa_flags=SA_NODEFER;

#else
sact na={ SA_NODEFER+SA_SIGINFO, eppic_except_handler, 0, 0 };
#endif
	

	for(i=0;i<S_NSIG;i++) {
		if(sigaction(sigs[i], &na, &osa[i])) {
			eppic_msg("Oops! Could'nt set handlers!");
		}
	}
	return osa;
}

void
eppic_rmexcept(void *sa)
{
sact *osa=(sact *)sa;
int i;

	for(i=0;i<S_NSIG;i++) {
		sigaction(sigs[i], &osa[i], 0);
	}
	eppic_free(osa);
}

/*
	This function is used to compile the prototype
	information being given for a builtin function.

	We push a dummy fdata on the stacj so that the declared
	function ends up in the global list of that fdata.
*/
var_t*
eppic_parsexpr(char *expr)
{
fdata *fd=eppic_calloc(sizeof(fdata));
char *exp2=eppic_alloc(strlen(expr)+2);
var_t*ret=0;

	strcpy(exp2, expr);
	strcat(exp2, ";");

	/* put it on the list */
	fd->fname="__expr__";
	fd->next=fall;
	fall=fd;

	eppic_pushbuf(exp2, "stdin", 0, 0, 0);
	parsing=1;
	if(!setjmp(parjmp)) {

		eppic_rsteofoneol();
		eppic_settakeproto(1);
		eppicparse();
		eppic_settakeproto(0);

		/* remove longjump for parsing */
		parsing=0;

		if(!fall->fgvs) {

			eppic_error("Invalid function declaration.");

		} 

		ret=fall->fgvs->next;

	} else {

		eppic_popallin();
		ret=0;

	}
	eppic_free(exp2);
	/* only free the top of the fgvs list to keep 'ret' */
	if(fall->fgvs) eppic_freevar(fall->fgvs);
	if(fall->fsvs) eppic_freesvs(fall->fsvs);
	fall=fd->next;
	eppic_free(fd);
	return ret;
}

/*
	Load a dso file.
	We are looking for the btinit() and btshutdown() functions.

	btinit() will should initialized the package and call eppic_builtin()
	to install the eppic functions.

	btshutdown(), if it exists, will be called when an unload of the 
	file is requested. The dso should deallocate memory etc... at that
	time.
*/
static int
eppic_loadso(char *fname, int silent)
{
void *h;

	if((h=dlopen(fname, RTLD_LAZY))) {

		typedef int (*fp_t)(void);
		fp_t fp;

		if((fp=(fp_t)dlsym(h, BT_INIDSO_SYM))) {

			btspec_t *sp;

			if(fp()) {

				if((sp=(btspec_t *)dlsym(h, BT_SPEC_SYM))) {

					int i;
					fdata *fd=eppic_calloc(sizeof(fdata));
					func **ff=&fd->funcs;

					fd->fname=fname;
					fd->isdso=1;
					fd->globs=h;

					for(i=0;sp[i].proto;i++) {

						var_t*v;

						if((v=eppic_builtin(sp[i].proto, sp[i].fp))) {

							func *f=eppic_alloc(sizeof(func));

							f->varlist=v;
							f->next=*ff;
							*ff=f;
						}
					}
					fd->next=fall;
					fall=fd;
					return 1;

				} else if(!silent) {

					eppic_msg("Missing '%s' table in dso [%s]", BT_SPEC_SYM, fname);

				}

			} else if(!silent) {

				eppic_msg("Could not initialize dso [%s]", fname);

			}

		} else if(!silent) {

			eppic_msg("Missing '%s' function in dso [%s]", BT_INIDSO_SYM, fname);
		}
		dlclose(h);
	}
	else if(!silent) eppic_msg(dlerror());
	eppic_free(fname);
	return 0;
}

void
eppic_addfunc_ctype(int idx)
{
fctype_t *fct=eppic_alloc(sizeof(fctype_t));

	fct->idx=idx;
	fct->next=fall->ctypes;
	fall->ctypes=fct;
}

int 
eppic_newfile(char *name, int silent)
{
fdata *fd;
fdata *oldf;
char *fname=eppic_strdup(name);
void *mtag;

	/* check if this is a dso type file */
	if(!strcmp(fname+strlen(fname)-3, ".so")) {

		if(eppic_findfile(name,0)) {

			if(!silent) 
				eppic_msg("Warning: dso must be unloaded before reload\n");
			return 0;
		}
		return eppic_loadso(fname, silent);
		
	}

	fd=eppic_calloc(sizeof(fdata));
	oldf=eppic_findfile(name,1);

	/* push this file onto the parser stack */
	if(!eppic_pushfile(fname)) {

		eppic_free(fname);
		if(!silent && errno != EISDIR) eppic_msg("File %s : %s\n", name, strerror(errno));
		return 0;
	}

	/* we also need to remove the globals for this file
	   before starting the parsing */
	if(oldf && oldf->globs) {

		eppic_rm_globals(oldf->globs);
		oldf->globs=0;

	}

	needvar=instruct=0;

	fd->fname=fname;

	/* put it on the list */
	fd->next=fall;
	fall=fd;
	
	/* we tag the current ctype list so we know later what to clean up */
	eppic_tagst();

	/* we also tag the macro stack so we can erase out defines and
	   keep the compiler and api ones. */
	mtag=eppic_curmac();

	parsing=1;
	if(!setjmp(parjmp)) {

		func *fct;
		int ret=1;

		/* parse it */
		eppic_rsteofoneol();

		eppicparse();

		/* remove longjump for parsing */
		parsing=0;

		/* before adding the globals we need to push all the static 
		   variables for this file since the initialization expressions
		   might use them (e.g. sizeof('a static var')). Eh, as long as
		   we keep the interpreter handling a superset of the 'standard' C
		   I don't have a problem with it. Do you ? */

		{
			int lev;

			lev=eppic_addsvs(S_STAT, fd->fsvs);

			/* ok to add the resulting globals now */
			fall->globs=eppic_add_globals(fall->fgvs);

			eppic_setsvlev(lev);
		}

		/* ok to free olf version */
		if(oldf) eppic_freefile(oldf);

		eppic_flushtdefs();
		eppic_flushmacs(mtag);

		/* we proceed with the callback */
		eppic_docallback(fd, 1);

		fd->time=time(0);

		/* compilation was ok , check for a __init() function to execute */
		if((fct=eppic_getfbyname("__init", fd))) {

			int *exval;
			jmp_buf exitjmp;
			sact *sa;

			sa=eppic_setexcept();

			if(!setjmp(exitjmp)) {

				eppic_pushjmp(J_EXIT, &exitjmp, &exval);
				eppic_freeval(eppic_execmcfunc(fct, 0));
				eppic_rmexcept(sa);
				eppic_popjmp(J_EXIT);

			}
			else {

				eppic_rmexcept(sa);
				ret=0;
			}

		}
		return ret;
	}
	else {

		/* remove all streams from the stack */
		eppic_popallin();

		/* error, free this partial one and reinstall old one */
		if(oldf) {
			/* we zap the top pointer (it's fd) */
			oldf->next=fall->next;
			fall=oldf;
			oldf->globs=eppic_add_globals(oldf->fgvs);
		}
		else {

			fall=fall->next;
		}

		/* and free fd */
		eppic_freefile(fd);
	}
	eppic_setsvlev(0);
    eppic_flushtdefs();
	eppic_flushmacs(mtag);
	return 0;
}

/* scan the current list of functions for the one named name */
static func *
eppic_getfbyname(char *name, fdata *thisfd)
{
fdata *fd;

	/* check localy first */
	if(thisfd) {

		for(fd=fall; fd; fd=fd->next) {

			func *f;

			if(fd->isdso) continue;

			/* skip non-local function */
			if(thisfd != fd) continue;

			for(f=fd->funcs; f; f=f->next) {

				if(!strcmp(f->name, name)) return f;
			}
		}
	}

	/* check global function */
	for(fd=fall; fd; fd=fd->next) {

		func *f;

		if(fd->isdso) continue;

		for(f=fd->funcs; f; f=f->next) {

			/* skip static functions not local */
			if(f->local) continue;

			if(!strcmp(f->name, name)) return f;
		}
	}
	return 0;
}

/* external boolean to check if a function exists */
int eppic_funcexists(char *name)
{
	return !(!(eppic_getfbyname(name, 0)));
}

/*
	This combined set of functions enables the aplication to
	get alist of currently defined commands that have a help.
*/
static fdata *nxtfdata=0;
static func *nxtfunc;
void
eppic_rstscan(void)
{
	nxtfdata=0;
}
char *
eppic_getnxtfct(void)
{
	if(!nxtfdata) {

		if(!fall) return 0;
		nxtfdata=fall;
		nxtfunc=nxtfdata->funcs;;
	}

	while(nxtfdata) {

		if(!nxtfdata->isdso) for(; nxtfunc; nxtfunc=nxtfunc->next) {

			int l=strlen(nxtfunc->name);

			if(l > 5) {

				if(!strcmp(nxtfunc->name+l-5, "_help")) {

					char buf[MAX_SYMNAMELEN+1];
					func *ret;

					strncpy(buf, nxtfunc->name, l-5);
					buf[l-5]='\0';

					/* make sure we do have the function */
					if((ret=eppic_getfbyname(buf, 0))) {

						nxtfunc=nxtfunc->next;
						return ret->name;
					}
				}
			}
		}
		nxtfdata=nxtfdata->next;
		if(nxtfdata) nxtfunc=nxtfdata->funcs;
	}
	eppic_rstscan();
	return 0;
}

/*
	This is the entry point for the error handling 
*/
void
eppic_exevi(char *fname, int line)
{
char buf[200];
char *ed=getenv("EDITOR");

	if(!ed) ed="vi";
	snprintf(buf, sizeof(buf), "%s +%d %s", ed, line, fname);
	if(!system(buf)) eppic_load(fname);
}

/*
	This funciton is called to start a vi session on a function
	(file=0) or a file (file=1);
*/
void
eppic_vi(char *fname, int file)
{
int line, freeit=0;
char *filename;

	if(file) {

		filename=eppic_filempath(fname);

		if(!filename) {

			eppic_msg("File not found : %s\n", fname);
			return;

		}

		line=1;
		freeit=1;


	} else {

		func *f=eppic_getfbyname(fname, 0);

		if(!f) {

			eppic_msg("Function not found : %s\n", fname);
			return;

		} else {

			filename=f->pos.file;
			line=f->pos.line;

		}
	}

	eppic_exevi(filename, line);

	if(freeit) eppic_free(filename);

}

char *
eppic_getfile(char *fname)
{
func *f;

	if((f=eppic_getfbyname(fname, 0))) return f->file->fname;
	return 0;
}

static void
eppic_insertfunc(func *f)
{
	f->next=fall->funcs;
	fall->funcs=f;
}

value_t *
eppic_execmcfunc(func *f, value_t **vp)
{
value_t *retval;
jmp_buf env;
var_t*parm=0;
int i=0;
char *ocurp, *curp;

	/* set the current path */
	{
	char *p;

		curp=eppic_strdup(f->file->fname);
		if((p=strrchr(curp, '/'))) *p='\0';
		ocurp=eppic_curp(curp);
	}
		

	if(!(setjmp(env))) {

		/* push a return level */
		eppic_pushjmp(J_RETURN, &env, &retval);

		/* Now it's ok to add any static vars for this file */
		eppic_addsvs(S_FILE, f->file->fsvs);

		/* we need to create brand new variables with 
		   the name of the declared arguments */
		if(f->varlist) {

			for(i=0, parm=f->varlist->next; 
			    vp && (parm != f->varlist) && vp[i];
			    parm=parm->next, i++) {

				var_t*var=eppic_newvar(parm->name);

				var->v=eppic_cloneval(parm->v);
				eppic_chkandconvert(var->v, vp[i]);
				eppic_add_auto(var);
				eppic_freeval(vp[i]);
		
			}
		}
		if(vp && vp[i]) {

			eppic_warning("Too many parameters to function call");

		} else if(parm != f->varlist) {

			eppic_warning("Not enough parameters for function call");
		}

		/* we execute the buddy of the function */
		retval=NODE_EXE(f->body);

		eppic_freeval(retval);

		retval=0;

		eppic_popjmp(J_RETURN);
	}

	/* make sure non void function do return something */
	if(!retval) {

		if(!eppic_isvoid(f->rvar->v->type.typattr))

			eppic_rwarning(&f->pos, "Non void function should return a value.");

	} else {

		/* type checking here ... */
	}

	eppic_curp(ocurp);
	eppic_free(curp);

	return retval;
}

/* this is the externalized function that the API users call to execute 
   a function */
ull
eppic_exefunc(char *fname, value_t **vp)
{
func *f;
ull ret;

	if(!eppic_chkfname(fname, 0))
		eppic_warning("Unknown function called: %s\n", fname);

	/* builtin vs cmc ...*/
	if((f=eppic_getfbyname(fname, 0))) ret=eppic_getval(eppic_execmcfunc(f, vp));
	else ret=eppic_getval(eppic_exebfunc(fname, vp));
	/* eppic_freeval(v); */
	return ret;
}

value_t *
eppic_exefunc_common(char *fname, node_t*parms, fdata *fd)
{
int i;
node_t*args;
value_t *vp[BT_MAXARGS+1];
func *f;

	/* We most execute before pushing the S_FILE vars so the the
	   local variable for the caller can still be accessed */
	for(i=0,args=parms; args; args=args->next) {

		if(i==BT_MAXARGS) {

			eppic_error("Max number of parameters exceeded [%d]", BT_MAXARGS);
		}
		vp[i++]=NODE_EXE(args);

	}

	/* null out the rest */
	for(;i<=BT_MAXARGS;i++) vp[i]=0;

	/* builtin vs cmc ...*/
	if((f=eppic_getfbyname(fname, fd))) return eppic_execmcfunc(f, vp);
	else return eppic_exebfunc(fname, vp);
}


/* this function is called by the eppic_exeop() through a CALL op. */
value_t *
eppic_docall(node_t*name, node_t*parms, void *arg)
{
fdata *fd = arg;
char *sname=eppic_vartofunc(name);
value_t *v=0;

	if(eppic_chkfname(sname, fd)) {

		v=eppic_exefunc_common(sname, parms, fd);

	}
	else eppic_rerror(&name->pos, "Unknown function being called:[%s]", sname, fd);
	/* eppic_vartofunc() allocates the name */
	/* we don't free this item if mem debug has been set */
	if(!eppic_ismemdebug()) eppic_free(sname);
	return v;

}

int 
eppic_newfunc(var_t*fvar, node_t* body)
{
var_t*v=fvar->next;

	if(v == fvar) {

		eppic_freevar(v);
		NODE_FREE(body);
		eppic_error("Syntax error in function declaration");

	}else{

		func *fn, *fi ;

		eppic_freevar(fvar);

		/* we do the func insertion first so that if we have a problem
		   we can jump our of the parser using the eppic_parback() function 
		   which will deallocate the stuff */

		fn=eppic_alloc(sizeof(func));
		if(eppic_isstatic(v->v->type.typattr)) fn->local=1;
		fn->rvar=v;
		fn->varlist=v->dv->fargs;

		/* check for func(void) */
		if(fn->varlist && fn->varlist->next != fn->varlist) {

			var_t*v=fn->varlist->next;

			if(v->v->type.type != V_REF && eppic_isvoid(v->v->type.typattr)) {

				/* cut the chain here */
				if(v->next != fn->varlist) {

					eppic_error("function parameter cannot have 'void' type");
				}
				eppic_freesvs(fn->varlist);
				fn->varlist=0;
			}
		}

		v->dv->fargs=0;
		fn->name=eppic_strdup(v->name);
		fn->local=eppic_isstatic(v->v->type.typattr)?1:0;
		fn->body=body;
		fn->file=fall;

		/* the position of the function is the position of the var_t*/
		memcpy(&fn->pos, &v->dv->pos, sizeof(srcpos_t));

		/* emit a warning for variables in the main statement group that
		   shadow ont of the parameters */
		if(fn->varlist) {

			var_t*v;

			for(v=fn->varlist->next; v!=fn->varlist; v=v->next) {

				var_t*vs;

				if((vs=eppic_inlist(v->name, eppic_getsgrp_avs(body))) ||
				   (vs=eppic_inlist(v->name, eppic_getsgrp_svs(body)))) {

					eppic_rwarning(&vs->dv->pos, "variable '%s' shadow's a function parameter"
						, v->name);

				}
			}
		}

		if((fi=eppic_getfbyname(fn->name, fall))) {

			/* check for local conflicts */
			if(fi->file == fn->file) {

				eppic_insertfunc(fn);
				eppic_rerror(&fn->pos, "Function '%s' redefinition, first defined in file '%s' line %d"
					, fn->name, fi->pos.file, fi->pos.line);
	
			/* check for global conflicts */
			} else if(!fn->local) {

				eppic_insertfunc(fn);
				eppic_rerror(&fn->pos, "Function '%s' already defined in file %s, line %d"
					, fn->name, fi->pos.file, fi->pos.line);

			} /* else... it's a static that shadows a global somewhere else. So it's ok */

		}

		/* Searching is all done, so insert it */
		eppic_insertfunc(fn);

		/* check out the storage class. Only 'static' is supported */
		if(!eppic_isjuststatic(v->v->type.typattr)) {

			eppic_error("Only 'static' storage class is valid for a function");
		}
	}
       	return 1;
}

/* check for the existance of a function in the list */
int
eppic_chkfname(char *fname, void *vfd)
{
fdata *fd=(fdata *)vfd;

	/* check script functions */
	if(!eppic_getfbyname(fname, fd)) {

		/* check builtin list */
		if(eppic_chkbuiltin(fname)) return 1;
		return 0;

	}
	return 1;
}

/*

	Thsi is the interface function with the command interpreter.
	It needs to be able to execute a function giving a name and
	passing some random parameters to it.

	A return of 0 means "no such function".
*/
int
eppic_runcmd(char *fname, var_t*args)
{
	if(eppic_chkfname(fname, 0)) {

		value_t *val;
		int *exval;
		jmp_buf exitjmp;
		void *vp;
		ull ret;
		sact *sa;

		/* make sure arguments are available in the global vars */
		vp=eppic_add_globals(args);

		/* we set the exception handler too... */
		sa=eppic_setexcept();

		if(!setjmp(exitjmp)) {

			eppic_pushjmp(J_EXIT, &exitjmp, &exval);

			/* we need to create a var with that name */
			val=eppic_exefunc_common(fname, 0, 0);

			eppic_popjmp(J_EXIT);

			if(val) {

				ret=unival(val);
				eppic_freeval(val);
			}
			else ret=0;
		}
		else {

			ret=*exval;
		}

		/* remove exception handlers and restore previous handlers */
		eppic_rmexcept(sa);

		/* remove args from global vars */
		eppic_rm_globals(vp);
		return ret;
	}
	return 0;
}

