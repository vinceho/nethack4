/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-03-31 */
/* Copyright (c) 2014 Alex Smith. */
/* This Sokoban puzzle generator may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v3 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl3 respectively.
 */

#include <sokogen.h>

bool diagonals = false; /* are we in diagonals mode? */

int
main(int argc, char **argv)
{
    struct xarray chambers = {0, 0, 0};
    size_t i;

    generate_chambers(&chambers, 5, 3, 1);

    const size_t n_across = 18;

    for (i = 0; i < chambers.length_in_use; i += n_across) {
        output_chambers(((struct chamber *)chambers.contents) + i,
                        (i + n_across > chambers.length_in_use ?
                         chambers.length_in_use - i : n_across),
                        true, true, stdout);
    }
    printf("%d chambers generated.\n", chambers.length_in_use);

    /* We could clean up memory here, but there's no real need; the OS will
       do it for us. */
    return EXIT_SUCCESS;
}
