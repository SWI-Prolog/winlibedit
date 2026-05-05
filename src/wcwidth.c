/*	$NetBSD: wcwidth.c,v 1.0 2026/05/05 SWI-Prolog $	*/

/*-
 * Copyright (c) 2026 SWI-Prolog
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES
 * ARE DISCLAIMED.
 */

/*
 * wcwidth.c: pluggable column-width function used throughout libedit.
 *
 * The bundled Markus Kuhn mk_wcwidth() table is gone.  Internal callers
 * see a wcwidth() macro that dispatches through libedit_wcwidth, which
 * defaults to:
 *   - the system wcwidth() when one is available (HAVE_WCWIDTH),
 *   - a minimal "1 if printable" fallback otherwise (Windows console).
 *
 * Embedders override this once at init via el_set(EL_WCWIDTH, fn) so
 * libedit's column tracking matches whatever Unicode table the host
 * uses elsewhere (in SWI-Prolog: PL_wcwidth, sharing the kernel's
 * src/mk_wcwidth.c with pl-write / pl-fmt / xpce).
 */

#include "config.h"
#include <wchar.h>

#include "el.h"

/* The macro from el.h would otherwise turn the call below into a
 * recursive jump into libedit_wcwidth.  Drop it locally so this file
 * sees the real system wcwidth(). */
#undef wcwidth

libedit_private int
default_wcwidth(int c)
{
#ifdef HAVE_WCWIDTH
	return wcwidth((wchar_t)c);
#else
	if (c == 0)
		return 0;
	if (c < 0x20 || c == 0x7F)
		return -1;
	return 1;
#endif
}

el_wcwfunc_t libedit_wcwidth = default_wcwidth;
