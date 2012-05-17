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
#include <termio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "eppic.h"

/* information necessary for a builtin function */
typedef struct builtin {

    var_t*v;        /* resulting variable declaration after parsing */
        bf_t *fp;       /* pointer to actual function */
    char *proto;        /* associated prototype_t*/
    struct builtin *next;   /* to chain them */

} builtin;

#define BT_EINVAL   1   /* Something is wrong and it's not ... */
value_t*
eppic_exit(int v)
{
    /* Were we compiling ? */
    eppic_parseback();

    /* we were running... exit () */
    eppic_dojmp(J_EXIT, &v);

    /* NOT REACHED */
    return 0;
}

value_t*
eppic_bexit(value_t *vv)
{
int v=eppic_getval(vv);

    /* we're not going back to the he caller so free 
       the input value_t */
    eppic_freeval(vv);
    eppic_exit(v);
    /* NOT REACHED */
    return 0;
}

#define MAXBYTES  4000
#define INCREMENT 16
value_t *
eppic_getstr(value_t *vmadr)
{
ull madr=eppic_getval(vmadr);
char *buf=eppic_alloc(MAXBYTES+1);
char *p=buf;
value_t *v;

    /* eppic as already verified that this is a V_REF */
    /* since this is reading from a unkown size pool
       we have to do an exponential reduction on the number of bytes
       read ... */
    buf[0]=0;
    while(1) {

        int i;

        if(!API_GETMEM(madr, p, INCREMENT)) break;

        /* have we found the '\0' yet ? */
        for(i=0;i<INCREMENT; i++) if(!p[i]) break;

        madr+=INCREMENT;
        p+=INCREMENT;
        if((p-buf) >= MAXBYTES) {
            buf[MAXBYTES]='\0';
            break;
        }

    }
    v=eppic_setstrval(eppic_newval(), buf);
    eppic_free(buf);
    return v;
}

value_t *
eppic_substr(value_t *vp, value_t *vi, value_t *vj)
{
char *p=eppic_getptr(vp, char); 
ul i=eppic_getval(vi); 
int l=strlen(p);
int j=(vj?eppic_getval(vj):(l-i+1));
char *s;
value_t *v;

    if((i+j-1)>l || !i) {

        eppic_error("Valid positions are [1..%d]\n", l);

    }

    s=eppic_alloc(j+1);
    strncpy(s, p+i-1, j);
    s[j]='\0';
    v=eppic_setstrval(eppic_newval(), s);
    eppic_free(s);
    return v;
}

value_t *
eppic_getnstr(value_t* vmadr, value_t* vl)
{
ull madr=eppic_getval(vmadr);
ul l=eppic_getval(vl);
char *buf=eppic_alloc(l+1);
value_t *v;

    if(!API_GETMEM(madr, buf, l)) buf[0]='\0';
    else buf[l]='\0';
    v=eppic_setstrval(eppic_newval(), buf);
    eppic_free(buf);
    return v;
}

value_t *
eppic_atoi(value_t *vs, value_t* vbase)
{
char *s=eppic_getptr(vs, char);
int base=vbase ? eppic_getval(vbase) : 0;

    return eppic_defbtypesize(eppic_newval(), strtoull(s, 0, base), B_ULL);
}

value_t *
eppic_itoa(value_t* vi)
{
ull i=eppic_getval(vi);
char p[40];
    
    sprintf(p, "%llu", (unsigned long long)i);
    return eppic_setstrval(eppic_newval(), p);
}

value_t *
eppic_strlen(value_t *vs)
{
char *s=eppic_getptr(vs, char);
ull l;
    if(!s) l=0;
    else l=strlen(s);

    return eppic_defbtype(eppic_newval(), l);
}

value_t *
eppic_getchar(void)
{
char c; 
struct termio tio, stio;
int in=fileno(stdin);

    if(ioctl(in, TCGETA, &tio)) c=255;
    else {
        stio=tio;
        tio.c_lflag &= ~(ICANON | ECHO);
        tio.c_iflag &= ~(ICRNL  | INLCR);
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        ioctl(in, TCSETA, &tio);
        c=getc(stdin);
        ioctl(in, TCSETA, &stio);
    }
    return eppic_defbtype(eppic_newval(), (ull)c);
}

value_t *
eppic_gets(void)
{
char p[1024];
    
    if(!fgets(p, sizeof(p)-1, stdin)) p[0]='\0';
    else p[strlen(p)-1]='\0';
    return eppic_setstrval(eppic_newval(), p);
}

static builtin *bfuncs=0;

/*
    Check for the existance of a bt function
*/
void *
eppic_chkbuiltin(char *name)
{
builtin *bf;

    for(bf=bfuncs; bf; bf=bf->next) {

        if(!strcmp(name, bf->v->name)) {

            return bf;
        }
    }
    return 0;
}

/*
    Remove a builtin.
    This is done when we 'unload' a *.so file.
*/
void
eppic_rmbuiltin(var_t*v)
{
builtin *bf;
builtin *last=0;

    for(bf=bfuncs; bf; bf=bf->next) {

        if(!strcmp(v->name, bf->v->name)) {

            if(!last) bfuncs=bf->next;
            else {

                last->next=bf->next;
            }
            eppic_free(bf->proto);
            eppic_free(bf);
        }
        last=bf;
    }
}

/* 
    Install a new builtin function.
*/
var_t* 
eppic_builtin(char *proto, bf_t* fp)
{
var_t*v;

    /* parse the prototype_t*/
    if((v=eppic_parsexpr(proto))) {

        builtin *bt;
        int nargs=0;

        /* check name */
        if(!v->name || !v->name[0]) {

            eppic_freevar(v);
            eppic_msg("Syntax error: no function name specified [%s]\n", proto);
            return 0;
        }

        /* check for function with same name */
        if(eppic_chkfname(v->name, 0)) {

            eppic_freevar(v);
            eppic_msg("Function already defined [%s]\n", proto);
            return 0;
        }

        if(v->dv->fargs) {

            var_t*vn=v->dv->fargs->next;

            while(vn!=v->dv->fargs) {

                nargs++;
                vn=vn->next;
            }
        }
        /* check number of args */
        if(nargs > BT_MAXARGS) {

            eppic_freevar(v);
            eppic_msg("Too many parameters to function (max=%d) [%s]\n", BT_MAXARGS, proto);
            return 0;
        }


        bt=eppic_alloc(sizeof(builtin));
        bt->proto=eppic_strdup(proto);
        bt->fp=fp;
        bt->v=v;
        bt->next=0;

        /* install it */
        if(!bfuncs) bfuncs=bt;
        else {
            builtin *btp;

            for(btp=bfuncs; ; btp=btp->next) if(!btp->next) break;
            btp->next=bt;
        }
        return v;
    }

    eppic_msg("Builtin [%s] not loaded.", proto);

    return 0;
}

#define bcast(f) ((bf_t*)f)

static btspec_t eppicbfuncs[] = {
    { "unsigned long long atoi(string, ...)",bcast(eppic_atoi)},
    { "int exists(string)",                 bcast(eppic_exists)},
    { "void exit(int)",                     bcast(eppic_bexit)},
    { "int getchar()",                      bcast(eppic_getchar)},
    { "string gets()",                      bcast(eppic_gets)},
    { "string getstr(char *)",              bcast(eppic_getstr)},
    { "string getnstr(char *, int)",        bcast(eppic_getnstr)},
    { "string itoa(int)",                   bcast(eppic_itoa)},
    { "void printf(string, ...)",           bcast(eppic_printf)},
    { "void showtemp()",                    bcast(eppic_showtemp)},
    { "void showaddr(char *)",              bcast(eppic_showaddr)},
    { "void memdebugon()",                  bcast(eppic_memdebugon)},
    { "void memdebugoff()",                 bcast(eppic_memdebugoff)},
    { "int eppic_load(string)",             bcast(eppic_bload)},
    { "int eppic_unload(string)",           bcast(eppic_bunload)},
    { "int depend(string)",                 bcast(eppic_bdepend)},
    { "int strlen(string)",                 bcast(eppic_strlen)},
    { "string sprintf(string, ...)",        bcast(eppic_sprintf)},
    { "string substr(string, int, ...)",    bcast(eppic_substr)},
    { "void prarr(string name, int i)",     bcast(eppic_prarr)},
    { "int member(void*, string name)",     bcast(eppic_ismember)},
    { "string findsym(string)",             bcast(eppic_findsym)},
};


/*
    Install the eppic builtins.
*/
void
eppic_setbuiltins()
{
int i;

    for(i=0;i<sizeof(eppicbfuncs)/sizeof(eppicbfuncs[0]);i++) {

        (void)eppic_builtin(eppicbfuncs[i].proto, eppicbfuncs[i].fp);
    }
}

value_t* 
eppic_exebfunc(char *name, value_t **vals)
{
builtin *bf;
value_t *lvals[BT_MAXARGS*2]; /* use factor 2 for api where char * is 4 bytes */
value_t *v, *vr;


    if((bf=eppic_chkbuiltin(name))) {

        int i=0, nargs=0;

        if(vals) for(i=0;vals[i];i++);

        memset(lvals, 0, sizeof(lvals));

        if(bf->v->dv->fargs) {

            var_t*vv=bf->v->dv->fargs->next;

            while(vv != bf->v->dv->fargs) {

                if(vv->name && !strcmp(vv->name, S_VARARG)) { 
                    while(nargs<i) {

                        lvals[nargs]=eppic_cloneval(vals[nargs]);
                        nargs++;
                    }
                    break; 
                }

                /* verify type compatibility and convert */
                if(vals[nargs]) {

                    lvals[nargs]=eppic_cloneval(vv->v);
                    eppic_chkandconvert(lvals[nargs], vals[nargs]);
                }
                nargs++;
                vv=vv->next;
            }
        }

        /* check parameters number */
        if(i<nargs) {

            eppic_rerror(&bf->v->dv->pos, "Too few parameters to '%s'", bf->proto);

        } else if(i>nargs){

            eppic_rerror(&bf->v->dv->pos, "Too many parameters to '%s'", bf->proto);

        }

        if(vals) {
            /* the actual call */
            v=(bf->fp) ( 
                lvals[0],  lvals[1],
                lvals[2],  lvals[3],
                lvals[4],  lvals[5],
                lvals[6],  lvals[7],
                lvals[8],  lvals[9],
                lvals[10], lvals[11],
                lvals[12], lvals[13],
                lvals[14], lvals[15],
                lvals[16], lvals[17],
                lvals[18], lvals[19]
                );
        } else {

            v=(bf->fp) ((value_t*)0);
        }

        while(i) {

            --i;
            eppic_freeval(vals[i]);
            eppic_freeval(lvals[i]);
        }

        /* make a copy of the return value_t info */
        vr=eppic_cloneval(bf->v->v);
        eppic_chkandconvert(vr, v);
        eppic_freeval(v);

        return vr;
    }

    eppic_error("Oops. eppic_exebfunc()");
    return 0;
}
