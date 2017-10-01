/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-10-01 */
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
    long capacity = 0;
    bool with_solution = false;
    bool debug = false;
    const int n_across = 10;

    srand(time(NULL));

    if (argc >= 2 && !strcmp(argv[1], "--diagonals")) {
        argv[1] = argv[0];
        argv++;
        argc--;
        diagonals = true;
    }
    if (argc >= 2 && !strcmp(argv[1], "--solution")) {
        argv[1] = argv[0];
        argv++;
        argc--;
        with_solution = true;
    }
    if (argc >= 2 && !strcmp(argv[1], "--debug")) {
        argv[1] = argv[0];
        argv++;
        argc--;
        debug = true;
    }

    if (argc == 3 || argc == 4)
        difficulty = strtoll(argv[2], NULL, 10);
    if (argc == 4)
        capacity = strtol(argv[3], NULL, 10);

    if (difficulty > 0 && difficulty < LLONG_MAX) {
        if (!strcmp(argv[1], "storage") && argc == 3) {

            struct chamber *chamber = generate_difficult_chamber(
                difficulty, rng, NULL, false);
            output_chambers(chamber, 1, debug, debug, stdout);
            free_chamber_internals(chamber);
            free(chamber);

        } else if (!strcmp(argv[1], "feed") && argc == 3) {

            int layoutindex;
            struct chamber *chamber = generate_difficult_chamber(
                difficulty, rng, &layoutindex, false);
            output_one_layout(chamber, layoutindex, debug, debug,
                              with_solution, n_across, stdout);
            free_chamber_internals(chamber);
            free(chamber);

        } else if (!strcmp(argv[1], "lockedfeed") && argc == 3) {

            int layoutindex;
            struct chamber *chamber = generate_difficult_chamber(
                difficulty, rng, &layoutindex, true);
            output_one_layout(chamber, layoutindex, debug, debug,
                              with_solution, n_across, stdout);
            free_chamber_internals(chamber);
            free(chamber);

        } else if (!strcmp(argv[1], "directed") && argc == 3) {

            /* "difficulty" is actually the amount of forced capacity */
            int layoutindex;
            struct chamber *chamber = generate_directed_chamber(
                difficulty, rng, &layoutindex);
            output_one_layout(chamber, layoutindex, debug, debug,
                              with_solution, n_across, stdout);
            free_chamber_internals(chamber);
            free(chamber);

        } else if (!strcmp(argv[1], "remcap") && capacity > 0 &&
                   capacity < LONG_MAX) {

            int layoutindex;
            struct chamber *chamber = generate_remcap_chamber(
                difficulty, capacity, rng, &layoutindex);
            output_one_layout(chamber, layoutindex, debug, debug,
                              with_solution, n_across, stdout);
            free_chamber_internals(chamber);
            free(chamber);

        } else if (!strcmp(argv[1], "hardfeed") && capacity > 0 &&
                   capacity < LONG_MAX) {

            int layoutindex1, layoutindex2;
            struct chamber *chamber1 = generate_remcap_chamber(
                difficulty, capacity, rng, &layoutindex1);
            struct chamber *chamber2 = generate_directed_chamber(
                capacity, rng, &layoutindex2);

            struct chamber *chamber3 = glue_chambers(
                chamber1, layoutindex1, chamber2, layoutindex2);
            free_chamber_internals(chamber1);
            free(chamber1);
            free_chamber_internals(chamber2);
            free(chamber2);

            output_one_layout(chamber3, 1, debug, debug,
                              with_solution, n_across, stdout);

            free_chamber_internals(chamber3);
            free(chamber3);

        } else {
            goto options_failure;
        }
        return EXIT_SUCCESS;
    }

    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Sokoban puzzle generator. Copyright (C) 2014, 2017 Alex Smith.");
        return EXIT_SUCCESS;
    }

    if (argc == 2 && !strcmp(argv[1], "place")) {
        struct chamber *chamber = parse_chamber(stdin);
        if (!chamber)
            return EXIT_FAILURE;

        int layoutindex = furthest_layout(chamber, INT_MAX, 0);

        if (layoutindex != -1)
            output_one_layout(chamber, layoutindex, debug, debug,
                              with_solution, n_across, stdout);

        free_chamber_internals(chamber);
        free(chamber);

        return EXIT_SUCCESS;
    }

options_failure:
    printf("Usage: %s command [difficulty] [capacity]\n\n", argv[0]);
    puts("difficulty is a number that is approximately "
         "the number of ways to");
    puts("screw up the puzzle; don't use values above "
         "1000 or so if you want");
    puts("the program to run in a reasonable time. "
         "Commands are as follows:\n");
    puts("storage     Generate a storage chamber [difficulty]");
    puts("feed        Generate a feed chamber [difficulty]");
    puts("lockedfeed  Generate a locked feed chamber [difficulty]");
    puts("directed    Generate a directed chamber [capacity]");
    puts("remcap      Generate a storage chamber with the given");
    puts("            remaining capacity [difficulty, capacity]");
    puts("hardfeed    Generate a feed chamber from a directed and");
    puts("            storage chamber [difficulty, capacity]");
    puts("place       Place crates to find the best feed layout for");
    puts("            a feed chamber read from standard input");

    return (argc == 2 && !strcmp(argv[1], "--help")) ?
        EXIT_SUCCESS : EXIT_FAILURE;
}
