/*	$NetBSD: chared.c,v 1.64 2024/06/29 14:13:14 christos Exp $	*/

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
static char sccsid[] = "@(#)chared.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: chared.c,v 1.64 2024/06/29 14:13:14 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * chared.c: Character editor utilities
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "el.h"
#include "common.h"
#include "fcns.h"
#include "utf8.h"

/* value to leave unused in line buffer */
#define	EL_LEAVE	2

/* cv_redo_start():
 *	Note that a change command begins here, for the vi redo command.
 *	Undo is recorded by the read loop, see c_undo_record().
 */
libedit_private void
cv_redo_start(EditLine *el)
{
	c_redo_t *r = &el->el_chared.c_redo;

	/* save command info for redo.  r->pos = r->buf is the only rewind
	 * of the key capture read.c fills in vi command mode. */
	r->count = el->el_state.doingarg ? el->el_state.argument : 0;
	r->action = el->el_chared.c_vcmd.action;
	r->pos = r->buf;
	r->cmd = el->el_state.thiscmd;
	r->ch = el->el_state.thisch;
}


		/************************************************************
		 *			UNDO / REDO			    *
		 ************************************************************/

/* The line is saved as an independent copy rather than as a pointer into
 * el_line.buffer, so ch_enlargebufs() need not rebase it and a snapshot
 * survives any reallocation.  Recording happens in one place, the command
 * dispatch loop of el_wgets(), which is the only point that sees every
 * edit as a finished unit -- several commands write the line without
 * going through c_insert()/c_delafter()/c_delbefore().
 */

/* c_undo_free():
 *	Discard a stack of saved lines
 */
static void
c_undo_free(c_undo_line_t **stack, size_t *n)
{
	size_t i;

	for (i = 0; i < *n; i++)
		el_free((*stack)[i].buf);
	el_free(*stack);
	*stack = NULL;
	*n = 0;
}


/* c_undo_save():
 *	Copy the current line into `to'.  Answers -1 on failure, in which
 *	case `to' is left empty rather than stale.
 */
static int
c_undo_save(EditLine *el, c_undo_line_t *to)
{
	ssize_t len = el->el_line.lastchar - el->el_line.buffer;
	wchar_t *buf;

	if (len < 0)			/* the SIGINT handler can shorten
					   the line under a command */
		len = 0;
	buf = el_realloc(to->buf, ((size_t)len + 1) * sizeof(*buf));
	if (buf == NULL) {
		el_free(to->buf);
		to->buf = NULL;
		to->len = 0;
		return -1;
	}
	(void)memcpy(buf, el->el_line.buffer, (size_t)len * sizeof(*buf));
	buf[len] = '\0';
	to->buf = buf;
	to->len = (size_t)len;
	to->cursor = (int)(el->el_line.cursor - el->el_line.buffer);
	to->eventno = el->el_history.eventno;
	return 0;
}


/* c_undo_push():
 *	Push a copy of `from' onto a stack, dropping the oldest entry once
 *	the stack is full.  Takes ownership of nothing; it copies.
 */
static int
c_undo_push(c_undo_line_t **stack, size_t *n, const c_undo_line_t *from)
{
	c_undo_line_t *ns;
	wchar_t *buf;

	if (*n == C_UNDO_MAX) {		/* drop the oldest */
		el_free((*stack)[0].buf);
		(void)memmove(*stack, *stack + 1,
		    (C_UNDO_MAX - 1) * sizeof(**stack));
		(*n)--;
	}
	ns = el_realloc(*stack, (*n + 1) * sizeof(*ns));
	if (ns == NULL)
		return -1;
	*stack = ns;
	buf = el_malloc((from->len + 1) * sizeof(*buf));
	if (buf == NULL)
		return -1;
	(void)memcpy(buf, from->buf, from->len * sizeof(*buf));
	buf[from->len] = '\0';
	ns[*n] = *from;
	ns[*n].buf = buf;
	(*n)++;
	return 0;
}


/* c_undo_restore():
 *	Put a saved line back.  The line buffer only ever grows within one
 *	line's lifetime and the stacks are cleared by ch_reset(), so a
 *	snapshot always fits; check anyway.
 */
static void
c_undo_restore(EditLine *el, const c_undo_line_t *from)
{
	size_t max = (size_t)(el->el_line.limit - el->el_line.buffer);
	size_t len = from->len < max ? from->len : max;

	(void)memcpy(el->el_line.buffer, from->buf, len * sizeof(wchar_t));
	el->el_line.lastchar = el->el_line.buffer + len;
	*el->el_line.lastchar = '\0';
	el->el_line.cursor = el->el_line.buffer +
	    ((size_t)from->cursor < len ? (size_t)from->cursor : len);
	el->el_history.eventno = from->eventno;
}


/* c_undo_reset():
 *	Forget the undo history.  Called per line from ch_reset().
 */
libedit_private void
c_undo_reset(EditLine *el)
{
	c_undo_t *un = &el->el_chared.c_undo;

	c_undo_free(&un->undo, &un->nundo);
	c_undo_free(&un->redo, &un->nredo);
	un->in_undo = 0;
	un->valid = 0;
	if (el->el_line.buffer != NULL && c_undo_save(el, &un->cur) == 0)
		un->valid = 1;
}


/* c_undo_end():
 *	Release everything.  ch_end() calls this before ch_reset().
 */
libedit_private void
c_undo_end(EditLine *el)
{
	c_undo_t *un = &el->el_chared.c_undo;

	c_undo_free(&un->undo, &un->nundo);
	c_undo_free(&un->redo, &un->nredo);
	el_free(un->cur.buf);
	un->cur.buf = NULL;
	un->cur.len = 0;
	un->valid = 0;
}


/* c_undo_coalesce():
 *	Should this command join the run recorded by the previous one?
 *	A run of self-insert is one unit, so undo takes back a word rather
 *	than a letter, but it stops at a space the way readline does.
 */
static int
c_undo_coalesce(EditLine *el, el_action_t cmd)
{

	return cmd == ED_INSERT && el->el_state.lastcmd == ED_INSERT &&
	    !iswspace(el->el_state.thisch);
}


/* c_undo_record():
 *	Called from the dispatch loop after every command, before
 *	el_state.lastcmd is updated -- the coalescing test above needs
 *	lastcmd to still be the previous command.
 */
libedit_private void
c_undo_record(EditLine *el, el_action_t cmd)
{
	c_undo_t *un = &el->el_chared.c_undo;
	ssize_t len = el->el_line.lastchar - el->el_line.buffer;

	if (len < 0)
		len = 0;

	/* A supplementary code point arrives as two wchar_t and the line
	 * holds only the lead surrogate at this point (see ed_insert()).
	 * Recording now could leave an undo on half a code point. */
#if SIZEOF_WCHAR_T == 2
	if (IS_UTF16_LEAD(el->el_state.thisch))
		return;
#endif

	if (!un->valid) {		/* could not save; try again */
		if (c_undo_save(el, &un->cur) == 0)
			un->valid = 1;
		un->in_undo = 0;
		return;
	}

	if (un->in_undo) {		/* our own edit: resync, keep redo */
		un->in_undo = 0;
		(void)c_undo_save(el, &un->cur);
		return;
	}

	if ((size_t)len == un->cur.len &&
	    memcmp(el->el_line.buffer, un->cur.buf,
		   (size_t)len * sizeof(wchar_t)) == 0) {
		/* text unchanged: keep the cursor current, so a later
		 * snapshot restores where the user actually was */
		un->cur.cursor = (int)(el->el_line.cursor - el->el_line.buffer);
		un->cur.eventno = el->el_history.eventno;
		return;
	}

	if (!c_undo_coalesce(el, cmd)) {
		if (c_undo_push(&un->undo, &un->nundo, &un->cur) == -1) {
			un->valid = 0;
			return;
		}
		c_undo_free(&un->redo, &un->nredo);
	}
	if (c_undo_save(el, &un->cur) == -1)
		un->valid = 0;
}


/* c_undo_apply():
 *	Step one line backwards, or forwards when `redo' is set.  Answers
 *	-1 when there is nothing to step to.
 */
libedit_private int
c_undo_apply(EditLine *el, int redo)
{
	c_undo_t *un = &el->el_chared.c_undo;
	c_undo_line_t **from = redo ? &un->redo : &un->undo;
	size_t *nfrom = redo ? &un->nredo : &un->nundo;
	c_undo_line_t **to = redo ? &un->undo : &un->redo;
	size_t *nto = redo ? &un->nundo : &un->nredo;

	if (*nfrom == 0 || !un->valid)
		return -1;
	if (c_undo_push(to, nto, &un->cur) == -1)
		return -1;

	(*nfrom)--;
	c_undo_restore(el, &(*from)[*nfrom]);
	el_free((*from)[*nfrom].buf);
	(*from)[*nfrom].buf = NULL;

	un->in_undo = 1;		/* do not record what we just did */
	return 0;
}

/* cv_yank():
 *	Save yank/delete data for paste
 */
libedit_private void
cv_yank(EditLine *el, const wchar_t *ptr, int size)
{
	c_kill_t *k = &el->el_chared.c_kill;

	(void)memcpy(k->buf, ptr, (size_t)size * sizeof(*k->buf));
	k->last = k->buf + size;
}


/* c_insert():
 *	Insert num characters
 */
libedit_private void
c_insert(EditLine *el, int num)
{
	wchar_t *cp;

	if (el->el_line.lastchar + num >= el->el_line.limit) {
		if (!ch_enlargebufs(el, (size_t)num))
			return;		/* can't go past end of buffer */
	}

	if (el->el_line.cursor < el->el_line.lastchar) {
		/* if I must move chars */
		for (cp = el->el_line.lastchar; cp >= el->el_line.cursor; cp--)
			cp[num] = *cp;
	}
	el->el_line.lastchar += num;
}


/* c_delafter():
 *	Delete num characters after the cursor
 */
libedit_private void
c_delafter(EditLine *el, int num)
{

	if (el->el_line.cursor + num > el->el_line.lastchar)
		num = (int)(el->el_line.lastchar - el->el_line.cursor);

	if (el->el_map.current != el->el_map.emacs) {
		cv_redo_start(el);
		cv_yank(el, el->el_line.cursor, num);
	}

	if (num > 0) {
		wchar_t *cp;

		for (cp = el->el_line.cursor; cp <= el->el_line.lastchar; cp++)
			*cp = cp[num];

		el->el_line.lastchar -= num;
	}
}


/* c_delafter1():
 *	Delete the character after the cursor, do not yank
 */
libedit_private void
c_delafter1(EditLine *el)
{
	wchar_t *cp;

	for (cp = el->el_line.cursor; cp <= el->el_line.lastchar; cp++)
		*cp = cp[1];

	el->el_line.lastchar--;
}


/* c_delbefore():
 *	Delete num characters before the cursor
 */
libedit_private void
c_delbefore(EditLine *el, int num)
{

	if (el->el_line.cursor - num < el->el_line.buffer)
		num = (int)(el->el_line.cursor - el->el_line.buffer);

	if (el->el_map.current != el->el_map.emacs) {
		cv_redo_start(el);
		cv_yank(el, el->el_line.cursor - num, num);
	}

	if (num > 0) {
		wchar_t *cp;

		for (cp = el->el_line.cursor - num;
		    &cp[num] <= el->el_line.lastchar;
		    cp++)
			*cp = cp[num];

		el->el_line.lastchar -= num;
	}
}


/* c_delbefore1():
 *	Delete the character before the cursor, do not yank
 */
libedit_private void
c_delbefore1(EditLine *el)
{
	wchar_t *cp;

	for (cp = el->el_line.cursor - 1; cp <= el->el_line.lastchar; cp++)
		*cp = cp[1];

	el->el_line.lastchar--;
}


/* ce__isword():
 *	Return if p is part of a word according to emacs
 */
libedit_private int
ce__isword(EditLine *el, wint_t p)
{
	/* Combining marks and variation selectors (wcwidth == 0, not a
	 * control char) logically attach to the preceding base character.
	 * Treat them as word chars so word navigation over NFD text
	 * (e.g. 'a'+U+0300 = 'à') doesn't stop between the base and its
	 * combiner. */
	if (!iswcntrl((wchar_t)p) && wcwidth((uchar_t)p) == 0)
		return 1;
	return iswalnum(p) || wcschr(el->el_map.wordchars, p) != NULL;
}


/* cv__isword():
 *	Return if p is part of a word according to vi
 */
libedit_private int
cv__isword(EditLine *el, wint_t p)
{
	/* Combining marks: see ce__isword().  Tag them as type-1 ("word") so
	 * they stay attached to the preceding alnum char during navigation. */
	if (!iswcntrl((wchar_t)p) && wcwidth((uchar_t)p) == 0)
		return 1;
	if (iswalnum(p) || wcschr(el->el_map.wordchars, p) != NULL)
		return 1;
	if (iswgraph(p))
		return 2;
	return 0;
}


/* cv__isWord():
 *	Return if p is part of a big word according to vi
 */
libedit_private int
cv__isWord(EditLine *el __attribute__((__unused__)), wint_t p)
{
	return !iswspace(p);
}


/* c__prev_word():
 *	Find the previous word
 */
libedit_private wchar_t *
c__prev_word(EditLine *el, wchar_t *p, wchar_t *low, int n,
    int (*wtest)(EditLine *, wint_t))
{
	p--;

	while (n--) {
		while ((p >= low) && !(*wtest)(el, *p))
			p--;
		while ((p >= low) && (*wtest)(el, *p))
			p--;
	}

	/* cp now points to one character before the word */
	p++;
	if (p < low)
		p = low;
	/* cp now points where we want it */
	return p;
}


/* c__next_word():
 *	Find the next word
 */
libedit_private wchar_t *
c__next_word(EditLine *el, wchar_t *p, wchar_t *high, int n,
    int (*wtest)(EditLine *, wint_t))
{
	while (n--) {
		while ((p < high) && !(*wtest)(el, *p))
			p++;
		while ((p < high) && (*wtest)(el, *p))
			p++;
	}
	if (p > high)
		p = high;
	/* p now points where we want it */
	return p;
}

/* cv_next_word():
 *	Find the next word vi style
 */
libedit_private wchar_t *
cv_next_word(EditLine *el, wchar_t *p, wchar_t *high, int n,
    int (*wtest)(EditLine *el, wint_t))
{
	int test;

	while (n--) {
		test = (*wtest)(el, *p);
		while ((p < high) && (*wtest)(el, *p) == test)
			p++;
		/*
		 * vi historically deletes with cw only the word preserving the
		 * trailing whitespace! This is not what 'w' does..
		 */
		if (n || el->el_chared.c_vcmd.action != (EL_DELETE|INSERT))
			while ((p < high) && iswspace(*p))
				p++;
	}

	/* p now points where we want it */
	if (p > high)
		return high;
	else
		return p;
}


/* cv_prev_word():
 *	Find the previous word vi style
 */
libedit_private wchar_t *
cv_prev_word(EditLine *el, wchar_t *p, wchar_t *low, int n,
    int (*wtest)(EditLine *el, wint_t))
{
	int test;

	p--;
	while (n--) {
		while ((p > low) && iswspace(*p))
			p--;
		test = (*wtest)(el, *p);
		while ((p >= low) && (*wtest)(el, *p) == test)
			p--;
		if (p < low)
			return low;
	}
	p++;

	/* p now points where we want it */
	if (p < low)
		return low;
	else
		return p;
}


/* cv_delfini():
 *	Finish vi delete action
 */
libedit_private void
cv_delfini(EditLine *el)
{
	int size;
	int action = el->el_chared.c_vcmd.action;

	if (action & INSERT)
		el->el_map.current = el->el_map.key;

	if (el->el_chared.c_vcmd.pos == 0)
		/* sanity */
		return;

	size = (int)(el->el_line.cursor - el->el_chared.c_vcmd.pos);
	if (size == 0)
		size = 1;
	el->el_line.cursor = el->el_chared.c_vcmd.pos;
	if (action & YANK) {
		if (size > 0)
			cv_yank(el, el->el_line.cursor, size);
		else
			cv_yank(el, el->el_line.cursor + size, -size);
	} else {
		if (size > 0) {
			c_delafter(el, size);
			re_refresh_cursor(el);
		} else  {
			c_delbefore(el, -size);
			el->el_line.cursor += size;
		}
	}
	el->el_chared.c_vcmd.action = NOP;
}


/* cv__endword():
 *	Go to the end of this word according to vi
 */
libedit_private wchar_t *
cv__endword(EditLine *el, wchar_t *p, wchar_t *high, int n,
    int (*wtest)(EditLine *, wint_t))
{
	int test;

	p++;

	while (n--) {
		while ((p < high) && iswspace(*p))
			p++;

		test = (*wtest)(el, *p);
		while ((p < high) && (*wtest)(el, *p) == test)
			p++;
	}
	p--;
	return p;
}

/* ch_init():
 *	Initialize the character editor
 */
libedit_private int
ch_init(EditLine *el)
{
	el->el_line.buffer		= el_calloc(EL_BUFSIZ,
	    sizeof(*el->el_line.buffer));
	if (el->el_line.buffer == NULL)
		return -1;

	el->el_line.cursor		= el->el_line.buffer;
	el->el_line.lastchar		= el->el_line.buffer;
	el->el_line.limit		= &el->el_line.buffer[EL_BUFSIZ - EL_LEAVE];

	el->el_chared.c_redo.buf	= el_calloc(EL_BUFSIZ,
	    sizeof(*el->el_chared.c_redo.buf));
	if (el->el_chared.c_redo.buf == NULL)
		goto out;
	el->el_chared.c_redo.pos	= el->el_chared.c_redo.buf;
	el->el_chared.c_redo.lim	= el->el_chared.c_redo.buf + EL_BUFSIZ;
	el->el_chared.c_redo.cmd	= ED_UNASSIGNED;

	el->el_chared.c_vcmd.action	= NOP;
	el->el_chared.c_vcmd.pos	= el->el_line.buffer;

	el->el_chared.c_kill.buf	= el_calloc(EL_BUFSIZ,
	    sizeof(*el->el_chared.c_kill.buf));
	if (el->el_chared.c_kill.buf == NULL)
		goto out;
	el->el_chared.c_kill.mark	= el->el_line.buffer;
	el->el_chared.c_kill.last	= el->el_chared.c_kill.buf;
	el->el_chared.c_resizefun	= NULL;
	el->el_chared.c_resizearg	= NULL;
	el->el_chared.c_aliasfun	= NULL;
	el->el_chared.c_aliasarg	= NULL;

	el->el_map.current		= el->el_map.key;

	el->el_state.inputmode		= MODE_INSERT; /* XXX: save a default */
	el->el_state.doingarg		= 0;
	el->el_state.metanext		= 0;
	el->el_state.argument		= 1;
	el->el_state.lastcmd		= ED_UNASSIGNED;

	return 0;
out:
	ch_end(el);
	return -1;
}

/* ch_reset():
 *	Reset the character editor
 */
libedit_private void
ch_reset(EditLine *el)
{
	el->el_line.cursor		= el->el_line.buffer;
	el->el_line.lastchar		= el->el_line.buffer;

	c_undo_reset(el);

	el->el_chared.c_vcmd.action	= NOP;
	el->el_chared.c_vcmd.pos	= el->el_line.buffer;

	el->el_chared.c_kill.mark	= el->el_line.buffer;

	el->el_map.current		= el->el_map.key;

	el->el_state.inputmode		= MODE_INSERT; /* XXX: save a default */
	el->el_state.doingarg		= 0;
	el->el_state.metanext		= 0;
	el->el_state.argument		= 1;
	el->el_state.lastcmd		= ED_UNASSIGNED;

	el->el_history.eventno		= 0;
}

/* ch_enlargebufs():
 *	Enlarge line buffer to be able to hold twice as much characters.
 *	Returns 1 if successful, 0 if not.
 */
libedit_private int
ch_enlargebufs(EditLine *el, size_t addlen)
{
	size_t sz, newsz;
	wchar_t *newbuffer, *oldbuf, *oldkbuf;

	sz = (size_t)(el->el_line.limit - el->el_line.buffer + EL_LEAVE);
	newsz = sz * 2;
	/*
	 * If newly required length is longer than current buffer, we need
	 * to make the buffer big enough to hold both old and new stuff.
	 */
	if (addlen > sz) {
		while(newsz - sz < addlen)
			newsz *= 2;
	}

	/*
	 * Reallocate line buffer.
	 */
	newbuffer = el_realloc(el->el_line.buffer, newsz * sizeof(*newbuffer));
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, (newsz - sz) * sizeof(*newbuffer));

	oldbuf = el->el_line.buffer;

	el->el_line.buffer = newbuffer;
	el->el_line.cursor = newbuffer + (el->el_line.cursor - oldbuf);
	el->el_line.lastchar = newbuffer + (el->el_line.lastchar - oldbuf);
	/* don't set new size until all buffers are enlarged */
	el->el_line.limit  = &newbuffer[sz - EL_LEAVE];

	/*
	 * Reallocate kill buffer.
	 */
	newbuffer = el_realloc(el->el_chared.c_kill.buf, newsz *
	    sizeof(*newbuffer));
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, (newsz - sz) * sizeof(*newbuffer));

	oldkbuf = el->el_chared.c_kill.buf;

	el->el_chared.c_kill.buf = newbuffer;
	el->el_chared.c_kill.last = newbuffer +
					(el->el_chared.c_kill.last - oldkbuf);
	el->el_chared.c_kill.mark = el->el_line.buffer +
					(el->el_chared.c_kill.mark - oldbuf);
	/* c_vcmd.pos is held across a dispatch iteration by cv_action(),
	 * and cv_delfini() dereferences it after the motion command may
	 * have grown the line. */
	if (el->el_chared.c_vcmd.pos != NULL)
		el->el_chared.c_vcmd.pos = el->el_line.buffer +
					(el->el_chared.c_vcmd.pos - oldbuf);

	/* The undo stack holds independent copies, so it needs no rebasing
	 * and no growing here. */

	newbuffer = el_realloc(el->el_chared.c_redo.buf,
	    newsz * sizeof(*newbuffer));
	if (!newbuffer)
		return 0;
	el->el_chared.c_redo.pos = newbuffer +
			(el->el_chared.c_redo.pos - el->el_chared.c_redo.buf);
	el->el_chared.c_redo.lim = newbuffer +
			(el->el_chared.c_redo.lim - el->el_chared.c_redo.buf);
	el->el_chared.c_redo.buf = newbuffer;

	if (!hist_enlargebuf(el, sz, newsz))
		return 0;

	/* Safe to set enlarged buffer size */
	el->el_line.limit  = &el->el_line.buffer[newsz - EL_LEAVE];
	if (el->el_chared.c_resizefun)
		(*el->el_chared.c_resizefun)(el, el->el_chared.c_resizearg);
	return 1;
}

/* ch_end():
 *	Free the data structures used by the editor
 */
libedit_private void
ch_end(EditLine *el)
{
	el_free(el->el_line.buffer);
	el->el_line.buffer = NULL;
	el->el_line.limit = NULL;
	c_undo_end(el);
	el_free(el->el_chared.c_redo.buf);
	el->el_chared.c_redo.buf = NULL;
	el->el_chared.c_redo.pos = NULL;
	el->el_chared.c_redo.lim = NULL;
	el->el_chared.c_redo.cmd = ED_UNASSIGNED;
	el_free(el->el_chared.c_kill.buf);
	el->el_chared.c_kill.buf = NULL;
	ch_reset(el);
}


/* el_insertstr():
 *	Insert string at cursor
 */
int
el_winsertstr(EditLine *el, const wchar_t *s)
{
	size_t len;

	if (s == NULL || (len = wcslen(s)) == 0)
		return -1;
	if (el->el_line.lastchar + len >= el->el_line.limit) {
		if (!ch_enlargebufs(el, len))
			return -1;
	}

	c_insert(el, (int)len);
	while (*s)
		*el->el_line.cursor++ = *s++;
	return 0;
}


/* el_deletestr():
 *	Delete num characters before the cursor
 */
void
el_deletestr(EditLine *el, int n)
{
	if (n <= 0)
		return;

	if (el->el_line.cursor < &el->el_line.buffer[n])
		return;

	c_delbefore(el, n);		/* delete before dot */
	el->el_line.cursor -= n;
	if (el->el_line.cursor < el->el_line.buffer)
		el->el_line.cursor = el->el_line.buffer;
}

/* el_deletestr1():
 *	Delete characters between start and end
 */
int
el_deletestr1(EditLine *el, int start, int end)
{
	size_t line_length, len;
	wchar_t *p1, *p2;

	if (end <= start)
		return 0;

	line_length = (size_t)(el->el_line.lastchar - el->el_line.buffer);

	if (start >= (int)line_length || end >= (int)line_length)
		return 0;

	len = (size_t)(end - start);
	if (len > line_length - (size_t)end)
		len = line_length - (size_t)end;

	p1 = el->el_line.buffer + start;
	p2 = el->el_line.buffer + end;
	for (size_t i = 0; i < len; i++) {
		*p1++ = *p2++;
		el->el_line.lastchar--;
	}

	if (el->el_line.cursor < el->el_line.buffer)
		el->el_line.cursor = el->el_line.buffer;

	return end - start;
}

/* el_wreplacestr():
 *	Replace the contents of the line with the provided string
 */
int
el_wreplacestr(EditLine *el, const wchar_t *s)
{
	size_t len;
	wchar_t * p;

	if (s == NULL || (len = wcslen(s)) == 0)
		return -1;

	if (el->el_line.buffer + len >= el->el_line.limit) {
		if (!ch_enlargebufs(el, len))
			return -1;
	}

	p = el->el_line.buffer;
	for (size_t i = 0; i < len; i++)
		*p++ = *s++;

	el->el_line.buffer[len] = '\0';
	el->el_line.lastchar = el->el_line.buffer + len;
	if (el->el_line.cursor > el->el_line.lastchar)
		el->el_line.cursor = el->el_line.lastchar;

	return 0;
}

/* el_cursor():
 *	Move the cursor to the left or the right of the current position
 */
int
el_cursor(EditLine *el, int n)
{
	if (n == 0)
		goto out;

	el->el_line.cursor += n;

	if (el->el_line.cursor < el->el_line.buffer)
		el->el_line.cursor = el->el_line.buffer;
	if (el->el_line.cursor > el->el_line.lastchar)
		el->el_line.cursor = el->el_line.lastchar;
out:
	return (int)(el->el_line.cursor - el->el_line.buffer);
}

/* c_gets():
 *	Get a string
 */
libedit_private int
c_gets(EditLine *el, wchar_t *buf, const wchar_t *prompt)
{
	ssize_t len;
	wchar_t *cp = el->el_line.buffer, ch;

	if (prompt) {
		len = (ssize_t)wcslen(prompt);
		(void)memcpy(cp, prompt, (size_t)len * sizeof(*cp));
		cp += len;
	}
	len = 0;

	for (;;) {
		el->el_line.cursor = cp;
		*cp = ' ';
		el->el_line.lastchar = cp + 1;
		re_refresh(el);

		if (el_wgetc(el, &ch) != 1) {
			ed_end_of_file(el, 0);
			len = -1;
			break;
		}

		switch (ch) {

		case L'\b':	/* Delete and backspace */
		case 0177:
			if (len == 0) {
				len = -1;
				break;
			}
			len--;
			cp--;
			continue;

		case 0033:	/* ESC */
		case L'\r':	/* Newline */
		case L'\n':
			buf[len] = ch;
			break;

		default:
			if (len >= (ssize_t)(EL_BUFSIZ - 16))
				terminal_beep(el);
			else {
				buf[len++] = ch;
				*cp++ = ch;
			}
			continue;
		}
		break;
	}

	el->el_line.buffer[0] = '\0';
	el->el_line.lastchar = el->el_line.buffer;
	el->el_line.cursor = el->el_line.buffer;
	return (int)len;
}


/* c_hpos():
 *	Return the current horizontal position of the cursor
 */
libedit_private int
c_hpos(EditLine *el)
{
	wchar_t *ptr;

	/*
	 * Find how many characters till the beginning of this line.
	 */
	if (el->el_line.cursor == el->el_line.buffer)
		return 0;
	else {
		for (ptr = el->el_line.cursor - 1;
		     ptr >= el->el_line.buffer && *ptr != '\n';
		     ptr--)
			continue;
		return (int)(el->el_line.cursor - ptr - 1);
	}
}

libedit_private int
ch_resizefun(EditLine *el, el_zfunc_t f, void *a)
{
	el->el_chared.c_resizefun = f;
	el->el_chared.c_resizearg = a;
	return 0;
}

libedit_private int
ch_aliasfun(EditLine *el, el_afunc_t f, void *a)
{
	el->el_chared.c_aliasfun = f;
	el->el_chared.c_aliasarg = a;
	return 0;
}
