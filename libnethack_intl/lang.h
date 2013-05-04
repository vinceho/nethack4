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
                   gr_relative, gr_preposition, gr_conjunction, gr_clause,
                   gr_sentence };
enum grammarrule {
    minus_N, minus_V, minus_A, minus_D, minus_Q, minus_E, minus_C,
    plus_NN, plus_VV, plus_AA, plus_DD, plus_QQ, plus_EE, plus_CC,
    verb_VN, verb_VA, verb_VD, verb_VV, verb_VC, verb_sVV, verb_mVX, verb_JVV,
    noun_NA, noun_NEN, noun_C, noun_fNA, noun_oNN, noun_lNN, noun_mX, noun_NC,
    noun_NV, noun_aNV, noun_qNN, noun_fNN, noun_sC, noun_cNV, noun_pNV, noun_NN,
    noun_ND,
    adverb_DD, adverb_tN, adverb_lN, adverb_aN, adverb_dN, adverb_mN, adverb_eN,
    adverb_eEN, adverb_oN, adverb_iN, adverb_QC, adverb_EN, adverb_ED,
    adverb_pV,
    adjective_cA, adjective_sA, adjective_AD, adjective_AV, adjective_QC,
    adjective_EN, adjective_lN, adjective_aN, adjective_V, adjective_N,
    adjective_pA, adjective_mN, adjective_oA,
    clause_NV, clause_iNV, clause_pNV, clause_cNV, clause_fNV, clause_qC,
    clause_sV, clause_isV, clause_psV, clause_csV, clause_fsV, clause_iV,
    clause_CD, clause_JC, clause_pnV, clause_cnV,
    gr_literal, gr_unknown,
};
enum grammargender { gg_male, gg_female, gg_neuter, gg_unknown };

struct grammarunit {
    char *uniquifier; /* often but not always null or a number */
    char *content;
    char *punctuation; /* must be present on a clause used as an anything,
                          irrelevant elsewhere */
    struct grammarunit *children[3];
    enum grammarrole role;
    enum grammarrule rule;
    enum grammargender gender;
    int quan; /* quantity; see the Q_* flags below */
    boolean tagged; /* used internally by lang-?? to reorder sentences,
                       everything else should maintain it at FALSE */
};

#define Q_PLURAL      (1 << 29)
/* Indicates the use of a format string for the quantity. */
#define Q_FORMAT      (1 << 28)
#define Q_INDEFINITE  (1 << 30)
#define Q_ZEROARTICLE (1 << 27)
/* Indicates that an explicit quantity was specified. */
#define Q_EXPLICIT    (1 << 26)
/* M for mask */
#define QM_DEFINITE   (~(Q_INDEFINITE | Q_ZEROARTICLE))
#define QM_QUANTITY   (~(Q_PLURAL | Q_FORMAT | Q_INDEFINITE | Q_ZEROARTICLE | \
                         Q_EXPLICIT))

extern void forcecontent(struct grammarunit *, boolean, boolean);
extern void free_grammarunit(struct grammarunit *);
extern char * malloc_parsestring(const char *, boolean, boolean) EXPORT;
extern int scan_string_with_callback(const char *, int (*)(boolean, boolean),
                                     boolean, boolean);

#endif
