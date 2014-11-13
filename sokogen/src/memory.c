/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-11-13 */
/* Copyright (c) 2014 Alex Smith. */
/* This Sokoban puzzle generator may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v3 or later
 * If you obtained it as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl3 respectively.
 */

#include "sokogen.h"

/* We have a malloc/realloc/free wrapper for extensible arrays, that does all
   the book-keeping. (Just for fun, I tried to get Splint to understand this
   once. I even almost managed it, although I needed to do a bunch of things
   like making variables into 1-element arrays to work around bugs.) */
void
alloc(struct xarray *x, size_t newcount, size_t elemsize)
{
    /* newcount == 0 means free the buffer. */
    if (newcount == 0) {
        if (x->allocsize != 0) {
            free(x->contents);
        }
        x->allocsize = 0;
        x->length_in_use = 0;
        return;
    }

    if (newcount <= x->allocsize)
        return;

    /* We start by allocating 1 element, then repeatedly doubling, unless we
       want to allocate a larger chunk than merely doubling would produce. */
    x->allocsize *= 2;
    if (x->allocsize < newcount)
        x->allocsize = newcount;
    if (x->allocsize < (size_t)1)
        x->allocsize = (size_t)1;

    assert(x->allocsize < SIZE_MAX / elemsize);

    if (x->allocsize == 0) {
        x->contents = malloc(x->allocsize * elemsize);
    } else {
        x->contents = realloc(x->contents, x->allocsize * elemsize);
    }

    if (x->contents == NULL) {
        perror("Allocating memory");
        exit(EXIT_FAILURE);
    }
}

/* Our "malloc" more generally. Memory allocated like this is freed
   using "free". */
void *
memdup(void *m, size_t z)
{
    void *rv = malloc(z);
    if (rv == NULL) {
        perror("Allocating memory");
        exit(EXIT_FAILURE);
    }

    memcpy(rv, m, z);
    return rv;
}

/* Our "realloc" more generally. */
void *
padrealloc(void *m, size_t elemsize, size_t newcount, size_t oldcount,
           const void *padding)
{
    m = realloc(m, elemsize * newcount);
    if (m == NULL) {
        perror("Allocating memory");
        exit(EXIT_FAILURE);
    }

    size_t z;
    for (z = oldcount; z < newcount; z++)
        memcpy(((unsigned char (*)[elemsize])m)[z], padding, elemsize);

    return m;
}

/* Adds a new element to an extensible array, and returns its index. Returning
   a pointer would be more useful, but hard to do without C++-style
   templates; so that's done with a macro in sokogen.h. */
size_t
new_in_xarray(struct xarray *x, size_t elemsize)
{
    alloc(x, ++(x->length_in_use), elemsize);
    return x->length_in_use - 1;
}
