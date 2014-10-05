/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2014-10-05 */
/* Copyright (c) 2014 Alex Smith. */
/* The NetHack server may be freely redistributed under the terms of either:
 *  - the NetHack license
 *  - the GNU General Public license v2 or later
 */

#include "nhserver.h"
#include "netconnect.h"
#include <sys/select.h>

#define STARTSCUM_POINTS 20000

static int
wait_input(int fd)
{
    fd_set rfds;
    struct timeval tv = {.tv_sec = 30, .tv_usec = 0};
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    int rv = select(fd + 1, &rfds, NULL, NULL, &tv);
    return rv > 0; /* no error, and some input */
}

static int
wait_numeric(int fd, int num)
{
    enum {wn_newline, wn_colon, wn_number, wn_junk} state = wn_junk;
    unsigned parsed_number = 0;
    for(;;) {
        char in;
        if (!wait_input(fd))
            return FALSE;
        if (read(fd, &in, 1) != 1)
            return FALSE;

        if (in == '\n') {
            state = wn_newline;
        } else if (state == wn_newline) {
            state = in == ':' ? wn_colon : wn_junk;
        } else if (state == wn_colon && in == ' ') {
            state = wn_number;
            parsed_number = 0;
        } else if (state == wn_number) {
            if (in >= '0' && in <= '9') {
                parsed_number *= 10;
                parsed_number += in - '0';
            } else if (in == ' ') {
                if (parsed_number == num)
                    return TRUE;
                state = wn_junk;
            } else
                state = wn_junk;
        }
    }
}

static int
wait_error(int fd)
{
    int charseq_seen = 1;
    static const char charseq[] = "\nERROR";
    for(;;) {
        char in;
        if (!wait_input(fd))
            return FALSE;
        if (read(fd, &in, 1) != 1)
            return FALSE;

        if (in != charseq[charseq_seen])
            charseq_seen = 0;
        else if (charseq_seen < sizeof charseq - 2)
            charseq_seen++;
        else
            return TRUE;
    }
}

static inline int
send_c_string(int fd, const char *str)
{
    log_msg(str);
    return write(fd, str, strlen(str));
}

extern void
irc_log_game_over(const struct nh_topten_entry *tte)
{
    /* startscum filter */
    if (tte->points < STARTSCUM_POINTS) {
        log_msg("Not reporting game over to IRC: score too low");
        return;
    }

    if (!settings.irchost || !settings.ircnick ||
        !settings.ircpass || !settings.ircchannel) {
        log_msg("Tried to report game over to IRC, but settings are missing");
        return;
    }

    /* paranoia: reject control characters */
    char *r;
    for (r = tte->entrytxt; *r; r++)
        if (*r < ' ' || *r > '~') {
            log_msg("Not sending game over to IRC: "
                    "dubious characters in death message\n");
            return;
        }
    for (r = user_info.username; *r; r++)
        if (*r < ' ' || *r > '~') {
            log_msg("Not sending game over to IRC: "
                    "dubious characters in username\n");
            return;
        }

    log_msg("Spawning a process to report the game over to IRC");

    /* Don't block on network connections. We can do this in the background
       safely. (clientmain has already stopped us becoming a zombie) */
    if (fork() != 0)
        return; /* we're the parent or this is an error */

    /* protect ourself from SIGHUP */
    signal(SIGHUP, SIG_IGN);

    char errmsg[256];
    int fd = connect_server(settings.irchost, 6667, 1, errmsg, sizeof errmsg);
    if (fd < 0)
        exit(0);

#define SS(x) if (send_c_string(fd, x) < 0) exit(0)

    SS("NICK "); SS(settings.ircnick); SS("\n");
    SS("PASS "); SS(settings.ircpass); SS("\n");
    SS("USER nethack4 nethack4 nethack4 :NetHack 4 Server\n");

    if (!wait_numeric(fd, 376)) {
        close(fd);
        exit(0);
    }

    SS("PRIVMSG "); SS(settings.ircchannel); SS(" :[account ");
    SS(user_info.username); SS("] ");

    char pointscount[30];
    snprintf(pointscount, 30, "%d ", tte->points);
    SS(pointscount);

    SS(tte->entrytxt); SS("\n"); 

    SS("QUIT\n");

    wait_error(fd);
    close(fd);

    exit(0);
}
