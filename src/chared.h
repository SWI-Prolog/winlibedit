/*	$NetBSD: chared.h,v 1.30 2016/05/22 19:44:26 christos Exp $	*/

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
 *
 *	@(#)chared.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.chared.h: Character editor interface
 */
#ifndef _h_el_chared
#define	_h_el_chared

/*
 * This is an issue of basic "vi" look-and-feel. Defining VI_MOVE works
 * like real vi: i.e. the transition from command<->insert modes moves
 * the cursor.
 *
 * On the other hand we really don't want to move the cursor, because
 * all the editing commands don't include the character under the cursor.
 * Probably the best fix is to make all the editing commands aware of
 * this fact.
 */
#define	VI_MOVE

/*
 * Undo/redo.  A saved line is an independent copy rather than a pointer
 * into el_line.buffer, so that ch_enlargebufs() need not rebase it.
 */
typedef struct c_undo_line_t {
	wchar_t	*buf;			/* full saved text, malloc'ed */
	size_t	 len;			/* length of saved line */
	int	 cursor;		/* position of saved cursor */
	int	 eventno;		/* history event it came from */
} c_undo_line_t;

#define	C_UNDO_MAX	64		/* deepest history we keep */

typedef struct c_undo_t {
	c_undo_line_t	*undo;		/* stack, oldest first */
	size_t		 nundo;
	c_undo_line_t	*redo;		/* stack, oldest first */
	size_t		 nredo;
	c_undo_line_t	 cur;		/* line as of the last record */
	int		 valid;		/* cur holds a line */
	int		 in_undo;	/* do not record our own edit */
} c_undo_t;

/* redo for vi */
typedef struct c_redo_t {
	wchar_t	*buf;			/* redo insert key sequence */
	wchar_t	*pos;
	wchar_t	*lim;
	el_action_t	cmd;		/* command to redo */
	wchar_t	ch;			/* char that invoked it */
	int	count;
	int	action;			/* from cv_action() */
} c_redo_t;

/*
 * Current action information for vi
 */
typedef struct c_vcmd_t {
	int	 action;
	wchar_t	*pos;
} c_vcmd_t;

/*
 * Kill buffer for emacs
 */
typedef struct c_kill_t {
	wchar_t	*buf;
	wchar_t	*last;
	wchar_t	*mark;
} c_kill_t;

typedef void (*el_zfunc_t)(EditLine *, void *);
typedef const char *(*el_afunc_t)(void *, const char *);

/*
 * Note that we use both data structures because the user can bind
 * commands from both editors!
 */
typedef struct el_chared_t {
	c_undo_t	c_undo;
	c_kill_t	c_kill;
	c_redo_t	c_redo;
	c_vcmd_t	c_vcmd;
	el_zfunc_t	c_resizefun;
	el_afunc_t	c_aliasfun;
	void *		c_resizearg;
	void *		c_aliasarg;
} el_chared_t;


#define	STRQQ		"\"\""

#define	isglob(a)	(strchr("*[]?", (a)) != NULL)

#define	NOP		0x00
#define	EL_DELETE		0x01
#define	INSERT		0x02
#define	YANK		0x04

#define	CHAR_FWD	(+1)
#define	CHAR_BACK	(-1)

#define	MODE_INSERT	0
#define	MODE_REPLACE	1
#define	MODE_REPLACE_1	2


libedit_private int	 cv__isword(EditLine *, wint_t);
libedit_private int	 cv__isWord(EditLine *, wint_t);
libedit_private void	 cv_delfini(EditLine *);
libedit_private wchar_t *cv__endword(EditLine *, wchar_t *, wchar_t *, int,
    int (*)(EditLine *, wint_t));
libedit_private int	 ce__isword(EditLine *, wint_t);
libedit_private void	 cv_redo_start(EditLine *);
libedit_private void	 c_undo_reset(EditLine *);
libedit_private void	 c_undo_end(EditLine *);
libedit_private void	 c_undo_record(EditLine *, el_action_t);
libedit_private int	 c_undo_apply(EditLine *, int);
libedit_private void	 cv_yank(EditLine *, const wchar_t *, int);
libedit_private wchar_t *cv_next_word(EditLine*, wchar_t *, wchar_t *, int,
			int (*)(EditLine *, wint_t));
libedit_private wchar_t *cv_prev_word(EditLine *, wchar_t *, wchar_t *, int,
    int (*)(EditLine *, wint_t));
libedit_private wchar_t *c__next_word(EditLine *, wchar_t *, wchar_t *, int,
    int (*)(EditLine *, wint_t));
libedit_private wchar_t *c__prev_word(EditLine *, wchar_t *, wchar_t *, int,
    int (*)(EditLine *, wint_t));
libedit_private void	 c_insert(EditLine *, int);
libedit_private void	 c_delbefore(EditLine *, int);
libedit_private void	 c_delbefore1(EditLine *);
libedit_private void	 c_delafter(EditLine *, int);
libedit_private void	 c_delafter1(EditLine *);
libedit_private int	 c_gets(EditLine *, wchar_t *, const wchar_t *);
libedit_private int	 c_hpos(EditLine *);

libedit_private int	 ch_init(EditLine *);
libedit_private void	 ch_reset(EditLine *);
libedit_private int	 ch_resizefun(EditLine *, el_zfunc_t, void *);
libedit_private int	 ch_aliasfun(EditLine *, el_afunc_t, void *);
libedit_private int	 ch_enlargebufs(EditLine *, size_t);
libedit_private void	 ch_end(EditLine *);

/* Surrogate- and combining-mark-aware grapheme stepping.  Defined in
 * common.c.  Each translation unit that does cursor-by-grapheme work
 * (common.c, emacs.c, ...) must call these instead of stepping wchar_t
 * units directly — on Windows a non-BMP code point is a surrogate pair
 * (two wchar_t), and stepping one slot at a time chops it in half. */
libedit_private wchar_t	*el_prev_grapheme(wchar_t *cursor, wchar_t *buffer);
libedit_private wchar_t	*el_next_grapheme(wchar_t *cursor, wchar_t *limit);

#endif /* _h_el_chared */
