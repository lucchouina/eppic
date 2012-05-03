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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "eppic.h"

char *eppicpp_create_buffer(void *, int);
void eppicpp_switch_to_buffer(void *);
void eppicpp_delete_buffer(void *);

typedef void fdone(void *data);
extern void* eppic_create_buffer(FILE *, int);
typedef struct inbuf_s {
	srcpos_t pos;		/* current filename,line,col */
	int cursor;		/* position of next input() character */
	int len;		/* length of the buffer */
	char *buf;		/* buffer */
	void *data;		/* opaque data for callback */
	void *mac;		/* for nested defines substitutions */
	fdone *donefunc;	/* function to call when done with buffer */
	int space;
	int eofonpop;		/* terminate parsing at end of buffer ? */
#if linux
	void* yybuf;
#endif

} inbuf_t;

void eppic_switch_to_buffer(void *);
void eppic_delete_buffer(void *);
#define MAXIN 20
static inbuf_t inlist[MAXIN];
static inbuf_t *in=0;
static int nin=0;
static int eol=0, virgin=1;
#if linux
static int inpp=0;
#endif

extern void *eppic_getmac(char *, int);

/* this function is called by the macro macro functions to set
   and test the current buffer level in order to take care of 
   imbedded macros w/ the same parameter names.
   see eppic_getmac().
*/ 
void *eppic_getcurmac(void) 
{ 
	return in ? in->mac : 0 ;
}

static void
eppic_pusherror(void)
{
	eppic_error("Too many level of input stream");
}

/*
	Push a buffer onto the parser input stream.
*/
void
eppic_pushbuf(char *buf, char *fname, void (*vf)(void *), void *d, void *m)
{
fdone *f=(fdone*)vf;

	if(nin==MAXIN) eppic_pusherror();

	/* if we are pushing a macro then use upper level coordinates */
	if(fname) {

		inlist[nin].pos.line=1;
		inlist[nin].pos.col=1;
		inlist[nin].pos.file=fname;

	} else eppic_setpos(&inlist[nin].pos);

	/* set it */
	if(nin) {

		eppic_curpos(&inlist[nin].pos, &inlist[nin-1].pos);

	} else {

		eppic_curpos(&inlist[nin].pos, 0);

	}

	inlist[nin].buf=buf;
	inlist[nin].donefunc=f;
	inlist[nin].space=0;
	inlist[nin].data=d;
	inlist[nin].mac=m;
	inlist[nin].cursor=0;
	inlist[nin].len=strlen(buf);
	inlist[nin].eofonpop=0;
#if linux
	if(inpp) {
		inlist[nin].yybuf=eppicpp_create_buffer(0, inlist[nin].len);
		eppicpp_switch_to_buffer(inlist[nin].yybuf);
	}else{
		inlist[nin].yybuf=eppic_create_buffer(0, inlist[nin].len);
		eppic_switch_to_buffer(inlist[nin].yybuf);
	}
#endif
	in=&inlist[nin];
	nin++;
}

/* read the rest of the "#include" line from the parser input stream
   open the corresponding file, push it's contain on the parser input
   stream.
*/
int
eppic_pushfile(char *name)
{
struct stat s;
char *fname;

	if(nin==MAXIN) eppic_pusherror();
	
	fname=eppic_fileipath(name);

	if(fname) {

		if(!stat(fname, &s)) {
	
			char *buf=eppic_alloc(s.st_size+1);
			int fd;

			if((fd=open(fname, O_RDONLY))==-1) {

					eppic_msg("%s: %s", fname, strerror(errno));

			}
			else {

				if(read(fd, buf, s.st_size) != s.st_size) {

					if(errno != EISDIR) 
						eppic_msg("%s: read error : %s", fname, strerror(errno));

				}
				else {

					
					buf[s.st_size]='\0';
					eppic_pushbuf(buf, fname, eppic_free, buf, 0);
					close(fd);
					return 1;

				}
				close(fd);
			}

			eppic_free(buf);

		}
		eppic_free(fname);
	}
	return 0;

}

/*
	Done with the current buffer.
	Go back to previous on the stack.
*/
static int
eppic_popin(void)
{

	if(eol || !nin) {

		if(!nin) in=0;
		return 1;
	
	} else {

		nin--;

		/* call back */
		if(inlist[nin].donefunc) {

			inlist[nin].donefunc(inlist[nin].data);
		}
		if(inlist[nin].eofonpop) {

			eol=1;
#if linux
			inpp=0;
#endif
		}
		if(!nin) in=0;
		else {

			in=&inlist[nin-1];
			if(!eol) {
#if linux
				if(inpp) {
					eppicpp_switch_to_buffer(inlist[nin-1].yybuf);
					eppicpp_delete_buffer(inlist[nin].yybuf);
				} else {
					eppic_switch_to_buffer(inlist[nin-1].yybuf);
					eppic_delete_buffer(inlist[nin].yybuf);
				}
#endif
			}
			eppic_curpos(&in->pos, 0);
		}
		return 0;
	}
}

/*
  With linux we need to use the wrap function
  so that the flex buffer stuff is keaped in the game.
*/
int
eppicwrap(void) 
{ 
	return eppic_popin(); 
}

int
eppicppwrap(void)
{ 
	if(eol) return 1;
	return eppic_popin();
}

void
eppic_popallin(void)
{
	while(nin) {
		eol=0;
		eppic_popin();
	}
}

#define BLK_IFDEF	1
#define BLK_IFNDEF	2
#define BLK_IF		3
#define BLK_ELIF	4
#define BLK_ELSE	5

typedef struct ifblk {
	int type;	/* type of block */
	int exprpos;    /* curpor to start of corresponding expression */
	int bstart;	/* curpor position at start of block */
	int dirlen;	/* length of the directive name */
	int bend;	/* cursor position at end of block */
	struct ifblk *next;
} ifblk_t;

static int
eppic_isif(int pos)
{
	if(
	      (in->len-pos>6 && !strncmp(in->buf+pos, "ifndef", 6))
	   || (in->len-pos>5 && !strncmp(in->buf+pos, "ifdef", 5)) 
	   || (in->len-pos>2 && !strncmp(in->buf+pos, "if", 2)) 

	) return 1;

	return 0;
}

/*
	Get directly to next block, skipping nested blocks.
*/
static int
eppic_nxtblk(int pos, int lev)
{
int virgin=0;

	while(1) {

		if(pos==in->len) {

			eppic_error("Block without endif");
		}

		if(virgin && in->buf[pos]=='#') {

			pos++;

			/* nested if ? */
			if(in->buf[pos]=='i' && eppic_isif(pos)) {

				while(1) {
					pos=eppic_nxtblk(pos, lev+1);
					if(in->len-pos>5 && !strncmp(in->buf+pos, "endif", 5)) break;
				}

			} else if(in->buf[pos]=='e') return pos;

		} else if(in->buf[pos]=='\n') {

			virgin=1;

		} else if(in->buf[pos] != ' ' && in->buf[pos] != '\t') {

			virgin=0;
		}
		pos++;
	}
}

static ifblk_t *
eppic_getblklst(void)
{
ifblk_t *lst, *last;
int doneelse=0, pos;

	lst=eppic_alloc(sizeof(ifblk_t));

	lst->bstart=in->cursor-1;
	if(!strncmp(in->buf+in->cursor, "ifdef", 5)) {

		lst->type=BLK_IFDEF;
		lst->exprpos=lst->bstart+6;
		lst->dirlen=6;

	} else if(!strncmp(in->buf+in->cursor, "ifndef", 6)){

		lst->type=BLK_IFNDEF;
		lst->exprpos=lst->bstart+7;
		lst->dirlen=7;

	} else {

		lst->type=BLK_IF;
		lst->exprpos=lst->bstart+3;
		lst->dirlen=3;
	}

	last=lst;
	pos=in->cursor;

	while(1) {

		ifblk_t *new=eppic_alloc(sizeof(ifblk_t));

		pos=eppic_nxtblk(pos, 0);

		last->bend=pos-2;
		new->bstart=pos-1;
		if(!strncmp(in->buf+pos, "elif", 4)) {

			if(doneelse) {

				eppic_error("Additional block found after #else directive");
			}
			new->type=BLK_ELIF;
			new->exprpos=new->bstart+5;
			new->dirlen=5;

		} else if(!strncmp(in->buf+pos, "else", 4)) {

			if(doneelse) {

				eppic_error("#else already done");
			}
			new->type=BLK_ELSE;
			new->exprpos=new->bstart+5;
			new->dirlen=5;
			doneelse=1;

		} else if(!strncmp(in->buf+pos, "endif", 5)) {

			eppic_free(new);
			last->next=0;
			break;
		}
		last->next=new;
		last=new;
	}
	return lst;
}

/*
	Zap a complete block.
	We put spaces everywhere but over the newline.
	Hey, it works. It's good enough for me.
*/
static void
eppic_zapblk(ifblk_t *blk)
{
int i;

	for(i=blk->bstart;i<blk->bend;i++) {

		if(in->buf[i]!='\n') in->buf[i]=' ';
	}
}

int eppic_eol(char c) { return (!c || c=='\n') ? 1 : 0; }

/*
	This function is called by eppic_input() when a #if[def] is found.
	We gather all blocks of the if/then/else into a list.
	Parsing and execution of the expression is done only when needed.
*/
void eppic_rsteofoneol(void)
{
	eol=0;
	virgin=1;
#if linux
	inpp=0;
#endif
}

void
eppic_zapif(void)
{
ifblk_t *lst=eppic_getblklst();
ifblk_t *last=lst;
int b=0;

	/* we scan the entire list untill a condition is true or we
	   reach #else or we reach the end */
	while(lst) {

		switch(lst->type) {

			case BLK_IFDEF:
			case BLK_IFNDEF:
			{
			char mname[MAX_SYMNAMELEN+1], c;
			int i=0, j=lst->bstart+lst->dirlen;
			int v;

				/* get the macro name and see if it exists */
				/* skip all white spaces */
				while((c=in->buf[j]) == ' ' || c == '\t') if(c=='\n' || !c) {

					eppic_error("Macro name not found!");

				} else j++;

				/* get the constant or macro name */
				while((c=in->buf[j]) != ' ' && c != '\t' && c != '(') {

					if(c=='\n' || !c) break;

					if(i==MAX_SYMNAMELEN) break;

					mname[i++]=c;
					j++;
				}
				mname[i]='\0';
				lst->dirlen += (j-lst->bstart-lst->dirlen);
				if(eppic_getmac(mname,0)) v=1;
				else v=0;
				b=lst->type==BLK_IFDEF?v:!v;

			}
			break;

			case BLK_IF: case BLK_ELIF:
			{
			node_t*n;
			void eppicpprestart(int);
			void eppicppparse(void);
			char *expr=eppic_getline();
			int len=lst->dirlen;

#if linux
				eppicpprestart(0);
				inpp=1;
#endif
				lst->dirlen += (in->cursor-lst->exprpos-1);
				eppic_pushbuf(expr, 0, eppic_free, expr, 0);
				in->eofonpop=1;
				in->cursor += len;
				eppicppparse();

				eppic_rsteofoneol();
				eol=0;

				/* get the resulting node_t*/
				n=eppic_getppnode();

				/* execute it */
				{
					
				int *exval;
				jmp_buf exitjmp;
				void *sa;
				value_t *v;

					sa=eppic_setexcept();

					if(!setjmp(exitjmp)) {

						eppic_pushjmp(J_EXIT, &exitjmp, &exval);
						v=NODE_EXE(n);
						eppic_rmexcept(sa);
						eppic_popjmp(J_EXIT);
						b=eppic_bool(v);
						eppic_freeval(v);

					} else {

						eppic_rmexcept(sa);
						eppic_parseback();
					}
				}
			}
			break;

			case BLK_ELSE:
			{

				b=1;

			}
			break;
		}

		last=lst;
		if(b) break;

		/* count new lines */
		{
			while(in->cursor < lst->bend+1) {

				if(eppic_eol(in->buf[in->cursor]))
					eppic_line(1);
				in->cursor++;
			}
	
		}
		lst=lst->next;
	}

	if(lst) {

		/* remove the # directive itself */
		memset(in->buf+lst->bstart, ' ', lst->dirlen);

		/* zap all remaining blocks */
		while((lst=lst->next)) { eppic_zapblk(lst); last=lst; }
	}

	/* most remove the #endif */
	memset(in->buf+last->bend+1, ' ', 6);
}

static int rawinput=0;
void eppic_rawinput(int on) { rawinput=on; }

/*
	Get the next character from the input stream tack.
*/
int
eppic_input(void) 
{ 
register char c;

redo:

	if(!in || eol) {

		return 0;
	}

	if(in->cursor==in->len) {

#if linux
		return (-1);
#else
		eppic_popin();
		goto redo;
#endif
	}

	c=in->buf[in->cursor++];
	if(!rawinput) {
		if(c=='\\') {

			if(in->cursor==in->len) return c;
			else if(in->buf[in->cursor]=='\n') {

				eppic_line(1);
				in->cursor++;
				goto redo;
			}

		} else if(c=='/') {

			if(in->cursor==in->len) return c;
			else if(in->buf[in->cursor]=='/') {

				/* C++ stype comment. Eat it. */
				in->cursor++;
				while(in->cursor<in->len) {

					c=in->buf[in->cursor++];
					if(c=='\n') { 
						/* leave the newline in there */
						in->cursor--;
						break;
					}
				}
				goto redo;

			}else if(in->buf[in->cursor]=='*') {

				/* C style comment, eat it */
				in->cursor++;
				while(in->cursor<in->len) {

					c=in->buf[in->cursor++];
					if(c=='*' && (in->cursor<in->len)) {

						if(in->buf[in->cursor]=='/') {

							in->cursor++;
							break;

						}

					} else if(c=='/' && (in->cursor<in->len)) {

						if(in->buf[in->cursor]=='*') {

							eppic_warning("Nested comment");

						}

					}
					if(c=='\n') eppic_line(1);
				}
				goto redo;
			}

		}else if(virgin && c=='#') {

			char *p=in->buf+in->cursor;
			char *end=in->buf+in->len;
			int c=0;

			/* skip white spaces '#      define ... ' */
			while(p<(end-4) && (*p==' ' || *p=='\t')) { p++; c++; }

			/* this must be a preprocessor command */
			/* we trigger on the if, ifdef only. #define, #undef, #include are
			   handled by the lexer */

			if(!strncmp(p, "if", 2)) {

				in->cursor += c;
				eppic_zapif();
				/* zapif sets the cursor correctly */
				goto redo;
			}
		}
	}

	if(c=='\n') {

		virgin=1;
		eppic_line(1);

	}else if(c != ' ' && c != '\t') {

		virgin=0;

	} 
	else if(!rawinput){

		register char c2=c;

		/* return one white space for a group of them */
		while((in->cursor < in->len) 
			&& in->buf[in->cursor]==c2) in->cursor++;

	}

	return c;
}

char *
eppic_cursorp()
{
	if(!in) return 0;
	return in->buf+in->cursor;
}

void
eppic_unput(char c)
{

	if(!c) return;
	if(!nin) return;
	if(!in->cursor) {

		eppic_error("Fatal unput error");

	}
	in->buf[--in->cursor]=c;
	if(c=='\n') {

		eppic_line(-1);
	}
}

/*
	Get a single line from the parser stream.
*/
char *
eppic_getline()
{
char *buf2=0;

	/* use the current input stream for that */
	if(in) {

		/* save the cursor */
		int n=0, c;
		char *buf=eppic_alloc(in->len-in->cursor+1);

		while(!eppic_eol(c=eppic_input()))
			buf[n++]=c;
		buf[n]='\0';
		buf2=eppic_alloc(n+2);
		strcpy(buf2,buf);
		buf2[n]=' ';
		buf2[n+1]='\0';
		eppic_free(buf);
		/* leave the newline there */
		eppic_unput(c);
	}
	return buf2;
}


/* read a complete line from the input stream */
void
eppic_include(void)
{
char name[MAX_SYMNAMELEN+1];
int n=0;
int c;
int found=0;

	while((c=eppic_input())) {

		if(c=='"') {

			if(!found) found++;
			else break;
			continue;
		}

		if(c=='<') {

			found++;
			continue;
			
		}
		if(c=='>') break;
		if(eppic_eol(c)) {

			eppic_error("Unexpected EOL on #include");
		}
		if(found) {

			if(n==MAX_SYMNAMELEN) {

				eppic_error("Filename too long");
			}
			name[n++]=c;

		}
	}
	name[n]='\0';

	/* flush the rest of the line */
	while((c=eppic_input())) {

		if(eppic_eol(c)) break;
	}
	eppic_unput(c);
	if(eppic_fileipath(name)) {

		eppic_pushfile(name);

	} else {

		eppic_msg("Include file not found: '%s' [include path is '%s']", name, eppic_getipath());
	}
}
