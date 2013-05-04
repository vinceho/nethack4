/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/

/*	Copyright (c) Alex Smith 2012 			          */
/* NetHack may be freely redistributed.  See license for details. */

/* Checks string constants in a given list of files to see if they parse. Also
   works out a vocabulary, and prints the parsed versions of every constant and
   every word in every tense. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef signed char boolean; /* 0 or 1 */
# ifndef TRUE   /* defined in some systems' native include files */
#  define FALSE ((boolean)0)
#  define TRUE  ((boolean)!0)
# endif

extern char *malloc_parsestring(const char *, boolean, boolean);

int
main(int argc, char **argv) {
    int c;
    boolean errors_only = FALSE;
    boolean done_args = FALSE;

    for (char **argv2 = argv + 1; *argv2; argv2++) {
        if (strcmp(*argv2, "-e") == 0)
            errors_only = TRUE;
        else if (strcmp(*argv2, "--") == 0)
            break;
        else if ((*argv2)[0] == '-' && (*argv2)[1] != '\0')
            fprintf(stderr, "Unknown option: %s", *argv2);
       
    }

    for (argv++; *argv; argv++) {
        FILE* f;
        if (!done_args) {
            if (strcmp(*argv, "--") == 0)
                done_args = TRUE;
            if ((*argv[0]) == '-' && (*argv)[1] != '\0')
                continue;
        }
        if ((*argv)[0] == '-' && (*argv)[1] == '\0')
            f = stdin;
        else
            f = fopen(*argv, "rt");

        boolean prev_newline = FALSE;
        boolean prev_slash = FALSE;
        boolean prev_backslash = FALSE;
        boolean preprocline = FALSE;
        boolean prev_nointl = FALSE;
        boolean all_nointl = FALSE;
        int linecount = 1;
        if (!f) {
            perror(*argv);
            continue;
        }
        while ((c = getc(f)) != EOF) {
            /* There are some situations in which we don't want to recognise
               strings. The very first line of each file is a modeline, which
               contains double quotes. Likewise, #include statements can contain
               filenames surrounded by double quotes. Finally, code that
               manipulates grammartree may need to write fragments; we mark
               these with a comment "nointl". */
            if (prev_newline && c == '#') {
                preprocline = TRUE;
                prev_nointl = FALSE;
            }
            prev_newline = (c == '\n');
            if (prev_newline) {
                preprocline = FALSE;
                linecount++;
            }
            if (prev_slash && c == '*') {
                prev_nointl = FALSE;
                /* We use a rolling buffer to look for "nointl" and 
                 * "!nointl{" and "}nointl!" */
                static const int bufsz = 512;
                char buf[bufsz], *b = buf;
                while ((c = getc(f)) != EOF) {
                    /* Re-use prev-slash as a misnomer */
                    if (prev_slash && c == '/')
                        break;
                    prev_slash = (c == '*');
                    if (c == '\n')
                        linecount++;
                    *b++ = c;
                    if (b == buf + bufsz - 1) {
                        *b = 0;
                        if (strstr(buf, "!nointl{"))
                            all_nointl = TRUE;
                        else if (strstr(buf, "}nointl!"))
                            all_nointl = FALSE;
                        else if (strstr(buf, "nointl"))
                            prev_nointl = TRUE;
                        memcpy(buf, b - 10, 10);
                        b = buf + 10;
                    }
                }
                *b = 0;
                if (strstr(buf, "!nointl{"))
                    all_nointl = TRUE;
                else if (strstr(buf, "}nointl!"))
                    all_nointl = FALSE;
                else if (strstr(buf, "nointl"))
                    prev_nointl = TRUE;
                continue;
            }
            if (c == '"' && !preprocline) {
                /* Let's start parsing. */
                int i = 0;
                unsigned size = 512;
                char *str = malloc(size);
                if (str == NULL) {
                    fprintf(stderr, "ERROR: out of memory\n");
                    return 1;
                }
                do {
                    prev_backslash = FALSE;
                    while ((c = getc(f)) != EOF &&
                           (c != '"' || prev_backslash)) {
                        str[i++] = c;
                        if (c == '\n') linecount++;
                        if (c == '\\') prev_backslash = !prev_backslash;
                        else prev_backslash = FALSE;

                        if (i == size) {
                            str = realloc(str, size*=2);
                            if (str == NULL) {
                                fprintf(stderr, "ERROR: out of memory\n");
                                return 1;
                            }
                        }
                    }
                    /* Often multiple strings are appended. If we have only
                       whitespace up to the next double quote... */
                    while ((c = getc(f)) != EOF && isspace(c))
                        if (c == '\n') linecount++;
                } while (c == '"');
                str[i] = 0;
                if (i && !prev_nointl && !all_nointl) {
                    /* not the null string, not a nointl */
                    char *parsed = malloc_parsestring(str, FALSE, TRUE);
                    if (!errors_only || strstr(parsed, "(ERROR") == parsed)
                        printf("%s:%d: %s\n\t%s\n", *argv, linecount, str, parsed);
                    free(parsed);
                }
                free(str);
                prev_nointl = FALSE;
            }
            if (c == '\'' && !preprocline) {
                prev_nointl = FALSE;
                /* Assume that this is only one logical character long */
                if ((c = getc(f)) == '\\') {
                    getc(f); /* skip first character after slash */
                    do { /* there may be a oct/hex constant */
                      c = getc(f);
                    }  while (c != EOF && c != '\'');
                } else
                    c = getc(f);

                if (c != '\'') {
                    fprintf(stderr,
                            "%s:%d: ERROR: ill-formed character constant\n",
                            *argv, linecount);
                    return 1;
                }
            }
            prev_slash = (c == '/');
            if (!isspace(c))
                prev_nointl = FALSE;
        }
        fclose(f);
    }
    return 0;
}
