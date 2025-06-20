// ncurses.h - Minimal header to support libedit using VT100 emulation on Windows

#ifndef _MINI_NCURSES_H
#define _MINI_NCURSES_H

#ifdef __cplusplus
extern "C" {
#endif

// Used by termcap API
int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
char *tgoto(const char *cap, int col, int row);
int tputs(const char *str, int affcnt, int (*putc)(int));

// Terminal attribute structure and functions
#include <termios.h>
int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int optional_actions, const struct termios *t);

// Compatibility
#define OK 0
#define ERR -1

#ifdef __cplusplus
}
#endif

#endif // _MINI_NCURSES_H
