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

bool diagonals = false; /* are we in diagonals mode? */

int
main(int argc, char **argv)
{
    size_t i;
    struct xarray chambers = {0, 0, 0};

    printf("Generating chambers...\r");
    fflush(stdout);
    generate_chambers(&chambers, 5, 4, 2);

    for (i = 0; i < chambers.length_in_use; i++) {
        printf("Generating layouts... %zd/%zd\r", i, chambers.length_in_use);
        fflush(stdout);
        struct chamber *chamber = ((struct chamber *)chambers.contents) + i;
        find_layouts_from(chamber, 0);
    }

    const size_t n_across = 18;

    for (i = 0; i < chambers.length_in_use; i += n_across) {
        output_chambers(((struct chamber *)chambers.contents) + i,
                        (i + n_across > chambers.length_in_use ?
                         chambers.length_in_use - i : n_across),
                        true, true, stdout);
    }
    printf("%zu chambers generated.\n\n", chambers.length_in_use);

    /* We could clean up memory here, but there's no real need; the OS will
       do it for us. */
    return EXIT_SUCCESS;
}
