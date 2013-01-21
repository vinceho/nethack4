/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* Contains code for 'd', 'D' (drop), '>', '<' (up, down) */

#include "hack.h"
#include "lev.h"

static void trycall(struct obj *);
static void dosinkring(struct obj *);
static int drop(struct obj *);
static int wipeoff(void);

static int menu_drop(int);
static void final_level(void);

/* static boolean badspot(XCHAR_P,XCHAR_P); */

static const char drop_types[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

/* 'd' command: drop one inventory item */
int
dodrop(struct obj *obj)
{
    int result, i = (invent) ? 0 : (SIZE(drop_types) - 1);

    if (*u.ushops)
        sellobj_state(SELL_DELIBERATE);
    if (!obj)
        obj = getobj(&drop_types[i], "V{drop}");
    result = drop(obj);
    if (*u.ushops)
        sellobj_state(SELL_NORMAL);
    reset_occupations();

    return result;
}


/* Called when a boulder is dropped, thrown, or pushed.  If it ends up
 * in a pool, it either fills the pool up or sinks away.  In either case,
 * it's gone for good...  If the destination is not a pool, returns FALSE.
 */
boolean
boulder_hits_pool(struct obj * otmp, int rx, int ry, boolean pushing)
{
    if (!otmp || otmp->otyp != BOULDER)
        impossible("S{Not a boulder?}");
    else if (!Is_waterlevel(&u.uz) &&
             (is_pool(level, rx, ry) || is_lava(level, rx, ry))) {
        boolean lava = is_lava(level, rx, ry), fills_up;
        /* what contains a uniquifier, so we get appropriate adverbs
           in some of the cases below */
        const char *what = waterbody_name(rx, ry);
        schar ltyp = level->locations[rx][ry].typ;
        int chance = rn2(10);   /* water: 90%; lava: 10% */

        fills_up = lava ? chance == 0 : chance != 0;

        if (fills_up) {
            struct trap *ttmp = t_at(level, rx, ry);

            if (ltyp == DRAWBRIDGE_UP) {
                /* clear lava */
                level->locations[rx][ry].drawbridgemask &= ~DB_UNDER;

                level->locations[rx][ry].drawbridgemask |= DB_FLOOR;
            } else
                level->locations[rx][ry].typ = ROOM;

            if (ttmp)
                delfloortrap(level, ttmp);
            bury_objs(level, rx, ry);

            newsym(rx, ry);
            if (pushing) {
                if (u.usteed) {
                    char *bp = y_monnam(u.usteed);
                    pline("C{N=%s,V{V{V{push},N=%s},D{e,E{into},N=%s}}}.",
                          bp, (xname(otmp)), what);
                } else {
                    pline("C{N=%s,V{V{V{push},N=%s},D{e,E{into},N=%s}}}.",
                          you, (xname(otmp)), what);
                }
                /* 'You can cross it now!', 'He can cross it now!'. 'It can
                   cross it now!' will be broken by pronoun tiebreakers,
                   i.e. 'Dudley the newt can cross the pool now!' */
                if (flags.verbose && !Blind)
                    pline("C{N=%s,V{V{V{can},V{V{cross},N=%s}},D{now}}}!",
                          you, what);
                /* no splashing in this case */
            }
        }
        if (!fills_up || !pushing) {    /* splashing occurs */
            if (!u.uinwater) {
                if (pushing ? !Blind : cansee(rx, ry)) {
                    pline("C{N=%s,V{V{V=%s,N=%s},D{o,N{N{i,splash},A{loud}}}}}.",
                          xname(otmp), fills_up ? "V{fill}" : "V{fall into}",
                          what);
                } else if (flags.soundok)
                    You_hear(".", lava ?
                             "N{N{i,splash},A{sizzling}}" : "N{i,splash}");
                wake_nearto(rx, ry, 40);
            }

            if (fills_up && u.uinwater && distu(rx, ry) == 0) {
                u.uinwater = 0;
                doredraw();
                vision_full_recalc = 1;
                pline("C{N=%s,V{V{V{V{find},N=%s},D{e,E{on},N{N{o,land},A{dry}}}},"
                      "D{again}}}!", you, you);
            } else if (lava && distu(rx, ry) <= 2) {
                pline("C{s,V{V{V{hit},D{t,N{N{o,lava},A{molten}}}},N=%s}}%c",
                      you, Fire_resistance ? '.' : '!');
                burn_away_slime();
                losehp(dice((Fire_resistance ? 1 : 3), 6),
                       "N{N{o,lava},A{molten}}", KILLED_BY);
            } else if (!fills_up && flags.verbose &&
                       (pushing ? !Blind : cansee(rx, ry)))
                pline("C{N=%s,V{V{sink},D{-,D{t,N{i,trace}}}}}!", xname(otmp));
        }

        /* boulder is now gone */
        if (pushing)
            delobj(otmp);
        else
            obfree(otmp, NULL);
        return TRUE;
    }
    return FALSE;
}

/* Used for objects which sometimes do special things when dropped; must be
 * called with the object not in any chain.  Returns TRUE if the object goes
 * away.
 */
boolean
flooreffects(struct obj * obj, int x, int y, const char *verb)
{
    struct level *lev = level;
    struct trap *t;
    struct monst *mtmp;

    if (obj->where != OBJ_FREE)
        panic("S{flooreffects: obj not free}");

    /* make sure things like water_damage() have no pointers to follow */
    obj->nobj = obj->nexthere = NULL;

    if (obj->otyp == BOULDER && boulder_hits_pool(obj, x, y, FALSE))
        return TRUE;
    else if (obj->otyp == BOULDER && (t = t_at(lev, x, y)) != 0 &&
             (t->ttyp == PIT || t->ttyp == SPIKED_PIT || t->ttyp == TRAPDOOR ||
              t->ttyp == HOLE)) {
        if (((mtmp = m_at(lev, x, y)) && mtmp->mtrapped) ||
            (u.utrap && u.ux == x && u.uy == y)) {
            if (*verb)
                pline("C{N{boulder},V{V=%s,D{e,E{into},N{pit}}}}.", verb);
            if (mtmp) {
                if (!passes_walls(mtmp->data) && !throws_rocks(mtmp->data)) {
                    if (hmon(mtmp, obj, TRUE) && !is_whirly(mtmp->data))
                        return FALSE;   /* still alive */
                }
                mtmp->mtrapped = 0;
            } else {
                if (!Passes_walls && !throws_rocks(youmonst.data)) {
                    losehp(rnd(15), "V{V{squish},D{e,E{under},N{i,boulder}}}",
                           NO_KILLER_PREFIX);
                    return FALSE;       /* player remains trapped */
                } else
                    u.utrap = 0;
            }
        }
        if (*verb) {
            if (Blind && couldsee(x, y)) {
                if (flags.soundok) {
                    if ((x == u.ux) && (y == u.uy))
                        You_hear(".", "N{N{i,CRASH},E{beneath},N=%s}", you);
                    else
                        You_hear("!", "N{N{i,CRASH},A{nearby}}");
                }
            } else if (cansee(x, y)) {
                char *verb = t->ttyp == TRAPDOOR ? "V{V{plug},N{i,trap door}}"
                    : t->ttyp == HOLE ? "V{V{plug},N{i,hole}}"
                    : "V{V{fill},N{i,pit}}";
                /* TODO: This produces the right output only coincidentally,
                   and should really be rewritten with proper uniquifiers. */
                if (t->tseen)
                    pline("C{N{boulder},V{+,V{trigger},V=%s}}.", verb);
                else
                    pline("C{N{boulder},V=%s}.", verb);
            } else if (flags.soundok) {
                You_hear("!", "N{N{i,CRASH},A{distant}}");
            }
        }
        deltrap(lev, t);
        obfree(obj, NULL);
        bury_objs(lev, x, y);
        newsym(x, y);
        return TRUE;
    } else if (is_lava(lev, x, y)) {
        return fire_damage(obj, FALSE, FALSE, x, y);
    } else if (is_pool(lev, x, y)) {
        /* Reasonably bulky objects (arbitrary) splash when dropped. If you're
           floating above the water even small things make noise. Stuff dropped 
           near fountains always misses */
        if ((Blind || (Levitation || Flying)) && flags.soundok &&
            ((x == u.ux) && (y == u.uy))) {
            if (!Underwater) {
                if (weight(obj) > 9) {
                    pline("S{Splash!}");
                } else if (Levitation || Flying) {
                    pline("S{Plop!}");
                }
            }
            map_background(x, y, 0);
            newsym(x, y);
        }
        return water_damage(obj, FALSE, FALSE);
    } else if (u.ux == x && u.uy == y && (!u.utrap || u.utraptype != TT_PIT) &&
               (t = t_at(lev, x, y)) != 0 && t->tseen &&
               (t->ttyp == PIT || t->ttyp == SPIKED_PIT)) {
        /* you escaped a pit and are standing on the precipice */
        if (Blind && flags.soundok)
            You_hear(".", "N{a,N=%s,V{V{tumble},D{downwards}}}", xname(obj));
        else
            pline("C{N=%s,V{V{tumble},D{e,E{into},N=%s}}}.", xname(obj),
                  the_your("N{pit}",t->madeby_u));
    }
    return FALSE;
}


void
doaltarobj(struct obj *obj)
{       /* obj is an object dropped on an altar */
    if (Blind)
        return;

    /* KMH, conduct */
    u.uconduct.gnostic++;

    if ((obj->blessed || obj->cursed) && obj->oclass != COIN_CLASS) {
        pline("C{N{x%dy%d|altar},V{V{V{flash},A=%s},D{Q{as},"
              "C{N=%s,V{V{hit},N{x%dy%d|altar}}}}}}.", u.ux, u.uy,
              hcolor(obj->blessed ? "A{amber}" : "A{black}"),
              doname(obj), u.ux, u.uy);
        if (!Hallucination)
            obj->bknown = 1;
    } else {
        pline("C{N=%s,V{V{land},D{e,E{on},N{altar}}}}.", doname(obj));
        obj->bknown = 1;
    }
    /* Also BCU one level deep inside containers */
    if (Has_contents(obj)) {
        int bcucount = 0;
        struct obj *otmp;

        for (otmp = obj->cobj; otmp; otmp = otmp->nobj) {
            if (otmp->blessed || otmp->cursed)
                bcucount++;
            if (!Hallucination)
                otmp->bknown = 1;
        }
        if (bcucount == 1) {
            pline("C{N=%s,V{V{see},"
                  "N{N{N{i,flash},A{V{color}}},E{inside},N=%s}}}.",
                  you, xname(obj));
        } else if (bcucount > 1) {
            pline("C{N=%s,V{V{see},"
                  "N{N{N{*,N{i,flash}},A{V{color}}},E{inside},N=%s}}}.",
                  you, xname(obj));
        }
    }

}

static void
trycall(struct obj *obj)
{
    if (!objects[obj->otyp].oc_name_known && !objects[obj->otyp].oc_uname)
        docall(obj);
}

static void
dosinkring(struct obj *obj)
{       /* obj is a ring being dropped over a kitchen sink */
    struct obj *otmp, *otmp2;
    boolean ideed = TRUE;

    pline("C{N=%s,V{V{V{drop},N=%s},D{e,E{down},N{drain}}}}.",
          you, doname(obj));
    obj->in_use = TRUE; /* block free identification via interrupt */
    switch (obj->otyp) {        /* effects that can be noticed without eyes */
    case RIN_SEARCHING:
        pline("C{N=%s,V{V{V{V{find},N=%s},D{easily}},D{again}}}!",
              you, xname(obj));
        goto giveback;
    case RIN_SLOW_DIGESTION:
        pline("C{s,V{V{regurgitate},N{ring}}}!");
    giveback:
        obj->in_use = FALSE;
        dropx(obj);
        trycall(obj);
        return;
    case RIN_LEVITATION:
        pline("C{N{sink},V{V{V{quiver},D{upward}},D{d,N{i,moment}}}}.");
        break;
    case RIN_POISON_RESISTANCE:
        pline("C{N=%s,V{V{smell},N{N{*,N=%s},A{V{rot}}}}}.", you,
              fruitname(FALSE));
        break;
    case RIN_AGGRAVATE_MONSTER:
        pline("C{N{N{*,N{i,fly}},A{several}},"
              "V{V{V{buzz},D{angrily}},D{e,E{around},N{sink}}}}.");
        break;
    case RIN_SHOCK_RESISTANCE:
        pline("C{N{o,static electricity},V{V{surround},N{sink}}}.");
        break;
    case RIN_CONFLICT:
        You_hear(".",
                 "N{a,N{N{*,N{i,noise}},A{loud}},V{V{come from},N{drain}}}");
        break;
    case RIN_SUSTAIN_ABILITY:  /* KMH */
        pline("C{N{water flow},V{V{seem},A{V{fix}}}}.");
        break;
    case RIN_GAIN_STRENGTH:
        pline("C{N{water flow},V{V{V{seem},A{c,A=%s}},D{now}}}.",
              (obj->spe < 0) ? "A{weak}" : "A{strong}");
        break;
    case RIN_GAIN_CONSTITUTION:
        pline("C{N{water flow},V{V{V{seem},A{c,A=%s}},D{now}}}.",
              (obj->spe < 0) ? "A{less}" : "A{great}");
        break;
    case RIN_INCREASE_ACCURACY:        /* KMH */
        pline("C{N{water flow},V{V=%s,N{drain}}}.",
              (obj->spe < 0) ? "V{miss}" : "V{hit}");
        break;
    case RIN_INCREASE_DAMAGE:
        pline("C{N{o,N{force},N{water}},V{V{V{seem},A{c,A=%s}},D{now}}}.",
              (obj->spe < 0) ? "A{small}" : "A{great}");
        break;
    case RIN_HUNGER:
        ideed = FALSE;
        for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp2) {
            otmp2 = otmp->nexthere;
            if (otmp != uball && otmp != uchain && !obj_resists(otmp, 1, 99)) {
                if (!Blind) {
                    pline("C{N=%s,V{V{vanish from},N{sink}}}!", doname(otmp));
                    ideed = TRUE;
                }
                delobj(otmp);
            }
        }
        break;
    case MEAT_RING:
        /* Not the same as aggravate monster; besides, it's obvious. */
        pline("C{N{N{*,N{i,fly}},A{several}},"
              "V{V{buzz},D{e,E{around},N{sink}}}}.");
        break;
    default:
        ideed = FALSE;
        break;
    }
    if (!Blind && !ideed && obj->otyp != RIN_HUNGER) {
        ideed = TRUE;
        switch (obj->otyp) {    /* effects that need eyes */
        case RIN_ADORNMENT:
            pline("C{N{*,N{faucet}},V{V{V{flash},D{brightly}},"
                  "D{d,N{i,moment}}}}.");
            break;
        case RIN_REGENERATION:
            pline("C{N{sink},V{V{look},A{as good as new}}}.");
            break;
        case RIN_INVISIBILITY:
            pline("C{N=%s,V{-,V{V{V{see},N{o,anything}},"
                  "V{V{happen to},N{sink}}}}}.", you);
            break;
        case RIN_FREE_ACTION:
            pline("C{N{ring},V{V{slide},D{e,E{right down},N{drain}}}}!");
            break;
        case RIN_SEE_INVISIBLE:
            pline("C{N=%s,V{V{V{see},N{N{o,air},A{some}}},D{a,N{sink}}}}.",
                  you);
            break;
        case RIN_STEALTH:
            pline("C{N{sink},V{V{V{seem},V{V{blend into},N{floor}}},"
                  "D{d,N{i,moment}}}}.");
            break;
        case RIN_FIRE_RESISTANCE:
            pline("C{N{N{faucet},A{N{N{water},A{hot}}}},"
                  "V{V{V{flash},D{brightly}},D{d,N{i,moment}}}}.");
            break;
        case RIN_COLD_RESISTANCE:
            pline("C{N{N{faucet},A{N{N{water},A{cold}}}},"
                  "V{V{V{flash},D{brightly}},D{d,N{i,moment}}}}.");
            break;
        case RIN_PROTECTION_FROM_SHAPE_CHANGERS:
            pline("C{N{sink},V{-,V{V{look like},N{i,fountain}}}}.");
            break;
        case RIN_PROTECTION:
            pline("C{N{sink},V{V{V{glow},A=%s},D{d,N{i,moment}}}}.",
                  hcolor((obj->spe < 0) ? "A{black}" : "A{silver}"));
            break;
        case RIN_WARNING:
            pline("C{N{sink},V{V{V{glow},A=%s},D{d,N{i,moment}}}}.",
                  hcolor("A{white}"));
            break;
        case RIN_TELEPORTATION:
            pline("C{N{sink},V{V{vanish},D{momentarily}}}.");
            break;
        case RIN_TELEPORT_CONTROL:
            verbalize("S{Beam the sink up, Scotty!}");
            break;
        case RIN_POLYMORPH:
            pline("C{N{sink},V{V{V{look like},N{i,fountain}},D{momentarily}}}.");
            break;
        case RIN_POLYMORPH_CONTROL:
            pline("C{N{sink},V{V{V{look like},"
                  "N{a,N{i,geyser},V{V{erupt},D{regularly}}}},"
                  "D{momentarily}}}.");
            break;
        }
    }
    if (ideed)
        trycall(obj);
    else
        You_hear(".", "N{a,N{ring},V{V{bounce down},N{drainpipe}}}");
    if (!rn2(20)) {
        pline("C{N{sink},V{+,V{back up},V{V{leave},N=%s}}}.", doname(obj));
        obj->in_use = FALSE;
        dropx(obj);
    } else
        useup(obj);
}


/* some common tests when trying to drop or throw items */
boolean
canletgo(struct obj *obj, const char *word)
{
    if (obj->owornmask & W_WORN) {
        if (*word)
            Norep("C{N=%s,V{-,V{V{can},"
                  "V{V=%s,N{s,C{c,N=%s,V{V{wear},N{o,something}}}}}}}}.",
                  you, word, you);
        return FALSE;
    }
    if (obj->otyp == LOADSTONE && obj->cursed) {
        /* getobj() kludge sets corpsenm to user's specified count when
           refusing to split a stack of cursed loadstones */
        if (*word) {
            /* getobj() ignores a count for throwing since that is implicitly
               forced to be 1; replicate its kludge... */
            if (!strcmp(word, "V{throw}") && obj->quan > 1L)
                obj->corpsenm = 1;
            pline("C{N=%s,V{-,V{V{can},V{V=%s,N=%s}}}}!", you, word,
                  obj->corpsenm ? "N{f,N{*,N{stone}},A{any}}" :
                  obj->quan > 1 ? "N{*,N{stone}}" : "N{stone}");
        }
        obj->corpsenm = 0;      /* reset */
        obj->bknown = 1;
        return FALSE;
    }
    if (obj->otyp == LEASH && obj->leashmon != 0) {
        if (*word)
            pline("C{s,V{V{V{tie},N{leash}},D{e,E{around},N{o,N=%s,N=%s}}}}.",
                  you, body_part(HAND));
        return FALSE;
    }
    if (obj->owornmask & W_SADDLE) {
        /* Literal 'something', we don't want 'you cannot drop it you
           are sitting on' */
        if (*word)
            pline("C{N=%s,V{-,V{V{can},V{V=%s,"
                  "N{s,C{c,N=%s,V{V{sit on},N{o,something}}}}}}}}.",
                  you, word, you);
        return FALSE;
    }
    return TRUE;
}

static int
drop(struct obj *obj)
{
    if (!obj)
        return 0;
    if (!canletgo(obj, "V{drop}"))
        return 0;
    if (obj == uwep) {
        if (welded(uwep)) {
            weldmsg(obj);
            return 0;
        }
        setuwep(NULL);
    }
    if (obj == uquiver) {
        setuqwep(NULL);
    }
    if (obj == uswapwep) {
        setuswapwep(NULL);
    }

    if (u.uswallow) {
        /* barrier between you and the floor */
        if (flags.verbose) {
            pline("C{N=%s,V{V{V{drop},N=%s},D{e,E{into},N{o,N=%s,N=%s}}}}.",
                  you, doname(obj), mbodypart(u.ustuck, STOMACH),
                  mon_nam(u.ustuck));
        }
    } else {
        if ((obj->oclass == RING_CLASS || obj->otyp == MEAT_RING) &&
            IS_SINK(level->locations[u.ux][u.uy].typ)) {
            dosinkring(obj);
            return 1;
        }
        if (!can_reach_floor()) {
            if (flags.verbose)
                pline("C{N=%s,V{V{drop},N=%s}}.", you, doname(obj));

            /* Ensure update when we drop gold objects */
            if (obj->oclass == COIN_CLASS)
                iflags.botl = 1;
            freeinv(obj);
            hitfloor(obj);
            return 1;
        }
        if (!IS_ALTAR(level->locations[u.ux][u.uy].typ) && flags.verbose)
            pline("C{N=%s,V{V{drop},N=%s}}.", you, doname(obj));
    }
    dropx(obj);
    return 1;
}

/* Called in several places - may produce output */
/* eg ship_object() and dropy() -> sellobj() both produce output */
void
dropx(struct obj *obj)
{
    /* Ensure update when we drop gold objects */
    if (obj->oclass == COIN_CLASS)
        iflags.botl = 1;
    freeinv(obj);
    if (!u.uswallow) {
        if (ship_object(obj, u.ux, u.uy, FALSE))
            return;
        if (IS_ALTAR(level->locations[u.ux][u.uy].typ))
            doaltarobj(obj);    /* set bknown */
    }
    dropy(obj);
}

void
dropy(struct obj *obj)
{
    if (obj == uwep)
        setuwep(NULL);
    if (obj == uquiver)
        setuqwep(NULL);
    if (obj == uswapwep)
        setuswapwep(NULL);

    if (!u.uswallow && flooreffects(obj, u.ux, u.uy, "V{drop}"))
        return;
    /* uswallow check done by GAN 01/29/87 */
    if (u.uswallow) {
        boolean could_petrify = FALSE;
        boolean could_poly = FALSE;
        boolean could_slime = FALSE;
        boolean could_grow = FALSE;
        boolean could_heal = FALSE;

        if (obj != uball) {     /* mon doesn't pick up ball */
            if (obj->otyp == CORPSE) {
                could_petrify = touch_petrifies(&mons[obj->corpsenm]);
                could_poly = polyfodder(obj);
                could_slime = (obj->corpsenm == PM_GREEN_SLIME);
                could_grow = (obj->corpsenm == PM_WRAITH);
                could_heal = (obj->corpsenm == PM_NURSE);
            }
            mpickobj(u.ustuck, obj);
            if (is_animal(u.ustuck->data)) {
                if (could_poly || could_slime) {
                    newcham(u.ustuck, could_poly ? NULL : &mons[PM_GREEN_SLIME],
                            FALSE, could_slime);
                    delobj(obj);        /* corpse is digested */
                } else if (could_petrify) {
                    minstapetrify(u.ustuck, TRUE);
                    /* Don't leave a cockatrice corpse in a statue */
                    if (!u.uswallow)
                        delobj(obj);
                } else if (could_grow) {
                    grow_up(u.ustuck, NULL);
                    delobj(obj);        /* corpse is digested */
                } else if (could_heal) {
                    u.ustuck->mhp = u.ustuck->mhpmax;
                    delobj(obj);        /* corpse is digested */
                }
            }
        }
    } else {
        place_object(obj, level, u.ux, u.uy);
        if (obj == uball)
            drop_ball(u.ux, u.uy, 0, 0);
        else
            sellobj(obj, u.ux, u.uy);
        stackobj(obj);
        if (Blind && Levitation)
            map_object(obj, 0);
        newsym(u.ux, u.uy);     /* remap location under self */
    }
}

/* things that must change when not held; recurse into containers.
   Called for both player and monsters */
void
obj_no_longer_held(struct obj *obj)
{
    if (!obj) {
        return;
    } else if ((Is_container(obj) || obj->otyp == STATUE) && obj->cobj) {
        struct obj *contents;

        for (contents = obj->cobj; contents; contents = contents->nobj)
            obj_no_longer_held(contents);
    }
    switch (obj->otyp) {
    case CRYSKNIFE:
        /* KMH -- Fixed crysknives have only 10% chance of reverting */
        /* only changes when not held by player or monster */
        if (!obj->oerodeproof || !rn2(10)) {
            obj->otyp = WORM_TOOTH;
            obj->oerodeproof = 0;
        }
        break;
    }
}

/* 'D' command: drop several things */
int
doddrop(void)
{
    int result = 0;

    add_valid_menu_class(0);    /* clear any classes already there */
    if (*u.ushops)
        sellobj_state(SELL_DELIBERATE);
    result = menu_drop(result);
    if (*u.ushops)
        sellobj_state(SELL_NORMAL);
    reset_occupations();

    return result;
}

/* Drop things from the hero's inventory, using a menu. */
static int
menu_drop(int retry)
{
    int n, i, n_dropped = 0;
    long cnt;
    struct obj *otmp, *otmp2;
    int pick_list[30];
    struct object_pick *obj_pick_list;
    boolean all_categories = TRUE;
    boolean drop_everything = FALSE;

    if (retry) {
        all_categories = (retry == -2);
    } else if (flags.menu_style == MENU_FULL) {
        all_categories = FALSE;
        n = query_category("C{q,C{i,V{V{drop},N{l,N{N{o,type},A{which}},"
                           "N{*,N{o,item}}}}}}?", invent,
                           UNPAID_TYPES | ALL_TYPES | CHOOSE_ALL | UNIDENTIFIED
                           | BUC_BLESSED | BUC_CURSED | BUC_UNCURSED |
                           BUC_UNKNOWN, pick_list, PICK_ANY);
        if (!n)
            goto drop_done;
        for (i = 0; i < n; i++) {
            if (pick_list[i] == ALL_TYPES_SELECTED)
                all_categories = TRUE;
            else if (pick_list[i] == 'A')
                drop_everything = TRUE;
            else
                add_valid_menu_class(pick_list[i]);
        }
    }

    if (drop_everything) {
        for (otmp = invent; otmp; otmp = otmp2) {
            otmp2 = otmp->nobj;
            n_dropped += drop(otmp);
        }
    } else {
        /* should coordinate with perm invent, maybe not show worn items */
        n = query_objlist("C{q,C{i,V{V{drop},N{N{*,N{o,item}},A{which}}}}}?",
                          invent, USE_INVLET | INVORDER_SORT, &obj_pick_list,
                          PICK_ANY,
                          all_categories ? allow_all : allow_category);
        if (n > 0) {
            for (i = 0; i < n; i++) {
                otmp = obj_pick_list[i].obj;
                cnt = obj_pick_list[i].count;
                if (cnt < otmp->quan) {
                    if (welded(otmp)) {
                        ;       /* don't split */
                    } else if (otmp->otyp == LOADSTONE && otmp->cursed) {
                        /* same kludge as getobj(), for canletgo()'s use */
                        otmp->corpsenm = (int)cnt;      /* don't split */
                    } else
                        otmp = splitobj(otmp, cnt);
                }
                n_dropped += drop(otmp);
            }
            free(obj_pick_list);
        }
    }

drop_done:
    return n_dropped;
}


/* on a ladder, used in goto_level */
static boolean at_ladder = FALSE;

int
dodown(void)
{
    struct trap *trap = 0;
    boolean stairs_down =
        ((u.ux == level->dnstair.sx && u.uy == level->dnstair.sy) ||
         (u.ux == level->sstairs.sx && u.uy == level->sstairs.sy &&
          !level->sstairs.up)), ladder_down = (u.ux == level->dnladder.sx &&
                                               u.uy == level->dnladder.sy);

    if (u.usteed && !u.usteed->mcanmove) {
        pline("C{N=%s,V{-,V{V{will},V{move}}}}!", mon_nam(u.usteed));
        return 0;
    } else if (u.usteed && u.usteed->meating) {
        pline("C{c,N=%s,V{V{eat},D{still}}}.", mon_nam(u.usteed));
        return 0;
    } else if (Levitation) {
        if ((HLevitation & I_SPECIAL) || (ELevitation & W_ARTI)) {
            /* end controlled levitation */
            if (ELevitation & W_ARTI) {
                struct obj *obj;

                for (obj = invent; obj; obj = obj->nobj) {
                    if (obj->oartifact && artifact_has_invprop(obj, LEVITATION)) {
                        if (obj->age < moves)
                            obj->age = moves + rnz(100);
                        else
                            obj->age += rnz(100);
                    }
                }
            }
            if (float_down(I_SPECIAL | TIMEOUT, W_ARTI))
                return 1;       /* came down, so moved */
        }
        if (level->locations[u.ux][u.uy].seenv &&
            level->locations[u.ux][u.uy].typ != S_stone) {
            boolean known_stairs = stairs_down &&
                level->locations[u.ux][u.uy].typ == S_dnstair;
            boolean known_ladder = ladder_down &&
                level->locations[u.ux][u.uy].typ == S_dnladder;
            floating_above(known_stairs ? "N{*,N{stair}}" :
                           known_ladder ? "N{ladder}" :
                           surface(u.ux, u.uy));
        } else {
            pline("C{c,N=%s,V{V{float},D{D{a,N{air}},D{high}}}}.", you);
        }
        return 0;       /* didn't move */
    }
    if (!stairs_down && !ladder_down) {
        boolean can_fall;

        trap = t_at(level, u.ux, u.uy);
        can_fall = trap && (trap->ttyp == TRAPDOOR || trap->ttyp == HOLE);
        if (!trap ||
            (trap->ttyp != TRAPDOOR && trap->ttyp != HOLE && trap->ttyp != PIT)
            || (!can_fall_thru(level) && can_fall) || !trap->tseen) {

            if (flags.autodig && !flags.nopick && uwep && is_pick(uwep)) {
                return use_pick_axe2(uwep, 0, 0, 1);
            } else {
                pline("C{N=%s,V{-,V{V{can},V{V{go down},D{here}}}}}.", you);
                return 0;
            }
        }
    }
    if (u.ustuck) {
        if (!u.uswallow)
            pline("C{N=%s,V{V{-,V{V{can},V{go down}}},"
                  "D{Q{because},C{c,s,V{V{hold},N=%s}}}}}.", you, you);
        else
            pline("C{N=%s,V{V{-,V{V{can},V{go down}}},"
                  "D{Q{because},C{s,V{V=%s,N=%s}}}}}.", you,
                  is_animal(u.ustuck->data) ? "V{swallow}" : "V{engulf}", you);
        return 1;
    }
    if (on_level(&valley_level, &u.uz) && !u.uevent.gehennom_entered) {
        char buf[BUFSZ];
        pline("C{c,N=%s,V{V{stand},"
              "D{l,N{N{gate},E{to^connection},P{Gehennom}}}}}.", you);
        pline("C{N{+,N{N{o,cruelty},A{unspeakable}},N{o,harm}},"
              "V{V{lurk},D{D{there},D{down}}}}.");
        sprintf(buf, "C{q,C{N=%s,V{V{are},N{N{o,sure},A{Q{that},"
                "C{N=%s,V{V{want},V{enter}}}}}}}}?", you, you);
        if (yn(buf) != 'y')
            return 0;
        else
            pline("S{So be it.}");
        u.uevent.gehennom_entered = 1;  /* don't ask again */
    }

    if (!next_to_u()) {
        pline("C{N{o,N{pet},N=%s},V{V{hold back},N=%s}}.", you, you);
        return 0;
    }

    if (trap) {
        if (trap->ttyp == PIT) {
            if (u.utrap && (u.utraptype == TT_PIT)) {
                if (flags.autodig && !flags.nopick && uwep && is_pick(uwep)) {
                    return use_pick_axe2(uwep, 0, 0, 1);
                } else {
                    /* YAFM needed */
                    pline("C{N=%s,V{V{V{are},D{already}},D{a,N{pit}}}}.", you);
                }
            } else {
                u.utrap = 1;
                u.utraptype = TT_PIT;
                pline("C{N=%s,V{V=%s,D{D{e,E{into},N{pit}},D{down}}}}.", you,
                      locomotion(youmonst.data, "V{go}"));
            }
            return 0;
        } else {
            pline("C{N=%s,V{V=%s,D=%s}}.", you, 
                  locomotion(youmonst.data, "V{jump}"),
                  trap->ttyp == HOLE ? "D{e,E{down},N{hole}}" :
                  "D{e,E{through},N{trap door}}");
        }
    }

    if (trap && Is_stronghold(&u.uz)) {
        goto_hell(FALSE, TRUE);
    } else {
        at_ladder = (boolean) (level->locations[u.ux][u.uy].typ == LADDER);
        next_level(!trap);
        at_ladder = FALSE;
    }
    return 1;
}

int
doup(void)
{
    if ((u.ux != level->upstair.sx || u.uy != level->upstair.sy)
        && (!level->upladder.sx || u.ux != level->upladder.sx ||
            u.uy != level->upladder.sy)
        && (!level->sstairs.sx || u.ux != level->sstairs.sx ||
            u.uy != level->sstairs.sy || !level->sstairs.up)
        ) {
        pline("C{N=%s,V{-,V{V{can},V{V{go up},D{here}}}}}.", you);
        return 0;
    }
    if (u.usteed && !u.usteed->mcanmove) {
        pline("C{N=%s,V{-,V{V{will},V{move}}}}!", mon_nam(u.usteed));
        return 0;
    } else if (u.usteed && u.usteed->meating) {
        pline("C{c,N=%s,V{V{eat},D{still}}}.", mon_nam(u.usteed));
        return 0;
    } else if (u.ustuck) {
        if (!u.uswallow)
            pline("C{N=%s,V{V{-,V{V{can},V{go up}}},"
                  "D{Q{because},C{c,s,V{V{hold},N=%s}}}}}.", you, you);
        else
            pline("C{N=%s,V{V{-,V{V{can},V{go up}}},"
                  "D{Q{because},C{s,V{V=%s,N=%s}}}}}.", you,
                  is_animal(u.ustuck->data) ? "V{swallow}" : "V{engulf}", you);
        return 1;
    }
    if (near_capacity() > SLT_ENCUMBER) {
        /* No levitation check; inv_weight() already allows for it */
        pline("C{N=%s,V{V{-,V{V{can},V{V{climb},N=%s}}},D{Q{because},"
              "C{N{o,N{load},N=%s},V{V{are},A{A{heavy},D{too}}}}}}}.", you,
              level->locations[u.ux][u.uy].typ == STAIRS ?
              "N{*,N{stair}}" : "N{ladder}", you);
        return 1;
    }
    if (ledger_no(&u.uz) == 1) {
        char buf[BUFSZ];
        sprintf(buf, "C{f,N=%s,V{-,V{V{be able},V{V{return from},"
                "D{E{beyond},N{o,here}}}}}}!  "
                "C{q,C{i,V{V{climb},D{anyway}}}}?", you);
        if (yn(buf) != 'y')
            return 0;
    }
    if (!next_to_u()) {
        pline("C{N{o,N{pet},N=%s},V{V{hold back},N=%s}}!", you, you);
        return 0;
    }
    at_ladder = (boolean) (level->locations[u.ux][u.uy].typ == LADDER);
    prev_level(TRUE);
    at_ladder = FALSE;
    return 1;
}


void
notify_levelchange(void)
{
    int mode;

    if (In_hell(&u.uz))
        mode = LDM_HELL;
    else if (In_quest(&u.uz))
        mode = LDM_QUEST;
    else if (In_mines(&u.uz))
        mode = LDM_MINES;
    else if (In_sokoban(&u.uz))
        mode = LDM_SOKOBAN;
    else if (Is_rogue_level(&u.uz))
        mode = LDM_ROGUE;
    else
        mode = LDM_DEFAULT;

    level_changed(mode);
}


void
goto_level(d_level * newlevel, boolean at_stairs, boolean falling,
           boolean portal)
{
    xchar new_ledger;
    boolean up = (depth(newlevel) < depth(&u.uz));
    boolean newdungeon = (u.uz.dnum != newlevel->dnum);
    boolean was_in_W_tower = In_W_tower(u.ux, u.uy, &u.uz);
    boolean new = FALSE;        /* made a new level? */
    struct monst *mtmp, *mtmp2;
    struct obj *otmp;
    struct level *origlev;
    boolean at_trapdoor = ((t_at(level, u.ux, u.uy)) &&
                           (t_at(level, u.ux, u.uy))->ttyp == TRAPDOOR);

    if (dunlev(newlevel) > dunlevs_in_dungeon(newlevel))
        newlevel->dlevel = dunlevs_in_dungeon(newlevel);
    if (newdungeon && In_endgame(newlevel)) {   /* 1st Endgame Level !!! */
        if (u.uhave.amulet)
            assign_level(newlevel, &earth_level);
        else
            return;
    }
    new_ledger = ledger_no(newlevel);
    if (new_ledger <= 0)
        done(ESCAPED);  /* in fact < 0 is impossible */

    /* Mysterious force removed because I couldn't be bothered to
       grammartreeise it and I'm planning to replace it anyway.
       How's that for a reason to remove it, bhaak? -- AIS */

    /* Prevent the player from going past the first quest level unless (s)he
       has been given the go-ahead by the leader. */
    if (on_level(&u.uz, &qstart_level) && !newdungeon && !ok_to_quest()) {
        pline("C{N=%s,V{-,V{V{can},"
              "V{V{V{start},N{o,N{quest},N=%s}},D{yet}}}}}.", you, you);
        return;
    }

    if (on_level(newlevel, &u.uz))
        return; /* this can happen */

    if (falling)        /* assuming this is only trap door or hole */
        impact_drop(NULL, u.ux, u.uy, newlevel->dlevel);

    check_special_room(TRUE);   /* probably was a trap door */
    if (Punished)
        unplacebc();
    u.utrap = 0;        /* needed in level_tele */
    fill_pit(level, u.ux, u.uy);
    u.ustuck = 0;       /* idem */
    u.uinwater = 0;
    u.uundetected = 0;  /* not hidden, even if means are available */
    keepdogs(FALSE);
    if (u.uswallow)     /* idem */
        u.uswldtim = u.uswallow = 0;
    /* 
     *  We no longer see anything on the level->  Make sure that this
     *  follows u.uswallow set to null since uswallow overrides all
     *  normal vision.
     */
    vision_recalc(2);

    if (iflags.purge_monsters) {
        /* purge any dead monsters */
        dmonsfree(level);
    }
    update_mlstmv();    /* current monsters are becoming inactive */


    assign_level(&u.uz0, &u.uz);
    assign_level(&u.uz, newlevel);
    assign_level(&u.utolev, newlevel);
    u.utotype = 0;

    /* If the entry level is the top level, then the dungeon goes down.
       Otherwise it goes up. */
    if (dungeons[u.uz.dnum].entry_lev == 1) {
        if (dunlev_reached(&u.uz) < dunlev(&u.uz))
            dunlev_reached(&u.uz) = dunlev(&u.uz);
    } else {
        if (dunlev_reached(&u.uz) > dunlev(&u.uz) || !dunlev_reached(&u.uz))
            dunlev_reached(&u.uz) = dunlev(&u.uz);
    }

    reset_rndmonst(NON_PM);     /* u.uz change affects monster generation */

    origlev = level;
    level = NULL;

    if (!levels[new_ledger]) {
        /* entering this level for first time; make it now */
        historic_event(FALSE, "V{V{reach},N=%s}.",
                       hist_lev_name(&u.uz, FALSE));
        level = mklev(&u.uz);
        new = TRUE;     /* made the level */
    } else {
        /* returning to previously visited level */
        level = levels[new_ledger];

        /* regenerate animals while on another level */
        for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon;
            if (moves > level->lastmoves)
                mon_catchup_elapsed_time(mtmp, moves - level->lastmoves);

            /* update shape-changers in case protection against them is
               different now */
            restore_cham(mtmp);
        }
    }

    /* some timers and lights might need to be transferred to the new level if
       they are attached to objects the hero is carrying */
    transfer_timers(origlev, level);
    transfer_lights(origlev, level);

    /* do this prior to level-change pline messages */
    vision_reset();     /* clear old level's line-of-sight */
    vision_full_recalc = 0;     /* don't let that reenable vision yet */
    flush_screen_disable();     /* ensure all map flushes are postponed */

    if (portal && !In_endgame(&u.uz)) {
        /* find the portal on the new level */
        struct trap *ttrap;

        for (ttrap = level->lev_traps; ttrap; ttrap = ttrap->ntrap)
            if (ttrap->ttyp == MAGIC_PORTAL)
                break;

        if (!ttrap)
            panic("S{goto_level: no corresponding portal!}");
        seetrap(ttrap);
        u_on_newpos(ttrap->tx, ttrap->ty);
    } else if (at_stairs && !In_endgame(&u.uz)) {
        if (up) {
            if (at_ladder) {
                u_on_newpos(level->dnladder.sx, level->dnladder.sy);
            } else {
                if (newdungeon) {
                    if (Is_stronghold(&u.uz)) {
                        xchar x, y;

                        do {
                            x = (COLNO - 2 - rnd(5));
                            y = rn1(ROWNO - 4, 3);
                        } while (occupied(level, x, y) ||
                                 IS_WALL(level->locations[x][y].typ));
                        u_on_newpos(x, y);
                    } else
                        u_on_sstairs();
                } else
                    u_on_dnstairs();
            }
        } else {        /* down */
            if (at_ladder) {
                u_on_newpos(level->upladder.sx, level->upladder.sy);
            } else {
                if (newdungeon)
                    u_on_sstairs();
                else
                    u_on_upstairs();
            }
            if (at_stairs && Flying) {} /* no message any more, it's spammy */
            else if (at_stairs &&
                     (near_capacity() > UNENCUMBERED || Punished || Fumbling)) {
                char buf[BUFSZ];
                pline("C{N=%s,V{V{fall down},N=%s}}.", you,
                      at_ladder ? "N{ladder}" : "N{*,N{stair}}");
                if (Punished) {
                    drag_down();
                    if (carried(uball)) {
                        if (uwep == uball)
                            setuwep(NULL);
                        if (uswapwep == uball)
                            setuswapwep(NULL);
                        if (uquiver == uball)
                            setuqwep(NULL);
                        freeinv(uball);
                    }
                }
                /* falling off steed has its own losehp() call */
                if (u.usteed)
                    dismount_steed(DISMOUNT_FELL);
                else
                    losehp(rnd(3), "V{V{fall down},N{*,N{stair}}}", KILLED_BY);
                sprintf(buf, "D{Q{as},C{N=%s,V{fall}}}", you);
                selftouch(buf, "V{+,V{V{fall down},N{*,N{stair}}},"
                          "V{V{touch},N=%s}}");
            }
        }
    } else {    /* trap door or level_tele or In_endgame */
        if (was_in_W_tower && On_W_tower_level(&u.uz))
            /* Stay inside the Wizard's tower when feasible. */
            /* Note: up vs down doesn't really matter in this case. */
            place_lregion(level, level->dndest.nlx, level->dndest.nly,
                          level->dndest.nhx, level->dndest.nhy, 0, 0, 0, 0,
                          LR_DOWNTELE, NULL);
        else if (up)
            place_lregion(level, level->updest.lx, level->updest.ly,
                          level->updest.hx, level->updest.hy, level->updest.nlx,
                          level->updest.nly, level->updest.nhx,
                          level->updest.nhy, LR_UPTELE, NULL);
        else
            place_lregion(level, level->dndest.lx, level->dndest.ly,
                          level->dndest.hx, level->dndest.hy, level->dndest.nlx,
                          level->dndest.nly, level->dndest.nhx,
                          level->dndest.nhy, LR_DOWNTELE, NULL);
        if (falling) {
            char buf[BUFSZ];
            char kbuf[BUFSZ];

            sprintf(buf, "D{Q{as},C{N=%s,V{fall}}}", you);
            sprintf(kbuf, "V{+,V{V{fall through},N=%s},"
                          "V{V{touch},N=%%s}}",
                    at_trapdoor ? "N{i,trap door}" : "N{i,hole}");
            if (Punished)
                ballfall();
            selftouch(buf, kbuf);
        }
    }

    if (Punished)
        placebc();

    /* only matters if falling; place objects that fell with the player nearby */
    while (level->objects[0][0])
        deliver_object(level->objects[0][0], u.uz.dnum, u.uz.dlevel,
                       MIGR_NEAR_PLAYER);

    for (otmp = invent; otmp; otmp = otmp->nobj)
        set_obj_level(level, otmp);
    losedogs();
    kill_genocided_monsters();  /* for those wiped out while in limbo */
    /* 
     * Expire all timers that have gone off while away.  Must be
     * after migrating monsters and objects are delivered
     * (losedogs and obj_delivery).
     */
    run_timers();

    initrack();

    if ((mtmp = m_at(level, u.ux, u.uy)) != 0 && mtmp != u.usteed) {
        /* There's a monster at your target destination; it might be one which
           accompanied you--see mon_arrive(dogmove.c)--or perhaps it was
           already here.  Randomly move you to an adjacent spot or else the
           monster to any nearby location.  Prior to 3.3.0 the latter was done
           unconditionally. */
        coord cc;

        if (!rn2(2) && enexto(&cc, level, u.ux, u.uy, youmonst.data) &&
            distu(cc.x, cc.y) <= 2)
            u_on_newpos(cc.x, cc.y);    /* [maybe give message here?] */
        else
            mnexto(mtmp);

        if ((mtmp = m_at(level, u.ux, u.uy)) != 0) {
            impossible("S{mnexto failed (do.c)?}");
            rloc(mtmp, FALSE);
        }
    }

    /* initial movement of bubbles just before vision_recalc */
    if (Is_waterlevel(&u.uz))
        movebubbles();

    if (level->flags.forgotten) {
        level->flags.forgotten = FALSE;
    }

    notify_levelchange();       /* inform the window code about the level
                                   change */

    /* Reset the screen. */
    vision_reset();     /* reset the blockages */
    doredraw(); /* does a full vision recalc */
    flush_screen_enable();
    flush_screen();

    /* 
     *  Move all plines beyond the screen reset.
     */

    /* give room entrance message, if any */
    check_special_room(FALSE);

    /* Check whether we just entered Gehennom. */
    if (!In_hell(&u.uz0) && Inhell) {
        if (Is_valley(&u.uz)) {
            pline("C{N=%s,V{V{arrive},D{l,N{Valley of the Dead}}}}...", you);
            pline("C{N{odor of burnt flesh and decay},V{V{pervade},N{air}}}.");
            /* cannot be underwater or asleep, so You_hear is unnecessary, and
               can't be used for this anyway */
            pline("C{N=%s,V{V{V{hear},"
                  "N{+,N{*,N{i,groan}},N{*,N{i,moan}}}},D{everywhere}}}.", you);
        } else {
            pline("C{N{o,it^environment},V{V{V{are},A{hot}},D{here}}}.");
            pline("C{N=%s,V{V{smell},N{o,smoke}}}...", you);
        }
    }

    if (new && Is_rogue_level(&u.uz))
        pline("C{N=%s,V{V{enter},N{N{i,world},"
              "A{+,A{c,A{old}},A{c,A{primitive}}}}}}.", you);
    /* Final confrontation */
    if (In_endgame(&u.uz) && newdungeon && u.uhave.amulet)
        resurrect();
    if (newdungeon && In_V_tower(&u.uz) && In_hell(&u.uz0))
        pline("C{N{+,N{heat},N{smoke}},V{V{are},A{gone}}}.");

    /* the message from your quest leader */
    if (!In_quest(&u.uz0) && at_dgn_entrance(&u.uz, "N{Quest}") &&
        !(u.uevent.qexpelled || u.uevent.qcompleted ||
          quest_status.leader_is_dead)) {

        if (u.uevent.qcalled) {
            com_pager(Role_if(PM_ROGUE) ? 4 : 3);
        } else {
            com_pager(2);
            u.uevent.qcalled = TRUE;
        }
    }

    /* once Croesus is dead, his alarm doesn't work any more */
    if (Is_knox(&u.uz) && (new || !mvitals[PM_CROESUS].died)) {
        pline("C{p,N=%s,V{V{penetrate},N{N{area},A{high security}}}}!", you);
        pline("C{N{i,alarm},V{sound}}!");
        for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
            if (!DEADMONSTER(mtmp) && mtmp->msleeping)
                mtmp->msleeping = 0;
    }

    if (on_level(&u.uz, &astral_level))
        final_level();
    else
        onquest();
    assign_level(&u.uz0, &u.uz);        /* reset u.uz0 */

    if (*level->levname)
        pline("C{p,N=%s,V{m,V{V{name},N{level}},S{\x1c%s\x1c}}}.",
              you, level->levname);

    /* assume this will always return TRUE when changing level */
    in_out_region(level, u.ux, u.uy);
    pickup(1);
}


static void
final_level(void)
{
    struct monst *mtmp;
    struct obj *otmp;
    coord mm;
    int i;

    /* reset monster hostility relative to player */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
        if (!DEADMONSTER(mtmp))
            reset_hostility(mtmp);

    /* create some player-monsters */
    create_mplayers(rn1(4, 3), TRUE);

    /* create a guardian angel next to player, if worthy */
    if (Conflict) {
        pline("C{N{i,voice},V{m,V{boom},"
              "C{s,f,V{V{fulfil},N{o,N{desire for conflict},N{thou}}}}!}}@");
        for (i = rnd(4); i > 0; --i) {
            mm.x = u.ux;
            mm.y = u.uy;
            if (enexto(&mm, level, mm.x, mm.y, &mons[PM_ANGEL]))
                mk_roamer(&mons[PM_ANGEL], u.ualign.type, level, mm.x, mm.y,
                          FALSE);
        }

    } else if (u.ualign.record > 8) {   /* fervent */
        pline("C{N{i,voice},V{m,V{whisper},"
              "C{p,N{thou},V{V{are worthy of},N{I}}}.}}@");
        mm.x = u.ux;
        mm.y = u.uy;
        if (enexto(&mm, level, mm.x, mm.y, &mons[PM_ANGEL])) {
            if ((mtmp =
                 mk_roamer(&mons[PM_ANGEL], u.ualign.type, level, mm.x, mm.y,
                           TRUE)) != 0) {
                if (!Blind)
                    pline("C{N{i,angel},V{V{appear near},N=%s}}.", you);
                else
                    pline("C{N=%s,V{V{V{feel the presence of},"
                          "N{N{i,angel},A{friendly}}},D{nearby}}}.", you);
                /* guardian angel -- the one case mtame doesn't imply an edog
                   structure, so we don't want to call tamedog(). */
                mtmp->mtame = 10;
                /* make him strong enough vs. endgame foes */
                mtmp->m_lev = rn1(8, 15);
                mtmp->mhp = mtmp->mhpmax =
                    dice((int)mtmp->m_lev, 10) + 30 + rnd(30);
                if ((otmp = select_hwep(mtmp)) == 0) {
                    otmp = mksobj(level, SILVER_SABER, FALSE, FALSE);
                    if (mpickobj(mtmp, otmp))
                        panic("S{merged weapon?}");
                }
                bless(otmp);
                if (otmp->spe < 4)
                    otmp->spe += rnd(4);
                if ((otmp = which_armor(mtmp, W_ARMS)) == 0 ||
                    otmp->otyp != SHIELD_OF_REFLECTION) {
                    mongets(mtmp, AMULET_OF_REFLECTION);
                    m_dowear(mtmp, TRUE);
                }
            }
        }
    }
}

static char *dfr_pre_msg = 0,   /* pline() before level change */
    *dfr_post_msg = 0;  /* pline() after level change */

/* change levels at the end of this turn, after monsters finish moving */
void
schedule_goto(d_level * tolev, boolean at_stairs, boolean falling,
              int portal_flag, const char *pre_msg, const char *post_msg)
{
    int typmask = 0100; /* non-zero triggers `deferred_goto' */

    /* destination flags (`goto_level' args) */
    if (at_stairs)
        typmask |= 1;
    if (falling)
        typmask |= 2;
    if (portal_flag)
        typmask |= 4;
    if (portal_flag < 0)
        typmask |= 0200;        /* flag for portal removal */
    u.utotype = typmask;
    /* destination level */
    assign_level(&u.utolev, tolev);

    if (pre_msg)
        dfr_pre_msg = strcpy(malloc(strlen(pre_msg) + 1), pre_msg);
    if (post_msg)
        dfr_post_msg = strcpy(malloc(strlen(post_msg) + 1), post_msg);
}

/* handle something like portal ejection */
void
deferred_goto(void)
{
    if (!on_level(&u.uz, &u.utolev)) {
        d_level dest;
        int typmask = u.utotype;        /* save it; goto_level zeroes u.utotype 
                                         */

        assign_level(&dest, &u.utolev);
        if (dfr_pre_msg)
            pline("C=%s", dfr_pre_msg);
        goto_level(&dest, ! !(typmask & 1), ! !(typmask & 2), ! !(typmask & 4));
        if (typmask & 0200) {   /* remove portal */
            struct trap *t = t_at(level, u.ux, u.uy);

            if (t) {
                deltrap(level, t);
                newsym(u.ux, u.uy);
            }
        }
        if (dfr_post_msg)
            pline("C=%s", dfr_post_msg);
    }
    u.utotype = 0;      /* our caller keys off of this */
    if (dfr_pre_msg)
        free(dfr_pre_msg), dfr_pre_msg = 0;
    if (dfr_post_msg)
        free(dfr_post_msg), dfr_post_msg = 0;
}


/*
 * Return TRUE if we created a monster for the corpse.  If successful, the
 * corpse is gone.
 */
boolean
revive_corpse(struct obj *corpse)
{
    struct monst *mtmp, *mcarry;
    boolean is_uwep, chewed;
    xchar where;
    char *cname, cname_buf[BUFSZ];
    struct obj *container = NULL;
    int container_where = 0;

#ifdef INVISIBLE_OBJECTS
    boolean vis = !corpse->oinvis || See_invisible;
#endif

    where = corpse->where;
    is_uwep = corpse == uwep;
    cname = corpse_xname(corpse, TRUE);
    sprintf(cname_buf, "N{N=%s,A{bite-covered}}", cname);
    mcarry = (where == OBJ_MINVENT) ? corpse->ocarry : 0;

    if (where == OBJ_CONTAINED) {
        struct monst *mtmp2 = NULL;

        container = corpse->ocontainer;
        mtmp2 = get_container_location(container, &container_where, NULL);
        /* container_where is the outermost container's location even if nested 
         */
        if (container_where == OBJ_MINVENT && mtmp2)
            mcarry = mtmp2;
    }
    mtmp = revive(corpse);      /* corpse is gone if successful */

    if (mtmp) {
        chewed = (mtmp->mhp < mtmp->mhpmax);
        if (chewed)
            cname = cname_buf;  /* include 'bite-covered' prefix */
        switch (where) {
        case OBJ_INVENT:
            if (is_uwep)
                pline("C{N=%s,V{V{writhe out of},N{o,N{grasp},N=%s}}}!",
                      cname, you);
            else
                pline("C{N=%s,V{V{V{feel},N{o,squirming}},"
                      "D{a,N{o,N{backpack},N=%s}}}}!", you, you);
            break;

        case OBJ_FLOOR:
            if (cansee(mtmp->mx, mtmp->my)
#ifdef INVISIBLE_OBJECTS
                && vis
#endif
                )
                pline(chewed ? "C{N=%s,V{rise from the dead}}!" :
                      "C{N{N=%s,A{bite-covered}},V{rise from the dead}}!",
                      mon_nam(mtmp));
            break;

        case OBJ_MINVENT:      /* probably a nymph's */
            if (cansee(mtmp->mx, mtmp->my)) {
                /* Don't use an() here, rather infer it from context,
                   because the sentence might be reordered; if it's
                   repeated rather than pronouned, we need 'the' for
                   the second occurrence */
                if (canseemon(mcarry))
                    pline("C{N{N=%s,A{V{startle}}},V{V{V{drop},N=%s},"
                          "D{Q{as},C{N=%s,V{revive}}}}}!",
                          mon_nam(mcarry), cname, cname);
                else
                    pline(chewed ? "C{N=%s,V{V{appear},D{suddenly}}}!" :
                          "C{N{N=%s,A{bite-covered}},V{V{appear},D{suddenly}}}!",
                          mon_nam(mtmp));
            }
            break;
        case OBJ_CONTAINED:
            if (container_where == OBJ_MINVENT && cansee(mtmp->mx, mtmp->my) &&
                mcarry && canseemon(mcarry) && container) {
                char sackname[BUFSZ];

                sprintf(sackname, "N{o,N=%s,N=%s}",
                        xname(container), mon_nam(mcarry));
                pline("C{N=%s,V{V{writhe out of},N=%s}}!",
                      a_monnam(mtmp), sackname);

            } else if (container_where == OBJ_INVENT && container) {
                char sackname[BUFSZ];

                strcpy(sackname, an(xname(container)));
                /* 3.4.3 is inconsistent on whether to use locomotion()
                   here. It does make sense in most cases but levitate(),
                   but it requires synthesizing compound verbs from parts,
                   something grammartree doesn't yet try to attempt. */
                pline("C{N=%s,V{V{writhe out of},"
                      "N{s,C{c,N=%s,V{V{carry},N=%s}}}}}!",
                      Blind ? "?{}" : a_monnam(mtmp), you, sackname);
            } else if (container_where == OBJ_FLOOR && container &&
                       cansee(mtmp->mx, mtmp->my)) {
                char sackname[BUFSZ];

                strcpy(sackname, an(xname(container)));
                pline("C{N=%s,V{V{escape from},N=%s}}!", a_monnam(mtmp), sackname);
            }
            break;
        default:
            /* we should be able to handle the other cases... */
            impossible("S{revive_corpse: lost corpse @ %d}", where);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* Revive the corpse via a timeout. */
 /*ARGSUSED*/ void
revive_mon(void *arg, long timeout)
{
    struct obj *body = (struct obj *)arg;

    /* if we succeed, the corpse is gone, otherwise, rot it away */
    if (!revive_corpse(body)) {
        /* TODO: Do we really want to use you? Or a literal 'you'? Or
           something else entirely? I guess if/when multiplayer is
           implemeneted, we can find out then. For now, 'you' in case
           someone feels like being referred to entirely in the third
           person. (Or the first person.) */
        if (is_rider(&mons[body->corpsenm]))
            pline("C{N=%s,V{V{feel},A{A{V{hassle}},D{less}}}}.", you);
        start_timer(body->olev, 250L - (moves - body->age), TIMER_OBJECT,
                    ROT_CORPSE, arg);
    }
}

int
donull(void)
{
    return 1;   /* Do nothing, but let other things happen */
}


static int
wipeoff(void)
{
    if (u.ucreamed < 4)
        u.ucreamed = 0;
    else
        u.ucreamed -= 4;
    if (Blinded < 4)
        Blinded = 0;
    else
        Blinded -= 4;
    if (!Blinded) {
        pline("C{p,N=%s,V{V{remove},N{glop}}}.", you);
        u.ucreamed = 0;
        Blinded = 1;
        make_blinded(0L, TRUE);
        return 0;
    } else if (!u.ucreamed) {
        pline("C{N{o,N=%s,N=%s},V{V{feel},A{clean}}}.", body_part(FACE), you);
        return 0;
    }
    return 1;   /* still busy */
}

int
dowipe(void)
{
    if (u.ucreamed) {
        static char buf[39];

        sprintf(buf, "V{V{wipe off},N{o,N=%s,N=%s}}", body_part(FACE), you);
        set_occupation(wipeoff, buf, 0);
        /* Not totally correct; what if they change back after now but before
           they're finished wiping? Luckily, set_occupation isn't used for
           death messages, or we'd need to distinguish between you and literal
           'you'. */
        return 1;
    }
    pline("C{N{o,N=%s,N=%s},V{V{V{are},A{clean}},D{already}}}.",
          body_part(FACE), you);
    return 0;
}

void
set_wounded_legs(long side, int timex)
{
    /* KMH If you are riding, your steed gets the wounded legs instead. You
       still call this function, but don't lose hp. Caller is also responsible
       for adjusting messages. */

    if (!Wounded_legs) {
        ATEMP(A_DEX)--;
        iflags.botl = 1;
    }

    if (!Wounded_legs || (HWounded_legs & TIMEOUT))
        HWounded_legs = timex;
    EWounded_legs = side;
    encumber_msg();
}

void
heal_legs(void)
{
    if (Wounded_legs) {
        if (ATEMP(A_DEX) < 0) {
            ATEMP(A_DEX)++;
            iflags.botl = 1;
        }

        if (!u.usteed) {
            /* KMH, intrinsics patch */
            if ((EWounded_legs & BOTH_SIDES) == BOTH_SIDES) {
                pline("C{N{o,N{*,N=%s},N=%s},V{V{feel},A{A{c,A{good}},D{somewhat}}}}.",
                      body_part(LEG), you);
            } else {
                pline("C{N{o,N=%s,N=%s},V{V{feel},A{A{c,A{good}},D{somewhat}}}}.",
                      body_part(LEG), you);
            }
        }
        HWounded_legs = EWounded_legs = 0;
    }
    encumber_msg();
}

/*do.c*/
