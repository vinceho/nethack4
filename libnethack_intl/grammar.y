%{ /* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/*	Copyright (c) Alex Smith 2012 			          */
/* NetHack may be freely redistributed.  See license for details. */

#include "lang.h"

#define YYDEBUG 1

#define mu(c1,c2,c3,role,rule) makeunit(c1,c2,c3,gr_##role,rule)

    static struct grammarunit *makeunit(
        struct grammarunit *c1, struct grammarunit *c2, struct grammarunit *c3,
        enum grammarrole role, enum grammarrule rule);

    static char *rv;
    static char errreason[512];
    static int first_column, last_column;
%}

%union
{
    struct grammarunit *u;
    char *s;              /* always malloc'ed */
    int i;
}

%{
    /* yyerror, yylex must come after the definitions of YYSTYPE, YYLTYPE */
    static void gyyerror(YYLTYPE *, boolean, boolean, const char *);
    extern int gyylex(YYSTYPE *, YYLTYPE *);
    static int gyyparse(boolean simple, boolean caps);
%}

%token N P V A D Q E J C UNKNOWN END COMMA EOFTOKEN INVALIDCHAR AT
%token FCOMMA MCOMMA NCOMMA ICOMMA PCOMMA CCOMMA OCOMMA LCOMMA SCOMMA TCOMMA
%token ACOMMA DCOMMA ECOMMA QCOMMA MINUSCOMMA PLUSCOMMA STARCOMMA
%token PERCENT_S PC_COMMA
%token NEQUALS AEQUALS VEQUALS SEQUALS DEQUALS EEQUALS JEQUALS CEQUALS
%token <s> LITERAL PUNCTUATION UNIQUIFIER S
%token <i> COUNTCOMMA

%destructor { free($$); } <s>
%destructor { free_grammarunit($$); } <u>

%type <s> substel substels
%type <u> nounish verbish adjectivish adverbish relativish prepositionish
%type <u> conjunctionish clausish clausish_noat sentenceish anything
%type <u> literalinner literalinner2

%token-table
%error-verbose
%verbose
%locations
%define api.pure
%start string
%parse-param {boolean simple}
%parse-param {boolean caps}
%name-prefix "gyy"

%%

string:
  EOFTOKEN                               { rv = malloc(1); *rv = 0; YYACCEPT; }
| substels EOFTOKEN                      { rv = $1; YYACCEPT; }
;

substels:                           
  substel                                { $$ = $1; }
| substel substels                       {
      $1 = realloc($1, strlen($1) + strlen($2) + 1);
      strcat($1, $2);
      free($2);
      $$ = $1;
  }
;

substel:
  anything                               {
      forcecontent($1, simple, caps);
      $$ = $1->content;
      free_grammarunit($1->children[0]);
      free_grammarunit($1->children[1]);
      free_grammarunit($1->children[2]);
      if ($1->punctuation) {
          $$ = realloc($$, strlen($$)+strlen($1->punctuation)+1);
          strcat($$, $1->punctuation);
          free($1->punctuation);
      }
      free($1->uniquifier);
      free($1);      
  }
| LITERAL                                { $$ = $1; }
| PUNCTUATION                            { $$ = $1; }
| PERCENT_S                              {
  $$ = malloc(3);
  strcpy($$, "%s");
}
;

anything:
  nounish                                { $$ = $1; }
| verbish                                { $$ = $1; }
| adjectivish                            { $$ = $1; }
| adverbish                              { $$ = $1; }
| relativish                             { $$ = $1; }
| prepositionish                         { $$ = $1; }
| sentenceish                            { $$ = $1; }
| clausish_noat PUNCTUATION              { $$ = $1; $$->punctuation = $2; }
| clausish_noat AT                       {
      $$ = $1;
      $$->punctuation = calloc(1, 1);
  }
;

literalinner2:
  LITERAL END                            {
      $$ = malloc(sizeof(struct grammarunit));
      $$->rule = gr_literal;
      $$->quan = 1;
      $$->uniquifier = 0;
      $$->punctuation = 0;
      $$->gender = gg_unknown;
      $$->content = $1;
      $$->tagged = FALSE;
      $$->children[0] = 0;
      $$->children[1] = 0;
      $$->children[2] = 0;
  }
;

literalinner:
  literalinner2               { $$ = $1; }
| UNIQUIFIER literalinner2    { $$ = $2; $$->uniquifier = $1;    }
| PC_COMMA literalinner       { $$ = $2; }
| FCOMMA literalinner         { $$ = $2; $$->gender = gg_female; }
| MCOMMA literalinner         { $$ = $2; $$->gender = gg_male;   }
| NCOMMA literalinner         { $$ = $2; $$->gender = gg_neuter; }
| ICOMMA literalinner         { $$ = $2; $$->quan |= Q_INDEFINITE;  }
| OCOMMA literalinner         { $$ = $2; $$->quan |= Q_ZEROARTICLE; }
;

/* TODO: Allow empty children. */
verbish:
  V literalinner                         { $$ = $2; $$->role = gr_verb;   }
| VEQUALS verbish                        { $$ = $2; }
| V verbish COMMA nounish END            { $$=mu($2,$4, 0,verb,verb_VN ); }
| V verbish COMMA adjectivish END        { $$=mu($2,$4, 0,verb,verb_VA ); }
| V verbish COMMA adverbish END          { $$=mu($2,$4, 0,verb,verb_VD ); }
| V verbish COMMA verbish END            { $$=mu($2,$4, 0,verb,verb_VV ); }
| V verbish COMMA clausish END           { $$=mu($2,$4, 0,verb,verb_VC ); }
| V MCOMMA verbish COMMA anything END    { $$=mu($3,$5, 0,verb,verb_mVX); }
| V SCOMMA verbish COMMA verbish END     { $$=mu($3,$5, 0,verb,verb_sVV); }
| V MINUSCOMMA verbish END               { $$=mu($3, 0, 0,verb,minus_V ); }
| V PLUSCOMMA verbish COMMA verbish END  { $$=mu($3,$5, 0,verb,plus_VV ); }
| V conjunctionish COMMA verbish COMMA
  verbish END                            { $$=mu($2,$4,$6,verb,verb_JVV); }
| VEQUALS PERCENT_S                      {
      $$ = mu(0, 0, 0, verb, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

nounish:
  N literalinner                         { $$ = $2; $$->role = gr_noun; }
| P literalinner                         { $$ = $2; $$->role = gr_propernoun; }
| NEQUALS nounish                        { $$ = $2; }
| UNKNOWN                                { $$=mu( 0, 0, 0,noun,gr_unknown); }
| N STARCOMMA nounish END                { $$ = $3; $$->quan |= Q_PLURAL; }
| N COUNTCOMMA nounish END               {
      $$ = $3;
      $$->quan = ($$->quan & ~(QM_DEFINITE)) | Q_EXPLICIT | $2;
      if ($2 != 1) $$->quan |= Q_PLURAL;
  }
| N nounish COMMA nounish END            { $$=mu($2,$4, 0,noun,noun_NN); }
| N nounish COMMA adjectivish END        { $$=mu($2,$4, 0,noun,noun_NA); }
| N nounish COMMA clausish END           { $$=mu($2,$4, 0,noun,noun_NC); }
| N nounish COMMA adverbish END          { $$=mu($2,$4, 0,noun,noun_ND ); }
| N clausish END                         { $$=mu($2, 0, 0,noun,noun_C  ); }
| N CCOMMA nounish COMMA verbish END     { $$=mu($3,$5, 0,noun,noun_cNV); }
| N PCOMMA nounish COMMA verbish END     { $$=mu($3,$5, 0,noun,noun_pNV); }
| N FCOMMA nounish COMMA adjectivish END { $$=mu($3,$5, 0,noun,noun_fNA); }
| N FCOMMA nounish COMMA nounish END     { $$=mu($3,$5, 0,noun,noun_fNN); }
| N OCOMMA nounish COMMA nounish END     { $$=mu($3,$5, 0,noun,noun_oNN); }
| N LCOMMA nounish COMMA nounish END     { $$=mu($3,$5, 0,noun,noun_lNN); }
| N QCOMMA nounish COMMA nounish END     { $$=mu($3,$5, 0,noun,noun_qNN); }
| N MCOMMA anything END                  { $$=mu($3, 0, 0,noun,noun_mX); }
| N nounish COMMA verbish END            { $$=mu($2,$4, 0,noun,noun_NV); }
| N ACOMMA nounish COMMA verbish END     { $$=mu($3,$5, 0,noun,noun_aNV); }
| N SCOMMA clausish END                  { $$=mu($3, 0, 0,noun,noun_sC); }
| N MINUSCOMMA nounish END               { $$=mu($3, 0, 0,noun,minus_N); }
| N PLUSCOMMA nounish COMMA nounish END  { $$=mu($3,$5, 0,noun,plus_NN); }
| N SCOMMA FCOMMA nounish COMMA adjectivish END {
      $$ = mu($4, $6, 0,noun,noun_fNA);
      $$->quan = 1;
  }
| NEQUALS PERCENT_S                      {
      $$ = mu(0, 0, 0, noun, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

adverbish:
  D literalinner                            { $$ = $2; $$->role = gr_adverb; }
| DEQUALS adverbish                         { $$ = $2; }
| D adverbish COMMA adverbish END           { $$=mu($2,$4,0,adverb,adverb_DD); }
| D TCOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_tN); }
| D LCOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_lN); }
| D ACOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_aN); }
| D OCOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_oN); }
| D DCOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_dN); }
| D MCOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_mN); }
| D ECOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_eN); }
| D ECOMMA prepositionish COMMA nounish END { $$=mu($3,$5,0,adverb,adverb_eEN);}
| D ICOMMA nounish END                      { $$=mu($3, 0,0,adverb,adverb_iN); }
| D PCOMMA verbish END                      { $$=mu($3, 0,0,adverb,adverb_pV); }
| D relativish COMMA clausish END           { $$=mu($2,$4,0,adverb,adverb_QC); }
| D prepositionish COMMA nounish END        { $$=mu($2,$4,0,adverb,adverb_EN); }
| D prepositionish COMMA adverbish END      { $$=mu($2,$4,0,adverb,adverb_ED); }
| D MINUSCOMMA adverbish END                { $$=mu($3, 0,0,adverb,minus_D);   }
| D PLUSCOMMA adverbish COMMA adverbish END { $$=mu($3,$5,0,adverb,plus_DD);   }
| DEQUALS PERCENT_S                         {
      $$ = mu(0, 0, 0, adverb, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

adjectivish:
  A literalinner                     { $$ = $2; $$->role = gr_adjective; }
| AEQUALS adjectivish                { $$ = $2; }
| A CCOMMA adjectivish END           { $$=mu($3, 0,0,adjective,adjective_cA); }
| A SCOMMA adjectivish END           { $$=mu($3, 0,0,adjective,adjective_sA); }
| A PCOMMA adjectivish END           { $$=mu($3, 0,0,adjective,adjective_pA); }
| A OCOMMA adjectivish END           { $$=mu($3, 0,0,adjective,adjective_oA); }
| A ACOMMA nounish END               { $$=mu($3, 0,0,adjective,adjective_aN); }
| A LCOMMA nounish END               { $$=mu($3, 0,0,adjective,adjective_lN); }
| A MCOMMA nounish END               { $$=mu($3, 0,0,adjective,adjective_mN); }
| A verbish END                      { $$=mu($2, 0,0,adjective,adjective_V ); }
| A nounish END                      { $$=mu($2, 0,0,adjective,adjective_N ); }
| A adjectivish COMMA adverbish END  { $$=mu($2,$4,0,adjective,adjective_AD); }
| A adjectivish COMMA verbish END    { $$=mu($2,$4,0,adjective,adjective_AV); }
| A relativish COMMA clausish END    { $$=mu($2,$4,0,adjective,adjective_QC); }
| A prepositionish COMMA nounish END { $$=mu($2,$4,0,adjective,adjective_EN); }
| A MINUSCOMMA adjectivish END       { $$=mu($3, 0,0,adjective,minus_A); }
| A PLUSCOMMA adjectivish COMMA adjectivish END {
      $$ = mu($3, $5, 0, adjective, plus_AA);
  }
| AEQUALS PERCENT_S                 {
      $$ = mu(0, 0, 0, adjective, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

prepositionish:
  E literalinner                        { $$ = $2; $$->role = gr_preposition; }
| EEQUALS prepositionish                { $$ = $2; }
| E MINUSCOMMA prepositionish END       { $$=mu($3,0,0,preposition,minus_E); }
| E PLUSCOMMA prepositionish COMMA prepositionish END  {
      $$=mu($3,$5,0,preposition,plus_EE);
  }
| EEQUALS PERCENT_S                 {
      $$ = mu(0, 0, 0, preposition, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

relativish:
  Q literalinner                          { $$ = $2; $$->role = gr_relative; }
| Q MINUSCOMMA relativish END             { $$=mu($3,0,0,relative,minus_Q); }
| Q PLUSCOMMA relativish COMMA relativish END  {
      $$=mu($3,$5,0,relative,plus_QQ);
  }
;

conjunctionish:
  J literalinner                          {
    $$ = $2;
    $$->role = gr_conjunction;
  }
| JEQUALS conjunctionish                  { $$ = $2; }
| JEQUALS PERCENT_S                       {
  $$ = mu(0, 0, 0, conjunction, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

clausish:
  clausish_noat                           { $$ = $1; }
| clausish AT                             { $$ = $1; }
;

clausish_noat:
/* Literal clauses would seem inadvisable compared to literal sentences, but
   just in case we ever need one... */
  C literalinner                          { $$ = $2; $$->role = gr_clause; }
| CEQUALS clausish_noat                   { $$ = $2; }
| C nounish COMMA verbish END             { $$=mu($2,$4,0,clause,clause_NV ); }
| C clausish COMMA adverbish END          { $$=mu($2,$4,0,clause,clause_CD ); }
| C conjunctionish COMMA clausish END     { $$=mu($2,$4,0,clause,clause_JC ); }
| C ICOMMA nounish COMMA verbish END      { $$=mu($3,$5,0,clause,clause_iNV); }
| C PCOMMA nounish COMMA verbish END      { $$=mu($3,$5,0,clause,clause_pNV); }
| C CCOMMA nounish COMMA verbish END      { $$=mu($3,$5,0,clause,clause_cNV); }
| C FCOMMA nounish COMMA verbish END      { $$=mu($3,$5,0,clause,clause_fNV); }
| C SCOMMA verbish END                    { $$=mu($3, 0,0,clause,clause_sV ); }
| C ICOMMA SCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_isV); }
| C PCOMMA SCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_psV); }
| C CCOMMA SCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_csV); }
| C FCOMMA SCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_fsV); }
| C SCOMMA ICOMMA verbish END             { $$=mu($4, 0,0,clause,clause_isV); }
| C SCOMMA PCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_psV); }
| C SCOMMA CCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_csV); }
| C SCOMMA FCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_fsV); }
| C NCOMMA CCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_cnV); }
| C NCOMMA PCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_pnV); }
| C CCOMMA NCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_cnV); }
| C PCOMMA NCOMMA verbish END             { $$=mu($4, 0,0,clause,clause_pnV); }
| C QCOMMA clausish END                   { $$=mu($3, 0,0,clause,clause_qC ); }
| C ICOMMA verbish END                    { $$=mu($3, 0,0,clause,clause_iV ); }
| C MINUSCOMMA clausish END               { $$=mu($3, 0,0,clause,minus_C); }
| C PLUSCOMMA clausish COMMA clausish END { $$=mu($3,$5,0,clause,plus_CC); }
| CEQUALS PERCENT_S                       {
        $$ = mu(0, 0, 0, clause, gr_literal);
        $$->content = malloc(3);
        strcpy($$->content, "%s");
  }
;

sentenceish:
/* Literal sentences are special, because we don't parse inside them or try to
   modify them in any way. */
  S                                     {
      $$ = malloc(sizeof(struct grammarunit));
      $$->role = gr_sentence;
      $$->rule = gr_literal;
      $$->quan = 1;
      $$->uniquifier = 0;
      $$->punctuation = 0;
      $$->gender = gg_unknown;
      $$->tagged = FALSE;
      $$->content = $1;
      $$->children[0] = 0;
      $$->children[1] = 0;
      $$->children[2] = 0;
  }
| SEQUALS sentenceish                   { $$ = $2; }
| SEQUALS PERCENT_S                 {
      $$ = mu(0, 0, 0, sentence, gr_literal);
      $$->content = malloc(3);
      strcpy($$->content, "%s");
  }
;

%%

void
free_grammarunit(struct grammarunit *u)
{
    if (!u) return;
    free_grammarunit(u->children[0]);
    free_grammarunit(u->children[1]);
    free_grammarunit(u->children[2]);
    free(u->punctuation);
    free(u->uniquifier);
    free(u->content);
    free(u);
}

static struct grammarunit *
makeunit(struct grammarunit *c1, struct grammarunit *c2,
         struct grammarunit *c3, enum grammarrole role,
         enum grammarrule rule)
{ 
    struct grammarunit *u = malloc(sizeof(struct grammarunit));
    u->role = role;
    u->rule = rule;
    u->children[0] = c1;
    u->children[1] = c2;
    u->children[2] = c3;
    /* We copy the gender and quanitity of the first child (which must
       not be NULL). We don't copy the uniquifier; that would lead to
       bizarre results where pronouns replaced the wrong part of a
       sentence. (Conceptually, we'd create a new uniquifier from the
       uniquifiers of the children, but that's overkill.)
       TODO: Make this less magical. */
    u->uniquifier = 0;
    u->punctuation = 0;
    u->content = 0;
    u->quan = c1 ? c1->quan : 1;
    u->gender = c1 ? c1->gender : gg_unknown;
    u->tagged = FALSE;
    /* "the singular object and the other singular object" is plural */
    if (u->rule == plus_NN) u->quan = 1<<29;
    return u;
}

char *
malloc_parsestring(const char *x, boolean simple, boolean caps)
{
    strcpy(errreason, "unknown error");
    first_column = 1;
    last_column = strlen(errreason) + 1;
    if (scan_string_with_callback(x, gyyparse, simple, caps) != 0) {
        rv = malloc(strlen(x) + strlen(errreason) + 100);
        strcpy(rv, "(ERROR: could not parse string '");
        strncat(rv, x, first_column - 1);
        strcat(rv, ">>");
        strncat(rv, x + first_column - 1, last_column - first_column + 1);
        strcat(rv, "<<");
        /* Only output the rest if it exists. */
        if (*(x + last_column - 1) != '\0') {
          strcat(rv, x + last_column);
        } 
        strcat(rv, "': ");
        strcat(rv, errreason);
        strcat(rv, ")");
    }
    return rv;
}

static void
gyyerror(YYLTYPE *locp, boolean simple, boolean caps, const char *x)
{
    (void) simple;
    (void) caps;
    strcpy(errreason, x);
    first_column = locp->first_column;
    last_column = locp->last_column;
}
