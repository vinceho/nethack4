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

/* Reads the given file until EOF; treats any |, - and space as wall,
   and requires an @ in the penultimate line to represent the
   entrance. The final line, and the rest of the penultimate line,
   must be solid. */
struct chamber *
parse_chamber(FILE *in) {
    struct xarray infile;
    int c;

    while (((c = getc(in))) != EOF)
        *NEW_IN_XARRAY(&infile, char) = c;

    int width = 0;
    int height = -3;
    int width_so_far = 0;

    int i;
    for (i = 0; i < infile.length_in_use; i++) {
        if (((char *)infile.contents)[i] == '\n') {
            if (width_so_far > width)
                width = width_so_far;
            width_so_far = 0;
            height++;
        } else
            width_so_far++;
    }

    if (width_so_far) {
        fprintf(stderr, "file does not end with a newline\n");
        return NULL;
    }

    width -= 2;

    if (height < 3 || width < 4) {
        fprintf(stderr, "puzzle too small\n");
        return NULL;
    }


    lpos wall = WALL;
    lpos *locations = padrealloc(NULL, sizeof (lpos), width * height, 0, &wall);
    int x = 0;
    int y = height;
    int entrypos = -1;

    for (i = 0; i < infile.length_in_use; i++) {
        c = ((char *)infile.contents)[i];
        bool iswall = c == '-' || c == '|' || c == ' ';
        if (c == '\n') {
            y--;
            x = -2;
        } else if (y == height) {
            if (!iswall) {
                fprintf(stderr, "missing top walls ('%c')\n", c);
                return NULL;
            }
        } else if (x < 0) {
            if (!iswall) {
                fprintf(stderr, "missing left walls ('%c')\n", c);
                return NULL;
            }
        } else if (x >= width) {
            if (!iswall) {
                fprintf(stderr, "missing right walls ('%c')\n", c);
                return NULL;
            }
        } else if (y >= 0) {
            locations[y * width + x] = iswall ? WALL : OUTSIDE;
        } else {
            if (y == -1 && c == '@')
                entrypos = x;
            else if (!iswall) {
                fprintf(stderr, "missing bottom walls ('%c')\n", c);
                return NULL;
            }
        }
        x++;
    }

    if (entrypos == -1) {
        fprintf(stderr, "missing entrypos\n");
        return NULL;
    }

    struct chamber *newchamber = memdup(&(struct chamber) {
            .width = width, .height = height, .entrypos = entrypos,
                .annexcap = 0}, sizeof *newchamber);
    memset(newchamber->layout_index, 0, sizeof newchamber->layout_index);
    struct layout *layout =
        NEW_IN_XARRAY(&(newchamber->layouts), struct layout);

    init_wall_locks(locations, width, height, entrypos, false);

    layout->playerpos = OUTSIDE;
    layout->locations = memdup(locations, width * height * sizeof (lpos));

    init_layout(layout, width, height, entrypos, 0, true);

    return newchamber;
}
