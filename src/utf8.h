/*  Part of SWI-Prolog

    Author:        Jan Wielemaker and Anjo Anjewierden
    E-mail:        jan@swi.psy.uva.nl
    WWW:           http://www.swi-prolog.org
    Copyright (c)  2005-2011, University of Amsterdam
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef UTF8_H_INCLUDED
#define UTF8_H_INCLUDED

#define UTF8_MALFORMED_REPLACEMENT 0xfffd

#define ISUTF8_MB(c) ((unsigned)(c) >= 0xc0 && (unsigned)(c) <= 0xfd)

#define ISUTF8_CB(c)  (((c)&0xc0) == 0x80) /* Is continuation byte */
#define ISUTF8_FB2(c) (((c)&0xe0) == 0xc0)
#define ISUTF8_FB3(c) (((c)&0xf0) == 0xe0)
#define ISUTF8_FB4(c) (((c)&0xf8) == 0xf0)
#define ISUTF8_FB5(c) (((c)&0xfc) == 0xf8)
#define ISUTF8_FB6(c) (((c)&0xfe) == 0xfc)

#define UTF8_FBN(c) (!(c&0x80)     ? 0 : \
		     ISUTF8_FB2(c) ? 1 : \
		     ISUTF8_FB3(c) ? 2 : \
		     ISUTF8_FB4(c) ? 3 : \
		     ISUTF8_FB5(c) ? 4 : \
		     ISUTF8_FB6(c) ? 5 : -1)
#define UTF8_FBV(c,n) ( n == 0 ? c : (c & ((0x01<<(6-n))-1)) )

#define F_UTF8_GET_CHAR libedit_utf8_get_char
#define F_UTF8_PUT_CHAR libedit_utf8_put_char
#define F_UTF8_STRLEN   libedit_utf8_strlen
#define F_UTF8_ENCLENW	libedit_utf8_enclenW
#define F_UTF8_ENCLENA	libedit_utf8_enclenA

#define utf8_get_char(in, chr) \
	(*(in) & 0x80 ? F_UTF8_GET_CHAR(in, chr) \
		      : (*(chr) = *(in), (char *)(in)+1))
#define utf8_put_char(out, chr) \
	(chr < 0x80 ? out[0]=(char)chr, out+1 : F_UTF8_PUT_CHAR(out, chr))
#define libedit_utf8_strlen(s, len) F_UTF8_STRLEN(s, len)
#define libedit_utf8_enclenW(s, len) F_UTF8_ENCLENW(s, len)
#define libedit_utf8_enclenA(s, len) F_UTF8_ENCLENA(s, len)

extern char *F_UTF8_GET_CHAR(const char *in, int *chr);
extern char *F_UTF8_PUT_CHAR(char *out, int chr);

extern size_t F_UTF8_STRLEN(const char *s, size_t len);
extern size_t F_UTF8_ENCLENW(const wchar_t *s, size_t len);
extern size_t F_UTF8_ENCLENA(const char *s, size_t len);




		 /*******************************
		 *	      UTF-16		*
		 *******************************/

#include <stddef.h>			/* get wchar_t */

/* libedit's build system doesn't generate a SIZEOF_WCHAR_T define.
 * Fall back to the compiler-provided __SIZEOF_WCHAR_T__ (gcc, clang,
 * MSVC) so the `#if SIZEOF_WCHAR_T == 2` branches below actually fire
 * on Windows; without this they silently evaluate 0 == 2 and the
 * surrogate-pair paths are compiled out. */
#ifndef SIZEOF_WCHAR_T
#  ifdef __SIZEOF_WCHAR_T__
#    define SIZEOF_WCHAR_T __SIZEOF_WCHAR_T__
#  else
#    define SIZEOF_WCHAR_T 4	/* assume UCS-4 */
#  endif
#endif

/* See https://en.wikipedia.org/wiki/UTF-16#Examples */

#define IS_UTF16_LEAD(c)      ((c) >= 0xD800 && (c) <= 0xDBFF)
#define IS_UTF16_TRAIL(c)     ((c) >= 0xDC00 && (c) <= 0xDFFF)
#define IS_UTF16_SURROGATE(c) ((c) >= 0xD800 && (c) <= 0xDFFF)
#define VALID_CODE_POINT(c)   ((c) >= 0 && (c) <= UNICODE_MAX && !IS_UTF16_SURROGATE(c))

static inline int
utf16_decode(int lead, int trail)
{ int l = (lead-0xD800) << 10;
  int t = (trail-0xDC00);

  return l+t+0x10000;
}

static inline void
utf16_encode(int c, int *lp, int *tp)
{ c -= 0x10000;
  *lp = (c>>10)+0xD800;
  *tp = (c&0X3FF)+0xDC00;
}

static inline wchar_t*
utf16_put_char(wchar_t *out, int chr)
{ if ( chr <= 0xffff )
  { *out++ = chr;
  } else
  { int l, t;

    utf16_encode(chr, &l, &t);
    *out++ = l;
    *out++ = t;
  }

  return out;
}

static inline wchar_t*
put_wchar(wchar_t *out, int chr)
{
#if SIZEOF_WCHAR_T == 2
  return utf16_put_char(out, chr);
#else
  *out++ = chr;
  return out;
#endif
}

static inline const wchar_t*
get_wchar(const wchar_t *in, int *chr)
{
#if SIZEOF_WCHAR_T == 2
  int c = *in++;
  if ( IS_UTF16_LEAD(c) && IS_UTF16_TRAIL(in[0]) )
  { *chr = utf16_decode(c, in[0]);
    in++;
  } else
  { *chr = c;
  }
  return in;
#else
  *chr = *in++;
  return in;
#endif
}

		 /*******************************
		 * GRAPHEME-UNIT CODE-POINT API *
		 *******************************/

/* el_cp_at(p, end, &adv) returns the Unicode code point starting at *p,
 * using p[1] as a trail surrogate if needed when sizeof(wchar_t) == 2.
 * Advances by *adv wchar_t units (1 or 2).  On a lone or malformed
 * surrogate it yields the raw wchar_t value so nothing is silently
 * dropped.  On Linux (SIZEOF_WCHAR_T == 4) it is always a single-step
 * dereference.
 */
#include <wchar.h>
#include <wctype.h>

static inline int
el_cp_at(const wchar_t *p, const wchar_t *end, int *adv)
{
#if SIZEOF_WCHAR_T == 2
  int c = *p;
  if ( IS_UTF16_LEAD(c) && p+1 < end && IS_UTF16_TRAIL(p[1]) )
  { *adv = 2;
    return utf16_decode(c, p[1]);
  }
  *adv = 1;
  return c;
#else
  (void)end;
  *adv = 1;
  return (int)*p;
#endif
}

/* The width / class / printability of the code point starting at *p.
 * These always decode a surrogate pair when sizeof(wchar_t) == 2 so
 * wcwidth / iswprint / iswcntrl see a real code point rather than a
 * lone surrogate half.
 *
 * Callers that need wcwidth must include mk_wcwidth.h before this
 * header so the wrapper resolves to the libedit replacement (Windows
 * has no libc wcwidth). */
static inline int
el_iswprint_at(const wchar_t *p, const wchar_t *end)
{ int adv;
  int cp = el_cp_at(p, end, &adv);
  return iswprint((wchar_t)cp);
}

static inline int
el_iswcntrl_at(const wchar_t *p, const wchar_t *end)
{ int adv;
  int cp = el_cp_at(p, end, &adv);
  return iswcntrl((wchar_t)cp);
}

/* Number of wchar_t units the code point starting at *p occupies. */
static inline int
el_cp_width_wchars(const wchar_t *p, const wchar_t *end)
{
#if SIZEOF_WCHAR_T == 2
  if ( IS_UTF16_LEAD(*p) && p+1 < end && IS_UTF16_TRAIL(p[1]) )
    return 2;
#else
  (void)p; (void)end;
#endif
  return 1;
}

#endif /*UTF8_H_INCLUDED*/
