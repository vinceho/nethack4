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
#include <time.h>

bool diagonals = false;    /* are we in diagonals mode? */

static int
rng(int max)
{
    return rand() % max;
}

int
main(int argc, char **argv)
{
    long long difficulty = 0;

    srand(time(NULL));

    if (argc >= 2 && !strcmp(argv[1], "--diagonals")) {
        argv[1] = argv[0];
        argv++;
        argc--;
        diagonals = true;
    }

    if (argc == 3)
        difficulty = strtoll(argv[2], NULL, 10);

    if (difficulty > 0 && difficulty < LLONG_MAX) {
        if (!strcmp(argv[1], "storage")) {

            struct chamber *chamber = generate_storage_chamber(difficulty, rng);
            output_chambers(chamber, 1, false, true, stdout);
            free_chamber_internals(chamber);
            free(chamber);

        } else {
            argc = 0;
        }
    } else {
        argc = 0;
    }

    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Sokoban puzzle generator. Copyright (C) 2014 Alex Smith.");
        return EXIT_SUCCESS;
    }

    if (argc != 3) {
        printf("Usage: %s command difficulty\n\n", argv[0]);
        puts("difficulty is a number that is approximately "
             "the number of ways to");
        puts("screw up the puzzle; don't use values above "
             "1000 or so if you want");
        puts("the program to run in a reasonable time. "
             "Commands are as follows:\n");
        puts("storage     Generate a storage chamber");

        return (argc == 2 && !strcmp(argv[1], "--help")) ?
            EXIT_SUCCESS : EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
