/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-10-03 */
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
#include <wchar.h>

/* Unicode line-drawing characters */
const wchar_t wallchars[16] = {
    0x2500, 0x2500, 0x2502, 0x2514, 0x2500, 0x2500, 0x2518, 0x2534,
    0x2502, 0x250c, 0x2502, 0x251c, 0x2510, 0x252c, 0x2524, 0x253c
};

/* Draws puzzles to the screen or a file. */
void
output_puzzles(struct puzzlerect *const *puzzles, int count, int across,
               FILE *fp)
{
    /* Calculate the maximum width and height of a puzzle. */
    int maxwidth = 0, maxheight = 0;
    for (int i = 0; i < count; i++) {
        if (puzzles[i]->width > maxwidth)
            maxwidth = puzzles[i]->width;
        if (puzzles[i]->height > maxheight)
            maxheight = puzzles[i]->height;
    }
    for (int py = 0; py * across < count; py++) {
        for (int y = 0; y < maxheight; y++) {
            for (int px = 0; px < across; px++) {
                for (int x = 0; x < maxwidth; x++) {
                    int pi = py * across + px;
                    if (pi >= count) {
                        putc(' ', fp);
                        continue;
                    }

                    ppos p = data_bounds_check(puzzles[pi], x, y);

                    if (p & PP_CRATE)
                        putc('0', fp);
                    else if (p & PP_PLAYER)
                        putc('>', fp);
                    else if (p == PP_EXIT)
                        putc('<', fp);
                    else if (p == PP_TARGET)
                        putc('^', fp);
                    else if (p == PP_FLOOR) {
                        // fprintf(fp, "%lc", 0xB7); /* mid-dot */
                        /* for testing */
                        putc(".lhjknuby"[1 + puzzle_pathfind(
                                     puzzles[pi], true, x, y,
                                     puzzles[pi]->conn[0].x,
                                     puzzles[pi]->conn[0].y)], fp);
                    } else {
                        /* Draw connections as +. */
                        for (int i = 0; i < puzzles[pi]->conncount; i++) {
                            if (x == puzzles[pi]->conn[i].x &&
                                y == puzzles[pi]->conn[i].y) {
                                putc('+', fp);
                                goto dont_draw_wall;
                            }
                        }

                        /* Wallify the puzzle. The rule is that a wall connects
                           in a given direction if there's another wall
                           orthogonally in that direction, and a non-wall at 45
                           or 90 degrees to that direction. */

                        uint8_t adjwalls = 0;
                        for (int i = 0; i < 8; i++) {
                            if ((data_bounds_check(puzzles[pi],
                                                   x + xyoffsets[i][0],
                                                   y + xyoffsets[i][1])
                                 & PP_GROUNDMASK) == PP_WALL)
                                adjwalls |= 1<<i;
                        }
                        /*
                         * adjwalls is formatted as follows:
                         * 0x01 east
                         * 0x02 west
                         * 0x04 south
                         * 0x08 north
                         * 0x10 southeast
                         * 0x20 northeast
                         * 0x40 southwest
                         * 0x80 northwest
                         */

                        int wallconn = 0;
                        if (adjwalls & 0x01 && ~adjwalls & 0x3C)
                            wallconn |= 1;
                        if (adjwalls & 0x08 && ~adjwalls & 0xA3)
                            wallconn |= 2;
                        if (adjwalls & 0x02 && ~adjwalls & 0xCC)
                            wallconn |= 4;
                        if (adjwalls & 0x04 && ~adjwalls & 0x53)
                            wallconn |= 8;

                        /* If a wall is orthogonally isolated, we draw it
                           even if wallcon = 0. Otherwise, it's an interior
                           wall which we draw as a space. */
                        if (!wallconn && adjwalls & 0x0F)
                            putc(' ', fp);
                        else
                            fprintf(fp, "%lc", wallchars[wallconn]);

                    dont_draw_wall:
                        ;
                    }
                }
                fprintf(fp, "  ");
            }
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n\n");
    }
}

/* Draws layouts to the screen or a file. */
static void
output_layout_list(const struct layout *const *layouts, int width, int height,
                   int entrypos, size_t n_across, bool show_regions,
                   bool show_locked, FILE *fp)
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

    output_layout_list(layouts, chambers[0].width, chambers[0].height,
                       chambers[0].entrypos, n_across,
                       show_regions, show_locked, fp);
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
                           chamber->entrypos, n, show_regions, show_locked, fp);
    }
}

void
output_one_layout(const struct chamber *chamber, int layoutindex,
                  bool show_regions, bool show_locked, bool with_solution,
                  int n_across, FILE *fp)
{
    struct xarray layoutlist = {0, 0, 0};
    const struct layout *layout = nth_layout(chamber, layoutindex);

    while (layout) {
        *NEW_IN_XARRAY(&layoutlist, const struct layout *) = layout;

        if (!with_solution)
            break;

        int layoutindex = layout->solution->nextindex;
        layout = layoutindex == -1 ? NULL : nth_layout(chamber, layoutindex);
    }

    int i;
    for (i = 0; i < layoutlist.length_in_use; i += n_across) {
        int n = layoutlist.length_in_use - i;
        if (n > n_across)
            n = n_across;
        output_layout_list((const struct layout **)layoutlist.contents + i,
                           chamber->width, chamber->height,
                           chamber->entrypos, n, show_regions, show_locked, fp);
    }

    free(layoutlist.contents);
}

void
output_two_layouts(const struct chamber *chamber, int layoutindex,
                   int layoutindex2, bool show_regions, bool show_locked,
                   FILE *fp)
{
    const struct layout *layout[2] =
        {nth_layout(chamber, layoutindex),
         nth_layout(chamber, layoutindex2)};

    output_layout_list(layout,
                       chamber->width, chamber->height, chamber->entrypos,
                       2, show_regions, show_locked, fp);
}
