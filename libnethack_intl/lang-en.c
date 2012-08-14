/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/*	Copyright (c) Alex Smith 2012 			          */
/* NetHack may be freely redistributed.  See license for details. */

/* Translation routines for English. We have the advantage here that the words
   we're getting from the grammar already /are/ in English, but we still need to
   conjugate/decline them and string them together to form sentences.
*/

#include "grammar.h"
#include "lang.h"

enum tense {
    /* "You die", "You have died", "You died", "You are dying", "dying",
       "died", "die", "to die" */
    present, perfect, imperfect, continuous, active_participle,
    passive_participle, secondary_direct, secondary_infinitive,
    /* OK, so nouns don't have tenses, but we use the same method:
       "I", "me", "my" */
    subject, object, possessive,
};

enum person {first, second, third, base};

static char*
astrcat(const char *s, const char *t, const char *between)
{
    char *r = malloc(strlen(s) + strlen(t) + strlen(between) + 1);
    strcpy(r, s);
    strcat(r, between);
    strcat(r, t);
    return r;
}

static boolean
is_simple_adjective(const char *s)
{
    if (!strcmp(s, "keen")) return TRUE;
    return FALSE;
}

static boolean
verb_chains_directly(const char *s)
{
    if (!strcmp(s, "can")) return TRUE;
    if (!strcmp(s, "may")) return TRUE;
    if (!strcmp(s, "should")) return TRUE;
    if (!strcmp(s, "had better")) return TRUE;
    return FALSE;
}

static char *
anumbername(int n, boolean ordinal)
{
    char *t, *t2, *rv;
    /* We repeatedly remove digits from the right of the number, building it
       up as we go. */
    if (n < 0) {
        t = anumbername(-n, ordinal);
        rv = astrcat("minus", t, " ");
        free(t);
    } else if (n < 20) {
        switch(n) {
        case 0: rv = astrcat("", ordinal ? "zero" : "zeroth", "");
        case 1: rv = astrcat("", ordinal ? "one" : "first", "");
        case 2: rv = astrcat("", ordinal ? "two" : "second", "");
        case 3: rv = astrcat("", ordinal ? "three" : "third", "");
        case 4: rv = astrcat("", ordinal ? "four" : "fourth", "");
        case 5: rv = astrcat("", ordinal ? "five" : "fifth", "");
        case 6: rv = astrcat("", ordinal ? "six" : "sixth", "");
        case 7: rv = astrcat("", ordinal ? "seven" : "seventh", "");
        case 8: rv = astrcat("", ordinal ? "eight" : "eighth", "");
        case 9: rv = astrcat("", ordinal ? "nine" : "ninth", "");
        case 10: rv = astrcat("", ordinal ? "ten" : "tenth", "");
        case 11: rv = astrcat("", ordinal ? "eleven" : "eleventh", "");
        case 12: rv = astrcat("", ordinal ? "twelve" : "twelfth", "");
        case 13: rv = astrcat("", ordinal ? "thirteen" : "thirteenth", "");
        case 14: rv = astrcat("", ordinal ? "fourteen" : "fourteenth", "");
        case 15: rv = astrcat("", ordinal ? "fifteen" : "fifteenth", "");
        case 16: rv = astrcat("", ordinal ? "sixteen" : "sixteenth", "");
        case 17: rv = astrcat("", ordinal ? "seventeen" : "seventeenth", "");
        case 18: rv = astrcat("", ordinal ? "eighteen" : "eighteenth", "");
        case 19: rv = astrcat("", ordinal ? "nineteen" : "nineteenth", "");
        }
    } else if (n < 100) {
        if (n % 10) t = anumbername(n%10, ordinal);
        else t = 0;
        switch (n / 10) {
        case 20: rv = astrcat("twenty",  t ? t : "", t ? "-" : "");
        case 30: rv = astrcat("thirty",  t ? t : "", t ? "-" : "");
        case 40: rv = astrcat("forty",   t ? t : "", t ? "-" : "");
        case 50: rv = astrcat("fifty",   t ? t : "", t ? "-" : "");
        case 60: rv = astrcat("sixty",   t ? t : "", t ? "-" : "");
        case 70: rv = astrcat("seventy", t ? t : "", t ? "-" : "");
        case 80: rv = astrcat("eighty",  t ? t : "", t ? "-" : "");
        case 90: rv = astrcat("ninety",  t ? t : "", t ? "-" : "");
        }
        free(t);
        if (ordinal && !(n % 10)) {
            /* we won't have a -th ending from the units place because there
               isn't a units place */
            t = rv;
            t[strlen(t)-1] = '\0';
            rv = astrcat(t, "ieth", "");
            free(t);
        }
    } else if (n < 1000) {
        if (n % 100) t = anumbername(n % 100, ordinal); else t = 0;
        t2 = anumbername(n / 100, ordinal);
        rv = astrcat(t2, t ? t : "", t ? " hundred and " :
                     ordinal ? " hundredth" : " hundred");
        free(t);
        free(t2);
    } else if (n < 1000000) {
        if (n % 1000) t = anumbername(n % 1000, ordinal); else t = 0;
        t2 = anumbername(n / 1000, ordinal);
        rv = astrcat(t2, t ? t : "",
                     t ? (n % 1000 >= 100 ? " thousand, " : " thousand and ") :
                     ordinal ? " thousandth" : " thousand");
        free(t);
        free(t2);
    } else if (n < 1000000000) {
        if (n % 1000000) t = anumbername(n % 1000000, ordinal); else t = 0;
        t2 = anumbername(n / 1000000, ordinal);
        rv = astrcat(t2, t ? t : "",
                     t ? (n % 1000000 >= 1000 ? " million, " :
                          " million and ") :
                     ordinal ? " millionth" : " million");
        free(t);
        free(t2);
    } else {
        if (n % 1000000000) t = anumbername(n % 1000000000, ordinal);
        else t = 0;
        t2 = anumbername(n / 1000000000, ordinal);
        rv = astrcat(t2, t ? t : "",
                     t ? (n % 1000000000 >= 1000000 ? " billion, " :
                          " billion and ") :
                     ordinal ? " billionth" : " billion");
        free(t);
        free(t2);
    }
    return rv;
}

/* Checks for relative pronouns and question words.
   Relative pronouns are along the lines of Q{when}; question words along the
   lines of N{when}. We can use the same function to check for both, because in
   the case of question words that aren't relative pronouns, we'll never see
   them in a Q{}, and likewise for relative pronouns in N{}. */
static boolean
is_relative_pronoun(const char *s)
{
    /* question words */
    if (!strcmp(s, "how")) return TRUE;
    if (!strcmp(s, "who")) return TRUE;
    if (!strcmp(s, "why")) return TRUE;
    if (!strcmp(s, "what")) return TRUE;
    /* relative pronouns */
    if (!strcmp(s, "while")) return TRUE;
    if (!strcmp(s, "as")) return TRUE;
    if (!strcmp(s, "that")) return TRUE;
    if (!strcmp(s, "because")) return TRUE; /* arguably a conjuction, but it's
                                               Q{because} in our grammar */
    /* both */
    if (!strcmp(s, "when")) return TRUE;
    if (!strcmp(s, "where")) return TRUE;
    return FALSE;
}

/* Is s pronounced staring with a vowel?
   Based off an() in 3.4.3's mkobj.c. (Materials, like "rock" and "ice", don't
   get "a" or "an" at all, but we can't deal with that here.) */
static boolean
vowlish(const char *s)
{
    /* All words starting "one" in my dictionary start with /w/, except
       "onerous" and its derivatives */
    if ((!strncmp(s, "one", 3) && strncmp(s, "onerous", 7)) ||
        /* Words starting u- with the sound /y/ */
        !strncmp(s, "useful", 6) || !strncmp(s, "unicorn", 7) ||
        !strncmp(s, "uranium", 7) || !strncmp(s, "eucalyptus", 10))
        return FALSE;
    if (strchr("aeiou", *s)) return TRUE;
    return FALSE;
}

/* Adds an appropriate article to s, which must be malloc'ed; takes ownership of
   the pointer, and returns a malloc'ed pointer (either the same one, or a new
   one, in which case it frees the old one). This assumes it's not being given a
   proper noun as argument. */
static char *
articulate(char *s, int quan)
{
    char *rv;
    if (quan & (1 << 30) && quan & (1 << 27)) return s; /* no article */
    if (is_relative_pronoun(s)) return s; /* no article for grammar reasons */
    if (quan & (1 << 30)) {
        if (quan != ((1 << 30) | 1)) return s; /* "four apples", etc. */
        rv = astrcat(vowlish(s) ? "an" : "a", s, " ");
        free(s);
        return rv;        
    } else {
        rv = astrcat("the", s, " ");
        free(s);
        return rv;
    }
}

static char *
resuffix(const char *s, const char *suffix)
{
    /* Generic rules for changing the suffices of words in English.
       This isn't designed for adding -s; that's makeplural's job (even on
       verbs). Rather, it's for -ed, -ing, -er, -est.

       The rules are:
       - First, change a final y into an i;
       - Then, remove a final e;
       - Then, if adding -ing, change a final i into a y;
       - Several words need a final consonant doubled (we dictionary-lookup
         these);
       - Then, add the suffix.
    */
    static char resuffix_buffer[BUFSZ];
    char *p = resuffix_buffer;
    while ((*(p++)=*(s++))) {} /* simultaneous strcpy and measure length */
    p -= 2;
    if (*p == 'y') *p = 'i';
    if (*p == 'e') *(p--) = 0;
    if (*suffix == 'i' && *p == 'i') *p = 'y';
    /* currently, no words double consonants when stemmed */
    strcpy(p+1, suffix);
    return resuffix_buffer;
}

/* Based on makeplural from 3.4.3's objnam.c. Adds a -s suffix to a word, to
   form the plural of a noun or third person singular of a verb. */
static char *
append_s(const char *oldstr)
{
    static char str[BUFSZ];
    /* Note: cannot use strcmpi here -- it'd give MATZot, CAVEMeN,... */
    char *spot;
    const char *excess = NULL;
    int len;

    while (*oldstr == ' ')
        oldstr++;
    if (!*oldstr) {
        strcpy(str, "(ERROR: plural of NULL)");
        return str;
    }
    strcpy(str, oldstr);

    /* 
     * Skip changing "pair of" to "pairs of".  According to Webster, usual
     * English usage is use pairs for humans, e.g. 3 pairs of dancers,
     * and pair for objects and non-humans, e.g. 3 pair of boots.  We don't
     * refer to pairs of humans in this game so just skip to the bottom.
     */
    if (!strncmp(str, "pair of ", 8))
        goto bottom;

    /* Search for common compounds, ex. lump of royal jelly */
    for (spot = str; *spot; spot++) {
        if (!strncmp(spot, " of ", 4)
            || !strncmp(spot, " labeled ", 9)
            || !strncmp(spot, " called ", 8)
            || !strncmp(spot, " named ", 7)
            || !strcmp(spot, " above")  /* lurkers above */
            ||!strncmp(spot, " versus ", 8)
            || !strncmp(spot, " from ", 6)
            || !strncmp(spot, " in ", 4)
            || !strncmp(spot, "-in-", 4)        /* mother-in-law */
            ||!strncmp(spot, " on ", 4)
            || !strncmp(spot, " a la ", 6)
            || !strncmp(spot, " with", 5)       /* " with "? */
            ||!strncmp(spot, " de ", 4)
            || !strncmp(spot, " d'", 3)
            || !strncmp(spot, " du ", 4)) {
            excess = oldstr + (int)(spot - str);
            *spot = 0;
            break;
        }
    }
    spot--;
    while (*spot == ' ')
        spot--; /* Strip blanks from end */
    *(spot + 1) = 0;
    /* Now spot is the last character of the string */

    len = strlen(str);

    /* Single letters */
    if (len == 1 || !isalpha(*spot) || *spot == '@') {
        strcpy(spot + 1, "'s");
        goto bottom;
    }

    /* Same singular and plural; mostly Japanese words except for "manes" */
    if ((len == 2 && !strcmp(str, "ya")) ||
        (len >= 2 && !strcmp(spot - 1, "ai")) ||    /* samurai, Uruk-hai */
        (len >= 3 && !strcmp(spot - 2, " ya")) ||
        (len >= 4 && (!strcmp(spot - 3, "fish") ||
                      !strcmp(spot - 3, "tuna") ||
                      !strcmp(spot - 3, "deer") ||
                      !strcmp(spot - 3, "yaki"))) ||
        (len >= 5 && (!strcmp(spot - 4, "sheep") ||
                      !strcmp(spot - 4, "ninja") ||
                      !strcmp(spot - 4, "ronin") ||
                      !strcmp(spot - 4, "shito") ||
                      !strcmp(spot - 4, "tengu") ||
                      !strcmp(spot - 4, "manes"))) ||
        (len >= 6 && !strcmp(spot - 5, "ki-rin")) ||
        (len >= 6 && !strcmp(spot - 5, "Nazgul")) ||
        (len >= 7 && !strcmp(spot - 6, "gunyoki")) ||
        (len >= 8 && !strcmp(spot - 7, "shuriken")))
        goto bottom;

    /* man/men ("Wiped out all cavemen.") */
    if (len >= 3 && !strcmp(spot - 2, "man") &&
        (len < 6 || strcmp(spot - 5, "shaman")) &&
        (len < 5 || strcmp(spot - 4, "human"))) {
        *(spot - 1) = 'e';
        goto bottom;
    }

    /* tooth/teeth */
    if (len >= 5 && !strcmp(spot - 4, "tooth")) {
        strcpy(spot - 3, "eeth");
        goto bottom;
    }

    /* knife/knives, etc... */
    if (!strcmp(spot - 1, "fe")) {
        strcpy(spot - 1, "ves");
        goto bottom;
    } else if (*spot == 'f') {
        if (strchr("lr", *(spot - 1)) || strchr("aeiou", *(spot - 1))) {
            strcpy(spot, "ves");
            goto bottom;
        } else if (len >= 5 && !strncmp(spot - 4, "staf", 4)) {
            strcpy(spot - 1, "ves");
            goto bottom;
        }
    }

    /* foot/feet (body part) */
    if (len >= 4 && !strcmp(spot - 3, "foot")) {
        strcpy(spot - 2, "eet");
        goto bottom;
    }

    /* ium/ia (mycelia, baluchitheria) */
    if (len >= 3 && !strcmp(spot - 2, "ium")) {
        *(spot--) = (char)0;
        *spot = 'a';
        goto bottom;
    }

    /* algae, larvae, hyphae (another fungus part) */
    if ((len >= 4 && !strcmp(spot - 3, "alga")) ||
        (len >= 5 &&
         (!strcmp(spot - 4, "hypha") || !strcmp(spot - 4, "larva")))) {
        strcpy(spot, "ae");
        goto bottom;
    }

    /* fungus/fungi, homunculus/homunculi, but buses, lotuses, wumpuses */
    if (len > 3 && !strcmp(spot - 1, "us") &&
        (len < 5 ||
         (strcmp(spot - 4, "lotus") &&
          (len < 6 || strcmp(spot - 5, "wumpus"))))) {
        *(spot--) = (char)0;
        *spot = 'i';
        goto bottom;
    }

    /* vortex/vortices */
    if (len >= 6 && !strcmp(spot - 3, "rtex")) {
        strcpy(spot - 1, "ices");
        goto bottom;
    }

    /* djinni/djinn (note: also efreeti/efreet) */
    if (len >= 6 && !strcmp(spot - 5, "djinni")) {
        *spot = (char)0;
        goto bottom;
    }

    /* mumak/mumakil */
    if (len >= 5 && !strcmp(spot - 4, "mumak")) {
        strcpy(spot + 1, "il");
        goto bottom;
    }

    /* sis/ses (nemesis) */
    if (len >= 3 && !strcmp(spot - 2, "sis")) {
        *(spot - 1) = 'e';
        goto bottom;
    }

    /* erinys/erinyes */
    if (len >= 6 && !strcmp(spot - 5, "erinys")) {
        strcpy(spot, "es");
        goto bottom;
    }

    /* mouse/mice,louse/lice (not a monster, but possible in food names) */
    if (len >= 5 && !strcmp(spot - 3, "ouse") && strchr("MmLl", *(spot - 4))) {
        strcpy(spot - 3, "ice");
        goto bottom;
    }

    /* matzoh/matzot, possible food name */
    if (len >= 6 && (!strcmp(spot - 5, "matzoh")
                     || !strcmp(spot - 5, "matzah"))) {
        strcpy(spot - 1, "ot");
        goto bottom;
    }
    if (len >= 5 && (!strcmp(spot - 4, "matzo")
                     || !strcmp(spot - 4, "matza"))) {
        strcpy(spot, "ot");
        goto bottom;
    }

    /* child/children (for wise guys who give their food funny names) */
    if (len >= 5 && !strcmp(spot - 4, "child")) {
        strcpy(spot, "dren");
        goto bottom;
    }

    /* note: -eau/-eaux (gateau, bordeau...) */
    /* note: ox/oxen, VAX/VAXen, goose/geese */

    /* Ends in z, x, s, ch, sh; add an "es" */
    if (strchr("zxs", *spot)
        || (len >= 2 && *spot == 'h' && strchr("cs", *(spot - 1)))
        /* Kludge to get "tomatoes" and "potatoes" right */
        || (len >= 4 && !strcmp(spot - 2, "ato"))) {
        strcpy(spot + 1, "es");
        goto bottom;
    }

    /* Ends in y preceded by consonant (note: also "qu") change to "ies" */
    if (*spot == 'y' && (!strchr("aeiou", *(spot - 1)))) {
        strcpy(spot, "ies");
        goto bottom;
    }

    /* Default: append an 's' */
    strcpy(spot + 1, "s");

bottom:
    if (excess)
        strcat(str, excess);
    return str;
}

static boolean
special_case_verb(char *obuf, const char *v, enum tense t,
                  boolean plural, enum person p)
{
    if (verb_chains_directly(v)) {
        /* Directly-chaining verbs don't conjugate.
           They also don't work properly in tenses other than the present, in
           which case we have to sawp in an entirely different verb (e.g.
           "be able" rather than "can"), but that's just a TODO for now. */
        strcpy(obuf, v);
        return TRUE;
    }
    if (!strcmp(v, "do")) {
        if (t == present) {
            if (!plural && (p == third)) strcpy(obuf, "does");
            else strcpy(obuf, "do");
            return TRUE;
        }
        if (t == imperfect) {
            strcpy(obuf, "did");
            return TRUE;
        }
        if (t == passive_participle) {
            strcpy(obuf, "done");
            return TRUE;
        }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "have")) {
        if (t == present) {
            if (!plural && (p == third)) strcpy(obuf, "has");
            else strcpy(obuf, "have");
            return TRUE;
        }
        if (t == imperfect || t == passive_participle) {
            strcpy(obuf, "had");
            return TRUE;
        }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "know")) {
        if (t == imperfect) { strcpy(obuf, "knew"); return TRUE; }
        if (t == passive_participle) { strcpy(obuf, "known"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "are")) {
        switch (t) {
        case present:
            switch(p) {
            case first: strcpy(obuf, "am"); break;
            case second: strcpy(obuf, "are"); break;
            case third: strcpy(obuf, "is"); break;
            case base: strcpy(obuf, "be"); break;
            }
            if (plural) strcpy(obuf, "are");
            break;
        case imperfect:
            strcpy(obuf, "were");
            if ((p == first || p == third) && !plural)
                strcpy(obuf, "was");
            break;
        case passive_participle: strcpy(obuf, "been"); break;
        case active_participle: strcpy(obuf, "being"); break;
        case secondary_direct: strcpy(obuf, "be"); break;
        default:
            strcpy(obuf, "(ERROR: invalid tense in special_case_verb)");
            break;
        }
        return TRUE;
    }
    return FALSE;
}

static char *
conjugate(const char *v, enum tense t, int quan, enum person p)
{
    static char conjugation_buffers[12][BUFSZ];
    static int which_buffer = 0;
    which_buffer++;
    which_buffer %= 12;
    char *w = conjugation_buffers[which_buffer];
    char *x, *y;
    boolean plural = (quan &~ ((1 << 30) | (1 << 27))) != 1;
    /* There are eight possible tenses that we might have to deal with for each
       verb. Some verbs are regular, and can be conjugated entirely using
       resuffix. Many more are irregular, and need a whole load of special
       cases. There's also the issue of adapting verbs for person; third person
       singular is, fortunately, the only case that's different. (It'd be
       entirely possible, although obnoxious, to implement the now-obsolete
       special case for second-person singular and get "thou art hit"; we don't
       do that right now, but it might make for an interesting patch.)

       Two cases use secondary verbs: the perfect behind "have", and the
       continuous behind "are". ("are" is weird in that it special-cases all
       three people in the singular. And different in several tenses from
       those!) We use recursive calls for these. Also to handle infinitives,
       which are the secondary direct ("stem") form with "to " prepended. */

    /* In English, compound verbs always just keep the preposition at the end,
       no matter what. (Unlike German, where they move to the start when forming
       the infinitive.) Potential exception: when special-casing a verb. */
    if (strchr(v, ' ') && !special_case_verb(w, v, t, plural, p)) {
        strcpy(w, v);
        x = strchr(w, ' ');
        *x = 0;
        y = conjugate(w, t, quan, p);
        *x = ' ';
        strcat(y, x); /* we know our own output is writable and extendable */
        return y;
    }

    switch(t) {
    case perfect: /* conjugate "have" + passive participle */
        x = conjugate("have", present, quan, p);
        strcat(x, " ");
        strcat(x, conjugate(v, passive_participle, 1, base));
        return x;
    case continuous: /* conjugate "are" + active participle */
        x = conjugate("are", present, quan, p);
        strcat(x, " ");
        strcat(x, conjugate(v, active_participle, 1, base));
        return x;
    case secondary_infinitive: /* "to" + stem */
        strcpy(w, "to ");
        strcat(w, conjugate(v, secondary_direct, quan, p));
        break;
    case present: /* stem, or stem + s in third person singular */
        if (special_case_verb(w, v, t, plural, p)) return w;
        if (p == third && !plural) {
            strcpy(w, append_s(v));
        } else strcpy(w, v);
        break;
    case imperfect: /* resuffix -ed, for regular verbs */
    case passive_participle: /* also resuffix -ed, for regular verbs */
        if (special_case_verb(w, v, t, plural, p)) return w;
        strcpy(w, resuffix(v, "ed"));
        break;
    case active_participle: /* resuffix -ing, for regular verbs */
        if (special_case_verb(w, v, t, plural, p)) return w;
        strcpy(w, resuffix(v, "ing"));
        break;
    case secondary_direct: /* stem = plural form, for regular verbs */
        if (special_case_verb(w, v, t, plural, p)) return w;
        strcpy(w, v);
        break;
    default:
        if (special_case_verb(w, v, t, plural, p)) return w;
        strcpy(w, "(ERROR: noun type found where verb tense expected)");
        break;
    }

    return w;
}

/* The grammar tree bubbles information /down/ from leaves to root. The params
   of force_unit bubble information /up/ the other way, e.g. pluralizing
   "potion of speed" should propagate the pluralness onto "potion" to give
   "potions of speed". Other languages may need to propagate linguistic gender
   the same way, but English doesn't. */
static enum person
force_unit(struct grammarunit *u, enum tense t, int quan, enum person p)
{
    boolean negated = FALSE;
    enum person nounperson;
    if (u->content && u->rule != gr_literal) {
        /* should never happen... */
        return base;
    }
    switch(u->rule) {
        /* All conjunctions work in much the same way: we combine with "and",
           or "but" if the RHS is a negation. */
    case plus_NN: /* "dwarf and goblin */
    case plus_VV: /* "grind and crush" */
    case plus_AA: /* "black and white" */
    case plus_DD: /* "slowly and carefully" */
    case plus_QQ: /* "when and where" */
    case plus_EE: /* "above and below" */
    case plus_CC: /* "you hit the goblin and the dwarf hits the hobbit" OR
                     "you hit the goblin, and the dwarf hits the hobbit"*/
        if (u->children[1]->rule == minus_N ||
            u->children[1]->rule == minus_V ||
            u->children[1]->rule == minus_A ||
            u->children[1]->rule == minus_D ||
            u->children[1]->rule == minus_Q ||
            u->children[1]->rule == minus_E ||
            u->children[1]->rule == minus_C) negated = TRUE;
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], t, quan, p);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content,
                             u->rule == plus_CC ?
                             (negated ? ", but " : ", and ") :
                             (negated ?  " but " :  " and "));
        break;
    case minus_N: /* "nonweapon" */
        force_unit(u->children[0], t, quan, p);
        u->content = astrcat("non", u->children[0]->content, "");
        break;
    case minus_A:
        /* "not red" or "nonred"; we apply "non" only to literally negating
           adjectives which contain no spaces */
        force_unit(u->children[0], t, quan, p);
        if (u->children[0]->rule != gr_literal ||
            strchr(u->children[0]->content, ' '))
            u->content = astrcat("not", u->children[0]->content, " ");
        else
            u->content = astrcat("non", u->children[0]->content, "");
        break;
    case minus_Q:
        /* This can only legally be used in the D{QC} and A{QC} constructs
           anyway, and the negation makes most sense shifted onto the verb; "I
           take the Amulet while the dragon sleeps", if you negate the "while",
           most naturally becomes "I take the Amulet while the dragon does not
           sleep", i.e. it's the same as negating "sleep". To get a perfect
           opposite, you need a conjunction: "I take the Amulet, but not while
           the dragon sleeps". In both cases, we need support from the surrounding
           level. So, err, we can't do much but just prefix "not". So we fall
           through. */
    case minus_E:
        /* "The dagger on the table", if you negate "on", you get "The dagger
           that is not on the table", or simply "The dagger not on the table".
           Either would be used depending on context in English; we'll just go
           for the "not" prefix because I'm lazy. Fall through. */
    case minus_D: /* "not slowly" is about the best we can do for verbs;
                     "not very" is perfect for adjectives */
        force_unit(u->children[0], t, quan, p);
        u->content = astrcat("not", u->children[0]->content, " ");
        /* TODO: special-case -D{tN} and possibly others */
        break;
    case minus_C:
        /* To negate a clause in English, you negate the verb: "You hit the
           newt" negates to "You do not hit the newt". */
        switch(u->children[0]->rule) {
        case clause_NV:
        case clause_iNV:
        case clause_pNV:
        case clause_cNV:
            /* verb is the second child of our first child */
            u->children[1] = u->children[0];
            u->children[0] = u->children[1]->children[0];
            u->children[2] = u->children[1]->children[2];
            u->rule = u->children[1]->rule;
            u->children[1]->children[0] = u->children[1]->children[1];
            u->children[1]->children[1] = 0;
            u->children[1]->children[2] = 0;
            u->children[1]->rule = minus_V;
            return force_unit(u, t, quan, p); /* tail-recurse */
        case clause_iV:
        case clause_sV:
        case clause_isV:
        case clause_psV:
        case clause_csV:
            /* verb is the first child of our first child */
        case clause_qC: /* clause is the first child of our first child */
            u->children[1] = u->children[0]->children[1];
            u->children[2] = u->children[0]->children[2];
            u->rule = u->children[0]->rule;
            u->children[0]->children[1] = 0;
            u->children[0]->children[2] = 0;
            u->children[0]->rule = (u->rule == clause_qC ? minus_C : minus_V);
            return force_unit(u, t, quan, p); /* tail-recurse */
        default:
            /* can't impossible() from grammar code, and we could have a
               literal; use an unwieldy but always clear negation */
            force_unit(u->children[0], t, quan, p);
            u->content = astrcat("it is not the case that",
                                 u->children[0]->content, " ");
            break;
        }
        break;
    case noun_NA: /* "the tame kitten" */
        force_unit(u->children[1], t, quan, p);
        {
            struct grammarunit *a;
            /* Be careful of the adjective_QC case: "the cave where the dragon
               sleeps", not "the where the dragon sleeps cave" (which is cute
               and understandable but nonidiomatic). adjective_lN and
               adjective_aN have similar rules: "the stranger at the door",
               etc. adjective_V also moves after the noun, but only when it's
               not a literal verb; "cursed dagger", but "dagger named
               Vladsbane". Parenthesizing adjectives does not change this
               rule. Nor does conjuncting them, although in dubious cases
               like "large and painted blue" we have to make an arbitrary
               choice (in this case, we go with the first adjective). */
            a = u->children[1];
            while (a->rule == adjective_pA || a->rule == plus_AA)
                a = a->children[0];
            if (a->rule == adjective_QC || a->rule == adjective_mN ||
                a->rule == adjective_lN || a->rule == adjective_aN ||
                (a->rule == adjective_V &&
                 a->children[0]->rule != gr_literal)) {
                nounperson = force_unit(u->children[0], t, quan, p);
                u->content = astrcat(u->children[0]->content,
                                     u->children[1]->content, " ");
            } else {
                /* We need to move any "the" or "a"/"an" to before the
                   /adjective/. We do this by suppressing the article on the
                   noun (this is what the 1<<30 | 1<<27 means), and adding it
                   here ourself. We need to ensure we don't eventually end up
                   with a proper noun before adding the article, though. */
                struct grammarunit *n;
                nounperson = force_unit(u->children[0], t,
                                        quan | (1 << 30) | (1 << 27), p);
                u->content = astrcat(u->children[1]->content,
                                     u->children[0]->content, " ");
                for (n = u; n->rule != gr_literal && n->rule != noun_mX &&
                         n->rule != gr_unknown; n = n->children[0]) {}
                if (n->role == gr_noun && n->rule != gr_unknown)
                    u->content = articulate(u->content, quan);
            }
        }
        return nounperson;
    case noun_NEN: /* "kitten in a pit trap" */
        nounperson = force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], t, quan, p);
        force_unit(u->children[2], object, u->children[2]->quan, base);
        {
            char *t;
            t = astrcat(u->children[0]->content, u->children[1]->content, " ");
            u->content = astrcat(t, u->children[2]->content, " ");
            free(t);
        }
        return nounperson;
    case noun_fNA: /* "two of the daggers" */
        /* Take the noun's quantity from the noun itself; "one of the daggers"
           is singular but should still pluralise "daggers" */
        nounperson = force_unit(u->children[0], t, u->children[0]->quan, p);
        /* Note that A{2} forces into "two", i.e. we're spelling the number out
           as words. This is what people expect. */
        force_unit(u->children[1], t, quan, p);
        u->content = astrcat(u->children[1]->content,
                             u->children[0]->content, " of ");
        return nounperson;
    case noun_fNN: /* "the slice of cake" */
        nounperson = force_unit(u->children[1], t, quan, p);
        force_unit(u->children[0], t,
                   u->children[0]->quan | (1 << 30) | (1 << 27), p);
        u->content = astrcat(u->children[1]->content,
                             u->children[0]->content, " of ");
        return nounperson;
    case noun_qNN: /* "boots of speed" */
        nounperson = force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], object,
                   u->children[1]->quan | (1 << 30) | (1 << 27), base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " of ");
        return nounperson;
    case noun_oNN: /* "Dudley's dungeon" */
        /* Remove articles on the [0] noun; "the goblin's dagger", not "the
           goblin's the dagger". */
        nounperson = force_unit(u->children[0], t,
                                quan | (1 << 30) | (1 << 27), p);
        force_unit(u->children[1], possessive, u->children[1]->quan, base);
        u->content = astrcat(u->children[1]->content,
                             u->children[0]->content, " ");
        return nounperson;
    case noun_lNN: /* "knowledge of dungeoneering" */
        /* In English, this maps to either "of" or "about". So far, "of" is
           better in every case we've used; this code will need to be revisited
           with some way to choose if a case where we need "about" comes into
           play. */
        nounperson = force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], object, u->children[1]->quan, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " of ");
        return nounperson;
    case noun_mX: /* anything, in quotes */
        force_unit(u->children[0],
                   u->children[0]->role == gr_noun ? object : present,
                   u->children[0]->quan | (1 << 30) | (1 << 27), base);
        u->content = astrcat("\"", "\"", u->children[0]->content);
        return third;
    case noun_aNV: /* "burning bush", "the newt hitting Dudley" */
        /* We put the verb first if it's a literal, afterwards otherwise. */
        if (u->children[1]->rule == gr_literal) {
            /* Move "the"/"a"/"an" before the adjective. */
            struct grammarunit *n;
            nounperson = force_unit(u->children[0], t,
                                    quan | (1 << 30) | (1 << 27), p);
            force_unit(u->children[1], active_participle, quan, nounperson);
            u->content = astrcat(u->children[1]->content,
                                 u->children[0]->content, " ");
            for (n = u; n->rule != gr_literal && n->rule != noun_mX &&
                     n->rule != gr_unknown; n = n->children[0]) {}
            if (n->role == gr_noun && n->rule != gr_unknown)
                u->content = articulate(u->content, quan);
        } else {
            nounperson = force_unit(u->children[0], t, quan, p);
            force_unit(u->children[1], active_participle, quan, nounperson);
            u->content = astrcat(u->children[0]->content,
                                 u->children[1]->content, " ");
        }
        return nounperson;
    case minus_V:
        /* If we have a primary verb, we're converting this to a secondary
           behind "do not". If it's a secondary, we're just prefixing "not".
           For instance, "You do not die", "You begin to not die", "You can not
           die". (Two of those cases can be meaningfully post-processed into
           "don't" and "cannot" or "can't", but that level of special casing
           isn't done here.) The exception is that chain-directly verbs (and
           "are" for some reason) are negated by /suffixing/ not.

           There's also the issue of if this is a compound verb. In general,
           there are no strong rules ("kill a newt" best goes to "fail to kill a
           newt", with a new primary verb added, but that's too much
           special-casing, again), so we just propagate the negation into the
           primary ("do not kill a newt"), which is OK when the verb's used as a
           primary, and a little weird but perfectly understandable as a
           secondary ("have to not kill a newt").

           There is also a quantifier mixup in English (that doesn't happen in
           German), but I'm not dealing with it right now. If it comes up, we
           can special-case some quantifiers. */
        if (u->children[0]->rule == minus_V) {
            /* Double negative. This would lead to an infinite loop if we
               simply shifted it. So instead, we just remove the "not not"
               entirely. */
            force_unit(u->children[0]->children[0], t, quan, p);
            u->content = astrcat("", u->children[0]->children[0]->content, "");
            return base;
        }
        if (u->children[0]->rule != gr_literal) {
            /* First child of a verb is always a verb. Fortunately, for
               everyone's sanity. */
            u->children[1] = u->children[0]->children[1];
            u->children[2] = u->children[0]->children[2];
            u->rule = u->children[0]->rule;
            u->children[0]->children[1] = 0;
            u->children[0]->children[2] = 0;
            u->children[0]->rule = minus_V;
            return force_unit(u, t, quan, p); /* tail-recurse */
        }
        /* If it's a literal that chains directly, or "are", we simply
           conjugate it and add "not". */
        if (verb_chains_directly(u->children[0]->content) ||
            !strcmp(u->children[0]->content, "are")) {
            u->content = astrcat(conjugate(u->children[0]->content, t, quan, p),
                                 "not", " ");
            return base;
        }
        /* It's a literal, so we have to conjugate it.
           "do not kill", "have not killed", "did not kill", "are not killing",
           "not killing", "not killed", "not kill", "to not kill"
         */
        switch(t)
        {
        case present:
        case imperfect:
            u->content = astrcat(
                conjugate("do not", t, quan, p),
                conjugate(u->children[0]->content, secondary_direct, 1, base),
                " ");
            break;
        case perfect:
            /* this uses the passive participle not the imperfect: "have not
               been", not "have not were" (it's non-obvious because most verbs
               have those equal in English) */
            u->content = astrcat(
                conjugate("have not", t, quan, p),
                conjugate(u->children[0]->content, passive_participle, 1, base),
                " ");
            break;
        case continuous:
            u->content = astrcat(
                conjugate("are not", t, quan, p),
                conjugate(u->children[0]->content, active_participle, 1, base),
                " ");
            break;
        case active_participle:
        case passive_participle:
        case secondary_direct:
            u->content = astrcat(
                "not",
                conjugate(u->children[0]->content, t, quan, p),
                " ");
            break;
        case secondary_infinitive:
            /* Infinitives /always/ start "to " in English, so we can calculate
               the infinitive and then cut the "to " off (3 chars). */
            u->content = astrcat(
                "to not",
                conjugate(u->children[0]->content, t, quan, p) + 3,
                " ");
            break;
        case subject:
        case object:
        case possessive:
            /* noun "tenses" */
            u->content = astrcat("", "(ERROR: Verb used in noun context)", "");
        }
        break;
    case verb_VN:
    case verb_VA:
        /* In English, in both these cases, we put the subject (VN) or
           complement (VA) just after the verb. Adjectives in English don't have
           to agree with the gender or quantity of the noun; if they did, we'd
           have an extra param to force_unit to track it.

           One potential issue is that this must occur inside complex adverbs
           (and it's simplest to make it occur inside all adverbs): it's OK to
           have "damage the newt in Gehennom" but not "damage in Gehennom the
           newt" (which is parseable for humans but sounds odd). For
           secondary verbs, "choose a spell to cast" is correct, "choose to
           cast a spell" is also correct /but/ means something else, and
           V{V{V{choose},V{cast}},N{i,spell}} means the /former/ (the latter is
           V{V{choose},V{V{cast},N{i,spell}}}). */
        if (u->children[0]->rule == verb_VD ||
            u->children[0]->rule == verb_VV) {
            struct grammarunit *temp_child = u->children[1];
            enum grammarrule temp_rule = u->rule;
            u->children[1] = u->children[0]->children[1];
            u->children[0]->children[1] = temp_child;
            u->rule = u->children[0]->rule;
            u->children[0]->rule = temp_rule;
            return force_unit(u, t, quan, p); /* tail-recurse */
        }
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], object, u->children[1]->quan, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " ");
        break;
    case verb_VV:
        /* Move VD outside VV, for the same reason we move both outside VN and
           VA. */
        if (u->children[0]->rule == verb_VD) {
            struct grammarunit *temp_child = u->children[1];
            u->children[1] = u->children[0]->children[1];
            u->children[0]->children[1] = temp_child;
            u->rule = u->children[0]->rule;
            u->children[0]->rule = verb_VV;
            return force_unit(u, t, quan, p); /* tail-recurse */
        }
        /* We're basically just concatenating the primary and the secondary; the
           choice of primary verb determines how we conjugate the secondary,
           though. We check the eventual literal that the primary corresponds to
           in order to determine whether we're using the secondary verb directly
           or as an infinitive. */
        {
            struct grammarunit *v;
            for (v = u; v->rule != gr_literal; v = v->children[0]) {}
            if (verb_chains_directly(v->content))
                force_unit(u->children[1], secondary_direct, 1, base);
            else
                force_unit(u->children[1], secondary_infinitive, 1, base);
            force_unit(u->children[0], t, quan, p);
            u->content = astrcat(u->children[0]->content,
                                 u->children[1]->content, " ");
        }
        break;
    case verb_VD:
        /* The code for VN and VA ensures that we have the object
           or complement (if any) as part of the verb already, so it's safe
           to put things at the end. Likewise, the code for VV ensures we
           have any secondary verb inside.

           We have four choices:
           "The goblin slowly opens the door."
           "The goblin opens the door slowly."
           "The goblin opens the door, slowly."
           "Slowly, the goblin opens the door."
           Deciding between these can be complex. We use the first case if it's
           reasonably simple; to be on the safe side, we use it only for
           single-word literals (constructions like "you quite well know that"
           are rather awkward). We can't use the fourth case without help
           from the clause rules, so if we get here at all, we're using the
           second or third. And the third seems unnatural in pretty much all
           cases, so we don't use it. (The main case in which we'd want to use
           the fourth is D{QC}, by the way; and the rule for C{NV} supports this
           way round of doing things.)

           One other issue is involved with infinitives: "boldly to go" may be
           /technically/ correct (or possibly "to go boldly"), but splitting the
           infinitive as "to boldly go" is much more common, even if it annoys
           some people. So we take care to split it if necessary. */
        force_unit(u->children[0], t == secondary_infinitive ?
                   secondary_direct : t, quan, p);
        force_unit(u->children[1], t, quan, p);
        if (u->children[1]->rule != gr_literal ||
            strchr(u->children[1]->content, ' '))
            u->content = astrcat(u->children[0]->content,
                                 u->children[1]->content, " ");
        else
            u->content = astrcat(u->children[1]->content,
                                 u->children[0]->content, " ");
        if (t == secondary_infinitive) {
            char *tx = u->content;
            u->content = astrcat("to", tx, " ");
            free(tx);
        }
        break;
    case adverb_DD: /* "very slowly" */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], t, quan, p);
        u->content = astrcat(u->children[1]->content,
                             u->children[0]->content, " ");
        break;
    case adverb_tN: /* "with a hammer" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "with ");
        break;
    case adverb_lN: /* "at the door" */
    case adjective_lN:
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "at ");
        break;
    case adverb_aN: /* "in Gehennom" */
    case adjective_aN:
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "in ");
        break;
    case adverb_dN: /* "for an hour" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "for ");
        break;
    case adverb_mN: /* "at midnight" */
    case adjective_mN:
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "at ");
        break;
    case adverb_eN: /* "to dust" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "to ");
        break;
    case adverb_iN: /* "to his mother" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "to ");
        break;
    case adverb_QC: /* "while the dragon sleeps" */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], t, u->children[1]->quan, p);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " ");
        break;
    case adjective_cA: /* "hotter", "more expensive to buy" */
    case adjective_sA: /* "hottest", "most expensive to buy" */
        /* If we're applying this to a literal, we're following one of two
           patterns: appending -er or -est respectively to the stem, or
           prepending "more " or "most " respectively. The general rule is
           that more "complicated" adjectives use the long form, but that's
           incredibly vague to write into a program. So we use a dictionary
           lookup instead. */
        if (u->children[0]->rule == gr_literal &&
            is_simple_adjective(u->children[0]->content)) {
            force_unit(u->children[0], t, quan, p);
            u->content = astrcat(
                "", resuffix(u->children[0]->content,
                             u->rule == adjective_cA ? "er" : "est"), "");
        } else {
            force_unit(u->children[0], t, quan, p);
            u->content = astrcat(u->rule == adjective_cA ? "more" : "most",
                                 u->children[0]->content, " ");
        }
        break;
    case adjective_pA: /* "(hot)" */
        force_unit(u->children[0], t, quan, p);
        u->content = astrcat("(", ")", u->children[0]->content);
        break;
    case adjective_AD: /* "very hot" */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], t, quan, p);
        u->content = astrcat(u->children[1]->content,
                             u->children[0]->content, " ");
        break;
    case adjective_AV: /* "hot to touch" */
    case noun_NV: /* "will to live" */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], secondary_infinitive, 1, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " ");
        break;
    case adjective_V: /* "painted red" */
        /* We basically just use the passive participle of the verb here. */
        force_unit(u->children[0], passive_participle, 1, base);
        u->content = astrcat("", u->children[0]->content, "");
        break;
    case adjective_N: /* "gnome" */
        /* Be careful not to pass on t or quan; also, suppress "the" */
        force_unit(u->children[0], object,
                   u->children[0]->quan | (1 << 30) | (1 << 27), base);
        u->content = astrcat("", u->children[0]->content, "");
        break;
    case adjective_QC: /* "where the dragon sleeps" */
        /* This one's a little awkward because it's placed after nouns rather
           than before, "the cave where the dragon sleeps". The support for this
           is in the noun_NA case, so we only need bother with translating the
           adjective itself. */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], present, u->children[1]->quan, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " ");
        break;
    case clause_NV:  /* "the goblin kills you" */
    case clause_iNV: /* "the goblin killed you" */
    case clause_pNV: /* "the goblin has killed you" */
    case clause_cNV: /* "the goblin is killing you" */
    {
        /* The verb gets its person and quan information from the noun, and its
           tense from the surroundings. English doesn't have gendered verbs
           (just gendered pronouns), so we can discard the noun's gender
           info. Our quan is copied from the noun, so we'd better give it
           back. Then we copy the noun's quantity (also quan) onto the verb.
           The actual sentence is just "noun verb".

           In the case where V has an adverb_QC applied to it, we move that to
           the start of the sentence, for increased drama. */
        struct grammarunit *v;

        nounperson = force_unit(u->children[0], subject, quan, base);
        if (u->children[1]->rule == verb_VD &&
            u->children[1]->children[1]->rule == adverb_QC) {
            v = u->children[1]->children[0];
        } else v = u->children[1];
        force_unit(v,
                   u->rule == clause_NV  ? present :
                   u->rule == clause_iNV ? imperfect :
                   u->rule == clause_pNV ? perfect : continuous,
                   quan, nounperson);
        u->content = astrcat(u->children[0]->content,
                             v->content, " ");
        if (u->children[1]->rule == verb_VD &&
            u->children[1]->children[1]->rule == adverb_QC) {
            char *tx = u->content;
            force_unit(u->children[1]->children[1], present, 1, base);
            u->content = astrcat(u->children[1]->children[1]->content,
                                 tx, ", ");
            free(tx);
        }
        break;
    }
    case clause_sV:  /* "you are killed" */
    case clause_isV: /* "you were killed" */
    case clause_psV: /* "you have been killed" */
    case clause_csV: /* "you are being killed" */
        /* In English, we form the passive using the passive participle and a
           conjugation of "are". To accomplish this, we have to grab the object
           out of the verb. (We can't form the passive of a verb with no object,
           amusing though it would be.) */
    {
        struct grammarunit *v;
        char *t;
        for (v = u; v->children[0]->rule != verb_VN; v = v->children[0])
            if (v->children[0]->rule == gr_literal) {
                /* This doesn't make sense... */
                u->content = astrcat(
                    "", "(ERROR: passive verb with no object)", "");
                return base;
            }
        /* We're changing the tree structure, and need to attach the old
           v->children[0] to the tree somewhere so that it gets freed, so
           we use our own [2]. */
        u->children[2] = v->children[0];
        v->children[0] = v->children[0]->children[0];
        u->children[2]->children[0] = 0;
        nounperson = force_unit(u->children[2]->children[1], subject,
                                u->children[2]->children[1]->quan, base);
        force_unit(u->children[0], passive_participle, 1, base);
        t = astrcat(u->children[2]->children[1]->content,
                    conjugate("are", 
                              u->rule == clause_sV  ? present :
                              u->rule == clause_isV ? imperfect :
                              u->rule == clause_psV ? perfect : continuous,
                              u->children[2]->quan, nounperson), " ");
        u->content = astrcat(t, u->children[0]->content, " ");
        free(t);
        break;
    }
    case clause_iV: /* "go away" */
        /* The imperative in English is the same as secondary direct tense.
           I hope, at least. */
        force_unit(u->children[0], secondary_direct, 1, base);
        u->content = astrcat("", u->children[0]->content, "");
        break;
    case clause_qC:
        /* The rules for forming questions are reasonably obnoxious, although
           also reasonably consistent:
           - If the clause starts with a relative pronoun, we leave it as is;
             otherwise:
           - If the sentence is using neither a directly chainable verb or
             "are", that verb is made into a secondary verb behind "do" (this
             is the same rule as is used for negation, incidentally), and
           - The primary verb is moved to the start of the sentence (while
             keeping everything else unchanged), and
           - The first relative pronoun that's used as a noun (i.e. N{where}
             rather than Q{where}) is moved to the start of the sentence, before
             the primary verb, if there is one (and if it's used to form an
             adverb, as in "at what time", the whole adverb phrase moves)

           It's also possible to form a question from an imperative, but that is
           (fortunately for my sanity) a no-op, apart from the use of a question
           mark (even if it contains a relative pronoun: "Go where?").

           We start off by finding the subject. This is easy in NV variants;
           in sV variants, we find it the same way as they do, and for iV,
           we know we can bail immediately. The only other legal possibility
           that even parses is qC, which is absurd and doesn't need a
           translation. */
    {
        struct grammarunit *v;
        char *tx;
        v = u;
        while(TRUE) {
            switch (v->rule) {
            case clause_NV:
            case clause_iNV:
            case clause_pNV:
            case clause_cNV:
            case clause_sV:
            case clause_isV:
            case clause_psV:
            case clause_csV:
            case clause_qC:
            case verb_VA:
            case verb_VD:
            case verb_VV:
            case noun_NA:
            case noun_NEN:
            case noun_fNA:
            case noun_oNN:
            case noun_lNN:
            case noun_NV:
                v = v->children[0];
                continue;
            case noun_mX:
                /* We don't move ""who"" to the start of the sentence, it's just
                   a mention of the relative pronoun */
                goto break_2;
            case verb_VN: /* we only get here in the passive case */
                v = v->children[1];
                continue;
            case clause_iV:
                /* This case is easy :) */
                force_unit(u->children[0], t, quan, p);
                u->content = astrcat("", u->children[0]->content, "");
                return base;
            case gr_literal:
                if (v->role == gr_noun || v->role == gr_propernoun) {
                    /* Looks like we found our subject. Is it a relative
                       pronoun? */
                    if (is_relative_pronoun(v->content)) {
                        force_unit(u->children[0], t, quan, p);
                        u->content = astrcat("", u->children[0]->content, "");
                        return base;
                    }
                    /* It isn't, we'll have to do things the long way round */
                    goto break_2;
                }
                /* otherwise fall through */
            default:
                /* This doesn't make sense... */
                u->content = astrcat(
                    "", "(ERROR: question could not find subject)", "");
                return base;
            }
        }
    break_2:
        /* We don't care about the noun now we know it isn't a relative pronoun,
           but now we have to find the verb, and chain it with "do" if
           necessary. */
        u->content = astrcat("", "(ERROR: unimplemented question)", "");
        break;
    }
    case gr_unknown:
        /* It may be worth considering using pronouns here, like 3.4.3.
           It'd be rather awkward, though, when we're using pronouns for other
           things too. Perhaps it should be an option. */
        u->content = astrcat("something","",t == possessive ? "'s" : "");
        return third;
    case gr_literal:
        /* Adverbs, adjectives, clauses, relative pronouns not used as nouns,
           prepositions, and sentences aren't declined in English. The main
           thing we need to do here is decline nouns and conjugate verbs. */
        switch(u->role) {
        case gr_adjective:
            /* One thing to watch out for here: A{2} translates to "two". */
            if (strspn(u->content, "0123456789") == strlen(u->content)) {
                char *tx = u->content;
                u->content = anumbername(atoi(tx), FALSE);
                free(tx);
                return base;
            }
            /* otherwise fall through */
        case gr_adverb:
        case gr_relative:
        case gr_preposition:
        case gr_clause:
        case gr_sentence:
            return base;
        case gr_propernoun:
        case gr_noun:
            /* We need to deal with four potential issues here:
               - If we have a pronoun (N{it}, N{you}, N{I}), it has entirely
                 special-cased grammar and also changes our return value (and
                 likewise, relative pronouns sometimes have special-cased
                 grammar and never take articles);
               - If quan is anything other than 1 with high bits set, we need to
                 pluralise the noun (that is, the passed-in quan, not the noun's
                 quan); and we also need to mark quantity;
               - Except for proper nouns, we need "a" for a non-plural noun with
                 the 1 << 30 bit set, and "the" for a plural or singular noun
                 without that bit set - but it needs to go outside adjectives,
                 and not appear if this noun is being possessed;
               - If it's a possessive, we need to add "'" to plural nouns ending
                 in -s, and "'s" to all others.
               The third issue obviously requires support from outside; we rely
               on the N{NA} and N{o,NN} rules to set quan appropriately in
               such cases. We let the N{NA} case put the article on the noun in
               that case (and it sends us a flag in quan to tell us not to put a
               second article on); the N{o,NN} case likewise sends the same
               flag. Because multiple sets of code are articulating things, the
               third issue's code is refactored out into a function
               articulate(). So we just need to bother about the other three. */
            if (!strcmp(u->content, "it")) {
                free(u->content);
                if ((quan &~ (1 << 30 | 1 << 27)) != 1) {
                    if (t == subject) u->content = astrcat("", "they", "");
                    else if (t == object) u->content = astrcat("", "them", "");
                    else u->content = astrcat("", "their", "");
                } else {
                    if (t == subject) u->content = astrcat("", "it", "");
                    else if (t == object) u->content = astrcat("", "it", "");
                    else u->content = astrcat("", "its", "");
                }
                return third;
            }
            if (!strcmp(u->content, "you")) {
                free(u->content);
                if (t == subject) u->content = astrcat("", "you", "");
                else if (t == object) u->content = astrcat("", "you", "");
                else u->content = astrcat("", "your", "");
                return second;
            }
            if (!strcmp(u->content, "I")) {
                free(u->content);
                if ((quan &~ (1 << 30 | 1 << 27)) != 1) {
                    if (t == subject) u->content = astrcat("", "we", "");
                    else if (t == object) u->content = astrcat("", "us", "");
                    else u->content = astrcat("", "our", "");
                } else {
                    if (t == subject) u->content = astrcat("", "I", "");
                    else if (t == object) u->content = astrcat("", "me", "");
                    else u->content = astrcat("", "my", "");
                }
                return first;
            }
            /* Possessive of both "who" and "what" is "whose"; object form of
               "who" is "whom" (although people keep forgetting about that one
               nowadays and its usage is dying out); relative pronouns don't
               take articles, can't be pluralized, and the other forms can't
               meaningfully be possessives either. */
            if ((!strcmp(u->content, "who") || !strcmp(u->content, "what")) &&
                t == possessive) {
                free(u->content);
                u->content = astrcat("", "whose", "");
                return third; /* can't pluralise relative pronouns */
            }
            if (!strcmp(u->content, "who") && t == object) {
                free(u->content);
                u->content = astrcat("", "whom", "");
                return third;
            }
            if (is_relative_pronoun(u->content))
                return third;
            /* If necessary, pluralize. */
            if ((quan &~ ((1 << 30) | (1 << 27))) != 1) {
                struct grammarunit *n;
                char *tx;
                for (n = u; n->rule != gr_literal && n->rule != noun_mX &&
                         n->rule != gr_unknown; n = n->children[0]) {}
                if (n->role == gr_noun && n->rule != gr_unknown)
                    tx = articulate(astrcat("", append_s(u->content), ""),
                                    quan);
                else
                    tx = astrcat("", append_s(u->content), "");
                free(u->content);
                /* If necessary, add "'" or "'s". */
                if (t == possessive) {
                    u->content = astrcat(
                        tx, tx[strlen(tx)-1] == 's' ? "'" : "'s", "");
                    free(tx);
                } else u->content = tx;
            } else {
                struct grammarunit *n;
                char *tx;
                for (n = u; n->rule != gr_literal && n->rule != noun_mX &&
                         n->rule != gr_unknown; n = n->children[0]) {}
                if (n->role == gr_noun && n->rule != gr_unknown)
                    tx = articulate(u->content, quan);
                else
                    tx = u->content;
                /* If necessary, add "'s"". */
                if (t == possessive) {
                    u->content = astrcat(tx, "'s", "");
                    free(tx);
                } else u->content = tx;                
            }
            return third;
        case gr_verb:
            /* We need to conjugate the verb to the given number and tense.
               This is factored out into a function conjugate(), because it's
               needed in other places too. */
        {
            char *c = conjugate(u->content, t, quan, p);
            free(u->content);
            u->content = astrcat("", c, "");
            return base;
        }
        }
    }
    return base;
}

static void
forcecontent_en(struct grammarunit *u, boolean simple, boolean caps)
{
    /* Recursively force the content of the unit in question, then initcaps the
       first letter. */
    force_unit(u, simple ? secondary_direct : present,
               u->quan | (simple ? (1 << 30) | (1 << 27) : 0),
               simple ? base : third);
    if (caps) *(u->content) = toupper(*(u->content));
}

/* For now, just a wrapper around the function for English.
   Eventually, this will probably move elsewhere and allow user-selectable
   languages. (Likely implementation: the server does the translation.)

   Its purpose is to force u->content to become a sentence that's the
   translation of the gramar structure u. */
void
forcecontent(struct grammarunit *u, boolean simple, boolean caps)
{
    forcecontent_en(u, simple, caps);
}
