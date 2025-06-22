// ncurses.h - Minimal header to support libedit using VT100 emulation on Windows

#ifndef _MINI_NCURSES_H
#define _MINI_NCURSES_H
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int tcenablecolor(HANDLE h);
int isconsole(HANDLE hOut);

// Used by termcap API
int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
char *tgoto(const char *cap, int col, int row);
//not used anymore
//int tputs(const char *str, int affcnt, int (*putc)(int));

// Terminal attribute structure and functions
#include <termios.h>
int tcgetattr(HANDLE fd, struct termios *t);
int tcsetattr(HANDLE fd, int optional_actions, const struct termios *t);

// Compatibility
#define OK 0
#define ERR -1

#ifdef __cplusplus
}
#endif

#endif // _MINI_NCURSES_H
