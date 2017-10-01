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

/* Calculates the minimum possible distance between two points, based only
   on their coordinates. */
static int
distance_estimator(int from_x, int from_y, struct coord to)
{
    int dx = to.x - from_x;
    int dy = to.y - from_y;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;

    if (diagonals) {
        return (dx > dy) ? dx : dy;
    } else {
        return dx + dy;
    }
}

/* Finds a path from one square to another in a puzzle. Returns -1 if there
   is no path, otherwise the coordinates of the first step on the path (as an
   index into xyoffsets).

   The caller can specify whether the pathfind should avoid crates (other than
   on from and to). Targets and exits will be treated as blocked. From and to
   will never be treated as blocked, regardless of what is there. */
int
puzzle_pathfind(struct puzzlerect *puzzle, bool crates_block,
                int from_x, int from_y, int to_x, int to_y)
{
    /* Return value. Initially -1, changed if we find a path.*/
    int rv = -1;

    /* When we add a coordinate to good_coords or bad_coords for the first
       time, we then replace that in the puzzle with a wall (so that we don't
       get stuck in a routing loop). We have to replace them all with floor
       again later, so this xarray remembers the coordinates that need
       replacing. */
    struct xarray torestore = {0, 0, 0};

    /* This array stores coordinates that are connected to to_xy, sorted by the
       number of squares that they are off the most direct route (mod 3; a
       movement can't take us more than 2 squares off the most direct route and
       we always exhaust one length before trying the next, so mod 3 is
       sufficient). */
    struct xarray working[3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    int indirectness_mod_3 = 0;

    /* The return value. -1 by default, we set it to a coordinate if we
       find a path. */

    struct xarray *cur_coords = working + indirectness_mod_3; //

    *NEW_IN_XARRAY(cur_coords, struct coord) =
        (struct coord){.x = to_x, .y = to_y};

    while (cur_coords->length_in_use) {

        const int i = --cur_coords->length_in_use;
        struct coord cur = ((struct coord *)(cur_coords->contents))[i];

        *NEW_IN_XARRAY(&torestore, struct coord) = cur;
        puzzle->data[puzzle->width * cur.y + cur.x] |= PP_WALL;

        const int old_de = distance_estimator(from_x, from_y, cur);

        for (int j = 0; j < (diagonals ? 8 : 4); j++) {
            struct coord prev = {.x = cur.x - xyoffsets[j][0],
                                 .y = cur.y - xyoffsets[j][1]};

            /* Have we found a path? */
            if (prev.x == from_x && prev.y == from_y) {
                rv = j;
                goto path_found;
            }

            /* Is this a legal square? */
            if (prev.x < 0 || prev.y < 0 ||
                prev.x >= puzzle->width || prev.y >= puzzle->height)
                continue; /* can happen if to is on a wall */

            ppos p = puzzle->data[puzzle->width * prev.y + prev.x];
            if ((p & PP_GROUNDMASK) != PP_FLOOR)
                continue;
            if (crates_block && p & PP_CRATE)
                continue;

            const int new_de = distance_estimator(from_x, from_y, prev);

            /* Ideally, new_de would equal old_de - 1 (i.e. we're going from
               somewhere closer to from). Howevere, old_de + 0 and old_de + 1
               are also possible. */
            const int de_diff = new_de - (old_de - 1);

            struct xarray *const into_coords = working +
                ((indirectness_mod_3 + de_diff) % 3);

            *NEW_IN_XARRAY(into_coords, struct coord) = prev;
        }

        if (cur_coords->length_in_use == 0) {
            /* OK, we've proven that the previous lower bound doesn't work.
               Try the next value up. */
            indirectness_mod_3++;
            indirectness_mod_3 %= 3;
            cur_coords = working + indirectness_mod_3;

            if (cur_coords->length_in_use == 0) {
                /* We have one more value to try before giving up. */
                indirectness_mod_3++;
                indirectness_mod_3 %= 3;
                cur_coords = working + indirectness_mod_3;
            }
        }
    }

path_found: /* or found not to exist */

    /* Restore the puzzle. */
    while (torestore.length_in_use) {
        const int i = --torestore.length_in_use;
        struct coord cur = ((struct coord *)(torestore.contents))[i];
        puzzle->data[puzzle->width * cur.y + cur.x] &= ~PP_WALL;
    }

    /* Free our xarrays. */
    if (torestore.allocsize) free(torestore.contents);
    if (working[0].allocsize) free(working[0].contents);
    if (working[1].allocsize) free(working[1].contents);
    if (working[2].allocsize) free(working[2].contents);

    return rv;
}
