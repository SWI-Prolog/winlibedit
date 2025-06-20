// term_win_vt100.c
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

/* tcgetattr / tcsetattr: minimal stubs enabling raw input mode */
int tcgetattr(int fd, struct termios *t) {
    if (fd != STDIN_FILENO) return -1;
    memset(t, 0, sizeof(*t));
    t->c_lflag = ECHO | ICANON;  // echo + canonical input default
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *t) {
    if (fd != STDIN_FILENO) return -1;
    DWORD mode = 0;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(h, &mode)) return -1;
    if ((t->c_lflag & (ICANON | ECHO)) == 0) {
        mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        mode &= ~ENABLE_LINE_INPUT;
        mode &= ~ENABLE_ECHO_INPUT;
    } else {
        mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
        mode |= ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    }
    return SetConsoleMode(h, mode) ? 0 : -1;
}

/* tgetent: fake termcap entry */
int tgetent(char *bp, const char *name) {
    (void)bp; (void)name;
    return 1;  // success
}

/* Only support a couple of common flags/capabilities */
int tgetflag(const char *id) {
    if (strcmp(id, "am") == 0) return 1; // auto margins
    if (strcmp(id, "bs") == 0) return 1; // backspace
    return 0;
}

/* Numeric capabilities: return -1 if unknown */
int tgetnum(const char *id) {
    if (strcmp(id, "co") == 0) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        return csbi.dwSize.X;
    }
    if (strcmp(id, "li") == 0) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        return csbi.dwSize.Y;
    }
    return -1;
}

/* String capabilities: VT100 sequences */
char *tgetstr(const char *id, char **area) {
    const char *s = NULL;
    if (strcmp(id, "cl") == 0) s = "\x1b[H\x1b[J";        // clear screen
    else if (strcmp(id, "ce") == 0) s = "\x1b[K";          // clear to end of line
    else if (strcmp(id, "al") == 0) s = "\x1b[L";          // insert line
    else if (strcmp(id, "dl") == 0) s = "\x1b[M";          // delete line
    else if (strcmp(id, "up") == 0) s = "\x1b[A";          // cursor up
    else if (strcmp(id, "do") == 0) s = "\x1b[B";          // cursor down
    else if (strcmp(id, "le") == 0) s = "\x1b[D";          // cursor left
    else if (strcmp(id, "nd") == 0) s = "\x1b[C";          // cursor right
    else return NULL;

    size_t len = strlen(s) + 1;
    char *out = malloc(len);
    if (!out) return NULL;
    memcpy(out, s, len);
    if (area) *area = out;
    return out;
}

/* tgoto: simple substitute */
char *tgoto(const char *cap, int col, int row) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row + 1, col + 1);
    return strdup(buf);
}

/* tputs: output count times, with padding char ignored */
int tputs(const char *s, int affcnt, int (*putc_func)(int)) {
    if (!s) return OK;
    while (*s) {
        putc_func((unsigned char)*s++);
    }
    return OK;
}
