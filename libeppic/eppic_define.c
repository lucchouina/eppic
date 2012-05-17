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
#include "eppic.h"
/*
    This set of functions handle #define for simple constant or macros.
    We read from the current parser input strem untill end of line.

    The big thing is that we need to do some parsing to get the deinf names
    and parameters. Also at the time of the macro instanciation, we need to parse
    the parameters again. That leads to a more complex package...
*/

#define MAXP 20
typedef struct mac_s {

    char *name;     /* this macro name */
    int np;         /* number of parameters */
    struct mac_s *m;/* parent mac for subs */
    int supressed;  /* used for disabling stacked sub after sub's expension into parse stream */
    int issub;      /* subs have to be threated differently */
    char **p;       /* parameters names */
    char *buf;      /* text for the macro itself */
    struct mac_s *next; /* next on the list */
    srcpos_t pos;

} mac_t;

typedef struct {
    mac_t *m;
    char **subs;
} smac_t;

static mac_t* macs=0;

/* we have to add a space at the end of the value
   Again, this is to prevent a EOF on the parsing stream */
def_t*
eppic_add_def(def_t*d, char*name, char*val)
{
def_t*nd=eppic_alloc(sizeof(def_t));
char *buf=eppic_alloc(strlen(val)+2);

    strcpy(buf, val);
    strcat(buf, " ");
    eppic_free(val);
    nd->name=name;
    nd->val=buf;
    nd->next=d;
    return nd;
}

static void
pmacs()
{
mac_t *eppic_getcurmac(void);
    int i=10;
    mac_t *m=eppic_getcurmac();
    eppic_dbg(DBG_MAC, 2, "=========================================\n");
    eppic_dbg(DBG_MAC, 2, "curmac=0x%p macs=0x%p\n", eppic_getcurmac(), macs);
    if(!m) m=macs;
    while(i-- && m) {
        eppic_dbg(DBG_MAC, 2, "   [%d] %s - %s\n", m->issub, m->name, m->buf);
        m=m->next;
    }
    eppic_dbg(DBG_MAC, 2, "=========================================\n");
}

/* search for a macro is the current list */
mac_t * 
eppic_getmac(char *name, int takeof)
{
mac_t *m, *mm=0;
mac_t *prev=0;
mac_t *eppic_getcurmac(void);
int nosubs=0;

    eppic_dbg_named(DBG_MAC, name, 2, "Looking for macro %s\n", name);
    
    for(m=macs; m; m=m->next) {

        eppic_dbg_named(DBG_MAC, m->name, 2, "     issub %d, m=%p, supressed %d, %s [%s]\n", m->issub, m->m, m->m->supressed, m->name, m->buf);

        if(m->issub && m->m->supressed) continue;

        if(!strcmp(m->name, name) ) {

            eppic_dbg_named(DBG_MAC, m->name, 2, "     Found it !!!!!!!!!!!!!!!!\n");
            if(takeof) {

                if(!prev) macs=m->next;
                else prev->next=m->next;

            }
            return m;
        }
        prev=m;
    }
    return 0;
}

node_t*
eppic_macexists(node_t*var)
{
char *name=NODE_NAME(var);
int val;

    if(eppic_getmac(name, 0)) val=1;
    else val=0;
    return eppic_makenum(B_UL, val);
}
static void
eppic_freemac(mac_t*m)
{
int i;

    for(i=0;i<m->np;i++) eppic_free(m->p[i]);
    if(m->np) eppic_free(m->p);
    eppic_free(m);
}

/*
    These are called at 2 different points.
    One call at the very begining. One call for each file.
*/
void* eppic_curmac(void) { return macs; }

void
eppic_flushmacs(void *vtag)
{
mac_t *m, *next;
mac_t *tag=(mac_t *)vtag;

    for(m=macs; m!=tag; m=next) {

        next=m->next;
        eppic_freemac(m);
    }
    macs=m;
}

/* this function is called to register a new macro.
   The text associated w/ the macro is still on the parser stream.
   Untill eol.
*/
void
eppic_newmac(char *mname, char *buf, int np, char **p, int silent)
{
char *p2;
mac_t *m;

    {
        char *p=buf+strlen(buf)-1;

        /* eliminate trailing blanks */
        while(*p && (*p==' ' || *p=='\t')) p--;
        *(p+1)='\0';

        /* eliminate leading blanks */
        p=buf;
        while(*p && (*p==' ' || *p=='\t')) p++;

        /* copy and append a space. This is to prevent unloading of the
        macro before the eppic_chkvarmac() call as been performed */
        p2=eppic_alloc(strlen(p)+2);
        strcpy(p2, p);
        eppic_free(buf);
        p2[strlen(p2)+1]='\0';
        p2[strlen(p2)]=' ';
        buf=p2;
    }

    if((m=eppic_getmac(mname, 1)) && strcmp(m->buf, buf)) {

        /* when processing the compile options, be silent. */
        if(!silent) {

            eppic_warning("Macro redefinition '%s' with different value_t\n"
                "value_t=[%s]\n"
                "Previous value_t at %s:%d=[%s]\n"
                , mname, buf, m->pos.file, m->pos.line, m->buf);
        }

    }
    m=(mac_t*)eppic_alloc(sizeof(mac_t));
    m->name=eppic_strdup(mname);
    m->np=np;
    m->p=p;
    m->m=m;
    m->supressed=0;
    m->buf=buf;
    m->next=macs;
    m->issub=0;
    eppic_setpos(&m->pos);
    macs=m;
}

/* this function is called by the enum declaration function and
   when a enum type is extracted from the image to push a set
   of define's onto the stack, that correspond to each identifier 
   in the enum.
*/
void
eppic_pushenums(enum_t *et)
{
    while(et) {

        char *buf=eppic_alloc(40);

        sprintf(buf, "%d", et->value);
        eppic_newmac(et->name, buf, 0, 0, 0);
        et=et->next;
    }
}

static void
eppic_skipcomment(void)
{
int c;

    while((c=eppic_input())) {

        if(c=='*') {

            int c2;

            if((c2=eppic_input())=='/') return;
            eppic_unput(c2);
        }
    }
}

static void
eppic_skipstr(void)
{
int c;

    while((c=eppic_input())) {

        if(c=='\\') eppic_input();
        else if(c=='"') return;
    }
}


/* skip over strings and comment to a specific chracter */
static void
eppic_skipto(int x)
{
int c;

    while((c=eppic_input())) {

        if(c==x) return;

        switch(c) {

            case '\\':
            eppic_input();
            break;

            case '"':
            eppic_skipstr();
            break;

            case '/': {

                int c2;

                if((c2=eppic_input())=='*') {

                    eppic_skipcomment();

                } else eppic_unput(c2);
            }
            break;

            case '(':

                eppic_skipto(')');
            break;

            case ')':
            eppic_error("Missing parameters to macro");
            break;
        }

    }

    eppic_error("Expected '%c'", x);
}

static void
eppic_popsub(void *vm)
{
    ((mac_t*)vm)->m->supressed=0;
}


/*
   This function gets called when the buffer for a macro as been fully
   parsed. We need to take the associated parameter substitution macros
   of of the stack and deallocate associated data.
*/
static void
eppic_popmac(void *vsm)
{
smac_t *sm=(smac_t *)vsm;
int i;

    
    eppic_dbg_named(DBG_MAC, sm->m->name, 2, "Poping mac %s\n", sm->m->name);
    for(i=0;i<sm->m->np;i++) {

        mac_t *m=eppic_getmac(sm->m->p[i], 1);

        if(!m) eppic_error("Oops macro pop!");
        eppic_free(m->buf);
        eppic_free(m->name);
        eppic_free(m);
    }
    eppic_free(sm->subs);
    eppic_free(sm);
}

/* 

  need to get the actual parameters from the parser stream.
  This can be simple variable or complex multiple line expressions
  with strings and commants imbedded in them... 

*/
static int 
eppic_pushmac(mac_t *m)
{
int i;
char **subs=eppic_alloc(sizeof(char*)*m->np);
smac_t *sm;
int eppiclex(void);

    /* the next token should be a '(' */
    if(eppiclex() != '(') {

        eppic_error("Expected '(' after '%s'", m->name);
        
    }
    
    eppic_dbg_named(DBG_MAC, m->name, 2, "Pushing macro : %s\n", m->name);

    /* get the parameters */
    for(i=0;i<m->np;i++) {

        char *p=eppic_cursorp();
        int nc;

        if(i<m->np-1) eppic_skipto(',');
        else eppic_skipto(')');

        nc=eppic_cursorp()-p-1;
        subs[i]=eppic_alloc(nc+2);
        strncpy(subs[i], p, nc);
        subs[i][nc]=' ';
        subs[i][nc+1]='\0';
    }

    /* take care of the macro() case. ex: IS_R10000()i.e. no parms */
    if(!m->np) 
        eppic_skipto(')');

    sm=eppic_alloc(sizeof(smac_t));

    sm->m=m;
    sm->subs=subs;

    /* we push the associated buffer on the stream */
    eppic_pushbuf(m->buf, 0, eppic_popmac, sm, m);

    /* we push the subs onto the macro stack */
    for(i=0;i<m->np;i++) {

        mac_t *pm=eppic_alloc(sizeof(mac_t));

        pm->name=eppic_alloc(strlen(m->p[i])+1);
        strcpy(pm->name, m->p[i]);
        pm->np=0;
        pm->p=0;
        eppic_dbg_named(DBG_MAC, m->name, 2, "    P map : %s ==> %s\n", m->p[i], subs[i]);
        pm->buf=subs[i];
        pm->next=macs;
        pm->issub=1;
        pm->supressed=0;
        pm->m=m;
        macs=pm;
    }
    return 1;
    
}


/*
    This one is called from the lexer to check if a 'var' is to be substituted for
    a macro
*/
int
eppic_chkmacvar(char *mname)
{
mac_t *m;

    if((m=eppic_getmac(mname, 0))) {

        eppic_dbg_named(DBG_MAC, m->name, 2, "    var '%s' is mac [issub %d] ==> [%s]\n", m->name, m->issub, m->buf);
        
        /* simple constant ? */
        if(!m->p) {

            m->m->supressed=1;
            eppic_pushbuf(m->buf, 0, eppic_popsub, m, m->issub ? m->m : m);

        } else {
            return eppic_pushmac(m);
        }
        return 1;

    }
    return 0;

}

/*
    Skip an unsupported preprocessor directive.
*/
void
eppic_skip_directive(void)
{
    eppic_free(eppic_getline());
}

void
eppic_undefine(void)
{
int c;
int i=0;
char mname[MAX_SYMNAMELEN+1];
mac_t *m;

    /* skip all white spaces */
    while((c=eppic_input()) == ' ' || c == '\t') if(c=='\n' || !c) {

        eppic_error("Macro name expected");
    }

    mname[i++]=c;

    /* get the constant or macro name */
    while((c=eppic_input()) != ' ' && c != '\t') {

        if(c=='\n' || !c) break;
        if(i==MAX_SYMNAMELEN) break;
        mname[i++]=c;
    }
    mname[i]='\0';
    if((m=eppic_getmac(mname, 1))) eppic_freemac(m);
        else eppic_addneg(mname);
}

/*
    This one is called from the lexer after #define as been detected 
*/
void
eppic_define(void)
{
int c;
int i=0;
char mname[MAX_SYMNAMELEN+1];

    /* skip all white spaces */
    while((c=eppic_input()) == ' ' || c == '\t') if(c=='\n' || !c) goto serror;

    mname[i++]=c;

    /* get the constant or macro name */
    while((c=eppic_input()) != ' ' && c != '\t' && c != '(') {

        if(c=='\n' || !c) break;

        if(i==MAX_SYMNAMELEN) break;

        mname[i++]=c;
    }
    mname[i]='\0';

    /* does this macro have paraneters */
    /* If so, '(' will be right after name of macro. No spaces. */
    if(c=='(') {

        int np, nc, done;
        char **pnames;
        char curname[MAX_SYMNAMELEN+1];

        np=nc=done=0;
        pnames=(char **)eppic_alloc(sizeof(char*)*MAXP);
        
        while(!done) {

            c=eppic_input();

            switch(c) {
                case '\n': case 0:
                goto serror;

                /* continuation */
                case '\\':
                if(eppic_input()!='\n') goto serror;
                break;

                case ',':
                if(!nc) goto serror;
last:
                curname[nc]='\0';
                pnames[np]=eppic_alloc(strlen(curname)+1);
                strcpy(pnames[np], curname);
                nc=0;
                np++;
                break;

                case ')':
                done=1;
                if(nc) goto last;
                break;

                case ' ':
                case '\t':
                break;

                default:
                curname[nc++]=c;
                break;
            }
        }
        eppic_newmac(mname, eppic_getline(), np, pnames, 0);
        return;

    } else if(c == '\n') {

        /* if nothing speciied then set to "1" */
        eppic_newmac(mname, eppic_strdup("1"), 0, 0, 0);

    } else {

        eppic_newmac(mname, eppic_getline(), 0, 0, 0);
    }
        
    return;

serror:

    eppic_error("Syntax error on macro definition");
}
