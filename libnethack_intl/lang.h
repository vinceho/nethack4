/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/*	Copyright (c) Alex Smith 2012 			          */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef LANG_H
#define LANG_H

# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>

/* Standard definitions matching those in the rest of the project */

# if !defined(STATIC_BUILD)
#  if defined (_MSC_VER)
#   define EXPORT __declspec(dllexport)
#  else
    /* gcc & clang with -fvisibility=hidden need this for exported syms */
#   define EXPORT __attribute__((__visibility__("default")))
#  endif
# else
#  define EXPORT
# endif

typedef signed char boolean; /* 0 or 1 */
# ifndef TRUE   /* defined in some systems' native include files */
#  define FALSE ((boolean)0)
#  define TRUE  ((boolean)!0)
# endif

# define BUFSZ 512




enum grammarrole { gr_noun, gr_propernoun, gr_verb, gr_adjective, gr_adverb,
                   gr_relative, gr_preposition, gr_clause, gr_sentence };
enum grammarrule {
    minus_N, minus_V, minus_A, minus_D, minus_Q, minus_E, minus_C,
    plus_NN, plus_VV, plus_AA, plus_DD, plus_QQ, plus_EE, plus_CC,
    verb_VN, verb_VA, verb_VD, verb_VV, verb_sVV,
    noun_NA, noun_NEN, noun_fNA, noun_oNN, noun_lNN, noun_mX, noun_NV,
    noun_aNV, noun_qNN, noun_fNN, noun_sC,
    adverb_DD, adverb_tN, adverb_lN, adverb_aN, adverb_dN, adverb_mN,
    adverb_eN, adverb_eEN, adverb_oN, adverb_iN, adverb_QC, adverb_EN,
    adverb_pV,
    adjective_cA, adjective_sA, adjective_AD, adjective_AV, adjective_QC,
    adjective_EN, adjective_lN, adjective_aN, adjective_V, adjective_VN,
    adjective_N, adjective_pA, adjective_mN, adjective_oA,
    clause_NV, clause_iNV, clause_pNV, clause_cNV, clause_qC, clause_iV,
    clause_sV, clause_isV, clause_psV, clause_csV,
    gr_literal, gr_unknown,
};
enum grammargender { gg_male, gg_female, gg_neuter, gg_unknown };

struct grammarunit {
    char *uniquifier; /* often but not always null or a number */
    char *content;
    struct grammarunit *children[3];
    enum grammarrole role;
    enum grammarrule rule;
    enum grammargender gender;
    int quan; /* quantity; 1 for unknown, 1 << 29 for plural,
                 1 << 28 for %d, 1 << 30 bit set if indefinite,
                 1 << 26 bit for explicit count;
                 we set 1 << 30 and 1 << 27 for no article */
    boolean tagged; /* used internally by lang-?? to reorder sentences,
                       everything else should maintain it at FALSE */
};

extern void forcecontent(struct grammarunit *, boolean, boolean);
extern void free_grammarunit(struct grammarunit *);
extern char * malloc_parsestring(const char *, boolean, boolean) EXPORT;
extern int scan_string_with_callback(const char *, int (*)(boolean, boolean),
                                     boolean, boolean);

#endif
