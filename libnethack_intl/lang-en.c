/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/*	Copyright (c) Alex Smith 2012 			          */
/* NetHack may be freely redistributed.  See license for details. */

/* Translation routines for English. We have the advantage here that the words
   we're getting from the grammar already /are/ in English, but we still need to
   conjugate/decline them and string them together to form sentences.
*/

#include "grammar.h"
#include "lang.h"

static boolean global_simple, global_caps;

enum tense {
    /* "You die", "You have died", "You died", "You are dying", "dying",
       "died", "die", "to die", "dying" */
    present, perfect, imperfect, continuous, future, active_participle,
    passive_participle, secondary_direct, secondary_infinitive, gerund,
    /* OK, so nouns don't have tenses, but we use the same method:
       "I", "me", "my" */
    subject, object, possessive,
};

/* I, you, it, thy, no subject */
enum person {first, second, third, archaic, base};

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
    if (!strcmp(s, "old")) return TRUE;
    if (!strcmp(s, "weak")) return TRUE;
    return FALSE;
}

static boolean
verb_chains_directly(const char *s)
{
    if (!strcmp(s, "can")) return TRUE;
    if (!strcmp(s, "will")) return TRUE;
    if (!strcmp(s, "may")) return TRUE;
    if (!strcmp(s, "should")) return TRUE;
    if (!strcmp(s, "had better")) return TRUE;
    if (!strcmp(s, "do")) return TRUE;
    return FALSE;
}

static boolean
verb_chains_via_participle(const char *s)
{
    if (!strcmp(s, "stop")) return TRUE;
    if (!strcmp(s, "like")) return TRUE;
    if (!strcmp(s, "see")) return TRUE;
    return FALSE;
}

static boolean
verb_chains_via_passive(const char *s)
{
    if (!strcmp(s, "are^passive")) return TRUE;
    return FALSE;
}

/* Verbs where the final consonant is repeated when suffixed, e.g. "flagged",
   not "flaged", is the passive participle of "flag" */
static boolean
stem_doubles_consonant(const char* s)
{
    if (!strcmp(s, "flag")) return TRUE;
    if (!strcmp(s, "rot")) return TRUE;
    if (!strcmp(s, "equip")) return TRUE;
    if (!strcmp(s, "hit")) return TRUE; /* "hitting" only */
    if (!strcmp(s, "trap")) return TRUE;
    if (!strcmp(s, "fulfil")) return TRUE;    
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
        case 0: rv = astrcat("", !ordinal ? "zero" : "zeroth", ""); break;
        case 1: rv = astrcat("", !ordinal ? "one" : "first", ""); break;
        case 2: rv = astrcat("", !ordinal ? "two" : "second", ""); break;
        case 3: rv = astrcat("", !ordinal ? "three" : "third", ""); break;
        case 4: rv = astrcat("", !ordinal ? "four" : "fourth", ""); break;
        case 5: rv = astrcat("", !ordinal ? "five" : "fifth", ""); break;
        case 6: rv = astrcat("", !ordinal ? "six" : "sixth", ""); break;
        case 7: rv = astrcat("", !ordinal ? "seven" : "seventh", ""); break;
        case 8: rv = astrcat("", !ordinal ? "eight" : "eighth", ""); break;
        case 9: rv = astrcat("", !ordinal ? "nine" : "ninth", ""); break;
        case 10: rv = astrcat("", !ordinal ? "ten" : "tenth", ""); break;
        case 11: rv = astrcat("", !ordinal ? "eleven" : "eleventh", ""); break;
        case 12: rv = astrcat("", !ordinal ? "twelve" : "twelfth", ""); break;
        case 13: rv = astrcat("", !ordinal ? "thirteen" : "thirteenth", ""); break;
        case 14: rv = astrcat("", !ordinal ? "fourteen" : "fourteenth", ""); break;
        case 15: rv = astrcat("", !ordinal ? "fifteen" : "fifteenth", ""); break;
        case 16: rv = astrcat("", !ordinal ? "sixteen" : "sixteenth", ""); break;
        case 17: rv = astrcat("", !ordinal ? "seventeen" : "seventeenth", ""); break;
        case 18: rv = astrcat("", !ordinal ? "eighteen" : "eighteenth", ""); break;
        case 19: rv = astrcat("", !ordinal ? "nineteen" : "nineteenth", ""); break;
        }
    } else if (n < 100) {
        if (n % 10) t = anumbername(n%10, ordinal);
        else t = 0;
        switch (n / 10) {
        case 20: rv = astrcat("twenty",  t ? t : "", t ? "-" : ""); break;
        case 30: rv = astrcat("thirty",  t ? t : "", t ? "-" : ""); break;
        case 40: rv = astrcat("forty",   t ? t : "", t ? "-" : ""); break;
        case 50: rv = astrcat("fifty",   t ? t : "", t ? "-" : ""); break;
        case 60: rv = astrcat("sixty",   t ? t : "", t ? "-" : ""); break;
        case 70: rv = astrcat("seventy", t ? t : "", t ? "-" : ""); break;
        case 80: rv = astrcat("eighty",  t ? t : "", t ? "-" : ""); break;
        case 90: rv = astrcat("ninety",  t ? t : "", t ? "-" : ""); break;
        default: /* impossible */
            rv = astrcat("(error)", t ? t : "", t ? "-" : ""); break;
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
        rv = astrcat(t2, t ? t : "", t ? (n % 1000000 >= 1000 ? " million, " :
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
    char *rv, *numname;
    char buf[30];
    if (quan & (1 << 26)) {
        /* Counts work like articles in our system. We need to decide between
           writing the number as digits and writing it as a word; the rule is
           that we use digits if either simple is set, or caps is not set, /and/
           the number is 11 or more, and a word otherwise. (So we have digits in
           menus and similar interface elements, and words in full
           sentences.) */
        if (((quan & ((1 << 26) - 1)) >= 11 &&
             (global_simple || !global_caps)) || (quan & (1 << 28))) {
            sprintf(buf, "%d", quan & ((1 << 26) - 1));
            if (quan & (1 << 28)) strcpy(buf, "%d");
            rv = astrcat(buf, s, " ");
        } else {
            numname = anumbername(quan & ((1 << 26) - 1), FALSE);
            rv = astrcat(numname, s, " ");
            free(numname);
        }
        free(s);
        return(rv);
    }
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
    p -= 2; /* invariant: p[1] is \0 */
    if (*p == 'y') *p = 'i';
    if (*p == 'e') *(p--) = 0;
    if (*suffix == 'i' && *p == 'i') *p = 'y';
    if (stem_doubles_consonant(resuffix_buffer)) {
        p[1] = *p;
        p[2] = '\0';
        p++;
    }
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

    /* knife/knives, etc...
       exception: we don't want "engulves" */
    if (!strcmp(str, "engulf")) {
        /* do nothing */
    } else if (!strcmp(spot - 1, "fe")) {
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
    if (!strcmp(v, "do")) {
        if (t == present) {
            if (!plural && (p == third)) strcpy(obuf, "does");
            else if (p == archaic) strcpy(obuf, "dost");
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
    if (!strcmp(v, "go")) {
        if (t == present) {
            if (!plural && (p == third)) strcpy(obuf, "goes");
            else strcpy(obuf, "go");
            return TRUE;
        }
        if (t == imperfect) {
            strcpy(obuf, "went");
            return TRUE;
        }
        if (t == passive_participle) {
            strcpy(obuf, "gone");
            return TRUE;
        }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "dare")) {
        if (t == present && p == archaic) {
            /* 3.4.3 inconsistently uses "darest" and "durst" here */
            strcpy(obuf, "durst");
            return TRUE;
        }
        return FALSE; /* usually regular */
    }
    if (verb_chains_directly(v)) {
        /* Directly-chaining verbs don't conjugate. Except for "do".
           They also don't work properly in tenses other than the present, in
           which case we have to swap in an entirely different verb (e.g.
           "be able" rather than "can"), but that's just a TODO for now. */
        strcpy(obuf, v);
        return TRUE;
    }
    if (!strcmp(v, "have")) {
        if (t == present) {
            if (!plural && (p == third)) strcpy(obuf, "has");
            else if (p == archaic) strcpy(obuf, "hast");
            else strcpy(obuf, "have");
            return TRUE;
        }
        if (t == imperfect || t == passive_participle) {
            strcpy(obuf, "had");
            return TRUE;
        }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "burn")) {
        /* "I burned", "I have burned", but "I am burnt"; we don't currently
           cover the second case because it isn't used and we don't have
           enough information to distinguish it from the third */
        if (t == passive_participle) { strcpy(obuf, "burnt"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "know")) {
        if (t == imperfect) { strcpy(obuf, "knew"); return TRUE; }
        if (t == passive_participle) { strcpy(obuf, "known"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "hold")) {
        if (t == imperfect) { strcpy(obuf, "held"); return TRUE; }
        if (t == passive_participle) { strcpy(obuf, "held"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "lay")) {
        if (t == imperfect || t == passive_participle) {
            strcpy(obuf, "laid");
            return TRUE;
        }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "eat")) {
        if (t == passive_participle) { strcpy(obuf, "eaten"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "ate"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "rot")) {
        if (t == passive_participle) { strcpy(obuf, "rotten"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "hit")) {
        if (t == passive_participle) { strcpy(obuf, "hit"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "hit"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "sit")) {
        if (t == passive_participle) { strcpy(obuf, "sat"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "sat"); return TRUE; }
        if (t == active_participle) { strcpy(obuf, "sitting"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "fall")) {
        if (t == passive_participle) { strcpy(obuf, "fallen"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "fell"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "blow")) {
        if (t == passive_participle) { strcpy(obuf, "blown"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "blew"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "draw")) {
        if (t == passive_participle) { strcpy(obuf, "drawn"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "drew"); return TRUE; }
        return FALSE; /* other cases are regular */
    }
    if (!strcmp(v, "will")) {
        if (p == archaic) {
            /* TODO: usually "shalt", actually, because if you're being
               pretentious you may as well go all the way; but it doesn't
               work in some contexts */
            if (t == present) { strcpy(obuf, "wilt"); return TRUE; }
            if (t == imperfect) { strcpy(obuf, "wouldst"); return TRUE; }
        }
        if (t == present) { strcpy(obuf, "will"); return TRUE; }
        if (t == imperfect) { strcpy(obuf, "would"); return TRUE; }
        /* the other cases don't exist... */
        strcpy(obuf, "(ERROR: bad 'will' tense)");
        return TRUE;
    }
    if (!strcmp(v, "are")) {
        switch (t) {
        case present:
            switch(p) {
            case first: strcpy(obuf, "am"); break;
            case second: strcpy(obuf, "are"); break;
            case third: strcpy(obuf, "is"); break;
            case archaic: strcpy(obuf, "art"); break;
            case base: strcpy(obuf, "be"); break;
            }
            if (plural) strcpy(obuf, "are");
            break;
        case imperfect:
            strcpy(obuf, "were");
            if ((p == first || p == third) && !plural)
                strcpy(obuf, "was");
            /* NetHack doesn't use this case and it's probably grammatically
               wrong, but for consistency with other angel speech... */
            if (p == archaic)
                strcpy(obuf, "wert");
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
    boolean plural = !!(quan & 1 << 29);
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
        /* some contractions */
        if (!strcmp(y, "are not")) sprintf(y, "aren't");
        if (!strcmp(y, "is not")) sprintf(y, "isn't");
        if (!strcmp(y, "was not")) sprintf(y, "wasn't");
        if (!strcmp(y, "were not")) sprintf(y, "weren't");
        if (!strcmp(y, "do not")) sprintf(y, "don't");
        if (!strcmp(y, "does not")) sprintf(y, "doesn't");
        if (!strcmp(y, "did not")) sprintf(y, "didn't");
        if (!strcmp(y, "has not")) sprintf(y, "hasn't");
        if (!strcmp(y, "have not")) sprintf(y, "haven't");
        if (!strcmp(y, "had not")) sprintf(y, "hadn't");
        if (!strcmp(y, "will not")) sprintf(y, "won't");
        if (!strcmp(y, "would not")) sprintf(y, "wouldn't");
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
    case future: /* conjugate "will" + present */
        x = conjugate("will", present, quan, p);
        strcat(x, " ");
        strcat(x, conjugate(v, present, 1, base));
        return x;
    case secondary_infinitive: /* "to" + stem */
        strcpy(w, "to ");
        strcat(w, conjugate(v, secondary_direct, quan, p));
        break;
    case present: /* stem, or stem + s in third person singular */
        if (special_case_verb(w, v, t, plural, p)) return w;
        if (p == third && !plural) {
            strcpy(w, append_s(v));
        } else if (p == archaic) {
            strcpy(w, append_s(v));
            strcat(w, "t");
        } else strcpy(w, v);
        break;
    case imperfect: /* resuffix -ed, for regular verbs */
    case passive_participle: /* also resuffix -ed, for regular verbs */
        if (special_case_verb(w, v, t, plural, p)) return w;
        strcpy(w, resuffix(v, "ed"));
        break;
    case gerund: /* for regular verbs, the gerund is the active participle */
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
        strcpy(w,
               "(ERROR: noun type found where verb tense expected)");
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
        force_unit(u->children[0], t, u->children[0]->quan, p);
        force_unit(u->children[1], t, u->children[0]->quan, p);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content,
                             u->rule == plus_CC ?
                             (negated ? ", but " : ", and ") :
                             (negated ?  " but " :  " and "));
        /* nounperson doesn't matter because this is necessarily pluralised,
           but it needs to not be base */
        if (u->rule == plus_NN) return third;
        break;
    case minus_N: /* "nonweapon", "not a weapon", "not the weapon" */
        force_unit(u->children[0], t, quan, p);
        if (strchr(u->children[0]->content, ' '))
            u->content = astrcat("not", u->children[0]->content, " ");
        else
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
    case minus_D: /* "Not slowly" is about the best we can do in general for
                     verbs. "Not very" is perfect for adjectives. For some
                     particular construction rules we can do better; for
                     instance, adverb_tN can be formed using "without". */
        if (u->children[0]->rule == adverb_tN) {
            force_unit(u->children[0], t, quan, p);
            u->content = astrcat(
                "", u->children[0]->children[0]->content, "without ");
            break;
        }
        /* otherwise fall through */
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
           for the "not" prefix because I'm lazy. */
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
        case clause_fNV:
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
        case clause_fsV:
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
        {
            struct grammarunit *a;
            /* Proper nouns are specialcased when interacting with adjective_N;
               instead of "the blue dragon Asidonhopo", we want "Asidonhopo the
               blue dragon". Also, any non-adjective_N adjectives that would be
               applied get pushed inside the adjective_N. */
            for (a = u->children[0];
                 a->rule != gr_literal && a->rule != noun_mX &&
                     a->rule != gr_unknown;
                 a = a->children[0]) {}
            if (a->role != gr_noun) {
                /* Look for an adjective_N being applied. */
                struct grammarunit *a_N = 0;
                for (a = u; a->rule == noun_NA; a = a->children[0]) {
                    if (a->children[1]->rule == adjective_N) {
                        a_N = a->children[1];
                        break;
                    }
                }
                if (a_N) {
                    for (a = u; a->rule == noun_NA; a = a->children[0]) {
                        /* Move an adjective from the proper noun to
                           the modifier: "the angry Asidonhopo the blue
                           dragon" -> "Asidonhopo the angry blue dragon" */
                        while (a->children[1]->rule != adjective_N) {
                            struct grammarunit *b = a->children[0];
                            struct grammarunit *c = a->children[1];
                            *a = *b; /* detaches b, c */
                            /* We're effectively freeing then mallocing b;
                               so instead we just overwrite every field */
                            b->rule = noun_NA;
                            b->children[0] = a_N->children[0];
                            b->children[1] = c; /* reattaches c */
                            b->children[2] = 0;
                            b->role = gr_noun;
                            b->content = 0;
                            b->uniquifier = 0;
                            b->punctuation = 0;
                            b->gender = b->children[0]->gender;
                            b->quan = b->children[0]->quan;
                            b->tagged = FALSE;
                            a_N->children[0] = b;
                        }
                    }
                    /* a_N is now u->children[1] */
                    force_unit(u->children[1]->children[0], t,
                               u->children[1]->children[0]->
                               quan & ~(1 << 30), p);
                    nounperson = force_unit(u->children[0], t, quan, p);
                    u->content = astrcat(u->children[0]->content,
                                         u->children[1]->children[0]->content,
                                         " ");
                    return nounperson;
                }
            }

            /* Be careful of the adjective_QC case: "the cave where the dragon
               sleeps", not "the where the dragon sleeps cave" (which is cute
               and understandable but nonidiomatic). adjective_lN and
               adjective_aN have similar rules: "the stranger at the door",
               etc. adjective_V also moves after the noun, but only when it's
               not a literal verb; "cursed dagger", but "dagger named
               Vladsbane". (And adjective_VN moves after the noun for the same
               reason.) Parenthesizing adjectives does not change this rule. Nor
               does conjuncting them, although in dubious cases like "large and
               painted blue" we have to make an arbitrary choice (in this case,
               we go with the first adjective). */
            force_unit(u->children[1], t, quan, p);
            a = u->children[1];
            while (a->rule == adjective_pA || a->rule == plus_AA)
                a = a->children[0];
            if (a->rule == adjective_QC || a->rule == adjective_EN ||
                a->rule == adjective_mN || a->rule == adjective_lN ||
                a->rule == adjective_aN ||
                (a->rule == adjective_V &&
                 a->children[0]->rule != gr_literal) ||
                a->rule == adjective_VN) {
                nounperson = force_unit(u->children[0], t, quan, p);
                u->content = astrcat(u->children[0]->content,
                                     u->children[1]->content, " ");
            } else {
                /* We need to move any "the" or "a"/"an" to before the
                   /adjective/. We do this by suppressing the article on the
                   noun (this is what the 1<<30 | 1<<27 means), and adding it
                   here ourself. With a proper noun, we always want to add
                   "the", unless the article is suppressed or it's plural
                   ("Vlad", "two Vlads", "the poor Vlad", "two poor Vlads"). */
                struct grammarunit *n;
                nounperson = force_unit(u->children[0], t,
                                        quan | (1 << 30) | (1 << 27), p);
                u->content = astrcat(u->children[1]->content,
                                     u->children[0]->content, " ");
                for (n = u; n->rule != gr_literal && n->rule != noun_mX &&
                         n->rule != gr_unknown; n = n->children[0]) {}
                if (n->role == gr_noun && n->rule != gr_unknown)
                    u->content = articulate(u->content, quan);
                else if (quan & (1 << 29))
                    u->content = articulate(
                        u->content, quan | (1 << 27) | (1 << 30));
                else if (!(quan & (1 << 27)))
                    u->content = articulate(u->content, quan & ~(1 << 30));
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
    case noun_V: /* "smashing" */
        nounperson = force_unit(u->children[0], gerund, 1, base);
        u->content = astrcat("",u->children[0]->content,"");
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
        if (u->children[0]->role == gr_clause &&
            u->children[0]->punctuation) {
            char buf[10];
            sprintf(buf, "%.5s\"", u->children[0]->punctuation);
            u->content = astrcat("\"", buf, u->children[0]->content);
        } else
            u->content = astrcat("\"", "\"", u->children[0]->content);
        return third;
    case verb_mVX: /* verb with a colon and anything in quotes */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1],
                   u->children[1]->role == gr_noun ? object : present,
                   u->children[0]->quan | (1 << 30) | (1 << 27), base);
        /* Uppercase the first letter of u->children[1] unless it's
           arbitrary freeform text */
        if (u->children[1]->role != gr_sentence)
            *(u->children[1]->content) =
                toupper(*(u->children[1]->content));
        /* the ! are for padding */
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content,
                             t == gerund ? " of \"!!!!!!" : ": \"!!!!!!");
        sprintf(u->content + strlen(u->children[0]->content) + 3,
                "%s%.5s\"", u->children[1]->content,
                u->children[1]->role == gr_clause &&
                u->children[1]->punctuation ?
                u->children[1]->punctuation : "");
        break;
    case noun_aNV: /* "burning bush", "the newt hitting Dudley" */
        /* We put the verb first if it's a literal or has exactly one literal
           adverb, afterwards otherwise. */
        if (u->children[1]->rule == gr_literal ||
            (u->children[1]->rule == verb_VD &&
             u->children[1]->children[0]->rule == gr_literal &&
             u->children[1]->children[1]->rule == gr_literal)) {
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
           "don't" and "cannot" or "can't"; "cannot" is done here, "don't" in
           conjugate().) The exception is that chain-directly verbs (and "are"
           for some reason) are negated by /suffixing/ not.

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
            !strcmp(u->children[0]->content, "are") ||
            !strcmp(u->children[0]->content, "are^passive")) {
            char *r = strchr(u->children[0]->content, '^'), c;
            if (r) {c = *r; *r = 0;}
            u->content = astrcat(conjugate(u->children[0]->content, t, quan, p),
                                 "not", " ");
            if (!strcmp(u->content, "can not")) sprintf(u->content, "cannot");
            if (r) *r = c;
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
                conjugate("have not", present, quan, p),
                conjugate(u->children[0]->content, passive_participle, 1, base),
                " ");
            break;
        case continuous:
            u->content = astrcat(
                conjugate("are not", present, quan, p),
                conjugate(u->children[0]->content, active_participle, 1, base),
                " ");
            break;
        case future:
            u->content = astrcat(
                conjugate("will not", present, quan, p),
                conjugate(u->children[0]->content, present, 1, base),
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
        case gerund:
            u->content = astrcat("", "(ERROR: Gerund negated)", "");
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
            u->children[0]->rule == verb_VV ||
            u->children[0]->rule == verb_sVV) {
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
        /* If the noun is tagged, surround it with \x1e. */
        if (u->children[1]->tagged) {
            char *tx;
            tx = astrcat(u->children[0]->content,
                         u->children[1]->content,
                         t == gerund ? " of \x1e" : " \x1e");
            u->content = astrcat(tx, "\x1e", "");
            free(tx);
        } else
            u->content = astrcat(u->children[0]->content,
                                 u->children[1]->content,
                                 t == gerund ? " of " : " ");
        break;
    case verb_sVV:
        /* We change this to a verb_VV with the secondary verb behind
           "are^passive", then tail-recurse. */
    {
        struct grammarunit *w;
        w = malloc(sizeof(struct grammarunit));
        w->role = gr_verb;
        w->rule = verb_VV;
        w->children[0] = malloc(sizeof(struct grammarunit));
        w->children[0]->role = gr_verb;
        w->children[0]->rule = gr_literal;
        w->children[0]->children[0] = 0;
        w->children[0]->children[1] = 0;
        w->children[0]->children[2] = 0;
        w->children[1] = u->children[1];
        u->children[1] = w;
        w->children[2] = 0;
        w->uniquifier = w->children[0]->uniquifier = 0;
        w->punctuation = w->children[0]->punctuation = 0;
        w->quan = w->children[0]->quan = 1;
        w->gender = w->children[0]->gender = gg_unknown;
        w->tagged = w->children[0]->tagged = FALSE;
        w->content = 0;
        w->children[0]->content = astrcat("", "are^passive", "");
        u->rule = verb_VV;
        return force_unit(u, t, quan, p);
    }
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
            else if (verb_chains_via_participle(v->content))
                force_unit(u->children[1], active_participle, 1, base);
            else if (verb_chains_via_passive(v->content))
                force_unit(u->children[1], passive_participle, 1, base);
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
           are rather awkward), and there are a few literals we also don't allow
           ("you anyway know that" sounds weird). We can't use the fourth case
           without help from the clause rules, so if we get here at all, we're
           using the second or third. And the third seems unnatural in pretty
           much all cases, so we don't use it. (The main case in which we'd want
           to use the fourth is D{QC}, by the way; and the rule for C{NV}
           supports this way round of doing things.)

           One other issue is involved with infinitives: "boldly to go" may be
           /technically/ correct (or possibly "to go boldly"), but splitting the
           infinitive as "to boldly go" is much more common, even if it annoys
           some people. So we take care to split it if necessary. */
        force_unit(u->children[0], t == secondary_infinitive ?
                   secondary_direct : t, quan, p);
        force_unit(u->children[1], t, quan, p);
        if (u->children[1]->rule != gr_literal ||
            strchr(u->children[1]->content, ' ') ||
            !strcmp(u->children[1]->content, "already") ||
            !strcmp(u->children[1]->content, "anyway") ||
            !strcmp(u->children[1]->content, "again") ||
            !strcmp(u->children[1]->content, "now") ||
            !strcmp(u->children[1]->content, "here") ||
            !strcmp(u->children[1]->content, "there") ||
            !strcmp(u->children[1]->content, "everywhere") ||
            !strcmp(u->children[1]->content, "nearby") ||
            !strcmp(u->children[1]->content, "brightly") ||
            !strcmp(u->children[1]->content, "upward") ||
            !strcmp(u->children[1]->content, "downwards"))
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
    case adverb_tN: /* "with a hammer", "by water" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        if (u->children[0]->quan & (1 << 27))
            u->content = astrcat("", u->children[0]->content, "by ");
        else
            u->content = astrcat("", u->children[0]->content, "with ");
        break;
    case adverb_oN: /* "with a bright flash" */
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
    case adverb_eEN: /* "into the box" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        force_unit(u->children[1], object, u->children[1]->quan, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " ");
        break;
    case adverb_iN: /* "to his mother" */
        force_unit(u->children[0], object, u->children[0]->quan, base);
        u->content = astrcat("", u->children[0]->content, "to ");
        break;
    case adverb_QC: /* "while the dragon sleeps" */
    case adverb_EN: /* "from hunger" */
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
        } else if (u->children[0]->rule == gr_literal &&
                   !strcmp(u->children[0]->content, "good")) {
            /* not a regular adjective */
            u->content = astrcat("", u->rule == adjective_cA ?
                                 "better" : "best", "");
        } else if (u->children[0]->rule == gr_literal &&
                   !strcmp(u->children[0]->content, "bad")) {
            /* also not a regular adjective */
            u->content = astrcat("", u->rule == adjective_cA ?
                                 "worse" : "worst", "");
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
        {
            struct grammarunit *a;

            force_unit(u->children[0], t, quan, p);
            force_unit(u->children[1], t, quan, p);

            a = u->children[1];
            while (a->rule == plus_DD)
                a = a->children[0];

            if (a->rule == adverb_EN)
                u->content = astrcat(u->children[0]->content,
                                     u->children[1]->content, " ");
            else
                u->content = astrcat(u->children[1]->content,
                                     u->children[0]->content, " ");
            break;
        }
    case adverb_pV: /* "stop to think" */
        force_unit(u->children[0], secondary_infinitive, 1 , base);
        u->content = astrcat("", u->children[0]->content, "");
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
    case adjective_VN: /* "blessed by the gods" */
        /* We use the passive participle of the verb, and add a preposition. */
        force_unit(u->children[0], passive_participle, 1, base);
        force_unit(u->children[1], subject, u->children[1]->quan, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " by ");
        break;
    case adjective_N: /* "gnome" */
        /* Be careful not to pass on t or quan; also, suppress "the"; also,
           we want to use the singular form of a plural noun but still show
           the count */
        force_unit(u->children[0], object,
                   (u->children[0]->quan | (1 << 30) | (1 << 27)) & ~(1 << 29),
                   base);
        u->content = astrcat("", u->children[0]->content, "");
        break;
    case adjective_QC: /* "where the dragon sleeps" */
    case adjective_EN: /* "in disgust" */
        /* This one's a little awkward because it's placed after nouns rather
           than before, "the cave where the dragon sleeps". The support for this
           is in the noun_NA case, so we only need bother with translating the
           adjective itself. */
        force_unit(u->children[0], t, quan, p);
        force_unit(u->children[1], present, u->children[1]->quan, base);
        u->content = astrcat(u->children[0]->content,
                             u->children[1]->content, " ");
        break;
    case adjective_oA: /* "second", "third" */
        if (u->children[0]->rule != gr_literal ||
            strspn(u->children[0]->content, "0123456789") !=
            strlen(u->children[0]->content)) {
            force_unit(u->children[0], t, quan, p);
            if (!strcmp(u->children[0]->content, "%d"))
                u->content = astrcat("", "%dth", "");
            else 
                u->content = astrcat("", u->children[0]->content, "");
        } else {
            u->content = anumbername(atoi(u->children[0]->content), TRUE);
        }
        break;
    case clause_NV:  /* "the goblin kills you" */
    case clause_iNV: /* "the goblin killed you" */
    case clause_pNV: /* "the goblin has killed you" */
    case clause_cNV: /* "the goblin is killing you" */
    case clause_fNV: /* "the goblin will kill you" */
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
                   u->rule == clause_pNV ? perfect :
                   u->rule == clause_fNV ? future : continuous,
                   u->children[0]->quan, nounperson);
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
    case clause_fsV: /* "you will be killed" */
        /* In English, we form the passive using the passive participle and a
           conjugation of "are". To accomplish this, we have to grab the object
           out of the verb. (We can't form the passive of a verb with no object,
           amusing though it would be.) */
    {
        struct grammarunit *v, *w, *x;
        boolean move_to_secondaries = FALSE;
        int cnumber = 0;
        v = u;
        while (TRUE) {
            cnumber = (move_to_secondaries &&
                       (v->rule == verb_VV || v->rule == verb_sVV)) ? 1 : 0;

            if (v->children[cnumber]->rule == gr_literal) {
                /* This is a little awkward. We get here in the case "something
                   can kill you" -> "you can be killed" but can't go here
                   unconditionally because "something wants you to kill" -> "you
                   are wanted to kill" (In the case where both these happen,
                   this case isn't met and we get "something wants you to kill a
                   goblin" -> "you are wanted to kill a goblin" which is
                   correct.)

                   So what we do is we start again, but this time, if we
                   encounter a secondary verb, we move to the second child.
                */
                if (move_to_secondaries) {
                    u->content = astrcat(
                        "", "(ERROR: passive verb with no object)", "");
                    return base;
                }
                move_to_secondaries = TRUE;
                v = u;
                cnumber = (move_to_secondaries &&
                           (v->rule == verb_VV || v->rule == verb_sVV)) ? 1 : 0;
            }

            if (v->children[cnumber]->rule == verb_VN) break;
            v = v->children[cnumber];
        }
        /* We change the old verb to a verb_VV with the old verb as its
           secondary and "are^passive" as the primary. (Auxiliary verbs chain
           directly, whereas "are" normally chains via infinitive: "you are to
           kill" and "you are killed" have quite different meanings.)*/
        w = malloc(sizeof(struct grammarunit));
        w->role = gr_verb;
        w->rule = verb_VV;
        w->children[0] = malloc(sizeof(struct grammarunit));
        w->children[0]->role = gr_verb;
        w->children[0]->rule = gr_literal;
        w->children[0]->children[0] = 0;
        w->children[0]->children[1] = 0;
        w->children[0]->children[2] = 0;
        w->children[1] = v->children[cnumber];
        v->children[cnumber] = w;
        w->children[2] = 0;
        w->uniquifier = w->children[0]->uniquifier = 0;
        w->punctuation = w->children[0]->punctuation = 0;
        w->quan = w->children[0]->quan = 1;
        w->gender = w->children[0]->gender = gg_unknown;
        w->tagged = w->children[0]->tagged = FALSE;
        w->content = 0;
        w->children[0]->content = astrcat("", "are^passive", "");

        /* Eliminate the noun from the verb we found. */
        x = w->children[1];
        v = x->children[1];
        w->children[1] = x->children[0]; /* now x and v are detached, x->0 isn't */
        x->children[0] = x->children[1] = 0; /* don't free x->0 or v */
        free_grammarunit(x);

        nounperson = force_unit(v, subject, v->quan, base);
        force_unit(u->children[0],
                   u->rule == clause_sV ? present :
                   u->rule == clause_isV ? imperfect :
                   u->rule == clause_psV ? perfect :
                   u->rule == clause_fsV ? future : continuous,
                   v->quan, nounperson);
        if (v->tagged) {
            char *tx = astrcat("\x1e", "\x1e", v->content);
            u->content = astrcat(tx, u->children[0]->content, " ");
            free(tx);
        } else
            u->content = astrcat(v->content, u->children[0]->content, " ");
        free_grammarunit(v); /* now free v, x already freed, x->0 still on tree */
        break;
    }
    case clause_CD: /* "Sadly, you die" */
        force_unit(u->children[1], p, quan, t);
        force_unit(u->children[0], p, quan, t);
        u->content = astrcat(u->children[1]->content, u->children[0]->content,
                             ", ");
        break;
    case noun_sC: /* "the newt that you hit" */
    {
        /* This is basically a case of tagging the object, and moving it to
           the start of the sentence after forcing. We find the object much
           the same way as for passive clauses. (Note that noun_sC works on
           passive clauses themselves; we mark the object, and it gets moved
           to the start of the sentence, then to the start of the sentence
           again, and the main difference is the addition of "that".) */
        struct grammarunit *v, *w;
        boolean move_to_secondaries = FALSE;
        int cnumber = 0;
        v = u->children[0];
        if (v->rule == clause_NV  || v->rule == clause_iNV ||
            v->rule == clause_pNV || v->rule == clause_cNV ||
            v->rule == clause_fNV)
            cnumber = 1;
        else cnumber = 0;
        w = v;
        while (TRUE) {
            if (v->children[cnumber]->rule == verb_VN) break;
            v = v->children[cnumber];

            cnumber = (move_to_secondaries &&
                       (v->rule == verb_VV || v->rule == verb_sVV)) ? 1 : 0;

            if (v->children[cnumber]->rule == gr_literal) {
                if (move_to_secondaries) {
                    u->content = astrcat(
                        "", "(ERROR: noun_sC with no object)", "");
                    return base;
                }
                move_to_secondaries = TRUE;
                v = w;
                if (v->rule == clause_NV  || v->rule == clause_iNV ||
                    v->rule == clause_pNV || v->rule == clause_cNV ||
                    v->rule == clause_fNV)
                    cnumber = 1;
                else cnumber = 0;
            }
        }
        v->children[cnumber]->children[1]->tagged = TRUE; /* tag it */
        force_unit(u->children[0], present, 1, base);
        /* Look for the \x1e markers. We want to remove them, the noun
           between them, and one surrounding space. We remove the space
           before the first \x1e, unless it's at the start of the sentence. */
        if (*u->children[0]->content == '\x1e') {
            char *tx = strchr(u->children[0]->content+1, '\x1e');
            if (!tx) {
                u->content = astrcat("", "(ERROR: unmatched \\x1e)", "");
                return base;
            }
            *tx = '\0'; tx++;
            u->content = astrcat(u->children[0]->content+1, tx, " that");
        } else {
            char *tx = strchr(u->children[0]->content, '\x1e');
            char *ty;
            if (!tx) {
                u->content = astrcat("", "(ERROR: missing \\x1e)", "");
                return base;
            }
            tx[-1] = '\0';
            ty = strchr(tx+1, '\x1e');
            if (!ty) {
                u->content = astrcat("", "(ERROR: unmatched \\x1e)", "");
                return base;
            }
            *ty = '\0'; ty++;
            ty = astrcat(u->children[0]->content, ty, "");
            u->content = astrcat(tx+1, ty, " that ");
            free(ty);
        }
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
           - If the sentence is using neither a directly chainable verb or
             "are", that verb is made into a secondary verb behind "do" (this
             is the same rule as is used for negation, incidentally), and
           - The primary verb is moved to the start of the sentence (while
             keeping everything else unchanged; even adverbs stay behind, e.g.
             "Are you certainly Rodney?" is the most natural question form of
             "You certainly are Rodney"; arguably this is just attaching the
             adverb to the complement or secondary, because "are" (which takes
             a complement) and direct-chainers (which take secondaries) are the
             only cases in which this can even come up, but adverbs don't
             normally modify nouns; and prepositions in separable verbs also
             stay behind, "Had you better do that now?"), and
           - The first relative pronoun that's used as a noun (i.e. N{where}
             rather than Q{where}) is moved to the start of the sentence, before
             the primary verb, if there is one (and if it's used to form an
             adverb, as in "at what time", the whole adverb phrase moves)

           There's also some sort of rule related to relative pronouns being
           used as subjects ("Who killed the goblin?", not "Who did kill the
           goblin?"), but it's inconsistent ("Who am I?", not "Who is I?"). So
           it's left out for the time being, until I can work out what it is;
           the questions come out reasonable, if a little unnatural, even
           without it. (Some ideas: in "Who am I?", "who" is the /object/, with
           the base sentence being "I am whom", but then why isn't it "Whom am
           I?"; and words like "where" are actually relative /proadverbs/, not
           /pronouns/, and so can't be used as subjects at all. (Try to
           construct the base form of a sentence using "where" as a noun. You
           can't do it.) Hmm, perhaps the rule is as simple as "do" being
           unneccessary if the statement rearrangement is a no-op, because the
           reason a secondary verb is added is to give something to rearrange.)

           It's also possible to form a question from an imperative, but that is
           (fortunately for my sanity) a no-op, apart from the use of a question
           mark (even if it contains a relative pronoun: "Go where?").

           We start off by finding the verb, and the base verb. */
    {
        int cnumber;
        struct grammarunit *v, *w;
        char *tx, *ty, r;
        /* Find the verb phrase. */
        switch (u->children[0]->rule) {
        case clause_iV:
            /* The easy case. */
            force_unit(u->children[0], t, quan, p);
            u->content = astrcat("", u->children[0]->content, "");
            return base;
        case clause_NV: case clause_iNV: case clause_pNV: case clause_cNV:
        case clause_fNV:
            cnumber = 1;
            break;
        case clause_sV: case clause_isV: case clause_psV: case clause_csV:
        case clause_fsV:
            cnumber = 0;
            break;
        default: /* clause_qC, literal, etc. */
            u->content = astrcat("", "(ERROR: unimplemented question)", "");
            return base;
        }
        /* Find the base verb. */
        for (v = u->children[0]->children[cnumber];
             v->rule != gr_literal; v = v->children[0]) {}
        if (!verb_chains_directly(v->content) &&
            strcmp(v->content,"are")) {
            /* Nest a secondary verb around u->children[cnumber]. */
            w = malloc(sizeof(struct grammarunit));
            w->role = gr_verb;
            w->rule = verb_VV;
            w->children[0] = malloc(sizeof(struct grammarunit));
            w->children[0]->role = gr_verb;
            w->children[0]->rule = gr_literal;
            w->children[0]->children[0] = 0;
            w->children[0]->children[1] = 0;
            w->children[0]->children[2] = 0;
            w->children[1] = u->children[0]->children[cnumber];
            u->children[0]->children[cnumber] = w;
            w->children[2] = 0;
            w->uniquifier = w->children[0]->uniquifier = 0;
            w->punctuation = w->children[0]->punctuation = 0;
            w->quan = w->children[0]->quan = 1;
            w->gender = w->children[0]->gender = gg_unknown;
            w->tagged = w->children[0]->tagged = FALSE;
            w->content = 0;
            w->children[0]->content = astrcat("", "do", "");
            v = w->children[0]; /* new base verb */
        }
        /* To move the primary verb, we mark the verb as needing a special
           tag, generate the sentence content, then search for it in the
           sentence afterwards. */
        v->tagged = TRUE;
        force_unit(u->children[0], t, quan, p);
        /* Find the start of the primary verb. */
        tx = strchr(u->children[0]->content, '\x1d');
        if (!tx) {
            u->content = astrcat(
                "", "(ERROR: verb went missing in question)", "");
            return base;
        }
        if (tx != u->children[0]->content) {
            /* We only move the verb if it isn't already at the start of the
               sentence. */
            if (tx[-1] != ' ') {
            u->content = astrcat(
                "", "(ERROR: verb not preceded by space in question)", "");
            return base;
            }
            tx[-1] = '\0';
            *(tx++) = '\0';
            /* Find the end of the verb, by looking for the space or punctuation
               mark after it (or the end of string). */
            ty = tx + strcspn(tx, " .?!,;()\"");
            r = *ty; *ty = '\0';
            tx = astrcat(tx, u->children[0]->content, " ");
            *ty = r;
            u->content = astrcat(tx, ty, "");
            free(tx);
        }
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
           thing we need to do here is decline nouns and conjugate verbs.
           We also need to remove any disambiguators ("^" and anything after
           it at the end of the word); for the time being, we do that first. */
        if (strchr(u->content, '^')) *(strchr(u->content, '^')) = '\0';
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
               - If quan is marked as plural, we need to pluralise the noun
                 (that is, the passed-in quan, not the noun's quan); and if it's
                 marked as having an explicit count, we also need to mark
                 quantity;
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
                if (quan & (1 << 29)) {
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
            if (!strcmp(u->content, "thou")) {
                free(u->content);
                if (t == subject) u->content = astrcat("", "thou", "");
                else if (t == object) u->content = astrcat("", "thee", "");
                else u->content = astrcat("", "thy", "");
                return archaic;
            }
            if (!strcmp(u->content, "I")) {
                free(u->content);
                if (quan & (1 << 29)) {
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
            /* If necessary, pluralize. Also add the count (articulate does
               this for us). */
            if (quan & (1 << 29)) {
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
               needed in other places too. If requested, we tag the first word
               of the verb with \x1d, so that question-forming code can find it
               in the resulting string. */
        {
            char *c = conjugate(u->content, t, quan, p);
            free(u->content);
            u->content = astrcat(u->tagged ? "\x1d" : "", c, "");
            return base;
        }
        }
    }
    return base;
}

static void
forcecontent_en(struct grammarunit *u, boolean simple, boolean caps)
{
    global_simple = simple;
    global_caps = caps;
    /* Recursively force the content of the unit in question, then initcaps the
       first letter. */
    force_unit(u, simple ? secondary_direct : present,
               u->quan | (simple ? (1 << 30) | (1 << 27) : 0),
               simple ? base : third);
    if (caps) *(u->content) = toupper(*(u->content));
}

/* For now, just a wrapper around the function for English.
   Eventually, this will probably move elsewhere and allow user-selectable
   languages. (Likely implementation: the client does the translation.)

   Its purpose is to force u->content to become a sentence that's the
   translation of the grammar structure u. */
void
forcecontent(struct grammarunit *u, boolean simple, boolean caps)
{
    forcecontent_en(u, simple, caps);
}
