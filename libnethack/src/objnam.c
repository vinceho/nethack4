/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Summary of all NetHack 4's object naming functions:
   obj_typename(otyp): entry in discovery list, from player's point of view
   '(orange) potion of speed'
   simple_typename(otyp): object type's actual name or appearance, ignoring
   user-assigned names
   'scroll of mail' (identified); 'scroll labeled KIRJE' (unidentified)
   distant_name(obj,func): name object as per func, except as if it isn't
   currently examinable by the user
   'potion', 'scroll', 'sword', 'orange potion' (if previously viewed)
   fruitname(juice): name of material fruit is made from, or fruit juice
   e.g. with 'slice of pizza': 'pizza' (juice==0), 'pizza juice' (juice == 1)
   xname(obj): general-use name as if player is viewing an object now
   'potions of sickness', 'potion' (blind), 'corpse', 'orange potion' (unIDed)
   mshot_xname(obj): name of fired missile in a volley
   'the 2nd dagger', 'the 4th arrow'
   doname(obj): fully detailed name of object or stack as seen in inventory
   'the blessed Amulet of Yendor (being worn)', 'a poisoned +4 dagger'
   doname_price(obj): like doname, but with price info for shop items
   corpse_xname(obj, ignore_oquan): describe a corpse or pile of corpses
   'horse corpse', 'horse corpses'
   cxname(obj): xname, but with specific corpse naming
   'potion of sickness', 'horse corpses'
   killer_xname(obj): name from the game's view, minus info like BCU and greasedness
   'scroll of mail' (even if un-IDed)
   singular(obj,func): name one object of a stack as per func
   an(str): prefix 'a' or 'an' to str, if necessary
   '4 horse corpses', '3 orange potions shatter!', 'speed boots burn'
   'The corpses', 'The orange potions shatter!', 'The speed boots burn'
   yname(obj): like xname(), but incorporates ownership details
   'your potions called Y', 'Medusa's potion of oil', 'the apple named X'
   ysimple_name(obj): like simple_typename(), with ownership details
   'your orange potions', 'Medusa's potion of oil', 'the apple'
   makeplural(str): returns plural version of str
   'sheep' -> 'sheep', 'lump of sheep' -> 'lumps of sheep', 'mumak' -> 'mumakil'
   makesingular(str): opposite of makeplural
   readobjname(str, default_obj, from_user): convert string to object
   if 'nothing' is given, default_obj is returned
   cloak_simple_name(cloak): return vague description of given cloak
   'robe', 'wrapping', 'apron', 'smock', 'cloak'
   mimic_obj_name(monster): return object that mimic is mimicking
   'gold', 'orange', 'whatcha-may-callit' (mimic is mimicking a ])
 */

#define PREFIX    120      /* (56) */
#define SCHAR_LIM 127
#define NUMOBUF   12

static boolean wishymatch(const char *, const char *, boolean);
static char *nextobuf(void);

struct Jitem {
    int item;
    const char *name;
};

/* true for gems/rocks that should have ' stone' appended to their names */
#define GemStone(typ)   (typ == FLINT ||                                \
                         (objects[typ].oc_material == GEMSTONE &&       \
                          typ != DILITHIUM_CRYSTAL && typ != RUBY &&    \
                          typ != DIAMOND && typ != SAPPHIRE &&          \
                          typ != BLACK_OPAL &&                          \
                          typ != EMERALD && typ != OPAL))


static const struct Jitem Japanese_items[] = {
    {SHORT_SWORD, "N{wakizashi}"},
    {BROADSWORD, "N{ninja-to}"},
    {FLAIL, "N{nunchaku}"},
    {GLAIVE, "N{naginata}"},
    {LOCK_PICK, "N{osaku}"},
    {WOODEN_HARP, "N{koto}"},
    {KNIFE, "N{shito}"},
    {PLATE_MAIL, "N{tanko}"},
    {HELMET, "N{kabuto}"},
    {LEATHER_GLOVES, "N{yugake}"},
    {FOOD_RATION, "N{gunyoki}"},
    {POT_BOOZE, "N{sake}"},
    {0, ""}
};


static const char *Japanese_item_name(int i);

/* manage a pool of NUMOBUF buffers, so callers don't have to */
static char *
nextobuf(void)
{
    static char bufs[NUMOBUF][BUFSZ];
    static int bufidx = 0;

    bufidx = (bufidx + 1) % NUMOBUF;
    return bufs[bufidx];
}

/* The function that puts the uniquifiers on objects. What we do is to look for
   the main noun by scanning forwards to a '}' or a 'N{m,', then backwards to a
   ',', '|' or '{'. (All nouns are either based on a noun as first child, or are
   using the noun_MX rule.)  Then we delete any existing uniquifier, and put our
   new one in. The uniquifier used is uniquifier as a number, or 't' followed by
   otyp if uniquifier is 0. */
static char *
reuniquify(int uniquifier, int otyp, const char *oldnoun) {
    char *buf = nextobuf();
    static char working[BUFSZ];
    char *r = working, *s, c;
    strcpy(working, oldnoun);
    while(*r && *r != '}' && !strncmp(r, /*nointl*/"N{m,", 4)) r++;
    if (!*r) {
        impossible("S{string '%s' is not a properly formed noun}",
                   oldnoun);
        strcpy(buf, oldnoun);
        return buf;
    }
    if (*r == '}') {
        while (*r != '{' && *r != ',' && *r != '|') r--;
        s = r + 1;
        if (*r == '|') {
            while (*r != ',' && *r != '{') *(r--) = 0;
        }
        r++;
    } else {r += 4; s = r;} /* move to just after the , */
    c = *r; *r = 0;
    strcpy(buf, working);
    *r = c;
    sprintf(eos(buf), uniquifier ? /*nointl*/"%d|" : /*nointl*/"t%d|",
            uniquifier ? uniquifier : otyp);
    strcat(buf, s);
    return buf;
}

char *
add_uniquifier(int uniquifier, const char *oldnoun) {
    return reuniquify(uniquifier, 0, oldnoun);
}

#define RU(s) reuniquify(uniquifier, otyp, (s))

/* Like reuniquify, but for modifiers, like i, and f,. */
static char *
remodify(char modifier, const char *oldnoun) {
    char *buf = nextobuf();
    static char working[BUFSZ];
    char *r = working, c;
    strcpy(working, oldnoun);
    while(*r && *r != '}' && !strncmp(r, /*nointl*/"N{m,", 4)) r++;
    if (!*r) {
        impossible("S{string '%s' is not a properly formed noun}",
                   oldnoun);
        strcpy(buf, oldnoun);
        return buf;
    }
    if (*r == '}') {
        while (*r != '{' && *r != ',' && *r != '|') r--;
        if (*r == '|') {
            while (*r != ',' && *r != '{') *(r--) = 0;
        }
        r++;
    } else r += 4; /* move to just after the , */
    c = *r; *r = 0;
    strcpy(buf, working);
    *r = c;
    if (r[-2] != modifier && (r < working + 4 || r[-4] != modifier))
        sprintf(eos(buf), /*nointl*/"%c,", modifier);
    strcat(buf, r);
    return buf;
}


/* For NetHack 4, we centralise the object naming in two functions.
   generic_typename does all the naming of object types; generic_xname does all
   the naming of other features of objects (and calls generic_typename to do the
   naming of the type within the object name).

   The return buffer is writable, and leaves PREFIX characters before the start
   that can also be written.
*/
static char *
generic_typename(int otyp, boolean show_notes, boolean desc_known,
                 boolean show_uname, boolean force_unknown, int uniquifier)
{
    char *buf = nextobuf() + PREFIX;
    const char *basetype;
    struct objclass *ocl = &objects[otyp];
    const char *actualn = OBJ_NAME(*ocl); /* a noun */
    const char *dn = OBJ_DESCR(*ocl); /* an adjective */
    const char *un = ocl->oc_uname; /* a user-entered string */
    char called[BUFSZ];
    int nn = ocl->oc_name_known;

    if (!show_uname) un = NULL;
    if (force_unknown) nn = 0;

    if (un) sprintf(called, "C{n,p,V{V{call},N{m,S{\x1c%s\x1c}}}}@", un);

    if (Role_if(PM_SAMURAI) && Japanese_item_name(otyp))
        actualn = Japanese_item_name(otyp);

    /* If the player knows what the item is, they get its real name, with the
       description adjective prefixed (e.g. '(orange) potion of speed'). If they
       don't, they get a generic item type with the description adjective
       prefixed ('crude arrow', 'orange potion'). The individual rules by class
       are more complex, and explained below. */

    switch (ocl->oc_class) {

        /* either actualn, or dn on a noun */
    case WEAPON_CLASS: basetype = skill_name(ocl->oc_skill); goto dn_noun;
    case ARMOR_CLASS:
        switch(ocl->oc_armcat) {
        case ARM_SHIELD: basetype = "N{shield}";     break;
        case ARM_HELM:   basetype = "N{hat}";        break;
        case ARM_GLOVES: basetype = "N{*,N{glove}}"; break;
        case ARM_BOOTS:  basetype = "N{*,N{boot}}";  break;
        case ARM_CLOAK:  basetype = "N{cloak}";      break;
        case ARM_SHIRT:  basetype = "N{shirt}";      break;
        case ARM_SUIT:   basetype = "N{armor}";      break;
        }
        goto dn_noun;
    case TOOL_CLASS:
        switch(otyp) {
        case TALLOW_CANDLE: case WAX_CANDLE: basetype = "N{candle}"; break;
        case OIL_LAMP: case MAGIC_LAMP: basetype = "N{lamp}"; break;
        case TIN_WHISTLE: case MAGIC_WHISTLE: basetype = "N{whistle}"; break;
        case WOODEN_FLUTE: case MAGIC_FLUTE: basetype = "N{flute}"; break;
        case TOOLED_HORN: case FROST_HORN: case FIRE_HORN: case HORN_OF_PLENTY:
            basetype = "N{horn}"; break;
        case WOODEN_HARP: case MAGIC_HARP: basetype = "N{harp}"; break;
        case LEATHER_DRUM: case DRUM_OF_EARTHQUAKE: basetype = "N{drum}"; break;
        case BELL: case BELL_OF_OPENING: basetype = "N{bell}"; break;
        }
    dn_noun:
        if (!desc_known) /* we don't get a uniquifier if we can't see it */
            sprintf(buf, "N=%s", basetype);
        else if (nn || !dn) {
            /* identified or always-identified */
            if (!dn || !strcmp(dn, UNID_ADJ) || !show_notes) {
                if (un) sprintf(buf, "N{N=%s,C=%s}", RU(actualn), called);
                else sprintf(buf, "N=%s", RU(actualn));
            } else {
                if (un) sprintf(buf, "N{N{N=%s,A{p,A=%s}},C=%s}",
                                RU(actualn), dn, called);
                else sprintf(buf, "N{N=%s,A{p,A=%s}}", RU(actualn), dn);
            }
        } else {
            /* not identified */
            if (un) {
                if (!strcmp(dn, UNID_ADJ) || !show_notes)
                    sprintf(buf, "N{N=%s,C=%s}", RU(basetype), called);
                else
                    sprintf(buf, "N{N{N=%s,A{p,A=%s}},C=%s}", RU(basetype),
                            dn, called);
            } else
                sprintf(buf, "N{N=%s,A=%s}", RU(basetype), dn);
        }
        return buf;

        /* gems have their own complex rules */
    case GEM_CLASS:
        /* There are five sets of gems:
           - Valuable gems, 'white gem', 'opal'
           - Valuable gem materials, 'yellow gem', '(yellow) citrine stone'
           - Worthless glass, 'blue gem', 'worthless piece of blue glass'
           - Gray stones, 'luckstone', 'flint stone'
           - Rocks */
        if (dn) {
            if (otyp >= LUCKSTONE && otyp <= FLINT) {
                /* Gray stones */
                if (!desc_known)
                    sprintf(buf, "N=%s", "N{stone}");
                else if (nn && otyp == FLINT)
                    sprintf(buf, "N{N=%s,A=%s}", RU("N{stone}"), actualn);
                else if (nn) sprintf(buf, "N=%s", actualn);
                else sprintf(buf, "N{N=%s,A=%s}", RU("N{stone}"), dn);
            } else if (!nn || !desc_known) {
                /* unidentified glass looks like unidentified gems */
                if (!desc_known)
                    sprintf(buf, "N=%s", "N{gem}");
                else if (un && show_notes)
                    sprintf(buf, "N{N{N=%s,A{p,A=%s}},A=%s}",
                            RU("N{gem}"), dn, called);
                else if (un) sprintf(buf, "N{N=%s,A=%s}", RU("N{gem}"), called);
                else sprintf(buf, "N{N=%s,A=%s}", RU("N{gem}"), dn);
            } else if (otyp >= WORTHLESS_PIECE_OF_WHITE_GLASS &&
                       otyp <= WORTHLESS_PIECE_OF_VIOLET_GLASS) {
                /* IDed worthless glass doesn't have a color reminder,
                   because it doesn't need one, and 'a (red) piece of
                   worthless red glass' looks silly */
                if (un && show_notes)
                    sprintf(buf, "N{N=%s,A{p,A=%s}}", RU(actualn), called);
                else 
                    sprintf(buf, "N=%s", RU(actualn));
            } else if (GemStone(otyp)) {
                /* append "stone" to the name */
                if (!show_notes)
                    sprintf(buf, "N{N=%s,A=%s}", RU("N{stone}"), actualn);
                else if (un)
                    sprintf(buf, "N{N{N{N=%s,A=%s},A{p,A=%s}},A{p,A=%s}}",
                            RU("N{stone}"), actualn, dn, called);
                else
                    sprintf(buf, "N{N{N=%s,A=%s},A{p,A=%s}}",
                            RU("N{stone}"), actualn, dn);
            } else {
                if (!show_notes)
                    sprintf(buf, "N=%s", RU(actualn));
                else if (un)
                    sprintf(buf, "N{N{N=%s,A{p,A=%s}},A{p,A=%s}}",
                            RU(actualn), dn, called);
                else
                    sprintf(buf, "N{N=%s,A{p,A=%s}}", RU(actualn), dn);
            }
            return buf;
        } /* otherwise it's a rock, fall through */

        /* always identified, so just use the identified name, even if we're
           blind */
    case FOOD_CLASS:
    case BALL_CLASS:
    case VENOM_CLASS:
    case CHAIN_CLASS:
    case COIN_CLASS:
    case ROCK_CLASS:
        sprintf(buf, "N=%s", RU(actualn));
        return buf;

        /* 'generictype of magicspell', always random appearance */
    case RING_CLASS:   basetype = "N{ring}";      goto a_of_b;
    case AMULET_CLASS: basetype = "N{amulet}";    goto a_of_b;
    case POTION_CLASS: basetype = "N{potion}";    goto a_of_b;
    case SPBOOK_CLASS: basetype = "N{spellbook}"; goto a_of_b;
    case WAND_CLASS:   basetype = "N{wand}";
    a_of_b:
        /* There are three possibilities (unIDed, called, formally IDed):
           unIDed: 'orange potion'
           called: '(orange) potion called foo'
           formal: '(orange) potion of speed' or
                   '(orange) potion of speed (called foo)' */
        if (!dn) break; /* fall through to impossible */

        if (!desc_known)
            sprintf(buf, "N=%s", basetype);
        else if (!nn && !un)
            sprintf(buf, "N{N=%s,A=%s}", RU(basetype), dn);
        else if (!nn) {
            if (show_notes)
                sprintf(buf, "N{N{N=%s,A{p,A=%s}},A=%s}",
                        RU(basetype), dn, called);
            else
                sprintf(buf, "N{N=%s,A=%s}", RU(basetype), called);
        } else if (!un || !show_notes) {
            if (show_notes)
                sprintf(buf, "N{q,N{N=%s,A{p,A=%s}},N=%s}",
                        RU(basetype), dn, actualn);
            else
                sprintf(buf, "N{q,N=%s,N=%s}", RU(basetype), actualn);
        } else
            sprintf(buf, "N{N{q,N{N=%s,A{p,A=%s}},N=%s},A{p,A=%s}}",
                    RU(basetype), dn, actualn, called);
        return buf;

        /* 'scroll labeled FOO' */
    case SCROLL_CLASS: basetype = "N{scroll}";
        /* blank paper is a special case, following the potion naming pattern
           we also do this if the player can't see the scroll*/
        if (otyp == SCR_BLANK_PAPER || !desc_known) goto a_of_b;
        /* Same possibilities as with potions, just different wording:
           unIDed: 'scroll labeled KIRJE'
           called: 'scroll called foo (labeled KIRJE)'
           formal: 'scroll of mail (labeled KIRJE)' or
                   'scroll of mail (called foo and labeled KIRJE)' */
        /* TODO: Get parentheses on these clauses */
        if (!dn) break; /* fall through to impossible */
        if (!nn && !un)
            sprintf(buf, "N{N=%s,C{n,p,V{V{label},N=%s}}}", RU(basetype), dn);
        else if (!nn) {
            if (show_notes)
                sprintf(buf, "N{N{N=%s,C=%s},C{n,p,V{V{label},N=%s}}}",
                        RU(basetype), called, dn);
            else
                sprintf(buf, "N{N=%s,C=%s}", RU(basetype), called);
        } else if (!un || !show_notes) {
            if (show_notes)
                sprintf(buf, "N{N{q,N=%s,N=%s},C{n,p,V{V{label},N=%s}}}",
                        RU(basetype), actualn, dn);
            else
                sprintf(buf, "N{q,N=%s,N=%s}", RU(basetype), actualn);
        } else
            sprintf(buf, "N{N{q,N=%s,N=%s},C{+,C=%s,C{n,p,V{V{label},N=%s}}}}",
                    RU(basetype), actualn, called, dn);
        return buf;
    }
    impossible("S{object typ %d needs a description}", otyp);
    strcpy(buf, "N{N{object},A{buggy}}");
    return buf;
}

#undef RU

char *
obj_typename(int otyp)
{
    return generic_typename(otyp, TRUE, TRUE, TRUE, FALSE, 0);
}

/* less verbose result than obj_typename(); either the actual name
   or the description (but not both); user-assigned name is ignored */
char *
simple_typename(int otyp)
{
    return generic_typename(otyp, FALSE, TRUE, FALSE, FALSE, 0);
}

boolean
obj_is_pname(const struct obj * obj)
{
    return ((boolean) (obj->dknown && obj->known && obj->onamelth &&
                       /* Since there aren't any objects which are both
                          artifacts and unique, the last check is redundant. */
                       obj->oartifact && !objects[obj->otyp].oc_unique));
}

/* Give the name of an object seen at a distance.  Unlike xname/doname,
 * we don't want to set dknown if it's not set already.  The kludge used is
 * to temporarily set Blind so that xname() skips the dknown setting.  This
 * assumes that we don't want to do this too often; if this function becomes
 * frequently used, it'd probably be better to pass a parameter to xname()
 * or doname() instead.
 */
char *
distant_name(const struct obj *obj, char *(*func) (const struct obj *))
{
    char *str;

    long save_Blinded = Blinded;

    Blinded = 1;
    str = (*func) (obj);
    Blinded = save_Blinded;
    return str;
}

/* convert player specified fruit name into corresponding fruit juice name
   ('slice of pizza' -> 'pizza juice' rather than 'slice of pizza juice')

   This function outputs grammartree, but operates on pl_fruit which is a
   user-entered value, and so deals with plan English strings on occasion. */
/* juice: whether or not to append ' juice' to the name */
char *
fruitname(boolean juice)
{
    char *buf = nextobuf();
    const char *fruit_nam = strstri(pl_fruit, /*nointl*/" of ");

    if (fruit_nam)
        fruit_nam += 4; /* skip past ' of ' */
    else
        fruit_nam = pl_fruit;   /* use it as is */

    if (juice)
        sprintf(buf, /*nointl*/"N{N{juice},A{N{\x1c%s\x1c}}", fruit_nam);
    else
        sprintf(buf, /*nointl*/"N{\x1c%s\x1c}", fruit_nam);
    return buf;
}


/* basic first look at the object; this used to be part of xname.
   examining an object this way happens automatically and does not involve
   touching (no stoning) */
void
examine_object(struct obj *obj)
{
    int typ = obj->otyp;
    struct objclass *ocl = &objects[typ];
    int nn = ocl->oc_name_known;

    /* clean up known when it's tied to oc_name_known, eg after AD_DRIN */
    if (!nn && ocl->oc_uses_known && ocl->oc_unique)
        obj->known = 0;
    if (!Blind)
        obj->dknown = TRUE;
    if (Role_if(PM_PRIEST))
        obj->bknown = TRUE;
}

#define ADD_ADJECTIVE(s) do {                           \
        buf -= 2;                                       \
        buf[0] = 'N'; buf[1] = '{';                     \
        sprintf(eos(buf), /*nointl*/",%s}", (s));       \
    }                                                   \
    while(0)
#define ADD_NOUN_MOD(c,s) do {                          \
        buf -= 4;                                       \
        buf[0] = 'N'; buf[1] = '{';                     \
        buf[2] = c; buf[3] = ',';                       \
        sprintf(eos(buf), /*nointl*/",%s}", (s));       \
    }                                                   \
    while(0)
#define ADD_PREPOSITION(e,n) do {                               \
        buf -= 2;                                               \
        buf[0] = 'N'; buf[1] = '{';                             \
        sprintf(eos(buf), /*nointl*/",%s,%s}", (e), (n));       \
    }                                                           \
    while(0)

/* Name one particular object. This acts approximately like 3.4.3's xname() if
   precise_adjectives is FALSE, and like 3.4.3's doname() if it's TRUE. */
static char *
generic_xname(const struct obj *obj, boolean ignore_oquan,
              boolean include_corpse_type, boolean precise_adjectives,
              boolean with_price, boolean precise_stack_size)
{
    char *buf;
    char tbuf[BUFSZ];
    int typ = obj->otyp;
    struct objclass *ocl = &objects[typ];
    boolean known = obj->known;
    boolean dknown = obj->dknown;
    boolean bknown = obj->bknown;
    boolean rknown = obj->rknown;
    int nn = ocl->oc_name_known;

    /* 
     * clean up known when it's tied to oc_name_known, eg after AD_DRIN
     * This is only required for unique objects since the article
     * printed for the object is tied to the combination of the two
     * and printing the wrong article gives away information.
     */
    if (!nn && ocl->oc_uses_known && ocl->oc_unique)
        known = 0;
    if (!Blind)
        dknown = TRUE;
    if (Role_if(PM_PRIEST))
        bknown = TRUE;

    buf = nextobuf() + PREFIX;

    /* Unlike the 3.4.3 routines, we do something much simpler: start with
       generic_typename, then pile on adjectives. The generic typename handles
       Japanese naming, and dknown, nn, dn, un. We have to handle everything
       else ourselves, though.

       Small exception: with Amulets of Yendor (and the fake variety), we force
       !nn if the 'known' flag isn't set. (In other words, each has to be IDed
       individually.) */
    sprintf(buf, "N=%s", generic_typename(
                typ, FALSE, dknown, TRUE,
                (typ == AMULET_OF_YENDOR ||
                 typ == FAKE_AMULET_OF_YENDOR) && !known,
                obj->o_id));

    /* Special case for fruits. (We'd always get our own fruitname
       otherwise.) */
    if (typ == SLIME_MOLD) {
        struct fruit *f;
        
        for (f = ffruit; f; f = f->nextf) {
            if (f->fid == obj->spe) {
                /* We need to add the uniquifier by hand here; and we need to
                   escape the name to mark it as user-entered (and in case it
                   contains grammartree metacharacters). */
                sprintf(buf, /*nointl*/"N{%d|\x1c%s\x1c}", obj->o_id, f->fname);
                break;
            }
        }
        if (!f)
            impossible("S{Bad fruit #%d?}", obj->spe);
    }

    /* Pile on adjectives. */

    /* Iron balls can be particularly heavy.
       The item basename is "N{N{N{ball},A{iron}},A{heavy}}"; we need
       to overwrite the base name in this case to put the adverb in
       the right place.*/
    if (ocl->oc_class == BALL_CLASS && obj->owt > ocl->oc_weight) {
        sprintf(buf, "N{N{N{%d|ball},A{iron}},A{A{heavy},D{very}}}",
                obj->o_id);
    }

    if (precise_adjectives) {
        long charges = -2;
        int recharged = 0;
        /* Many sorts of objects have numerical enchantments... */
        if (obj->known &&
            (obj->oclass == WEAPON_CLASS ||
             obj->oclass == ARMOR_CLASS ||
             (obj->oclass == RING_CLASS && objects[typ].oc_charged) ||
             is_weptool(obj))) {
            if (obj->spe >= 0)
                sprintf(tbuf, /*nointl*/"A{+%d}", obj->spe);
            else
                sprintf(tbuf, /*nointl*/"A{-%d}", -obj->spe);
            ADD_ADJECTIVE(tbuf);
        }
        /* ...or charge counts */
        if ((obj->oclass == TOOL_CLASS && objects[typ].oc_charged) ||
            obj->oclass == WAND_CLASS) {
            charges = obj->spe;
            recharged = obj->recharged;
        }
        if (obj->known && recharged) {
            if (obj->recharged >= 3)
                ADD_ADJECTIVE("C{n,p,V{V{recharge},D{repeatedly}}}@");
            else if (obj->recharged == 2)
                ADD_ADJECTIVE("C{n,p,V{V{recharge},D{twice}}}@");
            else if (obj->recharged >= 1)
                ADD_ADJECTIVE("C{n,p,V{recharge}}@");
        }
        if (ignitable(obj) && obj->known && obj->otyp != MAGIC_LAMP &&
            !artifact_light(obj)) {
            long timeout = obj->lamplit ?
                report_timer(obj->olev, BURN_OBJECT, (void *)obj) : moves;

            /* obj->age is the fuel remaining when the timer runs out. So we
               add it to the turns the timer has remaining to get remaining
               charge count. */
            charges = obj->age + timeout - moves;
        }
        if (obj->known && charges != -2) {
            if (charges == -1)
                ADD_ADJECTIVE("C{n,p,V{cancel}}@");
            else if (charges == 0)
                ADD_ADJECTIVE("A{empty}");
            else {
                sprintf(tbuf, "N{%ld,N{i,charge}}", charges);

                /* Candelabra have candles. We combine the candelabrum's
                   properties to lead to a more natural sentence, 'the blessed
                   Candelabrum of Invocation with 7 candles and 240 charges'. */
                if (typ == CANDELABRUM_OF_INVOCATION && precise_adjectives) {
                    sprintf(tbuf, "N{+,N{%d,N{i,candle}},N{%ld,N{i,charge}}}",
                            obj->spe, charges);
                }

                ADD_PREPOSITION("E{with}",tbuf);
            }
        }
    }

    /* Weapons can be poisoned. */
    if (is_poisonable(obj) && obj->opoisoned)
        ADD_ADJECTIVE("A{V{poison}}");

    /* Figurines, statues and corpses have associated monster types.

       We have two possibilities for expressing this:
       "N{N{figurine},A{N{gnome}}}" 'gnome figurine'
       "N{l,N{figurine},N{i,gnome}}" 'figurine of a gnome'

       The difference between the two is that the latter is more formal, in a
       way, and also is harder to modify (grammartree lets us express 'gnomier
       figurine', but not 'figurine more of a gnome', which fits quite well with
       actual English usage). The more formal version also fits NetHack's
       existing grammar better.
     */
    if (typ == FIGURINE || typ == STATUE ||
        ((include_corpse_type || precise_adjectives) && typ == CORPSE)) {
        /* Mark the monster name as indefinite. (Unless it's a proper name, in
           which case this step is nonsensical, or it's a unique without a P{}
           name ('the Oracle'), in which case we want the 'the'.) */
        if (*(mons[obj->corpsenm].mname) == 'N' &&
            !type_is_pname(&mons[obj->corpsenm])) {
            ADD_NOUN_MOD('l', remodify('i', mons[obj->corpsenm].mname));
        } else
            ADD_NOUN_MOD('l', mons[obj->corpsenm].mname);
    }
    /* Eggs also have associated monster types, but in this case, 'chicken egg'
       is the more natural wording. */
    if (typ == EGG && obj->corpsenm >= LOW_PM &&
        (obj->known || mvitals[obj->corpsenm].mvflags & MV_KNOWS_EGG)) {
        sprintf(tbuf, "A{N=%s}", mons[obj->corpsenm].mname);
        ADD_ADJECTIVE(tbuf);
        if (obj->spe && precise_adjectives) {
            sprintf(tbuf, "C{n,p,V{V{lay},D{E{by},N=%s}}}@", you);
            ADD_ADJECTIVE(tbuf);
        }
    }

    /* Statues can also be historic. */
    if (typ == STATUE && Role_if(PM_ARCHEOLOGIST) &&
        (obj->spe & STATUE_HISTORIC))
        ADD_ADJECTIVE("A{historic}");

    /* Tins have contents. The player might know what they are. */
    if (typ == TIN && known) {
        if (obj->spe > 0)
            ADD_NOUN_MOD('l', "N{o,spinach}");
        else if (obj->corpsenm == NON_PM)
            ADD_ADJECTIVE("A{empty}");
        else if (vegetarian(&mons[obj->corpsenm]))
            ADD_NOUN_MOD('l', remodify('o',mons[obj->corpsenm].mname));
        else {
            sprintf(tbuf, "N{N{o,meat},A{N=%s}}", mons[obj->corpsenm].mname);
            ADD_NOUN_MOD('l', tbuf);
        }
    }

    /* Water has custom BCU adjectives. */
    if (typ == POT_WATER && objects[POT_WATER].oc_name_known && bknown) {
        if (obj->blessed) ADD_ADJECTIVE("A{holy}");
        else if (obj->cursed) ADD_ADJECTIVE("A{unholy}");
        bknown = 0;
    } /* otherwise we do BCU later */

    /* Adjectives that are only used if we want precise adjectives */
    if (precise_adjectives) {

        /* Any item can be greased. */
        if (obj->greased)
            ADD_ADJECTIVE("A{greased}");

        /* Many sorts of objects can be eroded. In 3.4.3, potion dilution is
           shown even with imprecise adjectives, other erosion is shown only
           with precise adjectives. This is a bit inconsistent, so we now show
           all erosion with precise adjectives only. */
        if (obj->oclass == POTION_CLASS && dknown && obj->odiluted)
            ADD_ADJECTIVE("A{V{dilute}}");
        if (obj->oclass == FOOD_CLASS && obj->orotten)
            ADD_ADJECTIVE("A{rotten}");
        if (is_rustprone(obj)) {
            switch(obj->oeroded) {
            case 1: ADD_ADJECTIVE("A{rusty}"); break;
            case 2: ADD_ADJECTIVE("A{A{rusty},D{very}}"); break;
            case 3: ADD_ADJECTIVE("A{A{rusty},D{thoroughly}}"); break;
            }
        }
        if (is_flammable(obj)) {
            switch(obj->oeroded) {
            case 1: ADD_ADJECTIVE("A{V{burn}}"); break;
            case 2: ADD_ADJECTIVE("A{A{V{burn}},D{very}}"); break;
            case 3: ADD_ADJECTIVE("A{A{V{burn}},D{thoroughly}}"); break;
            }
        }
        if (is_corrodeable(obj)) {
            switch(obj->oeroded2) {
            case 1: ADD_ADJECTIVE("A{V{corrode}}"); break;
            case 2: ADD_ADJECTIVE("A{A{V{corrode}},D{very}}"); break;
            case 3: ADD_ADJECTIVE("A{A{V{corrode}},D{thoroughly}}"); break;
            }
        }
        if (is_rottable(obj)) {
            switch(obj->oeroded2) {
            case 1: ADD_ADJECTIVE("A{rotten}"); break;
            case 2: ADD_ADJECTIVE("A{A{rotten},D{very}}"); break;
            case 3: ADD_ADJECTIVE("A{A{rotten},D{thoroughly}}"); break;
            }
        }
        if (rknown && obj->oerodeproof) {
            if (typ == CRYSKNIFE) ADD_ADJECTIVE("A{V{fix}}");
            else if (is_rustprone(obj)) ADD_ADJECTIVE("A{rustproof}");
            else if (is_corrodeable(obj)) ADD_ADJECTIVE("A{corrodeproof}");
            else if (is_flammable(obj)) ADD_ADJECTIVE("A{fireproof}");
            else if (is_rottable(obj)) ADD_ADJECTIVE("A{rotproof}");
            /* otherwise no adjective, the fixing is a no-op */
        }

        /* Food might be partly eaten. */
        if (obj->oclass == FOOD_CLASS) {
            if (obj->oeaten) ADD_ADJECTIVE("A{A{V{eat}},D{partly}}");
        }

        /* Some objects can be lit. */
        if (ignitable(obj) && obj->lamplit) ADD_ADJECTIVE("C{n,p,V{light}}@");

        /* Blessed or cursed, if the player knows about it.
           TODO: We could really do with a better word for 'uncursed'. */
        if (bknown) {
            if (obj->cursed) ADD_ADJECTIVE("A{V{curse}}");
            else if (obj->blessed) ADD_ADJECTIVE("A{V{bless}}");
            else ADD_ADJECTIVE("A{uncursed}");
        }

#ifdef INVISIBLE_OBJECTS
        if (obj->o_invis)
            ADD_ADJECTIVE("A{invisible}");
#endif

        if (obj->owornmask & W_WEP) {
            if (!mrg_to_wielded) ADD_NOUN_MOD('p',"V{wield}");
        } else if (obj->owornmask & W_SWAPWEP && u.twoweap)
            ADD_ADJECTIVE("A{offhand}");
        else if (obj->owornmask & W_SWAPWEP)
            ADD_NOUN_MOD('p',"V{ready}");
        else if (obj->owornmask & W_QUIVER)
            ADD_NOUN_MOD('p',"V{quiver}");
        else if (obj->owornmask ||
                 (typ == LEASH && obj->leashmon))
            ADD_NOUN_MOD('p',"V{equip}");

        /* Objects can have price tags. */
        if (obj->unpaid) {
            xchar ox, oy;
            long quotedprice = unpaid_cost(obj);
            struct monst *shkp = NULL;
            if (Has_contents(obj) &&
                get_obj_location(obj, &ox, &oy, BURIED_TOO | CONTAINED_TOO) &&
                costly_spot(ox, oy) &&
                (shkp = shop_keeper(level, *in_rooms(level, ox, oy, SHOPBASE))))
                quotedprice += contained_cost(obj, shkp, 0L, FALSE, TRUE);
            sprintf(tbuf, "C{n,p,V{V{price},D{E{at},N{%ld,N{i,zorkmid}}}}}@",
                    quotedprice);
            ADD_ADJECTIVE(tbuf);
        } else if (with_price) {
            long price = shop_item_cost(obj);
            if (price > 0) {
                sprintf(tbuf,
                        "C{n,p,V{V{price},D{E{at},N{%ld,N{i,zorkmid}}}}}@",
                        price);
                ADD_ADJECTIVE(tbuf);
            }
        }
    }

    /* Some things are naturally pairs... */
    if (typ == LENSES || is_boots(obj) || is_gloves(obj))
        ADD_NOUN_MOD('f',"N{pair}");
    /* ...or sets. */
    if (typ >= GRAY_DRAGON_SCALES && typ <= YELLOW_DRAGON_SCALES)
        ADD_NOUN_MOD('f',"N{set}");

    if (obj->onamelth && dknown) {
        sprintf(tbuf, /*nointl*/"A{V{V{name},N{\x1c%s\x1c}}}", ONAME(obj));
        ADD_ADJECTIVE(tbuf);
    }

    if (!ignore_oquan && obj->quan != 1L) {
        if (precise_stack_size)
            sprintf(tbuf, "N{%d,N=%s}", obj->quan, buf);
        else
            sprintf(tbuf, "N{*,N=%s}", buf);
        strcpy(buf, tbuf);
    }

    return buf;
}

char *
imprecise_xname(const struct obj *obj)
{
    return generic_xname(obj, FALSE, FALSE, FALSE, FALSE, FALSE);
}

char *
xname(const struct obj *obj)
{
    return generic_xname(obj, FALSE, FALSE, FALSE, FALSE, TRUE);
}

/* xname() output augmented for multishot missile feedback */
char *
mshot_xname(const struct obj *obj)
{
    char tmpbuf[BUFSZ];
    char *onm = xname(obj);

    if (m_shot.n > 1 && m_shot.o == obj->otyp) {
        /* copy xname's result so that we can reuse its return buffer */
        strcpy(tmpbuf, onm);
        sprintf(onm, "N{N=%s,A{o,A{%d}}}", tmpbuf, m_shot.i);
    }

    return onm;
}

char *
doname(const struct obj *obj)
{
    return generic_xname(obj, FALSE, TRUE, TRUE, FALSE, TRUE);
}

char *
doname_price(const struct obj *obj)
{
    return generic_xname(obj, FALSE, TRUE, TRUE, TRUE, TRUE);
}

/* used from invent.c */
boolean
not_fully_identified_core(const struct obj * otmp, boolean ignore_bknown)
{
    /* gold doesn't have any interesting attributes [yet?] */
    if (otmp->oclass == COIN_CLASS)
        return FALSE;   /* always fully ID'd */

    /* check fundamental ID hallmarks first */
    if (!otmp->known || !otmp->dknown || (!ignore_bknown && !otmp->bknown) ||
        !objects[otmp->otyp].oc_name_known)       /* ?redundant? */
        return TRUE;
    if (otmp->oartifact && undiscovered_artifact(otmp->oartifact))
        return TRUE;
    /* otmp->rknown is the only item of interest if we reach here */
    /* 
     *  Note:  if a revision ever allows scrolls to become fireproof or
     *  rings to become shockproof, this checking will need to be revised.
     *  `rknown' ID only matters if xname() will provide the info about it.
     */
    if (otmp->rknown || (otmp->oclass != ARMOR_CLASS &&
                         otmp->oclass != WEAPON_CLASS &&
                         !is_weptool(otmp) &&    /* (redunant) */
                         otmp->oclass != BALL_CLASS))   /* (useless) */
        return FALSE;
    else        /* lack of `rknown' only matters for vulnerable objects */
        return (boolean) (is_rustprone(otmp) || is_corrodeable(otmp) ||
                          is_flammable(otmp) || is_rottable(otmp));
}

boolean
not_fully_identified(const struct obj * otmp)
{
    return not_fully_identified_core(otmp, FALSE);
}

char *
corpse_xname(const struct obj *otmp, boolean ignore_oquan)
{
    return generic_xname(otmp, ignore_oquan, TRUE, FALSE, FALSE, TRUE);
}

/* Just a wrapper nowadays, because corpse_xname can now handle anything,
   because /it's/ just a wrapper around generic_xname */
char *
cxname(const struct obj *obj)
{
        return corpse_xname(obj, FALSE);
}

/* Likewise. */
char *
cxname2(const struct obj *obj)
{
    return corpse_xname(obj, TRUE);
}

/* treat an object as fully ID'd when it might be used as reason for death */
char *
killer_xname(const struct obj *obj_orig)
{
    struct obj *obj;
    unsigned save_ocknown;
    char *buf, *save_ocuname;
    int osize;

    /* copy the object. */
    osize = sizeof (struct obj) + obj_orig->onamelth + obj_orig->oxlth;
    obj = malloc(osize);
    memcpy(obj, obj_orig, osize);

    /* killer name should be more specific than general xname; however, exact
       info like blessed/cursed and rustproof makes things be too verbose */
    obj->known = obj->dknown = 1;
    obj->bknown = obj->rknown = obj->greased = 0;
    /* if character is a priest[ess], bknown will get toggled back on */
    obj->blessed = obj->cursed = 0;
    /* 'killed by poisoned <obj>' would be misleading when poison is not the
       cause of death and 'poisoned by poisoned <obj>' would be redundant when
       it is, so suppress 'poisoned' prefix */
    obj->opoisoned = 0;
    /* strip user-supplied name; artifacts keep theirs */
    if (!obj->oartifact)
        obj->onamelth = 0;
    /* temporarily identify the type of object */
    save_ocknown = objects[obj->otyp].oc_name_known;
    objects[obj->otyp].oc_name_known = 1;
    save_ocuname = objects[obj->otyp].oc_uname;
    objects[obj->otyp].oc_uname = 0;    /* avoid "foo called bar" */

    buf = cxname(obj);

    objects[obj->otyp].oc_name_known = save_ocknown;
    objects[obj->otyp].oc_uname = save_ocuname;

    free(obj);
    return buf;
}

/*
 * Used if only one of a collection of objects is named (e.g. in eat.c).
 */
const char *
singular(struct obj *otmp, char *(*func) (const struct obj *))
{
    long savequan;
    char *nam;

    /* Note: using xname for corpses will not give the monster type */
    if (otmp->otyp == CORPSE && func == xname)
        return corpse_xname(otmp, TRUE);

    savequan = otmp->quan;
    otmp->quan = 1L;
    nam = (*func) (otmp);
    otmp->quan = savequan;
    return nam;
}

char *
an(const char *str)
{
    return remodify('i', str);
}

char *
remove_the(const char *str)
{
    return remodify('o', str);
}

/* returns 'your xname(obj)' or 'Foobar's xname(obj)' or 'the xname(obj)' */
char *
yname(const struct obj *obj)
{
    char *outbuf = nextobuf();
    char *s = shk_your(outbuf, obj);    /* s = outbuf */
    outbuf = nextobuf();
    sprintf(outbuf, "N{o,N=%s,N=%s}", cxname(obj), s);

    return outbuf;
}

/* returns 'your simple_typename(obj->otyp)', etc */

char *
ysimple_name(const struct obj *obj)
{
    char *outbuf = nextobuf();
    char *s = shk_your(outbuf, obj);    /* s = outbuf */
    outbuf = nextobuf();
    sprintf(outbuf, "N{o,N=%s,N=%s}", simple_typename(obj->otyp), s);

    return outbuf;
}

/* !nointl{ */
static const char *const wrp[] = {
    "wand", "ring", "potion", "scroll", "gem", "amulet",
    "spellbook", "spell book",
    /* for non-specific wishes */
    "weapon", "armor", "armour", "tool", "food", "comestible",
};
/* }nointl! */

static const char wrpsym[] = {
    WAND_CLASS, RING_CLASS, POTION_CLASS, SCROLL_CLASS, GEM_CLASS,
    AMULET_CLASS, SPBOOK_CLASS, SPBOOK_CLASS,
    WEAPON_CLASS, ARMOR_CLASS, ARMOR_CLASS, TOOL_CLASS, FOOD_CLASS,
    FOOD_CLASS
};


/* Pluralisation routine. Most of the hard work is now done by lang-??.c,
   when the string is converted into English or some other natural language;
   pluralising something in grammartree is trivial. */
char *
makeplural(const char *oldstr)
{
    char *obuf = nextobuf();
    sprintf(obuf, "N{*,N=%s}", oldstr);
    return obuf;
}

struct o_range {
    const char *name, oclass;
    int f_o_range, l_o_range;
};

/* wishable subranges of objects
 * !nointl{ */
static const struct o_range o_ranges[] = {
    {"bag", TOOL_CLASS, SACK, BAG_OF_TRICKS},
    {"lamp", TOOL_CLASS, OIL_LAMP, MAGIC_LAMP},
    {"candle", TOOL_CLASS, TALLOW_CANDLE, WAX_CANDLE},
    {"horn", TOOL_CLASS, TOOLED_HORN, HORN_OF_PLENTY},
    {"shield", ARMOR_CLASS, SMALL_SHIELD, SHIELD_OF_REFLECTION},
    {"helm", ARMOR_CLASS, ELVEN_LEATHER_HELM, HELM_OF_TELEPATHY},
    {"gloves", ARMOR_CLASS, LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY},
    {"gauntlets", ARMOR_CLASS, LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY},
    {"boots", ARMOR_CLASS, LOW_BOOTS, LEVITATION_BOOTS},
    {"shoes", ARMOR_CLASS, LOW_BOOTS, IRON_SHOES},
    {"cloak", ARMOR_CLASS, MUMMY_WRAPPING, CLOAK_OF_DISPLACEMENT},
    {"shirt", ARMOR_CLASS, HAWAIIAN_SHIRT, T_SHIRT},
    {"dragon scales",
     ARMOR_CLASS, GRAY_DRAGON_SCALES, YELLOW_DRAGON_SCALES},
    {"dragon scale mail",
     ARMOR_CLASS, GRAY_DRAGON_SCALE_MAIL, YELLOW_DRAGON_SCALE_MAIL},
    {"sword", WEAPON_CLASS, SHORT_SWORD, KATANA},
    {"venom", VENOM_CLASS, BLINDING_VENOM, ACID_VENOM},
    {"gray stone", GEM_CLASS, LUCKSTONE, FLINT},
    {"grey stone", GEM_CLASS, LUCKSTONE, FLINT},
};
/* }nointl! */

#define BSTRCMP(base,ptr,string) ((ptr) < base || strcmp((ptr),string))
#define BSTRCMPI(base,ptr,string) ((ptr) < base || strcmpi((ptr),string))
#define BSTRNCMP(base,ptr,string,num) ((ptr)<base || strncmp((ptr),string,num))
#define BSTRNCMPI(base,ptr,string,num) ((ptr)<base||strncmpi((ptr),string,num))

/*
 * Singularize a string the user typed in; this helps reduce the complexity
 * of readobjnam, and is also used in pager.c to singularize the string
 * for which help is sought.
 */
char *
makesingular(const char *oldstr)
{
    char *p, *bp;
    char *str = nextobuf();

    if (!oldstr || !*oldstr) {
        impossible("S{singular of null?}");
        str[0] = 0;
        return str;
    }
    strcpy(str, oldstr);
    bp = str;

    /* !nointl{ */
    while (*bp == ' ')
        bp++;
    /* find "cloves of garlic", "worthless pieces of blue glass" */
    if ((p = strstri(bp, "s of ")) != 0) {
        /* but don't singularize "gauntlets", "boots", "Eyes of the.." */
        if (BSTRNCMPI(bp, p - 3, "Eye", 3) && BSTRNCMP(bp, p - 4, "boot", 4) &&
            BSTRNCMP(bp, p - 8, "gauntlet", 8))
            while ((*p = *(p + 1)) != 0)
                p++;
        return bp;
    }

    /* remove -s or -es (boxes) or -ies (rubies) */
    p = eos(bp);
    if (p >= bp + 1 && p[-1] == 's') {
        if (p >= bp + 2 && p[-2] == 'e') {
            if (p >= bp + 3 && p[-3] == 'i') {
                if (!BSTRCMP(bp, p - 7, "cookies") ||
                    !BSTRCMP(bp, p - 4, "pies"))
                    goto mins;
                strcpy(p - 3, "y");
                return bp;
            }

            /* note: cloves / knives from clove / knife */
            if (!BSTRCMP(bp, p - 6, "knives")) {
                strcpy(p - 3, "fe");
                return bp;
            }
            if (!BSTRCMP(bp, p - 6, "staves")) {
                strcpy(p - 3, "ff");
                return bp;
            }
            if (!BSTRCMPI(bp, p - 6, "leaves")) {
                strcpy(p - 3, "f");
                return bp;
            }
            if (!BSTRCMP(bp, p - 8, "vortices")) {
                strcpy(p - 4, "ex");
                return bp;
            }

            /* note: nurses, axes but boxes */
            if (!BSTRCMP(bp, p - 5, "boxes") || !BSTRCMP(bp, p - 4, "ches")) {
                p[-2] = '\0';
                return bp;
            }

            if (!BSTRCMP(bp, p - 6, "gloves") || !BSTRCMP(bp, p - 6, "lenses")
                || !BSTRCMP(bp, p - 5, "shoes") ||
                !BSTRCMP(bp, p - 6, "scales"))
                return bp;

        } else if (!BSTRCMP(bp, p - 5, "boots") ||
                   !BSTRCMP(bp, p - 9, "gauntlets") ||
                   !BSTRCMP(bp, p - 6, "tricks") ||
                   !BSTRCMP(bp, p - 9, "paralysis") ||
                   !BSTRCMP(bp, p - 5, "glass") ||
                   !BSTRCMP(bp, p - 4, "ness") ||
                   !BSTRCMP(bp, p - 14, "shape changers") ||
                   !BSTRCMP(bp, p - 15, "detect monsters") ||
                   !BSTRCMPI(bp, p - 11, "Aesculapius") || /* staff */
                   !BSTRCMP(bp, p - 10, "eucalyptus") ||
                   !BSTRCMP(bp, p - 9, "iron bars") ||
                   !BSTRCMP(bp, p - 5, "aklys") ||
                   !BSTRCMP(bp, p - 6, "fungus"))
            return bp;
    mins:
        p[-1] = '\0';

    } else {

        if (!BSTRCMP(bp, p - 5, "teeth")) {
            strcpy(p - 5, "tooth");
            return bp;
        }

        if (!BSTRCMP(bp, p - 5, "fungi")) {
            strcpy(p - 5, "fungus");
            return bp;
        }

        /* here we cannot find the plural suffix */
    }
    return bp;
    /* }nointl! */
}

/* compare user string against object name string using fuzzy matching */
static boolean
wishymatch(const char *u_str,   /* from user, so might be variant spelling */
           const char *o_str,   /* from objects[], so is in canonical form */
           boolean retry_inverted)
{       /* optional extra "of" handling */
    /* special case: wizards can wish for traps.  The object is "beartrap" and
       the trap is "bear trap", so to let wizards wish for both we must not
       fuzzymatch. */
    /* !nointl{ */
    if (wizard && !strcmp(o_str, "beartrap"))
        return !strncmpi(o_str, u_str, 8);

    /* ignore spaces & hyphens and upper/lower case when comparing */
    if (fuzzymatch(u_str, o_str, " -", TRUE))
        return TRUE;

    if (retry_inverted) {
        const char *u_of, *o_of;
        char *p, buf[BUFSZ];

        /* when just one of the strings is in the form "foo of bar", convert it 
           into "bar foo" and perform another comparison */
        u_of = strstri(u_str, " of ");
        o_of = strstri(o_str, " of ");
        if (u_of && !o_of) {
            strcpy(buf, u_of + 4);
            p = eos(strcat(buf, " "));
            while (u_str < u_of)
                *p++ = *u_str++;
            *p = '\0';
            return fuzzymatch(buf, o_str, " -", TRUE);
        } else if (o_of && !u_of) {
            strcpy(buf, o_of + 4);
            p = eos(strcat(buf, " "));
            while (o_str < o_of)
                *p++ = *o_str++;
            *p = '\0';
            return fuzzymatch(u_str, buf, " -", TRUE);
        }
    }

    /* [note: if something like "elven speed boots" ever gets added, these
       special cases should be changed to call wishymatch() recursively in
       order to get the "of" inversion handling] */
    if (!strncmp(o_str, "dwarvish ", 9)) {
        if (!strncmpi(u_str, "dwarven ", 8))
            return fuzzymatch(u_str + 8, o_str + 9, " -", TRUE);
    } else if (!strncmp(o_str, "elven ", 6)) {
        if (!strncmpi(u_str, "elvish ", 7))
            return fuzzymatch(u_str + 7, o_str + 6, " -", TRUE);
        else if (!strncmpi(u_str, "elfin ", 6))
            return fuzzymatch(u_str + 6, o_str + 6, " -", TRUE);
    } else if (!strcmp(o_str, "aluminum")) {
        /* this special case doesn't really fit anywhere else... */
        /* (note that " wand" will have been stripped off by now) */
        if (!strcmpi(u_str, "aluminium"))
            return fuzzymatch(u_str + 9, o_str + 8, " -", TRUE);
    }

    return FALSE;
    /* }nointl! */
}

/* alternate spellings; if the difference is only the presence or
   absence of spaces and/or hyphens (such as "pickaxe" vs "pick axe"
   vs "pick-axe") then there is no need for inclusion in this list;
   likewise for ``"of" inversions'' ("boots of speed" vs "speed boots")

   !nointl{ */
static const struct alt_spellings {
    const char *sp;
    int ob;
} spellings[] = {
    {
    "pickax", PICK_AXE}, {
    "whip", BULLWHIP}, {
    "saber", SILVER_SABER}, {
    "silver sabre", SILVER_SABER}, {
    "smooth shield", SHIELD_OF_REFLECTION}, {
    "grey dragon scale mail", GRAY_DRAGON_SCALE_MAIL}, {
    "grey dragon scales", GRAY_DRAGON_SCALES}, {
    "enchant armour", SCR_ENCHANT_ARMOR}, {
    "destroy armour", SCR_DESTROY_ARMOR}, {
    "scroll of enchant armour", SCR_ENCHANT_ARMOR}, {
    "scroll of destroy armour", SCR_DESTROY_ARMOR}, {
    "leather armour", LEATHER_ARMOR}, {
    "studded leather armour", STUDDED_LEATHER_ARMOR}, {
    "iron ball", HEAVY_IRON_BALL}, {
    "lantern", BRASS_LANTERN}, {
    "mattock", DWARVISH_MATTOCK}, {
    "amulet of poison resistance", AMULET_VERSUS_POISON}, {
    "stone", ROCK}, {
    "camera", EXPENSIVE_CAMERA}, {
    "tee shirt", T_SHIRT}, {
    "can", TIN}, {
    "can opener", TIN_OPENER}, {
    "kelp", KELP_FROND}, {
    "eucalyptus", EUCALYPTUS_LEAF}, {
    "grapple", GRAPPLING_HOOK}, {
NULL, 0},};
/* }nointl! */

/*
 * Return something wished for.  Specifying a null pointer for
 * the user request string results in a random object.  Otherwise,
 * if asking explicitly for "nothing" (or "nil") return no_wish;
 * if not an object return &zeroobj; if an error (no matching object),
 * return null.
 * If from_user is false, we're reading from the wizkit, nothing was typed in.
 */
struct obj *
readobjnam(char *bp, struct obj *no_wish, boolean from_user)
{
    char *p;
    int i;
    struct obj *otmp;
    int cnt, spe, spesgn, typ, very, rechrg;
    int blessed, uncursed, iscursed, ispoisoned, isgreased;
    int eroded, eroded2, erodeproof;

#ifdef INVISIBLE_OBJECTS
    int isinvisible;
#endif
    int halfeaten, mntmp, contents;
    int islit, unlabeled, ishistoric, isdiluted;
    const struct alt_spellings *as = spellings;
    struct fruit *f;
    int ftype = current_fruit;
    char fruitbuf[BUFSZ];

    /* Fruits may not mess up the ability to wish for real objects (since you
       can leave a fruit in a bones file and it will be added to another
       person's game), so they must be checked for last, after stripping all
       the possible prefixes and seeing if there's a real name in there.  So we 
       have to save the full original name.  However, it's still possible to do 
       things like "uncursed burnt Alaska", or worse yet, "2 burned 5 course
       meals", so we need to loop to strip off the prefixes again, this time
       stripping only the ones possible on food. We could get even more
       detailed so as to allow food names with prefixes that _are_ possible on
       food, so you could wish for "2 3 alarm chilis".  Currently this isn't
       allowed; options.c automatically sticks 'candied' in front of such
       names. */

    char oclass;
    char *un, *dn, *actualn;
    const char *name = 0;

    cnt = spe = spesgn = typ = very = rechrg = blessed = uncursed = iscursed =
#ifdef INVISIBLE_OBJECTS
        isinvisible =
#endif
        ispoisoned = isgreased = eroded = eroded2 = erodeproof = halfeaten =
        islit = unlabeled = ishistoric = isdiluted = 0;
    mntmp = NON_PM;
#define UNDEFINED 0
#define EMPTY 1
#define SPINACH 2
    contents = UNDEFINED;
    oclass = 0;
    actualn = dn = un = 0;

    /* !nointl{ */
    if (!bp)
        goto any;
    /* first, remove extra whitespace they may have typed */
    mungspaces(bp);
    /* allow wishing for "nothing" to preserve wishless conduct... [now
       requires "wand of nothing" if that's what was really wanted] */
    if (!strcmpi(bp, "nothing") || !strcmpi(bp, "nil") || !strcmpi(bp, "none"))
        return no_wish;
    /* save the [nearly] unmodified choice string */
    strcpy(fruitbuf, bp);

    for (;;) {
        int l;

        if (!bp || !*bp)
            goto any;
        if (!strncmpi(bp, "an ", l = 3) || !strncmpi(bp, "a ", l = 2)) {
            cnt = 1;
        } else if (!strncmpi(bp, "the ", l = 4)) {
            ;   /* just increment `bp' by `l' below */
        } else if (!cnt && digit(*bp) && strcmp(bp, "0")) {
            cnt = atoi(bp);
            while (digit(*bp))
                bp++;
            while (*bp == ' ')
                bp++;
            l = 0;
        } else if (*bp == '+' || *bp == '-') {
            spesgn = (*bp++ == '+') ? 1 : -1;
            spe = atoi(bp);
            while (digit(*bp))
                bp++;
            while (*bp == ' ')
                bp++;
            l = 0;
        } else if (!strncmpi(bp, "blessed ", l = 8) ||
                   !strncmpi(bp, "holy ", l = 5)) {
            blessed = 1;
        } else if (!strncmpi(bp, "cursed ", l = 7) ||
                   !strncmpi(bp, "unholy ", l = 7)) {
            iscursed = 1;
        } else if (!strncmpi(bp, "uncursed ", l = 9)) {
            uncursed = 1;
#ifdef INVISIBLE_OBJECTS
        } else if (!strncmpi(bp, "invisible ", l = 10)) {
            isinvisible = 1;
#endif
        } else if (!strncmpi(bp, "rustproof ", l = 10) ||
                   !strncmpi(bp, "erodeproof ", l = 11) ||
                   !strncmpi(bp, "corrodeproof ", l = 13) ||
                   !strncmpi(bp, "fixed ", l = 6) ||
                   !strncmpi(bp, "fireproof ", l = 10) ||
                   !strncmpi(bp, "rotproof ", l = 9)) {
            erodeproof = 1;
        } else if (!strncmpi(bp, "lit ", l = 4) ||
                   !strncmpi(bp, "burning ", l = 8)) {
            islit = 1;
        } else if (!strncmpi(bp, "unlit ", l = 6) ||
                   !strncmpi(bp, "extinguished ", l = 13)) {
            islit = 0;
            /* "unlabeled" and "blank" are synonymous */
        } else if (!strncmpi(bp, "unlabeled ", l = 10) ||
                   !strncmpi(bp, "unlabelled ", l = 11) ||
                   !strncmpi(bp, "blank ", l = 6)) {
            unlabeled = 1;
        } else if (!strncmpi(bp, "poisoned ", l = 9)
                   || (wizard && !strncmpi(bp, "trapped ", l = 8))) {
            ispoisoned = 1;
        } else if (!strncmpi(bp, "greased ", l = 8)) {
            isgreased = 1;
        } else if (!strncmpi(bp, "very ", l = 5)) {
            /* very rusted very heavy iron ball */
            very = 1;
        } else if (!strncmpi(bp, "thoroughly ", l = 11)) {
            very = 2;
        } else if (!strncmpi(bp, "rusty ", l = 6) ||
                   !strncmpi(bp, "rusted ", l = 7) ||
                   !strncmpi(bp, "burnt ", l = 6) ||
                   !strncmpi(bp, "burned ", l = 7)) {
            eroded = 1 + very;
            very = 0;
        } else if (!strncmpi(bp, "corroded ", l = 9) ||
                   !strncmpi(bp, "rotted ", l = 7)) {
            eroded2 = 1 + very;
            very = 0;
        } else if (!strncmpi(bp, "partly eaten ", l = 13)) {
            halfeaten = 1;
        } else if (!strncmpi(bp, "historic ", l = 9)) {
            ishistoric = 1;
        } else if (!strncmpi(bp, "diluted ", l = 8)) {
            isdiluted = 1;
        } else if (!strncmpi(bp, "empty ", l = 6)) {
            contents = EMPTY;
        } else
            break;
        bp += l;
    }
    if (!cnt)
        cnt = 1;        /* %% what with "gems" etc. ? */
    if (strlen(bp) > 1) {
        if ((p = strrchr(bp, '(')) != 0) {
            if (p > bp && p[-1] == ' ')
                p[-1] = 0;
            else
                *p = 0;
            p++;
            if (!strcmpi(p, "lit)")) {
                islit = 1;
            } else {
                spe = atoi(p);
                while (digit(*p))
                    p++;
                if (*p == ':') {
                    p++;
                    rechrg = spe;
                    spe = atoi(p);
                    while (digit(*p))
                        p++;
                }
                if (*p != ')') {
                    spe = rechrg = 0;
                } else {
                    spesgn = 1;
                    p++;
                    if (*p)
                        strcat(bp, p);
                }
            }
        }
    }
/*
   otmp->spe is type schar; so we don't want spe to be any bigger or smaller.
   also, spe should always be positive  -- some cheaters may try to confuse
   atoi()
*/
    if (spe < 0) {
        spesgn = -1;    /* cheaters get what they deserve */
        spe = abs(spe);
    }
    if (spe > SCHAR_LIM)
        spe = SCHAR_LIM;
    if (rechrg < 0 || rechrg > 7)
        rechrg = 7;     /* recharge_limit */

    /* now we have the actual name, as delivered by xname, say green potions
       called whisky scrolls labeled "QWERTY" egg fortune cookies very heavy
       iron ball named hoei wand of wishing elven cloak */
    if ((p = strstri(bp, " named ")) != 0) {
        *p = 0;
        name = p + 7;
    }
    if ((p = strstri(bp, " called ")) != 0) {
        *p = 0;
        un = p + 8;
        /* "helmet called telepathy" is not "helmet" (a specific type) "shield
           called reflection" is not "shield" (a general type) */
        for (i = 0; i < SIZE(o_ranges); i++)
            if (!strcmpi(bp, o_ranges[i].name)) {
                oclass = o_ranges[i].oclass;
                goto srch;
            }
    }
    if ((p = strstri(bp, " labeled ")) != 0) {
        *p = 0;
        dn = p + 9;
    } else if ((p = strstri(bp, " labelled ")) != 0) {
        *p = 0;
        dn = p + 10;
    }
    if ((p = strstri(bp, " of spinach")) != 0) {
        *p = 0;
        contents = SPINACH;
    }

    /* 
       Skip over "pair of ", "pairs of", "set of" and "sets of".

       Accept "3 pair of boots" as well as "3 pairs of boots". It is valid
       English either way.  See makeplural() for more on pair/pairs.

       We should only double count if the object in question is not refered to
       as a "pair of".  E.g. We should double if the player types "pair of
       spears", but not if the player types "pair of lenses".  Luckily (?) all
       objects that are refered to as pairs -- boots, gloves, and lenses -- are 
       also not mergable, so cnt is ignored anyway. */
    if (!strncmpi(bp, "pair of ", 8)) {
        bp += 8;
        cnt *= 2;
    } else if (cnt > 1 && !strncmpi(bp, "pairs of ", 9)) {
        bp += 9;
        cnt *= 2;
    } else if (!strncmpi(bp, "set of ", 7)) {
        bp += 7;
    } else if (!strncmpi(bp, "sets of ", 8)) {
        bp += 8;
    }

    /* 
     * Find corpse type using "of" (figurine of an orc, tin of orc meat)
     * Don't check if it's a wand or spellbook.
     * (avoid "wand/finger of death" confusion).
     */
    if (!strstri(bp, "wand ")
        && !strstri(bp, "spellbook ")
        && !strstri(bp, "finger ")) {
        if ((p = strstri(bp, " of ")) != 0 &&
            (mntmp = name_to_mon(p + 4)) >= LOW_PM)
            *p = 0;
    }
    /* Find corpse type w/o "of" (red dragon scale mail, yeti corpse) */
    if (strncmpi(bp, "samurai sword", 13))      /* not the "samurai" monster! */
        if (strncmpi(bp, "wizard lock", 11))    /* not the "wizard" monster! */
            if (strncmpi(bp, "ninja-to", 8))    /* not the "ninja" rank */
                if (strncmpi(bp, "master key", 10)) /* not the "master" rank */
                    if (strncmpi(bp, "magenta", 7)) /* not the "mage" rank */
                        if (mntmp < LOW_PM && strlen(bp) > 2 &&
                            (mntmp = name_to_mon(bp)) >= LOW_PM) {
                            int mntmptoo, mntmplen;     /* double check for
                                                           rank title */
                            char *obp = bp;

                            mntmptoo = title_to_mon(bp, NULL, &mntmplen);
                            bp += mntmp != mntmptoo ?
                                (int)strlen(mons[mntmp].mname) : mntmplen;
                            if (*bp == ' ')
                                bp++;
                            else if (!strncmpi(bp, "s ", 2))
                                bp += 2;
                            else if (!strncmpi(bp, "es ", 3))
                                bp += 3;
                            else if (!*bp && !actualn && !dn && !un && !oclass) {
                                /* no referent; they don't really mean a
                                   monster type */
                                bp = obp;
                                mntmp = NON_PM;
                            }
                        }

    /* first change to singular if necessary */
    if (*bp) {
        char *sng = makesingular(bp);

        if (strcmp(bp, sng)) {
            if (cnt == 1)
                cnt = 2;
            strcpy(bp, sng);
        }
    }

    /* Alternate spellings (pick-ax, silver sabre, &c) */
    while (as->sp) {
        if (fuzzymatch(bp, as->sp, " -", TRUE)) {
            typ = as->ob;
            goto typfnd;
        }
        as++;
    }
    /* can't use spellings list for this one due to shuffling */
    if (!strncmpi(bp, "grey spell", 10))
        *(bp + 2) = 'a';

    /* dragon scales - assumes order of dragons */
    if (!strcmpi(bp, "scales") && mntmp >= PM_GRAY_DRAGON &&
        mntmp <= PM_YELLOW_DRAGON) {
        typ = GRAY_DRAGON_SCALES + mntmp - PM_GRAY_DRAGON;
        mntmp = NON_PM; /* no monster */
        goto typfnd;
    }

    p = eos(bp);
    if (!BSTRCMPI(bp, p - 10, "holy water")) {
        typ = POT_WATER;
        if ((p - bp) >= 12 && *(p - 12) == 'u')
            iscursed = 1;       /* unholy water */
        else
            blessed = 1;
        goto typfnd;
    }
    if (unlabeled && !BSTRCMPI(bp, p - 6, "scroll")) {
        typ = SCR_BLANK_PAPER;
        goto typfnd;
    }
    if (unlabeled && !BSTRCMPI(bp, p - 9, "spellbook")) {
        typ = SPE_BLANK_PAPER;
        goto typfnd;
    }
    /* 
     * NOTE: Gold pieces are handled as objects nowadays, and therefore
     * this section should probably be reconsidered as well as the entire
     * gold/money concept.  Maybe we want to add other monetary units as
     * well in the future. (TH)
     */
    if (!BSTRCMPI(bp, p - 10, "gold piece") || !BSTRCMPI(bp, p - 7, "zorkmid")
        || !strcmpi(bp, "gold") || !strcmpi(bp, "money") || !strcmpi(bp, "coin")
        || *bp == GOLD_SYM) {
        if (cnt > 5000 && !wizard)
            cnt = 5000;
        if (cnt < 1)
            cnt = 1;
        otmp = mksobj(level, GOLD_PIECE, FALSE, FALSE);
        otmp->quan = cnt;
        otmp->owt = weight(otmp);
        iflags.botl = 1;
        return otmp;
    }
    if (strlen(bp) == 1 && (i = def_char_to_objclass(*bp)) < MAXOCLASSES &&
        i > ILLOBJ_CLASS && (wizard || i != VENOM_CLASS)) {
        oclass = i;
        goto any;
    }

    /* Search for class names: XXXXX potion, scroll of XXXXX.  Avoid */
    /* false hits on, e.g., rings for "ring mail". */
    if (strncmpi(bp, "enchant ", 8) && strncmpi(bp, "destroy ", 8) &&
        strncmpi(bp, "food detection", 14) && strncmpi(bp, "ring mail", 9) &&
        strncmpi(bp, "studded leather arm", 19) &&
        strncmpi(bp, "leather arm", 11) && strncmpi(bp, "tooled horn", 11) &&
        strncmpi(bp, "food ration", 11) && strncmpi(bp, "meat ring", 9)
        )
        for (i = 0; i < (int)(sizeof wrpsym); i++) {
            int j = strlen(wrp[i]);

            if (!strncmpi(bp, wrp[i], j)) {
                oclass = wrpsym[i];
                if (oclass != AMULET_CLASS) {
                    bp += j;
                    if (!strncmpi(bp, " of ", 4))
                        actualn = bp + 4;
                    /* else if (*bp) ?? */
                } else
                    actualn = bp;
                goto srch;
            }
            if (!BSTRCMPI(bp, p - j, wrp[i])) {
                oclass = wrpsym[i];
                p -= j;
                *p = 0;
                if (p > bp && p[-1] == ' ')
                    p[-1] = 0;
                actualn = dn = bp;
                goto srch;
            }
        }

    /* "grey stone" check must be before general "stone" */
    for (i = 0; i < SIZE(o_ranges); i++)
        if (!strcmpi(bp, o_ranges[i].name)) {
            typ = rnd_class(o_ranges[i].f_o_range, o_ranges[i].l_o_range);
            goto typfnd;
        }

    if (!BSTRCMPI(bp, p - 6, " stone")) {
        p[-6] = 0;
        oclass = GEM_CLASS;
        dn = actualn = bp;
        goto srch;
    } else if (!strcmpi(bp, "looking glass")) {
        ;       /* avoid false hit on "* glass" */
    } else if (!BSTRCMPI(bp, p - 6, " glass") || !strcmpi(bp, "glass")) {
        char *g = bp;

        if (strstri(g, "broken"))
            return NULL;
        if (!strncmpi(g, "worthless ", 10))
            g += 10;
        if (!strncmpi(g, "piece of ", 9))
            g += 9;
        if (!strncmpi(g, "colored ", 8))
            g += 8;
        else if (!strncmpi(g, "coloured ", 9))
            g += 9;
        if (!strcmpi(g, "glass")) {     /* choose random color */
            /* 9 different kinds */
            typ = LAST_GEM + rnd(9);
            if (objects[typ].oc_class == GEM_CLASS)
                goto typfnd;
            else
                typ = 0;        /* somebody changed objects[]? punt */
        } else {        /* try to construct canonical form */
            char tbuf[BUFSZ];

            strcpy(tbuf, "worthless piece of ");
            strcat(tbuf, g);    /* assume it starts with the color */
            strcpy(bp, tbuf);
        }
    }

    actualn = bp;
    if (!dn)
        dn = actualn;   /* ex. "skull cap" */
srch:
    /* check real names of gems first */
    if (!oclass && actualn) {
        for (i = bases[GEM_CLASS]; i <= LAST_GEM; i++) {
            const char *zn;

            if ((zn = OBJ_NAME(objects[i])) && !strcmpi(actualn, zn)) {
                typ = i;
                goto typfnd;
            }
        }
    }
    i = oclass ? bases[(int)oclass] : 1;
    while (i < NUM_OBJECTS && (!oclass || objects[i].oc_class == oclass)) {
        const char *zn;

        if (actualn && (zn = OBJ_NAME(objects[i])) != 0 &&
            wishymatch(actualn, zn, TRUE)) {
            typ = i;
            goto typfnd;
        }
        if (dn && (zn = OBJ_DESCR(objects[i])) != 0 &&
            wishymatch(dn, zn, FALSE)) {
            /* don't match extra descriptions (w/o real name) */
            if (!OBJ_NAME(objects[i]))
                return NULL;
            typ = i;
            goto typfnd;
        }
        if (un && (zn = objects[i].oc_uname) != 0 && wishymatch(un, zn, FALSE)) {
            typ = i;
            goto typfnd;
        }
        i++;
    }
    if (actualn) {
        const struct Jitem *j = Japanese_items;

        while (j->item) {
            if (actualn && !strcmpi(actualn, j->name)) {
                typ = j->item;
                goto typfnd;
            }
            j++;
        }
    }
    if (!strcmpi(bp, "spinach")) {
        contents = SPINACH;
        typ = TIN;
        goto typfnd;
    }
    /* Note: not strncmpi.  2 fruits, one capital, one not, are possible. */
    {
        char *fp;
        int l, cntf;
        int blessedf, iscursedf, uncursedf, halfeatenf;

        blessedf = iscursedf = uncursedf = halfeatenf = 0;
        cntf = 0;

        fp = fruitbuf;
        for (;;) {
            if (!fp || !*fp)
                break;
            if (!strncmpi(fp, "an ", l = 3) || !strncmpi(fp, "a ", l = 2)) {
                cntf = 1;
            } else if (!cntf && digit(*fp)) {
                cntf = atoi(fp);
                while (digit(*fp))
                    fp++;
                while (*fp == ' ')
                    fp++;
                l = 0;
            } else if (!strncmpi(fp, "blessed ", l = 8)) {
                blessedf = 1;
            } else if (!strncmpi(fp, "cursed ", l = 7)) {
                iscursedf = 1;
            } else if (!strncmpi(fp, "uncursed ", l = 9)) {
                uncursedf = 1;
            } else if (!strncmpi(fp, "partly eaten ", l = 13)) {
                halfeatenf = 1;
            } else
                break;
            fp += l;
        }

        for (f = ffruit; f; f = f->nextf) {
            char *f1 = f->fname, *f2 = makeplural(f->fname);

            if (!strncmp(fp, f1, strlen(f1)) || !strncmp(fp, f2, strlen(f2))) {
                typ = SLIME_MOLD;
                blessed = blessedf;
                iscursed = iscursedf;
                uncursed = uncursedf;
                halfeaten = halfeatenf;
                cnt = cntf;
                ftype = f->fid;
                goto typfnd;
            }
        }

        /* make it possible to wish for "fruit" */
        if (!strcmpi(bp, "fruit")) {
            typ = SLIME_MOLD;
            goto typfnd;
        }
    }

    if (!oclass && actualn) {
        short objtyp;

        /* Perhaps it's an artifact specified by name, not type */
        name = artifact_name(actualn, &objtyp);
        if (name) {
            typ = objtyp;
            goto typfnd;
        }
    }

    /* Let wizards wish for traps --KAA */
    /* must come after objects check so wizards can still wish for trap objects 
       like beartraps */
    if (wizard && from_user) {
        int trap;

        for (trap = NO_TRAP + 1; trap < TRAPNUM; trap++) {
            const char *tname;

            tname = trapexplain[trap - 1];
            if (!strncmpi(tname, bp, strlen(tname))) {
                /* avoid stupid mistakes */
                if ((trap == TRAPDOOR || trap == HOLE)
                    && !can_fall_thru(level))
                    trap = ROCKTRAP;
                maketrap(level, u.ux, u.uy, trap);
                return &zeroobj;
            }
        }
        /* or some other dungeon features -dlc */
        p = eos(bp);
        if (!BSTRCMP(bp, p - 8, "fountain")) {
            level->locations[u.ux][u.uy].typ = FOUNTAIN;
            level->flags.nfountains++;
            if (!strncmpi(bp, "magic ", 6))
                level->locations[u.ux][u.uy].blessedftn = 1;
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 6, "throne")) {
            level->locations[u.ux][u.uy].typ = THRONE;
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 4, "sink")) {
            level->locations[u.ux][u.uy].typ = SINK;
            level->flags.nsinks++;
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 4, "pool")) {
            level->locations[u.ux][u.uy].typ = POOL;
            del_engr_at(level, u.ux, u.uy);
            /* Must manually make kelp! */
            water_damage(level->objects[u.ux][u.uy], FALSE, TRUE);
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 4, "lava")) {      /* also matches "molten lava" */
            level->locations[u.ux][u.uy].typ = LAVAPOOL;
            del_engr_at(level, u.ux, u.uy);
            if (!(Levitation || Flying))
                lava_effects();
            newsym(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 5, "altar")) {
            aligntyp al;

            level->locations[u.ux][u.uy].typ = ALTAR;
            if (!strncmpi(bp, "chaotic ", 8))
                al = A_CHAOTIC;
            else if (!strncmpi(bp, "neutral ", 8))
                al = A_NEUTRAL;
            else if (!strncmpi(bp, "lawful ", 7))
                al = A_LAWFUL;
            else if (!strncmpi(bp, "unaligned ", 10))
                al = A_NONE;
            else        /* -1 - A_CHAOTIC, 0 - A_NEUTRAL, 1 - A_LAWFUL */
                al = (!rn2(6)) ? A_NONE : rn2((int)A_LAWFUL + 2) - 1;
            level->locations[u.ux][u.uy].altarmask = Align2amask(al);
            newsym(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 5, "grave") || !BSTRCMP(bp, p - 9, "headstone")) {
            make_grave(level, u.ux, u.uy, NULL);
            newsym(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 4, "tree")) {
            level->locations[u.ux][u.uy].typ = TREE;
            newsym(u.ux, u.uy);
            block_point(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 4, "bars")) {
            level->locations[u.ux][u.uy].typ = IRONBARS;
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
    }

    if (!oclass)
        return NULL;
    /* }nointl! */
any:
    if (!oclass)
        oclass = wrpsym[rn2((int)sizeof (wrpsym))];
typfnd:
    if (typ)
        oclass = objects[typ].oc_class;

    /* check for some objects that are not allowed */
    if (typ && objects[typ].oc_unique) {
        if (!wizard)
            switch (typ) {
            case AMULET_OF_YENDOR:
                typ = FAKE_AMULET_OF_YENDOR;
                break;
            case CANDELABRUM_OF_INVOCATION:
                typ = rnd_class(TALLOW_CANDLE, WAX_CANDLE);
                break;
            case BELL_OF_OPENING:
                typ = BELL;
                break;
            case SPE_BOOK_OF_THE_DEAD:
                typ = SPE_BLANK_PAPER;
                break;
            }
    }

    /* catch any other non-wishable objects */
    if (objects[typ].oc_nowish && !wizard)
        return NULL;

    /* convert magic lamps to regular lamps before lighting them or setting the 
       charges */
    if (typ == MAGIC_LAMP && !wizard)
        typ = OIL_LAMP;

    if (typ) {
        otmp = mksobj(level, typ, TRUE, FALSE);
    } else {
        otmp = mkobj(level, oclass, FALSE);
        if (otmp)
            typ = otmp->otyp;
    }

    if (islit &&
        (typ == OIL_LAMP || typ == MAGIC_LAMP || typ == BRASS_LANTERN ||
         Is_candle(otmp) || typ == POT_OIL)) {
        place_object(otmp, level, u.ux, u.uy);  /* make it viable light source */
        begin_burn(otmp, FALSE);
        obj_extract_self(otmp); /* now release it for caller's use */
    }

    if (cnt > 0 && objects[typ].oc_merge && oclass != SPBOOK_CLASS &&
        (cnt < rnd(6) || wizard || (cnt <= 7 && Is_candle(otmp)) ||
         (cnt <= 20 && ((oclass == WEAPON_CLASS && is_ammo(otmp))
                        || typ == ROCK || is_missile(otmp)))))
        otmp->quan = (long)cnt;

    if (oclass == VENOM_CLASS)
        otmp->spe = 1;

    if (spesgn == 0)
        spe = otmp->spe;
    else if (wizard)    /* no alteration to spe */
        ;
    else if (oclass == ARMOR_CLASS || oclass == WEAPON_CLASS || is_weptool(otmp)
             || (oclass == RING_CLASS && objects[typ].oc_charged)) {
        if (spe > rnd(5) && spe > otmp->spe)
            spe = 0;
        if (spe > 2 && Luck < 0)
            spesgn = -1;
    } else {
        if (oclass == WAND_CLASS) {
            if (spe > 1 && spesgn == -1)
                spe = 1;
        } else {
            if (spe > 0 && spesgn == -1)
                spe = 0;
        }
        if (spe > otmp->spe)
            spe = otmp->spe;
    }

    if (spesgn == -1)
        spe = -spe;

    /* set otmp->spe.  This may, or may not, use spe... */
    switch (typ) {
    case TIN:
        if (contents == EMPTY) {
            otmp->corpsenm = NON_PM;
            otmp->spe = 0;
        } else if (contents == SPINACH) {
            otmp->corpsenm = NON_PM;
            otmp->spe = 1;
        }
        break;
    case SLIME_MOLD:
        otmp->spe = ftype;
        /* Fall through */
    case SKELETON_KEY:
    case CHEST:
    case LARGE_BOX:
    case HEAVY_IRON_BALL:
    case IRON_CHAIN:
    case STATUE:
        /* otmp->cobj already done in mksobj(level, ) */
        break;
    case WAN_WISHING:
        if (!wizard) {
            otmp->spe = (rn2(10) ? -1 : 0);
            break;
        }
        /* fall through, if wizard */
    default:
        otmp->spe = spe;
    }

    /* set otmp->corpsenm or dragon scale [mail] */
    if (mntmp >= LOW_PM) {
        if (mntmp == PM_LONG_WORM_TAIL)
            mntmp = PM_LONG_WORM;

        switch (typ) {
        case TIN:
            otmp->spe = 0;      /* No spinach */
            if (dead_species(mntmp, FALSE)) {
                otmp->corpsenm = NON_PM;        /* it's empty */
            } else if (!(mons[mntmp].geno & G_UNIQ) &&
                       !(mvitals[mntmp].mvflags & G_NOCORPSE) &&
                       mons[mntmp].cnutrit != 0) {
                otmp->corpsenm = mntmp;
            }
            break;
        case CORPSE:
            if (!(mons[mntmp].geno & G_UNIQ) &&
                !(mvitals[mntmp].mvflags & G_NOCORPSE)) {
                /* beware of random troll or lizard corpse, or of ordinary one
                   being forced to such */
                if (otmp->timed)
                    obj_stop_timers(otmp);
                if (mons[mntmp].msound == MS_GUARDIAN)
                    otmp->corpsenm = genus(mntmp, 1);
                else
                    otmp->corpsenm = mntmp;
                start_corpse_timeout(otmp);
            }
            break;
        case FIGURINE:
            if (!(mons[mntmp].geno & G_UNIQ)
                && !is_human(&mons[mntmp]))
                otmp->corpsenm = mntmp;
            break;
        case EGG:
            mntmp = can_be_hatched(mntmp);
            if (mntmp != NON_PM) {
                otmp->corpsenm = mntmp;
                if (!dead_species(mntmp, TRUE))
                    attach_egg_hatch_timeout(otmp);
                else
                    kill_egg(otmp);
            }
            break;
        case STATUE:
            otmp->corpsenm = mntmp;
            if (Has_contents(otmp) && verysmall(&mons[mntmp]))
                delete_contents(otmp);  /* no spellbook */
            otmp->spe = ishistoric ? STATUE_HISTORIC : 0;
            break;
        case SCALE_MAIL:
            /* Dragon mail - depends on the order of objects */
            /* & dragons.  */
            if (mntmp >= PM_GRAY_DRAGON && mntmp <= PM_YELLOW_DRAGON)
                otmp->otyp = GRAY_DRAGON_SCALE_MAIL + mntmp - PM_GRAY_DRAGON;
            break;
        }
    }

    /* set blessed/cursed -- setting the fields directly is safe since weight() 
       is called below and addinv() will take care of luck */
    if (iscursed) {
        curse(otmp);
    } else if (uncursed) {
        otmp->blessed = 0;
        otmp->cursed = (Luck < 0 && !wizard);
    } else if (blessed) {
        otmp->blessed = (Luck >= 0 || wizard);
        otmp->cursed = (Luck < 0 && !wizard);
    } else if (spesgn < 0) {
        curse(otmp);
    }
#ifdef INVISIBLE_OBJECTS
    if (isinvisible)
        otmp->oinvis = 1;
#endif

    /* set eroded */
    if (is_damageable(otmp) || otmp->otyp == CRYSKNIFE) {
        if (eroded && (is_flammable(otmp) || is_rustprone(otmp)))
            otmp->oeroded = eroded;
        if (eroded2 && (is_corrodeable(otmp) || is_rottable(otmp)))
            otmp->oeroded2 = eroded2;

        /* set erodeproof */
        if (erodeproof && !eroded && !eroded2)
            otmp->oerodeproof = (Luck >= 0 || wizard);
    }

    /* set otmp->recharged */
    if (oclass == WAND_CLASS) {
        /* prevent wishing abuse */
        if (otmp->otyp == WAN_WISHING && !wizard)
            rechrg = 1;
        otmp->recharged = (unsigned)rechrg;
    }

    /* set poisoned */
    if (ispoisoned) {
        if (is_poisonable(otmp))
            otmp->opoisoned = (Luck >= 0);
        else if (Is_box(otmp) || typ == TIN)
            otmp->otrapped = 1;
        else if (oclass == FOOD_CLASS)
            /* try to taint by making it as old as possible */
            otmp->age = 1L;
    }

    if (isgreased)
        otmp->greased = 1;

    if (isdiluted && otmp->oclass == POTION_CLASS && otmp->otyp != POT_WATER)
        otmp->odiluted = 1;

    if (name) {
        const char *aname;
        short objtyp;

        /* an artifact name might need capitalization fixing */
        aname = artifact_name(name, &objtyp);
        if (aname && objtyp == otmp->otyp)
            name = aname;

        otmp = oname(otmp, name);
        if (otmp->oartifact) {
            otmp->quan = 1L;
            u.uconduct.wisharti++;      /* KMH, conduct */
        }
    }

    /* more wishing abuse: don't allow wishing for certain artifacts */
    /* and make them pay; charge them for the wish anyway! */
    if ((is_quest_artifact(otmp) ||
         (otmp->oartifact && rn2(nartifact_exist()) > 1)) && !wizard) {
        artifact_exists(otmp, ONAME(otmp), FALSE);
        obfree(otmp, NULL);
        otmp = &zeroobj;
        pline("C{N{wish},V{fail}}!");
    }

    if (halfeaten && otmp->oclass == FOOD_CLASS) {
        if (otmp->otyp == CORPSE)
            otmp->oeaten = mons[otmp->corpsenm].cnutrit;
        else
            otmp->oeaten = objects[otmp->otyp].oc_nutrition;
        /* (do this adjustment before setting up object's weight) */
        consume_oeaten(otmp, 1);
    }
    otmp->owt = weight(otmp);
    if (very && otmp->otyp == HEAVY_IRON_BALL)
        otmp->owt += 160;

    return otmp;
}

int
rnd_class(int first, int last)
{
    int i, x, sum = 0;

    if (first == last)
        return first;
    for (i = first; i <= last; i++)
        sum += objects[i].oc_prob;
    if (!sum)   /* all zero */
        return first + rn2(last - first + 1);
    x = rnd(sum);
    for (i = first; i <= last; i++)
        if (objects[i].oc_prob && (x -= objects[i].oc_prob) <= 0)
            return i;
    return 0;
}

static const char *
Japanese_item_name(int i)
{
    const struct Jitem *j = Japanese_items;

    while (j->item) {
        if (i == j->item)
            return j->name;
        j++;
    }
    return NULL;
}

const char *
cloak_simple_name(const struct obj *cloak)
{
    if (cloak) {
        switch (cloak->otyp) {
        case ROBE:
            return "N{robe}";
        case MUMMY_WRAPPING:
            return "N{wrapping}";
        case ALCHEMY_SMOCK:
            return (objects[cloak->otyp].oc_name_known &&
                    cloak->dknown) ? "N{smock}" : "N{apron}";
        default:
            break;
        }
    }
    return "N{cloak}";
}

const char *
mimic_obj_name(const struct monst *mtmp)
{
    if (mtmp->m_ap_type == M_AP_OBJECT && mtmp->mappearance != STRANGE_OBJECT) {
        int idx = objects[mtmp->mappearance].oc_descr_idx;

        if (mtmp->mappearance == GOLD_PIECE)
            return "N{gold}";
        return obj_descr[idx].oc_name;
    }
    return "N{whatcha-may-callit}";
}

/*objnam.c*/
