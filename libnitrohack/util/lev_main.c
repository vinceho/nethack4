/*	Copyright (c) 1989 by Jean-Christophe Collet */
/* NitroHack may be freely redistributed.  See license for details. */

/*
 * This file contains the main function for the parser
 * and some useful functions needed by yacc
 */
#define SPEC_LEV	/* for MPW */
/* although, why don't we move those special defines here.. and in dgn_main? */

#include "hack.h"
#include "date.h"
#include "sp_lev.h"
#ifdef STRICT_REF_DEF
#include "tcap.h"
#endif

#ifndef O_WRONLY
#include <fcntl.h>
#endif
#ifndef O_CREAT	/* some older BSD systems do not define O_CREAT in <fcntl.h> */
#include <sys/file.h>
#endif
#ifndef O_BINARY	/* used for micros, no-op for others */
# define O_BINARY 0
#endif

#if defined(WIN32)
# define OMASK FCMASK
#else
# define OMASK 0644
#endif

#define ERR		(-1)

#define NewTab(type, size)	malloc(sizeof(type *) * size)
#define Free(ptr)		if (ptr) free((ptr))
#define Write(fd, item, size)	if (write(fd, (item), size) != size) return FALSE;

#define MAX_ERRORS	25

extern int  yyparse(void);
extern void init_yyin(FILE *);
extern void init_yyout(FILE *);

int  main(int, char **);
void yyerror(const char *);
void yywarning(const char *);
int  yywrap(void);
int get_floor_type(char);
int get_room_type(char *);
int get_trap_type(char *);
int get_monster_id(char *,char);
int get_object_id(char *,char);
boolean check_monster_char(char);
boolean check_object_char(char);
char what_map_char(char);
void scan_map(char *,sp_lev *,mazepart *);
boolean check_subrooms(sp_lev *);
boolean write_level_file(char *,sp_lev *);

void add_opcode(sp_lev *,int,void *);
void *get_last_opcode_data1(sp_lev *,int);
void *get_last_opcode_data2(sp_lev *,int,int);

static boolean write_common_data(int,sp_lev *);
static boolean write_monster(int,monster *);
static boolean write_object(int,object *);
static boolean write_engraving(int,engraving *);
static boolean write_maze(int,sp_lev *);
static boolean write_room(int,room *);
static boolean write_mazepart(int,mazepart *);
static void init_obj_classes(void);

static struct {
	const char *name;
	int type;
} trap_types[] = {
	{ "arrow",	ARROW_TRAP },
	{ "dart",	DART_TRAP },
	{ "falling rock", ROCKTRAP },
	{ "board",	SQKY_BOARD },
	{ "bear",	BEAR_TRAP },
	{ "land mine",	LANDMINE },
	{ "rolling boulder",	ROLLING_BOULDER_TRAP },
	{ "sleep gas",	SLP_GAS_TRAP },
	{ "rust",	RUST_TRAP },
	{ "fire",	FIRE_TRAP },
	{ "pit",	PIT },
	{ "spiked pit",	SPIKED_PIT },
	{ "hole",	HOLE },
	{ "trap door",	TRAPDOOR },
        { "vibrating square",   VIBRATING_SQUARE },
	{ "teleport",	TELEP_TRAP },
	{ "level teleport", LEVEL_TELEP },
	{ "magic portal",   MAGIC_PORTAL },
	{ "web",	WEB },
	{ "statue",	STATUE_TRAP },
	{ "magic",	MAGIC_TRAP },
	{ "anti magic",	ANTI_MAGIC },
	{ "polymorph",	POLY_TRAP },
	{ 0, 0 }
};

static struct {
	const char *name;
	int type;
} room_types[] = {
	/* for historical reasons, room types are not contiguous numbers */
	/* (type 1 is skipped) */
	{ "ordinary",	 OROOM },
	{ "throne",	 COURT },
	{ "swamp",	 SWAMP },
	{ "vault",	 VAULT },
	{ "beehive",	 BEEHIVE },
	{ "morgue",	 MORGUE },
	{ "barracks",	 BARRACKS },
	{ "zoo",	 ZOO },
	{ "delphi",	 DELPHI },
	{ "temple",	 TEMPLE },
	{ "lemurepit",	 LEMUREPIT },
	{ "anthole",	 ANTHOLE },
	{ "cocknest",	 COCKNEST },
	{ "garden",	 GARDEN },
	{ "leprehall",	 LEPREHALL },
	{ "shop",	 SHOPBASE },
	{ "armor shop",	 ARMORSHOP },
	{ "scroll shop", SCROLLSHOP },
	{ "potion shop", POTIONSHOP },
	{ "weapon shop", WEAPONSHOP },
	{ "food shop",	 FOODSHOP },
	{ "ring shop",	 RINGSHOP },
	{ "wand shop",	 WANDSHOP },
	{ "tool shop",	 TOOLSHOP },
	{ "book shop",	 BOOKSHOP },
	{ "candle shop", CANDLESHOP },
	{ "black market", BLACKSHOP },
	{ 0, 0 }
};

const char *fname = "(stdin)";
static char *outprefix = "";
int fatal_error = 0;
int want_warnings = 0;
int be_verbose = 0;

extern unsigned int max_x_map, max_y_map;

extern int line_number, colon_line_number;

int main(int argc, char **argv)
{
	FILE *fin;
	int i;
	boolean errors_encountered = FALSE;

	init_objlist();
	init_obj_classes();

	init_yyout(stdout);
	if (argc == 1) {		/* Read standard input */
	    init_yyin(stdin);
	    yyparse();
	    if (fatal_error > 0) {
		    errors_encountered = TRUE;
	    }
	} else {
	    /* first two args may be "-o outprefix" */
	    i = 1;
	    if (!strcmp(argv[1], "-o") && argc > 3) {
		outprefix = argv[2];
		i = 3;
	    }
	    /* Otherwise every argument is a filename */
	    for (; i<argc; i++) {
		    fname = argv[i];
		    if (!strcmp(fname, "-w")) {
			want_warnings++;
			continue;
		    } else if (!strcmp(fname, "-v")) {
			be_verbose++;
			continue;
		    }

		    fin = freopen(fname, "r", stdin);
		    if (!fin) {
			fprintf(stderr,"Can't open \"%s\" for input.\n",
						fname);
			perror(fname);
			errors_encountered = TRUE;
		    } else {
			init_yyin(fin);
			yyparse();
			line_number = 1;
			if (fatal_error > 0) {
				errors_encountered = TRUE;
				fatal_error = 0;
			}
		    }
	    }
	}
	
	free(objects);
	exit(errors_encountered ? EXIT_FAILURE : EXIT_SUCCESS);
	/*NOTREACHED*/
	return 0;
}

/*
 * Each time the parser detects an error, it uses this function.
 * Here we take count of the errors. To continue farther than
 * MAX_ERRORS wouldn't be reasonable.
 * Assume that explicit calls from lev_comp.y have the 1st letter
 * capitalized, to allow printing of the line containing the start of
 * the current declaration, instead of the beginning of the next declaration.
 */
void yyerror(const char *s)
{
	fprintf(stderr, "%s: line %d : %s\n", fname,
		(*s >= 'A' && *s <= 'Z') ? colon_line_number : line_number, s);
	if (++fatal_error > MAX_ERRORS) {
		fprintf(stderr,"Too many errors, good bye!\n");
		exit(EXIT_FAILURE);
	}
}

/*
 * Just display a warning (that is : a non fatal error)
 */
void yywarning(const char *s)
{
	fprintf(stderr, "%s: line %d : WARNING : %s\n",
				fname, colon_line_number, s);
}

/*
 * Stub needed for lex interface.
 */
int yywrap(void)
{
	return 1;
}

/*
 * Find the type of floor, knowing its char representation.
 */
int get_floor_type(char c)
{
	int val;

	val = what_map_char(c);
	if (val == INVALID_TYPE) {
	    val = ERR;
	    yywarning("Invalid fill character in MAZE declaration");
	}
	return val;
}

/*
 * Find the type of a room in the table, knowing its name.
 */
int get_room_type(char *s)
{
	int i;

	for (i=0; room_types[i].name; i++)
	    if (!strcmp(s, room_types[i].name))
		return (int) room_types[i].type;
	return ERR;
}

/*
 * Find the type of a trap in the table, knowing its name.
 */
int get_trap_type(char *s)
{
	int i;

	for (i=0; trap_types[i].name; i++)
	    if (!strcmp(s,trap_types[i].name))
		return trap_types[i].type;
	return ERR;
}

/*
 * Find the index of a monster in the table, knowing its name.
 */
int get_monster_id(char *s, char c)
{
	int i, class;

	class = c ? def_char_to_monclass(c) : 0;
	if (class == MAXMCLASSES) return ERR;

	for (i = LOW_PM; i < NUMMONS; i++)
	    if (!class || class == mons[i].mlet)
		if (!strcmp(s, mons[i].mname)) return i;
	return ERR;
}

/*
 * Find the index of an object in the table, knowing its name.
 */
int get_object_id(char *s, char c)
{
	int i, class;
	const char *objname;

	class = (c > 0) ? def_char_to_objclass(c) : 0;
	if (class == MAXOCLASSES) return ERR;

	for (i = class ? bases[class] : 0; i < NUM_OBJECTS; i++) {
	    if (class && objects[i].oc_class != class) break;
	    objname = obj_descr[i].oc_name;
	    if (objname && !strcmp(s, objname))
		return i;
	}
	return ERR;
}

static void init_obj_classes(void)
{
	int i, class, prev_class;

	prev_class = -1;
	for (i = 0; i < NUM_OBJECTS; i++) {
	    class = objects[i].oc_class;
	    if (class != prev_class) {
		bases[class] = i;
		prev_class = class;
	    }
	}
}

/*
 * Is the character 'c' a valid monster class ?
 */
boolean check_monster_char(char c)
{
	return def_char_to_monclass(c) != MAXMCLASSES;
}

/*
 * Is the character 'c' a valid object class ?
 */
boolean check_object_char(char c)
{
	return def_char_to_objclass(c) != MAXOCLASSES;
}

/*
 * Convert .des map letter into floor type.
 */
char what_map_char(char c)
{
	switch(c) {
		  case ' '  : return STONE;
		  case '#'  : return CORR;
		  case '.'  : return ROOM;
		  case '-'  : return HWALL;
		  case '|'  : return VWALL;
		  case '+'  : return DOOR;
		  case 'A'  : return AIR;
		  case 'B'  : return CROSSWALL; /* hack: boundary location */
		  case 'C'  : return CLOUD;
		  case 'S'  : return SDOOR;
		  case 'H'  : return SCORR;
		  case '{'  : return FOUNTAIN;
		  case '\\' : return THRONE;
		  case 'K'  : return SINK;
		  case '}'  : return MOAT;
		  case 'P'  : return POOL;
		  case 'L'  : return LAVAPOOL;
		  case 'I'  : return ICE;
		  case 'W'  : return WATER;
		  case 'T'  : return TREE;
		  case 'F'  : return IRONBARS; /* Fe = iron */
		  case 'x'  : return MAX_TYPE; /* 'see-through' */
	    }
	return INVALID_TYPE;
}

void add_opcode(sp_lev *sp, int opc, void *dat)
{
	long nop = sp->init_lev.n_opcodes;
	_opcode *tmp;

	if (opc < 0 || opc >= MAX_SP_OPCODES)
	    yyerror("Unknown opcode");

	tmp = malloc(sizeof(_opcode) * (nop + 1));
	if (sp->opcodes && nop) {
	    memcpy(tmp, sp->opcodes, sizeof(_opcode) * nop);
	    free(sp->opcodes);
	} else if (!tmp) {
	    yyerror("Couldn't alloc opcode space");
	}

	sp->opcodes = tmp;

	sp->opcodes[nop].opcode = opc;
	sp->opcodes[nop].opdat = dat;

	sp->init_lev.n_opcodes++;
}

void *get_last_opcode_data1(sp_lev *sp, int opc1)
{
	return get_last_opcode_data2(sp, opc1, opc1);
}

void *get_last_opcode_data2(sp_lev *sp, int opc1, int opc2)
{
	long nop = sp->init_lev.n_opcodes;
	int i;

	if (nop < 1)
	    yyerror("No opcodes yet?!");

	for (i = nop - 1; i >= 0; i--) {
	    if (sp->opcodes[i].opcode == opc1 ||
		sp->opcodes[i].opcode == opc2) {
		return sp->opcodes[i].opdat;
	    }
	}

	return NULL;
}

/*
 * Yep! LEX gives us the map in a raw mode.
 * Just analyze it here.
 */
void scan_map(char *map, sp_lev *sp, mazepart *mpart)
{
	int i, len;
	char *s1, *s2;
	int max_len = 0;
	int max_hig = 0;
	char msg[256];
	char *tmpmap[ROWNO];

	/* First, strip out digits 0-9 (line numbering) */
	for (s1 = s2 = map; *s1; s1++)
	    if (*s1 < '0' || *s1 > '9')
		*s2++ = *s1;
	*s2 = '\0';

	/* Second, find the max width of the map */
	s1 = map;
	while (s1 && *s1) {
		s2 = strchr(s1, '\n');
		if (s2) {
			len = (int) (s2 - s1);
			s1 = s2 + 1;
		} else {
			len = (int) strlen(s1);
			s1 = NULL;
		}
		if (len > max_len) max_len = len;
	}

	/* Then parse it now */
	while (map && *map) {
		tmpmap[max_hig] = malloc(max_len);
		s1 = strchr(map, '\n');
		if (s1) {
			len = (int) (s1 - map);
			s1++;
		} else {
			len = (int) strlen(map);
			s1 = map + len;
		}
		for (i=0; i<len; i++)
		  if ((tmpmap[max_hig][i] = what_map_char(map[i])) == INVALID_TYPE) {
		      sprintf(msg,
			 "Invalid character @ (%d, %d) - replacing with stone",
			      max_hig, i);
		      yywarning(msg);
		      tmpmap[max_hig][i] = STONE;
		    }
		while (i < max_len)
		    tmpmap[max_hig][i++] = STONE;
		map = s1;
		max_hig++;
	}

	/* Memorize boundaries */

	max_x_map = max_len - 1;
	max_y_map = max_hig - 1;

	/* Store the map into the mazepart structure */

	if (max_len > MAP_X_LIM || max_hig > MAP_Y_LIM) {
	    sprintf(msg, "Map too large! (max %d x %d)", MAP_X_LIM, MAP_Y_LIM);
	    yyerror(msg);
	}

	mpart->xsize = max_len;
	mpart->ysize = max_hig;
	mpart->map = malloc(max_hig * sizeof(char *));

	for (i = 0; i < max_hig; i++)
	    mpart->map[i] = tmpmap[i];

	add_opcode(sp, SPO_MAP, mpart);
}

/*
 * We need to check the subrooms apartenance to an existing room.
 */
boolean check_subrooms(sp_lev *sp)
{
	int i, j;
	boolean	found, ok = TRUE;
	char	msg[256];

	for (i = 0; i < sp->init_lev.n_opcodes; i++) {
	    if (sp->opcodes[i].opcode == SPO_SUBROOM) {
		room *subrm = (room *)sp->opcodes[i].opdat;
		found = FALSE;
		for (j = 0; j < sp->init_lev.n_opcodes; j++) {
		    if (sp->opcodes[j].opcode == SPO_ROOM) {
			room *parrm = (room *)sp->opcodes[j].opdat;
			if (parrm->name.str && !strcmp(subrm->parent.str, parrm->name.str)) {
			    found = TRUE;
			    break;
			}
		    }
		}
		if (!found) {
		    sprintf(msg,
			    "Subroom error : parent room '%s' not found!",
			    subrm->parent.str);
		    yyerror(msg);
		    ok = FALSE;
		}
	    }
	}
	return ok;
}

/*
 * Output some info common to all special levels.
 */
static boolean write_common_data(int fd, sp_lev *lvl)
{
	static struct version_info version_data = {
			VERSION_NUMBER, VERSION_FEATURES,
			VERSION_SANITY1
	};

	Write(fd, &version_data, sizeof version_data);
	Write(fd, &lvl->init_lev, sizeof(lev_init));
	return TRUE;
}

/*
 * Output monster info, which needs string fixups, then release memory.
 */
static boolean write_monster(int fd, monster *m)
{
	char *name, *appr;

	name = m->name.str;
	appr = m->appear_as.str;
	m->name.str = m->appear_as.str = 0;
	m->name.len = name ? strlen(name) : 0;
	m->appear_as.len = appr ? strlen(appr) : 0;
	Write(fd, m, sizeof *m);
	if (name) {
	    Write(fd, name, m->name.len);
	    Free(name);
	}
	if (appr) {
	    Write(fd, appr, m->appear_as.len);
	    Free(appr);
	}
	return TRUE;
}

/*
 * Output object info, which needs string fixup, then release memory.
 */
static boolean write_object(int fd, object *o)
{
	char *name;

	name = o->name.str;
	o->name.str = 0;	/* reset in case 'len' is narrower */
	o->name.len = name ? strlen(name) : 0;
	Write(fd, o, sizeof *o);
	if (name) {
	    Write(fd, name, o->name.len);
	    Free(name);
	}
	return TRUE;
}

/*
 * Output mazepart info, and release memory.
 */
static boolean write_mazepart(int fd, mazepart *pt)
{
	int j;

	Write(fd, &(pt->zaligntyp), sizeof(pt->zaligntyp));
	Write(fd, &(pt->keep_region), sizeof(pt->keep_region));
	Write(fd, &(pt->halign), sizeof(pt->halign));
	Write(fd, &(pt->valign), sizeof(pt->valign));
	Write(fd, &(pt->xsize), sizeof(pt->xsize));
	Write(fd, &(pt->ysize), sizeof(pt->ysize));
	if (pt->xsize > 0 && pt->ysize > 0) {
	    for (j = 0; j < pt->ysize; j++) {
		if (pt->xsize > 1 || pt->ysize > 1)
		    Write(fd, pt->map[j], pt->xsize * (signed)(sizeof *pt->map[j]));
		Free(pt->map[j]);
	    }
	    Free(pt->map);
	}

	return TRUE;
}

/*
 * Output engraving info, which needs string fixup, then release memory.
 */
static boolean write_engraving(int fd, engraving *e)
{
	char *engr;
	engr = e->engr.str;
	e->engr.str = 0;	/* reset in case 'len' is narrower */
	e->engr.len = strlen(engr);
	Write(fd, e, sizeof *e);
	Write(fd, engr, e->engr.len);
	Free(engr);
	return TRUE;
}

/*
 * Output room info, which needs string fixup, then release memory.
 */
static boolean write_room(int fd, room *pt)
{
	char *name, *parent;

	name = pt->name.str;
	parent = pt->parent.str;
	pt->name.str = pt->parent.str = 0;
	pt->name.len = name ? strlen(name) : 0;
	pt->parent.len = parent ? strlen(parent) : 0;
	Write(fd, pt, sizeof *pt);
	if (name) {
	    Write(fd, name, pt->name.len);
	    Free(name);
	}
	if (parent) {
	    Write(fd, parent, pt->parent.len);
	    Free(parent);
	}

	return TRUE;
}

static boolean write_levregion(int fd, lev_region *pt)
{
	char *rname;
	rname = pt->rname.str;
	pt->rname.str = 0;
	pt->rname.len = rname ? strlen(rname) : 0;
	Write(fd, pt, sizeof *pt);
	if (rname) {
	    Write(fd, rname, pt->rname.len);
	    Free(rname);
	}
	return TRUE;
}

/*
 * Here we write the sp_lev structure in the specified file (fd).
 * Also, we have to free the memory allocated via malloc().
 */
static boolean write_maze(int fd, sp_lev *maze)
{
	int i;
	uchar len;

	if (!write_common_data(fd, maze))
	    return FALSE;

	for (i = 0; i < maze->init_lev.n_opcodes; i++) {
	    _opcode tmpo = maze->opcodes[i];
	    Write(fd, &(tmpo.opcode), sizeof(tmpo.opcode));
	    switch (tmpo.opcode) {
	    case SPO_EXIT:
	    case SPO_POP_CONTAINER:
	    case SPO_WALLIFY:
	    case SPO_NULL:
	    case SPO_ENDROOM:
		break;
	    case SPO_MESSAGE:
	    case SPO_RANDOM_OBJECTS:
	    case SPO_RANDOM_MONSTERS:
	    case SPO_RANDOM_PLACES:
		len = (uchar)strlen((char *)tmpo.opdat);
		Write(fd, &len, sizeof len);
		if (len) Write(fd, tmpo.opdat, (int)len);
		break;
	    case SPO_MONSTER:
		write_monster(fd, tmpo.opdat);
		break;
	    case SPO_OBJECT:
		write_object(fd, tmpo.opdat);
		break;
	    case SPO_ENGRAVING:
		write_engraving(fd, tmpo.opdat);
		break;
	    case SPO_ROOM:
	    case SPO_SUBROOM:
		write_room(fd, tmpo.opdat);
		break;
	    case SPO_ROOM_DOOR:
		Write(fd, tmpo.opdat, sizeof(room_door));
		break;
	    case SPO_DOOR:
		Write(fd, tmpo.opdat, sizeof(door));
		break;
	    case SPO_STAIR:
		Write(fd, tmpo.opdat, sizeof(stair));
		break;
	    case SPO_LADDER:
		Write(fd, tmpo.opdat, sizeof(lad));
		break;
	    case SPO_ALTAR:
		Write(fd, tmpo.opdat, sizeof(altar));
		break;
	    case SPO_FOUNTAIN:
		Write(fd, tmpo.opdat, sizeof(fountain));
		break;
	    case SPO_SINK:
		Write(fd, tmpo.opdat, sizeof(sink));
		break;
	    case SPO_POOL:
		Write(fd, tmpo.opdat, sizeof(pool));
		break;
	    case SPO_TRAP:
		Write(fd, tmpo.opdat, sizeof(trap));
		break;
	    case SPO_GOLD:
		Write(fd, tmpo.opdat, sizeof(gold));
		break;
	    case SPO_CORRIDOR:
		Write(fd, tmpo.opdat, sizeof(corridor));
		break;
	    case SPO_REPLACETERRAIN:
		Write(fd, tmpo.opdat, sizeof(replaceterrain));
		break;
	    case SPO_RANDLINE:
		Write(fd, tmpo.opdat, sizeof(randline));
		break;
	    case SPO_TERRAIN:
		Write(fd, tmpo.opdat, sizeof(terrain));
		break;
	    case SPO_SPILL:
		Write(fd, tmpo.opdat, sizeof(spill));
		break;
	    case SPO_LEVREGION:
		write_levregion(fd, tmpo.opdat);
		break;
	    case SPO_DRAWBRIDGE:
		Write(fd, tmpo.opdat, sizeof(drawbridge));
		break;
	    case SPO_MAZEWALK:
		Write(fd, tmpo.opdat, sizeof(walk));
		break;
	    case SPO_NON_DIGGABLE:
	    case SPO_NON_PASSWALL:
		Write(fd, tmpo.opdat, sizeof(digpos));
		break;
	    case SPO_REGION:
		Write(fd, tmpo.opdat, sizeof(region));
		break;
	    case SPO_CMP:
		Write(fd, tmpo.opdat, sizeof(opcmp));
		break;
	    case SPO_JMP:
	    case SPO_JL:
	    case SPO_JG:
		Write(fd, tmpo.opdat, sizeof(opjmp));
		break;
	    case SPO_MAP:
		write_mazepart(fd, tmpo.opdat);
		break;
	    default:
		panic("unknown sp_lev opcode (%i)", tmpo.opcode);
	    }
	    Free(tmpo.opdat);
	}
	/* clear the struct for the next user */
	memset(&maze->init_lev, 0, sizeof maze->init_lev);

	return TRUE;
}

/*
 * Open and write special level file.
 * Return TRUE on success, FALSE on failure.
 */
boolean write_level_file(char *filename, sp_lev *lvl)
{
	int fout;
	char lbuf[60];

	lbuf[0] = '\0';
	strcat(lbuf, outprefix);
	strcat(lbuf, filename);
	strcat(lbuf, LEV_EXT);

	fout = open(lbuf, O_WRONLY|O_CREAT|O_BINARY, OMASK);
	if (fout < 0) return FALSE;

	if (!lvl) panic("write_level_file");

	if (be_verbose)
	    fprintf(stdout, "File: '%s', opcodes: %li\n",
		    lbuf, lvl->init_lev.n_opcodes);

	if (!write_maze(fout, lvl))
	    return FALSE;

	close(fout);

	return TRUE;
}

#ifdef STRICT_REF_DEF
/*
 * Any globals declared in hack.h and descendents which aren't defined
 * in the modules linked into lev_comp should be defined here.  These
 * definitions can be dummies:  their sizes shouldn't matter as long as
 * as their types are correct; actual values are irrelevant.
 */
#define ARBITRARY_SIZE 1
/* attrib.c */
struct attribs attrmax, attrmin;
/* files.c */
const char *configfile;
char lock[ARBITRARY_SIZE];
char SAVEF[ARBITRARY_SIZE];

/* termcap.c */
struct tc_lcl_data tc_lcl_data;
char *hilites[CLR_MAX];
/* trap.c */
const char *traps[TRAPNUM];
/* window.c */
struct window_procs windowprocs;
/* xxxtty.c */
# ifdef DEFINE_OSPEED
short ospeed;
# endif
#endif	/* STRICT_REF_DEF */

/*lev_main.c*/
