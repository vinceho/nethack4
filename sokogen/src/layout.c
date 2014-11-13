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

/* For variables, you define them "extern" everywhere, plain in one location.
   For inline functions, you define them "inline" everywhere, "extern" in one
   location. */

extern lpos location_bounds_check(lpos *, int, int, int, int, int);
extern struct layout *nth_layout(const struct chamber *, int);

/* Associates a number with each layout, for easily finding it in a list of
   layouts. The algorithm is to use one bit for each of the first few squares,
   and when we run out of bits, use a great many bits for the others so as to
   minimize the chance of clashes in puzzles involving a small number of
   crates.

   Exception: a layout with no crates always hashes to 0, a layout with crates
   will be given a hash of (layouthash)(-1) if it would otherwise hash to 0. */
layouthash
hash_layout(const lpos *locations, int w, int h)
{
    layouthash hash = 0;
    int i;
    int crates = 0;

    for (i = 0; i < w*h; i++) {

        if (hash & LAYOUTHASH_TOPBIT)
            hash ^= 0x8010844BLU;

        hash <<= 1;

        if ((locations[i] & ~LOCKED) == CRATE) {
            hash ^= 1;
            crates++;
        }
    }

    if (crates && !hash)
        hash = (layouthash)(-1);

    return hash;
}

/* Given a layout with only locations set and playerpos possibly set, sets the
   other fields appropriately (playerpos will not be read nor written). The
   caller is expected to have marked wall locks and the like; this function will
   not mark new squares as locked, but will redo the OUTSIDE/INTERIOR if
   adjust_lpos is set. */
void
init_layout(struct layout *layout, int w, int h, int entrypos, bool adjust_lpos)
{
    layout->cratecount = 0;
    layout->maxlpos = OUTSIDE;

    int i;
    for (i = 0; i < w * h; i++) {
        if ((layout->locations[i] & ~LOCKED) == CRATE)
            layout->cratecount++;
        else if ((layout->locations[i] & ~LOCKED) == WALL)
            layout->locations[i] = WALL; /* make sure we have no locked walls */
        else if (adjust_lpos)
            layout->locations[i] = /* clear regions */
                (layout->locations[i] & LOCKED) | SENTINEL;
        else if (layout->maxlpos < (layout->locations[i] & ~LOCKED))
            layout->maxlpos = layout->locations[i] & LOCKED;
    }

    layout->solution = memdup(&(struct layout_solution){
            .difficulty = 0, .loopgroup = NULL, .known = false},
        sizeof (struct layout_solution));

    if (adjust_lpos) {
        floodfill(layout->locations, w, h, entrypos, 0,
                  SENTINEL, OUTSIDE, true, false);
        int y, x;
        lpos curlpos = INTERIOR;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                if ((layout->locations[y * w + x] & ~LOCKED) == SENTINEL)
                    floodfill(layout->locations, w, h, x, y, SENTINEL,
                              curlpos++, true, false);
        layout->maxlpos = curlpos - 1;
    }
}

/* Checks to see if two sets of locations have the same crates. */
static bool
same_crates(const lpos *l1, const lpos *l2, int width, int height)
{
    int i;
    for (i = 0; i < width * height; i++) {
        if (((l1[i] & ~LOCKED) == CRATE) !=
            ((l2[i] & ~LOCKED) == CRATE))
            return false;
    }
    return true;
}

/* Returns the index of the given layout in the given chamber, or -1 if it
   doesn't exist. Only the crate and player positions will be compared, not
   things like walls or regions. */
static int
find_layout_in_chamber(const struct chamber *chamber, const lpos *locations,
                       int x, int y)
{
    layouthash hash = hash_layout(locations, chamber->width, chamber->height);
    if (!hash)
        return 0;   /* the first layout has no crates */

    const struct xarray *ix = &(chamber->layout_index[hash % HASHSIZE]);

    if (!ix)
        return -1;  /* no layouts have this hash */

    int i;
    for (i = 0; i < ix->length_in_use; i++) {
        int layoutindex = ((int *)(ix->contents))[i];
        struct layout *l = nth_layout(chamber, layoutindex);
        if (same_crates(l->locations, locations,
                        chamber->width, chamber->height) &&
            (l->playerpos | LOCKED) ==
            (l->locations[y * chamber->width + x] | LOCKED))
            return layoutindex;
    }

    return -1;
}

/* Given a chamber and layout index, adds all the legal moves from that layout
   to the chamber (that weren't already there), and calls the given callback on
   each of the layouts.  */
static void
loop_over_next_moves(struct chamber *chamber, int layoutindex,
                     void (*callback)(
                         struct chamber *chamber, int layoutindex, void *arg),
                     void *callbackarg)
{
    struct layout *layout = nth_layout(chamber, layoutindex);
    const int height = chamber->height;
    const int width = chamber->width;

#define AT(l2, x2, y2) location_bounds_check(                           \
        (l2)->locations, (x2), (y2), width, height, chamber->entrypos)

    /* Loop over the various locations for the player, looking for crates that
       could be pushed. */
    int y, x;
    for (y = -1; y < height; y++)
        for (x = 0; x < width; x++) {

            if (y == -1 && x != chamber->entrypos)
                continue;
            if ((AT(layout, x, y) & ~LOCKED) != (layout->playerpos & ~LOCKED))
                continue;

            static const int xyoffsets[4][2] =
                {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

            int d;
            for (d = 0; d < 4; d++) {
                int dx = xyoffsets[d][0];
                int dy = xyoffsets[d][1];

                lpos next = AT(layout, x + dx, y + dy);
                lpos beyond = AT(layout, x + dx * 2, y + dy * 2);

                if (((next & ~LOCKED) == CRATE || (y == -1 && dy == 1)) &&
                    (beyond & ~LOCKED) > CRATE && !(beyond & LOCKED)) {

                    lpos *working = memdup(layout->locations,
                                           sizeof (lpos) * width * height);

                    /* We can push a crate onto or out of the map. When pushing
                       a crate onto the map, the position it's conceptually
                       pushed from (just inside the entrypos) is in-bounds, so
                       we don't need to guard it. When pushing it out, though,
                       it's being pushed out of bounds, so we need a bounds
                       check. */

                    working[(y + dy) * width + x + dx] =
                        (next & LOCKED) | OUTSIDE;
                    if (y + dy * 2 >= 0) {
                        working[(y + dy * 2) * width + x + dx * 2] =
                            (beyond & LOCKED) | CRATE;
                    }

                    int layoutindex2 = find_layout_in_chamber(
                        chamber, working, x + dx, y + dy);

                    if (layoutindex2 == -1) {

                        struct layout *newlayout =
                            NEW_IN_XARRAY(&(chamber->layouts), struct layout);
                        newlayout->locations = working;
                        init_layout(newlayout, chamber->width, chamber->height,
                                    chamber->entrypos, true);
                        newlayout->playerpos = AT(newlayout, x + dx, y + dy);

                        layouthash hash = hash_layout(newlayout->locations,
                                                      chamber->width,
                                                      chamber->height);
                        *(NEW_IN_XARRAY(chamber->layout_index +
                                        hash % HASHSIZE, int)) =
                            chamber->layouts.length_in_use - 1;

                        callback(chamber, chamber->layouts.length_in_use - 1,
                                 callbackarg);

                    } else {

                        free(working);
                        callback(chamber, layoutindex2, callbackarg);

                    }

                    /* we might have reallocated chamber->layouts, or the
                       callback might have done */
                    layout = nth_layout(chamber, layoutindex);
                }
            }
        }
#undef AT
}

/* For efficiency, the rule for a 'loopgroup' pointer is that it must point to
   some other element in the group, DAG-style, and iteratively following the
   pointer eventually reaches the same structure for every member of the group.
   This function finds that member (and also updates the pointers as it goes,
   in order to make the next lookup faster). */
static struct layout_solution *
follow_loopgroup_pointer(struct layout_solution *ls)
{
    if (!ls->loopgroup)
        return ls;

    struct layout_solution *ls2 = follow_loopgroup_pointer(ls->loopgroup);
    ls->loopgroup = ls2;
    return ls2;
}

/* The rules for updating the "difficulty" fields of a layout. */
static void
set_difficulties(struct chamber *chamber, int layoutindex, void *parent)
{
    /* The basic idea: every move that goes from one loopgroup to another
       (i.e. is irreversible) increases the difficulty of the 'from' loopgroup
       by the difficulty of the 'to' loopgroup. */

    struct layout_solution *insol = nth_layout(chamber, layoutindex)->solution;
    struct layout_solution *psol  = parent;

    /* Avoid infinite recursion. */
    if (insol->known)
        return;
    insol->known = true;

    /* Set the difficulties of all positions reachable from here, and update
       the difficulty of this loopgroup for all loopgroups reachable from it. */
    insol = follow_loopgroup_pointer(insol);
    loop_over_next_moves(chamber, layoutindex, set_difficulties, &sdw);

    /* This will ensure that by the time the recursion ends, all loopgroups
       will have been appropriately updated. */
    if (psol && insol != psol)
        psol->difficulty += insol->difficulty;
}

struct fli_element {
    struct fli_element *next;
    int layoutindex;
};

/* Like find_layouts_from, but takes an extra argument that is a linked list of
   layouts that are known to be able to reach the given layouts. All layouts
   reachable from this chamber will have their 'loopgroup' set appropriately.
   'difficulty'/'known' will be reset to 1/false. */
static void
find_layouts_inner(struct chamber *chamber, int layoutindex, void *looplist)
{
    /* If this layout is on the looplist, then all layouts after it on the
       looplist must belong to the same loopgroup as it. */
    struct fli_element *fe = looplist;
    while (fe) {
        if (fe->layoutindex == layoutindex) {
            struct layout_solution *lls =
                nth_layout(chamber, layoutindex)->solution;
            lls = follow_loopgroup_pointer(lls);
            fe = looplist;
            while (fe->layoutindex != layoutindex) {
                struct layout_solution *ls =
                    nth_layout(chamber, fe->layoutindex)->solution;
                ls = follow_loopgroup_pointer(ls);
                if (ls != lls)
                    ls->loopgroup = lls;
                fe = fe->next;
            }
            return;
        }
        fe = fe->next;
    }

    struct fli_element fli = {.next = looplist, .layoutindex = layoutindex};

    struct layout_solution *lls = nth_layout(chamber, layoutindex)->solution;

    /* This layout isn't on the looplist. Do a search for layouts reachable from
       it, unless it has a difficulty > 0 (in which case it's already been
       processed). */
    if (lls->difficulty == 0)
        loop_over_next_moves(chamber, layoutindex, find_layouts_inner, &fli);

    lls->difficulty = 1;
    lls->known = false;
}

/* Finds all layouts accessible from the given layout (specified as a chamber +
   index), and adds them to the given chamber. All the layouts will have
   'difficulty' and 'loopgroup' set appropriately. */
void
find_layouts_from(struct chamber *chamber, int layoutindex)
{
    /* Set the difficulties of the existing layouts to 0. (This lets us use the
       "difficulty" value to see if a layout has been fully processed, as an
       optimization.) */
    int i;
    for (i = 0; i < chamber->layouts.length_in_use; i++) {
        struct layout *layout = nth_layout(chamber, i);
        layout->solution->difficulty = 0;
    }

    /* Ensure that all layouts accessible from the given layout exist, reset
       their solution stats, and set their loopgroups. */
    find_layouts_inner(chamber, layoutindex, NULL);

    /* Set the difficulties of the loopgroups. */
    set_difficulties(chamber, layoutindex, NULL);

    /* Set the difficulties of the layouts to match the loopgroups. */
    for (i = 0; i < chamber->layouts.length_in_use; i++) {
        struct layout *layout = nth_layout(chamber, i);
        layout->solution->difficulty =
            follow_loopgroup_pointer(layout->solution)->difficulty;
    }
}

/* Out of all valid storage layouts with the player outside, return the index of
   the one with the highest capacity. */
int
max_capacity_layout(const struct chamber *chamber)
{
    struct layout_solution *solvable =
        follow_loopgroup_pointer(nth_layout(chamber, 0)->solution);
    int maxcap = 0;
    int maxcap_index = 0;
    int i = 0;

    for (i = 1; i < chamber->layouts.length_in_use; i++) {
        struct layout *layout = nth_layout(chamber, i);
        if (layout->playerpos != OUTSIDE)
            continue;
        if (follow_loopgroup_pointer(layout->solution) != solvable)
            continue;
        if (layout->cratecount > maxcap) {
            maxcap = layout->cratecount;
            maxcap_index = i;
        }
    }
    return maxcap_index;
}
