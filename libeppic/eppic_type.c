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
#include "eppic.tab.h"
#include <string.h>
#include <errno.h>
#include <endian.h>
/*
    This file contains functions that deals with type and type
    casting operators.
*/
#define B_SIZE_MASK 0x00007f0
#define B_SIGN_MASK 0x000f000
#define B_STOR_MASK 0x01f0000
#define B_CHAR      0x0000010
#define B_SHORT     0x0000020
#define B_INT       0x0000040
#define B_LONG      0x0000080
#define B_LONGLONG  0x0000100
#define B_FLOAT     0x0000200
#define B_CONST     0x0000400
#define B_SIGNED    0x0001000
#define B_UNSIGNED  0x0002000
#define B_STATIC    0x0010000
#define B_REGISTER  0x0020000
#define B_VOLATILE  0x0040000
#define B_TYPEDEF   0x0080000
#define B_EXTERN    0x0100000
#define B_VOID      0x0800000
#define B_USPEC     0x0000001 /* user specified sign */
#define B_ENUM      0x0000002 /* btype is from a enum */

#define is_size(i)  ((i)&B_SIZE_MASK)
#define is_sign(i)  ((i)&B_SIGN_MASK)
#define is_stor(i)  ((i)&B_STOR_MASK)
#define issigned(v) (v->type.typattr & B_SIGNED)
#define vsize(v)    (is_size(v->type.typattr))

static struct {
    int btype;
    int key;
    char *name;
} blut[] = {
    { B_VOID,   VOID ,      "void"},
    { B_TYPEDEF,    TDEF ,      "tdef"},
    { B_EXTERN, EXTERN ,    "extern"},
    { B_STATIC, STATIC ,    "static"},
    { B_VOLATILE,   VOLATILE ,  "volatile"},
    { B_CONST,  CONST ,     "const"},
    { B_REGISTER,   REGISTER ,  "register"},
    { B_UNSIGNED,   UNSIGNED ,  "unsigned"},
    { B_SIGNED, SIGNED ,    "signed"},
    { B_CHAR,   CHAR,       "char" },
    { B_SHORT,  SHORT ,     "short"},
    { B_INT,    INT ,       "int"},
    { B_LONG,   LONG ,      "long"},
    { B_LONGLONG,   DOUBLE ,    "long long"},
    { B_FLOAT,  FLOAT ,     "float"},
};

type_t *
eppic_newtype()
{
    return eppic_calloc(sizeof(type_t));
}

void
eppic_freetype(type_t* t)
{
    if(t->idxlst) eppic_free(t->idxlst);
    eppic_free(t);
}

/* this function is called by the parser to merge the
   storage information (being hold in a basetype) into the real type_t*/
type_t*
eppic_addstorage(type_t*t1, type_t*t2)
{
    t1->typattr |= is_stor(t2->typattr);
    eppic_freetype(t2);
    return t1;
}

char *
eppic_ctypename(int type)
{
    switch(type) {

        case V_TYPEDEF: return "typedef";
        case V_STRUCT: return "struct";
        case V_UNION: return "union";
        case V_ENUM: return "enum";
        default: return "???";
    }
}

int eppic_isstatic(int atr) { return atr & B_STATIC; }
int eppic_isenum(int atr) { return atr & B_ENUM; }
int eppic_isconst(int atr) { return atr & B_CONST; }
int eppic_issigned(int atr) { return atr & B_SIGNED; }
int eppic_istdef(int atr) { return atr & B_TYPEDEF; }
int eppic_isxtern(int atr) { return atr & B_EXTERN; }
int eppic_isvoid(int atr) { return atr & B_VOID; }
int eppic_isstor(int atr) { return is_stor(atr); }
int eppic_is_struct(int ctype) { return ctype==V_STRUCT; }
int eppic_is_enum(int ctype) { return ctype==V_ENUM; }
int eppic_is_union(int ctype) { return ctype==V_UNION; }
int eppic_is_typedef(int ctype) { return ctype==V_TYPEDEF; }

/* type seting */
int eppic_type_gettype(type_t*t) { return t->type; }
void eppic_type_settype(type_t*t, int type) { t->type=type; }
void eppic_type_setsize(type_t*t, int size) { t->size=size; }
int  eppic_type_getsize(type_t*t) { return t->size; }
void eppic_type_setidx(type_t*t, ull idx) { t->idx=idx; }
ull eppic_type_getidx(type_t*t) { return t->idx; }
void eppic_type_setidxlst(type_t*t, int* idxlst) { t->idxlst=idxlst; }
void eppic_type_setref(type_t*t, int ref, int type) { t->ref=ref; t->rtype=type; }
void eppic_type_setfct(type_t*t, int val) { t->fct=val; }
void eppic_type_mkunion(type_t*t) { t->type=V_UNION; }
void eppic_type_mkenum(type_t*t) { t->type=V_ENUM; }
void eppic_type_mkstruct(type_t*t) { t->type=V_STRUCT; }
void eppic_type_mktypedef(type_t*t) { t->type=V_TYPEDEF; }

static int defbtype=B_LONG|B_SIGNED;
static int defbidx=B_SL;
static int defbsize=4;
static int defbsign=B_SIGNED;
int eppic_defbsize() { return defbsize; }

char *
eppic_getbtypename(int typattr)
{
int i;
char *name=eppic_alloc(200);

    name[0]='\0';
    for(i=0;i<sizeof(blut)/sizeof(blut[0]);i++) {

        /* skip sign attr. if defaults */
        if(is_sign(blut[i].btype)) {

            if(!(typattr & B_USPEC)) continue;
            if(typattr & B_INT) {
                if(blut[i].btype==B_SIGNED) continue;
            } else if(typattr & B_CHAR) {
                if(blut[i].btype == defbsign) continue;
            } else if(blut[i].btype==B_UNSIGNED) continue;
        }

        if(typattr & blut[i].btype) {

            strcat(name, blut[i].name);
            if(i<(sizeof(blut)/sizeof(blut[0]))-1) {

                strcat(name, " ");
            }
        }
    }
    return name;
}

/* promote a random base or ref into a ull */
ull
unival(value_t *v)
{
    if(v->type.type==V_REF) {

        return TYPE_SIZE(&v->type)==4 ? (ull)(v->v.ul) : v->v.ull;

    } else switch(v->type.idx) {

                case B_SC: return (ull)(v->v.sc);
                case B_UC: return (ull)(v->v.uc);
                case B_SS: return (ull)(v->v.ss);
                case B_US: return (ull)(v->v.us);
                case B_SL: return (ull)(v->v.sl);
                case B_UL: return (ull)(v->v.ul);
                case B_SLL: return (ull)(v->v.sll);
                case B_ULL: return (ull)(v->v.ull);
                default: eppic_error("Oops univ()[%d]", TYPE_SIZE(&v->type)); break;
    }
    return 0;
}

void
eppic_duptype(type_t*t, type_t*ts)
{
    // eppic_do_deref can call in here with same pointer
    if(t == ts) return;
    
    memmove(t, ts, sizeof(type_t));
    if(ts->idxlst) {

        t->idxlst=eppic_calloc(sizeof(int)*(MAXIDX+1));
        memmove(t->idxlst, ts->idxlst, sizeof(int)*(MAXIDX+1));
    }
}

#define asarray(v) (v->arr!=v->arr->next)

/*
    Duplicate a value_t.
    On duplication we do verification of the value_ts involved.
    this is to make it possible to pass array to subfunctions
    and to override specific value_ts that also have arrays attached
    to them.
*/
void
eppic_dupval(value_t *v, value_t *vs)
{
int isvoid=(v->type.typattr & B_VOID);

    /* if both have an attached array ... fail */
    if(asarray(v) && asarray(vs)) {

        eppic_error("Can't override array");

    }
    /* when we are attaching a new array to the destination value_t
       we need to add the destination reference count to the source */
    if(asarray(v)) {

        array_t*a=v->arr;

        /* preserve the array accross the freedata and memmove */
        v->arr=0;
        eppic_freedata(v);

        /* copy the new value_t over it */
        memmove(v, vs, sizeof(value_t));

        /* and restore the array_t*/
        v->arr=a;

    } else {

        eppic_refarray(vs, 1);
        eppic_freedata(v);
        memmove(v, vs, sizeof(value_t));
    } 

    eppic_duptype(&v->type, &vs->type);
    eppic_dupdata(v, vs);

    /* conserve the void atribute across asignements */
    v->type.typattr |= isvoid;
}

/*
    clone a value_t.
*/
value_t *
eppic_cloneval(value_t *v)
{
value_t *nv=eppic_alloc(sizeof(value_t));

    memmove(nv, v, sizeof(value_t));
    eppic_refarray(v, 1);
    eppic_dupdata(nv, v);
    return nv;
}

static signed long long 
twoscomp(ull val, int nbits)
{
    return val | (0xffffffffffffffffll << nbits);
    // XXX return (val-1)^0xffffffffll;
}

/*
    Get a bit field value_t from system image or live memory.
    We do all operations with a ull untill the end.
    Then we check for the basetype size and sign and convert
    apropriatly.
*/
void
get_bit_value(ull val, int nbits, int boff, int size, value_t *v)
{
    ull mask;
    int dosign=0;
    int vnbits=size*8;

    /* first get the value_t */
    if (nbits >= 32) {
            int upper_bits = nbits - 32;
            mask = ((1 << upper_bits) - 1);
            mask = (mask << 32) | 0xffffffff;
    }
    else {
            mask = ((1 << nbits) - 1);
    }

    if (__BYTE_ORDER == __LITTLE_ENDIAN)
        val = val >> boff;
    else
        val = val >> (vnbits - boff - nbits);
    val &= mask;

    if(issigned(v)) {

        /* get the sign bit */
        if(val >> (nbits-1)) dosign=1;

    }
    switch(vsize(v)) {

        case B_CHAR: {
            if(dosign) {
                v->v.sc=(signed char)twoscomp(val, nbits);
            }
            else {
                v->v.uc=val;
            }
        }
        break;
        case B_SHORT: {
            if(dosign) {
                v->v.ss=(signed short)twoscomp(val, nbits);
            }
            else {
                v->v.us=val;
            }
        }
        break;
        case B_LONG: 

            if(eppic_defbsize()==8) goto ll;

        case B_INT: {
            if(dosign) {
                v->v.sl=(signed long)twoscomp(val, nbits);
            }
            else {
                v->v.ul=val;
            }
        }
        break;
        case B_LONGLONG: {
ll:
            if(dosign) {
                v->v.sll=(signed long long)twoscomp(val, nbits);
            }
            else {
                v->v.ull=val;
            }
        }
        break;
        default:
            eppic_error("Oops get_bit_value_t...");
        break;
    }

}
/*
    Set a bit field value_t. dvalue_t is the destination value_t as read
    from either the system image of live memory.
 */
ull
set_bit_value_t(ull dvalue, ull value, int nbits, int boff)
{
    ull mask;

    if (nbits >= 32) {
            int upper_bits = nbits - 32;
            mask = ((1 << upper_bits) - 1);
            mask = (mask << 32) | 0xffffffff;
    }
    else {
            mask = ((1 << nbits) - 1);
    }
    /* strip out the current value_t */
    dvalue &= ~(mask << boff);

    /* put in the new one */
        dvalue |= (value << boff);
    return dvalue;
}

/* this function is called when we have determined the systems
   default int size (64 bit vs 32 bits) */
void 
eppic_setdefbtype(int size, int sign)
{
int idx=B_INT;

    switch(size) {

    case 1: defbtype=B_CHAR; idx=B_UC; break;
    case 2: defbtype=B_SHORT;idx=B_US;  break;
    case 4: defbtype=B_INT; idx=B_UL; break;
    case 8: defbtype=B_LONGLONG; idx=B_ULL; break;

    }
    if(sign) defbsign = B_SIGNED;
    else defbsign = B_UNSIGNED;
    defbtype |= defbsign;
    defbsize=size;
    defbidx=idx;
}

static int
getbtype(int token)
{
int i;

    for(i=0;i<sizeof(blut)/sizeof(blut[0]);i++) {

        if(blut[i].key==token) return blut[i].btype;
    }

    eppic_error("token not found in btype lut [%d]", token);
    return B_UNSIGNED;
}

int
eppic_isjuststatic(int attr)
{
int satr=is_stor(attr);

    return (satr & ~B_STATIC) == 0;
}

value_t *eppic_defbtypesize(value_t *v, ull i, int idx)
{
    v->type.type=V_BASE;
    v->setfct=eppic_setfct;
    v->type.idx=idx;
    v->mem=0;
    switch(idx) {

        case B_UC: case B_SC:
            v->type.size=1;
            v->v.uc=i;
        break;
        case B_US: case B_SS:
            v->type.size=2;
            v->v.us=i;
        break;
        case B_UL: case B_SL:
            v->type.size=4;
            v->v.ul=i;
        break;
        case B_ULL: case B_SLL:
            v->type.size=8;
            v->v.ull=i;
        break;
        default: eppic_error("Oops defbtypesize!"); break;
    }
    return v;
}

value_t *
eppic_defbtype(value_t *v, ull i)
{
    v->type.typattr=defbtype;
    return eppic_defbtypesize(v, i, defbidx);
}

value_t *
eppic_makebtype(ull i)
{
value_t *v=eppic_calloc(sizeof(value_t));

    eppic_defbtype(v, i);
    eppic_setarray(&v->arr);
    TAG(v);
    return v;
}

value_t *
eppic_newval()
{
value_t *v=eppic_makebtype(0);

    return v;
}

void
eppic_setmemaddr(value_t *v, ull mem)
{
        v->mem=mem;
        if(eppic_defbsize()==4) v->v.ul=(ul)mem;
        else v->v.ull=mem;
}

type_t *eppic_gettype(value_t *v)
{
    return &v->type;
}

/* take the current basetypes and generate a uniq index */
static void
settypidx(type_t*t)
{
int v1, v2, v3, size;

    if(t->typattr & B_CHAR) {
        size=1;
        v1=B_SC; v2=B_UC; 
        v3=(defbsign==B_SIGNED?B_SC:B_UC);
    } else if(t->typattr & B_SHORT) {
        size=2;
        v1=B_SS; v2=B_US; v3=B_SS;
    } else if(t->typattr & B_LONG) {
        if(eppic_defbsize()==4) {
            size=4;
            v1=B_SL; v2=B_UL; v3=B_SL;
        } else goto ll;
    } else if(t->typattr & B_INT) {
go:
        size=4;
        v1=B_SL; v2=B_UL; v3=B_SL;
    } else if(t->typattr & B_LONGLONG) {
ll:
        size=8;
        v1=B_SLL; v2=B_ULL; v3=B_SLL;
    }
    else goto go;

    if(t->typattr & B_SIGNED) t->idx=v1;
    else if(t->typattr & B_UNSIGNED) t->idx=v2;
    else t->idx=v3;
    t->size=size;
}

/* take the current basetypes and generate a uniq index */
int
eppic_idxtoattr(int idx)
{
int i;
static struct {

    int idx;
    int attr;

} atoidx[] = {

    {B_SC,  B_SIGNED  | B_CHAR}, 
    {B_UC,  B_UNSIGNED| B_CHAR}, 
    {B_SS,  B_SIGNED  | B_SHORT}, 
    {B_US,  B_UNSIGNED| B_SHORT}, 
    {B_SL,  B_SIGNED  | B_LONG}, 
    {B_UL,  B_UNSIGNED| B_LONG}, 
    {B_SLL, B_SIGNED  | B_LONGLONG}, 
    {B_ULL, B_UNSIGNED| B_LONGLONG}, 
};

    for(i=0; i < sizeof(atoidx)/sizeof(atoidx[0]); i++) {

        if(atoidx[i].idx==idx)  return atoidx[i].attr;
    }
    eppic_error("Oops eppic_idxtoattr!");
    return 0;
}

void
eppic_mkvsigned(value_t*v)
{
    v->type.typattr &= ~B_SIGN_MASK;
    v->type.typattr |= B_SIGNED;
    settypidx(&v->type);
}

/* if there's no sign set the default */
void
eppic_chksign(type_t*t)
{
    if(eppic_isvoid(t->typattr)) return;
    if(!is_sign(t->typattr)) {

        /* char is compile time dependant */
        if(t->idx==B_SC || t->idx==B_UC) t->typattr |= defbsign;
        /* all other sizes are signed by default */
        else t->typattr |= B_SIGNED;
    }
    settypidx(t);
}

/* if ther's no size specification, make it an INT */
void
eppic_chksize(type_t*t)
{
    if(!eppic_isvoid(t->typattr) && !is_size(t->typattr)) eppic_addbtype(t, INT);
}

/* create a new base type element */
type_t*
eppic_newbtype(int token)
{
int btype;
type_t*t=eppic_newtype();

    if(!token) btype=defbtype;
    else {

        btype=getbtype(token);
        if(is_sign(btype)) btype |= B_USPEC;
    }
    t->type=V_BASE;
    t->typattr=btype;
    settypidx(t);
    TAG(t);
    return t;
}

/* set the default sign on a type if user did'nt specify one and not int */
#define set_base_sign(a) if(!(base & (B_USPEC|B_INT))) base = (base ^ is_sign(base)) | a

/*
        char    short   int long    longlong
char        XXX XXX XXX XXX XXX
short       XXX XXX OOO XXX XXX
int         XXX OOO XXX OOO OOO
long        XXX XXX OOO OOO XXX
longlong    XXX XXX OOO XXX XXX

   the parser let's you specify any of the B_ type. It's here that we 
   have to check things out 

*/
type_t*
eppic_addbtype(type_t*t, int newtok)
{
int btype=getbtype(newtok);
int base=t->typattr;

    /* size specification. Check for 'long long' any other 
       combinaison of size is invalid as is 'long long long' */
    if(is_size(btype)) {

        int ibase=base;

        switch(btype) {

            case B_LONG: {


                if(!(base & (B_CHAR|B_SHORT))) {

                    set_base_sign(B_UNSIGNED);

                    if(base & B_LONG || eppic_defbsize()==8) {

                        ibase &= ~B_LONGLONG;
                        base |= B_LONGLONG;
                        base &= ~B_LONG;

                    } else {

                        base |= B_LONG;
                    }
                }
                break;
            }
            case B_INT: {

                /*
                 * This is a bit of a hack to circumvent the
                 * problem that "long int" or "long long int"
                 * is a valid statement in C.
                 */
                if(!(base & (B_INT|B_CHAR|B_LONG|B_LONGLONG))) {

                    set_base_sign(B_SIGNED);
                    base |= B_INT;
                }
                if (base & (B_LONG|B_LONGLONG))
                    ibase = 0;
                break;
            }
            case B_SHORT: {

                if(!(base & (B_SHORT|B_CHAR|B_LONG|B_LONGLONG))) {  

                    base |= B_SHORT;
                    set_base_sign(B_UNSIGNED);
                }

            }
            case B_CHAR: {

                if(!(base & (B_CHAR|B_SHORT|B_INT|B_LONG|B_LONGLONG))) {    

                    base |= B_CHAR;
                    set_base_sign(defbsign);
                }

            }
        }

        if(ibase == base) {

            eppic_warning("Invalid combinaison of sizes");

        }
    
    } else if(is_sign(btype)) {

        if(base & B_USPEC) {

            if(is_sign(btype) == is_sign(base))

                eppic_warning("duplicate type specifier");

            else

                eppic_error("invalid combination of type specifiers");
        }
        /* always keep last found signed specification */
        base ^= is_sign(base);
        base |= btype;
        base |= B_USPEC;

    } else if(is_stor(btype)) {

        if(is_stor(base)) {

            eppic_warning("Suplemental storage class ignore");

        }
        else base |= btype;
    }
    t->typattr=base;
    settypidx(t);
    return t;
}

/* this function gets called back from the API when the user need to parse
   a type declaration. Like when a typedef dwarf returns a type string */

void
eppic_pushref(type_t*t, int ref)
{
    if(t->type==V_REF) {

        t->ref += ref;

    } else {

        t->ref=ref;

        if(ref) {

            t->rtype=t->type;
            t->type=V_REF;
        }
    }
}
void
eppic_popref(type_t*t, int ref)
{

    if(!t->ref) return;

    t->ref-=ref;

    if(!t->ref) {

        t->type=t->rtype;
    }
}

typedef struct {
    int battr;
    char *str;
} bstr;
static bstr btypstr[] = {
    {CHAR,      "char"},
    {SHORT,     "short"},
    {INT,       "int"},
    {LONG,      "long"},
    {DOUBLE,    "double"},
    {SIGNED,    "signed"},
    {UNSIGNED,  "unsigned"},
    {STATIC,    "static"},
    {REGISTER,  "register"},
    {VOLATILE,  "volatile"},
    {VOID,      "void"},
};
int
eppic_parsetype(char *str, type_t*t, int ref)
{
char *p;
char *tok, *pend;
int ctype=0, i, first, found;
type_t*bt=0;

    /* if it's a simple unamed ctype return 0 */
        if(!strcmp(str, "struct")) { t->type=V_STRUCT; return 0; }
        if(!strcmp(str, "enum"))   { t->type=V_ENUM; return 0; }
        if(!strcmp(str, "union"))  { t->type=V_UNION; return 0; }

    p=eppic_strdup(str);

    /* get he level of reference */
    for(pend=p+strlen(p)-1; pend>=p; pend--) {

        if(*pend==' ' || *pend == '\t') continue;
        if(*pend == '*' ) ref ++;
        else break;

    }
    *++pend='\0';

again:
    tok=strtok(p," ");
    if(!strcmp(tok, "struct")) {

        ctype=V_STRUCT;

    } else if(!strcmp(tok, "union")) {

        ctype=V_UNION;

    } else if(!strcmp(tok, "enum")) {
        eppic_free(p);
        p=(char*)eppic_alloc(strlen("unsigned int") + 1);
        /* force enum type into unigned int type for now */
        strcpy(p, "unsigned int");
        goto again;

    }
    if(ctype) {

        char *name=strtok(NULL, " \t");
        bt=eppic_getctype(ctype, name, 1);

        /* we accept unknow struct reference if it's a ref to it */
        /* the user will probably cast it to something else anyway... */
        if(!bt) {

            if(ref) {

                bt=(type_t*)eppic_getvoidstruct(ctype);

            } else {

                eppic_error("Unknown Struct/Union/Enum %s", name);

            }
        }

        eppic_duptype(t, bt);
        eppic_freetype(bt);
        eppic_pushref(t, ref);
        eppic_free(p);
        return 1;
    }

    /* this must be a basetype_t*/
    first=1;
    do {
        found=0;
        for(i=0;i<sizeof(btypstr)/sizeof(btypstr[0]) && !found;i++) {

            if(!strcmp(tok, btypstr[i].str)) {

                found=1;
                if(first) {
                    first=0;
                    bt=eppic_newbtype(btypstr[i].battr);
                }
                else {

                    eppic_addbtype(bt, btypstr[i].battr);

                }

            }

        }
        if(!found) break;
    
    } while((tok=strtok(0, " \t")));

    /* if the tok and bt is set that means there was a bad token */
    if(bt && tok) {

        eppic_error("Oops typedef expension![%s]",tok);

    }
    /* could be a typedef */
    if(!bt) {

        int ret=0;

        if((bt=eppic_getctype(V_TYPEDEF, tok, 1))) {

            eppic_duptype(t, bt);
            eppic_freetype(bt);
            eppic_free(p);
            return ret;

        }
        eppic_free(p);
        return ret;

    }
    else if(bt) {

        /* make sure we have signed it and sized it */
        eppic_chksign(bt);
        eppic_chksize(bt);

        eppic_duptype(t, bt);
        eppic_freetype(bt);
        eppic_pushref(t, ref);
        eppic_free(p);
        return 1;

    }
    eppic_free(p);
    return 0;
}

type_t*
eppic_newcast(var_t*v)
{
type_t*type=eppic_newtype();

    eppic_duptype(type, &v->next->v->type);
    eppic_freesvs(v);
    return type;
}

typedef struct cast {

    type_t*t;
    node_t*n;
    srcpos_t pos;

} cast;

/* make sure we do the proper casting */
void
eppic_transval(int s1, int s2, value_t *v, int issigned)
{
vu_t u;

    if(s1==s2) return;

    if(issigned) {

        switch(s1) {
            case 1:
                switch(s2) {
                    case 2:
                        u.us=v->v.sc;
                    break;
                    case 4:
                        u.ul=v->v.sc;
                    break;
                    case 8:
                        u.ull=v->v.sc;
                    break;
                }
            break;
            case 2:
                switch(s2) {
                    case 1:
                        u.uc=v->v.ss;
                    break;
                    case 4:
                        u.ul=v->v.ss;
                    break;
                    case 8:
                        u.ull=v->v.ss;
                    break;
                }
            break;
            case 4:
                switch(s2) {
                    case 2:
                        u.us=v->v.sl;
                    break;
                    case 1:
                        u.uc=v->v.sl;
                    break;
                    case 8:
                        u.ull=v->v.sl;
                    break;
                }
            break;
            case 8:
                switch(s2) {
                    case 2:
                        u.us=v->v.sll;
                    break;
                    case 4:
                        u.ul=v->v.sll;
                    break;
                    case 1:
                        u.uc=v->v.sll;
                    break;
                }
            break;
        }

    } else {

        switch(s1) {
            case 1:
                switch(s2) {
                    case 2:
                        u.us=v->v.uc;
                    break;
                    case 4:
                        u.ul=v->v.uc;
                    break;
                    case 8:
                        u.ull=v->v.uc;
                    break;
                }
            break;
            case 2:
                switch(s2) {
                    case 1:
                        u.uc=v->v.us;
                    break;
                    case 4:
                        u.ul=v->v.us;
                    break;
                    case 8:
                        u.ull=v->v.us;
                    break;
                }
            break;
            case 4:
                switch(s2) {
                    case 2:
                        u.us=v->v.ul;
                    break;
                    case 1:
                        u.uc=v->v.ul;
                    break;
                    case 8:
                        u.ull=v->v.ul;
                    break;
                }
            break;
            case 8:
                switch(s2) {
                    case 2:
                        u.us=v->v.ull;
                    break;
                    case 4:
                        u.ul=v->v.ull;
                    break;
                    case 1:
                        u.uc=v->v.ull;
                    break;
                }
            break;
        }
    }
    memmove(&v->v, &u, sizeof(u));
    if(v->type.type!=V_REF) v->type.size=s2;
}

value_t *
eppic_execast(cast *c)
{
/* we execute the expression node_t*/
value_t *v=NODE_EXE(c->n);

    /* ... and validate the type cast */
    if(v->type.type != V_REF && v->type.type != V_BASE) {

        eppic_rerror(&c->pos, "Invalid typecast");

    }
    else {

        int vsize=TYPE_SIZE(&v->type);
        int issigned=eppic_issigned(v->type.typattr);

        /* Now, just copy the cast type over the current type_t*/
        eppic_duptype(&v->type, c->t);

        /* Take into account the size of the two objects */
        eppic_transval(vsize, TYPE_SIZE(c->t), v, issigned);
    }
    return v;
}

void
eppic_freecast(cast *c)
{
    NODE_FREE(c->n);
    eppic_freetype(c->t);
    eppic_free(c);
}

node_t*
eppic_typecast(type_t*type, node_t*expr)
{
    if(type->type==V_STRING) {
        
        eppic_error("Cannot cast to a 'string'");
        return 0;

    } else {

        node_t*n=eppic_newnode();
        cast *c=eppic_alloc(sizeof(cast));

        c->t=type;
        c->n=expr;
        n->exe=(xfct_t)eppic_execast;
        n->free=(ffct_t)eppic_freecast;
        n->data=c;
        eppic_setpos(&c->pos);
        return n;
    }
}

/*
    Validate type conversions on function calls and assignments.
*/
void
eppic_chkandconvert(value_t *vto, value_t *vfrm)
{
type_t*tto=&vto->type;
type_t*tfrm=&vfrm->type;

    if(tto->type == tfrm->type) {

        if(tto->type == V_BASE) {

            int attr=tto->typattr;
            int idx=tto->idx;

            eppic_transval(tfrm->size, tto->size, vfrm, eppic_issigned(vfrm->type.typattr));
            eppic_dupval(vto, vfrm);
            tto->typattr=attr;
            tto->idx=idx;
            return;

        } else if(tto->type == V_REF) {

            if(eppic_isvoid(tto->typattr) || eppic_isvoid(tfrm->typattr)) goto dupit;

            if(tto->ref == tfrm->ref && tto->rtype == tfrm->rtype) {

                if(is_ctype(tto->rtype)) {

                    if(tto->idx == tfrm->idx || eppic_samectypename(tto->rtype, tto->idx, tfrm->idx))
                        goto dupit;

                } else if(tto->size == tfrm->size) {

                    int attr=tto->typattr;
                    eppic_dupval(vto, vfrm);
                    tto->typattr=attr;
                    return;
                }
            }
        }
        /* Allow assignments between enums of the same type */
        else if(is_ctype(tto->type) || tto->type == V_ENUM) {

            /* same structure  type_t*/
            if(tto->idx == tfrm->idx || eppic_samectypename(tto->type, tto->idx, tfrm->idx))
                goto dupit;
        }
        else if(tto->type == V_STRING) goto dupit;

    } 
    else if((tto->type == V_ENUM && tfrm->type == V_BASE) ||
            (tto->type == V_BASE && tfrm->type == V_ENUM)) {
        /* convert type from or to enum */
        int attr=tto->typattr;
        int idx=tto->idx;

        eppic_transval(tfrm->size, tto->size, vfrm, eppic_issigned(vfrm->type.typattr));
        eppic_dupval(vto, vfrm);
        tto->typattr=attr;
        tto->idx=idx;
        return;
    }
        // support NULL assignment to pointer
        else if(tto->type == V_REF && tfrm->type == V_BASE && !eppic_getval(vfrm)) return;
        
    eppic_error("Invalid type conversion");

dupit:
    eppic_dupval(vto, vfrm);
}

