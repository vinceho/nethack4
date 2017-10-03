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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

/*********** Typedefs ************/

typedef uint16_t lpos; /* one map square while generating */

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

#define SENTINEL ((lpos)16383) /* used as a temporary in some algorithms */

#define ANNEX  ((lpos)16384)   /* representation of connected storage */
#define LOCKED ((lpos)32768)

typedef uint32_t layouthash;
#define LAYOUTHASH_TOPBIT (((layouthash)1)<<31)

typedef uint8_t ppos; /* one map square in a completed puzzle */

/* The bottom 2 bits determine what's on the "ground" of the square. The others
   are single bits that can be combined with them. Note that some code assumes
   that PP_FLOOR is 0, so that floor can be changed to other sorts of things via
   bitwise operations. */
#define PP_FLOOR    ((ppos)0x00)
#define PP_WALL     ((ppos)0x01)
#define PP_TARGET   ((ppos)0x02)
#define PP_EXIT     ((ppos)0x03)
#define PP_PLAYER   ((ppos)0x04)
#define PP_CRATE    ((ppos)0x08)

#define PP_GROUNDMASK ((ppos)0x03)

/************ Constants ************/

#define HASHSIZE 523
#define CONNMAX 4

/* the order here is significant; some files care about orthogonals before
   diagonals, output.c cares about this order specifically */
static const int xyoffsets[8][2] =
    {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

/************ Macros ************/

/* C doesn't have templates. Ths is the next best thing.

   C++ people, read this as a hacky way of implementing a function
   "new_in_xarray<typename t>(x)". */

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

/************ Types used while generating puzzles ************/

/* Mutable part of a layout. */
struct layout_solution {
    long long difficulty;              /* number of incorrect solutions */
    struct layout_solution *loopgroup; /* a layout this can reach to and from */
    int pushes;                        /* for the last furthest_layout call */
    int nextindex;                     /* for tracing the solution */
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

/* Structure used to represent a chamber while generating the chamber. (After
   generation, the chamber is converted to a puzzle-subset and stored in a
   struct puzzlerect.) To keep these smaller, the chamber is surrounded
   implicitly by walls (except for a gap at x = entrypos, y = -1). */
struct chamber {
    int width, height;

    /* We disallow 7/8 of the entrance positions as a simple method to reduce
       duplication of effort due to symmetry. The entrance is at x = entrypos, y
       = 0; entrypos has to be no more than half the width (rounded up). */
    int entrypos;

    /* The capacity of this chamber's annex. For directed chambers. */
    int annexcap;

    /* For speeding up sorting lists of chambers. */
    int unlocked_squares;

    /* A list of layouts we've found for the chamber. Index 0 is always the base
       layout (which has no crates). */
    struct xarray layouts;

    /* Makes it possible to quickly find a layout in layouts. */
    struct xarray layout_index[HASHSIZE];
};

/************ Types used for composing puzzles ************/

/* Coordinates within a puzzle. */
struct coord {
    int x, y;
};

/* A rectangular subset of a puzzle. This includes an entire puzzle as a
   special case. Unlike with chambers, these do not have an implicit boundary;
   the external wall must be included explicitly in the puzzle. There's an
   invariant that the puzzle is /always/ bounded by wall; however, "connections"
   can be specified, which are locations where holes can be punched in the wall
   in order to connect two puzzles together. (Unlike with a chamber, which
   always has just the entrypos as a connection, a puzzlerect can have anywhere
   frome 0 to CONNMAX connections inclusive.)

   These structures are always dynamically allocated. */
struct puzzlerect {
    int top, left;              /* where is the rectangle? */
    int width, height;          /* size of the rectangle */
    struct coord conn[CONNMAX]; /* places where connections can be made */
    int conncount;              /* 0 to CONNMAX */
    ppos data[];                /* data in row-major format */
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

/* Bounds-checking accessor for a puzzlerect.

   The "official" definition is in puzzle.c. */
inline ppos data_bounds_check(const struct puzzlerect *, int, int);
inline ppos
data_bounds_check(const struct puzzlerect *puzzle, int x, int y)
{
    if (x < 0 || y < 0 || x >= puzzle->width || y >= puzzle->height)
        return PP_WALL;
    return puzzle->data[y * puzzle->width + x];
}


/************ Externs ************/

/* memory.c */

/* xarray allocation/deallocation */
extern void alloc(struct xarray *, size_t, size_t);
extern size_t new_in_xarray(struct xarray *, size_t);

/* other allocation (this deallocates with free) */
extern void *memdup(void *, size_t);
extern void *padrealloc(void *, size_t, size_t, size_t, const void *);

/* locks.c */

extern void floodfill(lpos *, int, int, int, int, int,
                      lpos, lpos, bool, bool, bool);
extern bool init_wall_locks(lpos *, int, int, int, bool);

/* chamber.c */

extern void generate_chambers(struct xarray *, int, int, int, bool, bool,
                              int (*)(int));
extern void free_chamber_internals(struct chamber *);
extern struct chamber *generate_difficult_chamber(
    long long, int (*)(int), int *, bool);
extern struct chamber *generate_directed_chamber(int, int (*)(int), int *);
extern struct chamber *generate_remcap_chamber(
    long long, int, int (*)(int), int *);
extern struct chamber *glue_chambers(const struct chamber *, int,
                                     const struct chamber *, int);

/* layout.c */

extern layouthash hash_layout(const lpos *, int, int);
extern void init_layout(struct layout *, int, int, int, int, bool);
extern int find_layout_in_chamber(const struct chamber *, const lpos *,
                                  int, int);
extern void find_layouts_from(struct chamber *, int);
extern int max_capacity_layout(const struct chamber *);
extern int furthest_layout(struct chamber *, int, int);
extern void free_layout_internals(struct layout *);

/* output.c */

extern void output_puzzles(struct puzzlerect *const *,
                           int, int, FILE *);
extern void output_layouts(const struct chamber *, size_t,
                           bool, bool, FILE *);
extern void output_one_layout(const struct chamber *, int,
                              bool, bool, bool, int, FILE *);
extern void output_two_layouts(const struct chamber *, int, int,
                               bool, bool, FILE *);
extern void output_chambers(const struct chamber *, size_t,
                            bool, bool, FILE *);

/* parse.c */

extern struct chamber *parse_chamber(FILE *);

/* puzzle.c */

extern struct puzzlerect *puzzle_from_layout(
    const struct chamber *, int, int (*)(int));

extern int puzzle_pathfind(struct puzzlerect *, bool, int, int, int, int);
