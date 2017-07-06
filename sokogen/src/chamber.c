/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-07-07 */
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
    /* Note: not x >= width; that triggers a bug in a gcc warning (some inlined
       versions of this function end up looking like (width - k >= width) which
       looks like a buggy check for integer overflow) */
    if (x < 0 || (x - width) >= 0 || yrel * width + squares_generated < 0)
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
    struct xarray *, int, int, int, lpos *, int, lpos, bool, int(*)(int));

static int
compare_unlocked_squares(const void *chamber1, const void *chamber2)
{
    return ((struct chamber *)chamber2)->unlocked_squares -
        ((struct chamber *)chamber1)->unlocked_squares;
}

static int
all_chambers_rng(int x)
{
    return x - 1;
}

/* Generates all the possible chambers of particular dimensions and connection
   pattern, restricting itself to chambers that are actually useful (chambers
   will be omitted if they contain disconneted areas, and might be omitted if
   they decompose into smaller chambers or contain locked areas larger than a
   single tile wide, although both the latter cases are not checked reliably to
   save time). A chamber is specified by its crate-less layout.

   If 'storage' is true, then only chambers which make sense as storage chambers
   will be generated; otherwise, feed chambers will be generated too.

   The given RNG, if present and non-NULL will be used to randomly reject a
   subset of chambers, to reduce memory usage. In this case, at most 16 chambers
   will be returned, biased towards floor rather than wall. */
void
generate_chambers(struct xarray *chambers, int width, int height, int entrypos,
                  bool storage, int (*rng)(int))
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
                          working + width, 0, INTERIOR, storage,
                          rng ? rng : all_chambers_rng);

    qsort(chambers->contents, chambers->length_in_use,
          sizeof (struct chamber), compare_unlocked_squares);
}

/* Recursive helper function for generate_chambers. */
static void
generate_chamber_line(struct xarray *chambers, int width, int squares,
                      int entrypos, lpos *working, int squares_generated,
                      lpos highest_cnumber_used, bool storage,
                      int (*rng)(int))
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
        if (init_wall_locks(locations, width, squares / width,
                            entrypos, storage))
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
        newchamber->annexcap = 0;
        newchamber->layouts = (struct xarray){0, 0, 0};
        memset(newchamber->layout_index, 0, sizeof newchamber->layout_index);

        newchamber->unlocked_squares = 0;
        for (x = 0; x < squares; x++)
            if (locations[x] > WALL && !(locations[x] & LOCKED))
                newchamber->unlocked_squares++;

        struct layout *baselayout =
            NEW_IN_XARRAY(&(newchamber->layouts), struct layout);

        baselayout->playerpos = OUTSIDE;
        baselayout->locations = locations;
        init_layout(baselayout, width, squares / width, entrypos, 0, false);

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
       check it afterwards.

       We generate only a random subset of lines, to save memory, favouring more
       open areas. The size of the subset is proportional to the width. */

    unsigned long cpattern;
    bool near_goal = (squares - squares_generated <= width * 2);

    for (cpattern = rng(1UL << (width - 1)) + (1UL << (width - 1));
         cpattern < ULONG_MAX;
         cpattern = rng((cpattern + 3) / 2) + cpattern / 2 - 1) {

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
                              current_cnumber, storage, rng);

        if (rng != all_chambers_rng && chambers->length_in_use >= 16)
            break;
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

/* Generates a directed chamber with the given required storage capacity, and
   an appropriate layout for it. */
struct chamber *
generate_directed_chamber(int capacity, int (*rng)(int), int *layoutindex)
{
    int width = 4;
    int height = 3;
    int entrypos = 1;

    struct chamber *rv = NULL;

    while (!rv) {
        struct xarray chambers = {0, 0, 0};

        /* Randomly increase/reset the height and/or width. */
        if (rng(2)) {
            width++;
            if (width < height)
                height = width;
        } else {
            height++;
            if (height < width)
                width = height;
        }
        entrypos = rng((width - 1) / 2) + 1;

        if (width * height > 30) {
            /* This is too large to calculate in a reasonable time. */
            width = 4;
            height = 3;
            entrypos = 1;
        }

        /* Add some more randomization to the sizes we choose. */
        if (!rng(2))
            continue;

        /* Generate a random selection of chambers of this size, for us to work
           from. We request them to be valid storage chambers, because that
           increases the chance of the right sort of structure, even though
           we'll be using them more like feed chambers (storage chambers are a
           subset of feed chambers). */
        while (!chambers.length_in_use)
            generate_chambers(&chambers, width, height, entrypos, true, rng);

        /* For each of the chambers, add an annex in the positive-y direction.
           The idea is that if a feed chamber is solvable, but not solvable if
           the annex is slightly blocked, it must be because the annex is needed
           for temporary crate storage purposes. */
        int i;
        for (i = 0; i < chambers.length_in_use; i++) {
            struct chamber *chamber = ((struct chamber *)chambers.contents) + i;
            struct layout *layout = nth_layout(chamber, 0);

            chamber->annexcap = capacity;
            chamber->height++;
            const lpos wall = WALL;
            lpos *locations = padrealloc(layout->locations, sizeof (lpos),
                                         chamber->height * chamber->width,
                                         (chamber->height - 1) * chamber->width,
                                         &wall);
            layout->locations = locations;

            /* Pick a random place to place the annex. */
            int odds = 1;
            int chosen = -1;
            int x;
            for (x = 1; x < chamber->width - 1; x++) {
                /* To make these chambers less repetitive, disallow the most
                   commonly chosen case. */
                if (chamber->height <= 5 && chamber->width == 5 &&
                    x == 2)
                    continue;

                if (locations[(chamber->height - 3) * chamber->width + x] ==
                    OUTSIDE && !rng(odds++))
                    chosen = x;
            }
            if (chosen == -1)
                continue; /* nowhere to place the annex */

            /* Build the annex. */
            locations[(chamber->height - 2) * chamber->width + chosen] =
                OUTSIDE;
            locations[(chamber->height - 1) * chamber->width + chosen] =
                (ANNEX | capacity);

            /* We changed the layout, so recalculate the locks. */
            init_wall_locks(locations, chamber->width, chamber->height,
                            chamber->entrypos, false);

            /* There's no point in continuing if the annex is locked. */
            if (locations[(chamber->height - 2) * chamber->width + chosen] &
                LOCKED)
                continue;

            /* Generate feed chambers with an annex that already has one
               crate in it. */
            locations[(chamber->height - 1) * chamber->width + chosen]--;
            (void) furthest_layout(chamber, INT_MAX, 0);
            layout = nth_layout(chamber, 0); /* may have been realloced */

            int n_reduced_capacity = chamber->layouts.length_in_use;

            /* Generate feed chambers with a full capacity annex. */
            locations[(chamber->height - 1) * chamber->width + chosen]++;
            (void) furthest_layout(chamber, INT_MAX, 0);
            layout = nth_layout(chamber, 0); /* may have been realloced */

            /* Look for layouts with a full-capacity, empty, OUTSIDE annex and
               with the player outside, that have no corresponding layout with a
               pre-occupied annex. Tiebreak by most pushes. */
            int pushcount = 0;
            *layoutindex = 0;
            for (x = 0; x < chamber->layouts.length_in_use; x++) {
                struct layout *l2 = nth_layout(chamber, x);

                /* Check tiebreaker, player position. */
                if (l2->solution->pushes <= pushcount ||
                    l2->playerpos != OUTSIDE)
                    continue;

                /* Check that the annex is empty and OUTSIDE. */
                if (l2->locations[(chamber->height - 1) *
                                  chamber->width + chosen] !=
                    (ANNEX | capacity) ||
                    l2->locations[(chamber->height - 2) *
                                  chamber->width + chosen] != OUTSIDE)
                    continue;

                /* Check that the corresponding layout with a pre-occupied annex
                   is unsolvable. */
                lpos blocked[chamber->height * chamber->width];
                memcpy(blocked, l2->locations, sizeof blocked);
                blocked[(chamber->height - 1) * chamber->width + chosen]--;
                int otherlayout = find_layout_in_chamber(
                    chamber, blocked, chamber->entrypos, 0);

                if (otherlayout != -1 && otherlayout < n_reduced_capacity)
                    continue;

                pushcount = l2->solution->pushes;
                *layoutindex = x;
            }

            if (*layoutindex)
                rv = memdup(chamber, sizeof *chamber);
            break;
        }

        /* Free the chambers (apart from rv, if that's been chosen). */
        size_t c;
        for (c = 0; c < chambers.length_in_use; c++)
            if (!rv || c != i)
                free_chamber_internals(((struct chamber *)chambers.contents) + c);
        if (chambers.allocsize)
            free(chambers.contents);
    }

    return rv;
}

/* Generates a storage chamber (layoutindex == NULL) or feed chamber (placing
   the index of the layout whose difficulty was assessed in *layoutindex) of at
   least the given difficulty. The resulting chamber will be malloc-allocated.
   Keep the difficulty smallish if you want this to terminate in a reasonable
   time (or at all; after taking too long, the function restarts from the start,
   so it'll never reach the higher difficulties). */
struct chamber *
generate_difficult_chamber(long long difficulty, int (*rng)(int),
                           int *layoutindex)
{
    int width = 4;
    int height = 3;
    int entrypos = 1;

    struct chamber *rv = NULL;

    while (!rv) {
        struct xarray chambers = {0, 0, 0};

        /* Generate a random selection of chambers of this size, for us to work
           from. */
        while (!chambers.length_in_use)
            generate_chambers(&chambers, width, height, entrypos,
                              !layoutindex, rng);

        int maxchambers = chambers.length_in_use;
        int chamberindex = rng(4);
        if (chamberindex >= maxchambers)
            chamberindex = maxchambers - 1;

        while (!rv) {
            struct chamber *chamber = ((struct chamber *)chambers.contents) +
                chamberindex;

            /* Find all appropriate layouts for this chamber. */
            if (!layoutindex) {
                find_layouts_from(chamber, 0);
            } else {
                *layoutindex = furthest_layout(chamber, INT_MAX, 0);
                if (*layoutindex != -1)
                    find_layouts_from(chamber, *layoutindex);
            }

            long long cdiff;

            if (layoutindex && *layoutindex == -1)
                cdiff = -1;
            else
                cdiff = nth_layout(chamber, layoutindex ? *layoutindex : 0)
                    ->solution->difficulty;

            if (difficulty <= cdiff && difficulty * 10 > cdiff) {
                rv = memdup(chamber, sizeof *chamber);
                break;
            } else {
                /* Do something to find another chamber:
                   - increase the width or height; or
                   - try a chamber the same size with more free space */
                chamberindex += rng(4);
                if (chamberindex >= maxchambers) {
                    if (rng(2)) {
                        width++;
                        if (width < height)
                            height = width;
                    } else {
                        height++;
                        if (height < width)
                            width = height;
                    }
                    entrypos = rng((width - 1) / 2) + 1;

                    if (width * height > 32) {
                        /* This is too large to calculate in a reasonable
                           time / using reasonable memory. */
                        width = 4;
                        height = 3;
                        entrypos = 1;
                    }
                    break;
                }
            }
        }

        /* Free the chambers (apart from rv, if that's been chosen). */
        size_t i;
        for (i = 0; i < chambers.length_in_use; i++)
            if (!rv || i != chamberindex)
                free_chamber_internals(((struct chamber *)chambers.contents) + i);
        if (chambers.allocsize)
            free(chambers.contents);
    }

    return rv;
}

/* Generates a storage chamber, and finds a layout for it that can store an
   extra 'remcap' crates. */
struct chamber *
generate_remcap_chamber(long long difficulty, int remcap,
                        int (*rng)(int), int *layoutindex)
{
    bool found = false;
    struct chamber *chamber;
    while (!found) {
        chamber = generate_difficult_chamber(difficulty, rng, NULL);

        int maxcap =
            nth_layout(chamber, max_capacity_layout(chamber))->cratecount;

        found = maxcap >= remcap;

        if (found) {
            *layoutindex = furthest_layout(chamber, maxcap - remcap, maxcap);
            /* there must be some valid layout, because we already checked
               that the capacity is OK */
            assert(*layoutindex > -1);
        } else {
            /* If we need a high capacity and have a low difficulty, we
               might not be able to find a pattern with enough capacity.
               So, we allow the difficulty to steadily increase. */
            difficulty += (difficulty / 5) + 1;
        }
    }

    return chamber;
}


/* Creates a new chamber from the given layouts of two other chambers. The
   entrance to chamber1 will overwrite the annex of chamber2 (which must
   exist). The player will be placed outside. */
struct chamber *
glue_chambers(const struct chamber *chamber1, int layoutindex1,
              const struct chamber *chamber2, int layoutindex2)
{
    int annexpos;
    lpos *loc1 = nth_layout(chamber1, layoutindex1)->locations;
    lpos *loc2 = nth_layout(chamber2, layoutindex2)->locations;

    /* Find the annex. */
    for (annexpos = 0; annexpos < chamber2->width; annexpos++)
        if (loc2[(chamber2->height - 1) * chamber2->width + annexpos] & ANNEX)
            goto found_annex;
    assert(!"No annex in chamber2");
found_annex:
    ;

    /* Calculate the padding we need to use to line up the entrance of chamber1
       with the annex of chamber2 (by adding extra wall to the left). */
    int leftpad1 = 0;
    int leftpad2 = 0;
    if (annexpos < chamber1->entrypos)
        leftpad2 = chamber1->entrypos - annexpos;
    if (annexpos > chamber1->entrypos)
        leftpad1 = annexpos - chamber1->entrypos;

    int new_height = chamber1->height + chamber2->height;
    int new_width = chamber1->width + leftpad1;
    if (chamber2->width + leftpad2 > new_width)
        new_width = chamber2->width + leftpad2;

    lpos wall = WALL;
    lpos *locations = padrealloc(NULL, sizeof (lpos),
                                 new_height * new_width, 0, &wall);

    /* Copy the chamber contents. */
    int y, x;
    for (y = 0; y < chamber2->height; y++)
        for (x = 0; x < chamber2->width; x++)
            locations[y * new_width + x + leftpad2] =
                loc2[y * chamber2->width + x];

    for (y = 0; y < chamber1->height; y++)
        for (x = 0; x < chamber1->width; x++)
            locations[(y + chamber2->height) * new_width + x + leftpad1] =
                loc1[y * chamber1->width + x];

    locations[(chamber2->height - 1) * new_width + annexpos + leftpad2] =
        OUTSIDE;

    /* locations holds the layout with crates, and will become layout 1. We
       also need a layout 0; we can use init_wall_locks to clear the crates. */
    lpos *baselocations = memdup(
        locations, new_height * new_width * sizeof(lpos));
    init_wall_locks(baselocations, new_width, new_height,
                    chamber2->entrypos + leftpad2, false);

    struct chamber *newchamber = memdup(&(struct chamber) {
            .width = new_width, .height = new_height,
                .entrypos = chamber2->entrypos + leftpad2, .annexcap = 0},
        sizeof *newchamber);
    memset(newchamber->layout_index, 0, sizeof newchamber->layout_index);

    /* Be careful of reallocations... */
    (void) NEW_IN_XARRAY(&(newchamber->layouts), struct layout);
    struct layout *layout1 =
        NEW_IN_XARRAY(&(newchamber->layouts), struct layout);
    struct layout *layout0 = nth_layout(newchamber, 0);

    layout0->playerpos = layout1->playerpos = OUTSIDE;
    layout0->locations = baselocations;
    layout1->locations = locations;

    init_layout(layout0, new_width, new_height, newchamber->entrypos, 0, true);
    init_layout(layout1, new_width, new_height, newchamber->entrypos, 0, true);

    return newchamber;
}
