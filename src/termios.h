// termios.h - Extended minimal Windows-compatible termios replacement

#ifndef _MINI_TERMIOS_H
#define _MINI_TERMIOS_H

#ifdef __cplusplus
extern "C" {
#endif

// Type aliases
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

// Control characters array size (can be adjusted as needed)
#define NCCS 20

// Structure definition
struct termios {
    tcflag_t c_iflag;  // input modes
    tcflag_t c_oflag;  // output modes
    tcflag_t c_cflag;  // control modes
    tcflag_t c_lflag;  // local modes
    cc_t c_cc[NCCS];   // control characters
    speed_t c_ispeed;  // input speed
    speed_t c_ospeed;  // output speed
};

// Local modes
#define ICANON   0x0001
#define ECHO     0x0002
#define ECHOE    0x0004
#define ECHOK    0x0008
#define ECHONL   0x0010
#define ECHOCTL  0x0020
#define ISIG     0x0040
#define IEXTEN   0x0080
#define NOFLSH   0x0100
#define FLUSHO   0x0200
#define EXTPROC  0x0400

// Input modes
#define ICRNL    0x0001
#define INLCR    0x0002
#define IGNCR    0x0004
#define IXON     0x0010
#define IXOFF    0x0020

// Output modes
#define OPOST    0x0001
#define ONLCR    0x0002
#define ONLRET   0x0004
#define TAB3     0x0008

// Control modes
#define CSIZE    0x0030
#define CS8      0x0030

// Speed settings
#define B9600    9600

// Optional action constants
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

// Function declarations
int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int optional_actions, const struct termios *t);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
int cfsetispeed(struct termios *t, speed_t speed);
int cfsetospeed(struct termios *t, speed_t speed);

// Misc
int ffs(int i); // find first set bit

#ifdef __cplusplus
}
#endif

#endif // _MINI_TERMIOS_H
