// termios.h - Minimal Windows-compatible termios replacement

#ifndef _MINI_TERMIOS_H
#define _MINI_TERMIOS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int speed_t;

// Flag macros (fake values just for compatibility)
#define ICANON  0x0001
#define ECHO    0x0002

struct termios {
    unsigned int c_lflag;  // Only local modes needed for libedit (ICANON, ECHO)
};

// Optional actions (ignored in minimal implementation)
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int optional_actions, const struct termios *t);

#ifdef __cplusplus
}
#endif

#endif // _MINI_TERMIOS_H
