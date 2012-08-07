/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/*	Copyright (c) M. Stephenson 1988			  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static schar delay;     /* moves left for this spell */

/* spellmenu arguments; 0 thru n-1 used as spl_book[] index when swapping */
#define SPELLMENU_CAST (-2)
#define SPELLMENU_VIEW (-1)

#define KEEN 20000
#define MAX_SPELL_STUDY 3
#define incrnknow(spell)        spl_book[spell].sp_know = KEEN

#define spellev(spell)		spl_book[spell].sp_lev
#define spellname(spell)	OBJ_NAME(objects[spellid(spell)])
#define spellet(spell)	\
	((char)((spell < 26) ? ('a' + spell) : ('A' + spell - 26)))

static boolean cursed_book(struct obj *bp);
static boolean confused_book(struct obj *);
static void deadbook(struct obj *);
static int learn(void);
static boolean getspell(int *);
static boolean dospellmenu(const char *, int, int *);
static int percent_success(int);
static int throwspell(schar * dx, schar * dy);
static void cast_protection(void);
static void spell_backfire(int);
static const char *spelltypemnemonic(int);
static int isqrt(int);

static char putdownbuf[BUFSZ]; /* used for nomovemsg to point to */

/* The roles[] table lists the role-specific values for tuning
 * percent_success().
 *
 * Reasoning:
 *   spelbase, spelheal:
 *	Arc are aware of magic through historical research
 *	Bar abhor magic (Conan finds it 'interferes with his animal instincts')
 *	Cav are ignorant to magic
 *	Hea are very aware of healing magic through medical research
 *	Kni are moderately aware of healing from Paladin training
 *	Mon use magic to attack and defend in lieu of weapons and armor
 *	Pri are very aware of healing magic through theological research
 *	Ran avoid magic, preferring to fight unseen and unheard
 *	Rog are moderately aware of magic through trickery
 *	Sam have limited magical awareness, prefering meditation to conjuring
 *	Tou are aware of magic from all the great films they have seen
 *	Val have limited magical awareness, prefering fighting
 *	Wiz are trained mages
 *
 *	The arms penalty is lessened for trained fighters Bar, Kni, Ran,
 *	Sam, Val -
 *	the penalty is its metal interference, not encumbrance.
 *	The `spelspec' is a single spell which is fundamentally easier
 *	 for that role to cast.
 *
 *  spelspec, spelsbon:
 *	Arc map masters (SPE_MAGIC_MAPPING)
 *	Bar fugue/berserker (SPE_HASTE_SELF)
 *	Cav born to dig (SPE_DIG)
 *	Hea to heal (SPE_CURE_SICKNESS)
 *	Kni to turn back evil (SPE_TURN_UNDEAD)
 *	Mon to preserve their abilities (SPE_RESTORE_ABILITY)
 *	Pri to bless (SPE_REMOVE_CURSE)
 *	Ran to hide (SPE_INVISIBILITY)
 *	Rog to find loot (SPE_DETECT_TREASURE)
 *	Sam to be At One (SPE_CLAIRVOYANCE)
 *	Tou to smile (SPE_CHARM_MONSTER)
 *	Val control the cold (SPE_CONE_OF_COLD)
 *	Wiz all really, but SPE_MAGIC_MISSILE is their party trick
 *
 *	See percent_success() below for more comments.
 *
 *  uarmbon, uarmsbon, uarmhbon, uarmgbon, uarmfbon:
 *	Fighters find body armour & shield a little less limiting.
 *	Headgear, Gauntlets and Footwear are not role-specific (but
 *	still have an effect, except helm of brilliance, which is designed
 *	to permit magic-use).
 */

#define uarmhbon 4      /* Metal helmets interfere with the mind */
#define uarmgbon 6      /* Casting channels through the hands */
#define uarmfbon 2      /* All metal interferes to some degree */

/* TRUE: book should be destroyed by caller */
static boolean
cursed_book(struct obj *bp)
{
    int lev = objects[bp->otyp].oc_level;

    switch (rn2(lev)) {
    case 0:
        pline("C{N=%s,V{V{feel},N{N{i,sensation},A{wrenching}}}}.", you);
        tele(); /* teleport him */
        break;
    case 1:
        pline("C{N=%s,V{V{feel},A{V{threaten}}}}.", you);
        aggravate();
        break;
    case 2:
        make_blinded(Blinded + rn1(100, 250), TRUE);
        break;
    case 3:
        take_gold();
        break;
    case 4:
        pline("C{i,N{*,N{%d|rune}},V{V{V{are},"
              "A{A{A{much},D{too}},D{just}}},V{comprehend}}}.", bp->o_id);
        make_confused(HConfusion + rn1(7, 16), FALSE);
        break;
    case 5:
        pline("C{i,s,V{V{V{coat},N{book}},D{t,N{N{o,poison},A{contact}}}}}!");
        if (uarmg) {
            if (uarmg->oerodeproof || !is_corrodeable(uarmg)) {
                pline("C{s,V{V{-,V{V{seem},V{affect}}},"
                      "N{o,N{*,N{glove}},N=%s}}}.", you);
            } else if (uarmg->oeroded2 < MAX_ERODE) {
                if (uarmg->greased) {
                    grease_protect(uarmg, "N{*,N{glove}}", &youmonst);
                } else {
                    pline("C{N{o,N{*,N{glove}},N=%s},V=%s}!", you,
                          uarmg->oeroded2 + 1 == MAX_ERODE ?
                          "V{V{corrode},D{completely}}" :
                          uarmg->oeroded2 ?
                          "V{V{corrode},D{completely}}" : "V{corrode}");
                    uarmg->oeroded2++;
                }
            } else
                pline("C{N{o,N{*,N{glove}},N=%s},V{V=%s,"
                      "A{A{V{corrode}},D{completely}}}}.", you,
                      Blind ? "V{feel}" : "V{look}");
            break;
        }
        /* temp disable in_use; death should not destroy the book */
        bp->in_use = FALSE;
        losestr(Poison_resistance ? rn1(2, 1) : rn1(4, 3));
        losehp(rnd(Poison_resistance ? 6 : 10),
               "N{N{i,spellbook},A{A{V{poison}},"
               "D{t,N{N{o,poison},A{contact}}}}}", KILLED_BY_AN);
        bp->in_use = TRUE;
        break;
    case 6:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline("C{+,C{N{book},V{V{radiate},N{N{o,energy},A{explosive}}}},"
                  "C{-,C{s,V{V{harm},N=%s}}}}!", you);
        } else {
            int id = flags.ident++;
            pline("C{N{%d|book},V{V{V{V{radiate},N{N{o,energy},A{explosive}}},"
                  "D{a,N{o,N=%s,N=%s}}},D{Q{as},"
                  "C{N=%s,V{V{read},N{%d|book}}}}}}!",
                  id, body_part(FACE), you, you, id);
            losehp(2 * rnd(10) + 5, "N{a,N{i,rune},V{explode}}", KILLED_BY_AN);
        }
        return TRUE;
    default:
        rndcurse();
        break;
    }
    return FALSE;
}

/* study while confused: returns TRUE if the book is destroyed */
static boolean
confused_book(struct obj *spellbook)
{
    boolean gone = FALSE;

    if (!rn2(3) && spellbook->otyp != SPE_BOOK_OF_THE_DEAD) {
        spellbook->in_use = TRUE;       /* in case called from learn */
        pline("C{N=%s,V{V{V{V{tear},N{spellbook}},D{accidentally}},"
              "D{e,N{*,N{i,piece}}}}}.", you);
        if (!objects[spellbook->otyp].oc_name_known &&
            !objects[spellbook->otyp].oc_uname)
            docall(spellbook);
        useup(spellbook);
        gone = TRUE;
    } else {
        pline("C{N=%s,V{V{find},N{a,N=%s,V{V{V{read},N{N{line},A=%s}},"
              "D{over and over again}}}}}.",
              you, you, spellbook == book ? "A{next}" : "A{first}");
    }
    return gone;
}

/* special effects for The Book of the Dead */
static void
deadbook(struct obj *book2)
{
    struct monst *mtmp, *mtmp2;
    coord mm;

    pline("C{N=%s,V{V{turn},N{o,N{*,N{page}},N{Book of the Dead}}}}...", you);
    makeknown(SPE_BOOK_OF_THE_DEAD);
    /* KMH -- Need ->known to avoid '_a_ Book of the Dead' */
    book2->known = 1;
    if (invocation_pos(&u.uz, u.ux, u.uy) && !On_stairs(u.ux, u.uy)) {
        struct obj *otmp;
        boolean arti1_primed = FALSE, arti2_primed = FALSE, arti_cursed = FALSE;

        if (book2->cursed) {
            int id = flags.ident++;
            pline("C{N{*,N{%d|rune}},V{V{appear},A{V{scramble}}}}.",id);
            pline("C{N=%s,V{-,V{V{can},V{V{read},N{*,N{%d|rune}}}}}}!",you,id);
            return;
        }

        if (!u.uhave.bell || !u.uhave.menorah) {
            pline("C{N{i,chill},V{V{run down},N{o,N=%s,N=%s}}}.",
                  body_part(SPINE), you);
            if (!u.uhave.bell)
                pline("C{N=%s,V{V{hear},N{N{i,chime},A{faint}}}}...", you);
            if (!u.uhave.menorah)
                pline("C{s,V{V{amuse},N{o,N{doppelganger},P{Vlad}}}}.");
            return;
        }

        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (otmp->otyp == CANDELABRUM_OF_INVOCATION && otmp->spe == 7 &&
                otmp->lamplit) {
                if (!otmp->cursed)
                    arti1_primed = TRUE;
                else
                    arti_cursed = TRUE;
            }
            if (otmp->otyp == BELL_OF_OPENING && (moves - otmp->age) < 5L) {
                /* you rang it recently */
                if (!otmp->cursed)
                    arti2_primed = TRUE;
                else
                    arti_cursed = TRUE;
            }
        }

        if (arti_cursed) {
            pline("C{N{invocation},V{fail}}!");
            pline("C{N{s,f,N{*,N{artifact}},A{at least one}},"
                  "V{V{are},A{V{curse}}}}...");
        } else if (arti1_primed && arti2_primed) {
            unsigned soon = (unsigned)dice(2, 6); /* time til next intervene */
            char buf[BUFSZ];

            /* successful invocation */
            mkinvokearea();
            u.uevent.invoked = 1;
            sprintf(buf, "C{i,N=%s,V{V{perform},N{invocation}}}!", you);
            historic_event(FALSE, buf);
            /* in case you haven't killed the Wizard yet, behave as if you just 
               did */
            u.uevent.udemigod = 1;      /* wizdead() */
            if (!u.udg_cnt || u.udg_cnt > soon)
                u.udg_cnt = soon;
        } else {        /* at least one artifact not prepared properly */
            /* literal 'something', we don't want 'it is amiss' */
            pline("C{N=%s,V{V{have},N{N{i,feeling},A{Q{that},"
                  "C{N{o,something},V{V{are},A{amiss}}}}}}}...", you);
            goto raise_dead;
        }
        return;
    }

    /* when not an invocation situation */
    if (book2->cursed) {
    raise_dead:
        pline("C{i,N=%s,V{V{raise},N{dead}}}!", you);

        /* first maybe place a dangerous adversary */
        if (!rn2(3) &&
            ((mtmp =
              makemon(&mons[PM_MASTER_LICH], level, u.ux, u.uy,
                      NO_MINVENT)) != 0 ||
             (mtmp =
              makemon(&mons[PM_NALFESHNEE], level, u.ux, u.uy,
                      NO_MINVENT)) != 0)) {
            mtmp->mpeaceful = 0;
            set_malign(mtmp);
        }
        /* next handle the affect on things you're carrying */
        unturn_dead(&youmonst);
        /* last place some monsters around you */
        mm.x = u.ux;
        mm.y = u.uy;
        mkundead(level, &mm, TRUE, NO_MINVENT);
    } else if (book2->blessed) {
        for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
            mtmp2 = mtmp->nmon; /* tamedog() changes chain */
            if (DEADMONSTER(mtmp))
                continue;

            if (is_undead(mtmp->data) && cansee(mtmp->mx, mtmp->my)) {
                mtmp->mpeaceful = TRUE;
                if (sgn(mtmp->data->maligntyp) == sgn(u.ualign.type)
                    && distu(mtmp->mx, mtmp->my) < 4)
                    if (mtmp->mtame) {
                        if (mtmp->mtame < 20)
                            mtmp->mtame++;
                    } else
                        tamedog(mtmp, NULL);
                else
                    monflee(mtmp, 0, FALSE, TRUE);
            }
        }
    } else {
        switch (rn2(3)) {
        case 0:
            /* Originally 'your ancestors are annoyed at you', but this
               defies translation, especially as it's hard to automatically
               distinguish from 'your ancestors are annoyed by you'. */
            pline("C{N{o,N{*,N{ancestor}},N=%s},V{V{are},N{o,annoyed}}}!", you);
            break;
        case 1:
            pline("C{N{N{*,N{headstone}},A{a,N{cemetery}}},"
                  "V{V{begin},V{move}}}!");
            break;
        default:
            pline("S{Oh my!}");
            pline("C{N{o,N{name},N=%s},V{V{appear},D{a,N{book}}}}!", you);
        }
    }
    return;
}

static int
learn(void)
{
    int i;
    short booktype;
    char splname[BUFSZ];
    boolean costly = TRUE;

    /* JDS: lenses give 50% faster reading; 33% smaller read time */
    if (delay && ublindf && ublindf->otyp == LENSES && rn2(2))
        delay++;
    if (Confusion) {    /* became confused while learning */
        confused_book(book);
        book = 0;       /* no longer studying */
        nomul(delay, "V{V{read},N{i,book}}"); /* cannot now be interrupted */
        sprintf(putdownbuf, "C{N=%s,V{V{V{are},A{A{able},D{finally}}},"
                "V{V{put down},N{book}}}}.", you);
        nomovemsg = putdownbuf;
        delay = 0;
        return 0;
    }
    if (delay) {        /* not if (delay++), so at end delay == 0 */
        delay++;
        return 1;       /* still busy */
    }
    exercise(A_WIS, TRUE);      /* you're studying. */
    booktype = book->otyp;
    if (booktype == SPE_BOOK_OF_THE_DEAD) {
        deadbook(book);
        return 0;
    }

    sprintf(splname, objects[booktype].oc_name_known ?
            "N{m,N=%s}" : "N{N{spell},A{N{m,N=%s}}}",
            OBJ_NAME(objects[booktype]));
    for (i = 0; i < MAXSPELL; i++) {
        if (spellid(i) == booktype) {
            if (book->spestudied > MAX_SPELL_STUDY) {
                pline("C{N{%d|spellbook},V{V{V{are},A{A{faint},D{too}}},"
                      "V{V{be read},D{any more}}}}.", book->o_id);
                book->otyp = booktype = SPE_BLANK_PAPER;
            } else if (spellknow(i) <= 1000) {
                pline("C{N{l,N{o,N{knowledge},N=%s},N=%s},"
                      "V{V{are},A{c,A{keen}}}}.", you, splname);
                incrnknow(i);
                book->spestudied++;
                exercise(A_WIS, TRUE);  /* extra study */
            } else {    /* 1000 < spellknow(i) <= MAX_SPELL_STUDY */
                char qbuf[QBUFSZ];
                pline("C{N=%s,V{V{V{V{know},N=%s},D{D{well},D{quite}}},"
                      "D{already}}}.", you, splname);
                sprintf(qbuf,"C{q,C{N=%s,V{V{want},"
                        "V{V{V{read},N{book}},D{anyway}}}}}?", you);
                if (yn(qbuf) == 'y') {
                    pline("C{N=%s,V{V{refresh},"
                          "N{l,N{o,N{knowledge},N=%s},N=%s}}}.",
                          you, you, splname);
                    incrnknow(i);
                    book->spestudied++;
                } else
                    costly = FALSE;
            }
            /* make book become known even when spell is already known, in case 
               amnesia made you forget the book */
            makeknown((int)booktype);
            break;
        } else if (spellid(i) == NO_SPELL) {
            spl_book[i].sp_id = booktype;
            spl_book[i].sp_lev = objects[booktype].oc_level;
            incrnknow(i);
            book->spestudied++;
            if (i > 0)
                pline("C{N=%s,V{V{V{add},N=%s},D{i,N{o,N{repertoire},N=%s}}}}.",
                      you, splname, you);
            else
                pline("C{N=%s,V{V{learn},N=%s}}.", you, splname);
            makeknown((int)booktype);
            break;
        }
    }
    if (i == MAXSPELL)
        impossible("S{Too many spells memorized!}");

    if (book->cursed) { /* maybe a demon cursed it */
        if (cursed_book(book)) {
            useup(book);
            book = 0;
            return 0;
        }
    }
    if (costly)
        check_unpaid(book);
    book = 0;
    return 0;
}

int
study_book(struct obj *spellbook)
{
    int booktype = spellbook->otyp;
    boolean confused = (Confusion != 0);
    boolean too_hard = FALSE;

    if (delay && !confused && spellbook == book &&
        /* handle the sequence: start reading, get interrupted, have book
           become erased somehow, resume reading it */
        booktype != SPE_BLANK_PAPER) {
        pline("C{N=%s,V{V{continue},V{memorize}}}.", you);
    } else {
        /* KMH -- Simplified this code */
        if (booktype == SPE_BLANK_PAPER) {
            pline("C{N{%d|spellbook},V{V{are},A{A{blank},D{all}}}}.",
                  spellbook->o_id);
            makeknown(booktype);
            return 1;
        }
        switch (objects[booktype].oc_level) {
        case 1:
        case 2:
            delay = -objects[booktype].oc_delay;
            break;
        case 3:
        case 4:
            delay =
                -(objects[booktype].oc_level - 1) * objects[booktype].oc_delay;
            break;
        case 5:
        case 6:
            delay = -objects[booktype].oc_level * objects[booktype].oc_delay;
            break;
        case 7:
            delay = -8 * objects[booktype].oc_delay;
            break;
        default:
            impossible("S{Unknown spellbook level %d, book %d;}",
                       objects[booktype].oc_level, booktype);
            return 0;
        }

        /* Books are often wiser than their readers (Rus.) */
        spellbook->in_use = TRUE;
        if (!spellbook->blessed && spellbook->otyp != SPE_BOOK_OF_THE_DEAD) {
            if (spellbook->cursed) {
                too_hard = TRUE;
            } else {
                /* uncursed - chance to fail */
                int read_ability =
                    ACURR(A_INT) + 4 + u.ulevel / 2 -
                    2 * objects[booktype].oc_level +
                    ((ublindf && ublindf->otyp == LENSES) ? 2 : 0);
                /* only wizards know if a spell is too difficult */
                if (Role_if(PM_WIZARD) && read_ability < 20 && !confused) {
                    char qbuf[QBUFSZ];

                    sprintf(qbuf,
                            "C{N=%s,V{V{are},A{A=%s,V{comprehend}}}}.  "
                            "C{q,C{i,V{continue}}}?", xname(spellbook),
                            (read_ability < 12 ?
                             "A{A{difficult},D{very}}" : "A{difficult}"));
                    if (yn(qbuf) != 'y') {
                        spellbook->in_use = FALSE;
                        return 1;
                    }
                }
                /* it's up to random luck now */
                if (rnd(20) > read_ability) {
                    too_hard = TRUE;
                }
            }
        }

        if (too_hard) {
            boolean gone = cursed_book(spellbook);

            nomul(delay, "V{V{read},N{i,book}}");     /* study time */
            nomovemsg = 0;      /* default: "You can move again." */
            delay = 0;
            if (gone || !rn2(3)) {
                if (!gone)
                    pline("C{N=%s,V{V{crumble},D{e,N{o,dust}}}}!",
                          xname(spellbook));
                if (!objects[spellbook->otyp].oc_name_known &&
                    !objects[spellbook->otyp].oc_uname)
                    docall(spellbook);
                useup(spellbook);
            } else
                spellbook->in_use = FALSE;
            return 1;
        } else if (confused) {
            if (!confused_book(spellbook)) {
                spellbook->in_use = FALSE;
            }
            nomul(delay, "V{V{read},N{i,book}}");
            sprintf(putdownbuf, "C{N=%s,V{V{V{are},A{A{able},D{finally}}},"
                    "V{V{put down},N{book}}}}.", you);
            nomovemsg = putdownbuf;
            delay = 0;
            return 1;
        }
        spellbook->in_use = FALSE;

        pline("C{N=%s,V{V{begin},V{V=%s,N{*,N{rune}}}}}.", you,
              spellbook->otyp == SPE_BOOK_OF_THE_DEAD ?
              "V{recite}" : "V{memorize}");
    }

    book = spellbook;
    set_occupation(learn, "V{study}", 0);
    return 1;
}

/* a spellbook has been destroyed or the character has changed levels;
   the stored address for the current book is no longer valid */
void
book_disappears(struct obj *obj)
{
    if (obj == book)
        book = NULL;
}

/* renaming an object usually results in it having a different address;
   so the sequence start reading, get interrupted, name the book, resume
   reading would read the 'new' book from scratch */
void
book_substitution(struct obj *old_obj, struct obj *new_obj)
{
    if (old_obj == book)
        book = new_obj;
}

/* called from moveloop() */
void
age_spells(void)
{
    int i;

    /* 
     * The time relative to the hero (a pass through move
     * loop) causes all spell knowledge to be decremented.
     * The hero's speed, rest status, conscious status etc.
     * does not alter the loss of memory.
     */
    for (i = 0; i < MAXSPELL && spellid(i) != NO_SPELL; i++)
        if (spellknow(i))
            decrnknow(i);
    return;
}

/*
 * Return TRUE if a spell was picked, with the spell index in the return
 * parameter.  Otherwise return FALSE.
 */
static boolean
getspell(int *spell_no)
{
    if (spellid(0) == NO_SPELL) {
        pline("C{-,C{N=%s,V{V{V{know},N{N{*,N{i,spell}},A{any}}},"
              "D{right now}}}}.", you);
        return FALSE;
    }

    return dospellmenu("C{i,V{V{V{choose},N{i,spell}},V{cast}}}:",
                       SPELLMENU_CAST, spell_no);
}

/* the 'Z' command -- cast a spell */
int
docast(void)
{
    int spell_no;

    if (getspell(&spell_no))
        return spelleffects(spell_no, FALSE);
    return 0;
}

static const char *
spelltypemnemonic(int skill)
{
    switch (skill) {
    case P_ATTACK_SPELL:
        return "A{attack}";
    case P_HEALING_SPELL:
        return "A{healing}";
    case P_DIVINATION_SPELL:
        return "A{divination}";
    case P_ENCHANTMENT_SPELL:
        return "A{enchantment}";
    case P_CLERIC_SPELL:
        return "A{clerical}";
    case P_ESCAPE_SPELL:
        return "A{escape}";
    case P_MATTER_SPELL:
        return "A{matter}";
    default:
        impossible("S{Unknown spell skill, %d;}", skill);
        return "A{erroneous}";
    }
}

int
spell_skilltype(int booktype)
{
    return objects[booktype].oc_skill;
}

static void
cast_protection(void)
{
    int loglev = 0;
    int l = u.ulevel;
    int natac = u.uac - u.uspellprot;
    int gain;

    /* loglev=log2(u.ulevel)+1 (1..5) */
    while (l) {
        loglev++;
        l /= 2;
    }

    /***
       The more u.uspellprot you already have, the less you get, and the better 
       your natural ac, the less you get. 
       LEVEL  AC SPELLPROT from sucessive SPE_PROTECTION casts
       1      10  0, 1, 2, 3, 4
       1      0   0, 1, 2, 3
       1     -10  0, 1, 2
       2-3    10  0, 2, 4, 5, 6, 7, 8
       2-3    0   0, 2, 4, 5, 6
       2-3   -10  0, 2, 3, 4
       4-7    10  0, 3, 6, 8, 9, 10, 11, 12
       4-7    0   0, 3, 5, 7, 8, 9
       4-7   -10  0, 3, 5, 6
       7-15  -10  0, 3, 5, 6
       8-15   10  0, 4, 7, 10, 12, 13, 14, 15, 16
       8-15   0   0, 4, 7, 9, 10, 11, 12
       8-15  -10  0, 4, 6, 7, 8
       16-30  10  0, 5, 9, 12, 14, 16, 17, 18, 19, 20
       16-30  0   0, 5, 9, 11, 13, 14, 15
       16-30 -10  0, 5, 8, 9, 10
    */
    gain = loglev - (int)u.uspellprot / (4 - min(3, (10 - natac) / 10));

    if (gain > 0) {
        if (!Blind) {
            const char *hgolden = hcolor("A{golden}");

            if (u.uspellprot)
                pline("C{N{N{N{haze},A=%s},E{around},N=%s},"
                      "V{V{become},A{A{dense},D{more}}}}.", hgolden, you);
            else
                pline("C{N{N=%s,E{around},N=%s},V{V{begin},"
                      "V{V{shimmer with},N{N{i,haze},A=%s}}}}.",
                      (Underwater || Is_waterlevel(&u.uz)) ? "N{water}" :
                      u.uswallow ? mbodypart(u.ustuck, STOMACH) :
                      IS_STWALL(level->locations[u.dx][u.dy].typ) ?
                      "N{stone}" : "N{air}", you, hgolden);
        }
        u.uspellprot += gain;
        u.uspmtime =
            P_SKILL(spell_skilltype(SPE_PROTECTION)) == P_EXPERT ? 20 : 10;
        if (!u.usptime)
            u.usptime = u.uspmtime;
        find_ac();
    } else {
        /* Should this be body-parted? */
        pline("C{N{o,N{skin},N=%s},V{V{V{feel},A{warm}},D{d,N{i,moment}}}}.",
              you);
    }
}

/* attempting to cast a forgotten spell will cause disorientation */
static void
spell_backfire(int spell)
{
    long duration = (long)((spellev(spell) + 1) * 3);   /* 6..24 */

    /* prior to 3.4.1, the only effect was confusion; it still predominates */
    switch (rn2(10)) {
    case 0:
    case 1:
    case 2:
    case 3:
        make_confused(duration, FALSE); /* 40% */
        break;
    case 4:
    case 5:
    case 6:
        make_confused(2L * duration / 3L, FALSE);       /* 30% */
        make_stunned(duration / 3L, FALSE);
        break;
    case 7:
    case 8:
        make_stunned(2L * duration / 3L, FALSE);        /* 20% */
        make_confused(duration / 3L, FALSE);
        break;
    case 9:
        make_stunned(duration, FALSE);  /* 10% */
        break;
    }
    return;
}

int
spelleffects(int spell, boolean atme)
{
    int energy, damage, chance, n, intell;
    int skill, role_skill;
    boolean confused = (Confusion != 0);
    struct obj *pseudo;
    boolean dummy;
    coord cc;
    schar dx, dy, dz;
    char spellnoun[80];
    char buf[BUFSZ];
    sprintf(spellnoun, "N{t%d|spell}", spellid(spell));

    /* 
     * Spell casting no longer affects knowledge of the spell. A
     * decrement of spell knowledge is done every turn.
     */
    if (spellknow(spell) <= 0) {
        pline("C{s,V{V{twist},N{o,N{l,N{knowledge},N=%s},N=%s}}}.",
              spellnoun, you);
        pline("C{N=%s,V{V{V{invoke},N{N{*,N{i,image}},A{nightmarish}}},"
              "D{a,N{o,N{mind},N=%s}}}}!", spellnoun, you);
        spell_backfire(spell);
        return 0;
    } else if (spellknow(spell) <= 200) {       /* 1% */
        pline("C{N=%s,V{V{strain},V{V{recall},N{spell}}}}.", you);
    } else if (spellknow(spell) <= 1000) {      /* 5% */
        pline("C{c,N{l,N{o,N{knowledge},N=%s},N=%s},V{V{grow},A{faint}}}.",
              you, spellnoun);
    }
    energy = (spellev(spell) * 5);      /* 5 <= energy <= 35 */

    sprintf(buf,"C{N{o,N{concentration},N=%s},V{V{falter},D{Q{because},"
            "C{c,N=%s,V{V{carry},N{N{stuff},A{A{much},D{so}}}}}}}}.", you, you);

    if (u.uhunger <= 10 && spellid(spell) != SPE_DETECT_FOOD) {
        pline("C{N=%s,V{V{V{are},A{A{hungry},D{too}}},V{V{cast},N=%s}}}.",
              you, spellnoun);
        return 0;
    } else if (ACURR(A_STR) < 4) {
        pline("C{N=%s,V{V{lack},N{N{strength},V{V{cast},N{*,N{o,spell}}}}}}.",
              you);
        return 0;
    } else if (check_capacity(buf)) {
        return 1;
    } else if (!freehand()) {
        /* should this be body-parted? */
        pline("C{-,C{N{o,N{*,N{arm}},N=%s},V{V{are},A{A{free},V{cast}}}}}.",
              you);
        return 0;
    }

    if (u.uhave.amulet) {
        pline("C{N=%s,V{V{feel},N{N{Amulet},V{V{drain away},"
              "N{o,N{o,energy},N=%s}}}}}.", you, you);
        energy += rnd(2 * energy);
    }
    if (energy > u.uen) {
        pline("C{-,C{N=%s,V{V{V{have},N{N{o,energy},A{enough}}},"
              "V{V{cast},N=%s}}}}.", you, spellnoun);
        return 0;
    } else {
        if (spellid(spell) != SPE_DETECT_FOOD) {
            int hungr = energy * 2;

            /* If hero is a wizard, their current intelligence (bonuses +
               temporary + current) affects hunger reduction in casting a
               spell. 1. int = 17-18 no reduction 2. int = 16 1/4 hungr 3. int
               = 15 1/2 hungr 4. int = 1-14 normal reduction The reason for
               this is: a) Intelligence affects the amount of exertion in
               thinking. b) Wizards have spent their life at magic and
               understand quite well how to cast spells. */
            intell = acurr(A_INT);
            if (!Role_if(PM_WIZARD))
                intell = 10;
            switch (intell) {
            case 25:
            case 24:
            case 23:
            case 22:
            case 21:
            case 20:
            case 19:
            case 18:
            case 17:
                hungr = 0;
                break;
            case 16:
                hungr /= 4;
                break;
            case 15:
                hungr /= 2;
                break;
            }
            /* don't put player (quite) into fainting from casting a spell,
               particularly since they might not even be hungry at the
               beginning; however, this is low enough that they must eat before
               casting anything else except detect food */
            if (hungr > u.uhunger - 3)
                hungr = u.uhunger - 3;
            morehungry(hungr);
        }
    }

    chance = percent_success(spell);
    if (confused || (rnd(100) > chance)) {
        pline("C{N=%s,V{V{fail},V{V{V{cast},N{spell}},D{correctly}}}}.", you);
        u.uen -= energy / 2;
        iflags.botl = 1;
        return 1;
    }

    u.uen -= energy;
    iflags.botl = 1;
    exercise(A_WIS, TRUE);
    /* pseudo is a temporary 'false' object containing the spell stats */
    pseudo = mksobj(level, spellid(spell), FALSE, FALSE);
    pseudo->blessed = pseudo->cursed = 0;
    pseudo->quan = 20L; /* do not let useup get it */
    /* 
     * Find the skill the hero has in a spell type category.
     * See spell_skilltype for categories.
     */
    skill = spell_skilltype(pseudo->otyp);
    role_skill = P_SKILL(skill);

    switch (pseudo->otyp) {
        /* 
         * At first spells act as expected.  As the hero increases in skill
         * with the appropriate spell type, some spells increase in their
         * effects, e.g. more damage, further distance, and so on, without
         * additional cost to the spellcaster.
         */
    case SPE_CONE_OF_COLD:
    case SPE_FIREBALL:
        if (role_skill >= P_SKILLED) {
            if (throwspell(&dx, &dy)) {
                dz = 0;
                cc.x = dx;
                cc.y = dy;
                n = rnd(8) + 1;
                while (n--) {
                    if (!dx && !dy && !dz) {
                        if ((damage = zapyourself(pseudo, TRUE)) != 0) {
                            char buf[BUFSZ];

                            sprintf(buf, "V{V{V{zap},N=%s},D{t,N{i,spell}}}",
                                    you);
                            losehp(damage, buf, NO_KILLER_PREFIX);
                        }
                    } else {
                        explode(dx, dy, pseudo->otyp - SPE_MAGIC_MISSILE + 10,
                                u.ulevel / 2 + 1 + spell_damage_bonus(), 0,
                                (pseudo->otyp ==
                                 SPE_CONE_OF_COLD) ? EXPL_FROSTY : EXPL_FIERY);
                    }
                    dx = cc.x + rnd(3) - 2;
                    dy = cc.y + rnd(3) - 2;
                    if (!isok(dx, dy) || !cansee(dx, dy) ||
                        IS_STWALL(level->locations[dx][dy].typ) || u.uswallow) {
                        /* Spell is reflected back to center */
                        dx = cc.x;
                        dy = cc.y;
                    }
                }
            }
            break;
        }

        /* else fall through... */
        /* these spells are all duplicates of wand effects */
    case SPE_FORCE_BOLT:
    case SPE_SLEEP:
    case SPE_MAGIC_MISSILE:
    case SPE_KNOCK:
    case SPE_SLOW_MONSTER:
    case SPE_WIZARD_LOCK:
    case SPE_DIG:
    case SPE_TURN_UNDEAD:
    case SPE_POLYMORPH:
    case SPE_TELEPORT_AWAY:
    case SPE_CANCELLATION:
    case SPE_FINGER_OF_DEATH:
    case SPE_LIGHT:
    case SPE_DETECT_UNSEEN:
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
    case SPE_DRAIN_LIFE:
    case SPE_STONE_TO_FLESH:
        if (objects[pseudo->otyp].oc_dir != NODIR) {
            if (atme)
                dx = dy = dz = 0;
            else if (!getdir(NULL, &dx, &dy, &dz)) {
                /* getdir cancelled, generate a random direction */
                dz = 0;
                confdir(&dx, &dy);
                pline("C{s,V{V{release},N{N{energy},A{magical}}}}!");
            }
            if (!dx && !dy && !dz) {
                if ((damage = zapyourself(pseudo, TRUE)) != 0) {
                    char buf[BUFSZ];

                    sprintf(buf, "V{V{V{zap},N=%s},D{t,N{i,spell}}}", you);
                    losehp(damage, buf, NO_KILLER_PREFIX);
                }
            } else
                weffects(pseudo, dx, dy, dz);
        } else
            weffects(pseudo, 0, 0, 0);
        update_inventory();     /* spell may modify inventory */
        break;

        /* these are all duplicates of scroll effects */
    case SPE_REMOVE_CURSE:
    case SPE_CONFUSE_MONSTER:
    case SPE_DETECT_FOOD:
    case SPE_CAUSE_FEAR:
        /* high skill yields effect equivalent to blessed scroll */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
        /* fall through */
    case SPE_CHARM_MONSTER:
    case SPE_MAGIC_MAPPING:
    case SPE_CREATE_MONSTER:
    case SPE_IDENTIFY:
        seffects(pseudo, &dummy);
        break;

        /* these are all duplicates of potion effects */
    case SPE_HASTE_SELF:
    case SPE_DETECT_TREASURE:
    case SPE_DETECT_MONSTERS:
    case SPE_LEVITATION:
    case SPE_RESTORE_ABILITY:
        /* high skill yields effect equivalent to blessed potion */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
        /* fall through */
    case SPE_INVISIBILITY:
        peffects(pseudo);
        break;

    case SPE_CURE_BLINDNESS:
        healup(0, 0, FALSE, TRUE);
        break;
    case SPE_CURE_SICKNESS:
        if (Sick)
            pline("C{N=%s,V{V{are},A{A{ill},D{-,D{longer}}}}}.", you);
        if (Slimed) {
            pline("C{N{slime},V{disappear}}!");
            Slimed = 0;
            /* iflags.botl = 1; -- healup() handles this */
        }
        healup(0, 0, TRUE, FALSE);
        break;
    case SPE_CREATE_FAMILIAR:
        make_familiar(NULL, u.ux, u.uy, FALSE);
        break;
    case SPE_CLAIRVOYANCE:
        if (!BClairvoyant)
            do_vicinity_map();
        /* at present, only one thing blocks clairvoyance */
        else if (uarmh && uarmh->otyp == CORNUTHAUM)
            pline("C{N=%s,V{V{sense},N{N{N{i,hat},A{pointy}},"
                  "E{on top of},N{o,N=%s,N=%s}}}}.", you, body_part(HEAD), you);
        break;
    case SPE_PROTECTION:
        cast_protection();
        break;
    case SPE_JUMPING:
        if (!jump(max(role_skill, 1)))
            pline("C{N{o,nothing},V{happen}}.");
        break;
    default:
        impossible("S{Unknown spell %d attempted.}", spell);
        obfree(pseudo, NULL);
        return 0;
    }

    /* gain skill for successful cast */
    use_skill(skill, spellev(spell));

    obfree(pseudo, NULL);       /* now, get rid of it */
    return 1;
}

/* Choose location where spell takes effect. */
static int
throwspell(schar * dx, schar * dy)
{
    coord cc;

    if (u.uinwater) {
        pline("C{c,N=%s,V{joke}}!", you);
        pline("S{In this weather?}"); /* too much elided to spell it out */
        return 0;
    } else if (Is_waterlevel(&u.uz)) {
        pline("C{N=%s,V{V{had better},V{V{V{wait for},N{sun}},V{come out}}}}.",
              you);
        return 0;
    }

    pline("C{q,C{N=%s,V{V{want},V{V{V{cast},N{spell}},N{where}}}}}?", you);
    cc.x = u.ux;
    cc.y = u.uy;
    if (getpos(&cc, TRUE, "N{N{position},A{desired}}") < 0)
        return 0;       /* user pressed ESC */
    /* The number of moves from hero to where the spell drops. */
    if (distmin(u.ux, u.uy, cc.x, cc.y) > 10) {
        pline("C{N{spell},V{V{dissipate over},N{distance}}}!");
        return 0;
    } else if (u.uswallow) {
        pline("C{N{spell},V{V{are},A{cut short}}}!");
        exercise(A_WIS, FALSE); /* What were you THINKING! */
        *dx = 0;
        *dy = 0;
        return 1;
    } else
        if ((!cansee(cc.x, cc.y) &&
             (!MON_AT(level, cc.x, cc.y) ||
              !canspotmon(m_at(level, cc.x, cc.y)))) ||
            IS_STWALL(level->locations[cc.x][cc.y].typ)) {
            pline("C{N{o,N{mind},N=%s},V{V{fail},V{V{lock on to},"
                  "N{x%dy%d|location}}}}!", you, cc.x, cc.y);
        return 0;
    } else {
        *dx = cc.x;
        *dy = cc.y;
        return 1;
    }
}

void
losespells(void)
{
    boolean confused = (Confusion != 0);
    int n, nzap, i;

    book = 0;
    for (n = 0; n < MAXSPELL && spellid(n) != NO_SPELL; n++)
        continue;
    if (n) {
        nzap = rnd(n) + confused ? 1 : 0;
        if (nzap > n)
            nzap = n;
        for (i = n - nzap; i < n; i++) {
            spellid(i) = NO_SPELL;
            exercise(A_WIS, FALSE);     /* ouch! */
        }
    }
}

/* the '+' command -- view known spells */
int
dovspell(void)
{
    char qbuf[QBUFSZ];
    int splnum, othnum;
    struct spell spl_tmp;

    if (spellid(0) == NO_SPELL)
        pline("C{-,C{N=%s,V{V{V{know},"
              "N{N{*,N{o,spell}},A{any}}},D{right now}}}}.", you);
    else {
        while (dospellmenu("N{N{o,spells},A{A{known},D{currently}}}",
                           SPELLMENU_VIEW, &splnum)) {
            sprintf(qbuf, "C{i,V{V{reorder},N{*,N{o,spell}}}}: "
                    "C{q,C{i,V{V{V{swap},N{m,S{%c}}},D{t,N{what}}}}}?",
                    spellet(splnum));
            if (!dospellmenu(qbuf, splnum, &othnum))
                break;

            spl_tmp = spl_book[splnum];
            spl_book[splnum] = spl_book[othnum];
            spl_book[othnum] = spl_tmp;
        }
    }
    return 0;
}

static boolean
dospellmenu(const char *prompt,
            int splaction,  /* SPELLMENU_CAST, SPELLMENU_VIEW, or
                               spl_book[] index */
            int *spell_no)
{
    int i, n, how, count = 0;
    char buf[BUFSZ];
    struct nh_menuitem items[MAXSPELL + 1];
    int selected[MAXSPELL + 1];

    sprintf(buf, "N{o,name}\tN{o,level}\tN{o,category}\t"
            "N{o,fail}\tN{o,memory}");
    set_menuitem(&items[count++], 0, MI_HEADING, buf, 0, FALSE);
    for (i = 0; i < MAXSPELL && spellid(i) != NO_SPELL; i++) {
        sprintf(buf, "N=%s\tS{%d}S=%s\tN=%s\tS{%d%%}\tS{%d%%}",
                spellname(i), spellev(i),
                spellknow(i) ? "S{ }" : "S{*}",
                spelltypemnemonic(spell_skilltype(spellid(i))),
                100 - percent_success(i),
                (spellknow(i) * 100 + (KEEN - 1)) / KEEN);

        set_menuitem(&items[count++], i + 1, MI_NORMAL, buf, 0,
                     (i == splaction) ? TRUE : FALSE);
    }

    how = PICK_ONE;
    if (splaction == SPELLMENU_VIEW && spellid(1) == NO_SPELL)
        how = PICK_NONE;        /* only one spell => nothing to swap with */
    n = display_menu(items, count, prompt, how, PLHINT_ANYWHERE, selected);
    if (n > 0) {
        *spell_no = selected[0] - 1;
        /* menu selection for `PICK_ONE' does not de-select any preselected
           entry */
        if (n > 1 && *spell_no == splaction)
            *spell_no = selected[1] - 1;
        /* default selection of preselected spell means that user chose not to
           swap it with anything */
        if (*spell_no == splaction)
            return FALSE;
        return TRUE;
    } else if (splaction >= 0) {
        /* explicit de-selection of preselected spell means that user is still
           swapping but not for the current spell */
        *spell_no = splaction;
        return TRUE;
    }
    return FALSE;
}


void
dump_spells(void)
{
    /* note: the actual dumping is done in dump_display_menu(), we just need to 
       get the data there. */
    dospellmenu("N{N{*,N{i,spell}},A{V{V{know},D{m,N{end}}}}}:",
                SPELLMENU_VIEW, NULL);
}


/* Integer square root function without using floating point. */
static int
isqrt(int val)
{
    int rt = 0;
    int odd = 1;

    while (val >= odd) {
        val = val - odd;
        odd = odd + 2;
        rt = rt + 1;
    }
    return rt;
}

static int
percent_success(int spell)
{
    /* Intrinsic and learned ability are combined to calculate the probability
       of player's success at cast a given spell. */
    int chance, splcaster, special, statused;
    int difficulty;
    int skill;

    /* Calculate intrinsic ability (splcaster) */

    splcaster = urole.spelbase;
    special = urole.spelheal;
    statused = ACURR(urole.spelstat);

    if (uarm && is_metallic(uarm))
        splcaster += (uarmc &&
                      uarmc->otyp ==
                      ROBE) ? urole.spelarmr / 2 : urole.spelarmr;
    else if (uarmc && uarmc->otyp == ROBE)
        splcaster -= urole.spelarmr;
    if (uarms)
        splcaster += urole.spelshld;

    if (uarmh && is_metallic(uarmh) && uarmh->otyp != HELM_OF_BRILLIANCE)
        splcaster += uarmhbon;
    if (uarmg && is_metallic(uarmg))
        splcaster += uarmgbon;
    if (uarmf && is_metallic(uarmf))
        splcaster += uarmfbon;

    if (spellid(spell) == urole.spelspec)
        splcaster += urole.spelsbon;


    /* `healing spell' bonus */
    if (spellid(spell) == SPE_HEALING || spellid(spell) == SPE_EXTRA_HEALING ||
        spellid(spell) == SPE_CURE_BLINDNESS ||
        spellid(spell) == SPE_CURE_SICKNESS ||
        spellid(spell) == SPE_RESTORE_ABILITY ||
        spellid(spell) == SPE_REMOVE_CURSE)
        splcaster += special;

    if (splcaster > 20)
        splcaster = 20;

    /* Calculate learned ability */

    /* Players basic likelihood of being able to cast any spell is based of
       their `magic' statistic. (Int or Wis) */
    chance = 11 * statused / 2;

    /* 
     * High level spells are harder.  Easier for higher level casters.
     * The difficulty is based on the hero's level and their skill level
     * in that spell type.
     */
    skill = P_SKILL(spell_skilltype(spellid(spell)));
    skill = max(skill, P_UNSKILLED) - 1;        /* unskilled => 0 */
    difficulty = (spellev(spell) - 1) * 4 - ((skill * 6) + (u.ulevel / 3) + 1);

    if (difficulty > 0) {
        /* Player is too low level or unskilled. */
        chance -= isqrt(900 * difficulty + 2000);
    } else {
        /* Player is above level.  Learning continues, but the law of
           diminishing returns sets in quickly for low-level spells.  That is,
           a player quickly gains no advantage for raising level. */
        int learning = 15 * -difficulty / spellev(spell);

        chance += learning > 20 ? 20 : learning;
    }

    /* Clamp the chance: >18 stat and advanced learning only help to a limit,
       while chances below 'hopeless' only raise the specter of overflowing
       16-bit ints (and permit wearing a shield to raise the chances :-). */
    if (chance < 0)
        chance = 0;
    if (chance > 120)
        chance = 120;

    /* Wearing anything but a light shield makes it very awkward to cast a
       spell.  The penalty is not quite so bad for the player's role-specific
       spell. */
    if (uarms && weight(uarms) > (int)objects[SMALL_SHIELD].oc_weight) {
        if (spellid(spell) == urole.spelspec) {
            chance /= 2;
        } else {
            chance /= 4;
        }
    }

    /* Finally, chance (based on player intell/wisdom and level) is combined
       with ability (based on player intrinsics and encumbrances).  No matter
       how intelligent/wise and advanced a player is, intrinsics and
       encumbrance can prevent casting; and no matter how able, learning is
       always required. */
    chance = chance * (20 - splcaster) / 15 - splcaster;

    /* Clamp to percentile */
    if (chance > 100)
        chance = 100;
    if (chance < 0)
        chance = 0;

    return chance;
}


/* Learn a spell during creation of the initial inventory */
void
initialspell(struct obj *obj)
{
    int i;

    for (i = 0; i < MAXSPELL; i++) {
        if (spellid(i) == obj->otyp) {
            pline("S{Error: Spell %s already known.}",
                  OBJ_NAME(objects[obj->otyp]));
            return;
        }
        if (spellid(i) == NO_SPELL) {
            spl_book[i].sp_id = obj->otyp;
            spl_book[i].sp_lev = objects[obj->otyp].oc_level;
            incrnknow(i);
            return;
        }
    }
    impossible("S{Too many spells memorized!}");
    return;
}

/*spell.c*/
