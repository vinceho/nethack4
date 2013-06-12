/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Disintegration rays have special treatment; corpses are never left.
 * But the routine which calculates the damage is separate from the routine
 * which kills the monster.  The damage routine returns this cookie to
 * indicate that the monster should be disintegrated.
 */
#define MAGIC_COOKIE 1000

static boolean obj_zapped;
static int poly_zapped;

extern boolean notonhead;       /* for long worms */

/* kludge to use mondied instead of killed */
extern boolean m_using;

static void costly_cancel(struct obj *);
static void polyuse(struct obj *, int, int);
static void create_polymon(struct obj *, int);
static boolean zap_updown(struct obj *, schar);
static int zap_hit_mon(struct monst *, int, int, struct obj **);
static void zap_hit_u(int, int, const char *, xchar, xchar);
static void revive_egg(struct obj *);
static boolean zap_steed(struct obj *);
static void cancel_item(struct obj *);
static boolean obj_shudders(struct obj *);
static void do_osshock(struct obj *);

static int zap_hit_check(int, int);
static void backfire(struct obj *);
static int spell_hit_bonus(int);

#define ZT_MAGIC_MISSILE        (AD_MAGM-1)
#define ZT_FIRE                 (AD_FIRE-1)
#define ZT_COLD                 (AD_COLD-1)
#define ZT_SLEEP                (AD_SLEE-1)
#define ZT_DEATH                (AD_DISN-1)     /* or disintegration */
#define ZT_LIGHTNING            (AD_ELEC-1)
#define ZT_POISON_GAS           (AD_DRST-1)
#define ZT_ACID                 (AD_ACID-1)
/* 8 and 9 are currently unassigned */

#define ZT_WAND(x)              (x)
#define ZT_SPELL(x)             (10+(x))
#define ZT_BREATH(x)            (20+(x))

#define is_hero_spell(type)     ((type) >= 10 && (type) < 20)

/* TODO: Get "by the flash" not "with the flash" */
static const char blinded_by_the_flash[] =
    "C{s,V{V{V{blind},N=%s},D{t,N{flash}}}}!";

const char *const flash_types[] = {     /* also used in buzzmu(mcastu.c) */
    "N{i,magic missile}",       /* Wands must be 0-9 */
    "N{l,N{i,bolt},N{o,fire}}",
    "N{l,N{i,bolt},N{o,cold}}",
    "N{i,sleep ray}",
    "N{i,death ray}",
    "N{l,N{i,bolt},N{o,lightning}}",
    "",
    "",
    "",
    "",

    "N{i,magic missile}",       /* Spell equivalents must be 10-19 */
    "N{i,fireball}",
    "N{l,N{i,cone},N{o,cold}}",
    "N{i,sleep ray}",
    "N{i,finger of death}",
    "N{l,N{i,bolt},N{o,lightning}}",    /* There is no spell, used for
                                           retribution */
    "",
    "",
    "",
    "",

    "N{l,N{i,blast},N{o,missiles}}",    /* Dragon breath equivalents 20-29 */
    "N{l,N{i,blast},N{o,fire}}",
    "N{l,N{i,blast},N{o,frost}}",
    "N{l,N{i,blast},N{o,sleep gas}}",
    "N{l,N{i,blast},N{o,disintegration}}",
    "N{l,N{i,blast},N{o,lightning}}",
    "N{l,N{i,blast},N{o,poison gas}}",
    "N{l,N{i,blast},N{o,acid}}",
    "",
    ""
};

/* Routines for IMMEDIATE wands and spells. */
/* bhitm: monster mtmp was hit by the effect of wand or spell otmp */
int
bhitm(struct monst *mtmp, struct obj *otmp)
{
    boolean wake = TRUE;        /* Most 'zaps' should wake monster */
    boolean reveal_invis = FALSE;
    boolean dbldam = Role_if(PM_KNIGHT) && u.uhave.questart;
    int dmg, otyp = otmp->otyp;
    const char *zap_type_text = "N{spell}";
    struct obj *obj;
    boolean disguised_mimic = (mtmp->data->mlet == S_MIMIC &&
                               mtmp->m_ap_type != M_AP_NOTHING);

    if (u.uswallow && mtmp == u.ustuck)
        reveal_invis = FALSE;

    switch (otyp) {
    case WAN_STRIKING:
        zap_type_text = "N{wand}";
        /* fall through */
    case SPE_FORCE_BOLT:
        reveal_invis = TRUE;
        if (resists_magm(mtmp)) {       /* match effect on player */
            shieldeff(mtmp->mx, mtmp->my);
            break;      /* skip makeknown */
        } else if (u.uswallow || rnd(20) < 10 + find_mac(mtmp)) {
            dmg = dice(2, 12);
            if (dbldam)
                dmg *= 2;
            if (otyp == SPE_FORCE_BOLT)
                dmg += spell_damage_bonus();
            hit(zap_type_text, mtmp, exclam(dmg));
            resist(mtmp, otmp->oclass, dmg, TELL);
        } else
            miss(zap_type_text, mtmp);
        makeknown(otyp);
        break;
    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
        if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
            mon_adjust_speed(mtmp, -1, otmp);
            m_dowear(mtmp, FALSE);      /* might want speed boots */
            if (u.uswallow && (mtmp == u.ustuck) && is_whirly(mtmp->data)) {
                pline("C{N=%s,V{V{disrupt},N=%s}}!", you, mon_nam(mtmp));
                pline("C{N{N{i,hole},A{huge}},V{open up}}...");
                expels(mtmp, mtmp->data, TRUE);
            }
        }
        break;
    case WAN_SPEED_MONSTER:
        if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
            mon_adjust_speed(mtmp, 1, otmp);
            m_dowear(mtmp, FALSE);      /* might want speed boots */
        }
        break;
    case WAN_UNDEAD_TURNING:
    case SPE_TURN_UNDEAD:
        wake = FALSE;
        if (unturn_dead(mtmp))
            wake = TRUE;
        if (is_undead(mtmp->data)) {
            reveal_invis = TRUE;
            wake = TRUE;
            dmg = rnd(8);
            if (dbldam)
                dmg *= 2;
            if (otyp == SPE_TURN_UNDEAD)
                dmg += spell_damage_bonus();
            flags.bypasses = TRUE;      /* for make_corpse() */
            if (!resist(mtmp, otmp->oclass, dmg, NOTELL)) {
                if (mtmp->mhp > 0)
                    monflee(mtmp, 0, FALSE, TRUE);
            }
        }
        break;
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case POT_POLYMORPH:
        if (resists_magm(mtmp)) {
            /* magic resistance protects from polymorph traps, so make it guard 
               against involuntary polymorph attacks too... */
            shieldeff(mtmp->mx, mtmp->my);
        } else if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
            /* natural shapechangers aren't affected by system shock (unless
               protection from shapechangers is interfering with their
               metabolism...) */
            if (mtmp->cham == CHAM_ORDINARY && !rn2(25)) {
                if (canseemon(mtmp)) {
                    pline("C{N=%s,V{shudder}}!", mon_nam(mtmp));
                    makeknown(otyp);
                }
                /* dropped inventory shouldn't be hit by this zap */
                for (obj = mtmp->minvent; obj; obj = obj->nobj)
                    bypass_obj(obj);
                /* flags.bypasses = TRUE; ## for make_corpse() */
                /* no corpse after system shock */
                xkilled(mtmp, 3);
            } else if (newcham(mtmp, NULL, (otyp != POT_POLYMORPH), FALSE)) {
                if (!Hallucination && canspotmon(mtmp))
                    makeknown(otyp);
            }
        }
        break;
    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
        cancel_monst(mtmp, otmp, TRUE, TRUE, FALSE);
        break;
    case WAN_TELEPORTATION:
    case SPE_TELEPORT_AWAY:
        reveal_invis = !u_teleport_mon(mtmp, TRUE);
        break;
    case WAN_MAKE_INVISIBLE:
        {
            int oldinvis = mtmp->minvis;
            char nambuf[BUFSZ];

            /* format monster's name before altering its visibility */
            strcpy(nambuf, mon_nam(mtmp));
            mon_set_minvis(mtmp);
            if (!oldinvis && knowninvisible(mtmp)) {
                pline("C{N=%s,V{V{turn^become},A{transparent}}}!", nambuf);
                makeknown(otyp);
            }
            break;
        }
    case WAN_NOTHING:
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        wake = FALSE;
        break;
    case WAN_PROBING:
        wake = FALSE;
        reveal_invis = TRUE;
        probe_monster(mtmp);
        makeknown(otyp);
        break;
    case WAN_OPENING:
    case SPE_KNOCK:
        wake = FALSE;   /* don't want immediate counterattack */
        if (u.uswallow && mtmp == u.ustuck) {
            if (is_animal(mtmp->data)) {
                if (Blind)
                    pline("C{N=%s,V{V{feel},"
                          "N{N{l,N{i,rush},N{o,air}},A{sudden}}}}!", you);
                else
                    pline("C{N=%s,V{V{open},N{o,N{mouth},N=%s}}}!",
                          mon_nam(mtmp), mon_nam(mtmp));
            }
            expels(mtmp, mtmp->data, TRUE);
        } else if (! !(obj = which_armor(mtmp, W_SADDLE))) {
            mtmp->misc_worn_check &= ~obj->owornmask;
            update_mon_intrinsics(mtmp, obj, FALSE, FALSE);
            obj->owornmask = 0L;
            obj_extract_self(obj);
            place_object(obj, level, mtmp->mx, mtmp->my);
            /* call stackobj() if we ever drop anything that can merge */
            newsym(mtmp->mx, mtmp->my);
        }
        break;
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
        reveal_invis = TRUE;
        if (mtmp->data != &mons[PM_PESTILENCE]) {
            wake = FALSE;       /* wakeup() makes the target angry */
            mtmp->mhp += dice(6, otyp == SPE_EXTRA_HEALING ? 8 : 4);
            if (mtmp->mhp > mtmp->mhpmax)
                mtmp->mhp = mtmp->mhpmax;
            if (mtmp->mblinded) {
                mtmp->mblinded = 0;
                mtmp->mcansee = 1;
            }
            if (canseemon(mtmp)) {
                if (disguised_mimic) {
                    if (mtmp->m_ap_type == M_AP_OBJECT &&
                        mtmp->mappearance == STRANGE_OBJECT) {
                        /* it can do better now */
                        set_mimic_sym(mtmp, level);
                        newsym(mtmp->mx, mtmp->my);
                    } else
                        mimic_hit_msg(mtmp, otyp);
                } else
                    pline("C{N=%s,V{V{look^appear},A=%s}}.", mon_nam(mtmp),
                          otyp ==
                          SPE_EXTRA_HEALING ? "A{A{better},D{much}}" :
                          "A{better}");
            }
            if (mtmp->mtame || mtmp->mpeaceful) {
                adjalign(Role_if(PM_HEALER) ? 1 : sgn(u.ualign.type));
            }
        } else {        /* Pestilence */
            /* Pestilence will always resist; damage is half of 3d{4,8} */
            resist(mtmp, otmp->oclass,
                   dice(3, otyp == SPE_EXTRA_HEALING ? 8 : 4), TELL);
        }
        break;
    case WAN_LIGHT:    /* (broken wand) */
        if (flash_hits_mon(mtmp, otmp)) {
            makeknown(WAN_LIGHT);
            reveal_invis = TRUE;
        }
        break;
    case WAN_SLEEP:    /* (broken wand) */
        /* [wakeup() doesn't rouse victims of temporary sleep, so it's okay to
           leave `wake' set to TRUE here] */
        reveal_invis = TRUE;
        if (sleep_monst(mtmp, dice(1 + otmp->spe, 12), WAND_CLASS))
            slept_monst(mtmp);
        if (!Blind)
            makeknown(WAN_SLEEP);
        break;
    case SPE_STONE_TO_FLESH:
        if (monsndx(mtmp->data) == PM_STONE_GOLEM) {
            char *name = mon_nam(mtmp);

            /* turn into flesh golem */
            if (newcham(mtmp, &mons[PM_FLESH_GOLEM], FALSE, FALSE)) {
                if (canseemon(mtmp))
                    pline("C{N=%s,V{V{turn to^become},N{flesh}}}!", name);
            } else {
                if (canseemon(mtmp))
                    pline("C{N=%s,V{V{V{look^appear},A{A{fleshy},D{rather}}},"
                          "D{d,N{i,moment}}}}.", name);
            }
        } else
            wake = FALSE;
        break;
    case SPE_DRAIN_LIFE:
        dmg = rnd(8);
        if (dbldam)
            dmg *= 2;
        if (otyp == SPE_DRAIN_LIFE)
            dmg += spell_damage_bonus();
        if (resists_drli(mtmp))
            shieldeff(mtmp->mx, mtmp->my);
        else if (!resist(mtmp, otmp->oclass, dmg, NOTELL) && mtmp->mhp > 0) {
            mtmp->mhp -= dmg;
            mtmp->mhpmax -= dmg;
            if (mtmp->mhp <= 0 || mtmp->mhpmax <= 0 || mtmp->m_lev < 1)
                xkilled(mtmp, 1);
            else {
                mtmp->m_lev--;
                if (canseemon(mtmp))
                    pline("C{N=%s,V{V{V{seem},D{suddenly}},A{c,A{weak}}}}!",
                          mon_nam(mtmp));
            }
        }
        break;
    default:
        impossible("S{What an interesting effect (%d)}", otyp);
        break;
    }
    if (wake) {
        if (mtmp->mhp > 0) {
            wakeup(mtmp);
            m_respond(mtmp);
            if (mtmp->isshk && !*u.ushops)
                hot_pursuit(mtmp);
        } else if (mtmp->m_ap_type)
            seemimic(mtmp);     /* might unblock if mimicing a boulder/door */
    }
    /* note: bhitpos won't be set if swallowed, but that's okay since
       reveal_invis will be false.  We can't use mtmp->mx, my since it might be 
       an invisible worm hit on the tail. */
    if (reveal_invis) {
        if (mtmp->mhp > 0 && cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp))
            map_invisible(bhitpos.x, bhitpos.y);
    }
    return 0;
}


void
probe_monster(struct monst *mtmp)
{
    struct obj *otmp;

    mstatusline(mtmp);
    if (notonhead)
        return; /* don't show minvent for long worm tail */

    if (mtmp->minvent) {
        for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
            otmp->dknown = 1;   /* treat as "seen" */
        display_minventory(mtmp, MINV_ALL, NULL);
    } else {
        pline("C{c,N=%s,V{-,V{V{carry},N{o,anything}}}}.", noit_mon_nam(mtmp));
    }
}


/*
 * Return the object's physical location.  This only makes sense for
 * objects that are currently on the level (i.e. migrating objects
 * are nowhere).  By default, only things that can be seen (in hero's
 * inventory, monster's inventory, or on the ground) are reported.
 * By adding BURIED_TOO and/or CONTAINED_TOO flags, you can also get
 * the location of buried and contained objects.  Note that if an
 * object is carried by a monster, its reported position may change
 * from turn to turn.  This function returns FALSE if the position
 * is not available or subject to the constraints above.
 */
boolean
get_obj_location(const struct obj *obj, xchar * xp, xchar * yp, int locflags)
{
    switch (obj->where) {
    case OBJ_INVENT:
        *xp = u.ux;
        *yp = u.uy;
        return TRUE;
    case OBJ_FLOOR:
        *xp = obj->ox;
        *yp = obj->oy;
        return TRUE;
    case OBJ_MINVENT:
        if (obj->ocarry->mx) {
            *xp = obj->ocarry->mx;
            *yp = obj->ocarry->my;
            return TRUE;
        }
        break;  /* !mx => migrating monster */
    case OBJ_BURIED:
        if (locflags & BURIED_TOO) {
            *xp = obj->ox;
            *yp = obj->oy;
            return TRUE;
        }
        break;
    case OBJ_CONTAINED:
        if (locflags & CONTAINED_TOO)
            return get_obj_location(obj->ocontainer, xp, yp, locflags);
        break;
    }
    *xp = *yp = 0;
    return FALSE;
}

/* locflags non-zero means get location even if monster is buried */
boolean
get_mon_location(struct monst * mon, xchar * xp, xchar * yp, int locflags)
{
    if (mon == &youmonst) {
        *xp = u.ux;
        *yp = u.uy;
        return TRUE;
    } else if (mon->mx > 0 && (!mon->mburied || locflags)) {
        *xp = mon->mx;
        *yp = mon->my;
        return TRUE;
    } else {    /* migrating or buried */
        *xp = *yp = 0;
        return FALSE;
    }
}

static struct level *
object_dlevel(struct obj *obj)
{
    switch (obj->where) {
    case OBJ_FLOOR:
    case OBJ_BURIED:
        return obj->olev;
    case OBJ_CONTAINED:
        return object_dlevel(obj->ocontainer);
    case OBJ_INVENT:
        return level;
    case OBJ_MINVENT:
        return obj->ocarry->dlevel;
    case OBJ_ONBILL:
        panic("S{Object on bill in object_dlevel}");
    case OBJ_FREE:
    default:
        panic("S{Object is nowhere in object_dlevel}");
    }
    /* This should not occur, but silence warnings */
    return 0;
}

/* used by revive() and animate_statue() */
struct monst *
montraits(struct obj *obj, coord * cc)
{
    struct monst *mtmp = NULL;
    struct monst *mtmp2 = NULL;

    if (obj->oxlth && (obj->oattached == OATTACHED_MONST))
        mtmp2 = get_mtraits(obj, TRUE);
    if (mtmp2) {
        /* save_mtraits() validated mtmp2->mnum */
        mtmp2->data = &mons[mtmp2->mnum];
        if (mtmp2->mhpmax <= 0 && !is_rider(mtmp2->data))
            return NULL;
        mtmp =
            makemon(mtmp2->data, level, cc->x, cc->y,
                    NO_MINVENT | MM_NOWAIT | MM_NOCOUNTBIRTH);
        if (!mtmp)
            return mtmp;

        /* heal the monster */
        if (mtmp->mhpmax > mtmp2->mhpmax && is_rider(mtmp2->data))
            mtmp2->mhpmax = mtmp->mhpmax;
        mtmp2->mhp = mtmp2->mhpmax;
        /* Get these ones from mtmp */
        mtmp2->minvent = mtmp->minvent; /* redundant */
        /* monster ID is available if the monster died in the current game, but 
           should be zero if the corpse was in a bones level (we cleared it
           when loading bones) */
        if (!mtmp2->m_id)
            mtmp2->m_id = mtmp->m_id;
        mtmp2->mx = mtmp->mx;
        mtmp2->my = mtmp->my;
        mtmp2->mux = mtmp->mux;
        mtmp2->muy = mtmp->muy;
        mtmp2->mw = mtmp->mw;
        mtmp2->wormno = mtmp->wormno;
        mtmp2->misc_worn_check = mtmp->misc_worn_check;
        mtmp2->weapon_check = mtmp->weapon_check;
        mtmp2->mtrapseen = mtmp->mtrapseen;
        mtmp2->mflee = mtmp->mflee;
        mtmp2->mburied = mtmp->mburied;
        mtmp2->mundetected = mtmp->mundetected;
        mtmp2->mfleetim = mtmp->mfleetim;
        mtmp2->mlstmv = mtmp->mlstmv;
        mtmp2->m_ap_type = mtmp->m_ap_type;
        /* set these ones explicitly */
        mtmp2->mavenge = 0;
        mtmp2->meating = 0;
        mtmp2->mleashed = 0;
        mtmp2->mtrapped = 0;
        mtmp2->msleeping = 0;
        mtmp2->mfrozen = 0;
        mtmp2->mcanmove = 1;
        /* most cancelled monsters return to normal, but some need to stay
           cancelled */
        if (!dmgtype(mtmp2->data, AD_SEDU) && !dmgtype(mtmp2->data, AD_SSEX))
            mtmp2->mcan = 0;
        mtmp2->mcansee = 1;     /* set like in makemon */
        mtmp2->mblinded = 0;
        mtmp2->mstun = 0;
        mtmp2->mconf = 0;
        /* the corpse may have been moved, set the monster's location from the
           corpse's location */
        mtmp2->dlevel = object_dlevel(obj);
        replmon(mtmp, mtmp2);
    }
    return mtmp2;
}

/*
 * get_container_location() returns the following information
 * about the outermost container:
 * loc argument gets set to: 
 *   OBJ_INVENT   if in hero's inventory; return 0.
 *   OBJ_FLOOR    if on the floor; return 0.
 *   OBJ_BURIED   if buried; return 0.
 *   OBJ_MINVENT  if in monster's inventory; return monster.
 * container_nesting is updated with the nesting depth of the containers
 * if applicable.
 */
struct monst *
get_container_location(struct obj *obj, int *loc, int *container_nesting)
{
    if (!obj || !loc)
        return 0;

    if (container_nesting)
        *container_nesting = 0;
    while (obj && obj->where == OBJ_CONTAINED) {
        if (container_nesting)
            *container_nesting += 1;
        obj = obj->ocontainer;
    }
    if (obj) {
        *loc = obj->where;      /* outermost container's location */
        if (obj->where == OBJ_MINVENT)
            return obj->ocarry;
    }
    return NULL;
}

/*
 * Attempt to revive the given corpse, return the revived monster if
 * successful.  Note: this does NOT use up the corpse if it fails.
 */
struct monst *
revive(struct obj *obj)
{
    struct monst *mtmp = NULL;
    struct obj *container = NULL;
    int container_nesting = 0;
    schar savetame = 0;
    boolean recorporealization = FALSE;
    boolean in_container = FALSE;

    if (obj->otyp == CORPSE) {
        int montype = obj->corpsenm;
        xchar x, y;

        if (obj->where == OBJ_CONTAINED) {
            /* deal with corpses in [possibly nested] containers */
            struct monst *carrier;
            int holder = 0;

            container = obj->ocontainer;
            carrier =
                get_container_location(container, &holder, &container_nesting);
            switch (holder) {
            case OBJ_MINVENT:
                x = carrier->mx;
                y = carrier->my;
                in_container = TRUE;
                break;
            case OBJ_INVENT:
                x = u.ux;
                y = u.uy;
                in_container = TRUE;
                break;
            case OBJ_FLOOR:
                if (!get_obj_location(obj, &x, &y, CONTAINED_TOO))
                    return NULL;
                in_container = TRUE;
                break;
            default:
                return NULL;
            }
        } else {
            /* only for invent, minvent, or floor */
            if (!get_obj_location(obj, &x, &y, 0))
                return NULL;
        }
        if (in_container) {
            /* Rules for revival from containers: - the container cannot be
               locked - the container cannot be heavily nested (>2 is
               arbitrary) - the container cannot be a statue or bag of holding
               (except in very rare cases for the latter) */
            if (!x || !y || container->olocked || container_nesting > 2 ||
                container->otyp == STATUE || (container->otyp == BAG_OF_HOLDING
                                              && rn2(40)))
                return NULL;
        }

        if (MON_AT(level, x, y)) {
            coord new_xy;

            if (enexto(&new_xy, level, x, y, &mons[montype]))
                x = new_xy.x, y = new_xy.y;
        }

        if (cant_create(&montype, TRUE)) {
            /* make a zombie or worm instead */
            mtmp = makemon(&mons[montype], level, x, y, NO_MINVENT | MM_NOWAIT);
            if (mtmp) {
                mtmp->mhp = mtmp->mhpmax = 100;
                mon_adjust_speed(mtmp, 2, NULL);        /* MFAST */
            }
        } else {
            if (obj->oxlth && (obj->oattached == OATTACHED_MONST)) {
                coord xy;

                xy.x = x;
                xy.y = y;
                mtmp = montraits(obj, &xy);
                if (mtmp && mtmp->mtame && !mtmp->isminion)
                    wary_dog(mtmp, TRUE);
            } else
                mtmp =
                    makemon(&mons[montype], level, x, y,
                            NO_MINVENT | MM_NOWAIT | MM_NOCOUNTBIRTH);
            if (mtmp) {
                if (obj->oxlth && (obj->oattached == OATTACHED_M_ID)) {
                    unsigned m_id;
                    struct monst *ghost;

                    memcpy(&m_id, obj->oextra, sizeof (m_id));
                    ghost = find_mid(level, m_id, FM_FMON);
                    if (ghost && ghost->data == &mons[PM_GHOST]) {
                        int x2, y2;

                        x2 = ghost->mx;
                        y2 = ghost->my;
                        if (ghost->mtame)
                            savetame = ghost->mtame;
                        if (canseemon(ghost))
                            pline("C{s,V{V{V{V{draw^pull},D{suddenly}},N=%s},"
                                  "D{e,E{into},"
                                  "N{o,N{N{body},A{former}},N=%s}}}}!",
                                  mon_nam(ghost), mon_nam(ghost));
                        mondead(ghost);
                        recorporealization = TRUE;
                        newsym(x2, y2);
                    }
                    /* don't mess with obj->oxlth here */
                    obj->oattached = OATTACHED_NOTHING;
                }
                /* Monster retains its name */
                if (obj->onamelth)
                    mtmp = christen_monst(mtmp, ONAME(obj));
                /* flag the quest leader as alive. */
                if (mtmp->data->msound == MS_LEADER ||
                    mtmp->m_id == quest_status.leader_m_id) {
                    quest_status.leader_m_id = mtmp->m_id;
                    quest_status.leader_is_dead = FALSE;
                }
            }
        }
        if (mtmp) {
            if (obj->oeaten)
                mtmp->mhp = eaten_stat(mtmp->mhp, obj);
            /* track that this monster was revived at least once */
            mtmp->mrevived = 1;

            if (recorporealization) {
                /* If mtmp is revivification of former tame ghost */
                if (savetame) {
                    struct monst *mtmp2 = tamedog(mtmp, NULL);

                    if (mtmp2) {
                        mtmp2->mtame = savetame;
                        mtmp = mtmp2;
                    }
                }
                /* was ghost, now alive, it's all very confusing */
                mtmp->mconf = 1;
            }

            switch (obj->where) {
            case OBJ_INVENT:
                useup(obj);
                break;
            case OBJ_FLOOR:
                /* in case MON_AT+enexto for invisible mon */
                x = obj->ox, y = obj->oy;
                /* not useupf(), which charges */
                if (obj->quan > 1L)
                    obj = splitobj(obj, 1L);
                delobj(obj);
                newsym(x, y);
                break;
            case OBJ_MINVENT:
                m_useup(obj->ocarry, obj);
                break;
            case OBJ_CONTAINED:
                obj_extract_self(obj);
                obfree(obj, NULL);
                break;
            default:
                panic("S{revive}");
            }
        }
    }
    return mtmp;
}

static void
revive_egg(struct obj *obj)
{
    /* 
     * Note: generic eggs with corpsenm set to NON_PM will never hatch.
     */
    if (obj->otyp != EGG)
        return;
    if (obj->corpsenm != NON_PM && !dead_species(obj->corpsenm, TRUE))
        attach_egg_hatch_timeout(obj);
}

/* try to revive all corpses and eggs carried by `mon' */
int
unturn_dead(struct monst *mon)
{
    struct obj *otmp, *otmp2;
    struct monst *mtmp2;
    char owner[BUFSZ], corpse[BUFSZ];
    boolean youseeit;
    int once = 0, res = 0;

    youseeit = (mon == &youmonst) ? TRUE : canseemon(mon);
    otmp2 = (mon == &youmonst) ? invent : mon->minvent;

    while ((otmp = otmp2) != 0) {
        otmp2 = otmp->nobj;
        if (otmp->otyp == EGG)
            revive_egg(otmp);
        if (otmp->otyp != CORPSE)
            continue;
        /* save the name; the object is liable to go away */
        if (youseeit)
            strcpy(corpse, corpse_xname(otmp, TRUE));

        /* for a merged group, only one is revived; should this be fixed? */
        if ((mtmp2 = revive(otmp)) != 0) {
            ++res;
            if (youseeit) {
                pline("C{N{o,N=%s,N=%s},V{V{come alive},D{suddenly}}}!", corpse,
                      mon_nam(corpse));
            } else if (canseemon(mtmp2))
                pline("C{N=%s,V{V{appear},D{suddenly}}}!", amonnam(mtmp2));
        }
    }
    return res;
}


static const char charged_objs[] = { WAND_CLASS, WEAPON_CLASS, ARMOR_CLASS, 0 };

static void
costly_cancel(struct obj *obj)
{
    char objroom;
    struct monst *shkp = NULL;

    if (obj->no_charge)
        return;

    switch (obj->where) {
    case OBJ_INVENT:
        if (obj->unpaid) {
            shkp = shop_keeper(level, *u.ushops);
            if (!shkp)
                return;
            /* TODO: Add uniquifier to "object" Ensure we get the idiomatic
               version */
            Norep("C{+,C{N=%s,V{V{cancel},N{N{i,object},A{unpaid}}}},"
                  "C{N=%s,V{V{pay for},N{i,object}}}}!", you, you);
            bill_dummy_object(obj);
        }
        break;
    case OBJ_FLOOR:
        objroom = *in_rooms(level, obj->ox, obj->oy, SHOPBASE);
        shkp = shop_keeper(level, objroom);
        if (!costly_spot(obj->ox, obj->oy))
            return;
        /* "if costly_spot(u.ux, u.uy)" is correct. It checks whether shk can
           force the player to pay for the item by blocking the door. */
        if (costly_spot(u.ux, u.uy) && objroom == *u.ushops) {
            /* TODO: Add uniquifier to "it"? Ensure we get the idiomatic
               version */
            Norep("C{+,C{N=%s,V{V{cancel},N{o,it}}},"
                  "C{N=%s,V{V{pay for},N{o,it}}}}!", you, you);
            bill_dummy_object(obj);
        } else
            stolen_value(obj, obj->ox, obj->oy, FALSE, FALSE);
        break;
    }
}

/* cancel obj, possibly carried by you or a monster */
void
cancel_item(struct obj *obj)
{
    boolean u_ring = (obj == uleft) || (obj == uright);
    boolean holy = (obj->otyp == POT_WATER && (obj->blessed || obj->cursed));

    switch (obj->otyp) {
    case RIN_GAIN_STRENGTH:
        if ((obj->owornmask & W_RING) && u_ring) {
            ABON(A_STR) -= obj->spe;
            iflags.botl = 1;
        }
        break;
    case RIN_GAIN_CONSTITUTION:
        if ((obj->owornmask & W_RING) && u_ring) {
            ABON(A_CON) -= obj->spe;
            iflags.botl = 1;
        }
        break;
    case RIN_ADORNMENT:
        if ((obj->owornmask & W_RING) && u_ring) {
            ABON(A_CHA) -= obj->spe;
            iflags.botl = 1;
        }
        break;
    case RIN_INCREASE_ACCURACY:
        if ((obj->owornmask & W_RING) && u_ring)
            u.uhitinc -= obj->spe;
        break;
    case RIN_INCREASE_DAMAGE:
        if ((obj->owornmask & W_RING) && u_ring)
            u.udaminc -= obj->spe;
        break;
    case GAUNTLETS_OF_DEXTERITY:
        if ((obj->owornmask & W_ARMG) && (obj == uarmg)) {
            ABON(A_DEX) -= obj->spe;
            iflags.botl = 1;
        }
        break;
    case HELM_OF_BRILLIANCE:
        if ((obj->owornmask & W_ARMH) && (obj == uarmh)) {
            ABON(A_INT) -= obj->spe;
            ABON(A_WIS) -= obj->spe;
            iflags.botl = 1;
        }
        break;
        /* case RIN_PROTECTION: not needed */
    }
    if (objects[obj->otyp].oc_magic ||
        (obj->spe &&
         (obj->oclass == ARMOR_CLASS || obj->oclass == WEAPON_CLASS ||
          is_weptool(obj)))
        || obj->otyp == POT_ACID || obj->otyp == POT_SICKNESS) {
        if (obj->spe != ((obj->oclass == WAND_CLASS) ? -1 : 0) &&
            obj->otyp != WAN_CANCELLATION &&
            /* can't cancel cancellation */
            obj->otyp != MAGIC_LAMP && obj->otyp != CANDELABRUM_OF_INVOCATION) {
            costly_cancel(obj);
            obj->spe = (obj->oclass == WAND_CLASS) ? -1 : 0;
        }
        switch (obj->oclass) {
        case SCROLL_CLASS:
            costly_cancel(obj);
            obj->otyp = SCR_BLANK_PAPER;
            obj->spe = 0;
            break;
        case SPBOOK_CLASS:
            if (obj->otyp != SPE_CANCELLATION &&
                obj->otyp != SPE_BOOK_OF_THE_DEAD) {
                costly_cancel(obj);
                obj->otyp = SPE_BLANK_PAPER;
            }
            break;
        case POTION_CLASS:
            costly_cancel(obj);
            if (obj->otyp == POT_SICKNESS || obj->otyp == POT_SEE_INVISIBLE) {
                /* sickness is "biologically contaminated" fruit juice; cancel
                   it and it just becomes fruit juice... whereas see invisible
                   tastes like "enchanted" fruit juice, it similarly cancels. */
                obj->otyp = POT_FRUIT_JUICE;
            } else {
                obj->otyp = POT_WATER;
                obj->odiluted = 0;      /* same as any other water */
            }
            break;
        }
    }
    if (holy)
        costly_cancel(obj);
    unbless(obj);
    uncurse(obj);
#ifdef INVISIBLE_OBJECTS
    if (obj->oinvis)
        obj->oinvis = 0;
#endif
    return;
}

/* Remove a positive enchantment or charge from obj,
 * possibly carried by you or a monster
 */
boolean
drain_item(struct obj * obj)
{
    boolean u_ring;

    /* Is this a charged/enchanted object? */
    if (!obj ||
        (!objects[obj->otyp].oc_charged && obj->oclass != WEAPON_CLASS &&
         obj->oclass != ARMOR_CLASS && !is_weptool(obj)) || obj->spe <= 0)
        return FALSE;
    if (obj->oartifact && defends(AD_DRLI, obj))
        return FALSE;
    if (obj_resists(obj, 10, 90))
        return FALSE;

    /* Charge for the cost of the object */
    costly_cancel(obj); /* The term "cancel" is okay for now */

    /* Drain the object and any implied effects */
    obj->spe--;
    u_ring = (obj == uleft) || (obj == uright);
    switch (obj->otyp) {
    case RIN_GAIN_STRENGTH:
        if ((obj->owornmask & W_RING) && u_ring) {
            ABON(A_STR)--;
            iflags.botl = 1;
        }
        break;
    case RIN_GAIN_CONSTITUTION:
        if ((obj->owornmask & W_RING) && u_ring) {
            ABON(A_CON)--;
            iflags.botl = 1;
        }
        break;
    case RIN_ADORNMENT:
        if ((obj->owornmask & W_RING) && u_ring) {
            ABON(A_CHA)--;
            iflags.botl = 1;
        }
        break;
    case RIN_INCREASE_ACCURACY:
        if ((obj->owornmask & W_RING) && u_ring)
            u.uhitinc--;
        break;
    case RIN_INCREASE_DAMAGE:
        if ((obj->owornmask & W_RING) && u_ring)
            u.udaminc--;
        break;
    case HELM_OF_BRILLIANCE:
        if ((obj->owornmask & W_ARMH) && (obj == uarmh)) {
            ABON(A_INT)--;
            ABON(A_WIS)--;
            iflags.botl = 1;
        }
        break;
    case GAUNTLETS_OF_DEXTERITY:
        if ((obj->owornmask & W_ARMG) && (obj == uarmg)) {
            ABON(A_DEX)--;
            iflags.botl = 1;
        }
        break;
    case RIN_PROTECTION:
        iflags.botl = 1;
        break;
    }
    if (carried(obj))
        update_inventory();
    return TRUE;
}

/* ochance, achance:  percent chance for ordinary objects, artifacts */
boolean
obj_resists(struct obj * obj, int ochance, int achance)
{       /* percent chance for ordinary objects, artifacts */
    if (obj->otyp == AMULET_OF_YENDOR || obj->otyp == SPE_BOOK_OF_THE_DEAD ||
        obj->otyp == CANDELABRUM_OF_INVOCATION || obj->otyp == BELL_OF_OPENING
        || (obj->otyp == CORPSE && is_rider(&mons[obj->corpsenm]))) {
        return TRUE;
    } else {
        int chance = rn2(100);

        return (boolean) (chance < (obj->oartifact ? achance : ochance));
    }
}

boolean
obj_shudders(struct obj * obj)
{
    int zap_odds;

    if (obj->oclass == WAND_CLASS)
        zap_odds = 3;   /* half-life = 2 zaps */
    else if (obj->cursed)
        zap_odds = 3;   /* half-life = 2 zaps */
    else if (obj->blessed)
        zap_odds = 12;  /* half-life = 8 zaps */
    else
        zap_odds = 8;   /* half-life = 6 zaps */

    /* adjust for "large" quantities of identical things */
    if (obj->quan > 4L)
        zap_odds /= 2;

    return (boolean) (!rn2(zap_odds));
}


/* Use up at least minwt number of things made of material mat.
 * There's also a chance that other stuff will be used up.  Finally,
 * there's a random factor here to keep from always using the stuff
 * at the top of the pile.
 */
static void
polyuse(struct obj *objhdr, int mat, int minwt)
{
    struct obj *otmp, *otmp2;

    for (otmp = objhdr; minwt > 0 && otmp; otmp = otmp2) {
        otmp2 = otmp->nexthere;
        if (otmp == uball || otmp == uchain)
            continue;
        if (obj_resists(otmp, 0, 0))
            continue;   /* preserve unique objects */

        if (((int)objects[otmp->otyp].oc_material == mat) ==
            (rn2(minwt + 1) != 0)) {
            /* appropriately add damage to bill */
            if (costly_spot(otmp->ox, otmp->oy)) {
                if (*u.ushops)
                    addtobill(otmp, FALSE, FALSE, FALSE);
                else
                    stolen_value(otmp, otmp->ox, otmp->oy, FALSE, FALSE);
            }
            if (otmp->quan < LARGEST_INT)
                minwt -= (int)otmp->quan;
            else
                minwt = 0;
            delobj(otmp);
        }
    }
}

/*
 * Polymorph some of the stuff in this pile into a monster, preferably
 * a golem of the kind okind.
 */
static void
create_polymon(struct obj *obj, int okind)
{
    const struct permonst *mdat = NULL;
    struct monst *mtmp;
    const char *material;
    int pm_index;

    /* no golems if you zap only one object -- not enough stuff */
    if (!obj || (!obj->nexthere && obj->quan == 1L))
        return;

    /* some of these choices are arbitrary */
    switch (okind) {
    case IRON:
    case METAL:
    case MITHRIL:
        pm_index = PM_IRON_GOLEM;
        material = "A{metal}";
        break;
    case COPPER:
    case SILVER:
    case PLATINUM:
    case GEMSTONE:
    case MINERAL:
        pm_index = rn2(2) ? PM_STONE_GOLEM : PM_CLAY_GOLEM;
        material = "A{lithic}";
        break;
    case 0:
    case FLESH:
        /* there is no flesh type, but all food is type 0, so we use it */
        pm_index = PM_FLESH_GOLEM;
        material = "A{organic}";
        break;
    case WOOD:
        pm_index = PM_WOOD_GOLEM;
        material = "A{wood}";
        break;
    case LEATHER:
        pm_index = PM_LEATHER_GOLEM;
        material = "A{leather}";
        break;
    case CLOTH:
        pm_index = PM_ROPE_GOLEM;
        material = "A{cloth}";
        break;
    case BONE:
        pm_index = PM_SKELETON; /* nearest thing to "bone golem" */
        material = "A{bony}";
        break;
    case GOLD:
        pm_index = PM_GOLD_GOLEM;
        material = "A{gold}";
        break;
    case GLASS:
        pm_index = PM_GLASS_GOLEM;
        material = "A{glassy}";
        break;
    case PAPER:
        pm_index = PM_PAPER_GOLEM;
        material = "A{paper}";
        break;
    default:
        /* if all else fails... */
        pm_index = PM_STRAW_GOLEM;
        material = "A{varied}";
        break;
    }

    if (!(mvitals[pm_index].mvflags & G_GENOD))
        mdat = &mons[pm_index];

    mtmp = makemon(mdat, level, obj->ox, obj->oy, NO_MM_FLAGS);
    polyuse(obj, okind, (int)mons[pm_index].cwt);

    if (mtmp && cansee(mtmp->mx, mtmp->my)) {
        /* TODO: Fix "some"; both the "of" and the plural-killing */
        pline("C{+,C{N{f,N{o,some},N{N{o,object},A=%s}},V{meld}},"
              "C{N=%s,V{V{arise},D{e,E{from},N{pile}}}}}!", material,
              a_mon_nam(mtmp));
    }
}

/* Assumes obj is on the floor. */
void
do_osshock(struct obj *obj)
{
    long i;

    obj_zapped = TRUE;

    if (poly_zapped < 0) {
        /* some may metamorphosize */
        for (i = obj->quan; i; i--)
            if (!rn2(Luck + 45)) {
                poly_zapped = objects[obj->otyp].oc_material;
                break;
            }
    }

    /* if quan > 1 then some will survive intact */
    if (obj->quan > 1L) {
        if (obj->quan > LARGEST_INT)
            obj = splitobj(obj, (long)rnd(30000));
        else
            obj = splitobj(obj, (long)rnd((int)obj->quan - 1));
    }

    /* appropriately add damage to bill */
    if (costly_spot(obj->ox, obj->oy)) {
        if (*u.ushops)
            addtobill(obj, FALSE, FALSE, FALSE);
        else
            stolen_value(obj, obj->ox, obj->oy, FALSE, FALSE);
    }

    /* zap the object */
    delobj(obj);
}

/*
 * Polymorph the object to the given object ID.  If the ID is STRANGE_OBJECT
 * then pick random object from the source's class (this is the standard
 * "polymorph" case).  If ID is set to a specific object, inhibit fusing
 * n objects into 1.  This could have been added as a flag, but currently
 * it is tied to not being the standard polymorph case. The new polymorphed
 * object replaces obj in its link chains.  Return value is a pointer to
 * the new object.
 *
 * This should be safe to call for an object anywhere.
 */
struct obj *
poly_obj(struct obj *obj, int id)
{
    struct obj *otmp;
    xchar ox, oy;
    boolean can_merge = (id == STRANGE_OBJECT);
    int obj_location = obj->where;

    if (obj->otyp == BOULDER && In_sokoban(&u.uz))
        change_luck(-1);        /* Sokoban guilt */
    if (id == STRANGE_OBJECT) { /* preserve symbol */
        int try_limit = 3;

        /* Try up to 3 times to make the magic-or-not status of the new item be 
           the same as it was for the old one. */
        otmp = NULL;
        do {
            if (otmp)
                delobj(otmp);
            otmp = mkobj(level, obj->oclass, FALSE);
        } while (--try_limit > 0 &&
                 objects[obj->otyp].oc_magic != objects[otmp->otyp].oc_magic);
    } else {
        /* literally replace obj with this new thing */
        otmp = mksobj(level, id, FALSE, FALSE);
        /* Actually more things use corpsenm but they polymorph differently */
#define USES_CORPSENM(typ) ((typ)==CORPSE || (typ)==STATUE || (typ)==FIGURINE)
        if (USES_CORPSENM(obj->otyp) && USES_CORPSENM(id))
            otmp->corpsenm = obj->corpsenm;
#undef USES_CORPSENM
    }

    /* preserve quantity */
    otmp->quan = obj->quan;
    /* preserve the shopkeepers (lack of) interest */
    otmp->no_charge = obj->no_charge;
    /* preserve inventory letter if in inventory */
    if (obj_location == OBJ_INVENT)
        otmp->invlet = obj->invlet;

    /* avoid abusing eggs laid by you */
    if (obj->otyp == EGG && obj->spe) {
        int mnum, tryct = 100;

        /* first, turn into a generic egg */
        if (otmp->otyp == EGG)
            kill_egg(otmp);
        else {
            otmp->otyp = EGG;
            otmp->owt = weight(otmp);
        }
        otmp->corpsenm = NON_PM;
        otmp->spe = 0;

        /* now change it into something layed by the hero */
        while (tryct--) {
            mnum = can_be_hatched(rn2(NUMMONS));
            if (mnum != NON_PM && !dead_species(mnum, TRUE)) {
                otmp->spe = 1;  /* layed by hero */
                otmp->corpsenm = mnum;
                attach_egg_hatch_timeout(otmp);
                break;
            }
        }
    }

    /* keep special fields (including charges on wands) */
    if (strchr(charged_objs, otmp->oclass))
        otmp->spe = obj->spe;
    otmp->recharged = obj->recharged;

    otmp->cursed = obj->cursed;
    otmp->blessed = obj->blessed;
    otmp->oeroded = obj->oeroded;
    otmp->oeroded2 = obj->oeroded2;
    if (!is_flammable(otmp) && !is_rustprone(otmp))
        otmp->oeroded = 0;
    if (!is_corrodeable(otmp) && !is_rottable(otmp))
        otmp->oeroded2 = 0;
    if (is_damageable(otmp))
        otmp->oerodeproof = obj->oerodeproof;

    /* Keep chest/box traps and poisoned ammo if we may */
    if (obj->otrapped && Is_box(otmp))
        otmp->otrapped = TRUE;

    if (obj->opoisoned && is_poisonable(otmp))
        otmp->opoisoned = TRUE;

    if (id == STRANGE_OBJECT && obj->otyp == CORPSE) {
        /* turn crocodile corpses into shoes */
        if (obj->corpsenm == PM_CROCODILE) {
            otmp->otyp = LOW_BOOTS;
            otmp->oclass = ARMOR_CLASS;
            otmp->spe = 0;
            otmp->oeroded = 0;
            otmp->oerodeproof = TRUE;
            otmp->quan = 1L;
            otmp->cursed = FALSE;
        }
    }

    /* no box contents --KAA */
    if (Has_contents(otmp))
        delete_contents(otmp);

    /* 'n' merged objects may be fused into 1 object */
    if (otmp->quan > 1L &&
        (!objects[otmp->otyp].oc_merge ||
         (can_merge && otmp->quan > (long)rn2(1000))))
        otmp->quan = 1L;

    switch (otmp->oclass) {

    case TOOL_CLASS:
        if (otmp->otyp == MAGIC_LAMP) {
            otmp->otyp = OIL_LAMP;
            otmp->age = 1500L;  /* "best" oil lamp possible */
        } else if (otmp->otyp == MAGIC_MARKER) {
            otmp->recharged = 1;        /* degraded quality */
        }
        /* don't care about the recharge count of other tools */
        break;

    case WAND_CLASS:
        while (otmp->otyp == WAN_WISHING || otmp->otyp == WAN_POLYMORPH)
            otmp->otyp = rnd_class(WAN_LIGHT, WAN_LIGHTNING);
        /* altering the object tends to degrade its quality (analogous to
           spellbook `read count' handling) */
        if ((int)otmp->recharged < rn2(7))      /* recharge_limit */
            otmp->recharged++;
        break;

    case POTION_CLASS:
        while (otmp->otyp == POT_POLYMORPH)
            otmp->otyp = rnd_class(POT_GAIN_ABILITY, POT_WATER);
        break;

    case SPBOOK_CLASS:
        while (otmp->otyp == SPE_POLYMORPH)
            otmp->otyp = rnd_class(SPE_DIG, SPE_BLANK_PAPER);
        /* reduce spellbook abuse */
        otmp->spestudied = obj->spestudied + 1;
        break;

    case GEM_CLASS:
        if (otmp->quan > (long)rnd(4) &&
            objects[obj->otyp].oc_material == MINERAL &&
            objects[otmp->otyp].oc_material != MINERAL) {
            otmp->otyp = ROCK;  /* transmutation backfired */
            otmp->quan /= 2L;   /* some material has been lost */
        }
        break;
    }

    /* update the weight */
    otmp->owt = weight(otmp);

    /* for now, take off worn items being polymorphed */
    if (obj_location == OBJ_INVENT) {
        if (id == STRANGE_OBJECT)
            remove_worn_item(obj, TRUE);
        else {
            /* This is called only for stone to flesh.  It's a lot simpler than 
               it otherwise might be.  We don't need to check for special
               effects when putting them on (no meat objects have any) and only 
               three worn masks are possible. */
            otmp->owornmask = obj->owornmask;
            remove_worn_item(obj, TRUE);
            setworn(otmp, otmp->owornmask);
            if (otmp->owornmask & LEFT_RING)
                uleft = otmp;
            if (otmp->owornmask & RIGHT_RING)
                uright = otmp;
            if (otmp->owornmask & W_WEP)
                uwep = otmp;
            if (otmp->owornmask & W_SWAPWEP)
                uswapwep = otmp;
            if (otmp->owornmask & W_QUIVER)
                uquiver = otmp;
            goto no_unwear;
        }
    }

    /* preserve the mask in case being used by something else */
    otmp->owornmask = obj->owornmask;
no_unwear:

    if (obj_location == OBJ_FLOOR && obj->otyp == BOULDER &&
        otmp->otyp != BOULDER)
        unblock_point(obj->ox, obj->oy);

    /* ** we are now done adjusting the object ** */


    /* swap otmp for obj */
    replace_object(obj, otmp);
    if (obj_location == OBJ_INVENT) {
        /* 
         * We may need to do extra adjustments for the hero if we're
         * messing with the hero's inventory.  The following calls are
         * equivalent to calling freeinv on obj and addinv on otmp,
         * while doing an in-place swap of the actual objects.
         */
        freeinv_core(obj);
        addinv_core1(otmp);
        addinv_core2(otmp);
    }

    if ((!carried(otmp) || obj->unpaid) &&
        get_obj_location(otmp, &ox, &oy, BURIED_TOO | CONTAINED_TOO) &&
        costly_spot(ox, oy)) {
        struct monst *shkp =
            shop_keeper(level, *in_rooms(level, ox, oy, SHOPBASE));

        if ((!obj->no_charge ||
             (Has_contents(obj) &&
              (contained_cost(obj, shkp, 0L, FALSE, FALSE) != 0L)))
            && inhishop(shkp)) {
            if (shkp->mpeaceful) {
                if (*u.ushops &&
                    *in_rooms(level, u.ux, u.uy, 0) == *in_rooms(level,
                                                                 shkp->mx,
                                                                 shkp->my, 0) &&
                    !costly_spot(u.ux, u.uy))
                    make_angry_shk(shkp, ox, oy);
                else {
                    pline("C{N=%s,V{V{get},A{angry}}}!", mon_nam(shkp));
                    hot_pursuit(shkp);
                }
            } else
                Norep("C{N=%s,V{V{are},A{furious}}}!", mon_nam(shkp));
        }
    }
    delobj(obj);
    return otmp;
}


static int
hito_stone_to_flesh(struct obj *obj)
{
    int res = 1;
    boolean smell = FALSE;
    xchar refresh_x = obj->ox;
    xchar refresh_y = obj->oy;

    if (objects[obj->otyp].oc_material != MINERAL &&
        objects[obj->otyp].oc_material != GEMSTONE)
        return 0;

    /* add more if stone objects are added.. */
    switch (objects[obj->otyp].oc_class) {
    case ROCK_CLASS:   /* boulders and statues */
        if (obj->otyp == BOULDER) {
            poly_obj(obj, HUGE_CHUNK_OF_MEAT);
            smell = TRUE;
            break;
        } else if (obj->otyp == STATUE) {
            xchar oox, ooy;

            get_obj_location(obj, &oox, &ooy, 0);
            refresh_x = oox;
            refresh_y = ooy;
            if (vegetarian(&mons[obj->corpsenm])) {
                /* Don't animate monsters that aren't flesh */
                poly_obj(obj, MEATBALL);
                smell = TRUE;
                break;
            }
            if (!animate_statue(obj, oox, ooy, ANIMATE_SPELL, NULL)) {
                struct obj *item;

            makecorpse:if (mons[obj->corpsenm].geno & (G_NOCORPSE | G_UNIQ))
                {
                    res = 0;
                    break;
                }
                /* Unlikely to get here since genociding * monsters also sets
                   the G_NOCORPSE flag. * Drop the contents, poly_obj looses
                   them. */
                while ((item = obj->cobj) != 0) {
                    obj_extract_self(item);
                    place_object(item, level, oox, ooy);
                }
                poly_obj(obj, CORPSE);
                break;
            }
        } else {        /* new rock class object... */
            /* impossible? */
            res = 0;
        }
        break;
    case TOOL_CLASS:   /* figurine */
        {
            struct monst *mon;
            xchar oox, ooy;

            if (obj->otyp != FIGURINE) {
                res = 0;
                break;
            }
            if (vegetarian(&mons[obj->corpsenm])) {
                /* Don't animate monsters that aren't flesh */
                poly_obj(obj, MEATBALL);
                smell = TRUE;
                break;
            }
            get_obj_location(obj, &oox, &ooy, 0);
            refresh_x = oox;
            refresh_y = ooy;
            mon = makemon(&mons[obj->corpsenm], level, oox, ooy, NO_MM_FLAGS);
            if (mon) {
                delobj(obj);
                if (cansee(mon->mx, mon->my))
                    pline("C{N{figurine},V{animate}}!");
                break;
            }
            goto makecorpse;
        }
        /* maybe add weird things to become? */
    case RING_CLASS:   /* some of the rings are stone */
        poly_obj(obj, MEAT_RING);
        smell = TRUE;
        break;
    case WAND_CLASS:   /* marble wand */
        poly_obj(obj, MEAT_STICK);
        smell = TRUE;
        break;
    case GEM_CLASS:    /* rocks & gems */
        poly_obj(obj, MEATBALL);
        smell = TRUE;
        break;
    case WEAPON_CLASS: /* crysknife */
        /* fall through */
    default:
        res = 0;
        break;
    }

    if (smell) {
        if (herbivorous(youmonst.data) &&
            (!carnivorous(youmonst.data) || Role_if(PM_MONK) ||
             !u.uconduct.unvegetarian))
            Norep("C{N=%s,V{V{smell},N{l,N{odor},N{o,meat}}}}.");
        else
            Norep("C{N=%s,V{V{smell},N{N{i,smell},A{delicious}}}}.");
    }

    newsym(refresh_x, refresh_y);
    return res;
}

/*
 * Object obj was hit by the effect of the wand/spell otmp.  Return
 * non-zero if the wand/spell had any effect.
 */
int
bhito(struct obj *obj, struct obj *otmp)
{
    int res = 1;        /* affected object by default */

    if (obj->bypass) {
        /* The bypass bit is currently only used as follows: POLYMORPH - When a 
           monster being polymorphed drops something from its inventory as a
           result of the change.  If the items fall to the floor, they are not
           subject to direct subsequent polymorphing themselves on that same
           zap. This makes it consistent with items that remain in the
           monster's inventory. They are not polymorphed either. UNDEAD_TURNING 
           - When an undead creature gets killed via undead turning, prevent
           its corpse from being immediately revived by the same effect. The
           bypass bit on all objects is reset each turn, whenever
           flags.bypasses is set. We check the obj->bypass bit above AND
           flags.bypasses as a safeguard against any stray occurrence left in
           an obj struct someplace, although that should never happen. */
        if (flags.bypasses)
            return 0;
        else
            obj->bypass = 0;
    }

    /* 
     * Some parts of this function expect the object to be on the floor
     * obj->{ox,oy} to be valid.  The exception to this (so far) is
     * for the STONE_TO_FLESH spell.
     */
    if (!(obj->where == OBJ_FLOOR || otmp->otyp == SPE_STONE_TO_FLESH))
        impossible("S{bhito: obj is not floor or Stone To Flesh spell}");

    if (obj == uball) {
        res = 0;
    } else if (obj == uchain) {
        if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK) {
            unpunish();
            makeknown(otmp->otyp);
        } else
            res = 0;
    } else
        switch (otmp->otyp) {
        case WAN_POLYMORPH:
        case SPE_POLYMORPH:
            if (obj->otyp == WAN_POLYMORPH || obj->otyp == SPE_POLYMORPH ||
                obj->otyp == POT_POLYMORPH || obj_resists(obj, 5, 95)) {
                res = 0;
                break;
            }
            /* KMH, conduct */
            u.uconduct.polypiles++;
            /* any saved lock context will be dangerously obsolete */
            if (Is_box(obj))
                boxlock(obj, otmp);

            if (obj_shudders(obj)) {
                if (cansee(obj->ox, obj->oy))
                    makeknown(otmp->otyp);
                do_osshock(obj);
                break;
            }
            obj = poly_obj(obj, STRANGE_OBJECT);
            /* poly_obj doesn't block vision, do that ourselves now. */
            if (obj->where == OBJ_FLOOR && obj->otyp == BOULDER)
                block_point(obj->ox, obj->oy);
            newsym(obj->ox, obj->oy);
            break;
        case WAN_PROBING:
            res = !obj->dknown;
            /* target object has now been "seen (up close)" */
            obj->dknown = 1;
            if (Is_container(obj) || obj->otyp == STATUE) {
                boolean quantum_cat = FALSE;

                if ((obj->spe == 1) && (obj->otyp != STATUE)) {
                    observe_quantum_cat(obj);
                    quantum_cat = TRUE;
                }
                if (!obj->cobj)
                    pline("C{N=%s,V{V{are},A=%s}}.", xname(obj),
                          quantum_cat ? "A{A{empty},D{now}}" : "A{empty}");
                else {
                    struct obj *o;

                    /* view contents (not recursively) */
                    for (o = obj->cobj; o; o = o->nobj)
                        o->dknown = 1;  /* "seen", even if blind */
                    display_cinventory(obj);
                }
                res = 1;
            }
            if (res)
                makeknown(WAN_PROBING);
            break;
        case WAN_STRIKING:
        case SPE_FORCE_BOLT:
            if (obj->otyp == BOULDER)
                fracture_rock(obj);
            else if (obj->otyp == STATUE)
                break_statue(obj);
            else {
                if (!flags.mon_moving)
                    hero_breaks(obj, obj->ox, obj->oy, FALSE);
                else
                    breaks(obj, obj->ox, obj->oy);
                res = 0;
            }
            /* BUG[?]: shouldn't this depend upon you seeing it happen? */
            makeknown(otmp->otyp);
            break;
        case WAN_CANCELLATION:
        case SPE_CANCELLATION:
            cancel_item(obj);
            newsym(obj->ox, obj->oy);   /* might change color */
            break;
        case SPE_DRAIN_LIFE:
            drain_item(obj);
            break;
        case WAN_TELEPORTATION:
        case SPE_TELEPORT_AWAY:
            rloco(obj);
            break;
        case WAN_MAKE_INVISIBLE:
#ifdef INVISIBLE_OBJECTS
            obj->oinvis = TRUE;
            newsym(obj->ox, obj->oy);   /* make object disappear */
#endif
            break;
        case WAN_UNDEAD_TURNING:
        case SPE_TURN_UNDEAD:
            if (obj->otyp == EGG)
                revive_egg(obj);
            else
                res = ! !revive(obj);
            break;
        case WAN_OPENING:
        case SPE_KNOCK:
        case WAN_LOCKING:
        case SPE_WIZARD_LOCK:
            if (Is_box(obj))
                res = boxlock(obj, otmp);
            else
                res = 0;
            if (res /* && otmp->oclass == WAND_CLASS */ )
                makeknown(otmp->otyp);
            break;
        case WAN_SLOW_MONSTER: /* no effect on objects */
        case SPE_SLOW_MONSTER:
        case WAN_SPEED_MONSTER:
        case WAN_NOTHING:
        case SPE_HEALING:
        case SPE_EXTRA_HEALING:
            res = 0;
            break;
        case SPE_STONE_TO_FLESH:
            res = hito_stone_to_flesh(obj);
            break;
        default:
            impossible("S{What an interesting effect (%d)}", otmp->otyp);
            break;
        }
    return res;
}

/* returns nonzero if something was hit */
int
bhitpile(struct obj *obj, int (*fhito) (struct obj *, struct obj *), int tx,
         int ty)
{
    int hitanything = 0;
    struct obj *otmp, *next_obj;

    if (obj->otyp == SPE_FORCE_BOLT || obj->otyp == WAN_STRIKING) {
        struct trap *t = t_at(level, tx, ty);

        /* We can't settle for the default calling sequence of bhito(otmp) ->
           break_statue(otmp) -> activate_statue_trap(ox,oy) because that last
           call might end up operating on our `next_obj' (below), rather than
           on the current object, if it happens to encounter a statue which
           mustn't become animated. */
        if (t && t->ttyp == STATUE_TRAP && activate_statue_trap(t, tx, ty, TRUE)
            && obj->otyp == WAN_STRIKING)
            makeknown(obj->otyp);
    }

    poly_zapped = -1;
    for (otmp = level->objects[tx][ty]; otmp; otmp = next_obj) {
        /* Fix for polymorph bug, Tim Wright */
        next_obj = otmp->nexthere;
        hitanything += (*fhito) (otmp, obj);
    }
    if (poly_zapped >= 0)
        create_polymon(level->objects[tx][ty], poly_zapped);

    return hitanything;
}


/*
 * wrestable - returns 1 if a wand can only be zapped
 *             by wresting it.
 * added by bcd@pvv.org 16/9/08
 */
int
wrestable(struct obj *wand)
{
    return (wand->spe == 0);
}

/*
 * zappable - returns 1 if zap is available, 0 otherwise.
 *            it removes a charge from the wand if zappable.
 * added by GAN 11/03/86
 */
int
zappable(struct obj *wand)
{
    if (wand->spe < 0 || (wand->spe == 0 && rn2(121))) {
        pline("C{N=%s,V{V{feel},N{l,N{i,absence},"
              "N{N{o,power},A{magical}}}}}.", you);
        wand->known = 1;        /* we know the :0 */
        return 0;
    }

    if (wand->spe == 0)
        pline("C{N=%s,V{V{V{wrest},N{N{o,charge},A{one last}}},"
              "D{E{from},N{N{wand},A{worn-out}}}}}.", you)
            wand->spe--;
    return 1;
}

/*
 * zapnodir - zaps a NODIR wand/spell.
 * added by GAN 11/03/86
 */
void
zapnodir(struct obj *obj)
{
    boolean known = FALSE;

    switch (obj->otyp) {
    case WAN_LIGHT:
    case SPE_LIGHT:
        litroom(TRUE, obj);
        if (!Blind)
            known = TRUE;
        break;
    case WAN_SECRET_DOOR_DETECTION:
    case SPE_DETECT_UNSEEN:
        if (!findit())
            return;
        if (!Blind)
            known = TRUE;
        break;
    case WAN_CREATE_MONSTER:
        known = create_critters(rn2(23) ? 1 : rn1(7, 2), NULL);
        break;
    case WAN_WISHING:
        known = TRUE;
        if (Luck + rn2(5) < 0) {
            pline("C{C{N{o,nothing},V{happen}},D{unfortunately}}.");
            break;
        }
        makewish();
        break;
    case WAN_ENLIGHTENMENT:
        known = TRUE;
        pline("C{N=%s,V{V{feel},A{self-knowledgeable}}}...");
        win_pause_output(P_MESSAGE);
        enlightenment(FALSE);
        pline("C{N{feeling},V{subside}}.");
        exercise(A_WIS, TRUE);
        break;
    }
    if (known && !objects[obj->otyp].oc_name_known) {
        makeknown(obj->otyp);
        more_experienced(0, 10);
    }
}


static void
backfire(struct obj *otmp)
{
    otmp->in_use = TRUE;        /* in case losehp() is fatal */
    if (otmp->oartifact) {
        /* 
         * Artifacts aren't destroyed by a backfire, but the
         * explosion is more violent.
         */
        pline("C{N=%s,V{V{V{produce},D{suddenly}},"
              "N{N{l,N{i,outburst},N{o,energy}},A{violent}}}}!", xname(otmp));
        losehp(dice(otmp->spe + 4, 8), "N{N{i,wand},A{outbursting}}",
               KILLED_BY_AN);
    } else {
        pline("C{N=%s,V{V{explode},D{suddenly}}}!", xname(otmp));
        losehp(dice(otmp->spe + 2, 6), "N{N{i,wand},A{exploding}}",
               KILLED_BY_AN);
        useup(otmp);
    }
}

static const char zap_syms[] = { WAND_CLASS, 0 };

int
dozap(struct obj *obj)
{
    int damage;
    schar dx = 0, dy = 0, dz = 0;

    if (check_capacity(NULL))
        return 0;

    if (obj && !validate_object(obj, zap_syms, /*nointl*/ "zap"))
        return 0;
    else if (!obj)
        obj = getobj(zap_syms, /*nointl*/ "zap");
    if (!obj)
        return 0;

    check_unpaid(obj);

    if (obj->oartifact && !touch_artifact(obj, &youmonst))
        return 1;

    /* zappable addition done by GAN 11/03/86 */
    if (!zappable(obj)) {       /* zappable prints the message itself */
    } else if (obj->cursed && !rn2(100)) {
        backfire(obj);  /* the wand blows up in your face! */
        exercise(A_STR, FALSE);
        return 1;
    } else if (!(objects[obj->otyp].oc_dir == NODIR) &&
               !getdir(NULL, &dx, &dy, &dz)) {
        if (!Blind)
            pline("C{N=%s,V{+,V{glow},V{fade}}}.", xname(obj));
        /* make him pay for knowing !NODIR */
    } else if (!dx && !dy && !dz && !(objects[obj->otyp].oc_dir == NODIR)) {
        if ((damage = zapyourself(obj, TRUE)) != 0) {
            char buf[BUFSZ];

            sprintf(buf, "V{V{V{zap},N=%s},D{t,N{i,wand}}}", uhim());
            losehp(damage, buf, NO_KILLER_PREFIX);
        }
    } else {

        /* Are we having fun yet? weffects -> buzz(obj->otyp) -> zhitm (temple
           priest) -> attack -> hitum -> known_hitum -> ghod_hitsu ->
           buzz(AD_ELEC) -> destroy_item(WAND_CLASS) -> useup -> obfree ->
           dealloc_obj -> free(obj) */
        current_wand = obj;
        weffects(obj, dx, dy, dz);
        obj = current_wand;
        current_wand = 0;
    }
    if (obj && obj->spe < 0) {
        pline("C{N=%s,V{V{turn to},N{o,dust}}}.", xname(obj));
        useup(obj);
    }
    update_inventory(); /* maybe used a charge */
    return 1;
}

int
zapyourself(struct obj *obj, boolean ordinary)
{
    int damage = 0;
    char buf[BUFSZ];

    switch (obj->otyp) {
    case WAN_STRIKING:
        makeknown(WAN_STRIKING);
    case SPE_FORCE_BOLT:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("S{Boing!}");
        } else {
            if (ordinary) {
                pline("C{N=%s,V{V{bash},N=%s}}!", you, you);
                damage = dice(2, 12);
            } else
                damage = dice(1 + obj->spe, 6);
            exercise(A_STR, FALSE);
        }
        break;

    case WAN_LIGHTNING:
        makeknown(WAN_LIGHTNING);
        if (!Shock_resistance) {
            pline("C{N=%s,V{V{shock},N=%s}}!", you, you);
            damage = dice(12, 6);
            exercise(A_CON, FALSE);
        } else {
            shieldeff(u.ux, u.uy);
            pline("C{N=%s,V{+,V{V{zap},N=%s},V{-,V{V{seem},A{unharmed}}}}}.",
                  you, you);
            ugolemeffects(AD_ELEC, dice(12, 6));
        }
        destroy_item(WAND_CLASS, AD_ELEC);
        destroy_item(RING_CLASS, AD_ELEC);
        if (!resists_blnd(&youmonst)) {
            pline(blinded_by_the_flash);
            make_blinded((long)rnd(100), FALSE);
            if (!Blind)
                pline("C{N{o,N{vision},N=%s},V{V{clear},D{quickly}}}.");
        }
        break;

    case SPE_FIREBALL:
        pline("C{N=%s,V{V{V{explode},N{i,fireball}},D{e,E{on top of},N=%s}}}!",
              you, you);
        explode(u.ux, u.uy, 11, dice(6, 6), WAND_CLASS, EXPL_FIERY);
        break;
    case WAN_FIRE:
        makeknown(WAN_FIRE);
    case FIRE_HORN:
        if (Fire_resistance) {
            shieldeff(u.ux, u.uy);
            pline("C{N=%s,V{V{feel},A{A{warm},D{rather}}}}.", you);
            ugolemeffects(AD_FIRE, dice(12, 6));
        } else {
            pline("C{p,N=%s,V{V{V{set},A{afire}},N=%s}}!", you, you);
            damage = dice(12, 6);
        }
        burn_away_slime();
        burnarmor(&youmonst);
        destroy_item(SCROLL_CLASS, AD_FIRE);
        destroy_item(POTION_CLASS, AD_FIRE);
        destroy_item(SPBOOK_CLASS, AD_FIRE);
        break;

    case WAN_COLD:
        makeknown(WAN_COLD);
    case SPE_CONE_OF_COLD:
    case FROST_HORN:
        if (Cold_resistance) {
            shieldeff(u.ux, u.uy);
            pline("C{N=%s,V{V{feel},A{A{chill},D{a little}}}}.", you);
            ugolemeffects(AD_COLD, dice(12, 6));
        } else {
            pline("C{N=%s,V{V{imitate},A{i,popsicle}}}!", you);
            damage = dice(12, 6);
        }
        destroy_item(POTION_CLASS, AD_COLD);
        break;

    case WAN_MAGIC_MISSILE:
        makeknown(WAN_MAGIC_MISSILE);
    case SPE_MAGIC_MISSILE:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("C{N{*,N{missile^magic}},V{bounce^reflect}}!");
        } else {
            damage = dice(4, 6);
            pline("S{Idiot!} C{p,N=%s,V{V{shoot},N=%s}}!", you, you);
        }
        break;

    case WAN_POLYMORPH:
        if (!Unchanging)
            makeknown(WAN_POLYMORPH);
    case SPE_POLYMORPH:
        if (!Unchanging)
            polyself(FALSE);
        break;

    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
        cancel_monst(&youmonst, obj, TRUE, FALSE, TRUE);
        break;

    case SPE_DRAIN_LIFE:
        if (!Drain_resistance) {
            losexp("N{life drainage}");
            makeknown(obj->otyp);
        }
        damage = 0;     /* No additional damage */
        break;

    case WAN_MAKE_INVISIBLE:{
            /* have to test before changing HInvis but must change HInvis
               before doing newsym(). */
            int msg = !Invis && !Blind && !BInvis;

            if (BInvis && uarmc->otyp == MUMMY_WRAPPING) {
                /* A mummy wrapping absorbs it and protects you */
                pline("C{N=%s,V{V{V{feel},A{A{itchy},D{rather}}},"
                      "D{e,E{under},N{o,N=%s,N=%s}}}}.", you, xname(uarmc),
                      you);
                break;
            }
            if (ordinary || !rn2(10)) { /* permanent */
                HInvis |= FROMOUTSIDE;
            } else {    /* temporary */
                incr_itimeout(&HInvis, dice(obj->spe, 250));
            }
            if (msg) {
                makeknown(WAN_MAKE_INVISIBLE);
                newsym(u.ux, u.uy);
                self_invis_message();
            }
            break;
        }

    case WAN_SPEED_MONSTER:
        if (!(HFast & INTRINSIC)) {
            if (!Fast)
                pline("C{N=%s,V{speed up}}.");
            else
                pline("C{N{o,N{quickness},N=%s},"
                      "V{V{feel},A{c,A{natural}}}}.", you);
            makeknown(WAN_SPEED_MONSTER);
            exercise(A_DEX, TRUE);
        }
        HFast |= FROMOUTSIDE;
        break;

    case WAN_SLEEP:
        makeknown(WAN_SLEEP);
    case SPE_SLEEP:
        if (Sleep_resistance) {
            shieldeff(u.ux, u.uy);
            pline("C{N=%s,V{V{-,V{do}},V{V{feel},A{sleepy}}}}!", you);
        } else {
            pline("C{N{sleep ray},V{V{hit},N=%s}}!", you);
            fall_asleep(-rnd(50), TRUE);
        }
        break;

    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
        if (HFast & (TIMEOUT | INTRINSIC)) {
            u_slow_down();
            makeknown(obj->otyp);
        }
        break;

    case WAN_TELEPORTATION:
    case SPE_TELEPORT_AWAY:
        tele();
        if (Teleport_control || !couldsee(u.ux0, u.uy0) ||
            (distu(u.ux0, u.uy0) >= 16))
            makeknown(obj->otyp);
        break;

    case WAN_DEATH:
    case SPE_FINGER_OF_DEATH:
        if (nonliving(youmonst.data) || is_demon(youmonst.data)) {
            pline((obj->otyp ==
                   WAN_DEATH) ? "C{N{wand},V{V{V{shoot},N{N{i,beam},"
                  "A{A{harmless},D{apparently}}}},D{E{at^toward},N=%s}}}." :
                  "C{N=%s,V{V{seem},"
                  "A{A{-,A{c,A{dead^er}}},D{E{than},D{before}}}}}.");
            break;
        }
        /* TODO: with/by */
        sprintf(buf, "V{V{V{shoot},N=%s},D{t,N{i,death ray}}}", you);
        killer = buf;
        killer_format = NO_KILLER_PREFIX;
        /* TODO: with/by */
        pline("C{N=%s,V{V{V{irradiate},N=%s},D{t,N{N{o,energy},A{pure}}}}}!",
              you, you);
        pline("C{N=%s,V{die}}.", you);
        makeknown(obj->otyp);
        /* They might survive with an amulet of life saving */
        done(DIED);
        break;
    case WAN_UNDEAD_TURNING:
        makeknown(WAN_UNDEAD_TURNING);
    case SPE_TURN_UNDEAD:
        unturn_dead(&youmonst);
        if (is_undead(youmonst.data)) {
            pline("C{N=%s,V{V{feel},A{+,A{frigtened},A=%s}}}.", you,
                  Stunned ? "A{A{stunned},D{D{more},D{even}}}" : "A{stunned}");
            make_stunned(HStun + rnd(30), FALSE);
        } else
            pline("C{N=%s,V{V{shudder},D{E{in^with},N{dread}}}}.");
        break;
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
        healup(dice(6, obj->otyp == SPE_EXTRA_HEALING ? 8 : 4), 0, FALSE,
               (obj->otyp == SPE_EXTRA_HEALING));
        pline("C{N=%s,V{V{feel},A=%s}}.", you,
              obj->otyp ==
              SPE_EXTRA_HEALING ? "A{A{better},D{much}}" : "A{better}");
        break;
    case WAN_LIGHT:    /* (broken wand) */
        /* assert( !ordinary ); */
        damage = dice(obj->spe, 25);
    case EXPENSIVE_CAMERA:
        damage += rnd(25);
        if (!resists_blnd(&youmonst)) {
            pline(blinded_by_the_flash);
            make_blinded((long)damage, FALSE);
            makeknown(obj->otyp);
            if (!Blind)
                pline("C{N{o,N{vision},N=%s},V{V{clear},D{quickly}}}.", you);
        }
        damage = 0;     /* reset */
        break;
    case WAN_OPENING:
        if (Punished)
            makeknown(WAN_OPENING);
    case SPE_KNOCK:
        if (Punished)
            pline("C{N{o,N{chain},N=%s},V{V{quiver},D{d,N{i,moment}}}}.");
        break;
    case WAN_DIGGING:
    case SPE_DIG:
    case SPE_DETECT_UNSEEN:
    case WAN_NOTHING:
    case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        break;
    case WAN_PROBING:
        for (obj = invent; obj; obj = obj->nobj)
            obj->dknown = 1;
        /* note: `obj' reused; doesn't point at wand anymore */
        makeknown(WAN_PROBING);
        ustatusline();
        break;
    case SPE_STONE_TO_FLESH:
        {
            struct obj *otemp, *onext;
            boolean didmerge;

            if (u.umonnum == PM_STONE_GOLEM)
                polymon(PM_FLESH_GOLEM);
            if (Stoned)
                fix_petrification();    /* saved! */
            /* but at a cost.. */
            for (otemp = invent; otemp; otemp = onext) {
                onext = otemp->nobj;
                bhito(otemp, obj);
            }
            /* 
             * It is possible that we can now merge some inventory.
             * Do a higly paranoid merge.  Restart from the beginning
             * until no merges.
             */
            do {
                didmerge = FALSE;
                for (otemp = invent; !didmerge && otemp; otemp = otemp->nobj)
                    for (onext = otemp->nobj; onext; onext = onext->nobj)
                        if (merged(&otemp, &onext)) {
                            didmerge = TRUE;
                            break;
                        }
            } while (didmerge);
        }
        break;
    default:
        impossible("S{object %d used?}", obj->otyp);
        break;
    }
    return damage;
}

/* you've zapped a wand downwards while riding
 * Return TRUE if the steed was hit by the wand.
 * Return FALSE if the steed was not hit by the wand.
 */
/* obj: wand or spell */
static boolean
zap_steed(struct obj *obj)
{
    int steedhit = FALSE;

    switch (obj->otyp) {

        /* 
         * Wands that are allowed to hit the steed
         * Carefully test the results of any that are
         * moved here from the bottom section.
         */
    case WAN_PROBING:
        probe_monster(u.usteed);
        makeknown(WAN_PROBING);
        steedhit = TRUE;
        break;
    case WAN_TELEPORTATION:
    case SPE_TELEPORT_AWAY:
        /* you go together */
        tele();
        if (Teleport_control || !couldsee(u.ux0, u.uy0) ||
            (distu(u.ux0, u.uy0) >= 16))
            makeknown(obj->otyp);
        steedhit = TRUE;
        break;

        /* Default processing via bhitm() for these */
    case SPE_CURE_SICKNESS:
    case WAN_MAKE_INVISIBLE:
    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
    case WAN_SPEED_MONSTER:
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
    case SPE_DRAIN_LIFE:
    case WAN_OPENING:
    case SPE_KNOCK:
        bhitm(u.usteed, obj);
        steedhit = TRUE;
        break;

    default:
        steedhit = FALSE;
        break;
    }
    return steedhit;
}


/*
 * cancel a monster (possibly the hero).  inventory is cancelled only
 * if the monster is zapping itself directly, since otherwise the
 * effect is too strong.  currently non-hero monsters do not zap
 * themselves with cancellation.
 */
boolean
cancel_monst(struct monst * mdef, struct obj * obj, boolean youattack,
             boolean allow_cancel_kill, boolean self_cancel)
{
    boolean youdefend = (mdef == &youmonst);
    static const char writing_vanishes[] =
        "C{N{f,N{o,some},N{writing}},V{V{vanish},D{e,E{from},N{o,N{head},N=%s}}}}!";

    if (youdefend ? (!youattack && Antimagic)
        : resist(mdef, obj->oclass, 0, NOTELL))
        return FALSE;   /* resisted cancellation */

    if (self_cancel) {  /* 1st cancel inventory */
        struct obj *otmp;

        for (otmp = (youdefend ? invent : mdef->minvent); otmp;
             otmp = otmp->nobj)
            cancel_item(otmp);
        if (youdefend) {
            iflags.botl = 1;    /* potential AC change */
            find_ac();
        }
    }

    /* now handle special cases */
    if (youdefend) {
        if (Upolyd) {
            if ((u.umonnum == PM_CLAY_GOLEM) && !Blind)
                pline(writing_vanishes, you);

            if (Unchanging)
                /* TODO: "then", not "and" */
                pline("C{N=%s,V{+,V{V{V{grow},N{hot}},D{d,N{i,moment}}},"
                      "V{cool}}}.", you);
            else
                rehumanize();
        }
    } else {
        mdef->mcan = TRUE;

        if (is_were(mdef->data) && mdef->data->mlet != S_HUMAN)
            were_change(mdef);

        if (mdef->data == &mons[PM_CLAY_GOLEM]) {
            if (canseemon(mdef))
                pline(writing_vanishes, mon_nam(mdef));

            if (allow_cancel_kill) {
                if (youattack)
                    killed(mdef);
                else
                    monkilled(mdef, "", AD_SPEL);
            }
        }
    }
    return TRUE;
}

/* you've zapped an immediate type wand up or down */
/* obj: wand or spell */
static boolean
zap_updown(struct obj *obj, schar dz)
{
    boolean striking = FALSE, disclose = FALSE;
    int x, y, xx, yy, ptmp;
    struct obj *otmp;
    struct engr *e;
    struct trap *ttmp;
    char buf[BUFSZ];

    /* some wands have special effects other than normal bhitpile */
    /* drawbridge might change <u.ux,u.uy> */
    x = xx = u.ux;      /* <x,y> is zap location */
    y = yy = u.uy;      /* <xx,yy> is drawbridge (portcullis) position */
    ttmp = t_at(level, x, y);   /* trap if there is one */

    switch (obj->otyp) {
    case WAN_PROBING:
        ptmp = 0;
        if (dz < 0) {
            pline("C{N=%s,V{V{probe},D{e,E{towards},N=%s}}}.", you,
                  ceiling(x, y));
        } else {
            ptmp += bhitpile(obj, bhito, x, y);
            pline("C{N=%s,V{V{probe},D{e,E{beneath},N=%s}}}.", you,
                  surface(x, y));
            ptmp += display_binventory(x, y, TRUE);
        }
        if (!ptmp)
            pline("C{N=%s,V{V{reveal},N{o,nothing}}}.");
        return TRUE;    /* we've done our own bhitpile */
    case WAN_OPENING:
    case SPE_KNOCK:
        /* up or down, but at closed portcullis only */
        if (is_db_wall(x, y) && find_drawbridge(&xx, &yy)) {
            open_drawbridge(xx, yy);
            disclose = TRUE;
        } else if (dz > 0 && (x == level->dnstair.sx && y == level->dnstair.sy)
                   &&
                   /* can't use the stairs down to quest level 2 until leader
                      "unlocks" them; give feedback if you try */
                   on_level(&u.uz, &qstart_level) && !ok_to_quest()) {
            pline("C{N{*,N{stairs}},V{V{V{seem},V{ripple}},V{momentarily}}}.");
            disclose = TRUE;
        }
        break;
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        striking = TRUE;
     /*FALLTHRU*/ case WAN_LOCKING:
    case SPE_WIZARD_LOCK:
        /* down at open bridge or up or down at open portcullis */
        if ((level->locations[x][y].typ == DRAWBRIDGE_DOWN) ? (dz > 0)
            : (is_drawbridge_wall(x, y) && !is_db_wall(x, y)) &&
            find_drawbridge(&xx, &yy)) {
            if (!striking)
                close_drawbridge(xx, yy);
            else
                destroy_drawbridge(xx, yy);
            disclose = TRUE;
        } else if (striking && dz < 0 && rn2(3) && !Is_airlevel(&u.uz) &&
                   !Is_waterlevel(&u.uz) && !Underwater && !Is_qstart(&u.uz)) {
            /* similar to zap_dig() */
            /* TODO: Add uniquifier to the rock. */
            pline("C{+,C{s,V{V{V{dislodge},D{e,E{from},N=%s}},N{i,rock}}},"
                  "C{N{i,rock},V{V{fall on},N{o,N=%s,N=%s}}}}.", ceiling(x, y),
                  body_party(HEAD), you);
            losehp(rnd((uarmh && is_metallic(uarmh)) ? 2 : 6),
                   "N{falling rock}", KILLED_BY_AN);
            if ((otmp = mksobj_at(ROCK, level, x, y, FALSE, FALSE)) != 0) {
                xname(otmp);    /* set dknown, maybe bknown */
                stackobj(otmp);
            }
            newsym(x, y);
        } else if (!striking && ttmp && ttmp->ttyp == TRAPDOOR && dz > 0) {
            if (!Blind) {
                if (ttmp->tseen) {
                    /* TODO: "then" instead of "and" */
                    pline("C{N{N{i,trap door},A{E{beneath},N=%s}},"
                          "V{+,V{close up},V{vanish}}}.");
                    disclose = TRUE;
                } else {
                    pline("C{N=%s,V{V{see},N{N{l,N{swirl},N=%s},"
                          "A{E{beneath},N=%s}}}}.", you, is_ice(level, x,
                                                                y) ? "N{frost}"
                          : "N{dust}", you);
                }
            } else {
                /* TODO: "followed by" instead of "and" */
                You_hear("N{+,N{i,twang},N{i,thud}}");
            }
            deltrap(level, ttmp);
            ttmp = NULL;
            newsym(x, y);
        }
        break;
    case SPE_STONE_TO_FLESH:
        if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) || Underwater ||
            (Is_qstart(&u.uz) && dz < 0)) {
            pline("C{N{o,nothing},V{happen}}.");
        } else if (dz < 0) {    /* we should do more... */
            pline("C{N{o,blood},V{V{drip on},N=%s}}.", body_part(FACE));
        } else if (dz > 0 && !OBJ_AT(u.ux, u.uy)) {
            /* 
               Print this message only if there wasn't an engraving affected
               here.  If water or ice, act like waterlevel case. */
            e = engr_at(level, u.ux, u.uy);
            if (!(e && e->engr_type == ENGRAVE)) {
                if (is_pool(level, u.ux, u.uy) || is_ice(level, u.ux, u.uy))
                    pline("C{N{o,nothing},V{happen}}.");
                else
                    /* TODO: Fix body part plural problem */
                    pline("C{N{o,blood},V{V=%s,D{e,E=%s,N{o,N=%s,N=%s}}}}.",
                          is_lava(level, u.ux, u.uy) ? "V{boil}" : "V{pool}",
                          Levitation ? "E{beneath}" : "E{at}", body_part(FOOT),
                          you);
            }
        }
        break;
    default:
        break;
    }

    if (dz > 0) {
        /* zapping downward */
        bhitpile(obj, bhito, x, y);

        /* subset of engraving effects; none sets `disclose' */
        if ((e = engr_at(level, x, y)) != 0 && e->engr_type != HEADSTONE) {
            switch (obj->otyp) {
            case WAN_POLYMORPH:
            case SPE_POLYMORPH:
                del_engr(e, level);
                make_engr_at(level, x, y, random_engraving(buf), moves,
                             (xchar) 0);
                break;
            case WAN_CANCELLATION:
            case SPE_CANCELLATION:
            case WAN_MAKE_INVISIBLE:
                del_engr(e, level);
                break;
            case WAN_TELEPORTATION:
            case SPE_TELEPORT_AWAY:
                rloc_engr(e);
                break;
            case SPE_STONE_TO_FLESH:
                if (e->engr_type == ENGRAVE) {
                    /* only affects things in stone */
                    pline(Hallucination ?
                          "C{N{floor},V{V{run},D{E{like},N{o,butter}}}}." :
                          "C{N{N{*,N{edge}},D{E{on},N{floor}}},"
                          "V{V{get},A{smoother}}}.");
                    wipe_engr_at(level, x, y, dice(2, 4));
                }
                break;
            case WAN_STRIKING:
            case SPE_FORCE_BOLT:
                wipe_engr_at(level, x, y, dice(2, 4));
                break;
            default:
                break;
            }
        }
    }

    return disclose;
}


/* called for various wand and spell effects - M. Stephenson */
void
weffects(struct obj *obj, schar dx, schar dy, schar dz)
{
    int otyp = obj->otyp;
    boolean disclose = FALSE, was_unkn = !objects[otyp].oc_name_known;

    exercise(A_WIS, TRUE);
    if (u.usteed && (objects[otyp].oc_dir != NODIR) && !dx && !dy && (dz > 0) &&
        zap_steed(obj)) {
        disclose = TRUE;
    } else if (objects[otyp].oc_dir == IMMEDIATE) {
        obj_zapped = FALSE;

        if (u.uswallow) {
            bhitm(u.ustuck, obj);
            /* [how about `bhitpile(u.ustuck->minvent)' effect?] */
        } else if (dz) {
            disclose = zap_updown(obj, dz);
        } else {
            beam_hit(dx, dy, rn1(8, 6), ZAPPED_WAND, bhitm, bhito, obj, NULL);
        }
        /* give a clue if obj_zapped */
        if (obj_zapped)
            pline("C{N=%s,V{V{feel},N{N{*,N{o,vibration}},A{shuddering}}}}.");

    } else if (objects[otyp].oc_dir == NODIR) {
        zapnodir(obj);

    } else {
        /* neither immediate nor directionless */

        if (otyp == WAN_DIGGING || otyp == SPE_DIG)
            zap_dig(dx, dy, dz);
        else if (otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_FINGER_OF_DEATH)
            buzz(otyp - SPE_MAGIC_MISSILE + 10, u.ulevel / 2 + 1, u.ux, u.uy,
                 dx, dy);
        else if (otyp >= WAN_MAGIC_MISSILE && otyp <= WAN_LIGHTNING)
            buzz(otyp - WAN_MAGIC_MISSILE, (otyp == WAN_MAGIC_MISSILE) ? 2 : 6,
                 u.ux, u.uy, dx, dy);
        else
            impossible("S{weffects: unexpected spell or wand}");
        disclose = TRUE;
    }
    if (disclose && was_unkn) {
        makeknown(otyp);
        more_experienced(0, 10);
    }
    return;
}


/*
 * Generate the to damage bonus for a spell. Based on the hero's intelligence
 */
int
spell_damage_bonus(void)
{
    int tmp, intell = ACURR(A_INT);

    /* Punish low intellegence before low level else low intellegence gets
       punished only when high level */
    if (intell < 10)
        tmp = -3;
    else if (u.ulevel < 5)
        tmp = 0;
    else if (intell < 14)
        tmp = 0;
    else if (intell <= 18)
        tmp = 1;
    else        /* helm of brilliance */
        tmp = 2;

    return tmp;
}

/*
 * Generate the to hit bonus for a spell.  Based on the hero's skill in
 * spell class and dexterity.
 */
static int
spell_hit_bonus(int skill)
{
    int hit_bon = 0;
    int dex = ACURR(A_DEX);

    switch (P_SKILL(spell_skilltype(skill))) {
    case P_ISRESTRICTED:
    case P_UNSKILLED:
        hit_bon = -4;
        break;
    case P_BASIC:
        hit_bon = 0;
        break;
    case P_SKILLED:
        hit_bon = 2;
        break;
    case P_EXPERT:
        hit_bon = 3;
        break;
    }

    if (dex < 4)
        hit_bon -= 3;
    else if (dex < 6)
        hit_bon -= 2;
    else if (dex < 8)
        hit_bon -= 1;
    else if (dex < 14)
        hit_bon -= 0;   /* Will change when print stuff below removed */
    else
        hit_bon += dex - 14;    /* Even increment for dextrous heroes (see
                                   weapon.c abon) */

    return hit_bon;
}

int
exclam(int force)
{
    /* force == 0 occurs e.g. with sleep ray */
    /* note that large force is usual with wands so that !! would require
       information about hand/weapon/wand */
    return (const char *)((force < 0) ? '?' : (force <= 4) ? '.' : '!');
}

void
hit(const char *str, struct monst *mtmp, int force)
{       /* usually either "." or "!" */
    if ((!cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp) &&
         !(u.uswallow && mtmp == u.ustuck))
        || !flags.verbose)
        pline("C{N=%s,V{V{hit},?{}}}.", str);
    else
        pline("C{N=%s,V{V{hit},N=%s}}%c", str, mon_nam(mtmp), force);
}

void
miss(const char *str, struct monst *mtmp)
{
    pline("C{N=%s,V{V{miss},N=%s}}.", str,
          ((cansee(bhitpos.x, bhitpos.y) || canspotmon(mtmp))
           && flags.verbose) ? mon_nam(mtmp) : "?{}");
}


/*
 *  Called for the following distance effects:
 *      when a weapon is thrown (weapon == THROWN_WEAPON)
 *      when an object is kicked (KICKED_WEAPON)
 *      when an IMMEDIATE wand is zapped (ZAPPED_WAND)
 *      when a light beam is flashed (FLASHED_LIGHT)
 *      when a mirror is applied (INVIS_BEAM)
 *  A thrown/kicked object falls down at the end of its range or when a monster
 *  is hit.  The variable 'bhitpos' is set to the final position of the weapon
 *  thrown/zapped.  The ray of a wand may affect (by calling a provided
 *  function) several objects and monsters on its path.  The return value
 *  is the monster hit (weapon != ZAPPED_WAND), or a null monster pointer.
 *
 *  Check !u.uswallow before calling beam_hit().
 *  This function reveals the absence of a remembered invisible monster in
 *  necessary cases (throwing or kicking weapons).  The presence of a real
 *  one is revealed for a weapon, but if not a weapon is left up to fhitm().
 */
struct monst *
beam_hit(int ddx, int ddy, int range,   /* direction and range */
         int weapon,    /* see values in hack.h */
         /* fns called when mon/obj hit */
         int (*fhitm) (struct monst *, struct obj *), int (*fhito) (struct obj *, struct obj *), struct obj *obj,       /* object 
                                                                                                                           tossed/used 
                                                                                                                         */
         boolean * obj_destroyed        /* has object been deallocated? may be
                                           NULL */
    )
{
    struct monst *mtmp;
    struct tmp_sym *tsym;
    uchar typ;
    boolean shopdoor = FALSE, point_blank = TRUE;

    if (obj_destroyed)
        *obj_destroyed = FALSE;

    if (weapon == KICKED_WEAPON) {
        /* object starts one square in front of player */
        bhitpos.x = u.ux + ddx;
        bhitpos.y = u.uy + ddy;
        range--;
    } else {
        bhitpos.x = u.ux;
        bhitpos.y = u.uy;
    }

    if (weapon == FLASHED_LIGHT)
        tsym = tmpsym_init(DISP_BEAM, dbuf_effect(E_MISC, E_flashbeam));
    else if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM)
        tsym = tmpsym_initobj(obj);

    while (range-- > 0) {
        int x, y;

        bhitpos.x += ddx;
        bhitpos.y += ddy;
        x = bhitpos.x;
        y = bhitpos.y;

        if (!isok(x, y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        if (is_pick(obj) && inside_shop(level, x, y) &&
            (mtmp = shkcatch(obj, x, y))) {
            tmpsym_end(tsym);
            return mtmp;
        }

        typ = level->locations[bhitpos.x][bhitpos.y].typ;

        /* iron bars will block anything big enough */
        if ((weapon == THROWN_WEAPON || weapon == KICKED_WEAPON) &&
            typ == IRONBARS &&
            hits_bars(&obj, x - ddx, y - ddy, point_blank ? 0 : !rn2(5), 1)) {
            /* caveat: obj might now be null... */
            if (obj == NULL && obj_destroyed)
                *obj_destroyed = TRUE;
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }

        if (weapon == ZAPPED_WAND && find_drawbridge(&x, &y))
            switch (obj->otyp) {
            case WAN_OPENING:
            case SPE_KNOCK:
                if (is_db_wall(bhitpos.x, bhitpos.y)) {
                    if (cansee(x, y) || cansee(bhitpos.x, bhitpos.y))
                        makeknown(obj->otyp);
                    open_drawbridge(x, y);
                }
                break;
            case WAN_LOCKING:
            case SPE_WIZARD_LOCK:
                if ((cansee(x, y) || cansee(bhitpos.x, bhitpos.y))
                    && level->locations[x][y].typ == DRAWBRIDGE_DOWN)
                    makeknown(obj->otyp);
                close_drawbridge(x, y);
                break;
            case WAN_STRIKING:
            case SPE_FORCE_BOLT:
                if (typ != DRAWBRIDGE_UP)
                    destroy_drawbridge(x, y);
                makeknown(obj->otyp);
                break;
            }

        if (fhito) {
            if (bhitpile(obj, fhito, bhitpos.x, bhitpos.y))
                range--;
        }
        if ((mtmp = m_at(level, bhitpos.x, bhitpos.y)) != 0) {
            notonhead = (bhitpos.x != mtmp->mx || bhitpos.y != mtmp->my);
            if (weapon != FLASHED_LIGHT) {
                if (weapon != ZAPPED_WAND) {
                    if (weapon != INVIS_BEAM)
                        tmpsym_end(tsym);
                    if (cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp)) {
                        if (weapon != INVIS_BEAM) {
                            map_invisible(bhitpos.x, bhitpos.y);
                            return mtmp;
                        }
                    } else
                        return mtmp;
                }
                if (weapon != INVIS_BEAM) {
                    (*fhitm) (mtmp, obj);
                    range -= 3;
                }
            } else {
                /* FLASHED_LIGHT hitting invisible monster should pass through
                   instead of stop so we call flash_hits_mon() directly rather
                   than returning mtmp back to caller. That allows the flash to 
                   keep on going. Note that we use mtmp->minvis not
                   canspotmon() because it makes no difference whether the hero 
                   can see the monster or not. */
                if (mtmp->minvis) {
                    obj->ox = u.ux, obj->oy = u.uy;
                    flash_hits_mon(mtmp, obj);
                } else {
                    tmpsym_end(tsym);
                    return mtmp;        /* caller will call flash_hits_mon */
                }
            }
        } else {
            if (weapon == ZAPPED_WAND && obj->otyp == WAN_PROBING &&
                level->locations[bhitpos.x][bhitpos.y].mem_invis) {
                unmap_object(bhitpos.x, bhitpos.y);
                newsym(x, y);
            }
        }
        if (!fhito) {
            if (weapon == KICKED_WEAPON &&
                ((obj->oclass == COIN_CLASS && OBJ_AT(bhitpos.x, bhitpos.y)) ||
                 ship_object(obj, bhitpos.x, bhitpos.y,
                             costly_spot(bhitpos.x, bhitpos.y)))) {
                tmpsym_end(tsym);
                return NULL;
            }
        }
        if (weapon == ZAPPED_WAND && (IS_DOOR(typ) || typ == SDOOR)) {
            switch (obj->otyp) {
            case WAN_OPENING:
            case WAN_LOCKING:
            case WAN_STRIKING:
            case SPE_KNOCK:
            case SPE_WIZARD_LOCK:
            case SPE_FORCE_BOLT:
                if (doorlock(obj, bhitpos.x, bhitpos.y)) {
                    if (cansee(bhitpos.x, bhitpos.y) ||
                        (obj->otyp == WAN_STRIKING))
                        makeknown(obj->otyp);
                    if (level->locations[bhitpos.x][bhitpos.y].doormask ==
                        D_BROKEN &&
                        *in_rooms(level, bhitpos.x, bhitpos.y, SHOPBASE)) {
                        shopdoor = TRUE;
                        add_damage(bhitpos.x, bhitpos.y, 400L);
                    }
                }
                break;
            }
        }
        if (!ZAP_POS(typ) || closed_door(level, bhitpos.x, bhitpos.y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }
        if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM) {
            /* 'I' present but no monster: erase */
            /* do this before the tmpsym_at() */
            if (level->locations[bhitpos.x][bhitpos.y].mem_invis &&
                cansee(x, y)) {
                unmap_object(bhitpos.x, bhitpos.y);
                newsym(x, y);
            }
            tmpsym_at(tsym, bhitpos.x, bhitpos.y);
            win_delay_output();
            /* kicked objects fall in pools */
            if ((weapon == KICKED_WEAPON) &&
                (is_pool(level, bhitpos.x, bhitpos.y) ||
                 is_lava(level, bhitpos.x, bhitpos.y)))
                break;
            if (IS_SINK(typ) && weapon != FLASHED_LIGHT)
                break;  /* physical objects fall onto sink */
        }
        /* limit range of ball so hero won't make an invalid move */
        if (weapon == THROWN_WEAPON && range > 0 &&
            obj->otyp == HEAVY_IRON_BALL) {
            struct obj *bobj;
            struct trap *t;

            if ((bobj = sobj_at(BOULDER, level, x, y)) != 0) {
                if (cansee(x, y))
                    pline("C{N=%s,V{V{hit},N=%s}}.", distant_name(obj, xname),
                          xname(bobj));
                range = 0;
            } else if (obj == uball) {
                if (!test_move(x - ddx, y - ddy, ddx, ddy, 0, TEST_MOVE)) {
                    /* nb: it didn't hit anything directly */
                    if (cansee(x, y))
                        pline("C{N=%s,V{V{jerk},D{E{to},"
                              "N{N{i,halt},V{abrupt}}}}}.",
                              distant_name(obj, xname));
                    range = 0;
                } else if (In_sokoban(&u.uz) && (t = t_at(level, x, y)) != 0 &&
                           (t->ttyp == PIT || t->ttyp == SPIKED_PIT ||
                            t->ttyp == HOLE || t->ttyp == TRAPDOOR)) {
                    /* hero falls into the trap, so ball stops */
                    range = 0;
                }
            }
        }

        /* thrown/kicked missile has moved away from its starting spot */
        point_blank = FALSE;    /* affects passing through iron bars */
    }

    if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM)
        tmpsym_end(tsym);

    if (shopdoor)
        pay_for_damage("V{destroy}", FALSE);

    return NULL;
}

struct monst *
boomhit(int dx, int dy)
{
    int i, ct;
    int boom = E_boomleft;      /* showsym[] index */
    struct monst *mtmp;
    struct tmp_sym *tsym;

    bhitpos.x = u.ux;
    bhitpos.y = u.uy;

    for (i = 0; i < 8; i++)
        if (xdir[i] == dx && ydir[i] == dy)
            break;
    tsym = tmpsym_init(DISP_FLASH, dbuf_effect(E_MISC, boom));
    for (ct = 0; ct < 10; ct++) {
        if (i == 8)
            i = 0;
        boom = (boom == E_boomleft) ? E_boomright : E_boomleft;
        tmpsym_change(tsym, dbuf_effect(E_MISC, boom)); /* change glyph */
        dx = xdir[i];
        dy = ydir[i];
        bhitpos.x += dx;
        bhitpos.y += dy;
        if (MON_AT(level, bhitpos.x, bhitpos.y)) {
            mtmp = m_at(level, bhitpos.x, bhitpos.y);
            m_respond(mtmp);
            tmpsym_end(tsym);
            return mtmp;
        }
        if (!ZAP_POS(level->locations[bhitpos.x][bhitpos.y].typ) ||
            closed_door(level, bhitpos.x, bhitpos.y)) {
            bhitpos.x -= dx;
            bhitpos.y -= dy;
            break;
        }
        if (bhitpos.x == u.ux && bhitpos.y == u.uy) {   /* ct == 9 */
            if (Fumbling || rn2(20) >= ACURR(A_DEX)) {
                /* we hit ourselves */
                thitu(10, rnd(10), NULL, "N{boomerang}");
                break;
            } else {    /* we catch it */
                tmpsym_end(tsym);
                pline("C{N=%s,V{V{V{catch},D{skillfully}},N{boomerang}}}.",
                      you);
                return &youmonst;
            }
        }
        tmpsym_at(tsym, bhitpos.x, bhitpos.y);
        win_delay_output();
        if (ct % 5 != 0)
            i++;
        if (IS_SINK(level->locations[bhitpos.x][bhitpos.y].typ))
            break;      /* boomerang falls on sink */
    }
    tmpsym_end(tsym); /* do not leave last symbol */
    return NULL;
}

/* returns damage to mon */
static int
zap_hit_mon(struct monst *mon, int type, int nd, struct obj **ootmp)
{       /* to return worn armor for caller to disintegrate */
    int tmp = 0;
    int abstype = abs(type) % 10;
    boolean sho_shieldeff = FALSE;
    boolean spellcaster = is_hero_spell(type);  /* maybe get a bonus! */

    *ootmp = NULL;
    switch (abstype) {
    case ZT_MAGIC_MISSILE:
        if (resists_magm(mon)) {
            sho_shieldeff = TRUE;
            break;
        }
        tmp = dice(nd, 6);
        if (spellcaster)
            tmp += spell_damage_bonus();
        break;
    case ZT_FIRE:
        if (resists_fire(mon)) {
            sho_shieldeff = TRUE;
            break;
        }
        tmp = dice(nd, 6);
        if (resists_cold(mon))
            tmp += 7;
        if (spellcaster)
            tmp += spell_damage_bonus();

        if (burnarmor(mon)) {
            if (!rn2(3))
                destroy_mitem(mon, POTION_CLASS, AD_FIRE);
            if (!rn2(3))
                destroy_mitem(mon, SCROLL_CLASS, AD_FIRE);
            if (!rn2(5))
                destroy_mitem(mon, SPBOOK_CLASS, AD_FIRE);
        }
        break;
    case ZT_COLD:
        if (resists_cold(mon)) {
            sho_shieldeff = TRUE;
            break;
        }
        tmp = dice(nd, 6);
        if (resists_fire(mon))
            tmp += dice(nd, 3);
        if (spellcaster)
            tmp += spell_damage_bonus();
        if (!rn2(3))
            destroy_mitem(mon, POTION_CLASS, AD_COLD);
        break;
    case ZT_SLEEP:
        tmp = 0;
        sleep_monst(mon, dice(nd, 25),
                    type == ZT_WAND(ZT_SLEEP) ? WAND_CLASS : '\0');
        break;
    case ZT_DEATH:     /* death/disintegration */
        if (abs(type) != ZT_BREATH(ZT_DEATH)) { /* death */
            if (mon->data == &mons[PM_DEATH]) {
                mon->mhpmax += mon->mhpmax / 2;
                if (mon->mhpmax >= MAGIC_COOKIE)
                    mon->mhpmax = MAGIC_COOKIE - 1;
                mon->mhp = mon->mhpmax;
                tmp = 0;
                break;
            }
            if (nonliving(mon->data) || is_demon(mon->data) || resists_magm(mon)) {     /* similar 
                                                                                           to 
                                                                                           player 
                                                                                         */
                sho_shieldeff = TRUE;
                break;
            }
            type = -1;  /* so they don't get saving throws */
        } else {
            struct obj *otmp2;

            if (resists_disint(mon)) {
                sho_shieldeff = TRUE;
            } else if (mon->misc_worn_check & W_ARMS) {
                /* destroy shield; victim survives */
                *ootmp = which_armor(mon, W_ARMS);
            } else if (mon->misc_worn_check & W_ARM) {
                /* destroy body armor, also cloak if present */
                *ootmp = which_armor(mon, W_ARM);
                if ((otmp2 = which_armor(mon, W_ARMC)) != 0)
                    m_useup(mon, otmp2);
            } else {
                /* no body armor, victim dies; destroy cloak and shirt now in
                   case target gets life-saved */
                tmp = MAGIC_COOKIE;
                if ((otmp2 = which_armor(mon, W_ARMC)) != 0)
                    m_useup(mon, otmp2);
                if ((otmp2 = which_armor(mon, W_ARMU)) != 0)
                    m_useup(mon, otmp2);
            }
            type = -1;  /* no saving throw wanted */
            break;      /* not ordinary damage */
        }
        tmp = mon->mhp + 1;
        break;
    case ZT_LIGHTNING:
        if (resists_elec(mon)) {
            sho_shieldeff = TRUE;
            tmp = 0;
            /* can still blind the monster */
        } else
            tmp = dice(nd, 6);
        if (spellcaster)
            tmp += spell_damage_bonus();

        if (!resists_blnd(mon) && !(type > 0 && u.uswallow && mon == u.ustuck)) {
            unsigned rnd_tmp = rnd(50);

            mon->mcansee = 0;
            if ((mon->mblinded + rnd_tmp) > 127)
                mon->mblinded = 127;
            else
                mon->mblinded += rnd_tmp;
        }
        if (!rn2(3))
            destroy_mitem(mon, WAND_CLASS, AD_ELEC);
        /* not actually possible yet */
        if (!rn2(3))
            destroy_mitem(mon, RING_CLASS, AD_ELEC);
        break;
    case ZT_POISON_GAS:
        if (resists_poison(mon)) {
            sho_shieldeff = TRUE;
            break;
        }
        tmp = dice(nd, 6);
        break;
    case ZT_ACID:
        if (resists_acid(mon)) {
            sho_shieldeff = TRUE;
            break;
        }
        tmp = dice(nd, 6);
        if (!rn2(6))
            erode_obj(MON_WEP(mon), TRUE, TRUE);
        if (!rn2(6))
            erode_armor(mon, TRUE);
        break;
    }
    if (sho_shieldeff)
        shieldeff(mon->mx, mon->my);
    if (is_hero_spell(type) && (Role_if(PM_KNIGHT) && u.uhave.questart))
        tmp *= 2;
    if (tmp > 0 && type >= 0 &&
        resist(mon, type < ZT_SPELL(0) ? WAND_CLASS : '\0', 0, NOTELL))
        tmp /= 2;
    if (tmp < 0)
        tmp = 0;        /* don't allow negative damage */
    mon->mhp -= tmp;
    return tmp;
}

static void
zap_hit_u(int type, int nd, const char *fltxt, xchar sx, xchar sy)
{
    int dam = 0;

    switch (abs(type) % 10) {
    case ZT_MAGIC_MISSILE:
        if (Antimagic) {
            shieldeff(sx, sy);
            /* TODO: Stupid adverbials */
            pline("C{N{*,N{missile}},V{bounce off}}!");
        } else {
            dam = dice(nd, 6);
            exercise(A_STR, FALSE);
        }
        break;
    case ZT_FIRE:
        if (Fire_resistance) {
            shieldeff(sx, sy);
            pline("C{N=%s,V{V{-,V{feel}},A{hot}}}!", you);
            ugolemeffects(AD_FIRE, dice(nd, 6));
        } else {
            dam = dice(nd, 6);
        }
        burn_away_slime();
        if (burnarmor(&youmonst)) {     /* "body hit" */
            if (!rn2(3))
                destroy_item(POTION_CLASS, AD_FIRE);
            if (!rn2(3))
                destroy_item(SCROLL_CLASS, AD_FIRE);
            if (!rn2(5))
                destroy_item(SPBOOK_CLASS, AD_FIRE);
        }
        break;
    case ZT_COLD:
        if (Cold_resistance) {
            shieldeff(sx, sy);
            pline("C{N=%s,V{V{-,V{feel}},A{cold}}}!", you);
            ugolemeffects(AD_COLD, dice(nd, 6));
        } else {
            dam = dice(nd, 6);
        }
        if (!rn2(3))
            destroy_item(POTION_CLASS, AD_COLD);
        break;
    case ZT_SLEEP:
        if (Sleep_resistance) {
            shieldeff(u.ux, u.uy);
            pline("C{N=%s,V{V{-,V{feel}},A{sleepy}}}!", you);
        } else {
            fall_asleep(-dice(nd, 25), TRUE);   /* sleep ray */
        }
        break;
    case ZT_DEATH:
        if (abs(type) == ZT_BREATH(ZT_DEATH)) {
            if (Disint_resistance) {
                pline("C{s,V{V{-,V{disintegrate}},N=%s}}.", you);
                break;
            } else if (uarms) {
                /* destroy shield; other possessions are safe */
                destroy_arm(uarms);
                break;
            } else if (uarm) {
                /* destroy suit; if present, cloak goes too */
                if (uarmc)
                    destroy_arm(uarmc);
                destroy_arm(uarm);
                break;
            }
            /* no shield or suit, you're dead; wipe out cloak and/or shirt in
               case of life-saving or bones */
            if (uarmc)
                destroy_arm(uarmc);
            if (uarmu)
                destroy_arm(uarmu);
        } else if (nonliving(youmonst.data) || is_demon(youmonst.data)) {
            shieldeff(sx, sy);
            pline("C{N=%s,V{V{seem},A{unaffected}}}.", you);
            break;
        } else if (Antimagic) {
            shieldeff(sx, sy);
            pline("C{s,V{V{-,V{affect}},N=%s}}.", you);
            break;
        }
        killer_format = KILLED_BY_AN;
        killer = fltxt;
        /* when killed by disintegration breath, don't leave corpse */
        u.ugrave_arise = (type == -ZT_BREATH(ZT_DEATH)) ? -3 : NON_PM;
        done(DIED);
        return; /* lifesaved */
    case ZT_LIGHTNING:
        if (Shock_resistance) {
            shieldeff(sx, sy);
            pline("C{s,V{V{-,V{affect}},N=%s}}.", you);
            ugolemeffects(AD_ELEC, dice(nd, 6));
        } else {
            dam = dice(nd, 6);
            exercise(A_CON, FALSE);
        }
        if (!rn2(3))
            destroy_item(WAND_CLASS, AD_ELEC);
        if (!rn2(3))
            destroy_item(RING_CLASS, AD_ELEC);
        break;
    case ZT_POISON_GAS:
        poisoned("N{blast}", A_DEX, "N{N{blast},A{poisoned}}", 15);
        break;
    case ZT_ACID:
        if (Acid_resistance) {
            dam = 0;
        } else {
            pline("C{N{acid},V{burn}}!");
            dam = dice(nd, 6);
            exercise(A_STR, FALSE);
        }
        /* using two weapons at once makes both of them more vulnerable */
        if (!rn2(u.twoweap ? 3 : 6))
            erode_obj(uwep, TRUE, TRUE);
        if (u.twoweap && !rn2(3))
            erode_obj(uswapwep, TRUE, TRUE);
        if (!rn2(6))
            erode_armor(&youmonst, TRUE);
        break;
    }

    if (Half_spell_damage && dam && type < 0 && (type > -20 || type < -29))
        /* !Breath */
        dam = (dam + 1) / 2;
    losehp(dam, fltxt, KILLED_BY_AN);
    return;
}


/*
 * burn scrolls and spellbooks on floor at position x,y
 * return the number of scrolls and spellbooks burned
 */
int
burn_floor_paper(struct level *lev, int x, int y, boolean give_feedback,        /* caller 
                                                                                   needs 
                                                                                   to 
                                                                                   decide
                                                                                   about
                                                                                   visibility 
                                                                                   checks */
                 boolean u_caused)
{
    struct obj *obj, *obj2;
    long i, scrquan, delquan;
    char buf[BUFSZ];
    int cnt = 0;

    for (obj = lev->objects[x][y]; obj; obj = obj2) {
        obj2 = obj->nexthere;
        if (obj->oclass == SCROLL_CLASS || obj->oclass == SPBOOK_CLASS) {
            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL ||
                obj_resists(obj, 2, 100))
                continue;
            scrquan = obj->quan;        /* number present */
            delquan = 0;        /* number to destroy */
            for (i = scrquan; i > 0; i--)
                if (!rn2(3))
                    delquan++;
            if (delquan) {
                /* save name before potential delobj() */
                if (give_feedback) {
                    strcpy(buf,
                           (x == u.ux &&
                            y == u.uy) ? xname(obj) : distant_name(obj, xname));
                }
                /* useupf(), which charges, only if hero caused damage */
                if (u_caused)
                    useupf(obj, delquan);
                else if (delquan < scrquan)
                    obj->quan -= delquan;
                else
                    delobj(obj);
                cnt += delquan;
                if (give_feedback) {
                    pline("C{N{%d,N=%s},V{burn}}.", delquan, buf);
                }
            }
        }
    }
    return cnt;
}

/* will zap/spell/breath attack score a hit against armor class `ac'? */
static int
zap_hit_check(int ac, int type)
{       /* either hero cast spell type or 0 */
    int chance = rn2(20);
    int spell_bonus = type ? spell_hit_bonus(type) : 0;

    /* small chance for naked target to avoid being hit */
    if (!chance)
        return rnd(10) < ac + spell_bonus;

    /* very high armor protection does not achieve invulnerability */
    ac = AC_VALUE(ac);

    return (3 - chance) < ac + spell_bonus;
}

/* type ==   0 to   9 : you shooting a wand */
/* type ==  10 to  19 : you casting a spell */
/* type ==  20 to  29 : you breathing as a monster */
/* type == -10 to -19 : monster casting spell */
/* type == -20 to -29 : monster breathing at you */
/* type == -30 to -39 : monster shooting a wand */
/* called with dx = dy = 0 with vertical bolts */
void
buzz(int type, int nd, xchar sx, xchar sy, int dx, int dy)
{
    int range, abstype = abs(type) % 10;
    struct rm *loc;
    xchar lsx, lsy;
    struct monst *mon;
    coord save_bhitpos;
    boolean shopdamage = FALSE;
    const char *fltxt;
    struct obj *otmp;
    struct tmp_sym *tsym;
    int spell_type;

    /* if its a Hero Spell then get its SPE_TYPE */
    spell_type = is_hero_spell(type) ? SPE_MAGIC_MISSILE + abstype : 0;

    /* TODO: Uniquify the bolt */
    fltxt = flash_types[(type <= -30) ? abstype : abs(type)];
    if (u.uswallow) {
        int tmp;

        if (type < 0)
            return;
        tmp = zap_hit_mon(u.ustuck, type, nd, &otmp);
        if (!u.ustuck)
            u.uswallow = 0;
        else
            pline("C{N=%s,V{V{rip into},N=%s}}%c", exclam(tmp));
        /* Using disintegration from the inside only makes a hole... */
        if (tmp == MAGIC_COOKIE)
            u.ustuck->mhp = 0;
        if (u.ustuck->mhp < 1)
            killed(u.ustuck);
        return;
    }
    if (type < 0)
        newsym(u.ux, u.uy);
    range = rn1(7, 7);
    if (dx == 0 && dy == 0)
        range = 1;
    save_bhitpos = bhitpos;

    tsym = tmpsym_init(DISP_BEAM, zapdir_to_effect(dx, dy, abstype));
    while (range-- > 0) {
        lsx = sx;
        sx += dx;
        lsy = sy;
        sy += dy;
        if (isok(sx, sy) && (loc = &level->locations[sx][sy])->typ) {
            mon = m_at(level, sx, sy);
            if (cansee(sx, sy)) {
                /* reveal/unreveal invisible monsters before tmpsym_at() */
                if (mon && !canspotmon(mon))
                    map_invisible(sx, sy);
                else if (!mon && level->locations[sx][sy].mem_invis) {
                    unmap_object(sx, sy);
                    newsym(sx, sy);
                }
                if (ZAP_POS(loc->typ) || cansee(lsx, lsy))
                    tmpsym_at(tsym, sx, sy);
                win_delay_output();     /* wait a little */
            }
        } else
            goto make_bounce;

        /* hit() and miss() need bhitpos to match the target */
        bhitpos.x = sx, bhitpos.y = sy;
        /* Fireballs only damage when they explode */
        if (type != ZT_SPELL(ZT_FIRE))
            range += zap_over_floor(sx, sy, type, &shopdamage);

        if (mon) {
            if (type == ZT_SPELL(ZT_FIRE))
                break;
            if (type >= 0)
                mon->mstrategy &= ~STRAT_WAITMASK;
        buzzmonst:
            if (zap_hit_check(find_mac(mon), spell_type)) {
                if (const char *what = mon_reflects(mon)) {
                    if (cansee(mon->mx, mon->my)) {
                        hit(fltxt, mon, exclam(0));
                        shieldeff(mon->mx, mon->my);
                        /* TODO: Interjection! */
                        pline("C{J{but},C{N=%s,V{V{reflect},"
                              "D{E{from},N=%s}}}}!", fltxt, what);
                    }
                    dx = -dx;
                    dy = -dy;
                } else {
                    boolean mon_could_move = mon->mcanmove;
                    int tmp = zap_hit_mon(mon, type, nd, &otmp);

                    if (is_rider(mon->data) && abs(type) == ZT_BREATH(ZT_DEATH)) {
                        if (canseemon(mon)) {
                            hit(fltxt, mon, '.');
                            pline("C{N=%s,V{disintegrate}}.", mon_nam(mon));
                            pline("C{N{o,N{body},N=%s},V{V{reintegrate},"
                                  "D{E{before},N{o,N=%s,N=%s}}}}!",
                                  mon_nam(mon), body_part(EYE), you);
                            pline("C{N=%s,V{resurrect}}.", mon_nam(mon));
                        }
                        mon->mhp = mon->mhpmax;
                        break;  /* Out of while loop */
                    }
                    if (mon->data == &mons[PM_DEATH] && abstype == ZT_DEATH) {
                        if (canseemon(mon)) {
                            hit(fltxt, mon, '.');
                            pline("C{N=%s,V{V{absorb},N{N=%s,A{deadly}}}}!",
                                  mon_nam(mon), type == ZT_BREATH(ZT_DEATH) ?
                                    "N{blast}" : "N{ray}");
                            pline("C{N=%s,V{V{seem},A{A{A{stronger},D{even}},"
                                  "D{E{than},D{before}}}}}.");
                        }
                        break;  /* Out of while loop */
                    }

                    if (tmp == MAGIC_COOKIE) {  /* disintegration */
                        struct obj *otmp2, *m_amulet = mlifesaver(mon);

                        if (canseemon(mon)) {
                            if (!m_amulet)
                                pline("C{s,V{V{disintegrate},N=%s}}!", mon_nam(mon));
                            else
                                hit(fltxt, mon, '!');
                        }

/* note: worn amulet of life saving must be preserved in order to operate */
#define oresist_disintegration(obj) \
            (objects[obj->otyp].oc_oprop == DISINT_RES || \
             obj_resists(obj, 5, 50) || is_quest_artifact(obj) || \
             obj == m_amulet)

                        for (otmp = mon->minvent; otmp; otmp = otmp2) {
                            otmp2 = otmp->nobj;
                            if (!oresist_disintegration(otmp)) {
                                /* update the monsters intrinsics and saddle in
                                   case it is lifesaved. */
                                if (otmp->owornmask && otmp->otyp == SADDLE)
                                    mon->misc_worn_check &= ~W_SADDLE;
                                update_mon_intrinsics(mon, otmp, FALSE, TRUE);
                                obj_extract_self(otmp);
                                obfree(otmp, NULL);
                            }
                        }

                        if (type < 0)
                            monkilled(mon, NULL, -AD_RBRE);
                        else
                            xkilled(mon, 2);
                    } else if (mon->mhp < 1) {
                        if (type < 0)
                            monkilled(mon, fltxt, AD_RBRE);
                        else
                            killed(mon);
                    } else {
                        if (!otmp) {
                            /* normal non-fatal hit */
                            hit(fltxt, mon, exclam(tmp));
                        } else {
                            /* some armor was destroyed; no damage done */
                            if (canseemon(mon))
                                pline("C{s,V{V{disintegrate},N{o,N=%s,N=%s}}}!",
                                      s_suffix(Monnam(mon)), distant_name(otmp,
                                                                          xname));
                            m_useup(mon, otmp);
                        }
                        if (mon_could_move && !mon->mcanmove)   /* ZT_SLEEP */
                            slept_monst(mon);
                    }
                }
                range -= 2;
            } else {
                miss(fltxt, mon);
            }
        } else if (sx == u.ux && sy == u.uy && range >= 0) {
            nomul(0, NULL);
            if (u.usteed && !rn2(3) && !mon_reflects(u.usteed)) {
                mon = u.usteed;
                goto buzzmonst;
            } else if (zap_hit_check((int)u.uac, 0)) {
                range -= 2;
                pline("C{N=%s,V{V{hit},N=%s}}!", fltxt, you);
                if (Reflecting) {
                    if (!Blind) {
                        if (const char *what = ureflects())
                            pline("C{J{but},C{N=%s,V{V{reflect},"
                                  "D{E{from},N=%s}}}}!", fltxt, what);
                    } else
                        /* TODO: handle determiner "some"? */
                        pline("C{C{s,V{V{-,V{affect}},N=%s}},"
                              "D{E{for},N{some reason}}}.", you);
                    dx = -dx;
                    dy = -dy;
                    shieldeff(sx, sy);
                } else {
                    zap_hit_u(type, nd, fltxt, sx, sy);
                }
            } else {
                pline("C{N=%s,V{V{whiz},D{E{by},N=%s}}}!", fltxt, you);
            }
            if (abstype == ZT_LIGHTNING && !resists_blnd(&youmonst)) {
                pline(blinded_by_the_flash);
                make_blinded((long)dice(nd, 50), FALSE);
                if (!Blind)
                    pline("C{N{o,N{vision},N=%s},V{V{clear},D{quickly}}}.",
                          you);
            }
            stop_occupation();
            nomul(0, NULL);
        }

        if (!ZAP_POS(loc->typ) || (closed_door(level, sx, sy) && (range >= 0))) {
            int bounce;
            uchar rmn;

        make_bounce:
            if (type == ZT_SPELL(ZT_FIRE)) {
                sx = lsx;
                sy = lsy;
                break;  /* fireballs explode before the wall */
            }
            bounce = 0;
            range--;
            if (r,ange && isok(lsx, lsy) && cansee(lsx, lsy))
                pline("C{N=%s,V{bounce}}!", fltxt);
            if (!dx || !dy || !rn2(20)) {
                dx = -dx;
                dy = -dy;
            } else {
                if (isok(sx, lsy) &&
                    ZAP_POS(rmn = level->locations[sx][lsy].typ) &&
                    !closed_door(level, sx, lsy) && (IS_ROOM(rmn) ||
                                                     (isok(sx + dx, lsy) &&
                                                      ZAP_POS(level->
                                                              locations[sx + dx]
                                                              [lsy].typ))))
                    bounce = 1;
                if (isok(lsx, sy) &&
                    ZAP_POS(rmn = level->locations[lsx][sy].typ) &&
                    !closed_door(level, lsx, sy) && (IS_ROOM(rmn) ||
                                                     (isok(lsx, sy + dy) &&
                                                      ZAP_POS(level->
                                                              locations[lsx][sy
                                                                             +
                                                                             dy].
                                                              typ))))
                    if (!bounce || rn2(2))
                        bounce = 2;

                switch (bounce) {
                case 0:
                    dx = -dx;   /* fall into... */
                case 1:
                    dy = -dy;
                    break;
                case 2:
                    dx = -dx;
                    break;
                }
                tmpsym_change(tsym, zapdir_to_effect(dx, dy, abstype));
            }
        }
    }
    tmpsym_end(tsym);
    if (type == ZT_SPELL(ZT_FIRE))
        explode(sx, sy, type, dice(12, 6), 0, EXPL_FIERY);
    if (shopdamage)
        pay_for_damage(abstype == ZT_FIRE ? "V{burn away}" : abstype ==
                       ZT_COLD ? "V{shatter}" : abstype ==
                       ZT_DEATH ? "V{disintegrate}" : "V{destroy}", FALSE);
    bhitpos = save_bhitpos;
}


void
melt_ice(struct level *lev, xchar x, xchar y)
{
    struct rm *loc = &lev->locations[x][y];
    struct obj *otmp;
    boolean visible = (lev == level && cansee(x, y));

    if (loc->typ == DRAWBRIDGE_UP)
        loc->drawbridgemask &= ~DB_ICE; /* revert to DB_MOAT */
    else {      /* loc->typ == ICE */
        loc->typ = (loc->icedpool == ICED_POOL ? POOL : MOAT);
        loc->icedpool = 0;
    }
    obj_ice_effects(lev, x, y, FALSE);
    unearth_objs(lev, x, y);
    if (Underwater)
        vision_recalc(1);
    newsym(x, y);
    if (visible)
        Norep("C{N{ice},V{+,V{crackle},V{melt}}}.");
    if ((otmp = sobj_at(BOULDER, level, x, y)) != 0) {
        if (visible)
            pline("C{N=%s,V{settle}}...", xname(otmp));
        do {
            obj_extract_self(otmp);     /* boulder isn't being pushed */
            if (!boulder_hits_pool(otmp, x, y, FALSE))
                impossible("S{melt_ice: no pool?}");
            /* try again if there's another boulder and pool didn't fill */
        } while (is_pool(level, x, y) &&
                 (otmp = sobj_at(BOULDER, level, x, y)) != 0);
        newsym(x, y);
    }
    if (x == u.ux && y == u.uy)
        spoteffects(TRUE);      /* possibly drown, notice objects */
}

/* Burn floor scrolls, evaporate pools, etc...  in a single square.  Used
 * both for normal bolts of fire, cold, etc... and for fireballs.
 * Sets shopdamage to TRUE if a shop door is destroyed, and returns the
 * amount by which range is reduced (the latter is just ignored by fireballs)
 */
int
zap_over_floor(xchar x, xchar y, int type, boolean * shopdamage)
{
    struct monst *mon;
    int abstype = abs(type) % 10;
    struct rm *loc = &level->locations[x][y];
    int rangemod = 0;

    if (abstype == ZT_FIRE) {
        struct trap *t = t_at(level, x, y);

        if (t && t->ttyp == WEB) {
            /* a burning web is too flimsy to notice if you can't see it */
            if (cansee(x, y))
                Norep("C{N{i,web},V{V{burst into},N{*,N{o,flame}}}}!");
            delfloortrap(level, t);
            if (cansee(x, y))
                newsym(x, y);
        }
        if (is_ice(level, x, y)) {
            melt_ice(level, x, y);
        } else if (is_pool(level, x, y)) {
            const char buf[BUFSZ];
            sprintf(buf, "C{N=%s,V{V{hear},N{N{o,gas},A{hissing}}}}.");

            if (loc->typ != POOL) {     /* MOAT or DRAWBRIDGE_UP */
                if (cansee(x, y))
                    sprintf(buf, "C{N{f,N{o,some},N{water}},V{evaporate}}.");
            } else {
                struct trap *ttmp;

                rangemod -= 3;
                loc->typ = ROOM;
                ttmp = maketrap(level, x, y, PIT);
                if (ttmp)
                    ttmp->tseen = 1;
                if (cansee(x, y))
                    sprintf(buf,"C{N{water},V{evaporate}}.");
            }
            Norep(buf);
            if (loc->typ == ROOM)
                newsym(x, y);
        } else if (IS_FOUNTAIN(loc->typ)) {
            if (cansee(x, y))
                pline("C{N{o,steam},V{V{billow},D{e,E{from},N{fountain}}}}.");
            rangemod -= 1;
            dryup(x, y, type > 0);
        }
    } else if (abstype == ZT_COLD &&
               (is_pool(level, x, y) || is_lava(level, x, y))) {
        boolean lava = is_lava(level, x, y);
        boolean moat = is_moat(level, x, y);

        if (loc->typ == WATER) {
            /* For now, don't let WATER freeze. */
            if (cansee(x, y))
                pline("C{N{water},V{V{freeze},D{d,N{i,moment}}}}.");
            else
                You_hear("N{N{i,crackling},A{soft^quiet}}");
            rangemod -= 1000;   /* stop */
        } else {
            rangemod -= 3;
            if (loc->typ == DRAWBRIDGE_UP) {
                loc->drawbridgemask &= ~DB_UNDER;       /* clear lava */
                loc->drawbridgemask |= (lava ? DB_FLOOR : DB_ICE);
            } else {
                if (!lava)
                    loc->icedpool = (loc->typ == POOL ? ICED_POOL : ICED_MOAT);
                loc->typ = (lava ? ROOM : ICE);
            }
            bury_objs(level, x, y);
            if (cansee(x, y)) {
                if (moat)
                    Norep("C{N{moat},V{V{are},A{A{bridged},"
                          "D{E{with},N{o,ice}}}}}!");
                else if (lava)
                    Norep("C{N{lava},V{+,V{cool},V{solidify}}}.");
                else
                    Norep("C{N{water},V{freeze}}.");
                newsym(x, y);
            } else if (flags.soundok && !lava)
                You_hear("N{N{i,sound},A{crackling}}").

            if (x == u.ux && y == u.uy) {
                if (u.uinwater) {       /* not just `if (Underwater)' */
                    /* leave the no longer existent water */
                    u.uinwater = 0;
                    u.uundetected = 0;
                    doredraw();
                    vision_full_recalc = 1;
                } else if (u.utrap && u.utraptype == TT_LAVA) {
                    if (Passes_walls) {
                        pline("C{N=%s,V{V{pass through},"
                              "N{N{rock},A{now-solid}}}}.");
                    } else {
                        u.utrap = rn1(50, 20);
                        u.utraptype = TT_INFLOOR;
                        pline("C{N=%s,V{V{are},A{A{A{stuck},D{firmly}},"
                              "D{e,E{in},N{N{rock},A{cooling}}}}}}.");
                    }
                }
            } else if ((mon = m_at(level, x, y)) != 0) {
                /* probably ought to do some hefty damage to any non-ice
                   creature caught in freezing water; at a minimum, eels are
                   forced out of hiding */
                if (is_swimmer(mon->data) && mon->mundetected) {
                    mon->mundetected = 0;
                    newsym(x, y);
                }
            }
        }
        obj_ice_effects(level, x, y, TRUE);
    }
    if (closed_door(level, x, y)) {
        int new_doormask = -1;
        const char see_buf[BUFSZ] = {0}, sense_buf[BUFSZ] = {0},
              hear_buf[BUFSZ] = {0};

        rangemod = -1000;
        switch (abstype) {
        case ZT_FIRE:
            new_doormask = D_NODOOR;
            strcpy(see_buf, "C{N{door},V{V{are},A{A{consumed},"
                            "D{E{in},N{*,N{o,flame}}}}}}!");
            sprintf(sense_buf, "C{N=%s,V{V{smell},N{o,smoke}}}.");
            break;
        case ZT_COLD:
            new_doormask = D_NODOOR;
            strcpy(see_buf, "C{N{door},V{+,V{freeze},V{shatter}}}!");
            strcpy(sense_buf, "C{N=%s,V{V{feel},A{cold}}}.");
            break;
        case ZT_DEATH:
            /* death spells/wands don't disintegrate */
            if (abs(type) != ZT_BREATH(ZT_DEATH))
                goto def_case;
            new_doormask = D_NODOOR;
            strcpy(see_buf, "C{N{door},V{disintegrate}}!");
            strcpy(hear_buf, "N{N{wood},A{crashing}}");
            break;
        case ZT_LIGHTNING:
            new_doormask = D_BROKEN;
            strcpy(see_buf, "C{N{door},V{splinter}}!");
            strcpy(hear_buf, "N{C{n,c,V{crackle}}}");
            break;
        default:
        def_case:
            if (cansee(x, y)) {
                pline("C{N{door},V{V{absorb},N{o,N=%s,N=%s}}}!",
                      abs(type) < ZT_SPELL(0) ? "N{bolt}" : abs(type) <
                      ZT_BREATH(0) ? "N{spell}" : "N{blast}", you);
            } else
                pline("C{N=%s,V{V{feel},N{*,N{o,vibrations}}}}.");
            break;
        }
        if (new_doormask >= 0) {        /* door gets broken */
            if (*in_rooms(level, x, y, SHOPBASE)) {
                if (type >= 0) {
                    add_damage(x, y, 400L);
                    *shopdamage = TRUE;
                } else  /* caused by monster */
                    add_damage(x, y, 0L);
            }
            loc->doormask = new_doormask;
            unblock_point(x, y);        /* vision */
            if (cansee(x, y)) {
                pline(see_txt);
                newsym(x, y);
            } else if (sense_txt) {
                pline(sense_txt);
            } else if (hear_txt) {
                if (flags.soundok)
                    You_hear(hear_txt);
            }
            if (picking_at(x, y)) {
                stop_occupation();
                reset_pick();
            }
        }
    }

    if (OBJ_AT(x, y) && abstype == ZT_FIRE)
        if (burn_floor_paper(level, x, y, FALSE, type > 0) && couldsee(x, y)) {
            newsym(x, y);
            pline("C{N=%s,V{V=%s,N{l,N=%s,N{o,smoke}}}}.", you,
                  Blind ? "V{smell}" : "V{see}",
                  Blind ? "N{i,whiff}" : "N{i,puff}");
        }
    if ((mon = m_at(level, x, y)) != 0) {
        /* Cannot use wakeup() which also angers the monster */
        mon->msleeping = 0;
        if (mon->m_ap_type)
            seemimic(mon);
        if (type >= 0) {
            setmangry(mon);
            if (mon->ispriest && *in_rooms(level, mon->mx, mon->my, TEMPLE))
                ghod_hitsu(mon);
            if (mon->isshk && !*u.ushops)
                hot_pursuit(mon);
        }
    }
    return rangemod;
}

/* fractured by pick-axe or wand of striking */
void
fracture_rock(struct obj *obj)
{
    /* A little Sokoban guilt... */
    if (obj->otyp == BOULDER && In_sokoban(&u.uz) && !flags.mon_moving)
        change_luck(-1);

    obj->otyp = ROCK;
    obj->quan = (long)rn1(60, 7);
    obj->owt = weight(obj);
    obj->oclass = GEM_CLASS;
    obj->known = FALSE;
    obj->onamelth = 0;  /* no names */
    obj->oxlth = 0;     /* no extra data */
    obj->oattached = OATTACHED_NOTHING;
    if (obj->where == OBJ_FLOOR) {
        obj_extract_self(obj);  /* move rocks back on top */
        place_object(obj, level, obj->ox, obj->oy);
        if (!does_block(obj->olev, obj->ox, obj->oy))
            unblock_point(obj->ox, obj->oy);
        if (cansee(obj->ox, obj->oy))
            newsym(obj->ox, obj->oy);
    }
}

/* handle statue hit by striking/force bolt/pick-axe */
boolean
break_statue(struct obj *obj)
{
    /* [obj is assumed to be on floor, so no get_obj_location() needed] */
    struct trap *trap = t_at(level, obj->ox, obj->oy);
    struct obj *item;

    if (trap && trap->ttyp == STATUE_TRAP &&
        activate_statue_trap(trap, obj->ox, obj->oy, TRUE))
        return FALSE;
    /* drop any objects contained inside the statue */
    while ((item = obj->cobj) != 0) {
        obj_extract_self(item);
        place_object(item, level, obj->ox, obj->oy);
    }
    if (Role_if(PM_ARCHEOLOGIST) && !flags.mon_moving &&
        (obj->spe & STATUE_HISTORIC)) {
        if (cansee(obj->ox, obj->oy))
            /* TODO: What do do with "such"? */
            pline("You feel guilty about damaging such a historic statue.");
        adjalign(-1);
    }
    obj->spe = 0;
    fracture_rock(obj);
    return TRUE;
}

const char *const destroy_strings[] = { /* also used in trap.c */
    "V{+,V{freeze},V{shatter}}", "N{p,N{i,potion},V{shatter}}",
    "V{+,V{boil},V{explode}}", "N{c,N{i,potion},V{boil}}",
    "V{+,V{catch fire},V{burn}}", "N{c,N{i,scroll},V{burn}}",
    "V{+,V{catch fire},V{burn}}", "N{c,N{i,book},V{burn}}",
    "V{+,V{V{turn},D{E{to},N{o,dust}}},V{vanish}}", "?{}",
    "V{+,V{break apart},V{explode}}", "N{c,N{i,wand},V{explode}}"
};

void
destroy_item(int osym, int dmgtyp)
{
    struct obj *obj, *obj2;
    int dmg, xresist, skip;
    long i, cnt, quan;
    int dindx;

    for (obj = invent; obj; obj = obj2) {
        obj2 = obj->nobj;
        if (obj->oclass != osym)
            continue;   /* test only objs of type osym */
        if (obj->oartifact)
            continue;   /* don't destroy artifacts */
        if (obj->in_use && obj->quan == 1)
            continue;   /* not available */
        xresist = skip = 0;
        dmg = dindx = 0;
        quan = 0L;

        switch (dmgtyp) {
        case AD_COLD:
            if (osym == POTION_CLASS && obj->otyp != POT_OIL) {
                quan = obj->quan;
                dindx = 0;
                dmg = rnd(4);
            } else
                skip++;
            break;
        case AD_FIRE:
            xresist = (Fire_resistance && obj->oclass != POTION_CLASS);

            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                skip++;
            if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                skip++;
                if (!Blind) {
                    /* TODO: Huge kludge to get around odd construction. */
                    char buf[BUFSZ+2];
                    strcpy(buf+2, hcolor("N{N{i,red},A{dark}}"));
                    if (Hallucination)
                      strncpy(buf, /*nointl*/"N{i,", 4);

                    pline("C{N=%s,V{J{but},V{V{glow},N{N=%s,A{strange}}},"
                          "V{V{remain},A{intact}}}}.",
                          xname(obj), buf, xname(obj));
                }
            }
            quan = obj->quan;
            switch (osym) {
            case POTION_CLASS:
                dindx = 1;
                dmg = rnd(6);
                break;
            case SCROLL_CLASS:
                dindx = 2;
                dmg = 1;
                break;
            case SPBOOK_CLASS:
                dindx = 3;
                dmg = 1;
                break;
            default:
                skip++;
                break;
            }
            break;
        case AD_ELEC:
            xresist = (Shock_resistance && obj->oclass != RING_CLASS);
            quan = obj->quan;
            switch (osym) {
            case RING_CLASS:
                if (obj->otyp == RIN_SHOCK_RESISTANCE) {
                    skip++;
                    break;
                }
                dindx = 4;
                dmg = 0;
                break;
            case WAND_CLASS:
                if (obj->otyp == WAN_LIGHTNING) {
                    skip++;
                    break;
                }
                dindx = 5;
                dmg = rnd(10);
                break;
            default:
                skip++;
                break;
            }
            break;
        default:
            skip++;
            break;
        }
        if (!skip) {
            if (obj->in_use)
                --quan; /* one will be used up elsewhere */
            for (i = cnt = 0L; i < quan; i++)
                if (!rn2(3))
                    cnt++;

            if (!cnt)
                continue;

            const char *str;
            /* TODO: Determiners... */
            if (cnt == quan)
                pline("C{N{o,N=%s,N=%s},V=%s}!", xname(obj), you,
                      destroy_strings[dindx * 2]);
            else if (cnt == 1L)
                pline("C{N{s,f,N{o,N=%s,N=%s},A{1}},V=%s}!", xname(obj), you,
                      destroy_strings[dindx * 2]);
            else
                pline("C{N{f,N{o,some},N{o,N=%s,N=%s}},V=%s}!", xname(obj), you,
                      destroy_strings[dindx * 2]);

            if (osym == POTION_CLASS && dmgtyp != AD_COLD) {
                if (!breathless(youmonst.data) || haseyes(youmonst.data))
                    potionbreathe(obj);
            }
            if (obj->owornmask) {
                if (obj->owornmask & W_RING)    /* ring being worn */
                    Ring_gone(obj);
                else
                    setnotworn(obj);
            }
            if (obj == current_wand)
                current_wand = 0;       /* destroyed */
            for (i = 0; i < cnt; i++)
                useup(obj);
            if (dmg) {
                if (xresist)
                    pline("C{N=%s,V{-,V{V{are},A{hurt}}}}!");
                else {
                    const char *how = destroy_strings[dindx * 2 + 1];
                    boolean one = (cnt == 1L);

                    losehp(dmg, one ? how : (const char *)makeplural(how),
                           one ? KILLED_BY_AN : KILLED_BY);
                    exercise(A_STR, FALSE);
                }
            }
        }
    }
    return;
}

int
destroy_mitem(struct monst *mtmp, int osym, int dmgtyp)
{
    struct obj *obj, *obj2;
    int skip, tmp = 0;
    long i, cnt, quan;
    int dindx;
    boolean vis;

    if (mtmp == &youmonst) {    /* this simplifies artifact_hit() */
        destroy_item(osym, dmgtyp);
        return 0;       /* arbitrary; value doesn't matter to artifact_hit() */
    }

    vis = canseemon(mtmp);
    for (obj = mtmp->minvent; obj; obj = obj2) {
        obj2 = obj->nobj;
        if (obj->oclass != osym)
            continue;   /* test only objs of type osym */
        skip = 0;
        quan = 0L;
        dindx = 0;

        switch (dmgtyp) {
        case AD_COLD:
            if (osym == POTION_CLASS && obj->otyp != POT_OIL) {
                quan = obj->quan;
                dindx = 0;
                tmp++;
            } else
                skip++;
            break;
        case AD_FIRE:
            if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
                skip++;
            if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
                skip++;
                if (vis) {
                    /* TODO: Huge kludge to get around odd construction. */
                    char buf[BUFSZ+2];
                    strcpy(buf+2, hcolor("N{N{i,red},A{dark}}"));
                    if (Hallucination)
                      strncpy(buf, /*nointl*/"N{i,", 4);

                    pline("C{N=%s,V{J{but},V{V{glow},N{N=%s,A{strange}}},"
                          "V{V{remain},A{intact}}}}.",
                          distant_name(obj, xname), buf,
                          distane_name(obj, xname));
                }
            }
            quan = obj->quan;
            switch (osym) {
            case POTION_CLASS:
                dindx = 1;
                tmp++;
                break;
            case SCROLL_CLASS:
                dindx = 2;
                tmp++;
                break;
            case SPBOOK_CLASS:
                dindx = 3;
                tmp++;
                break;
            default:
                skip++;
                break;
            }
            break;
        case AD_ELEC:
            quan = obj->quan;
            switch (osym) {
            case RING_CLASS:
                if (obj->otyp == RIN_SHOCK_RESISTANCE) {
                    skip++;
                    break;
                }
                dindx = 4;
                break;
            case WAND_CLASS:
                if (obj->otyp == WAN_LIGHTNING) {
                    skip++;
                    break;
                }
                dindx = 5;
                tmp++;
                break;
            default:
                skip++;
                break;
            }
            break;
        default:
            skip++;
            break;
        }
        if (!skip) {
            for (i = cnt = 0L; i < quan; i++)
                if (!rn2(3))
                    cnt++;

            if (!cnt)
                continue;
            if (vis)
                pline("C{N{o,N=%s,N=%s},V=%s}!", mon_nam(mtmp), xname(obj),
                      destroy_strings[dindx * 2]);
            for (i = 0; i < cnt; i++)
                m_useup(mtmp, obj);
        }
    }
    return tmp;
}


int
resist(struct monst *mtmp, char oclass, int damage, int domsg)
{
    int resisted;
    int alev, dlev;

    /* attack level */
    switch (oclass) {
    case WAND_CLASS:
        alev = 12;
        break;
    case TOOL_CLASS:
        alev = 10;
        break;  /* instrument */
    case WEAPON_CLASS:
        alev = 10;
        break;  /* artifact */
    case SCROLL_CLASS:
        alev = 9;
        break;
    case POTION_CLASS:
        alev = 6;
        break;
    case RING_CLASS:
        alev = 5;
        break;
    default:
        alev = u.ulevel;
        break;  /* spell */
    }
    /* defense level */
    dlev = (int)mtmp->m_lev;
    if (dlev > 50)
        dlev = 50;
    else if (dlev < 1)
        dlev = is_mplayer(mtmp->data) ? u.ulevel : 1;

    resisted = rn2(100 + alev - dlev) < mtmp->data->mr;
    if (resisted) {
        if (domsg) {
            shieldeff(mtmp->mx, mtmp->my);
            pline("C{N=%s,V{resist}}!", mon_nam(mtmp));
        }
        damage = (damage + 1) / 2;
    }

    if (damage) {
        mtmp->mhp -= damage;
        if (mtmp->mhp < 1) {
            if (m_using)
                monkilled(mtmp, "", AD_RBRE);
            else
                killed(mtmp);
        }
    }
    return resisted;
}

void
makewish(void)
{
    char buf[BUFSZ];
    struct obj *otmp, nothing;
    int tries = 0;

    nothing = zeroobj;  /* lint suppression; only its address matters */
    if (flags.verbose)
        pline("C{N=%s,V{V{wish for},N{i,object}}}.");
retry:
    getlin("C{q,C{N=%s,V{V{wish},D{E{for},N{what}}}}}?", buf);
    if (buf[0] == '\033')
        buf[0] = 0;
    /* 
     *  Note: if they wished for and got a non-object successfully,
     *  otmp == &zeroobj.  That includes gold, or an artifact that
     *  has been denied.  Wishing for "nothing" requires a separate
     *  value to remain distinct.
     */
    otmp = readobjnam(buf, &nothing, TRUE);
    if (!otmp) {
        /* TODO: Determiner "that" */
        pline("C{N{N{o,nothing},C{n,c,V{V{fit},N{N{o,description},N{that}}}}},"
              "V{V{exist},D{E{in},N{game}}}}.");
        if (++tries < 5)
            goto retry;
        pline("C{N{that},V{V{are},N{N{*,N{o,try}},A{enough}}}}!");
        otmp = readobjnam(NULL, NULL, TRUE);
        if (!otmp)
            return;     /* for safety; should never happen */
    } else if (otmp == &nothing) {
        historic_event(FALSE, "V{V{refuse},N{i,wish}}");
        /* explicitly wished for "nothing", presumeably attempting to retain
           wishless conduct */
        return;
    } else
        historic_event(FALSE, "V{V{wish},D{E{for},N{o,\"%s\"}}}.", buf);

    /* KMH, conduct */
    u.uconduct.wishes++;

    if (otmp != &zeroobj) {
        examine_object(otmp);
        /* (aobjnam()) is safe since otmp is unidentified -dlc */
        /* TODO: hold_another_object */
        hold_another_object(otmp,
                            u.uswallow ? "Oops!  %s out of your reach!"
                            : (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
                               level->locations[u.ux][u.uy].typ < IRONBARS ||
                               level->locations[u.ux][u.uy].typ >=
                               ICE) ? "Oops!  %s away from you!" :
                            "Oops!  %s to the floor!",
                            (aobjnam
                             (otmp, Is_airlevel(&u.uz) ||
                              u.uinwater ? "slip" : "drop")), NULL);
        u.ublesscnt += rn1(100, 50);    /* the gods take notice */
    }
}

/*zap.c*/
