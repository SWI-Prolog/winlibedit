/*	$NetBSD: terminal.c,v 1.46 2023/02/04 14:34:28 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#if !defined(lint) && !defined(SCCSID)
#if 0
static char sccsid[] = "@(#)term.c	8.2 (Berkeley) 4/30/95";
#else
__RCSID("$NetBSD: terminal.c,v 1.46 2023/02/04 14:34:28 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * terminal.c: Editor/termcap-curses interface
 *	       We have to declare a static variable here, since the
 *	       termcap putchar routine does not take an argument!
 */
#include <sys/types.h>
#ifdef HAVE_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif
#if __WINDOWS__
#include "win_ncurses.h"
#elif HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif

/* Solaris's term.h does horrid things. */
#if defined(HAVE_TERM_H) && !defined(__sun) && !defined(HAVE_TERMCAP_H)
#include <term.h>
#endif

#if defined(__sun)
extern int tgetent(char *, const char *);
extern int tgetflag(char *);
extern int tgetnum(char *);
extern int tputs(const char *, int, int (*)(int));
extern char* tgoto(const char*, int, int);
extern char* tgetstr(char*, char**);
#endif

#ifdef _REENTRANT
#include <pthread.h>
#endif

#include "el.h"
#include "fcns.h"
#include "utf8.h"
#include <stdbool.h>

/*
 * IMPORTANT NOTE: these routines are allowed to look at the current screen
 * and the current position assuming that it is correct.  If this is not
 * true, then the update will be WRONG!  This is (should be) a valid
 * assumption...
 */

#define	TC_BUFSIZE	((size_t)2048)

#define	GoodStr(a)	(el->el_terminal.t_str[a] != NULL && \
			    el->el_terminal.t_str[a][0] != '\0')
#define	Str(a)		el->el_terminal.t_str[a]
#define	Val(a)		el->el_terminal.t_val[a]

static const struct termcapstr {
	const char *name;
	const char *long_name;
} tstr[] = {
#define	T_al	0
	{ "al", "add new blank line" },
#define	T_bl	1
	{ "bl", "audible bell" },
#define	T_cd	2
	{ "cd", "clear to bottom" },
#define	T_ce	3
	{ "ce", "clear to end of line" },
#define	T_ch	4
	{ "ch", "cursor to horiz pos" },
#define	T_cl	5
	{ "cl", "clear screen" },
#define	T_dc	6
	{ "dc", "delete a character" },
#define	T_dl	7
	{ "dl", "delete a line" },
#define	T_dm	8
	{ "dm", "start delete mode" },
#define	T_ed	9
	{ "ed", "end delete mode" },
#define	T_ei	10
	{ "ei", "end insert mode" },
#define	T_fs	11
	{ "fs", "cursor from status line" },
#define	T_ho	12
	{ "ho", "home cursor" },
#define	T_ic	13
	{ "ic", "insert character" },
#define	T_im	14
	{ "im", "start insert mode" },
#define	T_ip	15
	{ "ip", "insert padding" },
#define	T_kd	16
	{ "kd", "sends cursor down" },
#define	T_kl	17
	{ "kl", "sends cursor left" },
#define	T_kr	18
	{ "kr", "sends cursor right" },
#define	T_ku	19
	{ "ku", "sends cursor up" },
#define	T_md	20
	{ "md", "begin bold" },
#define	T_me	21
	{ "me", "end attributes" },
#define	T_nd	22
	{ "nd", "non destructive space" },
#define	T_se	23
	{ "se", "end standout" },
#define	T_so	24
	{ "so", "begin standout" },
#define	T_ts	25
	{ "ts", "cursor to status line" },
#define	T_up	26
	{ "up", "cursor up one" },
#define	T_us	27
	{ "us", "begin underline" },
#define	T_ue	28
	{ "ue", "end underline" },
#define	T_vb	29
	{ "vb", "visible bell" },
#define	T_DC	30
	{ "DC", "delete multiple chars" },
#define	T_DO	31
	{ "DO", "cursor down multiple" },
#define	T_IC	32
	{ "IC", "insert multiple chars" },
#define	T_LE	33
	{ "LE", "cursor left multiple" },
#define	T_RI	34
	{ "RI", "cursor right multiple" },
#define	T_UP	35
	{ "UP", "cursor up multiple" },
#define	T_kh	36
	{ "kh", "send cursor home" },
#define	T_at7	37
	{ "@7", "send cursor end" },
#define	T_kD	38
	{ "kD", "send cursor delete" },
#define	T_str	39
	{ NULL, NULL }
};

static const struct termcapval {
	const char *name;
	const char *long_name;
} tval[] = {
#define	T_am	0
	{ "am", "has automatic margins" },
#define	T_pt	1
	{ "pt", "has physical tabs" },
#define	T_li	2
	{ "li", "Number of lines" },
#define	T_co	3
	{ "co", "Number of columns" },
#define	T_km	4
	{ "km", "Has meta key" },
#define	T_xt	5
	{ "xt", "Tab chars destructive" },
#define	T_xn	6
	{ "xn", "newline ignored at right margin" },
#define	T_MT	7
	{ "MT", "Has meta key" },			/* XXX? */
#define	T_val	8
	{ NULL, NULL, }
};
/* do two or more of the attributes use me */

static void	terminal_setflags(EditLine *);
static int	terminal_rebuffer_display(EditLine *);
static void	terminal_free_display(EditLine *);
static int	terminal_alloc_display(EditLine *);
static void	terminal_alloc(EditLine *, const struct termcapstr *,
    const char *);
static void	terminal_init_arrow(EditLine *);
static void	terminal_reset_arrow(EditLine *);
#if !__WINDOWS__
static int	terminal_putc(int);
#endif
static void	terminal_tputs(EditLine *, const char *, int);

#if !__WINDOWS__
#ifdef _REENTRANT
static pthread_mutex_t terminal_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static FILE *terminal_outfile = NULL;
#endif


/* terminal_setflags():
 *	Set the terminal capability flags
 */
static void
terminal_setflags(EditLine *el)
{
#ifdef __WINDOWS__
	/* The Epilog terminal (xpce terminal_image) implements xterm's
	 * delayed wrap: a character written in the last column leaves the
	 * caret parked at the right margin and the wrap only happens when
	 * the next character arrives.  A real Windows console is written
	 * with WriteConsole() (see el_write_buffer()), which wraps
	 * immediately.  The Windows fake termcap (see win_ncurses.c)
	 * cannot know which of the two is on the other end, so record it
	 * here.
	 *
	 * Only re_cursor_at_width() still needs this.  Everywhere else the
	 * redisplay settles the wrap itself instead of predicting it -- see
	 * terminal_overwrite() -- and does not care which kind of terminal
	 * it is talking to.  re_cursor_at_width() reads a caret the
	 * terminal's own reflow placed, which cannot be settled that way.
	 */
	if (el->el_flags & EPILOG)
		Val(T_xn) = 1;
#endif

	EL_FLAGS = 0;
	if (el->el_tty.t_tabs)
		EL_FLAGS |= (Val(T_pt) && !Val(T_xt)) ? TERM_CAN_TAB : 0;

	EL_FLAGS |= (Val(T_km) || Val(T_MT)) ? TERM_HAS_META : 0;
	EL_FLAGS |= GoodStr(T_ce) ? TERM_CAN_CEOL : 0;
	EL_FLAGS |= (GoodStr(T_dc) || GoodStr(T_DC)) ? TERM_CAN_DELETE : 0;
	EL_FLAGS |= (GoodStr(T_im) || GoodStr(T_ic) || GoodStr(T_IC)) ?
	    TERM_CAN_INSERT : 0;
	EL_FLAGS |= (GoodStr(T_up) || GoodStr(T_UP)) ? TERM_CAN_UP : 0;
	EL_FLAGS |= Val(T_am) ? TERM_HAS_AUTO_MARGINS : 0;
	EL_FLAGS |= Val(T_xn) ? TERM_HAS_MAGIC_MARGINS : 0;

	if (GoodStr(T_me) && GoodStr(T_ue))
		EL_FLAGS |= (strcmp(Str(T_me), Str(T_ue)) == 0) ?
		    TERM_CAN_ME : 0;
	else
		EL_FLAGS &= ~TERM_CAN_ME;
	if (GoodStr(T_me) && GoodStr(T_se))
		EL_FLAGS |= (strcmp(Str(T_me), Str(T_se)) == 0) ?
		    TERM_CAN_ME : 0;


#ifdef DEBUG_SCREEN
	if (!EL_CAN_UP) {
		(void) el_printf(el, EL_PTR_ERR,
		    "WARNING: Your terminal cannot move up.\n");
		(void) el_printf(el, EL_PTR_ERR,
		    "Editing may be odd for long lines.\n");
	}
	if (!EL_CAN_CEOL)
		(void) el_printf(el, EL_PTR_ERR, "no clear EOL capability.\n");
	if (!EL_CAN_DELETE)
		(void) el_printf(el, EL_PTR_ERR, "no delete char capability.\n");
	if (!EL_CAN_INSERT)
		(void) el_printf(el, EL_PTR_ERR, "no insert char capability.\n");
#endif /* DEBUG_SCREEN */
}

/* terminal_init():
 *	Initialize the terminal stuff
 */
libedit_private int
terminal_init(EditLine *el)
{
#if __WINDOWS__
	tcenablecolor(el->el_hOut);
#endif
	el->el_terminal.t_buf = el_calloc(TC_BUFSIZE,
	    sizeof(*el->el_terminal.t_buf));
	if (el->el_terminal.t_buf == NULL)
		return -1;
	el->el_terminal.t_cap = el_calloc(TC_BUFSIZE,
	    sizeof(*el->el_terminal.t_cap));
	if (el->el_terminal.t_cap == NULL)
		goto out;
	el->el_terminal.t_fkey = el_calloc(A_K_NKEYS,
	    sizeof(*el->el_terminal.t_fkey));
	if (el->el_terminal.t_fkey == NULL)
		goto out;
	el->el_terminal.t_loc = 0;
	el->el_terminal.t_str = el_calloc(T_str,
	    sizeof(*el->el_terminal.t_str));
	if (el->el_terminal.t_str == NULL)
		goto out;
	el->el_terminal.t_val = el_calloc(T_val,
	    sizeof(*el->el_terminal.t_val));
	if (el->el_terminal.t_val == NULL)
		goto out;
	(void) terminal_set(el, NULL);
	terminal_init_arrow(el);
	return 0;
out:
	terminal_end(el);
	return -1;
}

/* terminal_end():
 *	Clean up the terminal stuff
 */
libedit_private void
terminal_end(EditLine *el)
{

	el_free(el->el_terminal.t_buf);
	el->el_terminal.t_buf = NULL;
	el_free(el->el_terminal.t_cap);
	el->el_terminal.t_cap = NULL;
	el->el_terminal.t_loc = 0;
	el_free(el->el_terminal.t_str);
	el->el_terminal.t_str = NULL;
	el_free(el->el_terminal.t_val);
	el->el_terminal.t_val = NULL;
	el_free(el->el_terminal.t_fkey);
	el->el_terminal.t_fkey = NULL;
	terminal_free_display(el);
}


/* terminal_alloc():
 *	Maintain a string pool for termcap strings
 */
static void
terminal_alloc(EditLine *el, const struct termcapstr *t, const char *cap)
{
	char termbuf[TC_BUFSIZE];
	size_t tlen, clen;
	char **tlist = el->el_terminal.t_str;
	char **tmp, **str = &tlist[t - tstr];

	(void) memset(termbuf, 0, sizeof(termbuf));
	if (cap == NULL || *cap == '\0') {
		*str = NULL;
		return;
	} else
		clen = strlen(cap);

	tlen = *str == NULL ? 0 : strlen(*str);

	/*
         * New string is shorter; no need to allocate space
         */
	if (clen <= tlen) {
		if (*str)
			(void) strcpy(*str, cap);	/* XXX strcpy is safe */
		return;
	}
	/*
         * New string is longer; see if we have enough space to append
         */
	if (el->el_terminal.t_loc + 3 < TC_BUFSIZE) {
						/* XXX strcpy is safe */
		(void) strcpy(*str = &el->el_terminal.t_buf[
		    el->el_terminal.t_loc], cap);
		el->el_terminal.t_loc += clen + 1;	/* one for \0 */
		return;
	}
	/*
         * Compact our buffer; no need to check compaction, cause we know it
         * fits...
         */
	tlen = 0;
	for (tmp = tlist; tmp < &tlist[T_str]; tmp++)
		if (*tmp != NULL && **tmp != '\0' && *tmp != *str) {
			char *ptr;

			for (ptr = *tmp; *ptr != '\0'; termbuf[tlen++] = *ptr++)
				continue;
			termbuf[tlen++] = '\0';
		}
	memcpy(el->el_terminal.t_buf, termbuf, TC_BUFSIZE);
	el->el_terminal.t_loc = tlen;
	if (el->el_terminal.t_loc + 3 >= TC_BUFSIZE) {
		(void) el_printf(el, EL_PTR_ERR,
		    "Out of termcap string space.\n");
		return;
	}
					/* XXX strcpy is safe */
	(void) strcpy(*str = &el->el_terminal.t_buf[el->el_terminal.t_loc],
	    cap);
	el->el_terminal.t_loc += (size_t)clen + 1;	/* one for \0 */
	return;
}


/* terminal_rebuffer_display():
 *	Rebuffer the display after the screen changed size
 */
static int
terminal_rebuffer_display(EditLine *el)
{
	coord_t *c = &el->el_terminal.t_size;

	terminal_free_display(el);

	c->h = Val(T_co);
	c->v = Val(T_li);

	if (terminal_alloc_display(el) == -1)
		return -1;
	return 0;
}

static wint_t **
terminal_alloc_buffer(EditLine *el)
{
	wint_t **b;
	coord_t *c = &el->el_terminal.t_size;
	int i;

	b =  el_calloc((size_t)(c->v + 1), sizeof(*b));
	if (b == NULL)
		return NULL;
	for (i = 0; i < c->v; i++) {
		/* Allocate EL_BUFSIZ+1 per row rather than c->h+1: NFD Unicode
		 * text has more code points than visual columns (each combining
		 * mark occupies its own code-point slot), so the display arrays
		 * must be wide enough to hold all code points for one visual
		 * line even when that exceeds the terminal column width. */
		b[i] = el_calloc(EL_BUFSIZ + 1, sizeof(**b));
		if (b[i] == NULL) {
			while (--i >= 0)
				el_free(b[i]);
			el_free(b);
			return NULL;
		}
	}
	b[c->v] = NULL;
	return b;
}

static void
terminal_free_buffer(wint_t ***bp)
{
	wint_t **b;
	wint_t **bufp;

	if (*bp == NULL)
		return;

	b = *bp;
	*bp = NULL;

	for (bufp = b; *bufp != NULL; bufp++)
		el_free(*bufp);
	el_free(b);
}

/* terminal_alloc_display():
 *	Allocate a new display.
 */
static int
terminal_alloc_display(EditLine *el)
{
	el->el_display = terminal_alloc_buffer(el);
	if (el->el_display == NULL)
		goto done;
	el->el_vdisplay = terminal_alloc_buffer(el);
	if (el->el_vdisplay == NULL)
		goto done;
	return 0;
done:
	terminal_free_display(el);
	return -1;
}


/* terminal_free_display():
 *	Free the display buffers
 */
static void
terminal_free_display(EditLine *el)
{
	terminal_free_buffer(&el->el_display);
	terminal_free_buffer(&el->el_vdisplay);
}


/* terminal_move_to_line():
 *	move to line <where> (first line == 0)
 *	as efficiently as possible
 */
libedit_private void
terminal_move_to_line(EditLine *el, int where)
{
	int del;

	rtlog("terminal_move_to_line(where=%d) el_cursor=(v=%d h=%d) "
	      "t_size=(v=%d h=%d)\n",
	      where, el->el_cursor.v, el->el_cursor.h,
	      el->el_terminal.t_size.v, el->el_terminal.t_size.h);

	if (where == el->el_cursor.v)
		return;

	if (where >= el->el_terminal.t_size.v) {
#ifdef DEBUG_SCREEN
		(void) el_printf(el, EL_PTR_ERR,
		    "%s: where is ridiculous: %d\r\n", __func__, where);
#endif /* DEBUG_SCREEN */
		return;
	}
	if ((del = where - el->el_cursor.v) > 0) {
		/*
		 * We don't use DO here because some terminals are buggy
		 * if the destination is beyond bottom of the screen.
		 */
		for (; del > 0; del--)
			terminal__putc(el, '\n');
		/* because the \n will become \r\n */
		el->el_cursor.h = 0;
	} else {		/* del < 0 */
		if (GoodStr(T_UP) && (-del > 1 || !GoodStr(T_up)))
			terminal_tputs(el, tgoto(Str(T_UP), -del, -del), -del);
		else {
			if (GoodStr(T_up))
				for (; del < 0; del++)
					terminal_tputs(el, Str(T_up), 1);
		}
	}
	el->el_cursor.v = where;/* now where is here */
}


/* terminal_move_to_char():
 *	Move to the character position specified
 */
libedit_private void
terminal_move_to_char(EditLine *el, int where)
{
	int del, i;

	rtlog("terminal_move_to_char(where=%d) entry el_cursor=(v=%d h=%d)\n",
	      where, el->el_cursor.v, el->el_cursor.h);

mc_again:
	if (where == el->el_cursor.h) {
		rtlog("  no-op (already there)\n");
		return;
	}

	if (where > el->el_terminal.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) el_printf(el, EL_PTR_ERR,
		    "%s: where is ridiculous: %d\r\n", __func__, where);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (!where) {		/* if where is first column */
		terminal__putc(el, '\r');	/* do a CR */
		el->el_cursor.h = 0;
		return;
	}
	del = where - el->el_cursor.h;

	if ((del < -4 || del > 4) && GoodStr(T_ch))
		/* go there directly */
		terminal_tputs(el, tgoto(Str(T_ch), where, where), where);
	else {
		if (del > 0) {	/* moving forward */
			if ((del > 4) && GoodStr(T_RI))
				terminal_tputs(el, tgoto(Str(T_RI), del, del),
				    del);
			else {
					/* if I can do tabs, use them */
				if (EL_CAN_TAB) {
					if ((el->el_cursor.h & 0370) !=
					    (where & ~0x7)
					    && (el->el_display[
					    el->el_cursor.v][where & 0370] !=
					    MB_FILL_CHAR)
					    ) {
						/* if not within tab stop */
						for (i =
						    (el->el_cursor.h & 0370);
						    i < (where & ~0x7);
						    i += 8)
							terminal__putc(el,
							    '\t');
							/* then tab over */
						el->el_cursor.h = where & ~0x7;
					}
				}
				/*
				 * it's usually cheaper to just write the
				 * chars, so we do.
				 *
				 * el_cursor.h is a visual-column count, but
				 * el_display[] is indexed by code-point slot.
				 * With NFD Unicode these diverge: a combining
				 * mark occupies a slot but advances no columns.
				 * First convert the current visual column back
				 * to a code-point index, then walk grapheme
				 * clusters (base char + combining marks) to
				 * reach the target visual column `where'.
				 */
				{
					const wint_t *line =
					    el->el_display[el->el_cursor.v];
					int vis = 0;
					int idx = 0;
					int w;

					/* Convert el_cursor.h (visual col) to
					 * a code-point index by scanning from
					 * the start of the line.
					 * Bound by EL_BUFSIZ (not t_size.h):
					 * NFD combining marks give lines with
					 * more code-point slots than columns.
					 * MB_FILL_CHAR is the right-half
					 * placeholder for wide chars: skip it
					 * in the cluster-tail loop so it does
					 * not erroneously consume a vis column
					 * (wcwidth returns -1 for it). */
					while (vis < el->el_cursor.h &&
					    idx < (int)EL_BUFSIZ &&
					    line[idx] != L'\0') {
						if ((wint_t)line[idx] ==
						    MB_FILL_CHAR) {
							idx++; continue;
						}
						w = ct_cell_vcols(el, line[idx]);
						if (w < 0) w = 1;
						vis += w;
						idx++;
						/* skip everything that paints no
						 * column of its own: combining
						 * marks, the MB_FILL_CHAR of a
						 * wide base char and a literal
						 * holding only an escape */
						if (w > 0) {
							while (idx < (int)EL_BUFSIZ
							    && line[idx] != L'\0' &&
							    ct_cell_vcols(el,
							    line[idx]) == 0)
								idx++;
						}
					}

					/* Write grapheme clusters from
					 * (vis,idx) forward until we reach
					 * visual column `where'. */
					while (vis < where &&
					    idx < (int)EL_BUFSIZ &&
					    line[idx] != L'\0') {
						if ((wint_t)line[idx] ==
						    MB_FILL_CHAR) {
							idx++; continue;
						}
						w = ct_cell_vcols(el, line[idx]);
						/* A literal paints no column of
						 * its own but must still be sent:
						 * it carries the escape sequence
						 * that colours what follows. */
						if (w <= 0 &&
						    !EL_IS_LITERAL(line[idx])) {
							idx++; continue;
						}
						terminal__putc(el, line[idx++]);
						if (w > 0)
							vis += w;
						/* write combining marks and
						 * literals; terminal__putc()
						 * ignores MB_FILL_CHAR */
						while (idx < (int)EL_BUFSIZ &&
						    line[idx] != L'\0' &&
						    ct_cell_vcols(el,
						    line[idx]) == 0)
							terminal__putc(el,
							    line[idx++]);
					}
					/* el_cursor.h corrected to `where'
					 * below */
				}

			}
		} else {	/* del < 0 := moving backward */
			if ((-del > 4) && GoodStr(T_LE))
				terminal_tputs(el, tgoto(Str(T_LE), -del, -del),
				    -del);
			else {	/* can't go directly there */
				/*
				 * if the "cost" is greater than the "cost"
				 * from col 0
				 */
				if (EL_CAN_TAB ?
				    ((unsigned int)-del >
				    (((unsigned int) where >> 3) +
				     (where & 07)))
				    : (-del > where)) {
					terminal__putc(el, '\r');/* do a CR */
					el->el_cursor.h = 0;
					goto mc_again;	/* and try again */
				}
				rtlog("  emitting %d \\b's\n", -del);
				for (i = 0; i < -del; i++)
					terminal__putc(el, '\b');
			}
		}
	}
	el->el_cursor.h = where;		/* now where is here */
	rtlog("terminal_move_to_char exit el_cursor=(v=%d h=%d)\n",
	      el->el_cursor.v, el->el_cursor.h);
}


/* terminal_overwrite():
 *	Overstrike num characters
 *	Assumes MB_FILL_CHARs are present to keep the column count correct
 */
libedit_private void
terminal_overwrite(EditLine *el, const wchar_t *cp, size_t n)
{
	if (n == 0)
		return;

	/* n is a code-point count, not a visual column count.  For NFD Unicode
	 * text, combining marks add extra code-point slots so n can legitimately
	 * exceed t_size.h while still fitting in one visual line.  Bound by
	 * EL_BUFSIZ (the allocated row size) instead. */
	if (n > EL_BUFSIZ) {
#ifdef DEBUG_SCREEN
		(void) el_printf(el, EL_PTR_ERR,
		    "%s: n is ridiculous: %zu\r\n", __func__, n);
#endif /* DEBUG_SCREEN */
		return;
	}

        do {
                /* terminal__putc() ignores any MB_FILL_CHARs */
                wchar_t _c = *cp++;
                int _w;
                /* MB_FILL_CHAR is the right-half placeholder for a wide
                 * char.  terminal__putc() emits nothing for it, and the
                 * preceding base char already accounted for the second
                 * column — so don't wcwidth() it (which currently routes
                 * through libedit_wcwidth → mk_wcwidth and returns 1
                 * for the (wint_t)-1 sentinel) and don't advance the
                 * cursor for it. */
                if ((wint_t)_c == MB_FILL_CHAR)
                        continue;
                /* A prompt literal holds an invisible escape sequence plus
                 * (maybe) one visible character; its width was recorded
                 * when it was created. */
                if (EL_IS_LITERAL(_c)) {
                        terminal__putc(el, _c);
                        el->el_cursor.h += literal_width(el, (wint_t)_c);
                        continue;
                }
#if SIZEOF_WCHAR_T == 2
                /* On Windows a non-BMP code point is split across two
                 * wchar_t slots (UTF-16 surrogate pair).  terminal__putc()
                 * buffers the lead and only emits bytes when the trail
                 * arrives, so the cursor must advance by the WCWIDTH OF
                 * THE DECODED CODE POINT, applied AFTER the trail — not
                 * by wcwidth() of the lead (whose return is undefined).
                 *
                 * If the lead is the last slot in cp[0..n) we are mid-
                 * cluster: emit it (buffered) but DO NOT advance the
                 * cursor — no glyph has been painted.  re_overwrite_nc()
                 * extends the call through any following trail surrogate
                 * to keep the pair atomic. */
                if (IS_UTF16_LEAD(_c) && n > 1 && IS_UTF16_TRAIL(*cp)) {
                        wchar_t _t = *cp++;
                        terminal__putc(el, _c);
                        terminal__putc(el, _t);
                        n--;
                        _w = wcwidth((uchar_t)utf16_decode(_c, _t));
                        if (_w > 0)
                                el->el_cursor.h += _w;
                        continue;
                }
                if (IS_UTF16_LEAD(_c)) {
                        terminal__putc(el, _c); /* buffered, no glyph yet */
                        continue;
                }
#endif
                _w = wcwidth(_c);
                terminal__putc(el, _c);
                /* Combining marks (w==0) do not advance the visual
                 * cursor; double-wide chars advance by 2. */
                if (_w > 0)
                        el->el_cursor.h += _w;
        } while (--n);

	if (el->el_cursor.h >= el->el_terminal.t_size.h) {	/* wrap? */
		if (EL_HAS_AUTO_MARGINS) {	/* yes */
			el->el_cursor.h = 0;
			if (el->el_cursor.v + 1 < el->el_terminal.t_size.v)
				el->el_cursor.v++;
			/* Settle the wrap by writing a space and then
			 * backspacing, whatever the terminal description says
			 * about xn.  Both kinds of auto-margin terminal end up
			 * in the same place:
			 *
			 *   magic margins -- the character that filled the row
			 *   left the wrap pending, so the space performs it and
			 *   lands at (v+1, 0), the backspace returns to (v+1, 0)
			 *
			 *   immediate wrap -- the terminal already moved to
			 *   (v+1, 0), so the space lands there too and the
			 *   backspace returns to (v+1, 0)
			 *
			 * so the physical cursor matches tracking at (v+1, 0)
			 * either way, and libedit need not trust xn to be
			 * right.  It frequently is not: a terminal description
			 * describes the terminal it was written for, not the
			 * one on the other end of the line.  Assuming the wrap
			 * had already happened when it had not left every
			 * following cursor motion -- which cancels a pending
			 * wrap -- acting one row too high, and the screen
			 * filled up from the bottom.
			 *
			 * The space costs the first cell of the new row, which
			 * is about to be written over: the caller continues
			 * there, and re_update_line() starts a row at column 0.
			 *
			 * The older approach of writing el_display[v+1][0]
			 * back briefly drops the following combining mark
			 * from the cell (we only write one code point),
			 * and leaves tracking.h at 1 which is out of sync
			 * with the subsequent \r in re_update_line. */
			terminal__putc(el, ' ');
			terminal__putc(el, '\b');
		} else		/* no wrap, but cursor stays on screen */
			el->el_cursor.h = el->el_terminal.t_size.h - 1;
	}
}


/* terminal_deletechars():
 *	Delete num characters
 */
libedit_private void
terminal_deletechars(EditLine *el, int num)
{
	rtlog("terminal_deletechars(num=%d) entry el_cursor=(v=%d h=%d)\n",
	      num, el->el_cursor.v, el->el_cursor.h);
	if (num <= 0)
		return;

	if (!EL_CAN_DELETE) {
#ifdef DEBUG_EDIT
		(void) el_printf(el, EL_PTR_ERR, "   ERROR: cannot delete   \n");
#endif /* DEBUG_EDIT */
		return;
	}
	if (num > el->el_terminal.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) el_printf(el, EL_PTR_ERR,
		    "%s: num is ridiculous: %d\r\n", __func__, num);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (GoodStr(T_DC))	/* if I have multiple delete */
		if ((num > 1) || !GoodStr(T_dc)) {	/* if dc would be more
							 * expen. */
			terminal_tputs(el, tgoto(Str(T_DC), num, num), num);
			return;
		}
	if (GoodStr(T_dm))	/* if I have delete mode */
		terminal_tputs(el, Str(T_dm), 1);

	if (GoodStr(T_dc))	/* else do one at a time */
		while (num--)
			terminal_tputs(el, Str(T_dc), 1);

	if (GoodStr(T_ed))	/* if I have delete mode */
		terminal_tputs(el, Str(T_ed), 1);
}


/* terminal_insertwrite():
 *	Puts terminal in insert character mode or inserts num
 *	characters in the line
 *      Assumes MB_FILL_CHARs are present to keep column count correct
 */
libedit_private void
terminal_insertwrite(EditLine *el, wchar_t *cp, int num)
{
	rtlog("terminal_insertwrite(num=%d) entry el_cursor=(v=%d h=%d) "
	      "IC=%d ic=%d im=%d\n", num, el->el_cursor.v, el->el_cursor.h,
	      GoodStr(T_IC) ? 1 : 0, GoodStr(T_ic) ? 1 : 0,
	      (GoodStr(T_im) && GoodStr(T_ei)) ? 1 : 0);
	if (num <= 0)
		return;
	if (!EL_CAN_INSERT) {
#ifdef DEBUG_EDIT
		(void) el_printf(el, EL_PTR_ERR, "   ERROR: cannot insert   \n");
#endif /* DEBUG_EDIT */
		return;
	}
	/* num is a code-point count; see terminal_overwrite() for why this
	 * must be compared against EL_BUFSIZ rather than t_size.h. */
	if (num > (int)EL_BUFSIZ) {
#ifdef DEBUG_SCREEN
		(void) el_printf(el, EL_PTR_ERR,
		    "%s: num is ridiculous: %d\r\n", __func__, num);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (GoodStr(T_IC))	/* if I have multiple insert */
		if ((num > 1) || !GoodStr(T_ic)) {
				/* if ic would be more expensive */
			/* T_IC takes a VISUAL column count (how many blank
			 * cells to insert on the terminal line), while num is
			 * the CODE-POINT count we want to write.  For NFD
			 * text these diverge: 'a'+U+0300 is 2 code points but
			 * only 1 visual column.  Without this distinction
			 * every combining mark in an insert pushes one extra
			 * cell to the right, so repeated inserts accumulate
			 * phantom gaps. */
			const wchar_t *p = cp, *end = cp + num;
			int num_vcols = 0;

			while (p < end) {
				int adv;
				num_vcols += ct_cp_vcols(el, p, end, &adv);
				p += adv;
			}
			rtlog("  insertwrite: IC path, num_vcols=%d\n",
			      num_vcols);
			if (num_vcols > 0)
				terminal_tputs(el,
				    tgoto(Str(T_IC), num_vcols, num_vcols),
				    num_vcols);
			terminal_overwrite(el, cp, (size_t)num);
				/* this updates el_cursor.h */
			return;
		}
	if (GoodStr(T_im) && GoodStr(T_ei)) {	/* if I have insert mode */
		const wchar_t *end = cp + num;

		rtlog("  insertwrite: insert-mode path\n");
		terminal_tputs(el, Str(T_im), 1);

		while (cp < end) {
			int adv, i;
			int _w = ct_cp_vcols(el, cp, end, &adv);

			for (i = 0; i < adv; i++)
				terminal__putc(el, cp[i]);
			cp += adv;
			el->el_cursor.h += _w;
		}

		if (GoodStr(T_ip))	/* have to make num chars insert */
			terminal_tputs(el, Str(T_ip), 1);

		terminal_tputs(el, Str(T_ei), 1);
		return;
	}
	{
		const wchar_t *end = cp + num;

		while (cp < end) {
			int adv, i;
			int _w = ct_cp_vcols(el, cp, end, &adv);

			/* T_ic opens ONE cell.  Open exactly as many as the
			 * code point will paint: two for a wide char, none
			 * for a combining mark (it joins the cluster in the
			 * cell before it) or a MB_FILL_CHAR. */
			rtlog("  insertwrite: per-char ic path, w=%d\n", _w);
			if (GoodStr(T_ic))
				for (i = 0; i < _w; i++)
					terminal_tputs(el, Str(T_ic), 1);

			for (i = 0; i < adv; i++)
				terminal__putc(el, cp[i]);
			cp += adv;
			el->el_cursor.h += _w;

			if (GoodStr(T_ip))	/* have to make num chars insert */
				terminal_tputs(el, Str(T_ip), 1);
					/* pad the inserted char */
		}
	}
}


/* terminal_clear_EOL():
 *	clear to end of line.  There are num characters to clear
 */
libedit_private void
terminal_clear_EOL(EditLine *el, int num)
{
	int i;

	if (num <= 0)		/* nothing to erase */
		return;
	if (EL_CAN_CEOL && GoodStr(T_ce))
		terminal_tputs(el, Str(T_ce), 1);
	else {
		for (i = 0; i < num; i++)
			terminal__putc(el, ' ');
		el->el_cursor.h += num;	/* have written num spaces */
	}
}


/* terminal_clear_screen():
 *	Clear the screen
 */
libedit_private void
terminal_clear_screen(EditLine *el)
{				/* clear the whole screen and home */

	if (GoodStr(T_cl))
		/* send the clear screen code */
		terminal_tputs(el, Str(T_cl), Val(T_li));
	else if (GoodStr(T_ho) && GoodStr(T_cd)) {
		terminal_tputs(el, Str(T_ho), Val(T_li));	/* home */
		/* clear to bottom of screen */
		terminal_tputs(el, Str(T_cd), Val(T_li));
	} else {
		terminal__putc(el, '\r');
		terminal__putc(el, '\n');
	}
}


/* terminal_beep():
 *	Beep the way the terminal wants us
 */
libedit_private void
terminal_beep(EditLine *el)
{
	if (GoodStr(T_bl))
		/* what termcap says we should use */
		terminal_tputs(el, Str(T_bl), 1);
	else
		terminal__putc(el, '\007');	/* an ASCII bell; ^G */
}


libedit_private void
terminal_get(EditLine *el, const char **term)
{
	*term = el->el_terminal.t_name;
}


/* terminal_set():
 *	Read in the terminal capabilities from the requested terminal
 */
libedit_private int
terminal_set(EditLine *el, const char *term)
{
	int i;
	char buf[TC_BUFSIZE];
	char *area;
	const struct termcapstr *t;
	sigset_t oset, nset;
	int lins, cols;

	(void) sigemptyset(&nset);
	(void) sigaddset(&nset, SIGWINCH);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	area = buf;


	if (term == NULL)
		term = getenv("TERM");

	if (!term || !term[0])
		term = "dumb";

	if (strcmp(term, "emacs") == 0)
		el->el_flags |= EDIT_DISABLED;

	(void) memset(el->el_terminal.t_cap, 0, TC_BUFSIZE);

	i = tgetent(el->el_terminal.t_cap, term);

	if (i <= 0) {
		if (i == -1)
			(void) el_printf(el, EL_PTR_ERR,
			    "Cannot read termcap database;\n");
		else if (i == 0)
			(void) el_printf(el, EL_PTR_ERR,
			    "No entry for terminal type \"%s\";\n", term);
		(void) el_printf(el, EL_PTR_ERR,
		    "using dumb terminal settings.\n");
		Val(T_co) = 80;	/* do a dumb terminal */
		Val(T_pt) = Val(T_km) = Val(T_li) = 0;
		Val(T_xt) = Val(T_MT);
		for (t = tstr; t->name != NULL; t++)
			terminal_alloc(el, t, NULL);
	} else {
		/* auto/magic margins */
		Val(T_am) = tgetflag("am");
		Val(T_xn) = tgetflag("xn");
		/* Can we tab */
		Val(T_pt) = tgetflag("pt");
		Val(T_xt) = tgetflag("xt");
		/* do we have a meta? */
		Val(T_km) = tgetflag("km");
		Val(T_MT) = tgetflag("MT");
		/* Get the size */
		Val(T_co) = tgetnum("co");
		Val(T_li) = tgetnum("li");
		for (t = tstr; t->name != NULL; t++) {
			/* XXX: some systems' tgetstr needs non const */
			terminal_alloc(el, t, tgetstr(strchr(t->name, *t->name),
			    &area));
		}
	}

	if (Val(T_co) < 2)
		Val(T_co) = 80;	/* just in case */
	if (Val(T_li) < 1)
		Val(T_li) = 24;

	el->el_terminal.t_size.v = Val(T_co);
	el->el_terminal.t_size.h = Val(T_li);

	terminal_setflags(el);

				/* get the correct window size */
	(void) terminal_get_size(el, &lins, &cols);
	if (terminal_change_size(el, lins, cols) == -1)
		return -1;
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	terminal_bind_arrow(el);
	el->el_terminal.t_name = term;
	return i <= 0 ? -1 : 0;
}

/* el_terminal_setfn()
   el_terminal_getfn()
Set/Get the function to retrieve the terminal screen size.
*/

libedit_private int
el_terminal_setfn(el_terminal_t *el_terminal, el_szfunc_t rc)
{
	el_terminal->t_getsize = rc;
	return 0;
}

libedit_private el_szfunc_t
el_terminal_getfn(el_terminal_t *el_terminal)
{
	return el_terminal->t_getsize;
}


/* terminal_get_size():
 *	Return the new window size in lines and cols, and
 *	true if the size was changed.
 */
libedit_private int
terminal_get_size(EditLine *el, int *lins, int *cols)
{

	*cols = Val(T_co);
	*lins = Val(T_li);

	if ( el->el_terminal.t_getsize &&
	     (*el->el_terminal.t_getsize)(el, cols, lins) == 0 ) {
		return Val(T_co) != *cols || Val(T_li) != *lins;
	}

#ifdef __WINDOWS__
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if ( GetConsoleScreenBufferInfo(el->el_hOut, &csbi) )
	{ /* The window, not the scrollback buffer: see tgetnum() in
	   * win_ncurses.c. */
	  *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	  *lins = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	}
#else/*__WINDOWS__*/
#ifdef TIOCGWINSZ
	{
		struct winsize ws;
		if (ioctl(el->el_infd, TIOCGWINSZ, &ws) != -1) {
			if (ws.ws_col)
				*cols = ws.ws_col;
			if (ws.ws_row)
				*lins = ws.ws_row;
		}
	}
#endif
#ifdef TIOCGSIZE
	{
		struct ttysize ts;
		if (ioctl(el->el_infd, TIOCGSIZE, &ts) != -1) {
			if (ts.ts_cols)
				*cols = ts.ts_cols;
			if (ts.ts_lines)
				*lins = ts.ts_lines;
		}
	}
#endif
#endif/*__WINDOWS__*/
	return Val(T_co) != *cols || Val(T_li) != *lins;
}


/* terminal_change_size():
 *	Change the size of the terminal
 */
libedit_private int
terminal_change_size(EditLine *el, int lins, int cols)
{
	/* Rewind the physical cursor to the start of the current input
	 * line and wipe every row of the pre-resize wrap before we reset
	 * libedit's internal display model to (0,0).
	 *
	 * A modern reflowing terminal (xterm w/ rewrap, terminator, gnome-
	 * terminal, xpce's rlc, ...) has already re-wrapped the prompt +
	 * input to the new width by the time SIGWINCH reaches us, so the
	 * physical cursor is at a row/col determined by the NEW column
	 * count — not the el_cursor.v tracked from the pre-resize layout.
	 * Walk the input buffer from the prompt with the new `cols` to
	 * compute where the cursor actually is (its row offset below the
	 * prompt row) and move up by that amount.  For a non-reflowing
	 * terminal the computation still gives a sensible value as long
	 * as the terminal preserves "cursor at end of input" across
	 * resizes (which well-behaved terminals do even without reflow).
	 *
	 * Skipped when both the pre-resize display had no multi-row
	 * activity (r_oldcv == 0) AND the reflowed cursor stays on the
	 * prompt row (new_v == 0): no input spans more than one row, so
	 * re_refresh's CR is enough.  On initial terminal_init the input
	 * buffer is empty and new_v == 0 — we must not touch the screen,
	 * which may hold unrelated banner content. */
	{
		int new_h, new_v;

		re_cursor_at_width(el, cols, &new_h, &new_v);

		rtlog("=== terminal_change_size(lins=%d cols=%d) "
		      "was (li=%d co=%d) r_oldcv=%d new=(v=%d h=%d) "
		      "el_cursor=(v=%d h=%d)\n",
		      lins, cols, Val(T_li), Val(T_co),
		      el->el_refresh.r_oldcv, new_v, new_h,
		      el->el_cursor.v, el->el_cursor.h);

		if (el->el_refresh.r_oldcv > 0 || new_v > 0) {
			if (new_v > 0) {
				if (GoodStr(T_UP) &&
				    (new_v > 1 || !GoodStr(T_up)))
					terminal_tputs(el,
					    tgoto(Str(T_UP), new_v, new_v),
					    new_v);
				else if (GoodStr(T_up))
					for (int i = 0; i < new_v; i++)
						terminal_tputs(el,
						    Str(T_up), 1);
			}
			terminal__putc(el, '\r');
			if (GoodStr(T_cd))
				terminal_tputs(el, Str(T_cd), Val(T_li));
			else
				re_clear_lines(el);
		}
	}

	/*
	 * Just in case
	 */
	Val(T_co) = (cols < 2) ? 80 : cols;
	Val(T_li) = (lins < 1) ? 24 : lins;

	/* re-make display buffers */
	if (terminal_rebuffer_display(el) == -1)
		return -1;
	re_clear_display(el);
	return 0;
}


/* terminal_init_arrow():
 *	Initialize the arrow key bindings from termcap
 */
static void
terminal_init_arrow(EditLine *el)
{
	funckey_t *arrow = el->el_terminal.t_fkey;

	arrow[A_K_DN].name = L"down";
	arrow[A_K_DN].key = T_kd;
	arrow[A_K_DN].fun.cmd = ED_NEXT_HISTORY;
	arrow[A_K_DN].type = XK_CMD;

	arrow[A_K_UP].name = L"up";
	arrow[A_K_UP].key = T_ku;
	arrow[A_K_UP].fun.cmd = ED_PREV_HISTORY;
	arrow[A_K_UP].type = XK_CMD;

	arrow[A_K_LT].name = L"left";
	arrow[A_K_LT].key = T_kl;
	arrow[A_K_LT].fun.cmd = ED_PREV_CHAR;
	arrow[A_K_LT].type = XK_CMD;

	arrow[A_K_RT].name = L"right";
	arrow[A_K_RT].key = T_kr;
	arrow[A_K_RT].fun.cmd = ED_NEXT_CHAR;
	arrow[A_K_RT].type = XK_CMD;

	arrow[A_K_HO].name = L"home";
	arrow[A_K_HO].key = T_kh;
	arrow[A_K_HO].fun.cmd = ED_MOVE_TO_BEG;
	arrow[A_K_HO].type = XK_CMD;

	arrow[A_K_EN].name = L"end";
	arrow[A_K_EN].key = T_at7;
	arrow[A_K_EN].fun.cmd = ED_MOVE_TO_END;
	arrow[A_K_EN].type = XK_CMD;

	arrow[A_K_DE].name = L"delete";
	arrow[A_K_DE].key = T_kD;
	arrow[A_K_DE].fun.cmd = ED_DELETE_NEXT_CHAR;
	arrow[A_K_DE].type = XK_CMD;
}


/* terminal_reset_arrow():
 *	Reset arrow key bindings
 */
static void
terminal_reset_arrow(EditLine *el)
{
	funckey_t *arrow = el->el_terminal.t_fkey;
	static const wchar_t strA[] = L"\033[A";
	static const wchar_t strB[] = L"\033[B";
	static const wchar_t strC[] = L"\033[C";
	static const wchar_t strD[] = L"\033[D";
	static const wchar_t strH[] = L"\033[H";
	static const wchar_t strF[] = L"\033[F";
	static const wchar_t stOA[] = L"\033OA";
	static const wchar_t stOB[] = L"\033OB";
	static const wchar_t stOC[] = L"\033OC";
	static const wchar_t stOD[] = L"\033OD";
	static const wchar_t stOH[] = L"\033OH";
	static const wchar_t stOF[] = L"\033OF";

	keymacro_add(el, strA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	keymacro_add(el, strB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	keymacro_add(el, strC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	keymacro_add(el, strD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	keymacro_add(el, strH, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	keymacro_add(el, strF, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	keymacro_add(el, stOA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	keymacro_add(el, stOB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	keymacro_add(el, stOC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	keymacro_add(el, stOD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	keymacro_add(el, stOH, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	keymacro_add(el, stOF, &arrow[A_K_EN].fun, arrow[A_K_EN].type);

	if (el->el_map.type != MAP_VI)
		return;
	keymacro_add(el, &strA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	keymacro_add(el, &strB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	keymacro_add(el, &strC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	keymacro_add(el, &strD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	keymacro_add(el, &strH[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	keymacro_add(el, &strF[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	keymacro_add(el, &stOA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	keymacro_add(el, &stOB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	keymacro_add(el, &stOC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	keymacro_add(el, &stOD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	keymacro_add(el, &stOH[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	keymacro_add(el, &stOF[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
}


/* terminal_set_arrow():
 *	Set an arrow key binding
 */
libedit_private int
terminal_set_arrow(EditLine *el, const wchar_t *name, keymacro_value_t *fun,
    int type)
{
	funckey_t *arrow = el->el_terminal.t_fkey;
	int i;

	for (i = 0; i < A_K_NKEYS; i++)
		if (wcscmp(name, arrow[i].name) == 0) {
			arrow[i].fun = *fun;
			arrow[i].type = type;
			return 0;
		}
	return -1;
}


/* terminal_clear_arrow():
 *	Clear an arrow key binding
 */
libedit_private int
terminal_clear_arrow(EditLine *el, const wchar_t *name)
{
	funckey_t *arrow = el->el_terminal.t_fkey;
	int i;

	for (i = 0; i < A_K_NKEYS; i++)
		if (wcscmp(name, arrow[i].name) == 0) {
			arrow[i].type = XK_NOD;
			return 0;
		}
	return -1;
}


/* terminal_print_arrow():
 *	Print the arrow key bindings
 */
libedit_private void
terminal_print_arrow(EditLine *el, const wchar_t *name)
{
	int i;
	funckey_t *arrow = el->el_terminal.t_fkey;

	for (i = 0; i < A_K_NKEYS; i++)
		if (*name == '\0' || wcscmp(name, arrow[i].name) == 0)
			if (arrow[i].type != XK_NOD)
				keymacro_kprint(el, arrow[i].name,
				    &arrow[i].fun, arrow[i].type);
}


/* terminal_bind_arrow():
 *	Bind the arrow keys
 */
libedit_private void
terminal_bind_arrow(EditLine *el)
{
	el_action_t *map;
	const el_action_t *dmap;
	int i, j;
	char *p;
	funckey_t *arrow = el->el_terminal.t_fkey;

	/* Check if the components needed are initialized */
	if (el->el_terminal.t_buf == NULL || el->el_map.key == NULL)
		return;

	map = el->el_map.type == MAP_VI ? el->el_map.alt : el->el_map.key;
	dmap = el->el_map.type == MAP_VI ? el->el_map.vic : el->el_map.emacs;

	terminal_reset_arrow(el);

	for (i = 0; i < A_K_NKEYS; i++) {
		wchar_t wt_str[VISUAL_WIDTH_MAX];
		wchar_t *px;
		size_t n;

		p = el->el_terminal.t_str[arrow[i].key];
		if (!p || !*p)
			continue;
		for (n = 0; n < VISUAL_WIDTH_MAX && p[n]; ++n)
			wt_str[n] = p[n];
		while (n < VISUAL_WIDTH_MAX)
			wt_str[n++] = '\0';
		px = wt_str;
		j = (unsigned char) *p;
		/*
		 * Assign the arrow keys only if:
		 *
		 * 1. They are multi-character arrow keys and the user
		 *    has not re-assigned the leading character, or
		 *    has re-assigned the leading character to be
		 *	  ED_SEQUENCE_LEAD_IN
		 * 2. They are single arrow keys pointing to an
		 *    unassigned key.
		 */
		if (arrow[i].type == XK_NOD)
			keymacro_clear(el, map, px);
		else {
			if (p[1] && (dmap[j] == map[j] ||
				map[j] == ED_SEQUENCE_LEAD_IN)) {
				keymacro_add(el, px, &arrow[i].fun,
				    arrow[i].type);
				map[j] = ED_SEQUENCE_LEAD_IN;
			} else if (map[j] == ED_UNASSIGNED) {
				keymacro_clear(el, map, px);
				if (arrow[i].type == XK_CMD)
					map[j] = arrow[i].fun.cmd;
				else
					keymacro_add(el, px, &arrow[i].fun,
					    arrow[i].type);
			}
		}
	}
}

#ifndef __WINDOWS__
/* terminal_putc():
 *	Add a character
 */
static int
terminal_putc(int c)
{
	if (terminal_outfile == NULL)
		return -1;
	return fputc(c, terminal_outfile);
}
#endif /*__WINDOWS__*/

/* Trace a capability string.  The moves say where the cursor is meant
 * to go; this says what was sent to get it there, which is the half
 * that differs between terminal descriptions.
 */

static void
rtlog_cap(const char *what, const char *cap)
{
	char buf[64];
	size_t n = 0;

	if (cap == NULL)
		return;
	for (const char *p = cap; *p && n < sizeof(buf)-6; p++) {
		unsigned char c = (unsigned char)*p;

		if (c == 033)
			n += (size_t)snprintf(buf+n, sizeof(buf)-n, "<ESC>");
		else if (c == '\r')
			n += (size_t)snprintf(buf+n, sizeof(buf)-n, "<CR>");
		else if (c == '\n')
			n += (size_t)snprintf(buf+n, sizeof(buf)-n, "<NL>");
		else if (c == '\b')
			n += (size_t)snprintf(buf+n, sizeof(buf)-n, "<BS>");
		else if (c >= 0x20 && c < 0x7F)
			buf[n++] = (char)c;
		else
			n += (size_t)snprintf(buf+n, sizeof(buf)-n, "<%02x>", c);
	}
	buf[n] = 0;
	rtlog("  %s: \"%s\"\n", what, buf);
}


static void
terminal_tputs(EditLine *el, const char *cap, int affcnt)
{
	rtlog_cap("terminal_tputs", cap);
#ifdef __WINDOWS__
        el_printf(el, EL_PTR_OUT, "%s", cap);
#else
#ifdef _REENTRANT
	pthread_mutex_lock(&terminal_mutex);
#endif
	terminal_outfile = el->el_outfile;
	(void)tputs(cap, affcnt, terminal_putc);
#ifdef _REENTRANT
	pthread_mutex_unlock(&terminal_mutex);
#endif
#endif
}

/* terminal__putc():
 *	Add a character
 */
libedit_private int
terminal__putc(EditLine *el, wint_t c)
{
	char buf[MB_LEN_MAX +1];
	ssize_t i;
	if (c >= 0x20 && c < 0x7F)
		rtlog("  >> terminal__putc 0x%X '%c'\n", (unsigned)c, (char)c);
	else
		rtlog("  >> terminal__putc 0x%X\n", (unsigned)c);
	if (c == MB_FILL_CHAR)
		return 0;
	if (EL_IS_LITERAL(c))
#ifdef __WINDOWS__
		return el_printf(el, EL_PTR_OUT, "%s", literal_get(el, c));
#else
		return fputs(literal_get(el, c), el->el_outfile);
#endif
#if SIZEOF_WCHAR_T == 2
	/* A supplementary code point lives in the buffer as a UTF-16
	 * surrogate pair.  Hold the lead until the trail arrives and then
	 * encode the whole code point as 4-byte UTF-8 — emitting each
	 * half separately would produce CESU-8 (two 3-byte sequences for
	 * U+D8xx and U+DCxx) which downstream readers decode as two
	 * lone-surrogate characters, not as the intended emoji. */
	if (IS_UTF16_LEAD(c)) {
		el->el_pending_lead = (unsigned)c;
		return 0;
	}
	if (IS_UTF16_TRAIL(c) && el->el_pending_lead != 0) {
		int cp = utf16_decode(el->el_pending_lead, (int)c);
		el->el_pending_lead = 0;
		/* Use utf8_put_char directly — ct_encode_char takes a
		 * wchar_t argument, which is 16 bits on Windows and would
		 * truncate the supplementary code point. */
		char *e = utf8_put_char(buf, cp);
		i = e - buf;
		goto emit;
	}
#endif
	i = ct_encode_char(buf, (size_t)MB_LEN_MAX, c);
#if SIZEOF_WCHAR_T == 2
emit:
#endif
	if (i <= 0)
		return (int)i;
	buf[i] = '\0';
#ifdef __WINDOWS__
	if ( c == '\n' && (el->el_flags&EPILOG) ) /* Pipes do no \r\n translation */
		return el_printf(el, EL_PTR_OUT, "\r\n", buf);
	return el_printf(el, EL_PTR_OUT, "%s", buf);
#else
	return fputs(buf, el->el_outfile);
#endif
}

#if __WINDOWS__
#include "utf8.h"

static bool
is_all_ascii(const char *buf, size_t len)
{ for(size_t i=0; i<len; i++)
  { if ( (buf[i]&0xff) > 127 )
      return false;
  }

  return true;
}

#define WBUF_SIZE 1024

static bool
el_write_buffer(EditLine *el, HANDLE hOut, const char *buf, size_t len)
{ if ( el->el_flags & EPILOG )
  { return WriteFile(hOut, buf, (DWORD)len, NULL, NULL);
  } else if ( is_all_ascii(buf, len) )
  { return WriteConsoleA(hOut, buf, (DWORD)len, NULL, NULL);
  } else
  { wchar_t wbuf[WBUF_SIZE];
    wchar_t *ws = wbuf;
    wchar_t *we = &wbuf[WBUF_SIZE];
    const char *s = buf;
    const char *e = &buf[len];

    while(s<e)
    { int chr;

      s = utf8_get_char(s, &chr);
      ws = put_wchar(ws, chr);
      if ( ws > we-2 )
      { if ( !WriteConsoleW(hOut, wbuf, (DWORD)(ws-wbuf), NULL, NULL) )
	  return false;
	ws = wbuf;
      }
    }

    return WriteConsoleW(hOut, wbuf, (DWORD)(ws-wbuf), NULL, NULL);
  }
}
#endif

/* terminal__flush():
 *	Flush output
 */
libedit_private void
terminal__flush(EditLine *el)
{
#ifdef __WINDOWS__
  if ( el->out_buffer.len )
  { el_write_buffer(el, el->el_hOut,
		    el->out_buffer.data, el->out_buffer.len);
    el->out_buffer.len = 0;
  }
#else
	(void) fflush(el->el_outfile);
#endif
}

/* terminal_writec():
 *	Write the given character out, in a human readable form
 */
libedit_private void
terminal_writec(EditLine *el, wint_t c)
{
	wchar_t visbuf[VISUAL_WIDTH_MAX +1];
	ssize_t vcnt = ct_visual_char(visbuf, VISUAL_WIDTH_MAX, c);
	if (vcnt < 0)
		vcnt = 0;
	visbuf[vcnt] = '\0';
	terminal_overwrite(el, visbuf, (size_t)vcnt);
	terminal__flush(el);
}


/* terminal_telltc():
 *	Print the current termcap characteristics
 */
libedit_private int
/*ARGSUSED*/
terminal_telltc(EditLine *el, int argc __attribute__((__unused__)),
    const wchar_t **argv __attribute__((__unused__)))
{
	const struct termcapstr *t;
	char **ts;

	(void) el_printf(el, EL_PTR_OUT, "\n\tYour terminal has the\n");
	(void) el_printf(el, EL_PTR_OUT, "\tfollowing characteristics:\n\n");
	(void) el_printf(el, EL_PTR_OUT, "\tIt has %d columns and %d lines\n",
	    Val(T_co), Val(T_li));
	(void) el_printf(el, EL_PTR_OUT,
	    "\tIt has %s meta key\n", EL_HAS_META ? "a" : "no");
	(void) el_printf(el, EL_PTR_OUT,
	    "\tIt can%suse tabs\n", EL_CAN_TAB ? " " : "not ");
	(void) el_printf(el, EL_PTR_OUT, "\tIt %s automatic margins\n",
	    EL_HAS_AUTO_MARGINS ? "has" : "does not have");
	if (EL_HAS_AUTO_MARGINS)
		(void) el_printf(el, EL_PTR_OUT, "\tIt %s magic margins\n",
		    EL_HAS_MAGIC_MARGINS ? "has" : "does not have");

	for (t = tstr, ts = el->el_terminal.t_str; t->name != NULL; t++, ts++) {
		const char *ub;
		if (*ts && **ts) {
			ub = ct_encode_string(ct_visual_string(
			    ct_decode_string(*ts, &el->el_scratch),
			    &el->el_visual), &el->el_scratch);
		} else {
			ub = "(empty)";
		}
		(void) el_printf(el, EL_PTR_OUT, "\t%25s (%s) == %s\n",
		    t->long_name, t->name, ub);
	}
#ifdef __WINDOWS__
	(void) el_printf(el, EL_PTR_OUT, "\n");
#else
	(void) fputc('\n', el->el_outfile);
#endif
	return 0;
}


/* terminal_settc():
 *	Change the current terminal characteristics
 */
libedit_private int
/*ARGSUSED*/
terminal_settc(EditLine *el, int argc __attribute__((__unused__)),
    const wchar_t **argv)
{
	const struct termcapstr *ts;
	const struct termcapval *tv;
	char what[8], how[8];
	long i;
	char *ep;

	if (argv == NULL || argv[1] == NULL || argv[2] == NULL)
		return -1;

	strlcpy(what, ct_encode_string(argv[1], &el->el_scratch), sizeof(what));
	strlcpy(how,  ct_encode_string(argv[2], &el->el_scratch), sizeof(how));

	/*
         * Do the strings first
         */
	for (ts = tstr; ts->name != NULL; ts++)
		if (strcmp(ts->name, what) == 0)
			break;

	if (ts->name != NULL) {
		terminal_alloc(el, ts, how);
		terminal_setflags(el);
		return 0;
	}
	/*
         * Do the numeric ones second
         */
	for (tv = tval; tv->name != NULL; tv++)
		if (strcmp(tv->name, what) == 0)
			break;

	if (tv->name == NULL) {
		(void) el_printf(el, EL_PTR_ERR,
		    "%ls: Bad capability `%s'.\n", argv[0], what);
		return -1;
	}

	if (tv == &tval[T_pt] || tv == &tval[T_km] ||
	    tv == &tval[T_am] || tv == &tval[T_xn]) {
		/*
		 * Booleans
		 */
		if (strcmp(how, "yes") == 0)
			el->el_terminal.t_val[tv - tval] = 1;
		else if (strcmp(how, "no") == 0)
			el->el_terminal.t_val[tv - tval] = 0;
		else {
			(void) el_printf(el, EL_PTR_ERR,
			    "%ls: Bad value `%s'.\n", argv[0], how);
			return -1;
		}
		terminal_setflags(el);
		return 0;
	}

	/*
	 * Numerics
	 */
	i = strtol(how, &ep, 10);
	if (*ep != '\0') {
		(void) el_printf(el, EL_PTR_ERR,
		    "%ls: Bad value `%s'.\n", argv[0], how);
		return -1;
	}
	el->el_terminal.t_val[tv - tval] = (int) i;
	i = 0;
	if (tv == &tval[T_co]) {
		el->el_terminal.t_size.v = Val(T_co);
		i++;
	} else if (tv == &tval[T_li]) {
		el->el_terminal.t_size.h = Val(T_li);
		i++;
	}
	if (i && terminal_change_size(el, Val(T_li), Val(T_co)) == -1)
		return -1;
	return 0;
}


/* terminal_gettc():
 *	Get the current terminal characteristics
 */
libedit_private int
/*ARGSUSED*/
terminal_gettc(EditLine *el, int argc __attribute__((__unused__)), char **argv)
{
	const struct termcapstr *ts;
	const struct termcapval *tv;
	char *what;
	void *how;

	if (argv == NULL || argv[1] == NULL || argv[2] == NULL)
		return -1;

	what = argv[1];
	how = argv[2];

	/*
         * Do the strings first
         */
	for (ts = tstr; ts->name != NULL; ts++)
		if (strcmp(ts->name, what) == 0)
			break;

	if (ts->name != NULL) {
		*(char **)how = el->el_terminal.t_str[ts - tstr];
		return 0;
	}
	/*
         * Do the numeric ones second
         */
	for (tv = tval; tv->name != NULL; tv++)
		if (strcmp(tv->name, what) == 0)
			break;

	if (tv->name == NULL)
		return -1;

	if (tv == &tval[T_pt] || tv == &tval[T_km] ||
	    tv == &tval[T_am] || tv == &tval[T_xn]) {
		static char yes[] = "yes";
		static char no[] = "no";
		if (el->el_terminal.t_val[tv - tval])
			*(char **)how = yes;
		else
			*(char **)how = no;
		return 0;
	} else {
		*(int *)how = el->el_terminal.t_val[tv - tval];
		return 0;
	}
}

/* terminal_echotc():
 *	Print the termcap string out with variable substitution
 */
libedit_private int
/*ARGSUSED*/
terminal_echotc(EditLine *el, int argc __attribute__((__unused__)),
    const wchar_t **argv)
{
	char *cap, *scap;
	wchar_t *ep;
	int arg_need, arg_cols, arg_rows;
	int verbose = 0, silent = 0;
	char *area;
	static const char fmts[] = "%s\n", fmtd[] = "%d\n";
	const struct termcapstr *t;
	char buf[TC_BUFSIZE];
	long i;

	area = buf;

	if (argv == NULL || argv[1] == NULL)
		return -1;
	argv++;

	if (argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'v':
			verbose = 1;
			break;
		case 's':
			silent = 1;
			break;
		default:
			/* stderror(ERR_NAME | ERR_TCUSAGE); */
			break;
		}
		argv++;
	}
	if (!*argv || *argv[0] == '\0')
		return 0;
	if (wcscmp(*argv, L"tabs") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmts, EL_CAN_TAB ? "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"meta") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmts, Val(T_km) ? "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"xn") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmts, EL_HAS_MAGIC_MARGINS ?
		    "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"am") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmts, EL_HAS_AUTO_MARGINS ?
		    "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"baud") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmtd, (int)el->el_tty.t_speed);
		return 0;
	} else if (wcscmp(*argv, L"rows") == 0 ||
                   wcscmp(*argv, L"lines") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmtd, Val(T_li));
		return 0;
	} else if (wcscmp(*argv, L"cols") == 0) {
		(void) el_printf(el, EL_PTR_OUT, fmtd, Val(T_co));
		return 0;
	}
	/*
         * Try to use our local definition first
         */
	scap = NULL;
	for (t = tstr; t->name != NULL; t++)
		if (strcmp(t->name,
		    ct_encode_string(*argv, &el->el_scratch)) == 0) {
			scap = el->el_terminal.t_str[t - tstr];
			break;
		}
	if (t->name == NULL) {
		/* XXX: some systems' tgetstr needs non const */
                scap = tgetstr(ct_encode_string(*argv, &el->el_scratch), &area);
	}
	if (!scap || scap[0] == '\0') {
		if (!silent)
			(void) el_printf(el, EL_PTR_ERR,
			    "echotc: Termcap parameter `%ls' not found.\n",
			    *argv);
		return -1;
	}
	/*
         * Count home many values we need for this capability.
         */
	for (cap = scap, arg_need = 0; *cap; cap++)
		if (*cap == '%')
			switch (*++cap) {
			case 'd':
			case '2':
			case '3':
			case '.':
			case '+':
				arg_need++;
				break;
			case '%':
			case '>':
			case 'i':
			case 'r':
			case 'n':
			case 'B':
			case 'D':
				break;
			default:
				/*
				 * hpux has lot's of them...
				 */
				if (verbose)
					(void) el_printf(el, EL_PTR_ERR,
				"echotc: Warning: unknown termcap %% `%c'.\n",
					    *cap);
				/* This is bad, but I won't complain */
				break;
			}

	switch (arg_need) {
	case 0:
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Warning: Extra argument `%ls'.\n",
				    *argv);
			return -1;
		}
		terminal_tputs(el, scap, 1);
		break;
	case 1:
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Warning: Missing argument.\n");
			return -1;
		}
		arg_cols = 0;
		i = wcstol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Bad value `%ls' for rows.\n",
				    *argv);
			return -1;
		}
		arg_rows = (int) i;
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Warning: Extra argument `%ls"
				    "'.\n", *argv);
			return -1;
		}
		terminal_tputs(el, tgoto(scap, arg_cols, arg_rows), 1);
		break;
	default:
		/* This is wrong, but I will ignore it... */
		if (verbose)
			(void) el_printf(el, EL_PTR_ERR,
			 "echotc: Warning: Too many required arguments (%d).\n",
			    arg_need);
		/* FALLTHROUGH */
	case 2:
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Warning: Missing argument.\n");
			return -1;
		}
		i = wcstol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Bad value `%ls' for cols.\n",
				    *argv);
			return -1;
		}
		arg_cols = (int) i;
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Warning: Missing argument.\n");
			return -1;
		}
		i = wcstol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Bad value `%ls' for rows.\n",
				    *argv);
			return -1;
		}
		arg_rows = (int) i;
		if (*ep != '\0') {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Bad value `%ls'.\n", *argv);
			return -1;
		}
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) el_printf(el, EL_PTR_ERR,
				    "echotc: Warning: Extra argument `%ls"
				    "'.\n", *argv);
			return -1;
		}
		terminal_tputs(el, tgoto(scap, arg_cols, arg_rows), arg_rows);
		break;
	}
	return 0;
}


libedit_private int
el_printf(EditLine *el, el_prt_stream to, const char *fmt, ...)
{ va_list args;
  int len;

  va_start(args, fmt);
#if __WINDOWS__
  char buf[BUFFER_SIZE];

  len = vsnprintf(buf, sizeof(buf)-1, fmt, args);
  assert(len < sizeof(buf));	/* TODO: create larger buffer */

  if ( to == EL_PTR_OUT )
  { if ( el->out_buffer.len+len > BUFFER_SIZE )
      terminal__flush(el);
    memcpy(&el->out_buffer.data[el->out_buffer.len], buf, len);
    el->out_buffer.len += len;
  } else
  { el_write_buffer(el, el->el_hErr, buf, len);
  }
#else
  FILE *out;

  if ( to == EL_PTR_OUT )
    out = el->el_outfile;
  else
    out = el->el_errfile;

  len = vfprintf(out, fmt, args);
#endif
  va_end(args);

  return len;
}
