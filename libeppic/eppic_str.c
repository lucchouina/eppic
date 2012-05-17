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
#include <ctype.h>
#include <string.h>
#include "eppic.h"

/*
        Create a new string node from a string.
*/

value_t *
eppic_setstrval(value_t *val, char *buf)
{
char *newbuf=eppic_strdup(buf);

    val->v.data=(void*)newbuf;
    val->type.type=V_STRING;
    val->type.size=strlen(buf)+1;
    val->set=0;
    return val;
}

value_t *
eppic_makestr(char *s)
{
    return eppic_setstrval(eppic_newval(), s);
}

static value_t*
eppic_exestr(char *buf)
{
value_t *v=eppic_newval();

    eppic_setstrval(v, buf);
    return v;
}

void
eppic_freestrnode(char *buf)
{
    eppic_free(buf);
}

node_t*
eppic_allocstr(char *buf)
{
node_t*n=eppic_newnode();

    n->exe=(xfct_t)eppic_exestr;
    n->free=(ffct_t)eppic_freestrnode;
    n->data=buf;
    eppic_setpos(&n->pos);

    return n;
}

node_t*
eppic_strconcat(node_t*n1, node_t*n2)
{
char *newbuf=eppic_alloc(strlen(n1->data)+strlen(n2->data)+1);

    strcpy(newbuf, n1->data);
    strcat(newbuf, n2->data);
    eppic_free(n1->data);
    n1->data=newbuf;
    eppic_freenode(n2);
    return n1;
}

static int
is_valid(int c, int base)
{
    switch(base)
    {
        case 16: return (c>='0' && c<='9') || (toupper(c) >= 'A' && toupper(c) <= 'F');
        case 10: return (c>='0' && c<='9');
        case 8:  return (c>='0' && c<='7');
    }
    return 0;
}

/* extract a number value_t from the input stream */
static int eppic_getnum(int base)
{
int val=0;
    while(1)
    {
    char c=eppic_input(), C;

        C=toupper(c);
        if(is_valid(C, base)) {

            val=val*base;
            val+=(C>='A')?10+('F'-C):'9'-C;
        }
        else
        {
            eppic_unput(c);
            break;
        }
    }
    return val;
}

int
eppic_getseq(int c)
{
int i;
static struct {
    int code;
    int value;
} seqs[] = {
    { 'n', '\n' },
    { 't', '\t' },
    { 'f', '\f' },
    { 'r', '\r' },
    { 'n', '\n' },
    { 'v', '\v' },
    { '\\', '\007' },
};
    for(i=0;i<sizeof(seqs)/sizeof(seqs[0]);i++) {

        if(seqs[i].code==c) return seqs[i].value;
    }
    return c;
}

node_t*
eppic_newstr()
{
int maxl=S_MAXSTRLEN;
char *buf=eppic_alloc(maxl);
int iline=eppic_line(0);
int i, c;

    /* let the input function knwo we want averyting from the 
       input stream. Comments and all... */
    eppic_rawinput(1);

    for(i=0;i<maxl;i++) {
        c=eppic_input();
        switch(c) {
        case '\\': /* escape sequence */
            switch(c=eppic_input()) {
            case 'x': /* hexa value_t */
                buf[i]=eppic_getnum(16);
            break;
            case '0': /* octal value_t */
                buf[i]=eppic_getnum(8);
            break;
            default : 
                if(isdigit(c))
                {
                    eppic_unput(c);
                    buf[i]=eppic_getnum(10);
                }
                else
                {
                    buf[i]=eppic_getseq(c); 
                }
            break;

            }
        break;
        case '"': /* we're finished */
        {
            buf[i]='\0';
            eppic_rawinput(0);
            return eppic_allocstr(buf);

        }
        case (-1):
            eppic_error("Unterminated string at line %d", iline);
        break;
        default:
            buf[i]=c;
        break;

        }
    }
    eppic_error("String too long at %d", iline);
    return NULLNODE;
}
