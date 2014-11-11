/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-11-11 */
/* Copyright (c) 2014 Alex Smith. */
/* This Sokoban puzzle generator may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v3 or later
 * If you obtained it as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl3 respectively.
 */

#include "sokogen.h"

/* Replaces a given lpos with a different given lpos in a connected region. */
void
floodfill(lpos *locations, int width, int height, int x, int y,
          lpos from, lpos to, bool ignore_locks)
{
    assert(from != to);
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    if ((locations[y * width + x] | (ignore_locks ? LOCKED : 0)) !=
        (from | (ignore_locks ? LOCKED : 0)))
        return;

    if (ignore_locks)
        locations[y * width + x] = (to & ~LOCKED) |
            (locations[y * width + x] & LOCKED);
    else
        locations[y * width + x] = to;

    floodfill(locations, width, height, x + 1, y, from, to, ignore_locks);
    floodfill(locations, width, height, x - 1, y, from, to, ignore_locks);
    floodfill(locations, width, height, x, y + 1, from, to, ignore_locks);
    floodfill(locations, width, height, x, y - 1, from, to, ignore_locks);
}


/* Finds wall-locked squares in a locations array. A locked square is a square
   on which, if a crate were placed there right now, it would never be possible
   to move that crate to a goal no matter what sort of other movements are
   made. This is function designed for initializing a chamber, and as such, will
   also clear any CRATEs and INTERIOR areas, setting them back to
   OUTSIDE.

   This function returns true if the wall-lock structure of the chamber is such
   that it should not be allowed to be used (because it has structures which are
   never useful for moving crates around). */
bool
init_wall_locks(lpos *locations, int width, int height, int entrypos)
{
    /*
     * A square can be wall-locked for any of the following reasons:
     *
     * - The square is in a corner (next to two walls in non-opposite
     *   directions, four under diagonals rules). This is a "corner lock".
     *
     * - The square is on a wall with no outwards-bending corners. (The entrance
     *   is assumed to potentially have outwards-bending corners even if they
     *   aren't visible on the map, which is why entrypos is required here.)
     *   This is an "edge lock".
     *
     * - The square would have to be pushed through a wall-locked area to
     *   navigate it to the goal. This is a "connectivity lock".
     *
     * - The square disconnects the area it's in, and pushing it in any
     *   direction accessible from the entrance would push it onto a
     *   wall-locked square. This is a "chokepoint lock".
     *
     * Wall-locked areas are thus defined recursively; we calculate which
     * squares are wall-locked once for the chamber, because it depends only
     * on the position of the walls.
     *
     * A square can also be crate-locked; this function does not calculate
     * that, and will clear any crate locks in the array.
     */

#define LOCATION_AT(dx, dy)                                     \
            location_bounds_check(locations, (x)+dx, (y)+dy,    \
                                  width, height, entrypos)

    /* We mark squares as initially locked, unlock them if we find no corner
       or edge locks, then handle connectivity and chokepoint locks.

       LOCKED = not checked for corner/edge locks (or found to have a lock);
       INTERIOR = not checked for connectivity locks. */
    int x, y;
    for (x = 0; x < width * height; x++)
        locations[x] = locations[x] == WALL ? WALL : (INTERIOR | LOCKED);

    /* Corner and edge locks. */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {

            /* Walls can't be locked. */
            if (LOCATION_AT(0, 0) == WALL)
                continue;

            /* Corner locks.
            
               Technically speaking, this is unnecessary; all corner locks are
               also chokepoint locks.  But this saves time floodfilling and
               unfloodfilling. */

            bool vwall = false;
            bool hwall = false;
            bool swall = !diagonals;
            bool bwall = !diagonals;
            vwall |= LOCATION_AT(-1, 0) == WALL;
            hwall |= LOCATION_AT(0, -1) == WALL;
            vwall |= LOCATION_AT(+1, 0) == WALL;
            hwall |= LOCATION_AT(0, +1) == WALL;
            swall |= LOCATION_AT(-1, -1) == WALL;
            bwall |= LOCATION_AT(+1, -1) == WALL;
            swall |= LOCATION_AT(+1, +1) == WALL;
            bwall |= LOCATION_AT(-1, +1) == WALL;

            if (vwall && hwall && swall && bwall)
                continue;

            /* Edge locks.

               These sometimes aren't chokepoint locks, so we need to check
               for them separately even in theory.
            
               TODO: In diagonals mode, we need to check that both squares
               at the end of the wall are walls, not just one. */

            int d;

            d = 0;
            while (LOCATION_AT( 0, d) != WALL &&
                   LOCATION_AT(+1, d) == WALL) d++;
            if (LOCATION_AT(+1, d) == WALL) {
                d = 0;
                while (LOCATION_AT( 0, d) != WALL &&
                       LOCATION_AT(+1, d) == WALL) d--;
                if (LOCATION_AT(+1, d) == WALL)
                    continue;
            }

            d = 0;
            while (LOCATION_AT( 0, d) != WALL &&
                   LOCATION_AT(-1, d) == WALL) d++;
            if (LOCATION_AT(-1, d) == WALL) {
                d = 0;
                while (LOCATION_AT( 0, d) != WALL &&
                       LOCATION_AT(-1, d) == WALL) d--;
                if (LOCATION_AT(-1, d) == WALL)
                    continue;
            }

            d = 0;
            while (LOCATION_AT(d,  0) != WALL &&
                   LOCATION_AT(d, +1) == WALL) d++;
            if (LOCATION_AT(d, +1) == WALL) {
                d = 0;
                while (LOCATION_AT(d,  0) != WALL &&
                       LOCATION_AT(d, +1) == WALL) d--;
                if (LOCATION_AT(d, +1) == WALL)
                    continue;
            }

            d = 0;
            while (LOCATION_AT(d,  0) != WALL &&
                   LOCATION_AT(d, -1) == WALL) d++;
            if (LOCATION_AT(d, -1) == WALL) {
                d = 0;
                while (LOCATION_AT(d,  0) != WALL &&
                       LOCATION_AT(d, -1) == WALL) d--;
                if (LOCATION_AT(d, -1) == WALL)
                    continue;
            }

            if (diagonals) {
                d = 0;
                while (LOCATION_AT(  d,   d) != WALL &&
                       LOCATION_AT(d+1, d-1) == WALL) d++;
                if (LOCATION_AT(d+1, d-1) == WALL) {
                    d = 0;
                    while (LOCATION_AT(  d,   d) != WALL &&
                           LOCATION_AT(d+1, d-1) == WALL) d--;
                    if (LOCATION_AT(d+1, d-1) == WALL)
                        continue;
                }

                d = 0;
                while (LOCATION_AT(  d,   d) != WALL &&
                       LOCATION_AT(d-1, d+1) == WALL) d++;
                if (LOCATION_AT(d-1, d+1) == WALL) {
                    d = 0;
                    while (LOCATION_AT(  d,   d) != WALL &&
                           LOCATION_AT(d-1, d+1) == WALL) d--;
                    if (LOCATION_AT(d-1, d+1) == WALL)
                        continue;
                }

                d = 0;
                while (LOCATION_AT(  d,   -d) != WALL &&
                       LOCATION_AT(d+1, -d+1) == WALL) d++;
                if (LOCATION_AT(d+1, -d+1) == WALL) {
                    d = 0;
                    while (LOCATION_AT(  d,   -d) != WALL &&
                           LOCATION_AT(d+1, -d+1) == WALL) d--;
                    if (LOCATION_AT(d+1, -d+1) == WALL)
                        continue;
                }

                d = 0;
                while (LOCATION_AT(  d,   -d) != WALL &&
                       LOCATION_AT(d-1, -d-1) == WALL) d++;
                if (LOCATION_AT(d-1, -1-d) == WALL) {
                    d = 0;
                    while (LOCATION_AT(  d,   -d) != WALL &&
                           LOCATION_AT(d-1, -d-1) == WALL) d--;
                    if (LOCATION_AT(d-1, -d-1) == WALL)
                        continue;
                }
            }

            /* It isn't edge or corner locked. It might be chokepoint or
               connectivity locked; set it to INTERIOR to show that
               connectivity hasn't been checked yet. */
            locations[y * width + x] = INTERIOR;            
        }
    }

    /* Chokepoint locks.  These are done before connectivity locks; a chokepoint
       can hurt connectivity, but a lack of connectivity cannot create new
       chokepoints (as all connectivity-locked areas are bounded by other sorts
       of lock). However, chokepoints can create new chokepoints, so we have to
       run this repeatedly until nothing further happens. */
    bool any_locks_found = true;
    while (any_locks_found) {
        any_locks_found = false;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (locations[y * width + x] != INTERIOR)
                    continue;
                
                bool is_locked = true;
                
                /* Discover which areas are blocked by a hypothetical crate
                   here. */
                locations[y * width + x] = CRATE;
                floodfill(locations, width, height, entrypos, 0,
                          INTERIOR, OUTSIDE, true);

                /* Do we have any pushes into unlocked areas from any direction
                   we could reach with the crate blocking our way? */
                if ((LOCATION_AT(-1, 0) == OUTSIDE ||
                     LOCATION_AT(-1, 0) == (OUTSIDE | LOCKED)) &&
                    (LOCATION_AT(+1, 0) == OUTSIDE ||
                     LOCATION_AT(+1, 0) == INTERIOR))
                    is_locked = false;
                if ((LOCATION_AT(+1, 0) == OUTSIDE ||
                     LOCATION_AT(+1, 0) == (OUTSIDE | LOCKED)) &&
                    (LOCATION_AT(-1, 0) == OUTSIDE ||
                     LOCATION_AT(-1, 0) == INTERIOR))
                    is_locked = false;
                if ((LOCATION_AT(0, -1) == OUTSIDE || 
                     LOCATION_AT(0, -1) == (OUTSIDE | LOCKED)) &&
                    (LOCATION_AT(0, +1) == OUTSIDE ||
                     LOCATION_AT(0, +1) == INTERIOR))
                    is_locked = false;
                if ((LOCATION_AT(0, +1) == OUTSIDE ||
                     LOCATION_AT(0, +1) == (OUTSIDE | LOCKED)) &&
                    (LOCATION_AT(0, -1) == OUTSIDE ||
                     LOCATION_AT(0, -1) == INTERIOR))
                    is_locked = false;

                if (diagonals) {
                    if ((LOCATION_AT(-1, -1) == OUTSIDE ||
                         LOCATION_AT(-1, -1) == (OUTSIDE | LOCKED)) &&
                        (LOCATION_AT(+1, +1) == OUTSIDE ||
                         LOCATION_AT(+1, +1) == INTERIOR))
                        is_locked = false;
                    if ((LOCATION_AT(+1, +1) == OUTSIDE ||
                         LOCATION_AT(+1, +1) == (OUTSIDE | LOCKED)) &&
                        (LOCATION_AT(-1, -1) == OUTSIDE ||
                         LOCATION_AT(-1, -1) == INTERIOR))
                        is_locked = false;
                    if ((LOCATION_AT(+1, -1) == OUTSIDE ||
                         LOCATION_AT(+1, -1) == (OUTSIDE | LOCKED)) &&
                        (LOCATION_AT(-1, +1) == OUTSIDE ||
                         LOCATION_AT(-1, +1) == INTERIOR))
                        is_locked = false;
                    if ((LOCATION_AT(-1, +1) == OUTSIDE ||
                         LOCATION_AT(-1, +1) == (OUTSIDE | LOCKED)) &&
                        (LOCATION_AT(+1, -1) == OUTSIDE ||
                         LOCATION_AT(+1, -1) == INTERIOR))
                        is_locked = false;
                }

                /* Undo our changes. */
                floodfill(locations, width, height, entrypos, 0,
                          OUTSIDE, INTERIOR, true);
                locations[y * width + x] =
                    is_locked ? (OUTSIDE | LOCKED) : INTERIOR;
                any_locks_found |= is_locked;
            }
        }
    }

    /* Connectivity locks. */
    /* Find locations accessible from the entrance. */
    floodfill(locations, width, height, entrypos, 0,
              INTERIOR, OUTSIDE, false);

    /* Now change all locked squares to OUTSIDE | LOCKED.  */
    int outside_squares_seen = 0;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (locations[y * width + x] == OUTSIDE)
                outside_squares_seen++;
            else if (locations[y * width + x] != WALL)
                locations[y * width + x] = OUTSIDE | LOCKED;
        }
    }

    /* Should we reject this chamber? We need space to put crates (the entry
       square and one other to give the marble room to turn round; chokepoint
       locking fails on the entry square, which is why the entry square can be
       marked as unlocked even if it's chokepoint locked), and we need to avoid
       non-corridor-style locked structures. */
    if (outside_squares_seen < 2)
        return true;

    for (y = 0; y < height - 1; y++) {
        for (x = 0; x < width - 1; x++) {
            if (LOCATION_AT(0, 0) == (OUTSIDE | LOCKED) &&
                LOCATION_AT(0, 1) == (OUTSIDE | LOCKED) &&
                LOCATION_AT(1, 0) == (OUTSIDE | LOCKED) &&
                LOCATION_AT(1, 1) == (OUTSIDE | LOCKED))
                return true;
        }
    }

#undef LOCATION_AT

    return false;
}
