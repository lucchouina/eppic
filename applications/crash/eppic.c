/*
 * $Id: eppic.c,v 1.15 2012/01/04 14:46:57 anderson Exp $
 *
 * This file is part of lcrash, an analysis tool for Linux memory dumps.
 *
 * Created by Silicon Graphics, Inc.
 * Contributions by IBM, and others
 *
 * Copyright (C) 1999 - 2005 Silicon Graphics, Inc. All rights reserved.
 * Copyright (C) 2001, 2002 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See the file COPYING for more
 * information.
 */

/* unistd.h must appear before gdb includes in order for getopt() prototype definition to get included */
#include "defs.h"

#include <eppic_api.h>

/*
 *  Global data (global_data.c) 
 */
extern char *args[];      
extern int argcnt;            
extern int argerrs;
#define SYNOPSIS      (0x1)
#define COMPLETE_HELP (0x2)
#define PIPE_TO_LESS  (0x4)
#define KVADDR (0x1)
#define QUIET (0x4)

typedef void (*cmd_func_t)(void);

typedef unsigned long long int ulonglong;

char findsym_namebuf[100];

//
/////////////////////////////////////////////////////////////////////////
/*
	This is the glue between the eppic interpreter and crash.
*/

static int
apigetmem(ull iaddr, void *p, int nbytes)
{
	return readmem(iaddr, KVADDR, p, nbytes, NULL, QUIET);
}

// Since crash is target dependant (build for the 
static uint8_t apigetuint8(void* ptr)
{
uint8_t val;
    if(!readmem((unsigned long)ptr, KVADDR, (char*)&val, sizeof val, NULL, QUIET)) return (uint8_t)-1;
    return val;
}

static uint16_t apigetuint16(void* ptr)
{
uint16_t val;
    if(!readmem((unsigned long)ptr, KVADDR, (char*)&val, sizeof val, NULL, QUIET)) return (uint16_t)-1;
    return val;
}

static uint32_t apigetuint32(void* ptr)
{
uint32_t val;
    if(!readmem((unsigned long)ptr, KVADDR, (char*)&val, sizeof val, NULL, QUIET)) return (uint32_t)-1;
    return val;
}

static uint64_t apigetuint64(void* ptr)
{
uint64_t val;
    if(!readmem((unsigned long)ptr, KVADDR, (char*)&val, sizeof val, NULL, QUIET)) return (uint64_t)-1;
    return val;
}

static int
apiputmem(ull iaddr, void *p, int nbytes)
{
	return 1;
}

/* set idx value to actual array indexes from specified size */
static void
eppic_setupidx(TYPE_S*t, int ref, int nidx, int *idxlst)
{
    eppic_type_setidxlst(t, idxlst);
    return;
        /* put the idxlst in index size format */
        if(nidx) {

                int i;

                for(i=0;i<nidx-1;i++) {
			/* kludge for array dimensions of [1] */
			if (idxlst[i+1] == 0) {
				idxlst[i+1] = 1;
			}
			idxlst[i]=idxlst[i]/idxlst[i+1];
		}

                /* divide by element size for last element bound */
                if(ref) idxlst[i] /= eppic_defbsize();
                else idxlst[i] /= eppic_type_getsize(t);
                eppic_type_setidxlst(t, idxlst);
        }
}

typedef struct apigdbpriv_s {

    void *stmember;
    void *type;
    void *enumt;
    void *value;
    void *name;
    void *addr;
    void *maddr;
    int *idxlst;
    char *tdef;
    int nidx;
    int ref;
    int fct;

} apigdbpriv_t;

static int
api_callback(drill_ops_t op, struct gnu_request *req, const void *v1, const void *v2, const void *v3, const void *v4)
{
apigdbpriv_t *p=(apigdbpriv_t *)req->priv;

    if(op != EOP_MEMBER_NAME && !p->type) {
        eppic_dbg(DBG_ALL, 2, "No type for op '%d', name '%s' stmember '%p'", op, req->name, p->stmember);
        return 1;
    }
    switch(op) {
    
        case EOP_POINTER:  
            eppic_dbg(DBG_ALL, 2, "Pointer '%d'", p->ref);
            p->ref++;
        break;
        case EOP_TYPEDEF:
            eppic_dbg(DBG_ALL, 2, "Typedef '%s' '%s'", req->name, v1);
            p->tdef=eppic_strdup(v1);
        break; 
        case EOP_FUNCTION:
            eppic_dbg(DBG_ALL, 2, "Function'%s'", req->type_name);
        break; 
        case EOP_ARRAY: {
            int total=*(int*)v1;
            int elem=*(int*)v2;
            eppic_dbg(DBG_ALL, 2, "Array total %d elem %d", total, elem);
            if(!p->idxlst) p->idxlst=eppic_calloc(sizeof(int)*(MAXIDX+1));
            if(p->nidx >= MAXIDX) eppic_error("Too many indexes! max=%d", MAXIDX);
            p->idxlst[p->nidx++]=total/elem;
        }
        break;
        case EOP_UNION:
            eppic_type_mkunion(p->type);
	    goto label;

	case EOP_ENUM:
	    eppic_type_mkenum(p->type);
	    goto label;

	case EOP_STRUCT:
	{
	    eppic_type_mkstruct(p->type);
label:
            eppic_dbg(DBG_ALL, 2, "ctype op %d name '%s', size %d typename '%s' tdef='%s' stmember=%p", op, req->name, *(int*)v1, v3, p->tdef, p->stmember);
	    eppic_type_setsize(p->type, *(int*)v1);
            eppic_type_setidxbyname(p->type, (char*)(v3?v3:p->tdef));
            if(p->stmember) {
                if(p->stmember) eppic_member_setidx(p->stmember, p->type);
            }
	}
	break;
        case EOP_INT:
            eppic_dbg(DBG_ALL, 2, "INT '%s' '%s' size %d", req->type_name, req->target_typename, *(long*)v1);
	    eppic_inttype(p->type, *(ull *)v1);
        break;
        case EOP_DONE:
            eppic_dbg(DBG_ALL, 2, "Done '%s' '%s'", req->type_name, req->target_typename);
	    eppic_setupidx(p->type, p->ref, p->nidx, p->idxlst);
	    if(p->fct) eppic_type_setfct(p->type, 1);
	    eppic_pushref(p->type, p->ref);
            if(p->tdef) { eppic_free(p->tdef); p->tdef=NULL; }
            if(p->maddr) {
                /* Arays and struct/unions are indexed via an operator so
                   we add a ref for the operator to resolve. */
                eppic_setmemaddr(p->value, (ull)p->maddr);
                if(!eppic_type_isinvmcore(p->type)) {
                    eppic_pushref(p->type, 1);
                    eppic_do_deref(p->value, p->value);
                }
            }
        break;
        case EOP_MEMBER_SIZES :
            eppic_dbg(DBG_ALL, 2, "Member sizes %d, %d, %d, %d", *(long*)v1, *(long*)v2, *(long*)v3, *(long*)v4);
            eppic_member_info(p->stmember, *(long*)v1, *(long*)v2, *(long*)v3, *(long*)v4);
        break;
        case EOP_MEMBER_NAME :
            eppic_dbg(DBG_ALL, 2, "Checking member '%s' against wanted '%s'", v1, req->member);
	    if(!req->member[0] || !strcmp(req->member, v1) ) {
                eppic_new_member(p->stmember, (void*)v1);
                p->type=eppic_stm_type(p->stmember);
                eppic_dbg(DBG_ALL, 2, "Setting p->t to %p", p->type);
                return 1;
            }
            return 0;
        break;
        case EOP_ENUMVAL:
            p->enumt=eppic_add_enum(p->enumt, eppic_strdup(v1), (int) *(long long*)v2);
        break;
        case EOP_VALUE:
            if(p->value) {
                p->maddr=(void*)symbol_value(req->name);
                eppic_dbg(DBG_ALL, 2, "variable '%s' is '%p'", req->name, p->maddr);
                p->type=eppic_gettype(p->value);
            }
            if(p->addr) *(ull*)p->addr=(ull) *(long*)v1;
            else if (!p->value) return 0; /* typedef check that matches a kernel symbol */
        break;
        case EOP_OOPS:
            eppic_dbg(DBG_ALL, 2, "'%d' : %s", *(drill_ops_t*)v1, v2);
        break;
        default:
            eppic_dbg(DBG_ALL, 2, "Unknow EOP %d", op);
        break;
    }
    return 1;
}

/* extract a complex type (struct, union and enum) */
static int
apigetctype(int ctype, char *name, TYPE_S *tout)
{
    struct gnu_request req;
    apigdbpriv_t priv;
    char buf[1024];
    eppic_dbg_named(DBG_TYPE, name, 1, "Looking for type %d name [%s]...\n", ctype, name);
    BZERO(&req, sizeof req);
    BZERO(&priv, sizeof priv);
    req.command = GNU_GET_DATATYPE;
    req.flags |= GNU_RETURN_ON_ERROR;
    req.name = buf;
    req.member = NULL;
    req.fp = pc->nullfp;
    req.priv = &priv;
    priv.type=tout;
    req.tcb = api_callback;
    buf[sizeof buf-1]='\0';
    
    if(eppic_is_struct(ctype)) snprintf(buf, sizeof buf, "struct %s", name);
    else {
        if(eppic_is_union(ctype)) snprintf(buf, sizeof buf, "union %s", name);
        else snprintf(buf, sizeof buf, "%s", name);
    }

    eppic_dbg_named(DBG_TYPE, name, 1, "Looking up ctype %d name '%s'\n", ctype, buf);
    gdb_interface(&req);

    if(req.typecode == TYPE_CODE_UNDEF) {
        eppic_dbg_named(DBG_TYPE, name, 1, "ctype not Found '%s' - ctype [%d]\n", name, ctype);
        return 0;
    }
    else if(eppic_is_typedef(ctype) && !req.is_typedef) {
        eppic_dbg_named(DBG_TYPE, name, 1, "ctype not a typedef '%s' - ctype [%d]\n", name, ctype);
        return 0;
    }
    else eppic_dbg_named(DBG_TYPE, name, 1,"ctype %d name '%s' FOUND\n", ctype, name);
    return 1;
}


/* 
	Get the type, size and position information for all member of a structure.
*/
static int
apimember(char *sname, char *mname, void **stmp)
{
    struct gnu_request req;
    apigdbpriv_t priv;
    eppic_dbg(DBG_ALL, 2, "Looking for members of [%s]...", sname);
    eppic_dbg_named(DBG_TYPE, sname, 2, "Looking for members of [%s]...", sname);    
    BZERO(&req, sizeof req);
    BZERO(&priv, sizeof priv);
    req.command = GNU_GET_DATATYPE;
    req.flags |= GNU_RETURN_ON_ERROR;
    req.name = sname;
    req.member = mname;
    req.priv = &priv;
    req.tcb = api_callback;
    req.fp = pc->nullfp;
    priv.stmember=stmp;
    gdb_interface(&req);
    if(req.member_offset >= 0) return 1;
    return 0;
}

static char *
apigetrtype(char *tstr, TYPE_S *t)
{
	return 0;
}

/* test a string for being a hex number */
static int
is_hex(char *c)
{
	char *cp = c;

	if (*cp == '0' && (*(cp+1) == 'x' || *(cp+1) == 'X'))
		return 1;
	while (*cp != '\0') {
		if ((*cp >= 'a') && (*cp <= 'f')) {
			cp++;
			continue;
		}
		if ((*cp >= 'A') && (*cp <= 'F')) {
			cp++;
			continue;
		}
		if ((*cp >= '0') && (*cp <= '9')) {
			cp++;
			continue;
		}
		return 0;
	}
	return 1;
}

/*
   	Return the name of a symbol at an address (if any)
        Or return the address of a symbol
*/
static char*
apifindsym(char *p)
{
	struct syment *syp;
	ulong value, offset;

	if (is_hex(p)) {
		value = strtoull(p, 0, 16);
		syp = value_search(value, (ulong *)&offset);
		if (syp) {
			sprintf(findsym_namebuf, "%s+%#lx", syp->name, value - syp->value);
			return (char *)findsym_namebuf;
		} else {
			findsym_namebuf[0] = 0;
			return (char *)findsym_namebuf;
		}
	} else {
		syp = symbol_search(p);
		if (syp) {
			sprintf(findsym_namebuf, "%#lx", syp->value);
			return (char *)findsym_namebuf;
		} else {
			findsym_namebuf[0] = 0;
			return (char *)findsym_namebuf;
		}
	}
}

/* get the value of a symbol */
static int
apigetval(char *name, ull *addr, VALUE_S *value)
{
    struct gnu_request req;
    apigdbpriv_t priv;

    TYPE_S *stype;
    eppic_dbg(DBG_ALL, 2, "Looking for value of [%s]...", name);
    eppic_dbg_named(DBG_TYPE, name, 2, "Looking for value of [%s]...", name);    
    BZERO(&req, sizeof req);
    BZERO(&priv, sizeof priv);
    req.command = GNU_GET_DATATYPE;
    req.flags |= GNU_RETURN_ON_ERROR;
    req.name = name;
    req.priv = &priv;
    req.fp = pc->nullfp;
    priv.value=value;
    priv.addr=addr;
    priv.type=eppic_gettype(value);
    req.tcb = api_callback;
    gdb_interface(&req);
    if(req.typecode == TYPE_CODE_UNDEF) {
        eppic_dbg(DBG_ALL, 2, "Value of name '%s' not found", name);
        eppic_dbg_named(DBG_TYPE, name, 2, "Not Found.\n");
        return 0;
    }
    else eppic_dbg(DBG_ALL, 2, "value of name '%s' FOUND", name);
    return 1;
}

/*
	Get the list of enum symbols.
*/
int
apigetenum(char *name, ENUM_S *e)
{
    struct gnu_request req;
    apigdbpriv_t priv;

    TYPE_S *stype;
    eppic_dbg_named(DBG_TYPE, name, 2, "Looking for enum of [%s]...", name);    
    BZERO(&req, sizeof req);
    BZERO(&priv, sizeof priv);
    req.command = GNU_GET_DATATYPE;
    req.flags |= GNU_RETURN_ON_ERROR;
    req.name = name;
    req.priv = &priv;
    priv.enumt=e;
    req.fp = pc->nullfp;
    req.tcb = api_callback;
    gdb_interface(&req);
    if(req.typecode == TYPE_CODE_UNDEF) {
        eppic_dbg_named(DBG_TYPE, name, 2, "Enum '%s' Not Found.\n", name);
        return 0;
    }
    else eppic_dbg_named(DBG_TYPE, name, 2, "Enum '%s' Found.\n", name); 
    return 1;
}

/*
	Return the list of preprocessor defines.
	For Irix we have to get the die for a startup.c file.
	Of dwarf type DW_TAG_compile_unit.
	the DW_AT_producer will contain the compile line.

	We then need to parse that line to get all the -Dname[=value]
*/
DEF_S *
apigetdefs(void)
{
DEF_S *dt=0;
int i;
static struct linuxdefs_s {

	char *name;
	char *value;

} linuxdefs[] = {

	{"CURTASK",		"(struct task_struct*)curtask()"},
	{"crash",		"1"},
	{"linux",		"1"},
	{"__linux",		"1"},
	{"__linux__",		"1"},
	{"unix",		"1"},
	{"__unix",		"1"},
	{"__unix__",		"1"},
	// helper macros
	{"LINUX_2_2_16",	"(LINUX_RELEASE==0x020210)"},
	{"LINUX_2_2_17",	"(LINUX_RELEASE==0x020211)"},
	{"LINUX_2_4_0",		"(LINUX_RELEASE==0x020400)"},
	{"LINUX_2_2_X",		"(((LINUX_RELEASE) & 0xffff00) == 0x020200)"},
	{"LINUX_2_4_X",		"(((LINUX_RELEASE) & 0xffff00) == 0x020400)"},
	{"LINUX_2_6_X",		"(((LINUX_RELEASE) & 0xffff00) == 0x020600)"},
	{"LINUX_3_X_X",         "(((LINUX_RELEASE) & 0xff0000) == 0x030000)"},
	{"NULL",         "0"},
#ifdef i386
	{"i386",         "1"},
	{"__i386",       "1"},
	{"__i386__",     "1"},
	{"BITS_PER_LONG","32"},
#endif
#ifdef s390
	{"s390",         "1"},
	{"__s390",       "1"},
	{"__s390__",     "1"},
	{"BITS_PER_LONG","32"},
#endif
#ifdef s390x
	 {"s390x",       "1"},
	 {"__s390x",     "1"},
	 {"__s390x__",   "1"},
	{"BITS_PER_LONG","32"},
#endif
#ifdef __ia64__
	{"ia64",         "1"},
	{"__ia64",       "1"},
	{"__ia64__",     "1"},
	{"__LP64__",     "1"},
	{"_LONGLONG",    "1"},
	{"__LONG_MAX__", "9223372036854775807L"},
	{"BITS_PER_LONG","64"},
#endif
#ifdef __x86_64__
	{"x86_64",         "1"},
	{"_x86_64_",       "1"},
	{"__x86_64__",     "1"},
	{"__LP64__",     "1"},
	{"_LONGLONG",    "1"},
	{"__LONG_MAX__", "9223372036854775807L"},
	{"BITS_PER_LONG","64"},
#endif
#ifdef ppc64
	{"ppc64",       "1"},
	{"__ppc64",     "1"},
	{"__ppc64__",   "1"},
	{"BITS_PER_LONG","64"},
#endif
	};
        
static char *untdef[] = { 
    "clock",  
    "mode",  
    "pid",  
    "uid",  
    "xtime",  
    "init_task", 
    "size", 
    "type",
    "level",
    0 
};

        /* remove some tdef with very usual identifier.
           could also be cases where the kernel defined a type and variable with same name e.g. xtime.
           the same can be accomplished in source using #undef <tdefname> or forcing the evaluation of 
           a indentifier as a variable name ex: __var(xtime).
           
           I tried to make the grammar as unambiqguous as I could.
           
           If this becomes to much of a problem I might diable usage of all image typedefs usage in eppic!
        */ 
        {
            char **tdefname=untdef;
            while(*tdefname) eppic_addneg(*tdefname++);;
            
        }
        
	/* insert constant defines from list above */
	for(i=0;i<sizeof(linuxdefs)/sizeof(linuxdefs[0]);i++) {

		dt=eppic_add_def(dt, eppic_strdup(linuxdefs[i].name), 
			eppic_strdup(linuxdefs[i].value));
	}

        {
            ull addr;
            char banner[200];
            addr=symbol_value("linux_banner");
            if(apigetmem(addr, banner, sizeof banner-1)) {

                // parse the banner string and set up release macros
                banner[sizeof banner -1]='\0';
                char *tok=strtok(banner, " \t");
                if(tok) tok=strtok(NULL, " \t");
                if(tok) tok=strtok(NULL, " \t");
                if(tok) {
                    int version, patchlevel, sublevel, ret;
                    ret = sscanf(tok, "%d.%d.%d-", &version, &patchlevel, &sublevel);
		    switch (ret) {
		    case 2:
			sublevel = 0;
		    case 3:
			sprintf(banner, "0x%02x%02x%02x", version, patchlevel, sublevel);
		        dt=eppic_add_def(dt, eppic_strdup("LINUX_RELEASE"), eppic_strdup(banner));
                        //eppic_msg("Core LINUX_RELEASE == '%s'\n", tok);
		    default:
			break;
                    }
                }
            }
            else eppic_msg("Eppic init: could not read symbol 'linux_banner' from corefile.\n");
        }
	return dt;
}

apiops icops= {
	apigetmem, 
	apiputmem, 
	apimember, 
	apigetctype, 
	apigetrtype, 
	apigetval, 
	apigetenum, 
	apigetdefs,
	apigetuint8,
	apigetuint16,
	apigetuint32,
	apigetuint64,
	apifindsym
};

void
eppic_version(void)
{
	eppic_msg("< Eppic interpreter version %d.%d >\n"
		, S_MAJOR, S_MINOR);
}

static void
run_callback(void)
{
extern char *crash_global_cmd();
FILE *ofp = NULL;

	if (fp) {
		ofp = eppic_getofile();
		eppic_setofile(fp);
	}

	eppic_cmd(crash_global_cmd(), args, argcnt);

	if (ofp) 
		eppic_setofile(ofp);
}


void
edit_cmd(void)
{
int c, file=0;
        while ((c = getopt(argcnt, args, "lf")) != EOF) {
                switch(c)
                {
                case 'l':
                    eppic_vilast();
                    return;
                break;
                case 'f':
                    file++;
                break;
                default:
                        argerrs++;
                        break;
                }
        }

        if (argerrs)
                cmd_usage(crash_global_cmd(), SYNOPSIS);

        else if(args[optind]) {
            while(args[optind]) {
	        eppic_vi(args[optind++], file);
            }
	}
        else cmd_usage(crash_global_cmd(), SYNOPSIS);
}

char *edit_help[]={
		"edit",
                "Start a $EDITOR session of a eppic function or file",
                "<-f fileName>|<function name>",
                "This command can be use during a tight development cycle",
                "where frequent editing->run->editing sequences are executed.",
                "To edit a known eppic macro file use the -f option. To edit the file",
                "at the location of a known function's declaration omit the -f option.",
                "Use a single -l option to be brought to the last compile error location.",
                "",
                "EXAMPLES:",
                "  %s> edit -f ps",
                "  %s> edit ps",
                "  %s> edit ps_opt",
                "  %s> edit -l",
                NULL
};


void
load_cmd(void)
{
	if(argcnt< 2) cmd_usage(crash_global_cmd(), SYNOPSIS);
	else {
            eppic_setofile(fp);
            eppic_loadunload(1, args[1], 0);
        }
}

char *load_help[]={
		"load",
                "Load a eppic file",
                "<fileName>|<Directory>",
                "  Load a file or a directory. In the case of a directory",
		"  all files in that directory will be loaded.",
                NULL
                
};

void
unload_cmd(void)
{
	if(argcnt < 2) cmd_usage(crash_global_cmd(), SYNOPSIS);
	else eppic_loadunload(0, args[1], 0);
}

char *unload_help[]={
		"unload",
                "Unload a eppic file",
                "<fileName>|<Directory>",
                "  Unload a file or a directory. In the case of a directory",
		"  all files in that directory will be unloaded.",
                NULL
};

void
sdebug_cmd(void)
{
	if(argcnt < 2) eppic_msg("Current eppic debug level is %d\n", eppic_getdbg());
	else eppic_setdbg(atoi(args[1]));
}

char *sdebug_help[]={
		"sdebug",
                "Print or set eppic debug level",
                "<Debug level 0..9>",
                "  Set the debug of eppic. Without any parameter, shows the current debug level.",
                NULL
};

void
sname_cmd(void)
{
	if(argcnt < 2) {
            if(eppic_getname()) eppic_msg("Current eppic name match is '%s'\n", eppic_getname());
            else eppic_msg("No name match specified yet.\n");
	} else eppic_setname(args[1]);
}

char *sname_help[]={
		"sname",
                "Print or set eppic name match.",
                "<name>",
                "  Set eppic name string for matches. Debug messages that are object oriented",
                "  will only be displayed if the object name (struct, type, ...) matches this",
		"  value.",
                NULL
};

void
sclass_cmd(void)
{
	if(argcnt < 2) {
            char **classes=eppic_getclass();
            eppic_msg("Current eppic classes are :");
            while(*classes) eppic_msg("'%s' ", *classes++);
            eppic_msg("\n");
	
        }
        else {
            int i;
            for(i=1; i<argcnt; i++) eppic_setclass(args[i]);
        }
}

char *sclass_help[]={
		"sclass",
                "Print or set eppic debug message class(es).",
                "<className>[, <className>]",
                "  Set eppic debug classes. Only debug messages that are in the specified classes",
                "  will be displayed.",
                NULL
};

#define NCMDS 200
static struct command_table_entry command_table[NCMDS] =  {

	{"edit", edit_cmd, edit_help},
	{"load", load_cmd, load_help},
	{"unload", unload_cmd, unload_help},
	{"sdebug", sdebug_cmd, sdebug_help},
	{"sname", sname_cmd, sname_help},
	{"sclass", sclass_cmd, sclass_help},
	{(char *)0 }
};

static void
add_eppic_cmd(char *name, void (*cmd)(void), char **help, int flags)
{
struct command_table_entry *cp;
struct command_table_entry *crash_cmd_table();

    // check for a clash with native commands
    for (cp = crash_cmd_table(); cp->name; cp++) {
        if (!strcmp(cp->name, name)) {
            eppic_msg("Eppic command name '%s' conflicts with native crash command.\n", name);
            return;
        }
    }

    // make sure we have enough space for the new command
    if(!command_table[NCMDS-2].name) {
        for (cp = command_table; cp->name; cp++);
        cp->name=eppic_strdup(name);
        cp->func=cmd;
        cp->help_data=help;
        cp->flags=flags;
        eppic_msg("\t\t\tcommand : %s - %s\n", name, help[0]);
    }
}

static void
rm_eppic_cmd(char *name)
{
struct command_table_entry *cp, *end;

    for (cp = command_table; cp->name; cp++) {
        if (!strcmp(cp->name, name)) {
            eppic_free(cp->name);
            memmove(cp, cp+1, sizeof *cp *(NCMDS-(cp-command_table)-1));
            break;
        }
    }
}

/*
	This function is called for every new function
	generated by a load command. This enables us to
	register new commands.

	We check here is the functions:

	fname_help()
	fname_opt()
	and
	fname_usage()

	exist, and if so then we have a new command.
	Then we associated (register) a function with
	the standard eppic callbacks.
*/
void
reg_callback(char *name, int load)
{
char fname[MAX_SYMNAMELEN+sizeof("_usage")+1];
char *help_str, *opt_str;
char **help=malloc(sizeof *help * 5);

    if(!help) return;
    snprintf(fname, sizeof(fname), "%s_help", name);
    if(eppic_chkfname(fname, 0)) {
        snprintf(fname, sizeof(fname), "%s_usage", name);
        if(eppic_chkfname(fname, 0)) {
            if(load) {
                opt_str=eppic_strdup((char*)(unsigned long)eppic_exefunc(fname, 0));
                snprintf(fname, sizeof(fname), "%s_help", name);
                help_str=eppic_strdup((char*)(unsigned long)eppic_exefunc(fname, 0));
                help[0]=eppic_strdup(name);
                help[1]="";
                help[2]=eppic_strdup(opt_str);
                help[3]=eppic_strdup(help_str);
                help[4]=0;
                add_eppic_cmd(name, run_callback, help, 0);
                eppic_free(help_str);
                eppic_free(opt_str);
                return;
            }
            else rm_eppic_cmd(name);
        }
    }
    free(help);
    return;
}

/* 
 *  The _fini() function is called if the shared object is unloaded. 
 *  If desired, perform any cleanups here. 
 */
void __attribute__((destructor))
eppic_fini(void) 
{ 
    // need to unload any files we have loaded
    
}

VALUE_S *curtask(VALUE_S *v, ...)
{
unsigned long get_curtask();
    return eppic_makebtype((ull)get_curtask());
}

void __attribute__((constructor)) 
eppic_init(void) /* Register the command set. */
{ 
#define LCDIR "/usr/share/eppic/crash"
#define LCIDIR "include"
#define LCUDIR ".eppic"

	if(eppic_open() >= 0) {

		char *path, *ipath;
		char *homepath=0;
               	char *home=getenv("HOME");

		/* set api, default size, and default sign for types */
#ifdef i386
#define EPPIC_ABI  ABI_INTEL_X86
#else 
#ifdef __ia64__
#define EPPIC_ABI  ABI_INTEL_IA
#else
#ifdef __x86_64__
#define EPPIC_ABI  ABI_INTEL_IA
#else
#ifdef __s390__
#define EPPIC_ABI  ABI_S390
#else
#ifdef __s390x__
#define EPPIC_ABI  ABI_S390X
#else
#ifdef PPC64
#define EPPIC_ABI  ABI_PPC64
#else
#ifdef ARM64
#define EPPIC_ABI  ABI_ARM64
#else
#error eppic: Unkown ABI 
#endif
#endif
#endif
#endif
#endif
#endif
#endif


		eppic_apiset(&icops, EPPIC_ABI, sizeof(long), 0);

		eppic_version();

        	/* set the macro search path */
        	if(!(path=getenv("EPPIC_MPATH"))) {

                	if(home) {

                        	path=eppic_alloc(strlen(home)+sizeof(LCUDIR)+sizeof(LCDIR)+4);
				homepath=eppic_alloc(strlen(home)+sizeof(LCUDIR)+2);

				/* build a path for call to eppic_load() */
				strcpy(homepath, home);
				strcat(homepath, "/");
				strcat(homepath, LCUDIR);

				/* built the official path */
                        	strcpy(path, LCDIR);
                        	strcat(path, ":");
                        	strcat(path, home);
                        	strcat(path, "/");
				strcat(path, LCUDIR);
                	}
                	else path=LCDIR;
		}
		eppic_setmpath(path);

		fprintf(fp, "\tLoading eppic commands from %s .... \n",
                                         path);

		/* include path */
		if(!(ipath=getenv("EPPIC_IPATH"))) {

                	if(home) {

                        	ipath=eppic_alloc(strlen(home)+sizeof(LCDIR)+sizeof(LCUDIR)+(sizeof(LCIDIR)*2)+(sizeof("/usr/include")+2)+6);

				/* built the official path */
                        	strcpy(ipath, LCDIR);
                        	strcat(ipath, "/"LCIDIR":");
                        	strcat(ipath, home);
                        	strcat(ipath, "/");
				strcat(ipath, LCUDIR);
				strcat(ipath, "/"LCIDIR);
				strcat(ipath, ":/usr/include");
                	}
                	else ipath=LCDIR"/"LCIDIR;
		}
		eppic_setipath(ipath);

		/* set the new function callback */
		eppic_setcallback(reg_callback);

		/* load the default macros */
		eppic_loadall();

		/* load some eppic specific commands */
                register_extension(command_table);
                
                /* some builtins */
        	eppic_builtin("unsigned long curtask()", curtask);
                
                fprintf(fp, "Done.\n");
	} 
}
