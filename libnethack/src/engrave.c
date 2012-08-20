/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include <ctype.h>

/* random engravings */
static const char *const random_mesg[] = {
    "Elbereth",
    /* trap engravings */
    "Vlad was here", "ad aerarium",
    /* take-offs and other famous engravings */
    "Owlbreath", "Galadriel",
    "Kilroy was here",
    "A.S. ->", "<- A.S.",       /* Journey to the Center of the Earth */
    "You won't get it up the steps",    /* Adventure */
    "Lasciate ogni speranza o voi ch'entrate.", /* Inferno */
    "Well Come",        /* Prisoner */
    "We apologize for the inconvenience.",      /* So Long... */
    "See you next Wednesday",   /* Thriller */
    "notary sojak",     /* Smokey Stover */
    "For a good time call 8?7-5309",
    "Please don't feed the animals.",   /* Various zoos around the world */
    "Madam, in Eden, I'm Adam.",        /* A palindrome */
    "Two thumbs up!",   /* Siskel & Ebert */
    "Hello, World!",    /* The First C Program */
    "As if!",   /* Clueless */
};

char *
random_engraving(char *outbuf)
{
    const char *rumor;

    /* a random engraving may come from the "rumors" file, or from the list
       above */
    if (!rn2(4) || !(rumor = getrumor(0, outbuf, TRUE, NULL)) || !*rumor)
        strcpy(outbuf, random_mesg[rn2(SIZE(random_mesg))]);

    wipeout_text(outbuf, (int)(strlen(outbuf) / 4), 0);
    return outbuf;
}

/* Partial rubouts for engraving characters. -3. */
static const struct {
    char wipefrom;
    const char *wipeto;
} rubouts[] = {
    {
    'A', "^"}, {
    'B', "Pb["}, {
    'C', "("}, {
    'D', "|)["}, {
    'E', "|FL[_"}, {
    'F', "|-"}, {
    'G', "C("}, {
    'H', "|-"}, {
    'I', "|"}, {
    'K', "|<"}, {
    'L', "|_"}, {
    'M', "|"}, {
    'N', "|\\"}, {
    'O', "C("}, {
    'P', "F"}, {
    'Q', "C("}, {
    'R', "PF"}, {
    'T', "|"}, {
    'U', "J"}, {
    'V', "/\\"}, {
    'W', "V/\\"}, {
    'Z', "/"}, {
    'b', "|"}, {
    'd', "c|"}, {
    'e', "c"}, {
    'g', "c"}, {
    'h', "n"}, {
    'j', "i"}, {
    'k', "|"}, {
    'l', "|"}, {
    'm', "nr"}, {
    'n', "r"}, {
    'o', "c"}, {
    'q', "c"}, {
    'w', "v"}, {
    'y', "v"}, {
    ':', "."}, {
    ';', ","}, {
    '0', "C("}, {
    '1', "|"}, {
    '6', "o"}, {
    '7', "/"}, {
    '8', "3o"}
};

void
wipeout_text(char *engr, int cnt, unsigned seed)
{       /* for semi-controlled randomization */
    char *s;
    int i, j, nxt, use_rubout, lth = (int)strlen(engr);

    if (lth && cnt > 0) {
        while (cnt--) {
            /* pick next character */
            if (!seed) {
                /* random */
                nxt = rn2(lth);
                use_rubout = rn2(4);
            } else {
                /* predictable; caller can reproduce the same sequence by
                   supplying the same arguments later, or a pseudo-random
                   sequence by varying any of them */
                nxt = seed % lth;
                seed *= 31, seed %= (BUFSZ - 1);
                use_rubout = seed & 3;
            }
            s = &engr[nxt];
            if (*s == ' ')
                continue;

            /* rub out unreadable & small punctuation marks */
            if (strchr("?.,'`-|_", *s)) {
                *s = ' ';
                continue;
            }

            if (!use_rubout)
                i = SIZE(rubouts);
            else
                for (i = 0; i < SIZE(rubouts); i++)
                    if (*s == rubouts[i].wipefrom) {
                        /* 
                         * Pick one of the substitutes at random.
                         */
                        if (!seed)
                            j = rn2(strlen(rubouts[i].wipeto));
                        else {
                            seed *= 31, seed %= (BUFSZ - 1);
                            j = seed % (strlen(rubouts[i].wipeto));
                        }
                        *s = rubouts[i].wipeto[j];
                        break;
                    }

            /* didn't pick rubout; use '?' for unreadable character */
            if (i == SIZE(rubouts))
                *s = '?';
        }
    }

    /* trim trailing spaces */
    while (lth && engr[lth - 1] == ' ')
        engr[--lth] = 0;
}

boolean
can_reach_floor(void)
{
    return (boolean) (!u.uswallow &&
                      /* Restricted/unskilled riders can't reach the floor */
                      !(u.usteed && P_SKILL(P_RIDING) < P_BASIC) &&
                      (!Levitation || Is_airlevel(&u.uz) ||
                       Is_waterlevel(&u.uz)));
}


const char *
surface(int x, int y)
{
    struct rm *loc = &level->locations[x][y];

    if ((x == u.ux) && (y == u.uy) && u.uswallow && is_animal(u.ustuck->data))
        return "maw";
    else if (IS_AIR(loc->typ) && Is_airlevel(&u.uz))
        return "air";
    else if (is_pool(level, x, y))
        return (Underwater && !Is_waterlevel(&u.uz)) ? "bottom" : "water";
    else if (is_ice(level, x, y))
        return "ice";
    else if (is_lava(level, x, y))
        return "lava";
    else if (loc->typ == DRAWBRIDGE_DOWN)
        return "bridge";
    else if (IS_ALTAR(level->locations[x][y].typ))
        return "altar";
    else if (IS_GRAVE(level->locations[x][y].typ))
        return "headstone";
    else if (IS_FOUNTAIN(level->locations[x][y].typ))
        return "fountain";
    else if ((IS_ROOM(loc->typ) && !Is_earthlevel(&u.uz)) || IS_WALL(loc->typ)
             || IS_DOOR(loc->typ) || loc->typ == SDOOR)
        return "floor";
    else
        return "ground";
}

const char *
ceiling(int x, int y)
{
    struct rm *loc = &level->locations[x][y];
    const char *what;

    /* other room types will no longer exist when we're interested -- see
       check_special_room() */
    if (*in_rooms(level, x, y, VAULT))
        what = "vault's ceiling";
    else if (*in_rooms(level, x, y, TEMPLE))
        what = "temple's ceiling";
    else if (*in_rooms(level, x, y, SHOPBASE))
        what = "shop's ceiling";
    else if (IS_AIR(loc->typ))
        what = "sky";
    else if (Underwater)
        what = "water's surface";
    else if ((IS_ROOM(loc->typ) && !Is_earthlevel(&u.uz)) || IS_WALL(loc->typ)
             || IS_DOOR(loc->typ) || loc->typ == SDOOR)
        what = "ceiling";
    else
        what = "rock above";

    return what;
}

struct engr *
engr_at(struct level *lev, xchar x, xchar y)
{
    struct engr *ep = lev->lev_engr;

    while (ep) {
        if (x == ep->engr_x && y == ep->engr_y)
            return ep;
        ep = ep->nxt_engr;
    }
    return NULL;
}

/* Decide whether a particular string is engraved at a specified
 * location; a case-insensitive substring match used.
 * Ignore headstones, in case the player names herself "Elbereth".
 */
int
sengr_at(const char *s, xchar x, xchar y)
{
    struct engr *ep = engr_at(level, x, y);

    return (ep && ep->engr_type != HEADSTONE && ep->engr_time <= moves &&
            strstri(ep->engr_txt, s) != 0);
}


void
u_wipe_engr(int cnt)
{
    if (can_reach_floor())
        wipe_engr_at(level, u.ux, u.uy, cnt);
}


void
wipe_engr_at(struct level *lev, xchar x, xchar y, xchar cnt)
{
    struct engr *ep = engr_at(lev, x, y);

    /* Headstones are indelible */
    if (ep && ep->engr_type != HEADSTONE) {
        if (ep->engr_type != BURN || is_ice(lev, x, y)) {
            if (ep->engr_type != DUST && ep->engr_type != ENGR_BLOOD) {
                cnt = rn2(1 + 50 / (cnt + 1)) ? 0 : 1;
            }
            wipeout_text(ep->engr_txt, (int)cnt, 0);
            while (ep->engr_txt[0] == ' ')
                ep->engr_txt++;
            if (!ep->engr_txt[0])
                del_engr(ep, lev);
        }
    }
}


void
read_engr_at(int x, int y)
{
    struct engr *ep = engr_at(level, x, y);
    int sensed = 0;
    char buf[BUFSZ];

    /* Sensing an engraving does not require sight, nor does it necessarily
       imply comprehension (literacy). */
    if (ep && ep->engr_txt[0]) {
        switch (ep->engr_type) {
        case DUST:
            if (!Blind) {
                sensed = 1;
                pline("Something is written here in the %s.",
                      is_ice(level, x, y) ? "frost" : "dust");
            }
            break;
        case ENGRAVE:
        case HEADSTONE:
            if (!Blind || can_reach_floor()) {
                sensed = 1;
                pline("Something is engraved here on the %s.", surface(x, y));
            }
            break;
        case BURN:
            if (!Blind || can_reach_floor()) {
                sensed = 1;
                pline("Some text has been %s into the %s here.",
                      is_ice(level, x, y) ? "melted" : "burned", surface(x, y));
            }
            break;
        case MARK:
            if (!Blind) {
                sensed = 1;
                pline("There's some graffiti on the %s here.", surface(x, y));
            }
            break;
        case ENGR_BLOOD:
            /* "It's a message! Scrawled in blood!" "What's it say?" "It
               says... `See you next Wednesday.'" -- Thriller */
            if (!Blind) {
                sensed = 1;
                pline("You see a message scrawled in blood here.");
            }
            break;
        default:
            impossible("Something is written in a very strange way.");
            sensed = 1;
        }
        if (sensed) {
            char *et;
            unsigned maxelen = BUFSZ - sizeof ("You feel the words: \"\". ");

            if (strlen(ep->engr_txt) > maxelen) {
                strncpy(buf, ep->engr_txt, (int)maxelen);
                buf[maxelen] = '\0';
                et = buf;
            } else
                et = ep->engr_txt;
            pline("You %s: \"%s\".", (Blind) ? "feel the words" : "read", et);
            if (flags.run > 1)
                nomul(0, NULL);
        }
    }
}


void
make_engr_at(struct level *lev, int x, int y, const char *s, long e_time,
             xchar e_type)
{
    struct engr *ep;

    if ((ep = engr_at(lev, x, y)) != 0)
        del_engr(ep, lev);
    ep = newengr(strlen(s) + 1);
    memset(ep, 0, sizeof (struct engr) + strlen(s) + 1);
    ep->nxt_engr = lev->lev_engr;
    lev->lev_engr = ep;
    ep->engr_x = x;
    ep->engr_y = y;
    ep->engr_txt = (char *)(ep + 1);
    strcpy(ep->engr_txt, s);
    while (ep->engr_txt[0] == ' ')
        ep->engr_txt++;
    /* engraving Elbereth shows wisdom */
    if (!in_mklev && !strcmp(s, "Elbereth"))
        exercise(A_WIS, TRUE);
    ep->engr_time = e_time;
    ep->engr_type = e_type > 0 ? e_type : rnd(N_ENGRAVE - 1);
    ep->engr_lth = strlen(s) + 1;
}

/* delete any engraving at location <x,y> */
void
del_engr_at(struct level *lev, int x, int y)
{
    struct engr *ep = engr_at(lev, x, y);

    if (ep)
        del_engr(ep, lev);
}

/*
 * freehand - returns true if player has a free hand
 */
int
freehand(void)
{
    return (!uwep || !welded(uwep) ||
            (!bimanual(uwep) && (!uarms || !uarms->cursed)));
/* if ((uwep && bimanual(uwep)) ||
           (uwep && uarms))
       return 0;
   else
       return 1;*/
}

static const char styluses[] =
    { ALL_CLASSES, ALLOW_NONE, TOOL_CLASS, WEAPON_CLASS, WAND_CLASS,
    GEM_CLASS, RING_CLASS, 0
};

/* Mohs' Hardness Scale:
 *  1 - Talc             6 - Orthoclase
 *  2 - Gypsum           7 - Quartz
 *  3 - Calcite          8 - Topaz
 *  4 - Fluorite         9 - Corundum
 *  5 - Apatite         10 - Diamond
 *
 * Since granite is a igneous rock hardness ~ 7, anything >= 8 should
 * probably be able to scratch the rock.
 * Devaluation of less hard gems is not easily possible because obj struct
 * does not contain individual oc_cost currently. 7/91
 *
 * steel     -  5-8.5   (usu. weapon)
 * diamond    - 10                      * jade       -  5-6      (nephrite)
 * ruby       -  9      (corundum)      * turquoise  -  5-6
 * sapphire   -  9      (corundum)      * opal       -  5-6
 * topaz      -  8                      * glass      - ~5.5
 * emerald    -  7.5-8  (beryl)         * dilithium  -  4-5??
 * aquamarine -  7.5-8  (beryl)         * iron       -  4-5
 * garnet     -  7.25   (var. 6.5-8)    * fluorite   -  4
 * agate      -  7      (quartz)        * brass      -  3-4
 * amethyst   -  7      (quartz)        * gold       -  2.5-3
 * jasper     -  7      (quartz)        * silver     -  2.5-3
 * onyx       -  7      (quartz)        * copper     -  2.5-3
 * moonstone  -  6      (orthoclase)    * amber      -  2-2.5
 */

/* return 1 if action took 1 (or more) moves, 0 if error or aborted */
static int
doengrave_core(struct obj *otmp, int auto_elbereth)
{
    boolean dengr = FALSE;      /* TRUE if we wipe out the current engraving */
    boolean doblind = FALSE;    /* TRUE if engraving blinds the player */
    boolean doknown = FALSE;    /* TRUE if we identify the stylus */
    boolean doknown_after = FALSE;      /* TRUE if we identify the stylus after
                                           * successfully engraving. */
    boolean eow = FALSE;        /* TRUE if we are overwriting oep */
    boolean jello = FALSE;      /* TRUE if we are engraving in slime */
    boolean ptext = TRUE;       /* TRUE if we must prompt for engrave text */
    boolean teleengr = FALSE;   /* TRUE if we move the old engraving */
    boolean zapwand = FALSE;    /* TRUE if we remove a wand charge */
    xchar type = DUST;  /* Type of engraving made */
    char buf[BUFSZ];    /* Buffer for final/poly engraving text */
    char ebuf[BUFSZ];   /* Buffer for initial engraving text */
    char qbuf[QBUFSZ];  /* Buffer for query text */
    char post_engr_text[BUFSZ]; /* Text displayed after engraving prompt */
    const char *everb;  /* Present tense of engraving type */
    const char *eloc;   /* Where the engraving is (ie dust/floor/...) */
    char *sp;   /* Place holder for space count of engr text */
    int len;    /* # of nonspace chars of new engraving text */
    int maxelen;        /* Max allowable length of engraving text */
    struct engr *oep = engr_at(level, u.ux, u.uy);

    /* The current engraving */
    char *writer;

    multi = 0;  /* moves consumed */
    nomovemsg = NULL;   /* occupation end message */

    buf[0] = (char)0;
    ebuf[0] = (char)0;
    post_engr_text[0] = (char)0;
    maxelen = BUFSZ - 1;
    if (is_demon(youmonst.data) || youmonst.data->mlet == S_VAMPIRE)
        type = ENGR_BLOOD;

    /* Can the adventurer engrave at all? */

    if (u.uswallow) {
        if (is_animal(u.ustuck->data)) {
            pline("What would you write?  \"Jonah was here\"?");
            return 0;
        } else if (is_whirly(u.ustuck->data)) {
            pline("You can't reach the %s.", surface(u.ux, u.uy));
            return 0;
        } else
            jello = TRUE;
    } else if (is_lava(level, u.ux, u.uy)) {
        pline("You can't write on the lava!");
        return 0;
    } else if (Underwater) {
        pline("You can't write underwater!");
        return 0;
    } else if (is_pool(level, u.ux, u.uy) ||
               IS_FOUNTAIN(level->locations[u.ux][u.uy].typ)) {
        pline("You can't write on the water!");
        return 0;
    }
    if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) /* in bubble */ ) {
        pline("You can't write in thin air!");
        return 0;
    }
    if (cantwield(youmonst.data)) {
        pline("You can't even hold anything!");
        return 0;
    }
    if (check_capacity(NULL))
        return 0;

    /* One may write with finger, or weapon, or wand, or..., or... Edited by
       GAN 10/20/86 so as not to change weapon wielded. */

    if (otmp && !validate_object(otmp, styluses, "write with"))
        return 0;
    else if (!otmp && !auto_elbereth)
        otmp = getobj(styluses, "write with");
    else if (!otmp)
        otmp = &zeroobj;        /* TODO: search for athames */
    if (!otmp)
        return 0;       /* otmp == zeroobj if fingers */

    if (otmp == &zeroobj)
        writer = makeplural(body_part(FINGER));
    else
        writer = xname(otmp);

    /* There's no reason you should be able to write with a wand while both
       your hands are tied up. */
    if (!freehand() && otmp != uwep && !otmp->owornmask) {
        pline("You have no free %s to write with!", body_part(HAND));
        return 0;
    }

    if (jello) {
        pline("You tickle %s with your %s.", mon_nam(u.ustuck), writer);
        pline("Your message dissolves...");
        return 0;
    }
    if (otmp->oclass != WAND_CLASS && !can_reach_floor()) {
        pline("You can't reach the %s!", surface(u.ux, u.uy));
        return 0;
    }
    if (IS_ALTAR(level->locations[u.ux][u.uy].typ)) {
        pline("You make a motion towards the altar with your %s.", writer);
        altar_wrath(u.ux, u.uy);
        return 0;
    }
    if (IS_GRAVE(level->locations[u.ux][u.uy].typ)) {
        if (otmp == &zeroobj) { /* using only finger */
            pline("You would only make a small smudge on the %s.",
                  surface(u.ux, u.uy));
            return 0;
        } else if (!level->locations[u.ux][u.uy].disturbed) {
            pline("You disturb the undead!");
            level->locations[u.ux][u.uy].disturbed = 1;
            makemon(&mons[PM_GHOUL], level, u.ux, u.uy, NO_MM_FLAGS);
            exercise(A_WIS, FALSE);
            return 1;
        }
    }

    /* SPFX for items */

    switch (otmp->oclass) {
    default:
    case AMULET_CLASS:
    case CHAIN_CLASS:
    case POTION_CLASS:
    case COIN_CLASS:
        break;

    case RING_CLASS:
        /* "diamond" rings and others should work */
    case GEM_CLASS:
        /* diamonds & other hard gems should work */
        if (objects[otmp->otyp].oc_tough) {
            type = ENGRAVE;
            break;
        }
        break;

    case ARMOR_CLASS:
        if (is_boots(otmp)) {
            type = DUST;
            break;
        }
        /* fall through */
        /* Objects too large to engrave with */
    case BALL_CLASS:
    case ROCK_CLASS:
        pline("You can't engrave with such a large object!");
        ptext = FALSE;
        break;

        /* Objects too silly to engrave with */
    case FOOD_CLASS:
    case SCROLL_CLASS:
    case SPBOOK_CLASS:
        pline("Your %s would get %s.", xname(otmp),
              is_ice(level, u.ux, u.uy) ? "all frosty" : "too dirty");
        ptext = FALSE;
        break;

    case RANDOM_CLASS: /* This should mean fingers */
        break;

        /* The charge is removed from the wand before prompting for the
           engraving text, because all kinds of setup decisions and
           pre-engraving messages are based upon knowing what type of engraving 
           the wand is going to do.  Also, the player will have potentially
           seen "You wrest .." message, and therefore will know they are using
           a charge. */
    case WAND_CLASS:
        if (zappable(otmp)) {
            check_unpaid(otmp);
            zapwand = TRUE;
            if (Levitation)
                ptext = FALSE;

            switch (otmp->otyp) {
                /* DUST wands */
            default:
                break;

                /* NODIR wands */
            case WAN_LIGHT:
            case WAN_SECRET_DOOR_DETECTION:
            case WAN_CREATE_MONSTER:
            case WAN_WISHING:
            case WAN_ENLIGHTENMENT:
                zapnodir(otmp);
                break;

                /* IMMEDIATE wands */
                /* If wand is "IMMEDIATE", remember to affect the previous
                   engraving even if turning to dust. */
            case WAN_STRIKING:
                strcpy(post_engr_text,
                       "The wand unsuccessfully fights your attempt to write!");
                doknown_after = TRUE;
                break;
            case WAN_SLOW_MONSTER:
                if (!Blind) {
                    sprintf(post_engr_text, "The bugs on the %s slow down!",
                            surface(u.ux, u.uy));
                    doknown_after = TRUE;
                }
                break;
            case WAN_SPEED_MONSTER:
                if (!Blind) {
                    sprintf(post_engr_text, "The bugs on the %s speed up!",
                            surface(u.ux, u.uy));
                    doknown_after = TRUE;
                }
                break;
            case WAN_POLYMORPH:
                if (oep) {
                    if (!Blind) {
                        type = (xchar) 0;       /* random */
                        random_engraving(buf);
                        doknown = TRUE;
                    }
                    dengr = TRUE;
                }
                break;
            case WAN_NOTHING:
            case WAN_UNDEAD_TURNING:
            case WAN_OPENING:
            case WAN_LOCKING:
            case WAN_PROBING:
                break;

                /* RAY wands */
            case WAN_MAGIC_MISSILE:
                ptext = TRUE;
                if (!Blind) {
                    sprintf(post_engr_text,
                            "The %s is riddled by bullet holes!",
                            surface(u.ux, u.uy));
                    doknown_after = TRUE;
                }
                break;

                /* can't tell sleep from death - Eric Backus */
            case WAN_SLEEP:
            case WAN_DEATH:
                if (!Blind) {
                    sprintf(post_engr_text, "The bugs on the %s stop moving!",
                            surface(u.ux, u.uy));
                }
                break;

            case WAN_COLD:
                if (!Blind) {
                    strcpy(post_engr_text,
                           "A few ice cubes drop from the wand.");
                    doknown_after = TRUE;
                }
                if (!oep || (oep->engr_type != BURN))
                    break;
            case WAN_CANCELLATION:
            case WAN_MAKE_INVISIBLE:
                if (oep && oep->engr_type != HEADSTONE) {
                    if (!Blind)
                        pline("The engraving on the %s vanishes!",
                              surface(u.ux, u.uy));
                    dengr = TRUE;
                }
                break;
            case WAN_TELEPORTATION:
                if (oep && oep->engr_type != HEADSTONE) {
                    if (!Blind)
                        pline("The engraving on the %s vanishes!",
                              surface(u.ux, u.uy));
                    teleengr = TRUE;
                }
                break;

                /* type = ENGRAVE wands */
            case WAN_DIGGING:
                ptext = TRUE;
                type = ENGRAVE;
                if (!objects[otmp->otyp].oc_name_known) {
                    if (flags.verbose)
                        pline("This %s is a wand of digging!", xname(otmp));
                    doknown = TRUE;
                }
                if (!Blind)
                    strcpy(post_engr_text,
                           IS_GRAVE(level->locations[u.ux][u.uy].
                                    typ) ? "Chips fly out from the headstone." :
                           is_ice(level, u.ux,
                                  u.
                                  uy) ? "Ice chips fly up from the ice surface!"
                           : "Gravel flies up from the floor.");
                else
                    strcpy(post_engr_text, "You hear drilling!");
                break;

                /* type = BURN wands */
            case WAN_FIRE:
                ptext = TRUE;
                type = BURN;
                if (!objects[otmp->otyp].oc_name_known) {
                    if (flags.verbose)
                        pline("This %s is a wand of fire!", xname(otmp));
                    doknown = TRUE;
                }
                strcpy(post_engr_text,
                       Blind ? "You feel the wand heat up." :
                       "Flames fly from the wand.");
                break;
            case WAN_LIGHTNING:
                ptext = TRUE;
                type = BURN;
                if (!objects[otmp->otyp].oc_name_known) {
                    if (flags.verbose)
                        pline("This %s is a wand of lightning!", xname(otmp));
                    doknown = TRUE;
                }
                if (!Blind) {
                    strcpy(post_engr_text, "Lightning arcs from the wand.");
                    doblind = TRUE;
                } else
                    strcpy(post_engr_text, "You hear crackling!");
                break;

                /* type = MARK wands */
                /* type = ENGR_BLOOD wands */
            }
        } else /* end if zappable */ if (!can_reach_floor()) {
            pline("You can't reach the %s!", surface(u.ux, u.uy));
            /* If it's a wrestable wand, the player wasted a turn trying. */
            if (wrestable(otmp))
                return 1;
            else
                return 0;
        }
        break;

    case WEAPON_CLASS:
        if (is_blade(otmp)) {
            if ((int)otmp->spe > -3)
                type = ENGRAVE;
            else
                pline("Your %s too dull for engraving.", aobjnam(otmp, "are"));
        }
        break;

    case TOOL_CLASS:
        if (otmp == ublindf) {
            pline("That is a bit difficult to engrave with, don't you think?");
            return 0;
        }
        switch (otmp->otyp) {
        case MAGIC_MARKER:
            if (otmp->spe <= 0)
                pline("Your marker has dried out.");
            else
                type = MARK;
            break;
        case TOWEL:
            /* Can't really engrave with a towel */
            ptext = FALSE;
            if (oep)
                if ((oep->engr_type == DUST) || (oep->engr_type == ENGR_BLOOD)
                    || (oep->engr_type == MARK)) {
                    if (!Blind)
                        pline("You wipe out the message here.");
                    else
                        pline("Your %s %s %s.", xname(otmp),
                              otense(otmp, "get"),
                              is_ice(level, u.ux, u.uy) ? "frosty" : "dusty");
                    dengr = TRUE;
                } else
                    pline("Your %s can't wipe out this engraving.",
                          xname(otmp));
            else
                pline("Your %s %s %s.", xname(otmp), otense(otmp, "get"),
                      is_ice(level, u.ux, u.uy) ? "frosty" : "dusty");
            break;
        default:
            break;
        }
        break;

    case VENOM_CLASS:
        if (wizard) {
            pline("Writing a poison pen letter??");
            break;
        }
    case ILLOBJ_CLASS:
        impossible("You're engraving with an illegal object!");
        break;
    }

    if (IS_GRAVE(level->locations[u.ux][u.uy].typ)) {
        if (type == ENGRAVE || type == 0)
            type = HEADSTONE;
        else {
            /* ensures the "cannot wipe out" case */
            type = DUST;
            dengr = FALSE;
            teleengr = FALSE;
            buf[0] = (char)0;
        }
    }

    /* End of implement setup */

    /* Identify stylus */
    if (doknown) {
        makeknown(otmp->otyp);
        more_experienced(0, 10);
    }

    if (teleengr) {
        rloc_engr(oep);
        oep = NULL;
    }

    if (dengr) {
        del_engr(oep, level);
        oep = NULL;
    }

    /* Something has changed the engraving here */
    if (*buf) {
        make_engr_at(level, u.ux, u.uy, buf, moves, type);
        pline("The engraving now reads: \"%s\".", buf);
        ptext = FALSE;
    }

    if (zapwand && (otmp->spe < 0)) {
        pline("%s %sturns to dust.", (xname(otmp)),
              Blind ? "" : "glows violently, then ");
        if (!IS_GRAVE(level->locations[u.ux][u.uy].typ))
            pline
                ("You are not going to get anywhere trying to write in the %s with your dust.",
                 is_ice(level, u.ux, u.uy) ? "frost" : "dust");
        useup(otmp);
        ptext = FALSE;
    }

    if (!ptext) {       /* Early exit for some implements. */
        if (otmp->oclass == WAND_CLASS && !can_reach_floor())
            pline("You can't reach the %s!", surface(u.ux, u.uy));
        return 1;
    }

    /* Special effects should have deleted the current engraving (if possible)
       by now. */

    if (oep) {
        char c = 'n';

        /* Give player the choice to add to engraving. */

        if (type == HEADSTONE) {
            /* no choice, only append */
            c = 'y';
        } else if ((type == oep->engr_type) &&
                   (!Blind || (oep->engr_type == BURN) ||
                    (oep->engr_type == ENGRAVE))) {
            if (auto_elbereth)
                c = 'y';
            else
                c = yn_function("Do you want to add to the current engraving?",
                                ynqchars, 'y');
            if (c == 'q') {
                pline("Never mind.");
                return 0;
            }
        }

        if (c == 'n' || Blind) {

            if ((oep->engr_type == DUST) || (oep->engr_type == ENGR_BLOOD) ||
                (oep->engr_type == MARK)) {
                if (!Blind) {
                    pline("You wipe out the message that was %s here.",
                          ((oep->engr_type == DUST) ? "written in the dust" :
                           ((oep->engr_type == ENGR_BLOOD) ?
                            "scrawled in blood" : "written")));
                    del_engr(oep, level);
                    oep = NULL;
                } else
                    /* Don't delete engr until after we *know* we're engraving */
                    eow = TRUE;
            } else if ((type == DUST) || (type == MARK) || (type == ENGR_BLOOD)) {
                pline("You cannot wipe out the message that is %s the %s here.",
                      oep->engr_type == BURN ? (is_ice(level, u.ux, u.uy) ?
                                                "melted into" : "burned into") :
                      "engraved in",
                      surface(u.ux, u.uy));
                return 1;
            } else if ((type != oep->engr_type) || (c == 'n')) {
                if (!Blind || can_reach_floor())
                    pline("You will overwrite the current message.");
                eow = TRUE;
            }
        }
    }

    eloc = surface(u.ux, u.uy);
    switch (type) {
    default:
        everb = (oep &&
                 !eow ? "add to the weird writing on" : "write strangely on");
        break;
    case DUST:
        everb = (oep && !eow ? "add to the writing in" : "write in");
        eloc = is_ice(level, u.ux, u.uy) ? "frost" : "dust";
        break;
    case HEADSTONE:
        everb = (oep && !eow ? "add to the epitaph on" : "engrave on");
        break;
    case ENGRAVE:
        everb = (oep && !eow ? "add to the engraving in" : "engrave in");
        break;
    case BURN:
        everb = (oep && !eow ? (is_ice(level, u.ux, u.uy) ?
                                "add to the text melted into" :
                                "add to the text burned into") :
                 (is_ice(level, u.ux, u.uy) ? "melt into" : "burn into"));
        break;
    case MARK:
        everb = (oep && !eow ? "add to the graffiti on" : "scribble on");
        break;
    case ENGR_BLOOD:
        everb = (oep && !eow ? "add to the scrawl on" : "scrawl on");
        break;
    }

    /* Tell adventurer what is going on */
    if (otmp != &zeroobj)
        pline("You %s the %s with %s.", everb, eloc, doname(otmp));
    else
        pline("You %s the %s with your %s.", everb, eloc,
              makeplural(body_part(FINGER)));

    /* Prompt for engraving! */
    sprintf(qbuf, "What do you want to %s the %s here?", everb, eloc);
    if (auto_elbereth)
        strcpy(ebuf, "Elbereth");
    else
        getlin(qbuf, ebuf);

    /* Count the actual # of chars engraved not including spaces */
    len = strlen(ebuf);
    for (sp = ebuf; *sp; sp++)
        if (isspace(*sp))
            len -= 1;

    if (len == 0 || strchr(ebuf, '\033')) {
        if (zapwand) {
            if (!Blind)
                pline("%s, then %s.", Tobjnam(otmp, "glow"),
                      otense(otmp, "fade"));
            return 1;
        } else {
            pline("Never mind.");
            if (otmp && otmp->oclass == WAND_CLASS && wrestable(otmp))
                return 1;       /* disallow zero turn wrest */
            else
                return 0;
        }
    }

    /* A single `x' is the traditional signature of an illiterate person */
    if (len != 1 || (!strchr(ebuf, 'x') && !strchr(ebuf, 'X')))
        u.uconduct.literate++;

    /* Mix up engraving if surface or state of mind is unsound. Note: this
       won't add or remove any spaces. */
    for (sp = ebuf; *sp; sp++) {
        if (isspace(*sp))
            continue;
        if (((type == DUST || type == ENGR_BLOOD) && !rn2(25)) ||
            (Blind && !rn2(11)) || (Confusion && !rn2(7)) ||
            (Stunned && !rn2(4)) || (Hallucination && !rn2(2)))
            *sp = ' ' + rnd(96 - 2);    /* ASCII '!' thru '~' (excludes ' ' and 
                                           DEL) */
    }

    /* Previous engraving is overwritten */
    if (eow) {
        del_engr(oep, level);
        oep = NULL;
    }

    /* Figure out how long it took to engrave, and if player has engraved too
       much. */
    switch (type) {
    default:
        multi = -(len / 10);
        if (multi)
            nomovemsg = "You finish your weird engraving.";
        break;
    case DUST:
        multi = -(len / 10);
        if (multi)
            nomovemsg = "You finish writing in the dust.";
        break;
    case HEADSTONE:
    case ENGRAVE:
        multi = -(len / 10);
        if ((otmp->oclass == WEAPON_CLASS) &&
            ((otmp->otyp != ATHAME) || otmp->cursed)) {
            multi = -len;
            maxelen = ((otmp->spe + 3) * 2) + 1;
            /* -2 = 3, -1 = 5, 0 = 7, +1 = 9, +2 = 11 Note: this does not allow 
               a +0 anything (except an athame) to engrave "Elbereth" all at
               once.  However, you could now engrave "Elb", then "ere", then
               "th". */
            pline("Your %s dull.", aobjnam(otmp, "get"));
            if (otmp->unpaid) {
                struct monst *shkp = shop_keeper(level, *u.ushops);

                if (shkp) {
                    pline("You damage it, you pay for it!");
                    bill_dummy_object(otmp);
                }
            }
            if (len > maxelen) {
                multi = -maxelen;
                otmp->spe = -3;
            } else if (len > 1)
                otmp->spe -= len >> 1;
            else
                otmp->spe -= 1; /* Prevent infinite engraving */
        } else if ((otmp->oclass == RING_CLASS) || (otmp->oclass == GEM_CLASS))
            multi = -len;
        if (multi)
            nomovemsg = "You finish engraving.";
        break;
    case BURN:
        multi = -(len / 10);
        if (multi)
            nomovemsg =
                is_ice(level, u.ux,
                       u.
                       uy) ? "You finish melting your message into the ice." :
                "You finish burning your message into the floor.";
        break;
    case MARK:
        multi = -(len / 10);
        if ((otmp->oclass == TOOL_CLASS) && (otmp->otyp == MAGIC_MARKER)) {
            maxelen = (otmp->spe) * 2;  /* one charge / 2 letters */
            if (len > maxelen) {
                pline("Your marker dries out.");
                otmp->spe = 0;
                multi = -(maxelen / 10);
            } else if (len > 1)
                otmp->spe -= len >> 1;
            else
                otmp->spe -= 1; /* Prevent infinite grafitti */
        }
        if (multi)
            nomovemsg = "You finish defacing the dungeon.";
        break;
    case ENGR_BLOOD:
        multi = -(len / 10);
        if (multi)
            nomovemsg = "You finish scrawling.";
        break;
    }

    /* Chop engraving down to size if necessary */
    if (len > maxelen) {
        for (sp = ebuf; (maxelen && *sp); sp++)
            if (!isspace(*sp))
                maxelen--;
        if (!maxelen && *sp) {
            *sp = (char)0;
            if (multi)
                nomovemsg = "You cannot write any more.";
            pline("You only are able to write \"%s\"", ebuf);
        }
    }

    /* Add to existing engraving */
    if (oep)
        strcpy(buf, oep->engr_txt);

    strncat(buf, ebuf, (BUFSZ - (int)strlen(buf) - 1));

    make_engr_at(level, u.ux, u.uy, buf, (moves - multi), type);

    if (strstri(buf, "Elbereth")) {
        u.uconduct.elbereths++;
    }

    if (post_engr_text[0])
        pline(post_engr_text);

    if (doknown_after) {
        makeknown(otmp->otyp);
        more_experienced(0, 10);
    }

    if (doblind && !resists_blnd(&youmonst)) {
        pline("You are blinded by the flash!");
        make_blinded((long)rnd(50), FALSE);
        if (!Blind)
            pline("Your vision quickly clears.");
    }

    return 1;
}

int
doengrave(struct obj *o)
{
    return doengrave_core(o, 0);
}

int
doelbereth(void)
{
    /* TODO: Athame? */
    return doengrave_core(&zeroobj, 1);
}

void
save_engravings(struct memfile *mf, struct level *lev)
{
    struct engr *ep;
    char *txtbase;      /* ep->engr_txt may have been incremented */

    mfmagic_set(mf, ENGRAVE_MAGIC);

    for (ep = lev->lev_engr; ep; ep = ep->nxt_engr) {
        if (ep->engr_lth && ep->engr_txt[0]) {
            /* To distinguish engravingss from each other in tags, we use x/y/z 
               coords */
            mtag(mf,
                 ledger_no(&lev->z) + ((int)ep->engr_x << 8) +
                 ((int)ep->engr_y << 16), MTAG_ENGRAVING);
            mwrite32(mf, ep->engr_lth);
            mwrite8(mf, ep->engr_x);
            mwrite8(mf, ep->engr_y);
            mwrite8(mf, ep->engr_type);
            txtbase = (char *)(ep + 1);
            mwrite(mf, txtbase, ep->engr_lth);
        }
    }
    mwrite32(mf, 0);    /* no more engravings */
}


void
free_engravings(struct level *lev)
{
    struct engr *ep2, *ep = lev->lev_engr;

    while (ep) {
        ep2 = ep->nxt_engr;
        dealloc_engr(ep);
        ep = ep2;
    }
    lev->lev_engr = NULL;
}


void
rest_engravings(struct memfile *mf, struct level *lev)
{
    struct engr *ep;
    unsigned lth;

    mfmagic_check(mf, ENGRAVE_MAGIC);
    lev->lev_engr = NULL;
    while (1) {
        lth = mread32(mf);
        if (!lth)       /* no more engravings */
            return;

        ep = newengr(lth);
        ep->engr_lth = lth;
        ep->engr_x = mread8(mf);
        ep->engr_y = mread8(mf);
        ep->engr_type = mread8(mf);
        ep->engr_txt = (char *)(ep + 1);
        mread(mf, ep->engr_txt, lth);

        ep->nxt_engr = lev->lev_engr;
        lev->lev_engr = ep;
        while (ep->engr_txt[0] == ' ')
            ep->engr_txt++;
        /* mark as finished for bones levels -- no problem for normal levels as 
           the player must have finished engraving to be able to move again */
        ep->engr_time = moves;
    }
}

void
del_engr(struct engr *ep, struct level *lev)
{
    if (ep == lev->lev_engr) {
        lev->lev_engr = ep->nxt_engr;
    } else {
        struct engr *ept;

        for (ept = lev->lev_engr; ept; ept = ept->nxt_engr)
            if (ept->nxt_engr == ep) {
                ept->nxt_engr = ep->nxt_engr;
                break;
            }
        if (!ept) {
            impossible("Error in del_engr?");
            return;
        }
    }
    dealloc_engr(ep);
}

/* randomly relocate an engraving */
void
rloc_engr(struct engr *ep)
{
    int tx, ty, tryct = 200;

    do {
        if (--tryct < 0)
            return;
        tx = rn1(COLNO - 3, 2);
        ty = rn2(ROWNO);
    } while (engr_at(level, tx, ty) || !goodpos(level, tx, ty, NULL, 0));

    ep->engr_x = tx;
    ep->engr_y = ty;
}


/* Epitaphs for random headstones */
static const char *const epitaphs[] = {
    "Rest in peace",
    "R.I.P.",
    "Rest In Pieces",
    "Note -- there are NO valuable items in this grave",
    "1994-1995. The Longest-Lived Hacker Ever",
    "The Grave of the Unknown Hacker",
    "We weren't sure who this was, but we buried him here anyway",
    "Sparky -- he was a very good dog",
    "Beware of Electric Third Rail",
    "Made in Taiwan",
    "Og friend. Og good dude. Og died. Og now food",
    "Beetlejuice Beetlejuice Beetlejuice",
    "Look out below!",
    "Please don't dig me up. I'm perfectly happy down here. -- Resident",
    "Postman, please note forwarding address: Gehennom, Asmodeus's Fortress, fifth lemure on the left",
    "Mary had a little lamb/Its fleece was white as snow/When Mary was in trouble/The lamb was first to go",
    "Be careful, or this could happen to you!",
    "Soon you'll join this fellow in hell! -- the Wizard of Yendor",
    "Caution! This grave contains toxic waste",
    "Sum quod eris",
    "Here lies an Atheist, all dressed up and no place to go",
    "Here lies Ezekiel, age 102.  The good die young.",
    "Here lies my wife: Here let her lie! Now she's at rest and so am I.",
    "Here lies Johnny Yeast. Pardon me for not rising.",
    "He always lied while on the earth and now he's lying in it",
    "I made an ash of myself",
    "Soon ripe. Soon rotten. Soon gone. But not forgotten.",
    "Here lies the body of Jonathan Blake. Stepped on the gas instead of the brake.",
    "Go away!"
};

/* Create a headstone at the given location.
 * The caller is responsible for newsym(x, y).
 */
void
make_grave(struct level *lev, int x, int y, const char *str)
{
    /* Can we put a grave here? */
    if ((lev->locations[x][y].typ != ROOM && lev->locations[x][y].typ != GRAVE)
        || t_at(lev, x, y))
        return;

    /* Make the grave */
    lev->locations[x][y].typ = GRAVE;

    /* Engrave the headstone */
    if (!str)
        str = epitaphs[rn2(SIZE(epitaphs))];
    del_engr_at(lev, x, y);
    make_engr_at(lev, x, y, str, 0L, HEADSTONE);
    return;
}

/*engrave.c*/
