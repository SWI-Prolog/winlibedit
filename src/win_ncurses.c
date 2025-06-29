// term_win_vt100.c
#define _CRT_SECURE_NO_WARNINGS
#include "ncurses.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

int
tcenablecolor(HANDLE hOut)
{ DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

int
isconsole(HANDLE hOut)
{ CONSOLE_SCREEN_BUFFER_INFO info;
  return !!GetConsoleScreenBufferInfo(hOut, &info);
}

/* tcgetattr / tcsetattr: minimal stubs enabling raw input mode */
int
tcgetattr(HANDLE fd, struct termios *t)
{ memset(t, 0, sizeof(*t));
  t->c_lflag = ICANON | ECHO;
  t->c_iflag = ICRNL | IXON;
  t->c_oflag = OPOST | ONLCR;
  t->c_cflag = CS8;
  t->c_ispeed = B9600;
  t->c_ospeed = B9600;
  return 0;
}

int
tcsetattr(HANDLE h, int optional_actions, const struct termios *t)
{ DWORD mode = 0;
  if (!GetConsoleMode(h, &mode)) return -1;
  if ((t->c_lflag & (ICANON | ECHO)) == 0)
  { mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    mode &= ~ENABLE_LINE_INPUT;
    mode &= ~ENABLE_ECHO_INPUT;
  } else
  { mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
    mode |= ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
  }
  return SetConsoleMode(h, mode) ? 0 : -1;
}

/* tgetent: fake termcap entry */
int
tgetent(char *bp, const char *name)
{ (void)bp; (void)name;
  return 1;  // success
}

/* Only support a couple of common flags/capabilities */
int
tgetflag(const char *id)
{ if (strcmp(id, "am") == 0) return 1; // auto margins
  if (strcmp(id, "bs") == 0) return 1; // backspace
  return 0;
}

/* Numeric capabilities: return -1 if unknown */
int
tgetnum(const char *id)
{ if ( strcmp(id, "co") == 0 )
  { CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.dwSize.X;
  }
  if ( strcmp(id, "li") == 0 )
  { CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.dwSize.Y;
  }
  return -1;
}

/* String capabilities: VT100 sequences */
char *
tgetstr(const char *id, char **area)
{ const char *s = NULL;
  if (strcmp(id, "cl") == 0) s = "\x1b[H\x1b[J";        // clear screen
  else if (strcmp(id, "ce") == 0) s = "\x1b[K";          // clear to end of line
  else if (strcmp(id, "al") == 0) s = "\x1b[L";          // insert line
  else if (strcmp(id, "dl") == 0) s = "\x1b[M";          // delete line
  else if (strcmp(id, "up") == 0) s = "\x1b[A";          // cursor up
  else if (strcmp(id, "do") == 0) s = "\x1b[B";          // cursor down
  else if (strcmp(id, "le") == 0) s = "\x1b[D";          // cursor left
  else if (strcmp(id, "nd") == 0) s = "\x1b[C";          // cursor right
  else if (strcmp(id, "cr") == 0) s = "\r";              // carriage return
  else if (strcmp(id, "ic") == 0) s = "\x1b[@";          // insert character
  else if (strcmp(id, "dc") == 0) s = "\x1b[P";          // delete character
  else if (strcmp(id, "ch") == 0) s = "\x1b[%i%p1%dG";   // Set col
  else if (strcmp(id, "cm") == 0) s = "\x1b[%i%p1%d;%p2%dH";   // Set col&row
  else return NULL;

  return (char*)s;
}

/* tgoto: simple substitute.  This is  not thread-safe, but nor is the
 * system tgoto.  As we'd be editing  in a single terminal anyway this
 * is good enough.
 */
char *
tgoto(const char *cap, int col, int row)
{ static char buf[128];
  char *p = buf;
  int p1 = col, p2 = row;
  int incr = 0;

  const char *c = cap;
  while (*c && (p - buf) < (int)sizeof(buf) - 16)
  { if (*c == '%')
    { c++;
      if (*c == 'i')		// increment parameters by 1
      { p1++; p2++;
	c++;
      } else if (*c == 'p')	// parameter reference
      { c++;
	if (*c == '1')
	{ c++;
	  if ( c[0] == '%' && c[1] == 'd' )
	  { p += sprintf(p, "%d", p1);
	    c += 2;
	  }
	} else if (*c == '2')
	{ c++;
	  if ( c[0] == '%' && c[1] == 'd' )
	  { p += sprintf(p, "%d", p2);
	    c += 2;
	  }
	}
      } else
      { *p++ = '%';
	*p++ = *c++;
      }
    } else
    { *p++ = *c++;
    }
  }

  *p = '\0';
  return buf;
}


#if 0					/* not used anymore */
/* tputs: output count times, with padding char ignored */
int
tputs(const char *s, int affcnt, int (*putc_func)(int))
{ if (!s) return OK;
  while (*s) {
    putc_func((unsigned char)*s++);
  }
  return OK;
}
#endif

speed_t
cfgetispeed(const struct termios *t)
{ return t->c_ispeed;
}

speed_t
cfgetospeed(const struct termios *t)
{ return t->c_ospeed;
}

int
cfsetispeed(struct termios *t, speed_t speed)
{ t->c_ispeed = speed;
  return 0;
}

int
cfsetospeed(struct termios *t, speed_t speed)
{ t->c_ospeed = speed;
  return 0;
}

/* normally defined in <string.h>, but not on Windows
*/

int
ffs(int i)
{ int bit = 1;
  while (i)
  { if (i & 1) return bit;
    i >>= 1;
    ++bit;
  }
  return 0;
}
