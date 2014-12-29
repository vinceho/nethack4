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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

/*********** Typedefs ************/

typedef uint16_t lpos; /* one map square */

/* Why is there no "inline const"? These are global, never change, and are the
   same for each file, so you'd think there'd be a way to get them to work as
   constants or enums, but neither works; "extern const" is not portable with an
   initializer (meaning the constants cannot be inlined), "static const"
   theoretically works but causes a huge number of warnings because gcc rightly
   thinks that local definitions shouldn't be exposed like that, "enum" doesn't
   let us force a particular range (such as unsigned short). Thus, we end up
   using #define. */

#define WALL ((lpos)0)
#define CRATE ((lpos)1)
#define TARGET ((lpos)2)
#define OUTSIDE ((lpos)3)
#define INTERIOR ((lpos)4)

#define SENTINEL ((lpos)32767) /* used as a temporary in some algorithms */

#define LOCKED ((lpos)32768)

typedef uint32_t layouthash;
#define LAYOUTHASH_TOPBIT (((layouthash)1)<<31)

/************ Constants ************/

#define HASHSIZE 523
static const int xyoffsets[8][2] =
    {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

/************ Macros ************/

/* C doesn't have templates. Ths is the next best thing.

   C++ people, read this as a hacky way of implementing a function
   "new_in_array<typename t>(x)". */

#define NEW_IN_XARRAY(x, t)                     \
    (new_in_xarray((x), sizeof (t)),            \
     ((t *)((x)->contents)) + ((x)->length_in_use - 1))


/************ General-purpose types ************/

/* Generic extensible array type. */
struct xarray {
    /* Contents of the array. Meaningless when allocsize == 0, so that we can
       memset an xarray to 0 to initialize it.*/
    void *contents;
    /* Number of elements actually used in the array. */
    size_t length_in_use;
    /* Number of elements for which memory is allocated. */
    size_t allocsize;
};

/************ Sokoban-related types ************/

/* Mutable part of a layout. */
struct layout_solution {
    long long difficulty;              /* number of incorrect solutions */
    struct layout_solution *loopgroup; /* a layout this can reach to and from */
    int pushes;                        /* for the last furthest_layout call */
    bool known;                        /* has 'difficulty' been calculated? */
};

/* One possible layout for a chamber.  This has walls in the same location as
   the chamber itself, but its own crate layout; and it also knows the connected
   region in which the player is (this avoids the need for a separate layout for
   each marble position, which would be much slower).

   The outside wall is just assumed, and not part of the layout structure
   itself.

   A layout must be at least 2x2 for it to be possible to determine which edge
   an edge square is on without tracking additional information.

   Once a layout is created, it is stored via a 'const' pointer, thus forcing
   it to be immutable. */
struct layout {
    /* width, height, entrypos are stored on the chamber itself */

    /* "locations" stores all the locations in row-major format. Walls, crates,
       and entrances have their own values. For floor locations, the value is
       OUTSIDE or more; different connected components have different values,
       with OUTSIDE being the connected component that connects to the outside,
       and INTERIOR being the lowest possible value for connected components
       that don't. LOCKED is bitwise-or'ed into any locations that cannot
       contain a crate; this is an optimization to save having to recalcuate it
       each time. */
    lpos *locations;

    /* "playerpos" stores the location where the player is (OUTSIDE or >=
       INTERIOR); OUTSIDE also includes outside the layout, thus can be used
       even if there is are no actual OUTSIDE squares (say because the entrance
       is blocked by a crate). */
    lpos playerpos; 

    /* Summary information about the layout, to save having to recalculate
       it all the time. */
    lpos maxlpos;     /* the maximum lpos used in locations */
    int cratecount;   /* the number of crates */

    /* Solution information */
    struct layout_solution *solution; /* owned pointer */
};

struct chamber {
    int width, height;

    /* We disallow 7/8 of the entrance positions as a simple method to reduce
       duplication of effort due to symmetry. The entrance is at x = entrypos, y
       = 0; entrypos has to be no more than half the width (rounded up). */
    int entrypos;

    /* For speeding up sorting lists of chambers. */
    int unlocked_squares;

    /* A list of layouts we've found for the chamber. Index 0 is always the base
       layout (which has no crates). */
    struct xarray layouts;

    /* Makes it possible to quickly find a layout in layouts. */
    struct xarray layout_index[HASHSIZE];
};


/************ Globals ************/

extern bool diagonals;      /* are we in diagonals mode? */

/************ Inlines ************/

/*
 * These use C99 "inline" semantics; all files are given a plain "inline"
 * version, and exactly one declares it "extern". This is the opposite of what
 * "extern" does elsewhere in C, and in fact, gcc originally implemented it the
 * other way round (pre-standardisation), leading to some confusion when the
 * standard was released.
 *
 * An easy way to remember how it works in GNU89 and C99:
 *
 * - GNU89: "extern" means "is available elsewhere"; inline functions differ
 *   from regular functions in that being inline, their definition needs to
 *   be available, thus, the weird part is that you get "extern" on a
 *   definition, something that doesn't exist elsewhere.
 *
 * - C99: "extern" means "is defined elsewhere" and is never accompanied by
 *   a definition, whether inline or not; for regular functions, you do this
 *   to use a definition from another file, whereas for inline functions, you
 *   do this to actually generate the version that gets linked against when
 *   the function isn't inlined. The weird part is that you end up with an
 *   "extern" in a source file, and no "extern" in the header file, which is
 *   again something that doesn't exist elsewhere.
 *
 * There's no really good option for the syntax here :-/
 */

#ifdef __GNUC_GNU_INLINE__
#error Your compiler is using nonstandard 'inline' semantics. \
       With gcc, use '-fno-gnu89-inline' to get the standard semantics.
#endif

/* The standard accessor for a fully generated layout. It implements sensible
   values for outside the area, and is inlined because the "if" guards which
   make up most of the function can usually be optimized out.

   The "official" definition is in layout.c. */
inline lpos location_bounds_check(lpos *, int, int, int, int, int);
inline lpos
location_bounds_check(lpos *locations, int x, int y,
                      int width, int height, int entrypos)
{
    if (y < 0 && x == entrypos)
        return OUTSIDE;
    if (x < -3 || y < -3 || x >= width + 3 || y >= height + 3)
        return OUTSIDE;
    if (x < 0 || y < 0 || x >= width || y >= height)
        return WALL;
    return locations[y * width + x];
}

/* Finding a layout from a chamber. This is done in lots of places, and a little
   nontrivial to write, so is a good candidate for an inline function. */
inline struct layout *nth_layout(const struct chamber *, int);
inline struct layout *
nth_layout(const struct chamber *chamber, int layoutindex) {
    return ((struct layout *)chamber->layouts.contents) + layoutindex;
}


/************ Externs ************/

/* memory.c */

/* xarray allocation/deallocation */
extern void alloc(struct xarray *, size_t, size_t);
extern size_t new_in_xarray(struct xarray *, size_t);

/* other allocation (this deallocates with free */
extern void *memdup(void *, size_t);

/* locks.c */

extern void floodfill(lpos *, int, int, int, int, int,
                      lpos, lpos, bool, bool, bool);
extern bool init_wall_locks(lpos *, int, int, int, bool);

/* chamber.c */

extern void generate_chambers(struct xarray *, int, int, int, bool,
                              int (*)(int));
extern void free_chamber_internals(struct chamber *);
extern struct chamber *generate_storage_chamber(long long, int (*)(int));

/* layout.c */

extern layouthash hash_layout(const lpos *, int, int);
extern void init_layout(struct layout *, int, int, int, bool);
extern void find_layouts_from(struct chamber *, int);
extern int max_capacity_layout(const struct chamber *);
extern int furthest_layout(struct chamber *, int, int);
extern void free_layout_internals(struct layout *);

/* output.c */

extern void output_layouts(const struct chamber *, size_t,
                           bool, bool, FILE *);
extern void output_one_layout(const struct chamber *, int,
                              bool, bool, FILE *);
extern void output_two_layouts(const struct chamber *, int, int,
                               bool, bool, FILE *);
extern void output_chambers(const struct chamber *, size_t,
                            bool, bool, FILE *);
