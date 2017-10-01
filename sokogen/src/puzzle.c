/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-10-01 */
/* Copyright (c) 2017 Alex Smith. */
/* This Sokoban puzzle generator may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v3 or later
 * If you obtained it as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl3 respectively.
 */

#include "sokogen.h"

extern inline ppos data_bounds_check(const struct puzzlerect *, int, int);

/* Allocates a new puzzlerect, making sure the flexible array member has the
   right size. top and left will be 0, conn and data will be uninitialized
   (but conncount will be 0, so the contents of conn don't matter).

   A puzzlerect can be freed simply using free(). */
static struct puzzlerect *
alloc_puzzlerect(int width, int height) {
    struct puzzlerect *rv =
        malloc((sizeof *rv) + width * height * sizeof (ppos));
    rv->width = width;
    rv->height = height;

    rv->top = rv->left = 0;
    rv->conncount = 0;

    return rv;
}

/* Allocates a new puzzle rectangle from the given chamber layout. The player
   will be placed on a random (non-crate) square within the */
struct puzzlerect *
puzzle_from_layout(const struct chamber *chamber, int layoutindex,
                   int (*rng)(int))
{
    struct layout *layout = nth_layout(chamber, layoutindex);

    /* Special case: an annex is converted into a connection (always), but we
       also remove one layer of wall unless diagonals mode is on. */
    bool has_north_annex = false;
    struct coord annexcoord = {-1, -1};
    for (int x = 0; x < chamber->width; x++) {
        if (layout->locations[(chamber->height - 1) *
                              chamber->width + x] == ANNEX) {
            has_north_annex = true;
            /* Note: there's an offset of (1, 1) from the layout to the puzzle,
               so we have to add 1 to both coordinates */
            annexcoord.x = x + 1;
            annexcoord.y = chamber->height;
        }
    }

    struct puzzlerect *rv = alloc_puzzlerect(
        chamber->width + 2, chamber->height + 2 - has_north_annex);

    int ppos_count = 0;

    for (int y = 0; y < rv->height; y++) {
        for (int x = 0; x < rv->width; x++) {
            lpos l;
            ppos p;
            if (x <= 0 || (x-1) >= chamber->width ||
                y <= 0 || (y-1) >= chamber->height)
                l = WALL;
            else
                l = layout->locations[(y-1) * chamber->width + x-1];

            switch (l & ~LOCKED) {
            case ANNEX:
            case WALL:
                p = PP_WALL;
                break;
            case CRATE:
                p = PP_FLOOR | PP_CRATE;
                break;
            case TARGET:
                p = PP_TARGET;
                break;
            default:
                p = PP_FLOOR;
                if ((l & ~LOCKED) == layout->playerpos) {
                    p |= PP_PLAYER;
                    ppos_count++;
                }
            }

            if (l & LOCKED)
                p |= PP_LOCKED;

            rv->data[y * rv->width + x] = p;
        }
    }

    int ppos_selected = 0;
    if (ppos_count)
        ppos_selected = rng(ppos_count);

    /* Place the player on only one square (by deleting all the player squares
       except the ppos_count-th; ppos_count counts down rather than having a
       separate counter to count up). */
    for (int i = 0; i < (rv->height * rv->width); i++) {
        if (rv->data[i] & PP_PLAYER) {
            if (ppos_selected)
                rv->data[i] &= ~PP_PLAYER;
            ppos_selected--;
        }
    }

    /* Mark the connections. */
    rv->conn[rv->conncount++] =
        (struct coord){.x = chamber->entrypos + 1, .y = 0};
    if (has_north_annex)
        rv->conn[rv->conncount++] = annexcoord;

    return rv;
}
