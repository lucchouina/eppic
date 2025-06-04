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
#include <string.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <ctype.h>
#include <stdarg.h>
#include <malloc.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdlib.h>
#include <regex.h>
#include <ctype.h>

static FILE *ofile=0;
static int cols=25;
static char *bold_on, *bold_off;


static void
eppic_getwinsize(void)
{
struct winsize w;

    if (ioctl (fileno(ofile), TIOCGWINSZ, &w) == 0)
    {
        cols=w.ws_col;
    }
    else /* use ENV */
    {
    char *ewidth;
        if ((ewidth = getenv ("COLUMNS")))
        cols = atoi (ewidth);
        /* use what's in terminfo */
        if (cols <= 0)
        cols = tigetnum ("co");
    }
    if(cols <= 10) cols=10;
    if(cols > 80) cols=80;
}

void
eppic_setofile(void * f)
{
int out;
int ret;
char *term;

    ofile=(FILE *)f;

    bold_on="";
    bold_off="";
    cols=80;

    out=fileno(ofile);
    if(isatty(out))
    {

        if(!(term = getenv ("TERM"))) term="dumb";
        if(setupterm(term, out, &ret)!=ERR)
        {
                bold_on=tigetstr("bold");
        if(!bold_on) bold_on="";
                bold_off=tigetstr("sgr0");
        if(!bold_off) bold_off="";
        }
        eppic_getwinsize();
    }
}

void *
eppic_getofile(void)
{
    return ofile;
}

/*
    Output a line of text to the screen with line wrap
    and escape sequence.
*/
#define ESC '<'
#define ESC2 '>'

static int
eppic_tabs(int tabs, char *t, int lf)
{
int i;

    if(lf) fprintf(ofile, "\n");
    for(i=0;i <tabs; i++) fprintf(ofile, "%s", t);
    return tabs*4;
}

void
eppic_format(int tabs, char *str)
{
char *t="    ";
char *p;
int n;
int mode=0;

    n=eppic_tabs(tabs, t, 0);
    eppic_getwinsize();
        
    for(p=str; *p; p++) {


        /* check for escape */
        if(!mode && *p == ESC && *(p+1) && *(p+1) == ESC) {

            fprintf(ofile, "%s", bold_on);
            p++;
            mode=1;

        } else if(mode && *p == ESC2 && *(p+1) && *(p+1) == ESC2) {

            fprintf(ofile, "%s", bold_off);
            p++;
            mode=0;

        } else if(*p==' ' || *p=='\t' ) {

            char *p2;
            int wl;

            for(p2=p+1; *p2 && *p2 != ' ' && *p2 != '\t'; p2++);

            wl=p2-p-1;

            if(wl > cols) {

                char *p3=p+(cols-n-1);

                char c=*p3;
                char c2=*(p3+1);

                *p3='-';
                *(p3+1)='\0';

                fprintf(ofile, "%s", p);
                *p3=c;
                *(p3+1)=c2;
                n=eppic_tabs(tabs, t, 0);

            } else if(n + (p2-p) >= cols) {

                n=eppic_tabs(tabs, t, 1);

            } else {

                fprintf(ofile, " ");
                n++;
            }

        } else if(*p=='\n') {

            n=eppic_tabs(tabs, t, 1);
            
        } else {

            fprintf(ofile, "%c", *p);
            n++;
        }
    }

}

void
eppic_msg(char *fmt, ...)
{
va_list ap;
    va_start(ap, fmt);
    vfprintf(ofile, fmt, ap);
    va_end(ap);
}

void
eppic_freenode(node_t *n)
{
    n->free(n->data);
    eppic_free(n);
}

int lineno=1, lastline=1;
int col=1;
static char *filename=0;
static char *lastfile=0;

void
eppic_setlastfile(char *fname, int line)
{
    if(!fname) return;
    if(lastfile) { lastfile=0; eppic_free(lastfile); }
    lastfile=eppic_strdup(fname);
    lastline=line;
}

void
eppic_rstpos(void)
{
    lineno=1;
    col=1;
    /* do not free filename */
    filename=0;
}

void
eppic_setpos(srcpos_t *p)
{
    p->line=lineno;
    p->col=col;
    p->file=filename;
}

/* set the current position */
void
eppic_curpos(srcpos_t *p, srcpos_t *s)
{
    if(s) {
        s->line=lineno;
        s->col=col;
        s->file=filename;
    }
    lineno=p->line;
    col=p->col;
    filename=p->file;
}

int
eppic_line(int inc){ return lineno+=inc; }

int
eppic_col(int inc) { return col+=inc; }

char *
eppic_filename(void) { return filename; }

/*
    This function scans a printf() fmt string and transaletes the %p
    to %08x or %016x depending on the pointer size of the object image.
    We also substiture %> for 8 spaces if the pointer size is 4 bytes, this
    permits easy allignment of output on either 32 or 64 bit images.

    ex:

    Proc    %> pid ppid
    %p    %3d %3d

    In this case the %> alligns the pid with it's corresponding value_t
    in the next line of output.

    We also process the '?' format which will be set to match the 
    corresponding value_t type.

    Also, format versus argument type validation is performed.

*/

/*
    Printf formats have the form : 
    %3$-*3$.*4$lld
    %20x
    %08x
    %-08.8f
*/
/* these are the buildin blocks for a regex matching formats */
#define F_POSP  "([0-9]+\\$)*"
#define F_FLGS  "([-'+ #0]*)"
#define F_WARG  "(\\*([0-9]+\\$)*){0,1}"
#define F_WIDTH "([0-9]*)"
#define F_PREC  "((\\.(\\*([0-9]+\\$)*)*([0-9]*))*)"
#define F_SIZE  "([hlL]*)"
#define F_FMT   "([diouxXfeEgGcCsSpn?>]{1})"
#define FMTREG F_POSP""F_FLGS""F_WARG""F_WIDTH""F_PREC""F_SIZE""F_FMT
#define M_POSP          1
#define M_FLAGS         2
#define M_WIDTHARG      3
#define M_WIDTDIGITS    4
#define M_WIDTH         5
#define M_PRECARG       8
#define M_PRECDIGITS    9
#define M_PREC          10
#define M_SIZE          11
#define M_FMT           12
#define NMATCH          16
static int addit[]={M_FLAGS,M_WIDTHARG,M_WIDTH,M_PRECARG,M_PREC,M_SIZE};

#define ptrto(idx) (matches[idx].rm_so==matches[idx].rm_eo?0:(pi+matches[idx].rm_so))
#define matchlen(idx) (matches[(idx)].rm_eo-matches[(idx)].rm_so)

void eppic_error(char *fmt, ...);

static int
chkforint(char *p, value_t **vals, int *curarg)
{
int pos=-1;

    if(!p) return -1;

    /* a single star ? */
    if(isdigit(p[1])) {

        if(sscanf(p+1, "%d", &pos)!=1) {

            return pos;
        }
        pos--;

    } else {

        pos=*curarg;
        *curarg=(*curarg)+1;

    }

    if(pos < BT_MAXARGS && vals[pos] && vals[pos]->type.type == V_BASE) return pos;
    eppic_error("Expected 'integer' type for arg%d", pos+1);
    return -1;
}

#define pushval(val, s, sig)    (                                       \
                    sig ?                                   \
                    (                                   \
                        (s==8) ?                            \
                            (val)->v.sll                        \
                        : (                             \
                            (s==4)  ?                       \
                                (val)->v.sl                 \
                            : (                         \
                                (s==2) ?                    \
                                    (val)->v.ss             \
                                :(                      \
                                    (s==1) ?                \
                                        (val)->v.sc         \
                                    :(                  \
                                        eppic_error("Oops pushval"),1   \
                                    )                   \
                                )                       \
                            )                           \
                        )                               \
                    ) : (                                   \
                        (s==8) ?                            \
                            (val)->v.ull                        \
                        : (                             \
                            (s==4)  ?                       \
                                (val)->v.ul                 \
                            : (                         \
                                (s==2) ?                    \
                                    (val)->v.us             \
                                :(                      \
                                    (s==1) ?                \
                                        (val)->v.uc         \
                                    :(                  \
                                        eppic_error("Oops pushval"),1   \
                                    )                   \
                                )                       \
                            )                           \
                        )                               \
                    )                                   \
            )
                        

static char *
add_fmt(int len, char *s, char *onefmt, int ppos, int wpos, int posarg, value_t **vals)
{
int size=(vals[posarg]->type.type == V_REF ? eppic_defbsize(): vals[posarg]->type.size);
int sign=(vals[posarg]->type.type == V_REF ? 0 : eppic_issigned(vals[posarg]->type.typattr));

    if(vals[posarg]->type.type == V_STRING) {

        if(wpos>=0 && ppos<0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[wpos])
                , vals[posarg]->v.data);
        else if(wpos<0 && ppos>=0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[ppos])
                , vals[posarg]->v.data);
        else if(wpos>=0 && ppos>=0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[wpos])
                , (int)eppic_getval(vals[ppos])
                , vals[posarg]->v.data);
        else s+=snprintf(s, len, onefmt
                , vals[posarg]->v.data);

    } else {
#if defined(__s390x__) || defined(__s390__)
        if(wpos>=0 && ppos<0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[wpos])
                , (unsigned long)pushval(vals[posarg], size, sign));
        else if(wpos<0 && ppos>=0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[ppos])
                , (unsigned long)pushval(vals[posarg], size, sign));
        else if(wpos>=0 && ppos>=0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[wpos])
                , (int)eppic_getval(vals[ppos])
                , (unsigned long) pushval(vals[posarg], size, sign));
        else s+=snprintf(s, len, onefmt
                , (unsigned long) pushval(vals[posarg], size, sign));
#else
        if(wpos>=0 && ppos<0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[wpos])
                , pushval(vals[posarg], size, sign));
        else if(wpos<0 && ppos>=0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[ppos])
                , pushval(vals[posarg], size, sign));
        else if(wpos>=0 && ppos>=0) 
            s+=snprintf(s, len, onefmt
                , (int)eppic_getval(vals[wpos])
                , (int)eppic_getval(vals[ppos])
                , pushval(vals[posarg], size, sign));
        else s+=snprintf(s, len, onefmt
                , pushval(vals[posarg], size, sign));
#endif
    }
    return s;
}

static char *
eppic_ptr(char *fmt, value_t **vals)
{
    /* We need to ensure that we dont overflow our string buffer. Although its unlikely we will overflow it with
       just numbers, strings will easliy overflow. So, lets check for strings and see how long they are.
     */  
int len=0;
char *nfmt=NULL,*ni=NULL;
char *onefmt=NULL, *onei=NULL;
char *p=fmt;
char last=' ';
int curarg=0;
#define NBYTES (len-(nfmt-ni))

int i = 0;

    while(vals[i] != NULL) {
        if(vals[i]->type.type == V_STRING)
            len+=vals[i]->type.size;
        else if(eppic_type_isinvmcore(&vals[i]->type)) {
            /* for printing we allow pointers to vmcore objects */
            eppic_pushref(&vals[i]->type, 1);
            vals[i]->v.ull=vals[i]->mem;
        }
        i++;
    }

    /* We add a fudge factor of 100, which should cover all the number arguments */
    len+=strlen(fmt) + 100;
    nfmt=eppic_alloc(len);
    ni=nfmt;
    onefmt=eppic_alloc(len);
    onei=onefmt;

    while(*p) {

        if(*p=='%') {

            static regex_t preg;
            static int done=0;
            regmatch_t matches[NMATCH];

            if(!done) {

                regcomp(&preg, FMTREG, REG_EXTENDED);
                done=1;
            }

            /* build a new format translation */
            onefmt=onei;
            *onefmt++=*p++;

            /* if the returned pointer is (char*)-1 or NULL then something is wrong */
            if(!regexec(&preg, p, NMATCH, matches, 0)) {

                int i, n=matches[0].rm_eo-1;
                int posarg, wpos, ppos;
                char *pi=p; /* save p for ptrto() macro */

                /* check that the width and precision field args point
                   to a int value_t. If they were used */
                wpos=chkforint(ptrto(M_WIDTHARG), vals, &curarg);
                ppos=chkforint(ptrto(M_PRECARG), vals, &curarg);

                /* argument position was specfified ? */
                if(ptrto(M_POSP)) {

                    /* we work from 0-n, printf works from 1-n */
                    if(sscanf(ptrto(M_POSP), "%d", &posarg)==1) posarg--;

                    if(posarg >= BT_MAXARGS || !vals[posarg]) {
                        eppic_error("Invalid arg position specified [%d]", posarg+1);
                    }

                } else posarg=curarg++;

                /* jump over the format spec in the original */
                p+=n;
#if 0
for(i=0;i<NMATCH;i++) {
    char buf[40];

    if(ptrto(i)) {
        int n=matchlen(i);
        strncpy(buf, pi+matches[i].rm_so, n);
        buf[n]='\0';
        printf("match[%d]=[%s]\n", i, buf);
    }
}
#endif

                /* copy all format specs to destination except fmt */
                for(i=0;i<sizeof(addit)/sizeof(addit[0]);i++) {

                    switch(addit[i]) {

                        case M_WIDTHARG:

                            if(wpos >=0 ){

                                *onefmt++='*';

                            } else goto def;

                        break;
                        case M_PRECARG:

                            if(ppos >=0 ){

                                *onefmt++='.';
                                *onefmt++='*';

                            } else goto def;

                        break;
                        case M_PREC:
                            if(ptrto(addit[i])) *onefmt++='.';
                            goto def;
                        default:
def:
                        {
                            char *p;
                            if((p=ptrto(addit[i]))) {
                                    strcpy(onefmt, p);
                                    onefmt+=matchlen(addit[i]);
                            }
                        }
                    }
                }

                if(*p=='p') {

ref:
                    /* if user overrides anything don't do nothing */
                    if(ptrto(M_FLAGS)||ptrto(M_WIDTH)||ptrto(M_WIDTHARG)||ptrto(M_PREC)||ptrto(M_PRECARG)||ptrto(M_SIZE)) {
                        *onefmt++='p';

                    } else {
                        if(eppic_defbsize()==8) {

                            strcpy(onefmt, "016llx");
                            onefmt+=6;

                        } else {

                            strcpy(onefmt, "08x");
                            onefmt+=3;
                        }
                    }
                    *onefmt='\0';
                    p++;
                    nfmt=add_fmt(NBYTES, nfmt, onei, ppos, wpos, posarg, vals);

                } else if(*p=='>') { 

                    nfmt--;
                    if(sizeof(void*)==8) {

                        int i;

                        for(i=0;i<8;i++) *nfmt++=last;
                    }
                    p++;
                    curarg--;

                } else if(*p=='?') {

                    /* put the proper format for the user */
                    if(!vals[posarg]) {

                        eppic_error("Expected additional argument %d\n", posarg+1);

                    } else switch(vals[posarg]->type.type) {

                        case V_BASE: case V_ENUM:
                        {
                            if(!ptrto(M_SIZE)) {

                                if(vals[posarg]->type.size==8) {

                                    *onefmt++='l';
                                    *onefmt++='l';
                                }
                            }
                            if(eppic_issigned(vals[posarg]->type.typattr)) {

                                *onefmt++='d';

                            }else{

                                *onefmt++='u';
                            }
                        }
                        break;
                        case V_REF:
                        {
                            *p='p';
                            goto ref;
                        }
                        case V_STRING:
                        {
                            *onefmt++='s';
                        }
                        break;
                    }
                    p++;
                    *onefmt='\0';
                    nfmt=add_fmt(NBYTES, nfmt, onei, ppos, wpos, posarg, vals);

                } else {

                    /* check that format and value_t agree */
                    /* can't do a lot more then check for strings vs anything_else */

                    if(!vals[posarg]) {

                        eppic_error("Expected additional argument %d\n", posarg+1);


                    } else if(*p=='s') {

                        if(vals[posarg]->type.type != V_STRING) {

                            eppic_error("Expected type 'string' as arg%d", posarg+1);
                        }

                    } else if(vals[posarg]->type.type == V_STRING) {

                        eppic_error("Incompatible type 'string' in arg%d", posarg+1);

                    }
                    *onefmt++=*p++;
                    *onefmt='\0';
                    nfmt=add_fmt(NBYTES, nfmt, onei, ppos, wpos, posarg, vals);
                }

            } else {

                eppic_warning("Malformed format specifier!");

            }

        } else {
    
            last=*p;
            if(nfmt-ni > len) eppic_error("format tranlation overflow!");
            *nfmt++=*p++;

        }
    }
    eppic_free(onei);
    *nfmt='\0';
    return ni;
}

value_t* eppic_printf(value_t *vfmt, ...)
{
char *fmt = eppic_getptr(vfmt, char);
va_list ap;
value_t *vals[BT_MAXARGS];
int i;
    
    va_start(ap, vfmt);
    for(i=0;i<BT_MAXARGS-2;i++){    
        vals[i]=va_arg(ap,value_t*);
    }                               
    va_end(ap);
    fmt=eppic_ptr(fmt, vals);
    fprintf(ofile, "%s", fmt);
    eppic_free(fmt);
    return eppic_makebtype(1);
}

#define MAX_SPRINTF 2000
value_t* eppic_sprintf(value_t *vfmt, ...)
{
char *fmt=eppic_getptr(vfmt, char);
int i;
va_list ap;
value_t *vals[BT_MAXARGS];
value_t *v;

    va_start(ap, vfmt);
    for(i=0;i<BT_MAXARGS-1;i++){
        vals[i]=va_arg(ap,value_t*);
    }
    va_end(ap);
    fmt=eppic_ptr(fmt, vals);
    v=eppic_setstrval(eppic_newval(), fmt);
    eppic_free(fmt);
    return v;
}


/*
    When there is a parse error in a file.
*/
void
eppic_error(char *fmt, ...)
{
va_list ap;

    eppic_setlastfile(filename, eppic_line(0));
    va_start(ap, fmt);
    fprintf(ofile, "File %s, line %d, Error: ", filename, eppic_line(0));
    vfprintf(ofile, fmt, ap);
    fprintf(ofile, "\n");
    va_end(ap);
    eppic_setsvlev(0);
    eppic_setvlev(0);
    eppic_exit(1);
}

/******************************************************************
   Debug messaging support.
******************************************************************/
static unsigned int dbglvl=0, clist=DBG_ALL;
static char *dbg_name=0;
unsigned int eppic_getdbg(void)
{
    return(dbglvl);
}

void eppic_setdbg(unsigned int lvl)
{
    if(lvl > 9)
        eppic_msg("Invalid debug level value.\n");
    else
        dbglvl=lvl;
}
char *eppic_getname(void)
{
    return dbg_name;
}

void eppic_setname(char *name)
{
    if(dbg_name) eppic_free(dbg_name);
    dbg_name=eppic_strdup(name);
}

#define MAXCLASSES 10
static struct {
    char *name;
    int class;
} classes [MAXCLASSES] = {
    { "type", DBG_TYPE },
    { "struct", DBG_STRUCT },
    { "mac", DBG_MAC },
    { 0 },
};

char **eppic_getclass(void)
{
int i,j;
static char *ptrs[MAXCLASSES+1];

    for(i=j=0;classes[i].name;i++) {
        if(clist&classes[i].class) ptrs[j++]=classes[i].name;
    }
    ptrs[i]=0;
    return ptrs;
}

void eppic_setclass(char *cl)
{
int i;
    
    clist=0;
    for(i=0;classes[i].name;i++) {
        if(!strcmp(classes[i].name,cl)) {
            clist |= classes[i].class;
            return;
        }
    }
    eppic_msg("Invalid class '%s' specified.\n", cl);
}

static void
eppic_dbg_all(int class, char *name, int lvl, char *fmt, va_list ap)
{
    if(lvl<=dbglvl && (clist & class) && (!dbg_name || (name && !strcmp(name, dbg_name)))) {
        eppic_setlastfile(filename, eppic_line(0));
        printf("%s : line %d : Debug:", filename, lineno);
        vprintf(fmt, ap);
        if(fmt[0] && fmt[strlen(fmt)-1] != '\n') printf("\n");
    }
}

void
eppic_dbg(int class, int lvl, char *fmt, ...)
{
va_list ap;
    va_start(ap, fmt);
    eppic_dbg_all(class, 0, lvl, fmt, ap);
    va_end(ap);
}

void
eppic_dbg_named(int class, char *name, int lvl, char *fmt, ...)
{
va_list ap;
    va_start(ap, fmt);
    eppic_dbg_all(class, name, lvl, fmt, ap);
    va_end(ap);
}
/******************************************************************/

void
eppic_rerror(srcpos_t *p, char *fmt, ...)
{
va_list ap;

    eppic_setlastfile(p->file, p->line);
    va_start(ap, fmt);
    fprintf(ofile, "%s : line %d : Error: ", p->file, p->line);
    vfprintf(ofile, fmt, ap);
    fprintf(ofile, "\n");
    va_end(ap);
    eppic_exit(1);
}

void
eppic_warning(char *fmt, ...)
{
va_list ap;

    eppic_setlastfile(filename, eppic_line(0));
    va_start(ap, fmt);
    fprintf(ofile, "%s : line %d : Warning: ", filename, lineno);
    vfprintf(ofile, fmt, ap);
    fprintf(ofile, "\n");
    va_end(ap);
}

void
eppic_rwarning(srcpos_t *p, char *fmt, ...)
{
va_list ap;

    eppic_setlastfile(p->file, p->line);
    va_start(ap, fmt);
    fprintf(ofile, "%s : line %d : Warning: ", p->file, p->line);
    vfprintf(ofile, fmt, ap);
    fprintf(ofile, "\n");
    va_end(ap);
}

void
eppic_vilast()
{
    if(lastfile) {

        eppic_exevi(lastfile, lastline);

    } else {

        eppic_msg("No last error record available");
    }
}

void
eppic_getcomment(void)
{
    while(1) {
    
    unsigned char c;

        while((c=eppic_input())!='*' && c!=255) 

        if(c==255) goto bad;

        if((c=eppic_input())=='/') return;
        else if(c==255) {
bad:
            eppic_error("Unterminated comment!");
        }
    }
}

/* on assignment this function is called to set the new value */
void
eppic_setfct(value_t *v1, value_t *v2)
{
    /* duplicate type and data, safeguarding array info */
    eppic_dupval(v1, v2);

    /* value_t v1 is still setable */
    v1->set=1;
    v1->setval=v1;
}

node_t *
eppic_sibling(node_t *n, node_t *m)
{
node_t *p;

    if(m) {

        for(p=n;p->next;p=p->next);
        p->next=m;
        m->next=0;
    }
    return n;
}

