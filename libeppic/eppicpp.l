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

#define YY_NO_UNPUT
#define YY_NO_INPUT
%}

%{
#include <string.h>

#ifdef linux
#define YY_INPUT(buf,result,max_size) \
{ \
    int c = eppic_input(); \
    result = (c == EOF) ? YY_NULL : (buf[0] = c, 1); \
}
#endif

#include    "eppic.h"
#include    "eppicpp.tab.h"
#if linux
#define yylval eppicpplval
#endif

#define retok(t) return(t)
int nomacs=0;
extern int eppic_chkmacvar(char *);
extern node_t *eppic_newchar(void);
%}

ABC     [a-zA-Z_]
ABCN        [a-zA-Z0-9_]
N       [0-9]
X       [0-9a-fA-F]

%%

[ \t\n]+    { ; }

"defined"   { retok(P_DEFINED); }
"&&"        { retok(P_BAND); }
"||"        { retok(P_BOR); }
"<"         { retok(P_LT); }
"<="        { retok(P_LE); }
"=="        { retok(P_EQ); }
">="        { retok(P_GE); }
">"         { retok(P_GT); }
"!="        { retok(P_NE); }
"|"         { retok(P_OR); }
"!"         { retok(P_NOT); }
"^"         { retok(P_XOR); }
">>"        { retok(P_SHR); }
"<<"        { retok(P_SHL); }
"+"         { retok(P_ADD); }
"-"         { retok(P_SUB); }
"/"         { retok(P_DIV); }
"%"         { retok(P_MOD); }
"*"         { retok(P_MUL); }

(("0x"+){X}+[lL]*|{N}+[lL]*)    { yylval.n = eppic_newnum(yytext); retok(P_NUMBER); }

{ABC}{ABCN}*    { 
                if(strlen(yytext) > MAX_SYMNAMELEN) {

                    eppic_error("Symbol name too long");
                }
                if(nomacs || !eppic_chkmacvar(yytext)) {

                    yylval.n = eppic_newvnode(yytext); 
                    retok(P_VAR);
                }
        }

\'.\'       { yylval.n = eppic_makenum(B_SC, yytext[1]); retok(P_NUMBER); }
\'\\.\'     { yylval.n = eppic_makenum(B_SC, eppic_getseq(yytext[2])); retok(P_NUMBER); }


.       { retok(yylval.i = yytext[0]); }

%%
#undef input
#undef unput
#define input()       eppic_input()
#define unput(c)      eppic_unput(c)
