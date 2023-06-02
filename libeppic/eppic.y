%{
/*
 * Copyright 2001 Silicon Graphics, Inc. All rights reserved.
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
// to help resolve type name versus var name ambiguity...
#define VARON  needvar=1;
#define VAROFF needvar=0;
#define PUSHV  eppic_vpush();
#define POPV   eppic_vpop();
static int eppic_toctype(int);
int eppicerror(char *);
%}

%union {
    node_t  *n;
    char    *s;
    int  i;
    type_t  *t;
    dvar_t  *d;
    var_t   *v;
}

%token  <i> STATIC DOBLK WHILE RETURN TDEF EXTERN VARARGS INLINE
%token  <i> CHAR SHORT FLOAT DOUBLE VOID INT UNSIGNED LONG SIGNED VOLATILE REGISTER STRTYPE CONST
%token  <i> BREAK CONTINUE DO FOR FUNC
%token  <i> IF PATTERN BASETYPE
%token  <i> STRUCT ENUM UNION
%token  <i> SWITCH CASE DEFAULT
%token  <n> ELSE CEXPR
%token  <n> VAR NUMBER STRING
%token  <t> TYPEDEF TYPEOF
%token  <i> '(' ')' ',' ';' '{' '}'

%type   <n> termlist term opt_term opt_termlist
%type   <n> stmt stmtlist expstmt stmtgroup
%type   <n> var opt_var c_string
%type   <n> for if while switch case caselist caseconstlist caseconst

%type   <d> dvar dvarlist dvarini

%type   <v> one_var_decl var_decl_list var_decl farglist decl_list

%type   <t> type ctype rctype btype_list tdef typecast
%type   <t> storage_list string type_decl
%type   <t> ctype_decl typeof
%type   <i> btype storage ctype_tok print

%right  <i> ASSIGN ADDME SUBME MULME DIVME MODME ANDME XORME
%right  <i> ORME SHLME SHRME
%right  <i> '?'
%left   <i> IN
%left   <i> BOR
%left   <i> BAND
%left   <i> OR
%left   <i> XOR
%left   <i> AND

%left   <i> EQ NE
%left   <i> GE GT LE LT
%left   <i> SHL SHR
%left   <i> ADD SUB
%left   <i> MUL DIV MOD
%left   <i> PRINT PRINTO PRINTD PRINTX TAKE_ARR
%right  <i> ADROF PTRTO PTR UMINUS SIZEOF TYPECAST POSTINCR PREINCR POSTDECR PREDECR INCR DECR FLIP NOT
%left   <i> ARRAY CALL INDIRECT DIRECT

%%

file:
    /* empty */
    | file fileobj
    ;

fileobj:
    function
    | var_decl ';'          { eppic_file_decl($1); }
    | ctype_decl ';'        { ; }
    ;

function:
    one_var_decl stmtgroup
                    { eppic_newfunc($1, $2); }
    ;


for:
    FOR '(' opt_termlist ';' opt_termlist ';' opt_termlist ')' expstmt
                    { $$ = eppic_newstat(FOR, 4, $3, $5, $7, $9); }
    | FOR '(' var IN term ')' expstmt
                    { $$ = eppic_newstat(IN, 3, $3, $5, $7); }
    ;

if:
    IF '(' {VARON;} term {VAROFF;} ')'      { $$ = $4; }
    ;

switch :
    SWITCH '(' {VARON;} term {VAROFF;} ')' '{' caselist '}'

                    { $$ = eppic_newstat(SWITCH, 2, $4, $8); }
    ;

caselist:
    case
    | caselist case         { $$ = eppic_addcase($1, $2); }
    ;

case :
    caseconstlist stmtlist      { $$ = eppic_newcase($1, $2); }
    ;

caseconst:
    CASE term ':'           { $$ = eppic_caseval(0, $2); }
    | DEFAULT ':'           { $$ = eppic_caseval(1, 0); }
    ;

caseconstlist:
    caseconst
    | caseconstlist caseconst   { $$ = eppic_addcaseval($1, $2); }
    ;

opt_term:
    /* empty */             { $$ = 0; }
    | term
    ;

termlist:
      term
    | termlist ',' term     { $$ = eppic_sibling($1, $3); }
    ;

opt_termlist:
      /* empty */           { $$ = 0; }
    | termlist
    ;

stmt:
      termlist ';'          { $$ = eppic_newstat(PATTERN, 1, $1); }
    | while expstmt         { $$ = eppic_newstat(WHILE, 2, $1, $2); }
    | switch
    | for
    | if expstmt ELSE expstmt   { $$ = eppic_newstat(IF, 3, $1, $2, $4); }
    | if expstmt            { $$ = eppic_newstat(IF, 2, $1, $2); }
    | DO expstmt WHILE '(' term ')' ';'
                    { $$ = eppic_newstat(DO, 2, $2, $5); }
    | RETURN term ';'       { $$ = eppic_newstat(RETURN, 1, $2); }
    | RETURN ';'            { $$ = eppic_newstat(RETURN, 1, NULLNODE); }
    | BREAK ';'             { $$ = eppic_newstat(BREAK, 0); }
    | CONTINUE ';'          { $$ = eppic_newstat(CONTINUE, 0); }
    | ';'                   { $$ = 0; }
    ;

stmtlist:
       /* empty */          { $$ = 0; }
    | stmt
    | stmtgroup
    | stmtlist stmt         { $$ = eppic_addstat($1, $2); }
    | stmtlist stmtgroup    { $$ = eppic_addstat($1, $2); }
    ;

stmtgroup:
    '{' {PUSHV;} decl_list stmtlist {POPV;} '}'   { $$ = eppic_stat_decl($4, $3); }
    | '{' {PUSHV;} stmtlist {POPV;} '}'       { $$ = eppic_stat_decl($3, 0); }
    ;

expstmt:
    stmt
    | stmtgroup
    ;

term:

      term '?' term ':' term %prec '?'
                                { $$ = eppic_newop(CEXPR, 3, $1, $3, $5); }
    | term BOR  term            { $$ = eppic_newop(BOR, 2, $1, $3); }
    | term BAND term            { $$ = eppic_newop(BAND, 2, $1, $3); }
    | NOT term                  { $$ = eppic_newop(NOT, 1, $2); }
    | term ASSIGN   term        { $$ = eppic_newop(ASSIGN, 2, $1, $3); }
    | term EQ   term            { $$ = eppic_newop(EQ, 2, $1, $3); }
    | term GE   term            { $$ = eppic_newop(GE, 2, $1, $3); }
    | term GT   term            { $$ = eppic_newop(GT, 2, $1, $3); }
    | term LE   term            { $$ = eppic_newop(LE, 2, $1, $3); }
    | term LT   term            { $$ = eppic_newop(LT, 2, $1, $3); }
    | term IN   term            { $$ = eppic_newop(IN, 2, $1, $3); }
    | term NE   term            { $$ = eppic_newop(NE, 2, $1, $3); }
    | '(' term ')'              { $$ = $2; }
    | '(' stmtgroup ')'         { $$ = $2; }
    | term ANDME    term        { $$ = eppic_newop(ANDME, 2, $1, $3); }
    | PTR term %prec PTRTO      { $$ = eppic_newptrto($1, $2); }
    | AND term %prec ADROF      { $$ = eppic_newadrof($2); }
    | term OR   term            { $$ = eppic_newop(OR, 2, $1, $3); }
    | term ORME term            { $$ = eppic_newop(ORME, 2, $1, $3); }
    | term XOR  term            { $$ = eppic_newop(XOR, 2, $1, $3); }
    | term XORME    term        { $$ = eppic_newop(XORME, 2, $1, $3); }
    | term SHR  term            { $$ = eppic_newop(SHR, 2, $1, $3); }
    | term SHRME    term        { $$ = eppic_newop(SHRME, 2, $1, $3); }
    | term SHL  term            { $$ = eppic_newop(SHL, 2, $1, $3); }
    | term SHLME    term        { $$ = eppic_newop(SHLME, 2, $1, $3); }
    | term ADDME    term        { $$ = eppic_newop(ADDME, 2, $1, $3); }
    | term SUBME    term        { $$ = eppic_newop(SUBME, 2, $1, $3); }
    | term MULME    term        { $$ = eppic_newop(MULME, 2, $1, $3); }
    | term DIV  term            { $$ = eppic_newop(DIV, 2, $1, $3); }
    | term DIVME    term        { $$ = eppic_newop(DIVME, 2, $1, $3); }
    | term MODME    term        { $$ = eppic_newop(MODME, 2, $1, $3); }
    | term MOD  term            { $$ = eppic_newop(MOD, 2, $1, $3); }
    | term SUB  term            { $$ = eppic_newop(SUB, 2, $1, $3); }
    | term ADD  term            { $$ = eppic_newop(ADD, 2, $1, $3); }
    | term PTR term %prec MUL   { $$ = eppic_newmult($1, $3, $2); }
    | term AND term             { $$ = eppic_newop(AND, 2, $1, $3); }
    | SUB term %prec UMINUS     { $$ = eppic_newop(UMINUS, 1, $2); }
    | '~' term %prec FLIP       { $$ = eppic_newop(FLIP, 1, $2); }
    | ADD term %prec UMINUS     { $$ = $2; }
    | term '(' ')' %prec CALL   { $$ = eppic_newcall($1, NULLNODE); }
    | term '(' termlist ')' %prec CALL  { $$ = eppic_newcall($1, $3); }
    | DECR term                 { $$ = eppic_newop(PREDECR, 1, $2); }
    | INCR term                 { $$ = eppic_newop(PREINCR, 1, $2); }
    | term DECR                 { $$ = eppic_newop(POSTDECR, 1, $1); }
    | term INCR                 { $$ = eppic_newop(POSTINCR, 1, $1); }
    | term INDIRECT var         { $$ = eppic_newmem(INDIRECT, $1, $3); }
    | term INDIRECT tdef        { $$ = eppic_newmem(INDIRECT, $1, eppic_tdeftovar($3)); } // resolve ambiguity
    | term DIRECT var           { $$ = eppic_newmem(DIRECT, $1, $3); }
    | term DIRECT tdef          { $$ = eppic_newmem(DIRECT, $1, eppic_tdeftovar($3)); } // resolve ambiguity
    | term  '[' term ']' %prec ARRAY    
                                { $$ = eppic_newindex($1, $3); }
    | NUMBER
    | c_string
    | typecast term %prec TYPECAST  { $$ = eppic_typecast($1, $2); }
    | SIZEOF '(' var_decl ')'
                    { $$ = eppic_sizeof(eppic_newcast($3), 1); }
    | SIZEOF term           { $$ = eppic_sizeof($2, 2); }
    | print '(' var_decl ')' %prec SIZEOF   
                    { $$ = eppic_newptype($3); }
    | print term %prec SIZEOF   { $$ = eppic_newpval($2, $1); }
    | TAKE_ARR '(' term ',' term ')' { $$ = $3; /* eppic_newtakearr($3, $5); */ }
    | var
    ;

print:
    PRINT
    | PRINTX
    | PRINTO
    | PRINTD
    ;

typecast:
        '(' var_decl ')'        { $$ = eppic_newcast($2); }
    ;

var_decl_list:
    var_decl ';'
    | var_decl_list var_decl ';'    { eppic_addnewsvs($1, $1, $2); $$=$1; }
    ;

decl_list:
    ctype_decl ';'              { $$ = 0; }
    | var_decl ';'              { $$ = $1; }
    | decl_list var_decl ';'    { $$=$1; if($1 && $2) eppic_addnewsvs($1, $1, $2); }
    | decl_list ctype_decl ';'  { $$ = $1; }
    ;


var_decl:
    type_decl dvarlist  { needvar=0; $$ = eppic_vardecl($2, $1); }
    ;

one_var_decl:
    type_decl dvar      { needvar=0; $$ = eppic_vardecl($2, $1); }
    ;

type_decl:
    type                    { $$=$1; needvar++; }
    | storage_list          { $$=$1; needvar++; }
    | type storage_list     { $$=eppic_addstorage($1, $2); needvar++; }
    | storage_list type     { $$=eppic_addstorage($2, $1); needvar++; }
    | type_decl PTR         { $$=$1; eppic_pushref($1, $2);; needvar++; }
    | type_decl storage_list    { $$=eppic_addstorage($1, $2); needvar++; }
    ;

type:
    ctype
    | typeof
    | tdef
    | btype_list
    | string
    | ctype_decl
    ;

typeof:
    TYPEOF '(' type ')'     { $$=$3; needvar++; }
    | TYPEOF '(' term ')'   { $$=eppic_typeof($3); needvar++; }
    ;

ctype_decl:
    ctype_tok var '{' {eppic_startctype(eppic_toctype($1),$2);instruct++;} var_decl_list '}'
                    { instruct--; $$ = eppic_ctype_decl(eppic_toctype($1), $2, $5); }
    | ctype_tok tdef '{' {eppic_startctype(eppic_toctype($1),lastv=eppic_tdeftovar($2));instruct++;} var_decl_list '}'
                    { instruct--; $$ = eppic_ctype_decl(eppic_toctype($1), lastv, $5); }
    | ctype_tok var '{' dvarlist '}'
                    { $$ = eppic_enum_decl(eppic_toctype($1), $2, $4); }
    | ctype_tok tdef '{' dvarlist '}'
                    { $$ = eppic_enum_decl(eppic_toctype($1), eppic_tdeftovar($2), $4); }
    ;

ctype:
    rctype              { $$ = $1; }
    | ctype_tok '{' {instruct++;} var_decl_list '}'
                    {  instruct--; $$ = eppic_ctype_decl(eppic_toctype($1), 0, $4); }
    | ctype_tok '{' dvarlist '}'
                    {  $$ = eppic_enum_decl(eppic_toctype($1), 0, $3); }
    ;

farglist:
    /* empty */             { $$ = 0; }
    | one_var_decl          { $$ = $1; }
    | farglist ',' one_var_decl { 
                        if(!$1) eppic_error("Syntax error"); 
                        if($3) eppic_addnewsvs($1, $1, $3); $$=$1; 
                            }
    | farglist ',' VARARGS      { 
                        if(!$1) eppic_error("Syntax error"); 
                        eppic_addtolist($1, eppic_newvar(S_VARARG)); $$=$1; 
                    }
    ;
    

string:
    STRTYPE         { 
                        type_t *t;
                        t=eppic_newtype(); 
                        t->type=V_STRING;
                        t->typattr=0;
                        $$ = t;
                    }
    ;

rctype:
    ctype_tok var           { $$ = eppic_newctype(eppic_toctype($1), $2); }
    | ctype_tok tdef        { $$ = eppic_newctype(eppic_toctype($1), eppic_tdeftovar($2)); }
    ;

ctype_tok:
    STRUCT
    | ENUM
    | UNION
    ;

btype_list:
    btype                   { $$ = eppic_newbtype($1); }
    | btype_list btype      { $$ = eppic_addbtype($1, $2); }
    ;

c_string:
    STRING                  { $$ = $1; }
    | c_string  STRING      { $$ = eppic_strconcat($1, $2); }
    ;

btype:
    LONG
    | CHAR
    | INT
    | SHORT
    | UNSIGNED
    | SIGNED
    | DOUBLE
    | FLOAT
    | VOID
    ;

storage_list:
    storage                 { $$ = eppic_newbtype($1); }
    | storage_list storage  { eppic_error("Only one storage class can be speficied"); }
    ;

storage:
    STATIC
    | VOLATILE
    | REGISTER
    | TDEF
    | EXTERN
    | CONST
    ;

dvarlist:
    dvarini                 { $$ = $1; }
    | dvarlist ',' dvarini  { $$ = eppic_linkdvar($1, $3); }
    ;

dvarini:
    dvar                    { $$ = $1; }
    | dvar ASSIGN  term     { $$ = eppic_dvarini($1, $3); }
    ;

dvar:
    opt_var                 { $$ = eppic_newdvar($1); needvar=0; }
    | ':' term              { $$ = eppic_dvarfld(eppic_newdvar(0), $2); }
    | dvar ':' term         { $$ = eppic_dvarfld($1, $3); }
    | dvar '[' opt_term ']' { $$ = eppic_dvaridx($1, $3); }
    | PTR dvar              { $$ = eppic_dvarptr($1, $2); }
    | dvar '(' ')'          { $$ = eppic_dvarfct($1, 0); }
    | dvar '(' farglist ')' { $$ = eppic_dvarfct($1, $3); }
    | '(' dvar ')'          { $$ = $2; }
    ;

opt_var:
    /* empty */             { $$ = 0; }
    | var                   { $$ = $1; }
    ;

var:
    VAR                     { $$ = $1; }
    ;   

tdef:
    TYPEDEF                 { $$ = $1; }
    ;

while:
    WHILE '(' {VARON;} term {VAROFF;} ')'  { $$ = $4; }
    ;

%%

static int
eppic_toctype(int tok)
{
    switch(tok) {
    case STRUCT: return V_STRUCT;
    case ENUM: return V_ENUM;
    case UNION: return V_UNION;
    default: eppic_error("Oops eppic_toctype!"); return 0;
    }
}

/*
    This file gets included into the yacc specs.
    So the "eppic.h" is already included 
*/

int eppicerror(char *p) { eppic_error(p); return 0; }

