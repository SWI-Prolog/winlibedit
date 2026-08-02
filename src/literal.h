/*	$NetBSD: literal.h,v 1.2 2017/06/30 20:26:52 kre Exp $	*/

/*-
 * Copyright (c) 2017 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * el.literal.h: Literal character
 */
#ifndef _h_el_literal
#define	_h_el_literal

/*
 * A literal occupies a single cell of the display buffer, holding a
 * "magic" character that terminal__putc() expands into the stored string.
 * The cells are wint_t, which is 16 bits on Windows: there the tag below
 * does not fit and we use the Unicode noncharacters U+FDD0..U+FDEF, which
 * cannot occur in text.  That limits the number of literals on one
 * screen; literal_add() refuses beyond EL_LITERAL_MAX.
 */
#if SIZEOF_WCHAR_T == 2
#define EL_LITERAL	((wint_t)0xfdd0)
#define EL_LITERAL_MAX	((size_t)0x20)
#else
#define EL_LITERAL	((wint_t)0x80000000)
#define EL_LITERAL_MAX	((size_t)0x40000000)
#endif

/* MB_FILL_CHAR is above the range in both cases */
#define EL_IS_LITERAL(c) \
	((wint_t)(c) >= EL_LITERAL && \
	 (wint_t)(c) < EL_LITERAL + (wint_t)EL_LITERAL_MAX)

typedef struct el_lit_t {
	char		*l_str;		/* the string to emit */
	int		 l_width;	/* visual columns it occupies */
} el_lit_t;

typedef struct el_literal_t {
	el_lit_t	*l_buf;		/* array of literals */
	size_t		l_idx;		/* max in use */
	size_t		l_len;		/* max allocated */
} el_literal_t;

libedit_private void literal_init(EditLine *);
libedit_private void literal_end(EditLine *);
libedit_private void literal_clear(EditLine *);
libedit_private wint_t literal_add(EditLine *, const wchar_t *,
    const wchar_t *, int *);
libedit_private const char *literal_get(EditLine *, wint_t);
libedit_private int literal_width(EditLine *, wint_t);

#endif /* _h_el_literal */
