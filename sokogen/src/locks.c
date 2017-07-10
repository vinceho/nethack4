/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-07-10 */
/* Copyright (c) 2014 Alex Smith. */
/* This Sokoban puzzle generator may be distributed under either of the
 * following licenses:
 *  - the NetHack General Public License
 *  - the GNU General Public License v3 or later
 * If you obtained it as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl3 respectively.
 */

#include "sokogen.h"

/* Replaces a given lpos with a different given lpos in a connected region.
   'ignore_locks' will flood even through locked areas. 'pull_only' will only
   flood from a position A to a position B if it's possible to push a crate from
   position B to position A. If 'storage' is set, then flooding from a position
   A to a position B will only happen if it's possible to push a crate from
   position A to position B.

   Setting 'pull_only' and 'storage' simultaneously is a Bad Idea, because it
   will fail to fill squares for which pushing and pulling must be done from
   different directions. */
void
floodfill(lpos *locations, int width, int height, int entrypos, int x, int y,
          lpos from, lpos to, bool ignore_locks, bool pull_only, bool storage)
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

#define IS_WALL(x2, y2) (                                               \
        (location_bounds_check(                                         \
            locations, (x2), (y2), width, height, entrypos) | LOCKED) == \
        (WALL | LOCKED))

    volatile int d;
    for (d = 0; d < (diagonals ? 8 : 4); d++) {
        const int dx = xyoffsets[d][0];
        const int dy = xyoffsets[d][1];

        if ((!pull_only || !IS_WALL(x + dx * 2, y + dy * 2)) &&
            (!storage   || !IS_WALL(x - dx,     y - dy)))
            floodfill(locations, width, height, entrypos, x + dx, y + dy,
                      from, to, ignore_locks, pull_only, storage);
    }
#undef IS_WALL
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
init_wall_locks(lpos *locations, int width, int height, int entrypos,
                bool storage)
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
     * - A crate on the square cannot be pushed to the goal. This is a
     *   "connectivity lock".
     *
     * - The square disconnects the area it's in, and pushing it in any
     *   direction accessible from the entrance would push it onto a
     *   wall-locked square. This is a "chokepoint lock".
     *
     * Wall-locked areas are thus defined recursively; we calculate which
     * squares are wall-locked once for the chamber, because it depends only
     * on the position of the walls.
     *
     * Edge locks and corner locks are special casess of connectivity locks,
     * so we don't need code to handle those.
     *
     * A square can also be crate-locked; this function does not calculate
     * that, and will clear any crate locks in the array.
     */

#define LOCATION_AT(dx2, dy2)                            \
    location_bounds_check(locations, (x)+dx2, (y)+dy2,   \
                          width, height, entrypos)

    /* We mark squares as initially locked. Then we mark all squares that are
       not connectivity-locked as unlocked; then we look for chokepoint
       locks. */
    int x, y;
    for (x = 0; x < width * height; x++)
        locations[x] = locations[x] == WALL ? WALL :
            (locations[x] & ANNEX) ? locations[x] : (INTERIOR | LOCKED);

    /* Corner and edge locks */
    /* We find the corner and edge locks by running the connectivity lock
       algorithm.  This will also find some connectivity locks, but perhaps not
       all of them; if it finds some, we use that information to optimize
       chokepoint lock calculation, but it's not vital that they're found. */
    floodfill(locations, width, height, entrypos, entrypos, 0,
              INTERIOR | LOCKED, INTERIOR, false, true, false);

    /* Chokepoint locks */
    /* These are done before finalizing connectivity locks; a chokepoint can
       hurt connectivity, but a lack of connectivity cannot create new
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
                floodfill(locations, width, height, entrypos, entrypos, 0,
                          INTERIOR, OUTSIDE, true, false, false);

                /* Do we have any pushes into unlocked areas from any direction
                   we could reach with the crate blocking our way?

                   Note: if this is not a storage chamber, check also directions
                   we "couldn't reach"; it's possible that we can start with a
                   crate in one of those areas and then push it onto the
                   chokepoint later, leaving us the other side of the ostensibly
                   locked square, and thus meaning the square isn't actually
                   locked after all. However, this is only relevant if there's
                   an unlocked square on the same side of the crate that it
                   could have arrived via. */
                int d;
                for (d = 0; d < (diagonals ? 8 : 4); d++) {
                    const int dx = xyoffsets[d][0];
                    const int dy = xyoffsets[d][1];

                    /* Check to make sure that the crate's being pushed to an
                       unlocked square. */
                    if (LOCATION_AT(+dx, +dy) != OUTSIDE &&
                        LOCATION_AT(+dx, +dy) != INTERIOR)
                        continue;

                    /* Check to make sure that the crate isn't being pushed
                       from a wall. (This bounds-checks the feed chamber
                       behaviour below.) */
                    if (LOCATION_AT(-dx, -dy) == WALL ||
                        LOCATION_AT(-dx, -dy) == (WALL | LOCKED))
                        continue;

                    /* Pushing from the entrance is a special case (as it
                       doesn't have a real location array entry), and is
                       also obviously always reachable as this could be
                       a newly introduced crate. */
                    if (y - dy < 0) {
                        is_locked = false;
                        break;
                    }

                    /* Pushing from a square reachable from the entrance (even
                       with this crate blocking it) is always fine, whether
                       feed or storage. */
                    if (LOCATION_AT(-dx, -dy) == OUTSIDE ||
                            LOCATION_AT(-dx, -dy) == (OUTSIDE | LOCKED) ||
                        (LOCATION_AT(-dx, -dy) & ANNEX)) {
                        is_locked = false;
                        break;
                    }

                    /* That's it for storage chambers. For feed chambers, we
                       currently play safe and treat the situation as unlocked.
                       TODO: A more accurate check here. */
                    if (!storage) {
                        is_locked = false;
                        break;
                    }
                }

                /* Undo our changes. */
                floodfill(locations, width, height, entrypos, entrypos, 0,
                          OUTSIDE, INTERIOR, true, false, false);
                locations[y * width + x] =
                    is_locked ? (OUTSIDE | LOCKED) : INTERIOR;
                any_locks_found |= is_locked;
            }
        }
    }

    /* Connectivity locks. */
    /* Now that we know where the chokepoint locks are, we can find locations
       accessible from the entrance via crate pulls (that don't go through
       chokepoint-locked areas). */
    floodfill(locations, width, height, entrypos, entrypos, 0, INTERIOR,
              storage ? OUTSIDE | LOCKED : OUTSIDE, false, true, false);
    if (storage) {
        floodfill(locations, width, height, entrypos, entrypos, 0,
                  OUTSIDE | LOCKED, OUTSIDE, false, false, true);
    }

    /* Now change all locked squares to OUTSIDE | LOCKED.  */
    int outside_squares_seen = 0;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (locations[y * width + x] == OUTSIDE)
                outside_squares_seen++;
            else if (locations[y * width + x] != WALL &&
                     !(locations[y * width + x] & ANNEX))
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
