/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-04-05 */
/* Copyright 1988, Mike Stephenson                                */
/* NetHack may be freely redistributed.  See license for details. */

/*      attrib.h - Header file for character class processing. */

#ifndef ATTRIB_H
# define ATTRIB_H

# include "global.h"

/* attribute IDs */
# define A_STR      0
# define A_INT      1
# define A_WIS      2
# define A_DEX      3
# define A_CON      4
# define A_CHA      5

# define A_MAX      6   /* used in rn2() selection of attrib */

# define ABASE(x)   (u.acurr.a[x])
# define ABON(x)    (u.abon.a[x])
# define AEXE(x)    (u.aexe.a[x])
# define ACURR(x)   (acurr(x))
# define ACURRSTR   (acurrstr())
/* should be: */
/* #define ACURR(x) (ABON(x) + ATEMP(x) + (Upolyd  ? MBASE(x) : ABASE(x)) */
# define MCURR(x)   (u.macurr.a[x])
# define AMAX(x)    (u.amax.a[x])
# define MMAX(x)    (u.mamax.a[x])

# define ATEMP(x)   (u.atemp.a[x])
# define ATIME(x)   (u.atime.a[x])

/* KMH -- Conveniences when dealing with strength constants */
# define STR18(x)   (18+(x))    /* 18/xx */
# define STR19(x)   (100+(x))   /* For 19 and above */

struct attribs {
    schar a[A_MAX];
};

# define ATTRMAX(x) \
    ((x == A_STR && Upolyd && strongmonst(youmonst.data)) ? STR18(100) \
                                                          : urace.attrmax[x])
# define ATTRMIN(x) (urace.attrmin[x])

#endif /* ATTRIB_H */

