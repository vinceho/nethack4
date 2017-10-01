/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-10-01 */
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

        /* Note: chambers.c sometimes changes ANNEX values in memory directly;
           make sure we keep a stable hash when this happens. */

        if ((locations[i] & ~LOCKED) == CRATE || locations[i] & ANNEX) {
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
init_layout(struct layout *layout, int w, int h, int entrypos, int annexcap,
            bool adjust_lpos)
{
    layout->cratecount = 0;
    layout->maxlpos = OUTSIDE;

    int i;
    for (i = 0; i < w * h; i++) {
        if ((layout->locations[i] & ~LOCKED) == CRATE)
            layout->cratecount++;
        else if ((layout->locations[i] & ~LOCKED) == WALL)
            layout->locations[i] = WALL; /* make sure we have no locked walls */
        else if (layout->locations[i] & ANNEX)
            layout->cratecount += annexcap - (layout->locations[i] & ~ANNEX);
        else if (adjust_lpos)
            layout->locations[i] = /* clear regions */
                (layout->locations[i] & LOCKED) | SENTINEL;
        else if (layout->maxlpos < (layout->locations[i] & ~LOCKED))
            layout->maxlpos = layout->locations[i] & ~LOCKED;
    }

    layout->solution = memdup(&(struct layout_solution){
            .difficulty = 0, .loopgroup = NULL, .known = false,
                .pushes = -1, .nextindex = -1},
        sizeof (struct layout_solution));

    if (adjust_lpos) {
        floodfill(layout->locations, w, h, entrypos, entrypos, 0,
                  SENTINEL, OUTSIDE, true, false, false);
        int y, x;
        lpos curlpos = INTERIOR;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                if ((layout->locations[y * w + x] & ~LOCKED) == SENTINEL)
                    floodfill(layout->locations, w, h, entrypos, x, y, SENTINEL,
                              curlpos++, true, false, false);
        layout->maxlpos = curlpos - 1;
    }
}

/* Checks to see if two sets of locations have the same crates. (An annex
   counts as a group of crates.) */
static bool
same_crates(const lpos *l1, const lpos *l2, int width, int height)
{
    int i;
    for (i = 0; i < width * height; i++) {
        if (((l1[i] & ~LOCKED) == CRATE) != ((l2[i] & ~LOCKED) == CRATE))
            return false;
        if ((l1[i] & ANNEX) && l1[i] != l2[i])
            return false;
    }
    return true;
}

/* Returns the index of the given layout in the given chamber, or -1 if it
   doesn't exist. Only the crate and player positions will be compared, not
   things like walls or regions. */
int
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
            ((y < 0 ? OUTSIDE : l->locations[y * chamber->width + x]) | LOCKED))
            return layoutindex;
    }

    return -1;
}

/* Given a chamber and layout index, adds all the legal moves from that layout
   to the chamber (that weren't already there), and calls the given callback on
   each of the layouts.  */
static void
loop_over_next_moves(struct chamber *chamber, int layoutindex, bool backwards,
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

            int d;
            for (d = 0; d < (diagonals ? 8 : 4); d++) {
                int dx = xyoffsets[d][0];
                int dy = xyoffsets[d][1];

                lpos from;
                lpos to;

                /* When "pushing" from an annex, the destination square is
                   behind the player, rather than behind the annex. */
                if (AT(layout, x + dx, y + dy) & ANNEX) {
                    if (backwards) {
                        to = AT(layout, x + dx, y + dy);
                        from = AT(layout, x - dx, y - dy);
                    } else {
                        from = AT(layout, x + dx, y + dy);
                        to = AT(layout, x - dx, y - dy);
                    }
                } else if (backwards) {
                    to = AT(layout, x + dx, y + dy);
                    from = AT(layout, x + dx * 2, y + dy * 2);
                } else {
                    from = AT(layout, x + dx, y + dy);
                    to = AT(layout, x + dx * 2, y + dy * 2);
                }

                /* Crates can't get onto locked squares, and shouldn't be pushed
                   onto a locked square, and can't be pushed onto another
                   crate, a wall, or an annex with no remaining capacity. */
                if ((from & LOCKED) || (to & LOCKED) || to <= CRATE ||
                    to == (ANNEX | 0))
                    continue;

                /* The simplest case: pushing/pulling a crate or nonempty annex
                   onto a blank space or annex. */
                bool legalpush = false;
                if (from == CRATE || ((from & ANNEX) &&
                                      (from != (ANNEX | chamber->annexcap))))
                    legalpush = true;

                /* When pushing/pulling from outside (y == -1), we can
                   materialize/dematerialize a crate just inside the
                   entrance. When pushing/pulling from inside, we must
                   materialize/dematerialize the crate just /outside/ the
                   entrance (because crates can be pushed along the y == 0
                   line without the y == 1 line being accessible). */
                if (!backwards && y == -1 && dy == 1)
                    legalpush = true;

                if (backwards && y == 1 && dy == -1 &&
                    (x + dx * 2) == chamber->entrypos)
                    legalpush = true;

                if (legalpush) {

                    lpos *working = memdup(layout->locations,
                                           sizeof (lpos) * width * height);

                    if (backwards) {

                        if (from & ANNEX)
                            working[(y + dy * 2) * width + x + dx * 2]++;
                        else if (to & ANNEX)
                            working[(y - dy) * width + x - dx] = OUTSIDE;
                        else if (y + dy * 2 >= 0)
                            working[(y + dy * 2) * width + x + dx * 2] =
                                OUTSIDE;

                        if (to & ANNEX)
                            working[(y + dy) * width + x + dx]--;
                        else if (y != -1)
                            working[(y + dy) * width + x + dx] = CRATE;

                    } else {

                        if (from & ANNEX)
                            working[(y + dy) * width + x + dx]++;
                        else if (y != -1)
                            working[(y + dy) * width + x + dx] = OUTSIDE;

                        if (from & ANNEX)
                            working[(y - dy) * width + x - dx] = CRATE;
                        else if (to & ANNEX)
                            working[(y + dy * 2) * width + x + dx * 2]--;
                        else if (y + dy * 2 >= 0)
                            working[(y + dy * 2) * width + x + dx * 2] = CRATE;

                    }

                    int layoutindex2 = find_layout_in_chamber(
                        chamber, working, x, y);

                    if (layoutindex2 == -1) {

                        struct layout *newlayout =
                            NEW_IN_XARRAY(&(chamber->layouts), struct layout);
                        newlayout->locations = working;
                        init_layout(newlayout, chamber->width, chamber->height,
                                    chamber->entrypos, chamber->annexcap, true);
                        newlayout->playerpos = AT(newlayout, x, y) & ~LOCKED;

                        layouthash hash = hash_layout(newlayout->locations,
                                                      chamber->width,
                                                      chamber->height);
                        *(NEW_IN_XARRAY(chamber->layout_index +
                                        hash % HASHSIZE, int)) =
                            chamber->layouts.length_in_use - 1;

                        layoutindex2 = chamber->layouts.length_in_use - 1;

                    } else {

                        free(working);
                    }

                    callback(chamber, layoutindex2, callbackarg);

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
struct set_difficulty_working {
    struct layout_solution *psol;
    int cratecount;
};
static void
set_difficulties(struct chamber *chamber, int layoutindex, void *parent)
{
    /* The basic idea: every move that goes from one loopgroup to another
       (i.e. is irreversible) increases the difficulty of the 'from' loopgroup
       by the difficulty of the 'to' loopgroup. Exception: We don't count moves
       that insert crates for this purpose (thus possible "beyond capacity"
       moves won't add to difficulty). */

    struct layout_solution *insol = nth_layout(chamber, layoutindex)->solution;
    struct set_difficulty_working *psdw = parent;
    struct layout_solution *psol = psdw ? psdw->psol : NULL;

    /* Avoid infinite recursion. */
    if (insol->known)
        return;
    insol->known = true;

    /* Set the difficulties of all positions reachable from here, and update
       the difficulty of this loopgroup for all loopgroups reachable from it. */
    insol = follow_loopgroup_pointer(insol);
    struct set_difficulty_working sdw = {
        .psol = insol,
        .cratecount = nth_layout(chamber, layoutindex)->cratecount
    };
    loop_over_next_moves(chamber, layoutindex, false, set_difficulties, &sdw);

    /* This will ensure that by the time the recursion ends, all loopgroups
       will have been appropriately updated. */
    if (psol && insol != psol && sdw.cratecount <= psdw->cratecount)
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
        loop_over_next_moves(chamber, layoutindex, false,
                             find_layouts_inner, &fli);

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

struct furthest_layout_queue {
    struct furthest_layout_queue *next;
    int layoutindex;
};

struct flq_wrapper {
    struct furthest_layout_queue *head;
    struct furthest_layout_queue **tailnext;
    int newpushes;
    int mostpushes;
    int mostpushes_index;
    int oldcrates;
    int oldindex;
    int curcrates;
    int target;
    int craterange_l;
    int craterange_h;
};

/* Handles updating the queue for furthest_layout. */
static void
furthest_layout_inner(struct chamber *chamber, int layoutindex, void *qv)
{
    struct flq_wrapper *q = qv;
    struct layout *layout = nth_layout(chamber, layoutindex);

    /* Have we already seen it (and not via a more complex route)? */
    if (layout->solution->pushes != -1 &&
        layout->solution->pushes <= q->newpushes)
        return;

    /* Is the number of crates out of range? */
    if (layout->cratecount < q->craterange_l ||
        layout->cratecount > q->craterange_h)
        return;
    if (q->curcrates <= q->target && layout->cratecount > q->oldcrates)
        return;

    /* No, mark the number of pushes and add it to the queue. */
    layout->solution->pushes = q->newpushes;
    struct furthest_layout_queue *temp = memdup
        (&(struct furthest_layout_queue){
            .next = NULL, .layoutindex = layoutindex}, sizeof *temp);
    *(q->tailnext) = temp;
    q->tailnext = &(temp->next);

    layout->solution->nextindex = q->oldindex;

    /* If we have a number of crates equal to the target, then this is a
       solvable layout with 'target' crates, just presumably one we hadn't seen
       before. Mark the number of pushes as 0 (thus causing us to perhaps
       recheck positions that reach this one). */
    if (layout->cratecount == q->target && layout->playerpos == OUTSIDE)
        layout->solution->pushes = 0;

    /* Is this a new record? */
    else if ((layout->cratecount == q->curcrates || q->curcrates == INT_MAX) &&
             layout->playerpos == OUTSIDE &&
             layout->solution->pushes > q->mostpushes) {
        q->mostpushes = layout->solution->pushes;
        q->mostpushes_index = layoutindex;
    }
}

/* Out of all valid feed layouts with the player outside, and that have
   'curcrates' crates in them, return the index of the one that requires the
   most pushes to reach a solvable layout with 'target' crates and the player
   outside. ('curcrates' can be specified as INT_MAX, if we don't care how many
   crates are used.) This updates 'pushes' and 'nextindex', and leaves the other
   solution fields alone.

   If 'curcrates' is less than or equal to 'target', the solution will not
   involve removing crates from the chamber.

   This function will add feed layouts to the chamber given as an argument. */
int
furthest_layout(struct chamber *chamber, int curcrates, int target)
{
    /* Mark the "pushes" field in all existing layouts, as 0 if it's solvable
       and has 'target' crates, or as -1 otherwise. The marked fields are placed
       into a queue. */
    int i = 0;

    struct flq_wrapper q;
    q.head = NULL;
    q.tailnext = &q.head;
    q.mostpushes_index = -1;
    q.mostpushes = -1;
    q.curcrates = curcrates;
    q.target = target;
    q.craterange_l = (curcrates > target) ? target : curcrates;
    q.craterange_h = (curcrates < target) ? target : curcrates;

    struct layout_solution *solvable =
        follow_loopgroup_pointer(nth_layout(chamber, 0)->solution);

    for (i = 0; i < chamber->layouts.length_in_use; i++) {
        struct layout *layout = nth_layout(chamber, i);

        if (follow_loopgroup_pointer(layout->solution) != solvable ||
            layout->cratecount != target || layout->playerpos != OUTSIDE) {
            layout->solution->pushes = -1;
        } else {
            layout->solution->pushes = 0;
            struct furthest_layout_queue *temp = memdup
                (&(struct furthest_layout_queue){
                    .next = NULL, .layoutindex = i}, sizeof *temp);
            if (curcrates == target)
                q.mostpushes_index = i;
            *q.tailnext = temp;
            q.tailnext = &(temp->next);
        }
    }

    /* Repeatedly consider all pulls from the head of the queue that yield
       layouts with pushes == -1. Update the 'pushes' field of those layouts,
       and place them at the tail of the queue. */
    while (q.head) {
        struct furthest_layout_queue *temp = q.head->next;
        int layoutindex = q.head->layoutindex;
        free(q.head);
        q.head = temp;
        if (!q.head)
            q.tailnext = &q.head;

        q.newpushes = nth_layout(chamber, layoutindex)->solution->pushes + 1;
        q.oldcrates = nth_layout(chamber, layoutindex)->cratecount;
        q.oldindex = layoutindex;

        loop_over_next_moves(chamber, layoutindex, true,
                             furthest_layout_inner, &q);
    }

    return q.mostpushes_index;
}

/* Deallocates everything pointed to by a given layout (but not the layout
   itself, because those are typically stored in xarrays). */
void
free_layout_internals(struct layout *layout)
{
    free(layout->locations);
    free(layout->solution);
}
