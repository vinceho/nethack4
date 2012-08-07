/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "emin.h"
#include "epri.h"

/* mon summons a monster */
void
msummon(struct monst *mon)
{
    const struct permonst *ptr;
    int dtype = NON_PM, cnt = 0;
    aligntyp atyp;
    struct monst *mtmp;
    struct d_level *dlev;

    if (mon) {
        ptr = mon->data;
        dlev = &mon->dlevel->z;
        atyp = (ptr->maligntyp == A_NONE) ? A_NONE : sgn(ptr->maligntyp);
        if (mon->ispriest || roamer_type(mon->data))
            atyp = EPRI(mon)->shralign;
    } else {
        ptr = &mons[PM_WIZARD_OF_YENDOR];
        atyp = (ptr->maligntyp == A_NONE) ? A_NONE : sgn(ptr->maligntyp);
        dlev = &u.uz;
    }

    if (is_dprince(ptr) || (ptr == &mons[PM_WIZARD_OF_YENDOR])) {
        dtype = (!rn2(20)) ? dprince(atyp) :
                (!rn2(4)) ? dlord(atyp) : ndemon(dlev, atyp);
        cnt = (!rn2(4) && is_ndemon(&mons[dtype])) ? 2 : 1;
    } else if (is_dlord(ptr)) {
        dtype = (!rn2(50)) ? dprince(atyp) :
                (!rn2(20)) ? dlord(atyp) : ndemon(dlev, atyp);
        cnt = (!rn2(4) && is_ndemon(&mons[dtype])) ? 2 : 1;
    } else if (is_ndemon(ptr)) {
        dtype = (!rn2(20)) ? dlord(atyp) :
                (!rn2(6)) ? ndemon(dlev, atyp) : monsndx(ptr);
        cnt = 1;
    } else if (mon && is_lminion(mon)) {
        dtype = (is_lord(ptr) && !rn2(20)) ? llord() :
                (is_lord(ptr) || !rn2(6)) ? lminion() : monsndx(ptr);
        cnt = (!rn2(4) && !is_lord(&mons[dtype])) ? 2 : 1;
    } else if (ptr == &mons[PM_ANGEL]) {
        /* non-lawful angels can also summon */
        if (!rn2(6)) {
            switch (atyp) {     /* see summon_minion */
            case A_NEUTRAL:
                dtype = PM_AIR_ELEMENTAL + rn2(4);
                break;
            case A_CHAOTIC:
            case A_NONE:
                dtype = ndemon(dlev, atyp);
                break;
            }
        } else {
            dtype = PM_ANGEL;
        }
        cnt = (!rn2(4) && !is_lord(&mons[dtype])) ? 2 : 1;
    }

    if (dtype == NON_PM)
        return;

    /* sanity checks */
    if (cnt > 1 && (mons[dtype].geno & G_UNIQ))
        cnt = 1;
    /* 
     * If this daemon is unique and being re-summoned (the only way we
     * could get this far with an extinct dtype), try another.
     */
    if (mvitals[dtype].mvflags & G_GONE) {
        dtype = ndemon(dlev, atyp);
        if (dtype == NON_PM)
            return;
    }

    while (cnt > 0) {
        mtmp = makemon(&mons[dtype], level, u.ux, u.uy, NO_MM_FLAGS);
        if (mtmp && roamer_type(&mons[dtype])) {
            /* alignment should match the summoner */
            EPRI(mtmp)->shralign = atyp;
        }
        cnt--;
    }
}

void
summon_minion(aligntyp alignment, boolean talk)
{
    struct monst *mon;
    int mnum;

    switch ((int)alignment) {
    case A_LAWFUL:
        mnum = lminion();
        break;
    case A_NEUTRAL:
        mnum = PM_AIR_ELEMENTAL + rn2(4);
        break;
    case A_CHAOTIC:
    case A_NONE:
        mnum = ndemon(&u.uz, alignment);
        break;
    default:
        impossible("unaligned player?");
        mnum = ndemon(&u.uz, A_NONE);
        break;
    }
    if (mnum == NON_PM) {
        mon = 0;
    } else if (mons[mnum].pxtyp == MX_NONE) {
        const struct permonst *pm = &mons[mnum];

        mon = makemon(pm, level, u.ux, u.uy, MM_EMIN);
        if (mon) {
            mon->isminion = TRUE;
            EMIN(mon)->min_align = alignment;
        }
    } else if (roamer_type(&mons[mnum])) {
        mon = makemon(&mons[mnum], level, u.ux, u.uy, NO_MM_FLAGS);
        if (mon) {
            mon->isminion = TRUE;
            EPRI(mon)->shralign = alignment;
        }
    } else
        mon = makemon(&mons[mnum], level, u.ux, u.uy, NO_MM_FLAGS);
    if (mon) {
        if (talk) {
            pline("The voice of %s booms:", align_gname(alignment));
            verbalize("Thou shalt pay for thy indiscretion!");
            if (!Blind)
                pline("%s appears before you.", Amonnam(mon));
        }
        mon->mpeaceful = FALSE;
        /* don't call set_malign(); player was naughty */
    }
}

#define Athome (Inhell && !mtmp->cham)

/* returns 1 if it won't attack. */
int
demon_talk(struct monst *mtmp)
{
    long cash, demand, offer;

    if (uwep && uwep->oartifact == ART_EXCALIBUR) {
        pline("%s looks very angry.", Amonnam(mtmp));
        mtmp->mpeaceful = mtmp->mtame = 0;
        set_malign(mtmp);
        newsym(mtmp->mx, mtmp->my);
        return 0;
    }

    /* Slight advantage given. */
    if (is_dprince(mtmp->data) && mtmp->minvis) {
        mtmp->minvis = mtmp->perminvis = 0;
        if (!Blind)
            pline("%s appears before you.", Amonnam(mtmp));
        newsym(mtmp->mx, mtmp->my);
    }
    if (youmonst.data->mlet == S_DEMON) {       /* Won't blackmail their own. */
        pline("%s says, \"Good hunting, %s.\"", Amonnam(mtmp),
              flags.female ? "Sister" : "Brother");
        if (!tele_restrict(mtmp))
            rloc(mtmp, FALSE);
        return 1;
    }
    cash = money_cnt(invent);
    demand =
        (cash * (rnd(80) + 20 * Athome)) / (100 *
                                            (1 +
                                             (sgn(u.ualign.type) ==
                                              sgn(mtmp->data->maligntyp))));

    if (!demand) {      /* you have no gold */
        mtmp->mpeaceful = 0;
        set_malign(mtmp);
        return 0;
    } else {
        /* make sure that the demand is unmeetable if the monster has the
           Amulet, preventing monster from being satisified and removed from
           the game (along with said Amulet...) */
        if (mon_has_amulet(mtmp))
            demand = cash + (long)rn1(1000, 40);

        pline("%s demands %ld %s for safe passage.", Amonnam(mtmp), demand,
              currency(demand));

        if ((offer = bribe(mtmp)) >= demand) {
            pline("%s vanishes, laughing about cowardly mortals.",
                  Amonnam(mtmp));
        } else if (offer > 0L && (long)rnd(40) > (demand - offer)) {
            pline("%s scowls at you menacingly, then vanishes.", Amonnam(mtmp));
        } else {
            pline("%s gets angry...", Amonnam(mtmp));
            mtmp->mpeaceful = 0;
            set_malign(mtmp);
            return 0;
        }
    }
    mongone(mtmp);
    return 1;
}


long
bribe(struct monst *mtmp)
{
    char buf[BUFSZ];
    long offer;
    long umoney = money_cnt(invent);

    getlin("How much will you offer?", buf);
    if (sscanf(buf, "%ld", &offer) != 1)
        offer = 0L;

    /* Michael Paddon -- fix for negative offer to monster */
    /* JAR880815 - */
    if (offer < 0L) {
        pline("You try to shortchange %s, but fumble.", mon_nam(mtmp));
        return 0L;
    } else if (offer == 0L) {
        pline("You refuse.");
        return 0L;
    } else if (offer >= umoney) {
        pline("You give %s all your gold.", mon_nam(mtmp));
        offer = umoney;
    } else {
        pline("You give %s %ld %s.", mon_nam(mtmp), offer, currency(offer));
    }
    money2mon(mtmp, offer);

    iflags.botl = 1;
    return offer;
}


int
dprince(aligntyp atyp)
{
    int tryct, pm;

    for (tryct = 0; tryct < 20; tryct++) {
        pm = rn1(PM_DEMOGORGON + 1 - PM_ORCUS, PM_ORCUS);
        if (!(mvitals[pm].mvflags & G_GONE) &&
            (atyp == A_NONE || sgn(mons[pm].maligntyp) == sgn(atyp)))
            return pm;
    }
    return dlord(atyp); /* approximate */
}

int
dlord(aligntyp atyp)
{
    int tryct, pm;

    for (tryct = 0; tryct < 20; tryct++) {
        pm = rn1(PM_YEENOGHU + 1 - PM_JUIBLEX, PM_JUIBLEX);
        if (!(mvitals[pm].mvflags & G_GONE) &&
            (atyp == A_NONE || sgn(mons[pm].maligntyp) == sgn(atyp)))
            return pm;
    }
    return ndemon(&u.uz, atyp); /* approximate */
}

/* create lawful (good) lord */
int
llord(void)
{
    if (!(mvitals[PM_ARCHON].mvflags & G_GONE))
        return PM_ARCHON;

    return lminion();   /* approximate */
}

int
lminion(void)
{
    int tryct;
    const struct permonst *ptr;

    for (tryct = 0; tryct < 20; tryct++) {
        ptr = mkclass(&u.uz, S_ANGEL, 0);
        if (ptr && !is_lord(ptr))
            return monsndx(ptr);
    }

    return NON_PM;
}

int
ndemon(const d_level * dlev, aligntyp atyp)
{
    int tryct;
    const struct permonst *ptr;

    for (tryct = 0; tryct < 20; tryct++) {
        ptr = mkclass(dlev, S_DEMON, 0);
        if (ptr && is_ndemon(ptr) &&
            (atyp == A_NONE || sgn(ptr->maligntyp) == sgn(atyp)))
            return monsndx(ptr);
    }

    return NON_PM;
}

/*minion.c*/
