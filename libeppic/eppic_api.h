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

/* minor and major version number 
    4.0 switch to new Eppic name and use of fully typed symbols.
*/
#ifndef EPPIC_API_H
#define EPPIC_API_H

#define S_MAJOR 5
#define S_MINOR 0

#define MAX_SYMNAMELEN  100
#define MAXIDX      20

/* abi values */
#define ABI_X86     1
#define ABI_ALPHA   2
#define ABI_PPC     3
#define ABI_IA64    4
#define ABI_S390    5
#define ABI_S390X   6
#define ABI_PPC64   7
#define ABI_X86_64  8
#define ABI_ARM     9
#define ABI_ARM64   10
#define ABI_MIPS    11
#define ABI_SPARC64 12
#define ABI_MIPS64  13
#define ABI_RISCV64 14

/* types of variables */
#define V_BASE          1
#define V_STRING        2
#define V_REF           3
#define V_ENUM          4
#define V_UNION         5
#define V_STRUCT        6
#define V_TYPEDEF       7
#define V_ARRAY         8

#define ENUM_S      struct enum_s
#define DEF_S       struct def_s
#define MEMBER_S    struct member_s
#define TYPE_S      struct typeX_s
#define VALUE_S     struct value_s
#define ARRAY_S     struct array_s
#define NODE_S      struct node_s
#define IDX_S       struct idx_s
#define VAR_S       struct var_s

ENUM_S;
DEF_S;
MEMBER_S;
TYPE_S;
VALUE_S;
ARRAY_S;
NODE_S;
IDX_S;
VAR_S;

#if linux
#include <stdint.h>
typedef uint64_t ull;
typedef uint32_t ul;
#else
typedef unsigned long long ull; 
typedef unsigned long ul;
#endif

/* THe API function calls numbers */
typedef struct {

    int         (*getmem)(ull, void *, int);        /* write to system image */
    int         (*putmem)(ull, void *, int);        /* read from system image */
    int         (*member)(char *, char *, void **); /* Get a members of a struct/union */
    int         (*getctype)(int ctype, char *, TYPE_S*); /* get struct/union type information */
    int         (*getval)(char *, ull *addr, VALUE_S *);/* get the value of a system variable */
    int         (*getenum)(char *name, ENUM_S *);   /* get the list of values for an enum type */
    DEF_S*      (*getdefs)(void);                   /* get the list of compiler pre-defined macros */
    uint8_t     (*get_uint8)(void*);
    uint16_t    (*get_uint16)(void*);
    uint32_t    (*get_uint32)(void*);
    uint64_t    (*get_uint64)(void*);
    char*       (*findsym)(char*);
} apiops; 

/*
    Builtin API defines....
*/
/* call this function to install a new builtin 

   proto is the function prototype ex:
   struct proc* mybuiltin(int flag, char *str);

   "mybuiltin" will be the eppic name for the function.
   "fp" is the pointer to the builtin function code.

*/
typedef VALUE_S* bf_t(VALUE_S*, ...);
typedef struct btspec {
    char *proto;
    bf_t *fp;
} btspec_t;

/* dso entry points */
#define BT_SPEC_TABLE   btspec_t bttlb[]
#define BT_SPEC_SYM    "bttlb"
#define BT_INIDSO_FUNC  int btinit
#define BT_INIDSO_SYM  "btinit"
#define BT_ENDDSO_FUNC  void btend
#define BT_ENDDSO_SYM  "btend"

/* maximum number of parameters that can be passed to a builtin */
#define BT_MAXARGS  20

extern apiops *eppic_ops;
#define API_GETMEM(i, p, n) ((eppic_ops->getmem)((i), (p), (n)))
#define API_PUTMEM(i, p, n) ((eppic_ops->putmem)((i), (p), (n)))
#define API_MEMBER(n, mn, s)((eppic_ops->member)((n), (mn), (s)))
#define API_GETCTYPE(i, n, t)   ((eppic_ops->getctype)((i), (n), (t)))
#define API_ALIGNMENT(i)    ((eppic_ops->alignment)((i)))
#define API_GETVAL(n,a, val)   ((eppic_ops->getval)((n), (a), (val)))
#define API_GETENUM(n, e)      ((eppic_ops->getenum)(n, e))
#define API_GETDEFS()       ((eppic_ops->getdefs)())
#define API_GET_UINT8(ptr)  ((eppic_ops->get_uint8)(ptr))
#define API_GET_UINT16(ptr) ((eppic_ops->get_uint16)(ptr))
#define API_GET_UINT32(ptr) ((eppic_ops->get_uint32)(ptr))
#define API_GET_UINT64(ptr) ((eppic_ops->get_uint64)(ptr))
#define API_FINDSYM(p)      ((eppic_ops->findsym)(p))

#if linux
#   if __LP64__
#       define eppic_getptr(v, t)   ((t*)eppic_getval(v))
#   else
#       define eppic_getptr(v, t)   ((t*)(ul)eppic_getval(v))
#   endif
#else
#   if (_MIPS_SZLONG == 64)
#       define eppic_getptr(v, t)   ((t*)eppic_getval(v))
#   else
#       define eppic_getptr(v, t)   ((t*)(ul)eppic_getval(v))
#   endif
#endif

/* startup function */
int  eppic_open(void);      /* initialize a session with eppic */
void     eppic_apiset(apiops *, int, int, int);/* define the API for a connection */
void     eppic_setofile(void *);        /* eppic should output messages to this file */
void    *eppic_getofile(void);      /* where is eppic currently outputing */
void     eppic_setmpath(char *p);   /* set the search path for eppic scripts */
void     eppic_setipath(char *p);   /* set the search path for eppic include files  */
VAR_S   *eppic_builtin(char *proto, bf_t);/* install a builtin function */
int      eppic_cmd(char *name, char **argv, int argc); /* execute a command w/ args */

/* load/unload of script files and directories */
ull  eppic_load(char *);        /* load/parse a file */
ull  eppic_unload(char *);      /* load/parse a file */
void     eppic_loadall(void);       /* load all files found in set path */

/* variables associated functions */
VAR_S   *eppic_newvar(char *);      /* create a new static/auto variable */
void    *eppic_add_globals(VAR_S*); /* add a set of variable to the globals context */
VAR_S   *eppic_newvlist(void);      /* create a root for a list of variables */

int  eppic_tryexe(char *, char**, int);/* try to execute a function */
int  eppic_parsetype(char*, TYPE_S *, int);/* parse a typedef line */
ull  eppic_exefunc(char *, VALUE_S **);/* to execute a function defined in eppic */

/* help related function */
void     eppic_showallhelp(void);   /* display help info for all commands */
int  eppic_showhelp(char *);        /* display help info for a single command */

/* allocation related function */
void    *eppic_alloc(int);      /* allocate some memory */
void    *eppic_calloc(int);     /* allocate some 0 filed memory */
void     eppic_free(void*);     /* free it */
char    *eppic_strdup(const char*); /* equivalent of strdup() returns eppic_free'able char */
void    *eppic_dupblock(void *p);   /* duplicate the contain of a block of allocated memory */
void    *eppic_realloc(void *p, int size);  /* reallocate a block */
void     eppic_maketemp(void *p);   /* put a block on the temp list */
void     eppic_freetemp(void);      /* free the temp list */
VALUE_S *eppic_makebtype(ull);      /* create a default base type value (int) */
void    eppic_setmemdebug(int on);  /* turn memory debug on / off */
/* handle values */
VALUE_S *eppic_newval(void);        /* get a new placeholder for a value */
void     eppic_freeval(VALUE_S *);  /* free a value* and associated structs */
VALUE_S *eppic_makestr(char *);     /* create a string value */
ull  eppic_getval(VALUE_S*);        /* transform a random value to a ull */
VALUE_S *eppic_cloneval(VALUE_S *); /* make a clone of a value */

/* array related */
/* add a new array element to a value */
void     eppic_addvalarray(VALUE_S*v, VALUE_S*idx, VALUE_S*val);
/* return the value associated with a int index */
VALUE_S *eppic_intindex(VALUE_S *, int);    
/* return the value associated with a 'string' index */
VALUE_S *eppic_strindex(VALUE_S *, char *);
/* set the value of an array element */
void     eppic_setarrbval(ARRAY_S*, int);   
/* get the array element coresponding to index */
ARRAY_S *eppic_getarrval(ARRAY_S**, VALUE_S*);
/* get the initiale array for a variable */
ARRAY_S *eppic_addarrelem(ARRAY_S**, VALUE_S*, VALUE_S*);
/* dereference a pointer to its value */
void  eppic_do_deref(VALUE_S* v, VALUE_S* ref);
void  eppic_addneg(char *name);

/* type manipulation */
int eppic_is_struct(int);
int eppic_is_enum(int);
int eppic_is_union(int);
int eppic_is_typedef(int);
int eppic_type_gettype(TYPE_S*t);
int eppic_chkfname(char *fname, void *vfd);
int eppic_loadunload(int load, char *name, int silent);

void eppic_type_settype(TYPE_S*t, int type);
void eppic_setcallback(void (*scb)(char *, int));
void eppic_vilast(void);
void eppic_vi(char *fname, int file);
void eppic_type_setsize(TYPE_S*t, int size);
int eppic_type_getsize(TYPE_S*t);
void eppic_type_setidx(TYPE_S*t, ull idx);
void eppic_type_setidxbyname(TYPE_S *t, char *name);
ull eppic_type_getidx(TYPE_S*t);
/* for backward compatibility */
#define eppic_typeislocal eppic_type_islocal
int eppic_type_islocal(TYPE_S*t);
void eppic_type_setidxlst(TYPE_S*t, int *idxlst);
void eppic_type_setref(TYPE_S*t, int ref, int type);
void eppic_type_setfct(TYPE_S*t, int val);
void eppic_type_mkunion(TYPE_S*t);
void eppic_type_mkenum(TYPE_S*t);
void eppic_type_mkstruct(TYPE_S*t);
void eppic_type_mktypedef(TYPE_S*t);
int eppic_type_isctype(TYPE_S*t);
int eppic_type_isinvmcore(TYPE_S*t);
TYPE_S*eppic_newtype(void);
void eppic_freetype(TYPE_S*t);
void eppic_inttype(TYPE_S *t, ull size);
TYPE_S*eppic_getctype(int ctype_t, char *name, int silent);
void eppic_type_free(TYPE_S* t);
void eppic_pushref(TYPE_S*t, int ref);
void eppic_duptype(TYPE_S*to, TYPE_S*from);
void eppic_chktype(TYPE_S*t, char *name);
int eppic_defbsize(void);
TYPE_S*eppic_newbtype(int token);
void eppic_setdbg(unsigned int lvl);
unsigned int eppic_getdbg(void);
void eppic_setname(char *name);
char *eppic_getname(void);
void eppic_setclass(char *class);
char **eppic_getclass(void);
void eppic_setmemaddr(VALUE_S*, ull mem);
TYPE_S*eppic_gettype(VALUE_S *v);
void *eppic_stm_type(void **stmp);
void eppic_member_info(void **stmp, long, long, long, long);

/* struct member functions */
void eppic_member_soffset(MEMBER_S*m, int offset);
void eppic_member_ssize(MEMBER_S*m, int size);
void eppic_member_sfbit(MEMBER_S*m, int fbit);
void eppic_member_snbits(MEMBER_S*m, int nbits);
void eppic_new_member(void **stmp, char *name);
void eppic_member_setidx(void **stmp, TYPE_S *t);

/* enums */
ENUM_S* eppic_add_enum(ENUM_S* e, char* name, int val);
/* defines */
DEF_S*  eppic_add_def(DEF_S* d, char *name, char *val);

/* error handling */
/* display error w/ file/line coordinates */
/* does not return */
void eppic_error(char *, ...);
/* display warning w/ file/line coordinates */
void eppic_warning(char *, ...);
/* display a message and continue */
void eppic_msg(char *, ...);
/* display a debug message */
#define DBG_TYPE            0x00000001
#define DBG_STRUCT          0x00000002
#define DBG_MAC             0x00000004
#define DBG_NAME            0x10000000  // 
#define DBG_ALL             0x0fffffff
void eppic_dbg(int class, int level, char *, ...);
void eppic_dbg_named(int class, char *name, int level, char *, ...);

/* parsers debug flags */
extern int eppicdebug, eppicppdebug;

#endif