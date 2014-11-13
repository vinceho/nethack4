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

/* We use offsets into a locations array to get previous lines while generating
   chambers. We need to make sure they're actually within the array and don't
   wrap left or right. This function only works for negative or zero yrel (while
   generating chambers, positive yrel aren't yet defined). */
static lpos
chamber_generation_offset(lpos *locations, int squares_generated,
                          int width, int x, int yrel)
{
    assert(yrel <= 0);
    if (x < 0 || x >= width || yrel * width + squares_generated < 0)
        return WALL;
    return locations[yrel * width + squares_generated + x];
}

/* A helper function: will adding a wall at the given position cause a dead end
   to be created? This only checks for leftwards, rightwards, and upwards dead
   ends, because it only checks negative yrel. */
static bool
cannot_generate_wall(lpos *locations, int squares_generated,
                     int width, int x)
{
    if (chamber_generation_offset(
            locations, squares_generated, width, x, -1) != WALL) {
        int wallcount = 0;
        wallcount += chamber_generation_offset(
            locations, squares_generated, width, x+1, -1) == WALL;
        wallcount += chamber_generation_offset(
            locations, squares_generated, width, x-1, -1) == WALL;
        wallcount += chamber_generation_offset(
            locations, squares_generated, width, x, -2) == WALL;
        if (wallcount >= 2)
            return true;
    }
    return false;
}

static void generate_chamber_line(
    struct xarray *, int, int, int, lpos *, int, lpos);

/* Generates all the possible chambers of particular dimensions and connection
   pattern, restricting itself to chambers that are actually useful (chambers
   will be omitted if they contain disconneted areas, and might be omitted if
   they decompose into smaller chambers or contain locked areas larger than a
   single tile wide, although both the latter cases are not checked reliably to
   save time). A chamber is specified by its crate-less layout. */
void
generate_chambers(struct xarray *chambers, int width, int height, int entrypos)
{
    /*
     * We want to iterate over all chambers that are /connected/ with respect to
     * their unlocked area, and also with respect to their total area. To
     * accomplish this, we work from the entrance side upwards, caching the
     * connection properties of each square so far with respect to the entrance:
     *
     * OUTSIDE
     *     the square is connected to the entrance through the area that has
     *     generated so far
     *
     * > INTERIOR
     *     the square is not connected to the entrance; different numbers give
     *     different components; INTERIOR itself is used as a temporary in
     *     generate_chamber_line
     *
     * the above values | LOCKED
     *     the connections in question definitively go through locked squares
     *     (without a | LOCKED, it's unclear whether they do or not)
     *
     * WALL
     *     the square is solid
     */

    /* Our working layout. In order to simplify generate_chamber line, we add an
       extra row before the start, that contains only the entry position; that
       way, generate_chamber_line doesn't have to understand how entry positions
       work nor special-case the first line. */
    lpos working[width * (height + 1)];

    int x;
    for (x = 0; x < width; x++)
        working[x] = (x == entrypos ? OUTSIDE : WALL);

    assert((size_t)width < CHAR_BIT * sizeof (unsigned long));

    generate_chamber_line(chambers, width, width * height, entrypos,
                          working + width, 0, INTERIOR);
}

/* Recursive helper function for generate_chambers. */
static void
generate_chamber_line(struct xarray *chambers, int width, int squares,
                      int entrypos, lpos *working,
                      int squares_generated, lpos highest_cnumber_used)
{
    /* Have we generated a complete layout? */
    if (squares_generated == squares) {

        /* Reject any layouts that still contain disconnected areas. */
        int x, y;
        for (x = 0; x < width; x++)
            if ((working[x + squares - width] & ~LOCKED) > INTERIOR)
                return;

        /* Reject any layouts that form dead ends when the top wall is added. */
        for (x = 0; x < width; x++)
            if (cannot_generate_wall(working, squares, width, x))
                return;

        /* Reject any layouts that do not touch all four walls.
           
           The bottom wall must be touched because there's an entrance there,
           and the top wall must be touched because generating a solid row is
           not allowed. So we just check the left and right walls. */

        bool left_wall_touched = false;
        bool right_wall_touched = false;
        for (y = 0; y < squares; y += width) {
            if (working[y] != WALL)
                left_wall_touched = true;
            if (working[y + width - 1] != WALL)
                right_wall_touched = true;
        }

        if (!left_wall_touched || !right_wall_touched)
            return;

        
        /* Now find the locked squares of the layout. (We have a flimsy
           approximation in the working array at the moment; we want an accurate
           version.) We only care about wall locks, because there are no crates
           yet. */
        lpos *locations = memdup(working, squares * sizeof (lpos));
        if (init_wall_locks(locations, width, squares / width, entrypos))
        {
            /* init_wall_locks can reject chambers for not having anywhere to
               place crates, and/or for having 2x2 wall-locked squares. */
            free(locations);
            return;
        }

        /* Create a new chamber with our calculated base layout. */

        struct chamber *newchamber = NEW_IN_XARRAY(chambers, struct chamber);

        newchamber->width = width;
        newchamber->height = squares / width;
        newchamber->entrypos = entrypos;
        newchamber->layouts = (struct xarray){0, 0, 0};
        memset(newchamber->layout_index, 0, sizeof newchamber->layout_index);
        
        struct layout *baselayout =
            NEW_IN_XARRAY(&(newchamber->layouts), struct layout);

        baselayout->playerpos = OUTSIDE;
        baselayout->locations = locations;
        init_layout(baselayout, width, squares / width, entrypos, false);

        return;
    }

    /* If not, generate all the remaining lines; the current line with a loop,
       the other lines using recursive calls.  We loop over all patterns,
       rejecting the illegal ones (because the width is not large, this is not
       massively inefficient, and is probably faster than trying to not
       /generate/ the illegal ones).

       Requirements: we have at least two connections (otherwise this chamber
       could be separated into two), or at least one if diagonals are allowed
       (chambers need to be separated by length-2 corridors in diagonals mode);
       we do not wall off an entire connected INTERIOR component; we have at
       least one OUTSIDE square in the resulting line (OUTSIDE|LOCKED is also
       acceptable when within two lines of the far end of the chamber); we do
       not generate dead ends (we can't block off a square on the previous line
       that already has two walls as neighbours).

       Most of these requirements are simple booleans that we can check as
       we move along the line.  The remaining requirement is more complex; we
       check it afterwards. */

    unsigned long cpattern;
    bool near_goal = (squares - squares_generated <= width * 2);

    for (cpattern = 0; cpattern < (1UL << width); cpattern++) {

        bool unlocked_connection_seen = false;
        bool blocked_dead_end = false;
        int squares_seen = 0;
        lpos current_cnumber = highest_cnumber_used;
        int x, x2;

        for (x = 0; x < width; x++) {
            if (cpattern & (1UL << x)) {

                /* It's open space. */
                lpos prev = working[x + squares_generated - width];
                lpos next = prev;

                if (prev == WALL)
                    next = INTERIOR;
                if ((x == 0 && entrypos != 0) || x == width - 1)
                    next |= LOCKED;

                working[x + squares_generated] = next;

                squares_seen++;
                if (next == OUTSIDE)
                    unlocked_connection_seen = true;

            } else {

                /* It's a wall. */
                working[x + squares_generated] = WALL;

                /* Did we just block a dead end? */
                /* Check leftwards, rightwards, upwards dead ends. */
                if (cannot_generate_wall(working, squares_generated, width, x))
                    blocked_dead_end = true;

                /* Downward dead ends are a special case; normally we check for
                   dead ends when generating the square above the dead end (this
                   is the third wall that forms the dead end), but for downward
                   dead ends, the third wall is to their right rather than above
                   them. */
                if (chamber_generation_offset(
                        working, squares_generated, width, x-2, 0) == WALL &&
                    chamber_generation_offset(
                        working, squares_generated, width, x-1, -1) == WALL &&
                    chamber_generation_offset(
                        working, squares_generated, width, x-1, 0) != WALL)
                    blocked_dead_end = true;
            }
        }

        /* Another special case of downward dead ends: if they are against the
           right wall. (Then the right-hand wall is implicit, and never
           generated.) */
        if (chamber_generation_offset(
                working, squares_generated, width, width-2, 0) == WALL &&
            chamber_generation_offset(
                working, squares_generated, width, width-1, -1) == WALL &&
            chamber_generation_offset(
                working, squares_generated, width, width-1, 0) != WALL)
            blocked_dead_end = true;

        if (squares_seen < (diagonals ? 1 : 2))
            continue; /* no good */
        if (!near_goal && !unlocked_connection_seen)
            continue; /* also no good */
        if (blocked_dead_end)
            continue; /* also no good */

        /* For each component in the previous line, ensure it is represented in
           the current line (and in an unlocked way, if it was unlocked in the
           previous line). */
        if (current_cnumber > INTERIOR) {
            bool abandoned_connection = false;
            for (x = 0; x < width; x++) {
                lpos prev = working[x + squares_generated - width];
                if (prev != WALL && prev != OUTSIDE &&
                    prev != (OUTSIDE | LOCKED)) {
                    abandoned_connection = true;
                    for (x2 = 0; x2 < width; x2++) {
                        lpos next = working[x2 + squares_generated];
                        if (prev == next || prev == (next | LOCKED)) {
                            abandoned_connection = false;
                            break;
                        }
                    }
                }
                if (abandoned_connection)
                    break;
            }
            if (abandoned_connection)
                continue;
        }

        /* Now renumber the components. Two squares get the same component
           number if they are connected together, either along this line, or via
           previous lines (which can be quickly checked by seeing if they have
           the same component number in the previous line; and the component
           number in /this/ line is equal to the component number in the
           previous line, by the construction above, untill we change it). We
           scan from left to right, unifying each square's component with the
           component of the square to its left.

           This algorithm works even in diagonals mode, because we disallow
           diagonal chokepoints, and without such chokepoints existing, the
           definition of connectivity is the same as in orthogonals mode. */

        for (x = 0; x < width; x++) {
            lpos *l = working + x + squares_generated;
            lpos lprev = x == 0 ? WALL : l[-1];

            /* Walls are not part of any component. */
            if (*l == WALL)
                continue;

            /* Choose a component number for the component containing this
               square and the square to its left. Unlocked takes precedence over
               locked; and OUTSIDE is better than generic components is better
               than wall (INTERIOR needs a new number allocated). */
            lpos cnumber = *l;
            if ((!(lprev & LOCKED) && lprev != WALL) || !(*l & LOCKED))
                cnumber &= ~LOCKED;
            if ((lprev & ~LOCKED) == OUTSIDE)
                cnumber = (cnumber & LOCKED) | OUTSIDE;
            if ((cnumber & ~LOCKED) == INTERIOR) {
                if (lprev != WALL)
                    cnumber = (cnumber & LOCKED) | (lprev & ~LOCKED);
                else {
                    /* This is only correct because we know lprev !=
                       INTERIOR (because we've already processed it). */
                    current_cnumber++;
                    cnumber = (cnumber & LOCKED) | current_cnumber;
                }
            }

            /* Mark everything connecting to either this square, or the square
               to the left, as part of the new component. In order for precision
               wrt the locked status, we copy the locked status of the original
               cell if its lpos matches an /unlocked/ lprev or *l, and overwrite
               it if its lpos matches a /locked/ lprev or *l (this is not
               perfect, but is about as good as we can manage on the information
               given, and has only false negatives and no false positives). */
            if (lprev != WALL && lprev != cnumber && lprev & LOCKED)
                for (x2 = 0; x2 < width; x2++)
                    if ((working[x2 + squares_generated] | LOCKED) == lprev)
                        working[x2 + squares_generated] = cnumber;

            if (*l != cnumber && *l & LOCKED)
                for (x2 = 0; x2 < width; x2++)
                    if ((working[x2 + squares_generated] | LOCKED) == *l)
                        working[x2 + squares_generated] = cnumber;

            if (lprev != WALL && lprev != cnumber && !(lprev & LOCKED))
                for (x2 = 0; x2 < width; x2++)
                    if ((working[x2 + squares_generated] & ~LOCKED) == lprev)
                        working[x2 + squares_generated] = cnumber |
                            (working[x2 + squares_generated] & LOCKED);

            if (*l != cnumber && !(*l & LOCKED))
                for (x2 = 0; x2 < width; x2++)
                    if ((working[x2 + squares_generated] & ~LOCKED) == *l)
                        working[x2 + squares_generated] = cnumber |
                            (working[x2 + squares_generated] & LOCKED);
        }

        /* Recursively produce the other lines. */
        generate_chamber_line(chambers, width, squares, entrypos,
                              working, squares_generated + width,
                              current_cnumber);
    }
}

/* Frees everything pointed to by a chamber (but not the chamber itself, because
   it might be part of an xarray). The chamber is left uninitialized. */
void
free_chamber_internals(struct chamber *chamber)
{
    int i;
    for (i = 0; i < HASHSIZE; i++)
        if (chamber->layout_index[i].allocsize)
            free(chamber->layout_index[i].contents);

    for (i = 0; i < chamber->layouts.length_in_use; i++)
        free_layout_internals(nth_layout(chamber, i));

    if (chamber->layouts.allocsize)
        free(chamber->layouts.contents);
}
