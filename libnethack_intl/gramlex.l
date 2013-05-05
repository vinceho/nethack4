%{ /* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/*	Copyright (c) Alex Smith 2012 			          */
/* NetHack may be freely redistributed.  See license for details. */

#include "lang.h"
#include "grammar.h"

#define YY_USER_INIT colcount = 1;
#define YY_USER_ACTION {                        \
        yylloc->first_column = colcount;        \
        colcount += yyleng;                     \
        yylloc->last_column = colcount-1;       \
    }

    int colcount;

%}

%option noyywrap nounput noinput
%option bison-bridge bison-locations
%option prefix="gyy"

UI [a-zA-Z0-9_%]+
LIT [^{},|=%*+\x1c?!.@]
L2  [^{},=\x1c]

%%

"N{"          return N;
"P{"          return P;
"V{"          return V;
"A{"          return A;
"D{"          return D;
"Q{"          return Q;
"E{"          return E;
"J{"          return J;
"C{"          return C;
"}"           return END;
"N="          return NEQUALS;
"A="          return AEQUALS;
"V="          return VEQUALS;
"S="          return SEQUALS;
"D="          return DEQUALS;
"E="          return EEQUALS;
"C="          return CEQUALS;
"%"+"s"       return PERCENT_S;
"%c,"         return PC_COMMA;
"S{"[^}]*"}"  { yylval->s = malloc(yyleng-2); yytext[yyleng-1] = 0;
                strcpy(yylval->s, yytext+2); return S; }
","           return COMMA;
"f,"          return FCOMMA;
"m,"          return MCOMMA;
"n,"          return NCOMMA;
"i,"          return ICOMMA;
"p,"          return PCOMMA;
"c,"          return CCOMMA;
"o,"          return OCOMMA;
"l,"          return LCOMMA;
"s,"          return SCOMMA;
"t,"          return TCOMMA;
"a,"          return ACOMMA;
"d,"          return DCOMMA;
"e,"          return ECOMMA;
"q,"          return QCOMMA;
"-,"          return MINUSCOMMA;
"+,"          return PLUSCOMMA;
"*,"          return STARCOMMA;
"?{}"         return UNKNOWN;
[0-9]+","     yylval->i = atoi(yytext); return COUNTCOMMA;
"%d,"         yylval->i = (1 << 28); return COUNTCOMMA;
"%ld,"        yylval->i = (1 << 28); return COUNTCOMMA;
{UI}"|"       { yylval->s = malloc(yyleng); yytext[yyleng-1] = 0;
                strcpy(yylval->s, yytext); return UNIQUIFIER; }
"@"           return AT;
"?" |
"!" |
"." |
"%c" |
"..."         { yylval->s = malloc(yyleng+1); strcpy(yylval->s, yytext);
                return PUNCTUATION; }
{LIT}{L2}*/[^{=]  { yylval->s = malloc(yyleng+1); strcpy(yylval->s, yytext);
                      return LITERAL; }
"%d" |
\x1c[^\x1c]+\x1c  { yylval->s = malloc(yyleng+1); strcpy(yylval->s, yytext);
                return LITERAL; }
\x01          return EOFTOKEN;
.             return INVALIDCHAR;

%%

int
scan_string_with_callback(const char *x, int (*f)(boolean, boolean),
                          boolean simple, boolean caps)
{
    YY_BUFFER_STATE scanbuf;
    char copybuf[512];
    sprintf(copybuf, "%s\x01", x);
    scanbuf = yy_scan_string(copybuf);
    colcount = 1;
    int i = f(simple, caps);
    yy_delete_buffer(scanbuf);
    return i;
}
