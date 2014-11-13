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
#include <math.h>

/* Drawing layouts to the screen or a file. */
static void
output_layout_list(const struct layout *const *layouts, int width, int height,
                   int entrypos, size_t n_across, bool show_regions,
                   bool show_locked, const struct layout_solution *solvable,
                   FILE *fp)
{
    int y, x;
    size_t n;
    lpos playerpos[n_across];
    for (n = 0; n < n_across; n++)
        playerpos[n] = layouts[n]->playerpos;

    /* We flip the map vertically, so the entrance is at the bottom. */
    for (y = -2; y <= height; y++) {
        for (n = 0; n < n_across; n++) {
            for (x = -1; x <= width; x++) {
                lpos l = WALL;
                int dx, dy;
                bool any_adjacent_nonwall = false;
                bool any_adjacent_nonwall_dy = false;
                bool any_adjacent_nonwall_dy_n = false;
                bool any_adjacent_nonwall_dy_s = false;

                for (dx = -1; dx <= 1; dx++)
                    for (dy = -2; dy <= 2; dy++) {
                        lpos dl = location_bounds_check(
                            layouts[n]->locations,
                            x + dx, (height - y - dy - 2),
                            width, height, entrypos);

                        if (y + dy >= height)
                            dl = WALL;

                        if (!dx && !dy)
                            l = dl;

                        /* We draw a wall if it's adjacent to any nonwall,
                           including diagonally. */
                        if (dl != WALL && abs(dy) <= 1)
                            any_adjacent_nonwall = true;

                        /* We draw a wall vertically if it's adjacent to
                           walls directly north and south, and those squares
                           are adjacent to nonwalls. */
                        if (dl != WALL && !dx && abs(dy) <= 1)
                            any_adjacent_nonwall_dy = true;
                        if (dl != WALL && dy <= 0)
                            any_adjacent_nonwall_dy_n = true;
                        if (dl != WALL && dy >= 0)
                            any_adjacent_nonwall_dy_s = true;
                    }

                any_adjacent_nonwall_dy |=
                    !(any_adjacent_nonwall_dy_n & any_adjacent_nonwall_dy_s);

                if (!show_locked)
                    l &= ~LOCKED;

                if (l == WALL)
                    putc(any_adjacent_nonwall ? any_adjacent_nonwall_dy ?
                         '-' : '|' : ' ', fp);
                else if (l == CRATE)
                    putc('0', fp);
                else if (l == TARGET)
                    putc('^', fp);
                else if (l & ANNEX)
                    putc('1' + (l & ~ANNEX), fp);
                else if ((l == playerpos[n] ||
                          l == (playerpos[n] | LOCKED)) &&
                         (playerpos[n] != OUTSIDE || y == height - 1)) {
                    putc('@', fp);
                    playerpos[n] = WALL; /* don't draw the player again */
                } else if (y == height - 1)
                    putc('>', fp);
                else if (l == (OUTSIDE | LOCKED) ||
                           (!show_regions && (l & LOCKED)))
                    putc('8', fp);
                else if (l == OUTSIDE || (!show_regions))
                    putc('.', fp);
                else if (l & LOCKED)
                    putc('A' + (l - INTERIOR - LOCKED), fp);
                else
                    putc('a' + (l - INTERIOR), fp);
            }
            putc(' ', fp);
            putc(' ', fp);
        }
        putc('\n', fp);
    }
    putc('\n', fp);
}

/* Draws chambers to the screen or a file. They will be drawn at max capacity,
   with the player outside them. */
void
output_chambers(const struct chamber *chambers, size_t n_across,
                bool show_regions, bool show_locked, FILE *fp)
{
    const struct layout *layouts[n_across];
    size_t n;

    for (n = 0; n < n_across; n++)
        layouts[n] = nth_layout(chambers + n, max_capacity_layout(chambers + n));

    struct layout_solution unique_address;

    output_layout_list(layouts, chambers[0].width, chambers[0].height,
                       chambers[0].entrypos, n_across,
                       show_regions, show_locked, &unique_address, fp);
}

/* Draws the layouts of a single chamber to the screen or a file. */
void
output_layouts(const struct chamber *chamber, size_t n_across,
               bool show_regions, bool show_locked, FILE *fp)
{
    const struct layout *layouts[chamber->layouts.length_in_use];
    int i;
    for (i = 0; i < chamber->layouts.length_in_use; i++)
        layouts[i] = nth_layout(chamber, i);

    for (i = 0; i < chamber->layouts.length_in_use; i += n_across) {
        int n = chamber->layouts.length_in_use - i;
        if (n > n_across)
            n = n_across;
        output_layout_list(layouts + i, chamber->width, chamber->height,
                           chamber->entrypos, n, show_regions, show_locked,
                           layouts[0]->solution->loopgroup ?
                           layouts[0]->solution->loopgroup :
                           layouts[0]->solution, fp);
    }
}

void
output_one_layout(const struct chamber *chamber, int layoutindex,
                  bool show_regions, bool show_locked, FILE *fp)
{
    struct layout_solution unique_address;
    const struct layout *layout = nth_layout(chamber, layoutindex);

    output_layout_list(&layout,
                       chamber->width, chamber->height, chamber->entrypos,
                       1, show_regions, show_locked, &unique_address, fp);
}

void
output_two_layouts(const struct chamber *chamber, int layoutindex,
                   int layoutindex2, bool show_regions, bool show_locked,
                   FILE *fp)
{
    struct layout_solution unique_address;
    const struct layout *layout[2] =
        {nth_layout(chamber, layoutindex),
         nth_layout(chamber, layoutindex2)};

    output_layout_list(layout,
                       chamber->width, chamber->height, chamber->entrypos,
                       2, show_regions, show_locked, &unique_address, fp);
}
