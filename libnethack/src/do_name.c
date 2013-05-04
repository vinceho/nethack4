/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

struct monst *
christen_monst(struct monst *mtmp, const char *name)
{
    int lth;
    struct monst *mtmp2;
    char buf[PL_PSIZ];

    /* dogname & catname are PL_PSIZ arrays; object names have same limit */
    lth = *name ? (int)(strlen(name) + 1) : 0;
    if (lth > PL_PSIZ) {
        lth = PL_PSIZ;
        name = strncpy(buf, name, PL_PSIZ - 1);
        buf[PL_PSIZ - 1] = '\0';
    }
    if (lth == mtmp->mnamelth) {
        /* don't need to allocate a new monst struct */
        if (lth)
            strcpy(NAME(mtmp), name);
        return mtmp;
    }
    mtmp2 = newmonst(mtmp->mxtyp, lth);
    *mtmp2 = *mtmp;
    memcpy(mtmp2->mextra, mtmp->mextra, mtmp->mxlth);
    mtmp2->mnamelth = lth;
    if (lth)
        strcpy(NAME(mtmp2), name);
    replmon(mtmp, mtmp2);
    return mtmp2;
}


int
do_mname(void)
{
    char buf[BUFSZ];
    coord cc;
    int cx, cy;
    struct monst *mtmp;
    char qbuf[QBUFSZ];

    if (Hallucination) {
        pline("C{-,C{N=%s,V{V{can},V{V{tell apart},N{*,N{i,monster}}}}}}!", you);
        return 0;
    }
    cc.x = u.ux;
    cc.y = u.uy;
    /* literal 'you' is OK in getpos */
    if (getpos(&cc, FALSE,
               "N{s,C{N{you},V{V{want},V{V{name},N{monster}}}}}") < 0 ||
        (cx = cc.x) < 0)
        return 0;
    cy = cc.y;

    if (cx == u.ux && cy == u.uy) {
        if (u.usteed && canspotmon(u.usteed))
            mtmp = u.usteed;
        else {
            pline("C{+,C{s,V{V{V{call},N{m,S{\x1c%s\x1c}}},"
                  "N{N{%c,x%dy%d|creature},A=%s}}},"
                  "C{-,C{N{%c,x%dy%d|creature},V{s,V{can},V{rename}}}}}.",
                  plname, flags.female ? 'f' : 'm', u.ux, u.uy,
                  ACURR(A_CHA) > 14 ?
                  (flags.female ? "A{beautiful}" : "A{handsome}") :
                  "A{ugly}", flags.female ? 'f' : 'm', u.ux, u.uy);
            return 0;
        }
    } else
        mtmp = m_at(level, cx, cy);

    if (!mtmp ||
        (!sensemon(mtmp) &&
         (!(cansee(cx, cy) || see_with_infrared(mtmp)) || mtmp->mundetected ||
          mtmp->m_ap_type == M_AP_FURNITURE || mtmp->m_ap_type == M_AP_OBJECT ||
          (mtmp->minvis && !See_invisible)))) {
        pline("C{-,C{N{I},V{V{V{see},N{i,monster}},D{there}}}}.");
        return 0;
    }
    /* special case similar to the one in lookat() */
    distant_monnam(mtmp, ARTICLE_THE, buf);
    sprintf(qbuf, "C{q,C{N=%s,V{V{want},V{V{V{call},N=%s},N{what}}}}}?",
            you, buf);
    getlin(qbuf, buf);
    if (!*buf || *buf == '\033')
        return 0;
    /* strip leading and trailing spaces; unnames monster if all spaces */
    mungspaces(buf);

    if (mtmp->data->geno & G_UNIQ) {
        distant_monnam(mtmp, ARTICLE_THE, buf);
        pline("C{-,C{N=%s,V{s,V{like},V{V{call},N{*,N{i,name}}}}}}!", buf);
    } else
        christen_monst(mtmp, buf);
    return 0;
}

/* all but coins */
static const char nameable[] = {
    SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS, AMULET_CLASS,
    GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS, WEAPON_CLASS,
    ROCK_CLASS, CHAIN_CLASS, BALL_CLASS, 0
};

/*
 * This routine changes the address of obj. Be careful not to call it
 * when there might be pointers around in unknown places. For now: only
 * when obj is in the inventory.
 */
int
do_oname(struct obj *obj)
{
    char buf[BUFSZ], qbuf[QBUFSZ];
    const char *aname;
    short objtyp;

    if (!obj)
        obj = getobj(nameable, "V{name}");
    if (!obj)
        return 0;

    sprintf(qbuf, "C{q,C{N=%s,V{V{want},V{V{V{name},N=%s},N{what}}}}}?",
            you, xname(obj));

    getlin(qbuf, buf);
    if (!*buf || *buf == '\033')
        return 0;
    /* strip leading and trailing spaces; unnames item if all spaces */
    mungspaces(buf);

    /* relax restrictions over proper capitalization for artifacts */
    if ((aname = artifact_name(buf, &objtyp)) != 0 && objtyp == obj->otyp)
        strcpy(buf, aname);

    if (obj->oartifact) {
        pline("C{N{artifact},V{V{seem},V{V{resist},N{attempt}}}}.");
        return 0;
    } else if (restrict_name(obj, buf) || exist_artifact(obj->otyp, buf)) {
        int n = rn2((int)strlen(buf));
        char c1, c2;

        c1 = lowc(buf[n]);
        do
            c2 = 'a' + rn2('z' - 'a' + 1);
        while (c1 == c2);
        pline("C{N{o,N=%s,N=%s},V{V{slip},D{Q{while},C{N=%s,V{engrave}}}}}.",
              body_part(HAND), you, you);
    }
    oname(obj, buf);
    return 0;
}

/*
 * Allocate a new and possibly larger storage space for an obj.
 */
struct obj *
realloc_obj(struct obj *obj, int oextra_size, void *oextra_src, int oname_size,
            const char *name)
{
    struct obj *otmp;

    otmp = newobj(oextra_size + oname_size);
    *otmp = *obj;       /* the cobj pointer is copied to otmp */
    if (oextra_size) {
        if (oextra_src)
            memcpy(otmp->oextra, oextra_src, oextra_size);
    } else {
        otmp->oattached = OATTACHED_NOTHING;
    }
    otmp->oxlth = oextra_size;
    otmp->onamelth = oname_size;

    if (oname_size) {
        if (name)
            strcpy(ONAME(otmp), name);
    }

    /* !obj->olev means the obj is currently being restored and no pointer from 
       or to it is valid. Re-equipping, timer linking, etc. will happen
       elsewhere in that case. */
    if (obj->olev) {
        if (obj->owornmask) {
            boolean save_twoweap = u.twoweap;

            /* unwearing the old instance will clear dual-wield mode if this
               object is either of the two weapons */
            setworn(NULL, obj->owornmask);
            setworn(otmp, otmp->owornmask);
            u.twoweap = save_twoweap;
        }

        /* replace obj with otmp */
        replace_object(obj, otmp);

        /* fix ocontainer pointers */
        if (Has_contents(obj)) {
            struct obj *inside;

            for (inside = obj->cobj; inside; inside = inside->nobj)
                inside->ocontainer = otmp;
        }

        /* move timers and light sources from obj to otmp */
        otmp->timed = 0;        /* not timed, yet */
        if (obj->timed)
            obj_move_timers(obj, otmp);
        otmp->lamplit = 0;      /* ditto */
        if (obj->lamplit)
            obj_move_light_source(obj, otmp);

        /* objects possibly being manipulated by multi-turn occupations which
           have been interrupted but might be subsequently resumed */
        if (obj->oclass == FOOD_CLASS)
            food_substitution(obj, otmp);       /* eat food or open tin */
        else if (obj->oclass == SPBOOK_CLASS)
            book_substitution(obj, otmp);       /* read spellbook */
    } else {
        /* make sure dealloc_obj doesn't explode */
        obj->where = OBJ_FREE;
        obj->timed = FALSE;
        obj->lamplit = FALSE;
    }
    /* obfree(obj, otmp); now unnecessary: no pointers on bill */
    dealloc_obj(obj);   /* let us hope nobody else saved a pointer */
    return otmp;
}

struct obj *
oname(struct obj *obj, const char *name)
{
    int lth;
    char buf[PL_PSIZ];

    lth = *name ? (int)(strlen(name) + 1) : 0;
    if (lth > PL_PSIZ) {
        lth = PL_PSIZ;
        name = strncpy(buf, name, PL_PSIZ - 1);
        buf[PL_PSIZ - 1] = '\0';
    }
    /* If named artifact exists in the game, do not create another. Also trying 
       to create an artifact shouldn't de-artifact it (e.g. Excalibur from
       prayer). In this case the object will retain its current name. */
    if (obj->oartifact || (lth && exist_artifact(obj->otyp, name)))
        return obj;

    if (lth == obj->onamelth) {
        /* no need to replace entire object */
        if (lth)
            strcpy(ONAME(obj), name);
    } else {
        obj = realloc_obj(obj, obj->oxlth, obj->oextra, lth, name);
    }
    if (lth)
        artifact_exists(obj, name, TRUE);
    if (obj->oartifact) {
        /* can't dual-wield with artifact as secondary weapon */
        if (obj == uswapwep)
            untwoweapon();
        /* activate warning if you've just named your weapon 'Sting' */
        if (obj == uwep)
            set_artifact_intrinsic(obj, TRUE, W_WEP);
    }
    if (carried(obj))
        update_inventory();
    return obj;
}

static void
docall_inner(int otyp)
{
    char buf[BUFSZ], qbuf[QBUFSZ];
    char **str1;

    /* to help make it more intuitive what Cn does, we pluralize:
       'Call orange potions:' */
    sprintf(qbuf, "C{i,V{V{call},N{*,N=%s}}}@:", simple_typename(otyp));
    getlin(qbuf, buf);
    if (!*buf || *buf == '\033')
        return;

    /* clear old name */
    str1 = &(objects[otyp].oc_uname);
    if (*str1)
        free(*str1);

    /* strip leading and trailing spaces; uncalls item if all spaces */
    (void)mungspaces(buf);
    if (!*buf) {
        if (*str1) {    /* had name, so possibly remove from disco[] */
            /* strip name first, for the update_inventory() call from
               undiscover_object() */
            *str1 = (char *)0;
            undiscover_object(otyp);
        }
    } else {
        *str1 = strcpy((char *)malloc((unsigned)strlen(buf) + 1), buf);
        discover_object(otyp, FALSE, TRUE, TRUE); /* possibly add to disco[] */
    }
}

static const char callable[] = {
    SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS, AMULET_CLASS,
    GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS, 0
};

int
do_tname(struct obj *obj)
{
    if (!obj)
        obj = getobj(callable, "V{call}");
    if (obj) {
        /* behave as if examining it in inventory; this might set dknown if 
           it was picked up while blind and the hero can now see */
        xname(obj);
        
        if (!obj->dknown) {
            pline("C{-,C{N=%s,V{V{can},V{V{V{see},N=%s},"
                  "D{p,V{compare}}}}}}.",
                  you, xname(obj));
            return 0;
        }
        docall_inner(obj->otyp);
    }
    return 0;
}

int
do_naming(void)
{
    int n, selected[1];
    char classes[20], *s;
    struct menulist menu;

    init_menulist(&menu);

    add_menuitem(&menu, 1, "C{i,V{V{name},N{i,monster}}}@", 'C', FALSE);
    add_menuitem(&menu, 2, "C{i,V{V{name},N{N{level},A{current}}}}@",
                 'f', FALSE);
    add_menuitem(&menu, 3, "C{i,V{V{name},N{N{i,item},A{individual}}}}@",
                 'y', FALSE);
    add_menuitem(&menu, 4, "C{i,V{V{name},"
                 "N{l,N{N{*,N{i,item}},A{all}},N{N{i,type},A{given}}}}}@",
                 'n', FALSE);
    add_menuitem(&menu, 5, "C{i,V{V{V{name},N{N{i,type},A{N{item}}}},"
                 "D{t,N{o,appearance}}}}@", 'A', FALSE);
    if (flags.recently_broken_otyp != STRANGE_OBJECT) {
        char buf[BUFSZ];

        sprintf(buf, "C{i,V{V{name},N{N{*,N{i,item}},"
                "D{E{like},N{N=%s,A{p,A{A{broken},D{recently}}}}}}}}@",
                obj_typename(flags.recently_broken_otyp));
        add_menuitem(&menu, 6, buf, 'V', FALSE);
    }

    n = display_menu(menu.items, menu.icount,
                     "C{q,C{N{you},V{V{wish},V{V{name},N{what}}}}}?",
                     PICK_ONE, PLHINT_ANYWHERE, selected);
    free(menu.items);
    if (n > 0)
        n = selected[0] - 1;
    else
        return 0;

    switch (n) {
    default:
        break;
    case 0:
        do_mname();
        break;

    case 1:
        donamelevel();
        break;

    case 2:
        do_oname(NULL);
        break;

    case 3:
        do_tname(NULL);
        break;

    case 4:
        strcpy(classes, flags.inv_order);
        init_menulist(&menu);
        /* This leaks information unless we put things in a consistent order.
           It's probably simplest to alphabetise. */
        for (s = classes; *s; s++) {
            int alphaorder[NUM_OBJECTS];
            int aop = 0;
            int i;

            if (*s != RING_CLASS && *s != AMULET_CLASS && *s != SCROLL_CLASS &&
                *s != POTION_CLASS && *s != WAND_CLASS && *s != SPBOOK_CLASS &&
                *s != ARMOR_CLASS)
                continue;
            for (n = bases[(int)*s];
                 n < NUM_OBJECTS && objects[n].oc_class == *s; n++) {
                if (!objects[n].oc_name_known && !objects[n].oc_unique &&
                    n != FAKE_AMULET_OF_YENDOR) {
                    if (*s != ARMOR_CLASS ||
                        (n >= HELMET && n <= HELM_OF_TELEPATHY) ||
                        (n >= LEATHER_GLOVES && n <= GAUNTLETS_OF_DEXTERITY) ||
                        (n >= CLOAK_OF_PROTECTION && n <= CLOAK_OF_DISPLACEMENT)
                        || (n >= SPEED_BOOTS && n <= LEVITATION_BOOTS))
                        alphaorder[aop++] = n;
                }
            }
            for (n = 0; n < aop; n++) {
                for (i = n + 1; i < aop; i++) {
                    if (strcmp
                        (OBJ_DESCR(objects[alphaorder[i]]),
                         OBJ_DESCR(objects[alphaorder[n]])) < 0) {
                        int t = alphaorder[i];

                        alphaorder[i] = alphaorder[n];
                        alphaorder[n] = t;
                    }
                }
            }
            for (i = 0; i < aop; i++) {
                add_menuitem(&menu, alphaorder[i], obj_typename(alphaorder[i]),
                             0, FALSE);
            }
        }
        n = display_menu(menu.items, menu.icount,
                         "C{q,C{i,V{V{name},N{N{*,N{i,item}},"
                         "D{E{with},N{N{o,appearance},A{which}}}}}}}?",
                         PICK_ONE, PLHINT_INVENTORY, selected);
        free(menu.items);
        if (n == 1)
            docall_inner(selected[0]);
        break;

    case 5:
        docall_inner(flags.recently_broken_otyp);
        break;
    }
    return 0;
}


void
docall(struct obj *obj)
{
    char buf[BUFSZ];
    struct obj otemp;

    if (!obj->dknown)
        return; /* probably blind */
    otemp = *obj;
    otemp.quan = 1L;
    otemp.onamelth = 0;
    otemp.oxlth = 0;
    if (objects[otemp.otyp].oc_class == POTION_CLASS && otemp.fromsink)
        /* kludge, meaning it's sink water */
        sprintf(buf, "(C{N=%s,V{V{V{can},V{V{name},"
                "N{f,N{i,stream},N{N{o,fluid},A=%s}}}},"
                "D{t,N{N{menu},A{N{N{naming},A{N{item}}}}}}}}@).",
                you, OBJ_DESCR(objects[otemp.otyp]));
    else
        sprintf(buf, "(C{N=%s,V{V{V{can},V{V{name},N=%s}},"
                "D{t,N{N{menu},A{N{N{naming},A{N{item}}}}}}}}@).",
                you, an(xname(&otemp)));
    pline(/*nointl*/"%s", buf);
    flags.recently_broken_otyp = otemp.otyp;
}


static const char *const ghostnames[] = {
    /* these names should have length < PL_NSIZ */
    /* Capitalize the names for aesthetics -dgk */
    "P{Adri}", "P{Andries}", "P{Andreas}", "P{Bert}", "P{David}", "P{Dirk}",
    "P{Emile}", "P{Frans}", "P{Fred}", "P{Greg}", "P{Hether}", "P{Jay}",
    "P{John}", "P{Jon}", "P{Karnov}", "P{Kay}", "P{Kenny}", "P{Kevin}",
    "P{Maud}", "P{Michiel}", "P{Mike}", "P{Peter}", "P{Robert}", "P{Ron}",
    "P{Tom}", "P{Wilmar}", "P{Nick Danger}", "P{Phoenix}", "P{Jiro}",
    "P{Mizue}", "P{Stephan}", "P{Lance Braccus}", "P{Shadowhawk}"
};

/* ghost names formerly set by x_monnam(), now by makemon() instead */
const char *
rndghostname(void)
{
    return rn2(7) ? ghostnames[rn2(SIZE(ghostnames))] : (const char *)plname;
}

const char *
the_your(const char *x, boolean your)
{
    static char buf[BUFSZ];
    if (!your) return x;
    sprintf(buf, "N{o,N=%s,N=%s}", x, you);
    return buf;
}

/* Monster naming functions:
 * x_monnam is the generic monster-naming function.
 *                seen        unseen       detected               named
 * mon_nam:     the newt        it      the invisible orc       Fido
 * noit_mon_nam:the newt (as if detected) the invisible orc     Fido
 * l_monnam:    newt            it      invisible orc           dog called fido
 * Monnam:      The newt        It      The invisible orc       Fido
 * noit_Monnam: The newt (as if detected) The invisible orc     Fido
 * Adjmonnam:   The poor newt   It      The poor invisible orc  The poor Fido
 * Amonnam:     A newt          It      An invisible orc        Fido
 * a_monnam:    a newt          it      an invisible orc        Fido
 * m_monnam:    newt            xan     orc                     Fido
 * y_monnam:    your newt     your xan  your invisible orc      Fido
 */

/* Bug: if the monster is a priest or shopkeeper, not every one of these
 * options works, since those are special cases.
 */
char *
x_monnam(const struct monst *mtmp,
         int article, /* ARTICLE_NONE, ARTICLE_THE, ARTICLE_A: obvious
                         ARTICLE_YOUR: 'your' on pets,'the' on
                         everything else */
         const char *adjective,
         int suppress,   /* SUPPRESS_IT, SUPPRESS_INVISIBLE,
                            SUPPRESS_HALLUCINATION,
                            SUPPRESS_SADDLE. EXACT_NAME: combination
                            of all the above */
         boolean called)
{
    static char buf[BUFSZ];

    const struct permonst *mdat = mtmp->data;
    boolean do_hallu, do_invis, do_it, do_saddle;
    char *bp, *sn;

    if (program_state.gameover)
        suppress |= SUPPRESS_HALLUCINATION;
    if (article == ARTICLE_YOUR && !mtmp->mtame)
        article = ARTICLE_THE;

    do_hallu = Hallucination && !(suppress & SUPPRESS_HALLUCINATION);
    do_invis = mtmp->minvis && !(suppress & SUPPRESS_INVISIBLE);
    do_it = !canspotmon(mtmp) && article != ARTICLE_YOUR &&
        !program_state.gameover && mtmp != u.usteed &&
        !(u.uswallow && mtmp == u.ustuck) &&
        !(suppress & SUPPRESS_IT);
    do_saddle = !(suppress & SUPPRESS_SADDLE);

    bp = buf + 12; /* leave room for 6 adjectives */
    bp[0] = 0;

    /* unseen monsters, etc. We have a marker for this nowadays. (It doesn't
       get a uniquifier for the time being so that the player can't track
       out-of-sight monsters via pronouns. We might want to change that,
       though; it'll just take careful consideration before we do.) */
    if (do_it) {
        strcpy(bp, "?{}");
        return bp;
    }

    /* priests and minions: don't even use this function */
    if (mtmp->ispriest || mtmp->isminion) {
        struct monst *priestmon = newmonst(mtmp->mxtyp, mtmp->mnamelth);
        char priestnambuf[BUFSZ];
        char *name;
        long save_prop = EHalluc_resistance;

        memcpy(priestmon, mtmp,
               sizeof (struct monst) + mtmp->mxlth + mtmp->mnamelth);

        /* when true name is wanted, explicitly block Hallucination */
        if (!do_hallu)
            EHalluc_resistance = 1L;
        if (!do_invis)
            priestmon->minvis = 0;
        name = priestname(priestmon, priestnambuf);
        EHalluc_resistance = save_prop;
        if (article == ARTICLE_NONE) name = remove_the(name);
        strcpy(bp, name);
        free(priestmon);
        return add_uniquifier(mtmp->m_id, bp);
    }

    /* Shopkeepers: use shopkeeper name.  For normal shopkeepers, just
       'Asidonhopo'; for unusual ones, 'Asidonhopo the invisible shopkeeper' or 
       'Asidonhopo the blue dragon'.  If hallucinating, none of this applies. */
    if (mtmp->isshk && !do_hallu) {
        /* 3.4.3 has a pathological case here that needs sorting out:
           'Asidonhopo'
           "P{Asidonhopo}"
           'Asidonhopo the blue dragon'
           "N{P{Asidonhopo},A{N{N{dragon},A{blue}}}}"
           'the angry Asidonhopo'
           "N{P{Asidonhopo},A{angry}}"
           'Asidonhopo the angry blue dragon'
           "N{N{P{Asidonhopo},A{N{N{dragon},A{blue}}}},A{angry}}"
           Instead of trying to handle this ourselves, we push it onto the
           grammar code. (Especially because languages other than English may
           use different rules.) */
        sn = shkname(mtmp);
        if (mdat == &mons[PM_SHOPKEEPER] && !do_invis) {
            strcpy(bp, sn);
            return add_uniquifier(mtmp->m_id, bp);
        }
        if (do_invis)
            sprintf(bp, "N{N{N=%s,A{N=%s}},A{invisible}}", sn, mdat->mname);
        else
            sprintf(bp, "N{N=%s,A{N=%s}}", sn, mdat->mname);
        return add_uniquifier(mtmp->m_id, bp);
    }

    /* Put the actual monster name or type into the buffer */
    if (do_hallu) {
        int idx = rndmonidx();

        strcpy(bp, monnam_for_index(idx));
    } else if (mtmp->mnamelth) {
        char *name = NAME(mtmp);

        if (mdat == &mons[PM_GHOST]) {
            sprintf(bp, /*nointl*/"N{o,N{ghost},P{\x1c%s\x1c}}", name);
        } else if (called) {
            sprintf(bp, "N{N=%s,C{n,p,V{V{call},N{m,S{\x1c%s\x1c}}}}}",
                    mdat->mname, name);
        } else if (is_mplayer(mdat)) {
            /* Player-monster names are written in grammartree, unlike
               other monster names which are written raw. */
            strcpy(bp, name);
        } else {
            sprintf(bp, /*nointl*/"P{\x1c%s\x1c}", name);
        }
    } else if (is_mplayer(mdat) && !In_endgame(&u.uz)) {
        strcpy(bp, rank_of((int)mtmp->m_lev, monsndx(mdat),
                           (boolean) mtmp->female));
    } else {
        strcpy(bp, mdat->mname);
    }

    /* Put the adjectives in the buffer */
    if (adjective) {
        *(bp--) = '{'; *(bp--) = 'N';
        sprintf(eos(bp), /*nointl*/",A=%s}", adjective);
    }
    if (do_invis) {
        *(bp--) = '{'; *(bp--) = 'N';
        strcat(buf, /*nointl*/",A{invisible}}");
    }

    if (do_saddle && (mtmp->misc_worn_check & W_SADDLE) &&
        !Blind && !Hallucination) {
        *(bp--) = '{'; *(bp--) = 'N';
        strcat(buf, /*nointl*/",A{V{saddle}}}");
    }

    if ((mdat->geno & G_UNIQ) && article == ARTICLE_A) {
        article = ARTICLE_THE;
    }

    switch (article) {
    case ARTICLE_YOUR:
        *(bp--) = ','; *(bp--) = 'o'; *(bp--) = '{'; *(bp--) = 'N';
        sprintf(eos(bp), /*nointl*/",N=%s}", you);
        /* fall through */
    case ARTICLE_THE:
        return add_uniquifier(mtmp->m_id, bp);
    case ARTICLE_A:
        return add_uniquifier(mtmp->m_id, an(bp));
    case ARTICLE_NONE:
    default:
        return add_uniquifier(mtmp->m_id, remove_the(bp));
    }
}


char *
l_monnam(const struct monst *mtmp)
{
    return (x_monnam
            (mtmp, ARTICLE_NONE, NULL, mtmp->mnamelth ? SUPPRESS_SADDLE : 0,
             TRUE));
}


char *
mon_nam(const struct monst *mtmp)
{
    return (x_monnam
            (mtmp, ARTICLE_THE, NULL, mtmp->mnamelth ? SUPPRESS_SADDLE : 0,
             FALSE));
}

/* print the name as if mon_nam() was called, but assume that the player
 * can always see the monster--used for probing and for monsters aggravating
 * the player with a cursed potion of invisibility
 */
char *
noit_mon_nam(const struct monst *mtmp)
{
    return (x_monnam
            (mtmp, ARTICLE_THE, NULL,
             mtmp->mnamelth ? (SUPPRESS_SADDLE | SUPPRESS_IT) : SUPPRESS_IT,
             FALSE));
}

/* TODO: Remove these once all uses are changed. */
char *
Monnam(const struct monst *mtmp)
{
    char *bp = mon_nam(mtmp);

    return bp;
}
char *
noit_Monnam(const struct monst *mtmp)
{
    char *bp = noit_mon_nam(mtmp);

    return bp;
}

/* monster's own name */
char *
m_monnam(const struct monst *mtmp)
{
    return x_monnam(mtmp, ARTICLE_NONE, NULL, EXACT_NAME, FALSE);
}

/* pet name: 'your little dog' */
char *
y_monnam(const struct monst *mtmp)
{
    int prefix, suppression_flag;

    prefix = mtmp->mtame ? ARTICLE_YOUR : ARTICLE_THE;
    /* 'saddled' is redundant when mounted */
    suppression_flag = (mtmp->mnamelth ||
                        mtmp == u.usteed) ? SUPPRESS_SADDLE : 0;

    return x_monnam(mtmp, prefix, NULL, suppression_flag, FALSE);
}


char *
Adjmonnam(const struct monst *mtmp, const char *adj)
{
    char *bp = x_monnam(mtmp, ARTICLE_THE, adj,
                        mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE);

    *bp = highc(*bp);
    return bp;
}

char *
a_monnam(const struct monst *mtmp)
{
    return x_monnam(mtmp, ARTICLE_A, NULL, mtmp->mnamelth ? SUPPRESS_SADDLE : 0,
                    FALSE);
}

char *
Amonnam(const struct monst *mtmp)
{
    char *bp = a_monnam(mtmp);

    return bp;
}

/* used for monster ID by the '/', ';', and 'C' commands to block remote
   identification of the endgame altars via their attending priests */
char *
distant_monnam(const struct monst *mon, int article, char *outbuf)
{
    /* high priest(ess)'s identity is concealed on the Astral Plane, unless
       you're adjacent (overridden for hallucination which does its own
       obfuscation) */
    if (mon->data == &mons[PM_HIGH_PRIEST] && !Hallucination &&
        Is_astralevel(&u.uz) && distu(mon->mx, mon->my) > 2) {
        strcpy(outbuf, mon->female ?
               "N{N{f,i,priestess},A{high^rank}}" :
               "N{N{m,i,priest},A{high^rank}}");
        /* Hack to change articles: repeat the gender if we really do want 'the'
           (normal article handling won't apply because we got rid of the
           uniquifiers) */
        if (article == ARTICLE_THE) outbuf[6] = outbuf[4];
        if (article == ARTICLE_NONE) outbuf[6] = 'o';
    } else {
        strcpy(outbuf, x_monnam(mon, article, NULL, 0, TRUE));
    }
    return outbuf;
}

/* Note: these are intentionally not grammartree'ised apart from N{} or P{}
   article tags. This is because this section's full of references which would
   become inaccurate if the grammar changed around even a bit. */
static struct {
    const char *name;
    const boolean pname;
} bogusmons[] = {
    /* misc. */
    {"N{jumbo shrimp}", FALSE},
    {"N{giant pigmy}", FALSE},
    {"N{gnu}", FALSE},
    {"N{killer penguin}", FALSE},
    {"N{giant cockroach}", FALSE},
    {"N{giant slug}", FALSE},
    {"N{maggot}", FALSE},
    {"N{pterodactyl}", FALSE},
    {"N{tyrannosaurus rex}", FALSE},
    {"N{basilisk}", FALSE},
    {"N{beholder}", FALSE},
    {"N{nightmare}", FALSE},
    {"N{efreeti}", FALSE},
    {"N{marid}", FALSE},
    {"N{rot grub}", FALSE},
    {"N{bookworm}", FALSE},
    {"N{master lichen}", FALSE},
    {"N{shadow}", FALSE},
    {"N{hologram}", FALSE},
    {"N{jester}", FALSE},
    {"N{attorney}", FALSE},
    {"N{sleazoid}", FALSE},
    {"N{killer tomato}", FALSE},
    {"N{amazon}", FALSE},
    {"N{robot}", FALSE},
    {"N{battlemech}", FALSE},
    {"N{rhinovirus}", FALSE},
    {"N{harpy}", FALSE},
    {"N{lion-dog}", FALSE},
    {"N{rat-ant}", FALSE},
    {"N{Y2K bug}", FALSE},
        /* Quendor (Zork, &c.) */
    {"N{grue}", FALSE},
    {"N{Christmas-tree monster}", FALSE},
    {"N{luck sucker}", FALSE},
    {"N{paskald}", FALSE},
    {"N{brogmoid}", FALSE},
    {"N{dornbeast}", FALSE},
        /* Moria */
    {"N{Ancient Multi-Hued Dragon}", FALSE},
    {"N{Evil Iggy}", FALSE},
        /* Rogue */
    {"N{emu}", FALSE},
    {"N{kestrel}", FALSE},
    {"N{xeroc}", FALSE},
    {"N{venus flytrap}", FALSE},
        /* Wizardry */
    {"N{creeping coins}", FALSE},
        /* Greek legend */
    {"N{hydra}", FALSE},
    {"N{siren}", FALSE},
        /* Monty Python */
    {"N{killer bunny}", FALSE},
        /* The Princess Bride */
    {"N{rodent of unusual size}", FALSE},
        /* 'Only you can prevent forest fires!' */
    {"P{Smokey the bear}", TRUE},
        /* Discworld */
    {"N{Luggage}", FALSE},
        /* Lord of the Rings */
    {"N{Ent}", FALSE},
        /* Xanth */
    {"N{tangle tree}", FALSE},
    {"N{nickelpede}", FALSE},
    {"N{wiggle}", FALSE},
        /* Lewis Carroll */
    {"N{white rabbit}", FALSE},
    {"N{snark}", FALSE},
        /* Dr. Dolittle */
    {"N{pushmi-pullyu}", FALSE},
        /* The Smurfs */
    {"N{smurf}", FALSE},
        /* Star Trek */
    {"N{tribble}", FALSE},
    {"N{Klingon}", FALSE},
    {"N{Borg}", FALSE},
        /* Star Wars */
    {"N{Ewok}", FALSE},
        /* Tonari no Totoro */
    {"N{Totoro}", FALSE},
        /* Nausicaa */
    {"N{ohmu}", FALSE},
        /* Sailor Moon */
    {"N{youma}", FALSE},
        /* Pokemon (Meowth) */
    {"N{nyaasu}", FALSE},
        /* monster movies */
    {"P{Godzilla}", TRUE},
    {"P{King} Kong", TRUE},
        /* old L of SH */
    {"N{earthquake beast}", FALSE},
        /* Robotech */
    {"N{Invid}", FALSE},
        /* The Terminator */
    {"N{Terminator}", FALSE},
        /* Bubblegum Crisis */
    {"N{boomer}", FALSE},
        /* Dr. Who ('Exterminate!') */
    {"N{Dalek}", FALSE},
        /* Hitchhiker's Guide to the Galaxy */
    {"N{microscopic space fleet}", FALSE},
    {"N{Ravenous Bugblatter Beast of Traal}", FALSE},
        /* TMNT */
    {"N{teenage mutant ninja turtle}", FALSE},
        /* Usagi Yojimbo */
    {"N{samurai rabbit}", FALSE},
        /* Cerebus */
    {"N{aardvark}", FALSE},
        /* Little Shop of Horrors */
    {"P{Audrey II}", TRUE},
        /* 50's rock 'n' roll */
    {"N{witch doctor}", FALSE},
    {"N{one-eyed one-horned flying purple people eater}", FALSE},
        /* saccharine kiddy TV */
    {"P{Barney the dinosaur}", TRUE},
        /* Angband */
    {"P{Morgoth}", TRUE},
        /* Babylon 5 */
    {"N{Vorlon}", FALSE},
        /* King Arthur */
    {"N{questing beast}", FALSE},
        /* Movie */
    {"N{Predator}", FALSE},
        /* common pest */
    {"N{mother-in-law}", FALSE},
        /* Battlestar Galactica */
    {"N{cylon}", FALSE},
};

/* Return a random monster name, for hallucination. */
int
rndmonidx(void)
{
    int idx;

    do {
        idx = display_rng(SPECIAL_PM + SIZE(bogusmons) - LOW_PM) + LOW_PM;
    } while (idx < SPECIAL_PM &&
             (type_is_pname(&mons[idx]) || (mons[idx].geno & G_NOGEN)));

    return idx;
}

const char *
monnam_for_index(int idx)
{
    if (idx >= SPECIAL_PM)
        return (bogusmons[idx - SPECIAL_PM].name);
    return mons[idx].mname;
}

boolean
monnam_is_pname(int idx)
{
    if (idx >= SPECIAL_PM)
        return (bogusmons[idx - SPECIAL_PM].pname);
    return type_is_pname(&mons[idx]);
}

const char *
roguename(void)
{       /* Name of a Rogue player */
    char *i, *opts;

    /* The strings here are nointl because they're used by Rogue, not (usually)
       by NetHack; we're checking for the name of a Rogue player by seeing if
       the current account has Rogue options set. */
    if ((opts = nh_getenv(/*nointl*/"ROGUEOPTS")) != 0) {
        for (i = opts; *i; i++)
            if (!strncmp(/*nointl*/"name=", i, 5)) {
                char *j;

                if ((j = strchr(i + 5, ',')) != 0)
                    *j = (char)0;
                return i + 5;
            }
    }
    return rn2(3) ? (rn2(2) ? "P{Michael Toy}" : "P{Kenneth Arnold}")
        : "P{Glenn Wichman}";
}

static const char *const hcolors[] = {
    "A{ultraviolet}", "A{infrared}", "A{bluish-orange}", "A{reddish-green}",
    "A{dark white}", "A{light black}", "A{sky blue-pink}", "A{salty}",
    "A{sweet}", "A{sour}", "A{bitter}", "A{striped}", "A{spiral}", "A{swirly}",
    "A{plaid}", "A{checkered}", "A{argyle}", "A{paisley}", "A{blotchy}",
    "A{guernsey-spotted}", "A{polka-dotted}", "A{square}", "A{round}",
    "A{triangular}", "A{cabernet}", "A{sangria}", "A{fuchsia}", "A{wisteria}",
    "A{lemon-lime}", "A{strawberry-banana}", "A{peppermint}", "A{romantic}",
    "A{incandescent}"
};

/* return colorpref, unless hallucinating in which case return a random color */
const char *
hcolor(const char *colorpref)
{
    return (Hallucination || !colorpref) ?
        hcolors[display_rng(SIZE(hcolors))] : colorpref;
}

/* return a random real color unless hallucinating */
const char *
rndcolor(void)
{
    int k = rn2(CLR_MAX);

    return Hallucination ? hcolor(NULL) :
        (k == NO_COLOR) ? "A{colorless}" : c_obj_colors[k];
}

/* Aliases for road-runner nemesis
 */
static const char *const coynames[] = {
    "S{Carnivorous Vulgaris}", "S{Road-Runnerus Digestus}",
    "S{Eatibus Anythingus}", "S{Famishus-Famishus}",
    "S{Eatibus Almost Anythingus}", "S{Eatius Birdius}",
    "S{Famishius Fantasticus}", "S{Eternalii Famishiis}",
    "S{Famishus Vulgarus}", "S{Famishius Vulgaris Ingeniusi}",
    "S{Eatius-Slobbius}", "S{Hardheadipus Oedipus}",
    "S{Carnivorous Slobbius}", "S{Hard-Headipus Ravenus}",
    "S{Evereadii Eatibus}", "S{Apetitius Giganticus}",
    "S{Hungrii Flea-Bagius}", "S{Overconfidentii Vulgaris}",
    "S{Caninus Nervous Rex}", "S{Grotesques Appetitus}",
    "S{Nemesis Riduclii}", "S{Canis latrans}"
};

/* Returns a grammartree anything string, not a noun. */
char *
coyotename(const struct monst *mtmp, char *buf)
{
    if (mtmp && buf) {
        sprintf(buf, "N=%s - S=%s", x_monnam(mtmp, ARTICLE_NONE, NULL, 0, TRUE),
                mtmp->mcan ? coynames[SIZE(coynames) - 1] :
                coynames[display_rng(SIZE(coynames) - 1)]);
    }
    return buf;
}

/*do_name.c*/
