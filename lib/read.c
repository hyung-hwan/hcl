/*
 * $Id$
 *
    Copyright (c) 2016-2018 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hcl-prv.h"

static int begin_include (hcl_t* hcl);
static int end_include (hcl_t* hcl);

#define BUFFER_ALIGN 128
#define BALIT_BUFFER_ALIGN 128
#define SALIT_BUFFER_ALIGN 128
#define ARLIT_BUFFER_ALIGN 128

#define CHAR_TO_NUM(c,base) \
	((c >= '0' && c <= '9')? ((c - '0' < base)? (c - '0'): base): \
	 (c >= 'A' && c <= 'Z')? ((c - 'A' + 10 < base)? (c - 'A' + 10): base): \
	 (c >= 'a' && c <= 'z')? ((c - 'a' + 10 < base)? (c - 'a' + 10): base): base)

static struct voca_t
{
	hcl_oow_t len;
	hcl_ooch_t str[11];
} vocas[] =
{
	{  8, { '#','i','n','c','l','u','d','e'                               } },
	{  7, { '#','p','r','a','g','m','a'                                   } },
	{ 11, { '#','\\','b','a','c','k','s','p','a','c','e'                  } },
	{ 10, { '#','\\','l','i','n','e','f','e','e','d'                      } },
	{  9, { '#','\\','n','e','w','l','i','n','e'                          } },
	{  5, { '#','\\','n','u','l'                                          } },
	{  6, { '#','\\','p','a','g','e'                                      } },
	{  8, { '#','\\','r','e','t','u','r','n'                              } },
	{  8, { '#','\\','r','u','b','o','u','t'                              } },
	{  7, { '#','\\','s','p','a','c','e'                                  } },
	{  5, { '#','\\','t','a','b'                                          } },
	{  6, { '#','\\','v','t','a','b'                                      } },
	{  5, { '<','E','O','L','>'                                           } },
	{  5, { '<','E','O','F','>'                                           } }
};

enum voca_id_t
{
	VOCA_INCLUDE,
	VOCA_PRAGMA,

	VOCA_BACKSPACE,
	VOCA_LINEFEED,
	VOCA_NEWLINE,
	VOCA_NUL,
	VOCA_PAGE,
	VOCA_RETURN,
	VOCA_RUBOUT,
	VOCA_SPACE,
	VOCA_TAB,
	VOCA_VTAB,

	VOCA_EOL,
	VOCA_EOF
};
typedef enum voca_id_t voca_id_t;


enum list_flag_t
{
	QUOTED     = (1 << 0),
	DOTTED     = (1 << 1),
	COMMAED    = (1 << 2),
	COLONED    = (1 << 3),
	CLOSED     = (1 << 4),
	JSON       = (1 << 5),
	DATA_LIST  = (1 << 6)
};

#define LIST_FLAG_GET_CONCODE(x) (((x) >> 8) & 0xFF)
#define LIST_FLAG_SET_CONCODE(x,type) ((x) = ((x) & ~0xFF00) | ((type) << 8))


static int string_to_ooi (hcl_t* hcl, hcl_oocs_t* str, int radixed, hcl_ooi_t* num)
{
	/* it is not a generic conversion function.
	 * it assumes a certain pre-sanity check on the string
	 * done by the lexical analyzer */

	int v, negsign, base;
	const hcl_ooch_t* ptr, * end;
	hcl_oow_t value, old_value;

	negsign = 0;
	ptr = str->ptr,
	end = str->ptr + str->len;

	HCL_ASSERT (hcl, ptr < end);

	if (*ptr == '+' || *ptr == '-')
	{
		negsign = *ptr - '+';
		ptr++;
	}

	if (radixed)
	{
		HCL_ASSERT (hcl, ptr < end);

		if (*ptr != '#')
		{
			hcl_seterrbfmt (hcl, HCL_EINVAL, "radixed number not starting with # - %*.js", str->len, str->ptr);
			return -1;
		}
		ptr++; /* skip '#' */

		if (*ptr == 'x') base = 16;
		else if (*ptr == 'o') base = 8;
		else if (*ptr == 'b') base = 2;
		else
		{
			hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid radix specifier - %c", *ptr);
			return -1;
		}
		ptr++;
	}
	else base = 10;

	HCL_ASSERT (hcl, ptr < end);

	value = old_value = 0;
	while (ptr < end && (v = CHAR_TO_NUM(*ptr, base)) < base)
	{
		value = value * base + v;
		if (value < old_value)
		{
			/* overflow must have occurred */
			hcl_seterrbfmt (hcl, HCL_ERANGE, "number too big - %.*js", str->len, str->ptr);
			return -1;
		}
		old_value = value;
		ptr++;
	}

	if (ptr < end)
	{
		/* trailing garbage? */
		hcl_seterrbfmt (hcl, HCL_EINVAL, "trailing garbage after numeric literal - %.*js", str->len, str->ptr);
		return -1;
	}

	if (value > HCL_TYPE_MAX(hcl_ooi_t) + (negsign? 1: 0)) /* assume 2's complement */
	{
		hcl_seterrbfmt (hcl, HCL_ERANGE, "number too big - %.*js", str->len, str->ptr);
		return -1;
	}

	*num = value;
	if (negsign) *num *= -1;

	return 0;
}

static hcl_oop_t string_to_num (hcl_t* hcl, hcl_oocs_t* str, int radixed)
{
	int negsign, base;
	const hcl_ooch_t* ptr, * end;

	negsign = 0;
	ptr = str->ptr,
	end = str->ptr + str->len;

	HCL_ASSERT (hcl, ptr < end);

	if (*ptr == '+' || *ptr == '-')
	{
		negsign = *ptr - '+';
		ptr++;
	}

#if 0
	if (radixed)
	{
		HCL_ASSERT (hcl, ptr < end);

		base = 0;
		do
		{
			base = base * 10 + CHAR_TO_NUM(*ptr, 10);
			ptr++;
		}
		while (*ptr != 'r');

		ptr++;
	}
	else base = 10;
#else
	if (radixed)
	{
		HCL_ASSERT (hcl, ptr < end);

		if (*ptr != '#')
		{
			hcl_seterrbfmt(hcl, HCL_EINVAL, "radixed number not starting with # - %.*js", str->len, str->ptr);
			return HCL_NULL;
		}
		ptr++; /* skip '#' */

		if (*ptr == 'x') base = 16;
		else if (*ptr == 'o') base = 8;
		else if (*ptr == 'b') base = 2;
		else
		{
			hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid radix specifier - %c", *ptr);
			return HCL_NULL;
		}
		ptr++;
	}
	else base = 10;
#endif

/* TODO: handle floating point numbers ... etc */
	if (negsign) base = -base;
	return hcl_strtoint(hcl, ptr, end - ptr, base);
}

static hcl_oop_t string_to_fpdec (hcl_t* hcl, hcl_oocs_t* str, const hcl_ioloc_t* loc)
{
	hcl_oow_t pos;
	hcl_oow_t scale = 0;
	hcl_oop_t v;

	pos = str->len;
	while (pos > 0)
	{
		pos--;
		if (str->ptr[pos] == '.')
		{
			scale = str->len - pos - 1;
			if (scale > HCL_SMOOI_MAX)
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMRANGE, loc, str, "too many digits after decimal point");
				return HCL_NULL;
			}

			HCL_ASSERT (hcl, scale > 0);
			/*if (scale > 0)*/ HCL_MEMMOVE (&str->ptr[pos], &str->ptr[pos + 1], scale * HCL_SIZEOF(str->ptr[0])); /* remove the decimal point */
			break;
		}
	}

	/* if no decimal point is included or no digit after the point , you must not call this function */
	HCL_ASSERT (hcl, scale > 0);

	v = hcl_strtoint(hcl, str->ptr, str->len - 1, 10);
	if (!v) return HCL_NULL;

	return hcl_makefpdec(hcl, v, scale);
}

static HCL_INLINE int is_spacechar (hcl_ooci_t c)
{
	/* TODO: handle other space unicode characters */
	switch (c)
	{
		case ' ':
		case '\f': /* formfeed */
		case '\n': /* linefeed */
		case '\r': /* carriage return */
		case '\t': /* horizon tab */
		case '\v': /* vertical tab */
			return 1;

		default:
			return 0;
	}
}

static HCL_INLINE int is_linebreak (hcl_ooci_t c)
{
	/* TODO: different line end conventions? */
	return c == '\n';
}

static HCL_INLINE int is_alphachar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static HCL_INLINE int is_digitchar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= '0' && c <= '9');
}

static HCL_INLINE int is_xdigitchar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static HCL_INLINE int is_alnumchar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static HCL_INLINE int is_delimchar (hcl_ooci_t c)
{
	return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
	       c == ';' || c == '|' || c == ',' || c == '.' || c == ':' || 
	       /* the first characters of tokens in delim_token_tab up to this point */
	       c == '#'  || c == '\"' || c == '\'' || is_spacechar(c) || c == HCL_UCI_EOF;
}

static int copy_string_to (hcl_t* hcl, const hcl_oocs_t* src, hcl_oocs_t* dst, hcl_oow_t* dst_capa, int append, hcl_ooch_t add_delim)
{
	hcl_oow_t len, pos;

	if (append)
	{
		pos = dst->len;
		len = dst->len + src->len;
		if (add_delim != '\0') len++;
	}
	else
	{
		pos = 0;
		len = src->len;
	}

	if (len > *dst_capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t capa;

		capa = HCL_ALIGN(len, BUFFER_ALIGN);

		tmp = (hcl_ooch_t*)hcl_reallocmem(hcl, dst->ptr, HCL_SIZEOF(*tmp) * capa);
		if (HCL_UNLIKELY(!tmp)) return -1;

		dst->ptr = tmp;
		*dst_capa = capa;
	}

	if (append && add_delim) dst->ptr[pos++] = add_delim;
	hcl_copy_oochars (&dst->ptr[pos], src->ptr, src->len);
	dst->len = len;
	return 0;
}

#define GET_CHAR(hcl) \
	do { if (get_char(hcl) <= -1) return -1; } while (0)

#define GET_CHAR_TO(hcl,c) \
	do { \
		if (get_char(hcl) <= -1) return -1; \
		c = (hcl)->c->lxc.c; \
	} while(0)


#define GET_TOKEN(hcl) \
	do { if (get_token(hcl) <= -1) return -1; } while (0)

#define GET_TOKEN_WITH_ERRRET(hcl, v_ret) \
	do { if (get_token(hcl) <= -1) return v_ret; } while (0)

#define GET_TOKEN_WITH_GOTO(hcl, goto_label) \
	do { if (get_token(hcl) <= -1) goto goto_label; } while (0)

#define ADD_TOKEN_STR(hcl,s,l) \
	do { if (add_token_str(hcl, s, l) <= -1) return -1; } while (0)

#define ADD_TOKEN_CHAR(hcl,c) \
	do { if (add_token_char(hcl, c) <= -1) return -1; } while (0)

#define CLEAR_TOKEN_NAME(hcl) ((hcl)->c->tok.name.len = 0)
#define SET_TOKEN_TYPE(hcl,tv) ((hcl)->c->tok.type = (tv))
#define SET_TOKEN_LOC(hcl,locv) ((hcl)->c->tok.loc = *(locv))

#define TOKEN_TYPE(hcl) ((hcl)->c->tok.type)
#define TOKEN_NAME(hcl) (&(hcl)->c->tok.name)
#define TOKEN_NAME_CAPA(hcl) ((hcl)->c->tok.name_capa)
#define TOKEN_NAME_LEN(hcl) ((hcl)->c->tok.name.len)
#define TOKEN_NAME_PTR(hcl) ((hcl)->c->tok.name.ptr)
#define TOKEN_NAME_CHAR(hcl,index) ((hcl)->c->tok.name.ptr[index])
#define TOKEN_LOC(hcl) (&(hcl)->c->tok.loc)
#define LEXER_LOC(hcl) (&(hcl)->c->lxc.l)

static HCL_INLINE int add_token_str (hcl_t* hcl, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_oocs_t tmp;
	tmp.ptr = (hcl_ooch_t*)ptr;
	tmp.len = len;
	return copy_string_to(hcl, &tmp, TOKEN_NAME(hcl), &TOKEN_NAME_CAPA(hcl), 1, '\0');
}

static HCL_INLINE int does_token_name_match (hcl_t* hcl, voca_id_t id)
{
	return hcl->c->tok.name.len == vocas[id].len &&
	       hcl_equal_oochars(hcl->c->tok.name.ptr, vocas[id].str, vocas[id].len);
}

static HCL_INLINE int add_token_char (hcl_t* hcl, hcl_ooch_t c)
{
	hcl_oocs_t tmp;

	tmp.ptr = &c;
	tmp.len = 1;
	return copy_string_to(hcl, &tmp, TOKEN_NAME(hcl), &TOKEN_NAME_CAPA(hcl), 1, '\0');
}

static HCL_INLINE void unget_char (hcl_t* hcl, const hcl_iolxc_t* c)
{
	/* Make sure that the unget buffer is large enough */
	HCL_ASSERT (hcl, hcl->c->nungots < HCL_COUNTOF(hcl->c->ungot));
	hcl->c->ungot[hcl->c->nungots++] = *c;
}

static int get_directive_token_type (hcl_t* hcl, hcl_iotok_type_t* tok_type)
{
	if (does_token_name_match(hcl, VOCA_INCLUDE)) 
	{
		*tok_type = HCL_IOTOK_INCLUDE;
		return 0;
	}
	else if (does_token_name_match(hcl, VOCA_PRAGMA)) 
	{
		*tok_type = HCL_IOTOK_PRAGMA;
		return 0;
	}

	return -1;
}

static int _get_char (hcl_t* hcl, hcl_ioinarg_t* inp)
{
	hcl_ooci_t lc;

	if (inp->b.pos >= inp->b.len)
	{
		if (hcl->c->reader(hcl, HCL_IO_READ, inp) <= -1) return -1;

		if (inp->xlen <= 0)
		{
			inp->lxc.c = HCL_OOCI_EOF;
			inp->lxc.l.line = inp->line;
			inp->lxc.l.colm = inp->colm;
			inp->lxc.l.file = inp->name;
			/* indicate that EOF has been read. lxc.c is also set to EOF. */
			return 0;
		}

		inp->b.pos = 0;
		inp->b.len = inp->xlen;
	}

	if (inp->lxc.c == '\n' || inp->lxc.c == '\r')
	{
		/* inp->lxc.c is a previous character. the new character
		 * to be read is still in the buffer (inp->buf).
		 * hcl->cu->curinp->colm has been incremented when the previous
		 * character has been read. */
		if (inp->line > 1 && inp->colm == 2 && inp->nl != inp->lxc.c)
		{
			/* most likely, it's the second character in '\r\n' or '\n\r'
			 * sequence. let's not update the line and column number. */
			/*inp->colm = 1;*/
		}
		else
		{
			/* if the previous charater was a newline,
			 * increment the line counter and reset column to 1.
			 * incrementing the line number here instead of
			 * updating inp->lxc causes the line number for
			 * TOK_EOF to be the same line as the lxc newline. */
			inp->line++;
			inp->colm = 1;
			inp->nl = inp->lxc.c;
		}
	}

	lc = inp->buf[inp->b.pos++];

	inp->lxc.c = lc;
	inp->lxc.l.line = inp->line;
	inp->lxc.l.colm = inp->colm++;
	inp->lxc.l.file = inp->name;

	return 1; /* indicate that a normal character has been read */
}

static int get_char (hcl_t* hcl)
{
	int n;

	if (hcl->c->nungots > 0)
	{
		/* something in the unget buffer */
		hcl->c->lxc = hcl->c->ungot[--hcl->c->nungots];
		return 0;
	}

	n = _get_char(hcl, hcl->c->curinp);
	if (n >= 0) hcl->c->lxc = hcl->c->curinp->lxc;
	return n;
}

static int skip_comment (hcl_t* hcl)
{
	hcl_ooci_t c = hcl->c->lxc.c;
	hcl_iolxc_t lc;

	if (c == ';') goto single_line_comment;
	if (c != '#') return 0; /* not a comment */

	/* attempt to handle #! or ## */

	lc = hcl->c->lxc; /* save the last character */
	GET_CHAR_TO (hcl, c); /* read a following character */

	if (c == '!' || c == '#')
	{
	single_line_comment:
		do
		{
			GET_CHAR_TO (hcl, c);
			if (c == HCL_OOCI_EOF)
			{
				break;
			}
			else if (c == '\r' || c == '\n')
			{
				GET_CHAR (hcl); /* keep the first meaningful character in lxc */
				break;
			}
		}
		while (1);

		return 1; /* single line comment led by ## or #! or ; */
	}

	/* unget the leading '#' */
	unget_char (hcl, &hcl->c->lxc);
	/* restore the previous state */
	hcl->c->lxc = lc;

	return 0;
}

static int get_string (hcl_t* hcl, hcl_ooch_t end_char, hcl_ooch_t esc_char, int regex, hcl_oow_t preescaped, hcl_synerrnum_t synerr_code)
{
	hcl_ooci_t c;
	hcl_oow_t escaped = preescaped;
	hcl_oow_t digit_count = 0;
	hcl_ooci_t c_acc = 0;

	SET_TOKEN_TYPE (hcl, HCL_IOTOK_STRLIT);

	while (1)
	{
		GET_CHAR_TO (hcl, c);

		if (c == HCL_OOCI_EOF)
		{
			hcl_setsynerr (hcl, synerr_code, TOKEN_LOC(hcl) /*LEXER_LOC(hcl)*/, HCL_NULL);
			return -1;
		}

		if (escaped == 3)
		{
			if (c >= '0' && c <= '7')
			{
				/* more octal digits */
				c_acc = c_acc * 8 + c - '0';
				digit_count++;
				if (digit_count >= escaped)
				{
					/* should i limit the max to 0xFF/0377?
					 * if (c_acc > 0377) c_acc = 0377;*/
					ADD_TOKEN_CHAR (hcl, c_acc);
					escaped = 0;
				}
				continue;
			}
			else
			{
				ADD_TOKEN_CHAR (hcl, c_acc);
				escaped = 0;
			}
		}
		else if (escaped == 2 || escaped == 4 || escaped == 8)
		{
			if (c >= '0' && c <= '9')
			{
				c_acc = c_acc * 16 + c - '0';
				digit_count++;
				if (digit_count >= escaped)
				{
					ADD_TOKEN_CHAR (hcl, c_acc);
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'A' && c <= 'F')
			{
				c_acc = c_acc * 16 + c - 'A' + 10;
				digit_count++;
				if (digit_count >= escaped)
				{
					ADD_TOKEN_CHAR (hcl, c_acc);
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'a' && c <= 'f')
			{
				c_acc = c_acc * 16 + c - 'a' + 10;
				digit_count++;
				if (digit_count >= escaped)
				{
					ADD_TOKEN_CHAR (hcl, c_acc);
					escaped = 0;
				}
				continue;
			}
			else
			{
				hcl_ooch_t rc;

				rc = (escaped == 2)? 'x':
				     (escaped == 4)? 'u': 'U';
				if (digit_count == 0)
					ADD_TOKEN_CHAR (hcl, rc);
				else ADD_TOKEN_CHAR (hcl, c_acc);

				escaped = 0;
			}
		}

		if (escaped == 0 && c == end_char)
		{
			/* terminating quote */
			/* GET_CHAR (hcl); */
			break;
		}

		if (escaped == 0 && c == esc_char)
		{
			escaped = 1;
			continue;
		}

		if (escaped == 1)
		{
			if (c == 'a') c = '\a';
			else if (c == 'b') c = '\b';
			else if (c == 'f') c = '\f';
			else if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
			else if (c == 't') c = '\t';
			else if (c == 'v') c = '\v';
			else if (c >= '0' && c <= '7' && !regex)
			{
				/* i don't support the octal notation for a regular expression.
				 * it conflicts with the backreference notation between \1 and \7 inclusive. */
				escaped = 3;
				digit_count = 1;
				c_acc = c - '0';
				continue;
			}
			else if (c == 'x')
			{
				escaped = 2;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
		#if (HCL_SIZEOF_OOCH_T >= 2)
			else if (c == 'u')
			{
				escaped = 4;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
		#endif
		#if (HCL_SIZEOF_OOCH_T >= 4)
			else if (c == 'U')
			{
				escaped = 8;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
		#endif
			else if (regex)
			{
				/* if the following character doesn't compose a proper
				 * escape sequence, keep the escape character.
				 * an unhandled escape sequence can be handled
				 * outside this function since the escape character
				 * is preserved.*/
				ADD_TOKEN_CHAR (hcl, esc_char);
			}

			escaped = 0;
		}

		ADD_TOKEN_CHAR (hcl, c);
	}

	return 0;
}

static int get_radixed_number (hcl_t* hcl, hcl_ooci_t rc, int radix)
{
	hcl_ooci_t c;

	ADD_TOKEN_CHAR (hcl, '#');
	ADD_TOKEN_CHAR (hcl, rc);

	GET_CHAR_TO (hcl, c);

	if (CHAR_TO_NUM(c, radix) >= radix)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
			"no digit after radix specifier in %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
		return -1;
	}

	do
	{
		ADD_TOKEN_CHAR(hcl, c);
		GET_CHAR_TO (hcl, c);
	}
	while (CHAR_TO_NUM(c, radix) < radix);

	if (!is_delimchar(c))
	{
		/* collect more characters to form a complete token for the error message below */
		do
		{
			ADD_TOKEN_CHAR(hcl, c);
			GET_CHAR_TO (hcl, c);
		}
		while (!is_delimchar(c));

		hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
			"invalid digit in radixed number in %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
		return -1;
	}

	unget_char (hcl, &hcl->c->lxc);
	SET_TOKEN_TYPE (hcl, HCL_IOTOK_RADNUMLIT);

	return 0;
}

static int get_hmarked_token (hcl_t* hcl)
{
	hcl_ooci_t c;
	int radix;

	HCL_ASSERT (hcl, hcl->c->lxc.c == '#');

	GET_CHAR_TO (hcl, c);

	/*
	 * #xXXXX hexadecimal
	 * #oOOOO octal
	 * #bBBBB binary
	 * #eDDD   error
	 * #pHHH   smptr
	 * #\C      character
	 * #\xHHHH  unicode character
	 * #\UHHHH  unicode character
	 * #\uHHHH  unicode character
	 * #\backspace
	 * #\linefeed
	 * #\newline
	 * #\nul
	 * #\page
	 * #\return
	 * #\rubout
	 * #\space
	 * #\tab
	 * #\vtab
	 * #include
	 * #[ ]     byte array
	 * #( )     qlist
	 */

	switch (c)
	{
		case 'x':
			radix = 16;
			goto radixnum;
		case 'o':
			radix = 8;
			goto radixnum;
		case 'b':
			radix = 2;
		radixnum:
			if (get_radixed_number (hcl, c, radix) <= -1) return -1;
			break;

		case 'e':
			if (get_radixed_number(hcl, c, 10) <= -1) return -1;
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_ERRLIT);
			break;

		case 'p':
			if (get_radixed_number(hcl, c, 16) <= -1) return -1;
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_SMPTRLIT);
			break;

		case '\\': /* character literal */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR (hcl, '\\');

			GET_CHAR_TO (hcl, c);
			if (is_delimchar(c))
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
					"no valid character after #\\ in %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
				return -1;
			}

			SET_TOKEN_TYPE (hcl, HCL_IOTOK_CHARLIT);
			do
			{
				ADD_TOKEN_CHAR (hcl, c);
				GET_CHAR_TO (hcl, c);
			}
			while (!is_delimchar(c));

			if (TOKEN_NAME_LEN(hcl) >= 4)
			{
				int max_digit_count = 0;

				if (TOKEN_NAME_CHAR(hcl, 2) == 'x')
				{
					hcl_oow_t i;
					max_digit_count = 2;

				hexcharlit:
					if (TOKEN_NAME_LEN(hcl) - 3 > max_digit_count)
					{
						hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
							"invalid hexadecimal character in %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
						return -1;
					}
					c = 0;
					for (i = 3; i < TOKEN_NAME_LEN(hcl); i++)
					{
						if (!is_xdigitchar(TOKEN_NAME_CHAR(hcl, i)))
						{
							hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
								"invalid hexadecimal character in %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
							return -1;
						}
						c = c * 16 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 16); /* don't care if it is for 'p' */
					}
				}
			#if (HCL_SIZEOF_OOCH_T >= 2)
				else if (TOKEN_NAME_CHAR(hcl, 2) == 'u')
				{
					max_digit_count = 4;
					goto hexcharlit;
				}
			#endif
			#if (HCL_SIZEOF_OOCH_T >= 4)
				else if (TOKEN_NAME_CHAR(hcl, 2) == 'U')
				{
					max_digit_count = 8;
					goto hexcharlit;
				}
			#endif
				else if (does_token_name_match(hcl, VOCA_BACKSPACE))
				{
					c = '\b';
				}
				else if (does_token_name_match(hcl, VOCA_LINEFEED))
				{
					c = '\n';
				}
				else if (does_token_name_match(hcl, VOCA_NEWLINE))
				{
					/* TODO: convert it to host newline convention. how to handle if it's composed of 2 letters like \r\n? */
					c = '\n';
				}
				else if (does_token_name_match(hcl, VOCA_NUL)) /* null character. not #nil */
				{
					c = '\0';
				}
				else if (does_token_name_match(hcl, VOCA_PAGE))
				{
					c = '\f';
				}
				else if (does_token_name_match(hcl, VOCA_RETURN))
				{
					c = '\r';
				}
				else if (does_token_name_match(hcl, VOCA_RUBOUT))
				{
					c = '\x7F'; /* DEL */
				}
				else if (does_token_name_match(hcl, VOCA_SPACE))
				{
					c = ' ';
				}
				else if (does_token_name_match(hcl, VOCA_TAB))
				{
					c = '\t';
				}
				else if (does_token_name_match(hcl, VOCA_VTAB))
				{
					c = '\v';
				}
				else
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
						"invalid character literal %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
					return -1;
				}
			}
			else
			{
				HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) == 3);
				c = TOKEN_NAME_CHAR(hcl, 2);
			}

			/* reset the token name to the converted character */
			if (hcl->c->tok.type == HCL_IOTOK_CHARLIT)
			{
				CLEAR_TOKEN_NAME (hcl);
				ADD_TOKEN_CHAR (hcl, c);
			}

			unget_char (hcl, &hcl->c->lxc);
			break;

		case '[': /* #[ - byte array opener */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_BAPAREN);
			break;

		case '(': /* #( - qlist opener */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_QLPAREN);
			break;

		default:
		{
			hcl_iotok_type_t tok_type;

			if (is_delimchar(c))
			{
				/* EOF, whitespace, etc */
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_HASHLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
					"invalid hash-marked literal #%jc", c);
				return -1;
			}

			ADD_TOKEN_CHAR (hcl, '#');
			do
			{
				ADD_TOKEN_CHAR (hcl, c);
				GET_CHAR_TO (hcl, c);
			}
			while (!is_delimchar(c));

			if (get_directive_token_type(hcl, &tok_type) <= -1)
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_HASHLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
					"invalid hash-marked literal %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
				return -1;
			}
			else
			{
				SET_TOKEN_TYPE (hcl, tok_type);
			}

			unget_char (hcl, &hcl->c->lxc);
			break;
		}
	}

	return 0;
}

static hcl_iotok_type_t classify_ident_token (hcl_t* hcl, const hcl_oocs_t* v)
{
	hcl_oow_t i;
	struct
	{
		hcl_oow_t len;
		hcl_ooch_t name[10];
		hcl_iotok_type_t type;
	} tab[] =
	{
		{ 4, { 'n','u','l','l' },     HCL_IOTOK_NIL },
		{ 4, { 't','r','u','e' },     HCL_IOTOK_TRUE },
		{ 5, { 'f','a','l','s','e' }, HCL_IOTOK_FALSE },
		{ 4, { 's','e','l','f' },     HCL_IOTOK_SELF },
		{ 5, { 's','u','p','e','r' }, HCL_IOTOK_SUPER }
	};

	for (i = 0; i < HCL_COUNTOF(tab); i++)
	{
		if (hcl_comp_oochars(v->ptr, v->len, tab[i].name, tab[i].len) == 0) return tab[i].type;
	}

	return HCL_IOTOK_IDENT;
}

static int get_token (hcl_t* hcl)
{
	hcl_ooci_t c, oldc;
	int n;

retry:
	GET_CHAR (hcl);

	do
	{
		/* skip spaces */
		while (is_spacechar(hcl->c->lxc.c)) GET_CHAR (hcl);
		/* the first character after the last space is in hcl->c->lxc */
		if ((n = skip_comment(hcl)) <= -1) return -1;
	}
	while (n >= 1);

	/* clear the token name, reset its location */
	SET_TOKEN_TYPE (hcl, HCL_IOTOK_EOF); /* is it correct? */
	CLEAR_TOKEN_NAME (hcl);
	SET_TOKEN_LOC (hcl, LEXER_LOC(hcl));

	c = hcl->c->lxc.c;

	switch (c)
	{
		case HCL_OOCI_EOF:
		{
			int n;

			n = end_include(hcl);
			if (n <= -1) return -1;
			if (n >= 1) goto retry;

			SET_TOKEN_TYPE (hcl, HCL_IOTOK_EOF);
			ADD_TOKEN_STR(hcl, vocas[VOCA_EOF].str, vocas[VOCA_EOF].len);
			break;
		}

		case '(':
			oldc = c;
			GET_CHAR_TO (hcl, c);
			if(c == ':')
			{
				SET_TOKEN_TYPE (hcl, HCL_IOTOK_LPARCOLON);
				ADD_TOKEN_CHAR (hcl, oldc);
				ADD_TOKEN_CHAR (hcl, c);
				break;
			}
			else
			{
				unget_char (hcl, &hcl->c->lxc);
			}
			c = oldc;

			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_LPAREN);
			break;

		case ')':
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_RPAREN);
			break;

		case '[':
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_LBRACK);
			break;

		case ']':
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_RBRACK);
			break;

		case '{':
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_LBRACE);
			break;

		case '}':
			ADD_TOKEN_CHAR (hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_RBRACE);
			break;

		case '|':
			ADD_TOKEN_CHAR (hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_VBAR);
			break;

		case '.':
			oldc = c;
			GET_CHAR_TO (hcl, c);
			if(c == '.')
			{
				hcl_iolxc_t sd;
				hcl_ooci_t oldc2;

				sd = hcl->c->lxc; 

				oldc2 = c;
				GET_CHAR_TO (hcl, c);
				if (c == '.')
				{
					SET_TOKEN_TYPE (hcl, HCL_IOTOK_ELLIPSIS);
					ADD_TOKEN_CHAR (hcl, oldc);
					ADD_TOKEN_CHAR (hcl, oldc2);
					ADD_TOKEN_CHAR (hcl, c);
					break;
				}

				unget_char (hcl, &hcl->c->lxc);
				unget_char (hcl, &sd);
			}
			else
			{
				unget_char (hcl, &hcl->c->lxc);
			}
			c = oldc;

			SET_TOKEN_TYPE (hcl, HCL_IOTOK_DOT);
			ADD_TOKEN_CHAR (hcl, c);
			break;

		case ',':
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_COMMA);
			ADD_TOKEN_CHAR (hcl, c);
			break;

		case ':':
			oldc = c;
			GET_CHAR_TO (hcl, c);
			if(c == ':')
			{
				hcl_iolxc_t sd;
				hcl_ooci_t oldc2;

				sd = hcl->c->lxc;

				oldc2 = c;
				GET_CHAR_TO (hcl, c);
				if (c == ':')
				{
					SET_TOKEN_TYPE (hcl, HCL_IOTOK_TRPCOLONS);
					ADD_TOKEN_CHAR (hcl, oldc);
					ADD_TOKEN_CHAR (hcl, oldc2);
					ADD_TOKEN_CHAR (hcl, c);
					break;
				}
				else if (c == '*')
				{
					/* double-cloned star */
					SET_TOKEN_TYPE (hcl, HCL_IOTOK_DCSTAR);
					ADD_TOKEN_CHAR (hcl, oldc);
					ADD_TOKEN_CHAR (hcl, oldc2);
					ADD_TOKEN_CHAR (hcl, c);
					break;
				}

				unget_char (hcl, &hcl->c->lxc);
				unget_char (hcl, &sd);
			}
			else
			{
				unget_char (hcl, &hcl->c->lxc);
			}
			c = oldc;

			SET_TOKEN_TYPE (hcl, HCL_IOTOK_COLON);
			ADD_TOKEN_CHAR (hcl, c);
			break;

		case '\"':
			if (get_string(hcl, '\"', '\\', 0, 0, HCL_SYNERR_STRLIT) <= -1) return -1;
			break;

		case '\'':
			if (get_string(hcl, '\'', '\\', 0, 0, HCL_SYNERR_CHARLIT) <= -1) return -1;
			if (TOKEN_NAME_LEN(hcl) != 1)
			{
				hcl_setsynerr (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				return -1;
			}
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_CHARLIT);
			break;

		case '#':
			if (get_hmarked_token(hcl) <= -1) return -1;
			break;

		case '+':
		case '-':
			oldc = c;
			GET_CHAR_TO (hcl, c);
			if(is_digitchar(c))
			{
				unget_char (hcl, &hcl->c->lxc);
				c = oldc;
				goto numlit;
			}
			else if (c == '#')
			{
				int radix;
				hcl_iolxc_t sharp;

				sharp = hcl->c->lxc; /* back up '#' */

				GET_CHAR_TO (hcl, c);
				switch (c)
				{
					case 'b':
						radix = 2;
						goto radnumlit;
					case 'o':
						radix = 8;
						goto radnumlit;
					case 'x':
						radix = 16;
					radnumlit:
						ADD_TOKEN_CHAR (hcl, oldc);
						if (get_radixed_number(hcl, c, radix) <= -1) return -1;
						break;

					default:
						unget_char (hcl, &hcl->c->lxc);
						unget_char (hcl, &sharp);
						c = oldc;
						goto ident;
				}
			}
			else
			{
				unget_char (hcl, &hcl->c->lxc);
				c = oldc;
				goto ident;
			}
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		numlit:
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_NUMLIT);
			while (1)
			{
				ADD_TOKEN_CHAR (hcl, c);
				GET_CHAR_TO (hcl, c);
				if (TOKEN_TYPE(hcl) == HCL_IOTOK_NUMLIT && c == '.')
				{
					SET_TOKEN_TYPE (hcl, HCL_IOTOK_FPDECLIT);
					ADD_TOKEN_CHAR (hcl, c);
					GET_CHAR_TO (hcl, c);
					if (!is_digitchar(c))
					{
						/* the first character after the decimal point is not a decimal digit */
						hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl), "invalid numeric literal with no digit after decimal point");
						return -1;
					}
				}

				if (!is_digitchar(c))
				{
					unget_char (hcl, &hcl->c->lxc);
					break;
				}
			}

			break;

		default:
		ident:
			if (is_delimchar(c))
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ILCHR, TOKEN_LOC(hcl), HCL_NULL, "illegal character %jc encountered", c);
				return -1;
			}

			SET_TOKEN_TYPE (hcl, HCL_IOTOK_IDENT);
			while (1)
			{
				ADD_TOKEN_CHAR (hcl, c);
				GET_CHAR_TO (hcl, c);

				if (c == '.')
				{
					hcl_iolxc_t period;
					hcl_iotok_type_t type;

					type = classify_ident_token(hcl, TOKEN_NAME(hcl));
					if (type != HCL_IOTOK_IDENT)
					{
						SET_TOKEN_TYPE (hcl, type);
						unget_char (hcl, &hcl->c->lxc);
						break;
					}

					period = hcl->c->lxc;

				read_more_seg:
					GET_CHAR_TO (hcl, c);
					if (!is_delimchar(c))
					{
						hcl_oow_t start;
						hcl_oocs_t seg;

						SET_TOKEN_TYPE (hcl, HCL_IOTOK_IDENT_DOTTED);
						ADD_TOKEN_CHAR (hcl, '.');

						start = TOKEN_NAME_LEN(hcl);
						do
						{
							ADD_TOKEN_CHAR (hcl, c);
							GET_CHAR_TO (hcl, c);
						}
						while (!is_delimchar(c));

						seg.ptr = &TOKEN_NAME_CHAR(hcl,start);
						seg.len = TOKEN_NAME_LEN(hcl) - start;
						if (classify_ident_token(hcl, &seg) != HCL_IOTOK_IDENT)
						{
							hcl_setsynerr (hcl, HCL_SYNERR_MSEGIDENT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
							return -1;
						}

						if (c == '.') goto read_more_seg;

						unget_char (hcl, &hcl->c->lxc);
						break;
					}
					else
					{
						unget_char (hcl, &hcl->c->lxc);
						unget_char (hcl, &period);
					}
					break;
				}
				else if (is_delimchar(c))
				{
					unget_char (hcl, &hcl->c->lxc);
					break;
				}
			}

			if (TOKEN_TYPE(hcl) == HCL_IOTOK_IDENT)
			{
				hcl_iotok_type_t type;
				type = classify_ident_token(hcl, TOKEN_NAME(hcl));
				SET_TOKEN_TYPE (hcl, type);
			}
			break;
	}

#if defined(HCL_DEBUG_LEXER)
	HCL_DEBUG2 (hcl, "TOKEN: [%.*js]\n", (hcl_ooi_t)TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
#endif

	return 0;
}

static void clear_io_names (hcl_t* hcl)
{
	hcl_iolink_t* cur;

	HCL_ASSERT (hcl, hcl->c != HCL_NULL);

	while (hcl->c->io_names)
	{
		cur = hcl->c->io_names;
		hcl->c->io_names = cur->link;
		hcl_freemem (hcl, cur);
	}
}

static const hcl_ooch_t* add_io_name (hcl_t* hcl, const hcl_oocs_t* name)
{
	hcl_iolink_t* link;
	hcl_ooch_t* ptr;

	link = (hcl_iolink_t*)hcl_callocmem (hcl, HCL_SIZEOF(*link) + HCL_SIZEOF(hcl_ooch_t) * (name->len + 1));
	if (HCL_UNLIKELY(!link)) return HCL_NULL;

	ptr = (hcl_ooch_t*)(link + 1);

	hcl_copy_oochars (ptr, name->ptr, name->len);
	ptr[name->len] = '\0';

	link->link = hcl->c->io_names;
	hcl->c->io_names = link;

	return ptr;
}

/* -------------------------------------------------------------------------- */

static int begin_include (hcl_t* hcl)
{
	hcl_ioinarg_t* arg;
	const hcl_ooch_t* io_name;

	io_name = add_io_name(hcl, TOKEN_NAME(hcl));
	if (HCL_UNLIKELY(!io_name)) return -1;

	arg = (hcl_ioinarg_t*)hcl_callocmem(hcl, HCL_SIZEOF(*arg));
	if (HCL_UNLIKELY(!arg)) goto oops;

	arg->name = io_name;
	arg->line = 1;
	arg->colm = 1;
	/*arg->nl = '\0';*/
	arg->includer = hcl->c->curinp;

	if (hcl->c->reader(hcl, HCL_IO_OPEN, arg) <= -1)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_INCLUDE, TOKEN_LOC(hcl), TOKEN_NAME(hcl), "unable to include %js", io_name);
		goto oops;
	}

#if 0
	GET_TOKEN_WITH_GOTO (hcl, oops);
	if (TOKEN_TYPE(hcl) != HCL_IOTOK_DOT)
	{
		/* check if a period is following the includee name */
		hcl_setsynerr (hcl, HCL_SYNERR_PERIOD, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		goto oops;
	}
#endif

	/* switch to the includee's stream */
	hcl->c->curinp = arg;
	/* hcl->c->depth.incl++; */

	/* read in the first character in the included file.
	 * so the next call to get_token() sees the character read
	 * from this file. */
	if (get_token(hcl) <= -1)
	{
		end_include (hcl);
		/* i don't jump to oops since i've called
		 * end_include() which frees hcl->c->curinp/arg */
		return -1;
	}

	return 0;

oops:
	if (arg) hcl_freemem (hcl, arg);
	return -1;
}

static int end_include (hcl_t* hcl)
{
	int x;
	hcl_ioinarg_t* cur;

	if (hcl->c->curinp == &hcl->c->inarg) return 0; /* no include */

	/* if it is an included file, close it and
	 * retry to read a character from an outer file */

	x = hcl->c->reader(hcl, HCL_IO_CLOSE, hcl->c->curinp);

	/* if closing has failed, still destroy the
	 * sio structure first as normal and return
	 * the failure below. this way, the caller
	 * does not call HCL_IO_CLOSE on
	 * hcl->c->curinp again. */

	cur = hcl->c->curinp;
	hcl->c->curinp = hcl->c->curinp->includer;

	HCL_ASSERT (hcl, cur->name != HCL_NULL);
	hcl_freemem (hcl, cur);
	/* hcl->parse.depth.incl--; */

	if (x != 0)
	{
		/* the failure mentioned above is returned here */
		return -1;
	}

	hcl->c->lxc = hcl->c->curinp->lxc;
	return 1; /* ended the included file successfully */
}

static HCL_INLINE int enter_list (hcl_t* hcl, const hcl_ioloc_t* loc, int flagv)
{
	hcl_rstl_t* rstl;
	rstl = hcl_callocmem(hcl, HCL_SIZEOF(*rstl));
	if (HCL_UNLIKELY(!rstl)) return -1;
	rstl->loc = *loc;
	rstl->flagv = flagv;
	rstl->prev = hcl->c->r.st; /* push */
	hcl->c->r.st = rstl;
	return 0;
}

static HCL_INLINE hcl_cnode_t* leave_list (hcl_t* hcl, int* flagv, int* oldflagv)
{
	hcl_rstl_t* rstl;
	hcl_cnode_t* head;
	hcl_ioloc_t loc;
	int fv, concode;

	/* the stack must not be empty - cannot leave a list without entering it */
	HCL_ASSERT (hcl, hcl->c->r.st != HCL_NULL);
	rstl = hcl->c->r.st; /* get the stack top */

	head = rstl->head;
	fv = rstl->flagv;
	loc = rstl->loc;
	concode = LIST_FLAG_GET_CONCODE(fv);

	hcl->c->r.st = rstl->prev; /* pop off  */
	hcl_freemem (hcl, rstl); /* dispose of the stack node */

	if (fv & (COMMAED | COLONED))
	{
		hcl_setsynerr (hcl, ((fv & COMMAED)? HCL_SYNERR_COMMANOVALUE: HCL_SYNERR_COLONNOVALUE), TOKEN_LOC(hcl), HCL_NULL);
		if (head) hcl_freecnode (hcl, head);
		return HCL_NULL;
	}

	*oldflagv = fv;
	if (!hcl->c->r.st)
	{
		/* the stack is empty after popping.
		 * it is back to the top level.
		 * the top level can never be quoted. */
		*flagv = 0;
	}
	else
	{
		/* restore the flag for the outer returning level */
		*flagv = hcl->c->r.st->flagv;
	}

	/* NOTE: empty xlist will get translated to #nil.
	 *       this is useful when used in the lambda expression to express an empty argument. also in defun.
	 *      (lambda () ...) is equivalent to  (lambda #nil ...)
	 *      (defun x() ...) */

	if (head) 
	{
		HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(head));
		HCL_CNODE_CONS_CONCODE(head) = concode;
		return head;
	}

	/* the list is empty */
	return hcl_makecnodeelist(hcl, &loc, concode);
}

static HCL_INLINE int can_dot_list (hcl_t* hcl)
{
	hcl_rstl_t* rstl;

	HCL_ASSERT (hcl, hcl->c->r.st != HCL_NULL);
	rstl = hcl->c->r.st;

	/* mark the state that a dot has appeared in the list */
	if (rstl->count <= 0) return 0;
	if (LIST_FLAG_GET_CONCODE(rstl->flagv) != HCL_CONCODE_QLIST) return 0;

	rstl->flagv |= DOTTED;
	return 1;
}

static HCL_INLINE int can_comma_list (hcl_t* hcl)
{
	hcl_rstl_t* rstl;

	HCL_ASSERT (hcl, hcl->c->r.st != HCL_NULL);
	rstl = hcl->c->r.st;

	if (rstl->count <= 0) return 0;
	if (rstl->count == 1) rstl->flagv |= JSON;
	else if (!(rstl->flagv & JSON)) return 0;
	if (rstl->flagv & (COMMAED | COLONED)) return 0;

	if (LIST_FLAG_GET_CONCODE(rstl->flagv) == HCL_CONCODE_DIC)
	{
		if (rstl->count & 1) return 0;
	}
	else if (LIST_FLAG_GET_CONCODE(rstl->flagv) != HCL_CONCODE_ARRAY &&
	         LIST_FLAG_GET_CONCODE(rstl->flagv) != HCL_CONCODE_BYTEARRAY)
	{
		return 0;
	}

	rstl->flagv |= COMMAED;
	return 1;
}

static HCL_INLINE int can_colon_list (hcl_t* hcl)
{
	hcl_rstl_t* rstl;

	HCL_ASSERT (hcl, hcl->c->r.st != HCL_NULL);
	rstl = hcl->c->r.st;

	/* mark the state that a colon has appeared in the list */
	if (rstl->count <= 0) return 0;
	if (rstl->count == 1) rstl->flagv |= JSON;
	else if (!(rstl->flagv & JSON)) return 0;

	if (rstl->flagv & (COMMAED | COLONED)) return 0;

	if (LIST_FLAG_GET_CONCODE(rstl->flagv) != HCL_CONCODE_DIC) return 0;

	if (!(rstl->count & 1)) return 0;

	rstl->flagv |= COLONED;
	return 1;
}

static HCL_INLINE void clear_comma_colon_flag (hcl_t* hcl)
{
	hcl_rstl_t* rstl;
	HCL_ASSERT (hcl, hcl->c->r.st != HCL_NULL);
	rstl = hcl->c->r.st;
	rstl->flagv &= ~(COMMAED | COLONED);
}

static int chain_to_list (hcl_t* hcl, hcl_cnode_t* obj)
{
	hcl_rstl_t* rstl;
	int flagv;

	HCL_ASSERT (hcl, hcl->c->r.st != HCL_NULL);
	rstl = hcl->c->r.st;
	flagv = rstl->flagv;

	if (flagv & CLOSED)
	{
		/* the list has already been closed and cannot add more items
		 * for instance,  see this faulty expression #(1 2 . 3 4 ).
		 * you can have only 1 item  after the period. this condition
		 * can only be triggered by a wrong qlist where a period is
		 * allowed. so i can safely hard-code the error code to
		 * HCL_SYNERR_RPAREN */
		hcl_setsynerr (hcl, HCL_SYNERR_RPAREN, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		return -1;
	}
	else if (flagv & DOTTED)
	{
		hcl_cnode_t* tail;
		/* the list must not be empty to have reached the dotted state */
		HCL_ASSERT (hcl, rstl->head != HCL_NULL);
		HCL_ASSERT (hcl, rstl->tail != HCL_NULL);
		HCL_ASSERT (hcl, rstl->count > 0);

		/* chain the object via 'cdr' of the tail cell */
		tail = rstl->tail;
		HCL_ASSERT (hcl, tail != HCL_NULL);
		HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(tail));

		if (HCL_CNODE_IS_CONS(obj) && HCL_CNODE_CONS_CONCODE(obj) != HCL_CONCODE_QLIST)
		{
			hcl_cnode_t* shell;

			/* if the last element is another non-data list
			 * for example, #( 1 2 . [ 3 4 5  ]) 
			 * use a shell node to wrap the actual object list node head
			 * for the compiler.
			 */
			shell = hcl_makecnodeshell(hcl, HCL_CNODE_GET_LOC(obj), obj);
			if (HCL_UNLIKELY(!shell)) return -1;

			tail->u.cons.cdr = shell;
		}
		else
		{
			tail->u.cons.cdr = obj;
		}

		/* update the flag to CLOSED so that you can have more than
		 * one item after the dot. */
		flagv |= CLOSED;
		rstl->flagv = flagv;
/* TODO: check overflow on count??? */
		rstl->count++;
	}
	else
	{
		hcl_cnode_t* cons, * tail;

		if ((flagv & JSON) && rstl->count > 0 && !(flagv & (COMMAED | COLONED)))
		{
			/* there is no separator between array/dictionary elements
			 * for instance, [1 2] { 10 20 } */
			hcl_setsynerr (hcl, HCL_SYNERR_NOSEP, TOKEN_LOC(hcl), HCL_NULL);
			return -1;
		}

		cons = hcl_makecnodecons(hcl, HCL_CNODE_GET_LOC(obj), obj, HCL_NULL);
		if (HCL_UNLIKELY(!cons)) return -1;

		if (rstl->count <= 0)
		{
			/* the list head is not set yet. it is the first
			 * element added to the list. let both head and tail
			 * point to the new cons cell */
			HCL_ASSERT (hcl, rstl->tail == HCL_NULL);
			HCL_ASSERT (hcl, rstl->head == HCL_NULL);

			rstl->head = cons;
			rstl->tail = cons;
		}
		else
		{
			/* the new cons cell is not the first element.
			 * append it to the list */
			tail = rstl->tail;
			HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(tail));
			tail->u.cons.cdr = cons;
			rstl->tail = cons;
		}

/* TODO: check overflow on count??? */
		rstl->count++;
	}

	return 0;
}

static hcl_cnode_t* read_vlist (hcl_t* hcl)
{
	hcl_cnode_t* vh, * ve;
	hcl_ioloc_t start_loc;

	HCL_ASSERT (hcl, TOKEN_TYPE(hcl) == HCL_IOTOK_VBAR);

	vh = ve = HCL_NULL;
	start_loc = *TOKEN_LOC(hcl);

	GET_TOKEN_WITH_GOTO(hcl, oops);

	while (TOKEN_TYPE(hcl) == HCL_IOTOK_IDENT /* || TOKEN_TYPE(hcl) == HCL_IOTOK_IDENT_DOTTED */)
	{
		hcl_cnode_t* sym, * cons;

		if (hcl_getsyncodebyoocs_noseterr(hcl, TOKEN_NAME(hcl)) > 0)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
				"special symbol not to be declared as a variable");
			goto oops;
		}

		sym = hcl_makecnodesymbol(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		if (HCL_UNLIKELY(!sym)) goto oops;

		cons = hcl_makecnodecons(hcl, HCL_CNODE_GET_LOC(sym), sym, HCL_NULL);
		if (HCL_UNLIKELY(!cons)) 
		{
			hcl_freesinglecnode (hcl, sym); /* manual disposal because sym is not chained to the list */
			goto oops;
		}

		if (!vh)
		{
			vh = cons;
			ve = cons;
		}
		else
		{
			HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(ve));
			ve->u.cons.cdr = cons;
			ve = cons;
		}

		GET_TOKEN_WITH_GOTO (hcl, oops);
	}

	if (TOKEN_TYPE(hcl) != HCL_IOTOK_VBAR)
	{
		hcl_setsynerr (hcl, HCL_SYNERR_VBAR, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		goto oops;
	}

	if (vh)
	{
		HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(vh));
		HCL_CNODE_CONS_CONCODE(vh) = HCL_CONCODE_VLIST;
		return vh;
	}

	/* this is an empty list */
	return hcl_makecnodeelist(hcl, &start_loc, HCL_CONCODE_VLIST);

oops:
	if (vh) hcl_freecnode (hcl, vh);
	return HCL_NULL;
}

static hcl_cnode_t* read_object (hcl_t* hcl)
{
	/* this function read an s-expression non-recursively
	 * by manipulating its own stack. */

	int level = 0, flagv = 0;
	hcl_cnode_t* obj = HCL_NULL;

	while (1)
	{
	redo:
		switch (TOKEN_TYPE(hcl))
		{
			default:
				hcl_setsynerr (hcl, HCL_SYNERR_ILTOK, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				goto oops;

			case HCL_IOTOK_EOF:
				hcl_setsynerr (hcl, HCL_SYNERR_EOF, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				goto oops;

			case HCL_IOTOK_INCLUDE:
				/* TODO: should i limit where #include can be specified?
				 *       disallow it inside a list literal or an array literal? */
				GET_TOKEN_WITH_GOTO (hcl, oops);
				if (TOKEN_TYPE(hcl) != HCL_IOTOK_STRLIT)
				{
					hcl_setsynerr (hcl, HCL_SYNERR_STRING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}
				if (begin_include(hcl) <= -1) goto oops;
				goto redo;

			case HCL_IOTOK_LBRACK: /* [] */
				flagv = DATA_LIST;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_ARRAY);
				goto start_list;

			case HCL_IOTOK_BAPAREN: /* #[ */
				flagv = DATA_LIST;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_BYTEARRAY);
				goto start_list;

			case HCL_IOTOK_LBRACE: /* { */
				flagv = DATA_LIST;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_DIC);
				goto start_list;

			case HCL_IOTOK_QLPAREN: /* #( */
				flagv = DATA_LIST;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_QLIST);
				goto start_list;

			case HCL_IOTOK_LPARCOLON: /* (: */
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_MLIST);
				goto start_list;
				
			case HCL_IOTOK_LPAREN: /* ( */
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_XLIST);
			start_list:
				if (level >= HCL_TYPE_MAX(int))
				{
					/* the nesting level has become too deep */
					hcl_setsynerr (hcl, HCL_SYNERR_NESTING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}

				/* push some data to simulate recursion into
				 * a list literal or an array literal */
				if (enter_list(hcl, TOKEN_LOC(hcl), flagv) <= -1) goto oops;
				level++;

				/* read the next token */
				GET_TOKEN_WITH_GOTO (hcl, oops);
				goto redo;

			case HCL_IOTOK_DOT:
				if (level <= 0 || !can_dot_list(hcl))
				{
					/* cannot have a period:
					 *   1. at the top level - not inside ()
					 *   2. at the beginning of a list
					 *   3. inside an array, byte-array, dictionary, xlist */
					hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, TOKEN_LOC(hcl), HCL_NULL);
					goto oops;
				}

				GET_TOKEN_WITH_GOTO (hcl, oops);
				goto redo;

			case HCL_IOTOK_COLON:
				if (level <= 0 || !can_colon_list(hcl))
				{
					hcl_setsynerr (hcl, HCL_SYNERR_COLONBANNED, TOKEN_LOC(hcl), HCL_NULL);
					goto oops;
				}

				GET_TOKEN_WITH_GOTO (hcl, oops);
				goto redo;

			case HCL_IOTOK_COMMA:
				if (level <= 0 || !can_comma_list(hcl))
				{
					hcl_setsynerr (hcl, HCL_SYNERR_COMMABANNED, TOKEN_LOC(hcl), HCL_NULL);
					goto oops;
				}

				GET_TOKEN_WITH_GOTO (hcl, oops);
				goto redo;

			case HCL_IOTOK_RPAREN: /* xlist (), qlist #() */
			case HCL_IOTOK_RBRACK: /* bytearray #[], array[] */
			case HCL_IOTOK_RBRACE: /* dictionary {} */
			{
				static struct
				{
					int             closer;
					hcl_synerrnum_t synerr;
				} req[] =
				{
					{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }, /* XLIST     ( )  */
					{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }, /* MLIST     (: ) */
					{ HCL_IOTOK_RBRACK, HCL_SYNERR_RBRACK }, /* ARRAY     [ ] */
					{ HCL_IOTOK_RBRACK, HCL_SYNERR_RBRACK }, /* BYTEARRAY #[ ] */
					{ HCL_IOTOK_RBRACE, HCL_SYNERR_RBRACE }, /* DIC       { } */
					{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }  /* QLIST     #( )  */
				};

				int oldflagv;
				int concode;

				if (level <= 0)
				{
					hcl_setsynerr (hcl, HCL_SYNERR_UNBALPBB, TOKEN_LOC(hcl), HCL_NULL);
					goto oops;
				}

				concode = LIST_FLAG_GET_CONCODE(flagv);

				if (req[concode].closer != TOKEN_TYPE(hcl))
				{
					hcl_setsynerr (hcl, req[concode].synerr, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}

#if 0
				if ((flagv & QUOTED) || level <= 0)
				{
					/* the right parenthesis can never appear while
					 * 'quoted' is true. 'quoted' is set to false when
					 * entering a normal list. 'quoted' is set to true
					 * when entering a quoted list. a quoted list does
					 * not have an explicit right parenthesis.
					 * so the right parenthesis can only pair up with
					 * the left parenthesis for the normal list.
					 *
					 * For example, '(1 2 3 ') 5 6)
					 *
					 * this condition is triggerred when the first ) is
					 * met after the second quote.
					 *
					 * also it is illegal to have the right parenthesis
					 * with no opening(left) parenthesis, which is
					 * indicated by level<=0.
					 */
					hcl_setsynerr (hcl, HCL_SYNERR_LPAREN, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}
#endif
				obj = leave_list(hcl, &flagv, &oldflagv);

				level--;
				break;
			}

			case HCL_IOTOK_VBAR:
				if (hcl->c->r.st && (hcl->c->r.st->flagv & DATA_LIST))
				{
					hcl_setsynerr (hcl, HCL_SYNERR_VBARBANNED, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}
				obj = read_vlist(hcl);
				break;

			case HCL_IOTOK_NIL:
				obj = hcl_makecnodenil(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_TRUE:
				obj = hcl_makecnodetrue(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_FALSE:
				obj = hcl_makecnodefalse(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_SELF:
				obj = hcl_makecnodeself(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_SUPER:
				obj = hcl_makecnodesuper(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_ELLIPSIS:
				obj = hcl_makecnodeellipsis(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_TRPCOLONS:
				obj = hcl_makecnodetrpcolons(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_DCSTAR:
				obj = hcl_makecnodedcstar(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_SMPTRLIT:
			{
				hcl_oow_t i;
				hcl_oow_t v = 0;

				HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) >= 3);
				for (i = 2; i < TOKEN_NAME_LEN(hcl); i++)
				{
					HCL_ASSERT (hcl, is_xdigitchar(TOKEN_NAME_CHAR(hcl, i)));
					v = v * 16 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 16);
				}

				if (!HCL_IN_SMPTR_RANGE(v))
				{
					hcl_setsynerr (hcl, HCL_SYNERR_SMPTRLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}

				obj = hcl_makecnodesmptrlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl), v);
				break;
			}

			case HCL_IOTOK_ERRLIT:
			{
				hcl_oow_t i;
				hcl_ooi_t v = 0;

				HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) >= 3);
				for (i = 2; i < TOKEN_NAME_LEN(hcl); i++)
				{
					HCL_ASSERT (hcl, is_digitchar(TOKEN_NAME_CHAR(hcl, i)));
					v = v * 10 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 10);

					if (v > HCL_ERROR_MAX)
					{
						hcl_setsynerr (hcl, HCL_SYNERR_ERRLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
						goto oops;
					}
				}

				obj = hcl_makecnodeerrlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl), v);
				break;
			}

			case HCL_IOTOK_CHARLIT:
				obj = hcl_makecnodecharlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl), TOKEN_NAME_CHAR(hcl, 0));
				break;

			case HCL_IOTOK_NUMLIT:
				obj = hcl_makecnodenumlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_RADNUMLIT:
				obj = hcl_makecnoderadnumlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_FPDECLIT:
				obj = hcl_makecnodefpdeclit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			/*
			case HCL_IOTOK_REAL:
				obj = hcl_makerealnum(hcl, HCL_IOTOK_RVAL(hcl));
				break;
			*/

			case HCL_IOTOK_STRLIT:
				obj = hcl_makecnodestrlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_IDENT:
				obj = hcl_makecnodesymbol(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

			case HCL_IOTOK_IDENT_DOTTED:
				obj = hcl_makecnodedsymbol(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				break;

		}

		if (!obj) goto oops;

#if 0
		/* check if the element is read for a quoted list */
		while (flagv & QUOTED)
		{
			int oldflagv;

			HCL_ASSERT (hcl, level > 0);

			/* if so, append the element read into the quote list */
			if (chain_to_list(hcl, obj) <= -1) goto oops;

			/* exit out of the quoted list. the quoted list can have one element only. */
			obj = leave_list(hcl, &flagv, &oldflagv);

			/* one level up toward the top */
			level--;
		}
#endif

		/* check if we are at the top level */
		if (level <= 0) break; /* yes */

		/* if not, append the element read into the current list.
		 * if we are not at the top level, we must be in a list */
		if (chain_to_list(hcl, obj) <= -1) goto oops;

		/* because it has been chained to the list, it belongs to the current stack top. 
		 * mark that obj is not stand-alone by nullifying it. without this, if a jump
		 * is made to oops, the momory block pointed to by obj may get freed twice. */
		obj = HCL_NULL; 

		clear_comma_colon_flag (hcl);

		/* read the next token */
		GET_TOKEN_WITH_GOTO (hcl, oops);
	}

	/* upon exit, we must be at the top level */
	HCL_ASSERT (hcl, level == 0);

	HCL_ASSERT (hcl, hcl->c->r.st == HCL_NULL);
	HCL_ASSERT (hcl, obj != HCL_NULL);
	return obj;

oops:
	if (obj) hcl_freecnode (hcl, obj);

	/* clean up the reader stack for a list */
	while (hcl->c->r.st)
	{
		hcl_rstl_t* rstl;
		rstl = hcl->c->r.st;
		hcl->c->r.st = rstl->prev;
		if (rstl->head) hcl_freecnode (hcl, rstl->head);
		hcl_freemem (hcl, rstl);
	}

	return HCL_NULL;
}

hcl_cnode_t* hcl_read (hcl_t* hcl)
{
	HCL_ASSERT (hcl, hcl->c && hcl->c->reader);
	if (get_token(hcl) <= -1) return HCL_NULL;
	if (TOKEN_TYPE(hcl) == HCL_IOTOK_EOF)
	{
		hcl_seterrnum (hcl, HCL_EFINIS);
		return HCL_NULL;
	}
	return read_object(hcl);
}

/* ------------------------------------------------------------------------ */

/* TODO:
hcl_cnodetoobj (hcl_t* hcl, hcl_cnode_t* x)
{
 * drop location information and compose object ??
 * is it doable? can convert a dotted symbol to a proper value? 
}
*/ 

/* ---------------------------------------------------------------------- */

static int on_fed_cnode (hcl_t* hcl, hcl_cnode_t* obj)
{
	/* the default handler for a cnode composed via feeding - just compile the object node. */
	return hcl_compile(hcl, obj, 0);
}

/* ---------------------------------------------------------------------- */

static void init_feed (hcl_t* hcl)
{
	HCL_MEMSET (&hcl->c->feed, 0, HCL_SIZEOF(hcl->c->feed));

	hcl->c->feed.lx.state = HCL_FLX_START;
	hcl->c->feed.lx.loc.line = 1;
	hcl->c->feed.lx.loc.colm = 1;
	hcl->c->feed.lx.loc.file = HCL_NULL;

	hcl->c->feed.on_cnode = on_fed_cnode;
}

/* ------------------------------------------------------------------------ */

static int feed_begin_include (hcl_t* hcl)
{
	hcl_ioinarg_t* arg;
	const hcl_ooch_t* io_name;

	io_name = add_io_name(hcl, TOKEN_NAME(hcl));
	if (HCL_UNLIKELY(!io_name)) return -1;

	arg = (hcl_ioinarg_t*)hcl_callocmem(hcl, HCL_SIZEOF(*arg));
	if (HCL_UNLIKELY(!arg)) goto oops;

	arg->name = io_name;
	arg->line = 1;
	arg->colm = 1;
	/*arg->nl = '\0';*/
	arg->includer = hcl->c->curinp;

	if (hcl->c->reader(hcl, HCL_IO_OPEN, arg) <= -1)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_INCLUDE, TOKEN_LOC(hcl), TOKEN_NAME(hcl), "unable to include %js", io_name);
		goto oops;
	}

	if (arg->includer == &hcl->c->inarg)
	{
		/* TODO: remove hcl_readbaseinchar() and clean up this part.
		 * hcl_readbaseinchar(), if called in the middle of feeds,
		 * updates hcl->c->inarg's line and colm. so use a separate
		 * field to store the current feed location for now */
		hcl->c->feed.lx._oloc = hcl->c->feed.lx.loc;
	}
	else
	{
		arg->includer->name = hcl->c->feed.lx.loc.file;
		arg->includer->line = hcl->c->feed.lx.loc.line;
		arg->includer->colm = hcl->c->feed.lx.loc.colm;
	}
	hcl->c->feed.lx.loc.file = arg->name;
	hcl->c->feed.lx.loc.line = arg->line;
	hcl->c->feed.lx.loc.colm = arg->colm;

	/* switch to the includee's stream */
	hcl->c->curinp = arg;
	/* hcl->c->depth.incl++; */

	return 0;

oops:
	if (arg) hcl_freemem (hcl, arg);
	return -1;
}

static int feed_end_include (hcl_t* hcl)
{
	int x;
	hcl_ioinarg_t* cur;

	if (hcl->c->curinp == &hcl->c->inarg) return 0; /* no include */

	/* if it is an included file, close it and
	 * retry to read a character from an outer file */

	x = hcl->c->reader(hcl, HCL_IO_CLOSE, hcl->c->curinp);

	/* if closing has failed, still destroy the sio structure 
	 * first as normal and return the failure below. this way, 
	 * the caller doesn't call HCL_IO_CLOSE on hcl->c->curinp again. */

	cur = hcl->c->curinp;
	hcl->c->curinp = hcl->c->curinp->includer;

	if (hcl->c->curinp == &hcl->c->inarg)
	{
		hcl->c->feed.lx.loc = hcl->c->feed.lx._oloc;
	}
	else
	{
		hcl->c->feed.lx.loc.file = hcl->c->curinp->name;
		hcl->c->feed.lx.loc.line = hcl->c->curinp->line;
		hcl->c->feed.lx.loc.colm = hcl->c->curinp->colm;
	}

	HCL_ASSERT (hcl, cur->name != HCL_NULL);
	hcl_freemem (hcl, cur);
	/* hcl->parse.depth.incl--; */

	if (x != 0)
	{
		/* the failure mentioned above is returned here */
		return -1;
	}

	hcl->c->lxc = hcl->c->curinp->lxc;
	return 1; /* ended the included file successfully */
}

static int feed_process_token (hcl_t* hcl)
{
	hcl_frd_t* frd = &hcl->c->feed.rd;

	/* this function composes an s-expression non-recursively
	 * by manipulating its own stack. */

/*hcl_logbfmt (hcl, HCL_LOG_STDERR, "TOKEN => [%.*js] type=%d LOC=%d.%d\n", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl), TOKEN_TYPE(hcl), TOKEN_LOC(hcl)->line, TOKEN_LOC(hcl)->colm);*/
	if (frd->expect_include_file)
	{
		/* the #include directive is an exception to the general expression rule.
		 * use this exceptional code block to divert the major token processing */

		if (TOKEN_TYPE(hcl) != HCL_IOTOK_STRLIT)
		{
			hcl_setsynerr (hcl, HCL_SYNERR_STRING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			goto oops;
		}

		frd->expect_include_file = 0;

		/* indicate that the file inclusion should be performed soon.
		 * don't perform actual inclusion here so that the return value of 
		 * feed_char() advances the input pointers properly. */
		frd->do_include_file = 1; 

		goto ok;
	}

	if (frd->expect_vlist_item && TOKEN_TYPE(hcl) != HCL_IOTOK_IDENT && TOKEN_TYPE(hcl) != HCL_IOTOK_VBAR)
	{
		/* vlist also has special requirement that it can only contain variable names. */
		hcl_setsynerr (hcl, HCL_SYNERR_VARNAME, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		goto oops;
	}

	switch (TOKEN_TYPE(hcl))
	{
		default:
			hcl_setsynerr (hcl, HCL_SYNERR_ILTOK, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			goto oops;

		case HCL_IOTOK_EOF:
			hcl_setsynerr (hcl, HCL_SYNERR_EOF, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			goto oops;

		case HCL_IOTOK_INCLUDE:
			/* TODO: should i limit where #include can be specified?
			 *       disallow it inside a list literal or an array literal? */
			frd->expect_include_file = 1;
			goto ok;

		case HCL_IOTOK_VBAR:
			if (frd->expect_vlist_item)
			{
				/* closer */
				int oldflagv;
				frd->expect_vlist_item = 0;
				frd->obj = leave_list(hcl, &frd->flagv, &oldflagv);
				frd->level--;
				break;
			}
			else
			{
				/* opener */
			
				/* the vlist is different from other lists in that
				 *   it uses the same opener and the closer
				 *   it allows only variable names.
				 *   it prohibits nesting of other lists
				 */
				if (hcl->c->r.st && (hcl->c->r.st->flagv & DATA_LIST))
				{
					/* if the outer list is a data list */
					hcl_setsynerr (hcl, HCL_SYNERR_VBARBANNED, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}

				/* neither a data list nor an executable list. handle this specially using
				 * a dedicated frd->expect_vlist_item variable */
				frd->flagv = 0; 
				LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_VLIST);
				frd->expect_vlist_item = 1;
				goto start_list;
			}

		case HCL_IOTOK_LBRACK: /* [ */
			frd->flagv = DATA_LIST;
			LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_ARRAY);
			goto start_list;

		case HCL_IOTOK_BAPAREN: /* #[ */
			frd->flagv = DATA_LIST;
			LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_BYTEARRAY);
			goto start_list;

		case HCL_IOTOK_LBRACE: /* { */
			frd->flagv = DATA_LIST;
			LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_DIC);
			goto start_list;

		case HCL_IOTOK_QLPAREN: /* #( */
			frd->flagv = DATA_LIST;
			LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_QLIST);
			goto start_list;

		case HCL_IOTOK_LPARCOLON: /* (: */
			frd->flagv = 0;
			LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_MLIST);
			goto start_list;
			
		case HCL_IOTOK_LPAREN: /* ( */
			frd->flagv = 0;
			LIST_FLAG_SET_CONCODE (frd->flagv, HCL_CONCODE_XLIST);
		start_list:
			if (frd->level >= HCL_TYPE_MAX(int))
			{
				/* the nesting level has become too deep */
				hcl_setsynerr (hcl, HCL_SYNERR_NESTING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				goto oops;
			}

			/* push some data to simulate recursion into
			 * a list literal or an array literal */
			if (enter_list(hcl, TOKEN_LOC(hcl), frd->flagv) <= -1) goto oops;
			frd->level++;

			/* read the next token */
			goto ok;

		case HCL_IOTOK_DOT:
			if (frd->level <= 0 || !can_dot_list(hcl))
			{
				/* cannot have a period:
				 *   1. at the top frd->level - not inside ()
				 *   2. at the beginning of a list
				 *   3. inside an array, byte-array, dictionary, xlist */
				hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, TOKEN_LOC(hcl), HCL_NULL);
				goto oops;
			}

			goto ok;

		case HCL_IOTOK_COLON:
			if (frd->level <= 0 || !can_colon_list(hcl))
			{
				hcl_setsynerr (hcl, HCL_SYNERR_COLONBANNED, TOKEN_LOC(hcl), HCL_NULL);
				goto oops;
			}

			goto ok;

		case HCL_IOTOK_COMMA:
			if (frd->level <= 0 || !can_comma_list(hcl))
			{
				hcl_setsynerr (hcl, HCL_SYNERR_COMMABANNED, TOKEN_LOC(hcl), HCL_NULL);
				goto oops;
			}

			goto ok;

		case HCL_IOTOK_RPAREN: /* xlist (), qlist #() */
		case HCL_IOTOK_RBRACK: /* bytearray #[], array[] */
		case HCL_IOTOK_RBRACE: /* dictionary {} */
		{
			static struct
			{
				int             closer;
				hcl_synerrnum_t synerr;
			} req[] =
			{
				{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }, /* XLIST     ( )  */
				{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }, /* MLIST     (: ) */
				{ HCL_IOTOK_RBRACK, HCL_SYNERR_RBRACK }, /* ARRAY     [ ] */
				{ HCL_IOTOK_RBRACK, HCL_SYNERR_RBRACK }, /* BYTEARRAY #[ ] */
				{ HCL_IOTOK_RBRACE, HCL_SYNERR_RBRACE }, /* DIC       { } */
				{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }  /* QLIST     #( )  */
			};

			int oldflagv;
			int concode;

			if (frd->level <= 0)
			{
				hcl_setsynerr (hcl, HCL_SYNERR_UNBALPBB, TOKEN_LOC(hcl), HCL_NULL);
				goto oops;
			}

			concode = LIST_FLAG_GET_CONCODE(frd->flagv);

			if (req[concode].closer != TOKEN_TYPE(hcl))
			{
				hcl_setsynerr (hcl, req[concode].synerr, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				goto oops;
			}

#if 0
			if ((flagv & QUOTED) || frd->level <= 0)
			{
				/* the right parenthesis can never appear while
				 * 'quoted' is true. 'quoted' is set to false when
				 * entering a normal list. 'quoted' is set to true
				 * when entering a quoted list. a quoted list does
				 * not have an explicit right parenthesis.
				 * so the right parenthesis can only pair up with
				 * the left parenthesis for the normal list.
				 *
				 * For example, '(1 2 3 ') 5 6)
				 *
				 * this condition is triggerred when the first ) is
				 * met after the second quote.
				 *
				 * also it is illegal to have the right parenthesis
				 * with no opening(left) parenthesis, which is
				 * indicated by frd->level<=0.
				 */
				hcl_setsynerr (hcl, HCL_SYNERR_LPAREN, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				goto oops;
			}
#endif
			frd->obj = leave_list(hcl, &frd->flagv, &oldflagv);
			frd->level--;
			break;
		}

		case HCL_IOTOK_NIL:
			frd->obj = hcl_makecnodenil(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_TRUE:
			frd->obj = hcl_makecnodetrue(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_FALSE:
			frd->obj = hcl_makecnodefalse(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_SELF:
			frd->obj = hcl_makecnodeself(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_SUPER:
			frd->obj = hcl_makecnodesuper(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_ELLIPSIS:
			frd->obj = hcl_makecnodeellipsis(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_TRPCOLONS:
			frd->obj = hcl_makecnodetrpcolons(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_DCSTAR:
			frd->obj = hcl_makecnodedcstar(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_SMPTRLIT:
		{
			hcl_oow_t i;
			hcl_oow_t v = 0;

			HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) >= 3);
			for (i = 2; i < TOKEN_NAME_LEN(hcl); i++)
			{
				HCL_ASSERT (hcl, is_xdigitchar(TOKEN_NAME_CHAR(hcl, i)));
				v = v * 16 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 16);
			}

			if (!HCL_IN_SMPTR_RANGE(v))
			{
				hcl_setsynerr (hcl, HCL_SYNERR_SMPTRLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				goto oops;
			}

			frd->obj = hcl_makecnodesmptrlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl), v);
			break;
		}

		case HCL_IOTOK_ERRLIT:
		{
			hcl_oow_t i;
			hcl_ooi_t v = 0;

			HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) >= 3);
			for (i = 2; i < TOKEN_NAME_LEN(hcl); i++)
			{
				HCL_ASSERT (hcl, is_digitchar(TOKEN_NAME_CHAR(hcl, i)));
				v = v * 10 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 10);

				if (v > HCL_ERROR_MAX)
				{
					hcl_setsynerr (hcl, HCL_SYNERR_ERRLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					goto oops;
				}
			}

			frd->obj = hcl_makecnodeerrlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl), v);
			break;
		}

		case HCL_IOTOK_CHARLIT:
			frd->obj = hcl_makecnodecharlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl), TOKEN_NAME_CHAR(hcl, 0));
			break;

		case HCL_IOTOK_NUMLIT:
			frd->obj = hcl_makecnodenumlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_RADNUMLIT:
			frd->obj = hcl_makecnoderadnumlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_FPDECLIT:
			frd->obj = hcl_makecnodefpdeclit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		/*
		case HCL_IOTOK_REAL:
			frd->obj = hcl_makerealnum(hcl, HCL_IOTOK_RVAL(hcl));
			break;
		*/

		case HCL_IOTOK_STRLIT:
			frd->obj = hcl_makecnodestrlit(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_IDENT:
			frd->obj = hcl_makecnodesymbol(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;

		case HCL_IOTOK_IDENT_DOTTED:
			frd->obj = hcl_makecnodedsymbol(hcl, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			break;
	}

	if (!frd->obj) goto oops;

#if 0
	/* check if the element is read for a quoted list */
	while (flagv & QUOTED)
	{
		int oldflagv;

		HCL_ASSERT (hcl, frd->level > 0);

		/* if so, append the element read into the quote list */
		if (chain_to_list(hcl, obj) <= -1) goto oops;

		/* exit out of the quoted list. the quoted list can have one element only. */
		obj = leave_list(hcl, &flagv, &oldflagv);

		/* one frd->level up toward the top */
		frd->level--;
	}
#endif

	/* check if we are at the top frd->level */
	if (frd->level <= 0) 
	{
		int n;

		/* upon exit, we must be at the top level */
		HCL_ASSERT (hcl, frd->level == 0);

		HCL_ASSERT (hcl, hcl->c->r.st == HCL_NULL);
		HCL_ASSERT (hcl, frd->obj != HCL_NULL);

		n = hcl->c->feed.on_cnode(hcl, frd->obj);
		hcl_freecnode (hcl, frd->obj); /* not needed any more */
		frd->obj = HCL_NULL;
		if (n <= -1) goto oops;
	}
	else
	{
		/* if not, append the element read into the current list.
		 * if we are not at the top frd->level, we must be in a list */
		if (chain_to_list(hcl, frd->obj) <= -1) goto oops;

		/* because it has been chained to the list, it belongs to the current stack top. 
		 * mark that obj is not stand-alone by nullifying it. without this, if a jump
		 * is made to oops, the momory block pointed to by obj may get freed twice. */
		frd->obj = HCL_NULL; 

		clear_comma_colon_flag (hcl);
	}

ok:
	return 0;

oops:
	if (frd->obj) 
	{
		hcl_freecnode (hcl, frd->obj);
		frd->obj = HCL_NULL;
	}

	/* clean up the reader stack for a list */
	while (hcl->c->r.st)
	{
		hcl_rstl_t* rstl;
		rstl = hcl->c->r.st;
		hcl->c->r.st = rstl->prev;
		if (rstl->head) hcl_freecnode (hcl, rstl->head);
		hcl_freemem (hcl, rstl);
	}

	return -1;
}


/* ------------------------------------------------------------------------ */

struct delim_token_t
{
	const char*      t_value;
	hcl_oow_t        t_len;
	hcl_iotok_type_t t_type;
};
typedef struct delim_token_t delim_token_t;

static delim_token_t delim_token_tab[] =
{
	/* [NOTE 1] 
	 *  if you add a new token, ensure the first character is listed in is_delimchar() 
	 * 
	 * [NOTE 2]
	 *  for the implementation limitation in find_delim_token_char(),
	 *  the entries in this table must be laid out in a certain way.
	 * 
	 *    Group the items with the same prefix together.
	 *    List the shorter before the longer items in the same group.
	 *    The length must not differ by greater than 1 between 2 items in the same group.
	 * 
	 * [NOTE 3]
	 *  don't list #( and #[ here because of overlapping use of # for various purposes.
	 *  however, # is included in is_delimchar().
	 */

	{ "(",        1, HCL_IOTOK_LPAREN },
	{ "(:",       2, HCL_IOTOK_LPARCOLON },
	{ ")",        1, HCL_IOTOK_RPAREN },

	{ "[",        1, HCL_IOTOK_LBRACK },
	{ "]",        1, HCL_IOTOK_RBRACK },
	
	{ "{",        1, HCL_IOTOK_LBRACE },
	{ "}",        1, HCL_IOTOK_RBRACE },

	{ "|",        1, HCL_IOTOK_VBAR },
	{ ",",        1, HCL_IOTOK_COMMA },

	{ ".",        1, HCL_IOTOK_DOT },
	{ "..",       2, HCL_IOTOK_DBLDOTS },
	{ "...",      3, HCL_IOTOK_ELLIPSIS },

	{ ":",        1, HCL_IOTOK_COLON },
	{ "::",       2, HCL_IOTOK_DBLCOLONS },
	{ "::*",      3, HCL_IOTOK_DCSTAR },
	{ ":::",      3, HCL_IOTOK_TRPCOLONS  }
};

static int find_delim_token_char (hcl_t* hcl, const hcl_ooci_t c, int row_start, int row_end, int col, hcl_flx_dt_t* dt)
{
	int found = 0, i;

	for (i = row_start; i <= row_end; i++)
	{
		if (col < delim_token_tab[i].t_len && c == delim_token_tab[i].t_value[col]) 
		{
			if (!found) dt->row_start = i;
			dt->row_end = i;
			found = 1;
		}
		else if (found) break;
	}

	if (found) dt->col_next = col + 1;
	return found;
}

static HCL_INLINE int feed_wrap_up (hcl_t* hcl, hcl_iotok_type_t type)
{
	int n;
	SET_TOKEN_TYPE (hcl, type);

	n = feed_process_token(hcl);

	hcl->c->feed.lx.state = HCL_FLX_START;
	return n;
}

static int feed_wrap_up_with_char (hcl_t* hcl, hcl_ooci_t c, hcl_iotok_type_t type)
{
	ADD_TOKEN_CHAR (hcl, c);
	return feed_wrap_up(hcl, type);
}

static int feed_wrap_up_with_str (hcl_t* hcl, const hcl_ooch_t* str, hcl_oow_t len, hcl_iotok_type_t type)
{
	ADD_TOKEN_STR (hcl, str, len);
	return feed_wrap_up(hcl, type);
}

static int feed_continue (hcl_t* hcl, hcl_flx_state_t state)
{
	hcl->c->feed.lx.state = state;
	return 0;
}

static int feed_continue_with_char (hcl_t* hcl, hcl_ooci_t c, hcl_flx_state_t state)
{
	ADD_TOKEN_CHAR (hcl, c);
	hcl->c->feed.lx.state = state;
	return 0;
}

#define FEED_WRAP_UP(hcl, type) do { if (feed_wrap_up(hcl, type) <= -1) return -1; } while(0)
#define FEED_WRAP_UP_WITH_CHAR(hcl, c, type) do { if (feed_wrap_up_with_char(hcl, c, type) <= -1) return -1; } while(0)
#define FEED_WRAP_UP_WITH_CHARS(hcl, str, len, type) do { if (feed_wrap_up_with_str(hcl, str, len, type) <= -1) return -1; } while(0)
#define FEED_CONTINUE(hcl, state) do { if (feed_continue(hcl, state) <= -1) return -1; } while(0)
#define FEED_CONTINUE_WITH_CHAR(hcl, c, state) do { if (feed_continue_with_char(hcl, c, state) <= -1) return -1; } while(0)

/* ------------------------------------------------------------------------ */

/* short-cuts to basic lexer data */
#define FLX_STATE(hcl) ((hcl)->c->feed.lx.state)
#define FLX_LOC(hcl) (&((hcl)->c->feed.lx.loc))

/* short-cuts to lexer state data */
#define FLX_DT(hcl) (&((hcl)->c->feed.lx.u.dt))
#define FLX_HC(hcl) (&((hcl)->c->feed.lx.u.hc))
#define FLX_HI(hcl) (&((hcl)->c->feed.lx.u.hi))
#define FLX_HN(hcl) (&((hcl)->c->feed.lx.u.hn))
#define FLX_PI(hcl) (&((hcl)->c->feed.lx.u.pi))
#define FLX_PN(hcl) (&((hcl)->c->feed.lx.u.pn))
#define FLX_QT(hcl) (&((hcl)->c->feed.lx.u.qt))
#define FLX_ST(hcl) (&((hcl)->c->feed.lx.u.st))

static HCL_INLINE void init_flx_hc (hcl_flx_hc_t* hc)
{
	HCL_MEMSET (hc, 0, HCL_SIZEOF(*hc));
}

static HCL_INLINE void init_flx_hi (hcl_flx_hi_t* hi)
{
	HCL_MEMSET (hi, 0, HCL_SIZEOF(*hi));
}

static HCL_INLINE void init_flx_hn (hcl_flx_hn_t* hn, hcl_iotok_type_t tok_type, hcl_synerrnum_t synerr_code, int radix)
{
	HCL_MEMSET (hn, 0, HCL_SIZEOF(*hn));
	hn->tok_type = tok_type;
	hn->synerr_code = synerr_code;
	hn->radix = radix;
}

static HCL_INLINE void init_flx_qt (hcl_flx_qt_t* qt, hcl_iotok_type_t tok_type, hcl_synerrnum_t synerr_code, hcl_ooch_t end_char, hcl_ooch_t esc_char, hcl_oow_t min_len, hcl_oow_t max_len)
{
	HCL_MEMSET (qt, 0, HCL_SIZEOF(*qt));
	qt->tok_type = tok_type;
	qt->synerr_code = synerr_code;
	qt->end_char = end_char;
	qt->esc_char = esc_char;
	qt->min_len = min_len;
	qt->max_len = max_len;
}

static HCL_INLINE void init_flx_pi (hcl_flx_pi_t* pi)
{
	HCL_MEMSET (pi, 0, HCL_SIZEOF(*pi));
}

static HCL_INLINE void init_flx_pn (hcl_flx_pn_t* pn)
{
	HCL_MEMSET (pn, 0, HCL_SIZEOF(*pn));
}

static HCL_INLINE void init_flx_st (hcl_flx_st_t* st, hcl_ooch_t sign_c)
{
	HCL_MEMSET (st, 0, HCL_SIZEOF(*st));
	st->sign_c = sign_c;
}

static void reset_flx_token (hcl_t* hcl)
{
	/* clear the token name, reset its location */
	SET_TOKEN_TYPE (hcl, HCL_IOTOK_EOF); /* is it correct? */
	CLEAR_TOKEN_NAME (hcl);
	SET_TOKEN_LOC (hcl, &hcl->c->feed.lx.loc);
}

static int flx_start (hcl_t* hcl, hcl_ooci_t c)
{
	HCL_ASSERT (hcl, FLX_STATE(hcl) == HCL_FLX_START);

	if (is_spacechar(c)) goto consumed; /* skip spaces */

	reset_flx_token (hcl);

//HCL_DEBUG1 (hcl, "XXX[%jc]\n", c);
	if (find_delim_token_char(hcl, c, 0, HCL_COUNTOF(delim_token_tab) - 1, 0, FLX_DT(hcl))) 
	{
		/* the character is one of the first character of a delimiter token such as (, [, :, etc */
		if (FLX_DT(hcl)->row_start == FLX_DT(hcl)->row_end && 
		    FLX_DT(hcl)->col_next == delim_token_tab[FLX_DT(hcl)->row_start].t_len)
		{
			/* single character delimiter token */
			FEED_WRAP_UP_WITH_CHAR (hcl, c, delim_token_tab[FLX_DT(hcl)->row_start].t_type);
		}
		else
		{
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_DELIM_TOKEN); /* consume c and move to HCL_FLX_DELIM_TOKEN state */
		}
		goto consumed;
	}

	switch (c)
	{
		case HCL_OOCI_EOF:
			/* only EOF of the top-level stream is supposed to be fed in.
			 * the internal logic discard EOFs of included streams */
			FEED_WRAP_UP_WITH_CHARS (hcl, vocas[VOCA_EOF].str, vocas[VOCA_EOF].len, HCL_IOTOK_EOF);
			goto consumed;

		case ';':
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_COMMENT);
			goto consumed;

		case '#':
			/* no state date to initialize. just change the state */
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_HMARKED_TOKEN);
			goto consumed;

		case '\"':
			init_flx_qt (FLX_QT(hcl), HCL_IOTOK_STRLIT, HCL_SYNERR_STRLIT, c, '\\', 0, HCL_TYPE_MAX(hcl_oow_t));
			FEED_CONTINUE (hcl, HCL_FLX_QUOTED_TOKEN); /* discard the quote itself. move on the the QUOTED_TOKEN state */
			goto consumed;

		case '\'':
			init_flx_qt (FLX_QT(hcl), HCL_IOTOK_CHARLIT, HCL_SYNERR_CHARLIT, c, '\\', 1, 1);
			FEED_CONTINUE (hcl, HCL_FLX_QUOTED_TOKEN); /* discard the quote itself. move on the the QUOTED_TOKEN state */
			goto consumed;

		case '+':
		case '-':
			init_flx_st (FLX_ST(hcl), c);
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_SIGNED_TOKEN);
			goto consumed;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			init_flx_pn (FLX_PN(hcl));
			FEED_CONTINUE (hcl, HCL_FLX_PLAIN_NUMBER);
			goto not_consumed;

		default:
			init_flx_pi (FLX_PI(hcl));
			FEED_CONTINUE (hcl, HCL_FLX_PLAIN_IDENT);
			goto not_consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_comment (hcl_t* hcl, hcl_ooci_t c)
{
	if (is_linebreak(c)) FEED_CONTINUE (hcl, HCL_FLX_START);
	return 1; /* consumed */
}

static int flx_delim_token (hcl_t* hcl, hcl_ooci_t c)
{
	if (find_delim_token_char(hcl, c, FLX_DT(hcl)->row_start, FLX_DT(hcl)->row_end, FLX_DT(hcl)->col_next, FLX_DT(hcl))) 
	{
		if (FLX_DT(hcl)->row_start == FLX_DT(hcl)->row_end && 
		    FLX_DT(hcl)->col_next == delim_token_tab[FLX_DT(hcl)->row_start].t_len)
		{
			/* complete token and switch to the HCL_FLX_START state */
			FEED_WRAP_UP_WITH_CHAR (hcl, c, delim_token_tab[FLX_DT(hcl)->row_start].t_type); 
		}
		else
		{
			ADD_TOKEN_CHAR(hcl, c);
		}
		goto consumed;
	}
	else
	{
		/* the longest match so far */
		FEED_WRAP_UP(hcl, delim_token_tab[FLX_DT(hcl)->row_start].t_type); 
		goto not_consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_hmarked_token (hcl_t* hcl, hcl_ooci_t c)
{
	/*
	 * #xXXXX hexadecimal
	 * #oOOOO octal
	 * #bBBBB binary
	 * #eDDD   error
	 * #pHHH   smptr
	 * #\C      character
	 * #\xHHHH  unicode character
	 * #\UHHHH  unicode character
	 * #\uHHHH  unicode character
	 * #\backspace
	 * #\linefeed
	 * #\newline
	 * #\nul
	 * #\page
	 * #\return
	 * #\rubout
	 * #\space
	 * #\tab
	 * #\vtab
	 * #include
	 * #[ ]     byte array
	 * #( )     qlist
	 */

	switch (c)
	{
		case '#':
		case '!':
			/* ## comment start
			 * #! also comment start.
			 * ; comment start */
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_COMMENT);
			goto consumed;

		/* --------------------------- */

		case 'x':
			init_flx_hn (FLX_HN(hcl), HCL_IOTOK_RADNUMLIT, HCL_SYNERR_NUMLIT, 16);
			goto radixed_number;

		case 'o':
			init_flx_hn (FLX_HN(hcl), HCL_IOTOK_RADNUMLIT, HCL_SYNERR_NUMLIT, 8);
			goto radixed_number;

		case 'b':
			init_flx_hn (FLX_HN(hcl), HCL_IOTOK_RADNUMLIT, HCL_SYNERR_NUMLIT, 2);
			goto radixed_number;

		case 'e':
			init_flx_hn (FLX_HN(hcl), HCL_IOTOK_ERRLIT, HCL_SYNERR_ERRLIT, 10);
			goto radixed_number;

		case 'p':
			init_flx_hn (FLX_HN(hcl), HCL_IOTOK_SMPTRLIT, HCL_SYNERR_SMPTRLIT, 16);
		radixed_number:
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_HMARKED_NUMBER);
			goto consumed;

		/* --------------------------- */
		case '\\':
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_HMARKED_CHAR);
			goto consumed;

		/* --------------------------- */
		case '[':
			FEED_WRAP_UP_WITH_CHAR (hcl, c, HCL_IOTOK_BAPAREN);
			goto consumed;

		case '(':
			FEED_WRAP_UP_WITH_CHAR (hcl, c, HCL_IOTOK_QLPAREN);
			goto consumed;

		/* --------------------------- */
		default:
			/* the character used as case values above can never be the first character of a hash-marked identifier */
			init_flx_hi (FLX_HI(hcl));
			FEED_CONTINUE (hcl, HCL_FLX_HMARKED_IDENT);
			goto not_consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_hmarked_char (hcl_t* hcl, hcl_ooci_t c)
{
	hcl_flx_hc_t* hc = FLX_HC(hcl);

	if (is_delimchar(c))
	{
		if (hc->char_count == 0)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
				"no valid character in character literal %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
			return -1;
		}

		if (TOKEN_NAME_LEN(hcl) >= 4)
		{
			int max_digit_count = 0;

			if (TOKEN_NAME_CHAR(hcl, 2) == 'x')
			{
				hcl_oow_t i;
				max_digit_count = 2;

			hexcharlit:
				if (TOKEN_NAME_LEN(hcl) - 3 > max_digit_count)
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
						"invalid hexadecimal character character literal %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
					return -1;
				}
				c = 0;
				for (i = 3; i < TOKEN_NAME_LEN(hcl); i++)
				{
					if (!is_xdigitchar(TOKEN_NAME_CHAR(hcl, i)))
					{
						hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
							"invalid hexadecimal character character literal %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
						return -1;
					}
					c = c * 16 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 16); /* don't care if it is for 'p' */
				}
			}
		#if (HCL_SIZEOF_OOCH_T >= 2)
			else if (TOKEN_NAME_CHAR(hcl, 2) == 'u')
			{
				max_digit_count = 4;
				goto hexcharlit;
			}
		#endif
		#if (HCL_SIZEOF_OOCH_T >= 4)
			else if (TOKEN_NAME_CHAR(hcl, 2) == 'U')
			{
				max_digit_count = 8;
				goto hexcharlit;
			}
		#endif
			else if (does_token_name_match(hcl, VOCA_BACKSPACE)) c = '\b';
			else if (does_token_name_match(hcl, VOCA_LINEFEED))  c = '\n';
			else if (does_token_name_match(hcl, VOCA_NEWLINE))   c = '\n'; 	/* TODO: convert it to host newline convention. how to handle if it's composed of 2 letters like \r\n? */
			else if (does_token_name_match(hcl, VOCA_NUL))       c = '\0';  /* null character. not the object null */
			else if (does_token_name_match(hcl, VOCA_PAGE))      c = '\f';
			else if (does_token_name_match(hcl, VOCA_RETURN))    c = '\r';
			else if (does_token_name_match(hcl, VOCA_RUBOUT))    c = '\x7F'; /* DEL */
			else if (does_token_name_match(hcl, VOCA_SPACE))     c = ' ';
			else if (does_token_name_match(hcl, VOCA_TAB))       c = '\t';
			else if (does_token_name_match(hcl, VOCA_VTAB))      c = '\v';
			else
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
					"invalid character literal %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
				return -1;
			}
		}
		else
		{
			HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) == 3);
			c = TOKEN_NAME_CHAR(hcl, 2);
		}

		/* reset the token name to the converted character */
		CLEAR_TOKEN_NAME (hcl);
		ADD_TOKEN_CHAR (hcl, c);
		FEED_WRAP_UP (hcl, HCL_IOTOK_CHARLIT);
		goto not_consumed;
	}
	else
	{
		ADD_TOKEN_CHAR (hcl, c);
		hc->char_count++;
		goto consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_hmarked_ident (hcl_t* hcl, hcl_ooci_t c)
{
	hcl_flx_hi_t* hi = FLX_HI(hcl);

	if (is_delimchar(c))
	{
		hcl_iotok_type_t tok_type;

		if (hi->char_count == 0)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_HASHLIT, FLX_LOC(hcl), HCL_NULL,
				"no valid character after the hash sign");
			return -1;
		}

		if (get_directive_token_type(hcl, &tok_type) <= -1)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_HASHLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
				"invalid hash-marked literal %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
			return -1;
		}
		else
		{
			FEED_WRAP_UP (hcl, tok_type);
			goto not_consumed;
		}
	}
	else
	{
		ADD_TOKEN_CHAR (hcl, c);
		hi->char_count++;
		goto consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_hmarked_number (hcl_t* hcl, hcl_ooci_t c)
{
	hcl_flx_hn_t* rn = FLX_HN(hcl);

	if (CHAR_TO_NUM(c, rn->radix) >= rn->radix)
	{
		if (is_delimchar(c))
		{
			if (rn->digit_count == 0)
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
					"no valid digit after radix specifier in %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
				return -1;
			}
			else if (rn->invalid_digit_count > 0)
			{
				/* invalid as a radixed number, but this could be a hash-marked directive */
				hcl_iotok_type_t tok_type;

				if (get_directive_token_type(hcl, &tok_type) <= -1)
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
						"neither valid radixed number nor valid directive %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
					return -1;
				}
				else
				{
					FEED_WRAP_UP (hcl, tok_type);
					goto not_consumed;
				}
			}

			FEED_WRAP_UP (hcl, rn->tok_type);
			goto not_consumed;
		}
		else
		{
			ADD_TOKEN_CHAR(hcl, c);
			rn->digit_count++;
			rn->invalid_digit_count++;
			goto consumed;
		}
	}
	else
	{
		HCL_ASSERT (hcl, !is_delimchar(c));
		ADD_TOKEN_CHAR(hcl, c);
		rn->digit_count++;
		goto consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_plain_ident (hcl_t* hcl, hcl_ooci_t c) /* identifier */
{
	hcl_flx_pi_t* pi = FLX_PI(hcl);

	if (is_delimchar(c)) /* [NOTE] . is one of the delimiter character */
	{
		hcl_oow_t start;
		hcl_oocs_t seg;
		hcl_iotok_type_t tok_type;

		if (pi->seg_len == 0)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_MSEGIDENT, TOKEN_LOC(hcl), TOKEN_NAME(hcl), "blank segment");
			return -1;
		}

		start = TOKEN_NAME_LEN(hcl) - pi->seg_len;
		seg.ptr = &TOKEN_NAME_CHAR(hcl, start);
		seg.len = pi->seg_len;
		tok_type = classify_ident_token(hcl, &seg);
		if (tok_type != HCL_IOTOK_IDENT) 
		{
			pi->non_ident_seg_count++;
			pi->last_non_ident_type = tok_type;
		}

		pi->seg_len = 0; /* the length of the segment to be worked on */
		pi->seg_count++; /* total number of segments completed */

		if (c == '.')
		{
			/* move on to the next segment */
			ADD_TOKEN_CHAR(hcl, c);
			pi->char_count++;
			goto consumed;
		}

		/* finish */
		if (pi->non_ident_seg_count > 0)
		{
			if (pi->seg_count == 1)
			{
				FEED_WRAP_UP (hcl, pi->last_non_ident_type);
				goto not_consumed;
			}
			else
			{
				hcl_setsynerr (hcl, HCL_SYNERR_MSEGIDENT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				return -1;
			}
		}

		FEED_WRAP_UP (hcl, (pi->seg_count == 1? HCL_IOTOK_IDENT: HCL_IOTOK_IDENT_DOTTED));
		goto not_consumed;
	}
	else
	{
		ADD_TOKEN_CHAR(hcl, c);
		pi->char_count++;
		pi->seg_len++;
		goto consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_plain_number (hcl_t* hcl, hcl_ooci_t c) /* number */
{
	hcl_flx_pn_t* pn = FLX_PN(hcl);

	if (is_digitchar(c))
	{
		ADD_TOKEN_CHAR (hcl, c);
		pn->digit_count[pn->fpdec]++;
		goto consumed;
	}
	else
	{
		if (!pn->fpdec && c == '.')
		{
			pn->fpdec = 1;
			ADD_TOKEN_CHAR (hcl, c);
			goto consumed;
		}

		if (pn->digit_count[0] == 0)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl), "invalid numeric literal with no digit before decimal point");
			return -1;
		}
		else if (pn->fpdec && pn->digit_count[1] == 0)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_NUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl), "invalid numeric literal with no digit after decimal point");
			return -1;
		}

		FEED_WRAP_UP (hcl, (pn->fpdec? HCL_IOTOK_FPDECLIT: HCL_IOTOK_NUMLIT));
		goto not_consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

static int flx_quoted_token (hcl_t* hcl, hcl_ooci_t c) /* string, character */
{
	hcl_flx_qt_t* qt = FLX_QT(hcl);

	if (c == HCL_OOCI_EOF)
	{
		hcl_setsynerr (hcl, qt->synerr_code, TOKEN_LOC(hcl) /*FLX_LOC(hcl) instead?*/, HCL_NULL);
		return -1;
	}

	if (qt->escaped == 3)
	{
		if (c >= '0' && c <= '7')
		{
			/* more octal digits */
			qt->c_acc = qt->c_acc * 8 + c - '0';
			qt->digit_count++;
			if (qt->digit_count >= qt->escaped)
			{
				/* should i limit the max to 0xFF/0377?
				 * if (qt->c_acc > 0377) qt->c_acc = 0377;*/
				ADD_TOKEN_CHAR (hcl, qt->c_acc);
				qt->escaped = 0;
			}
			goto consumed;
		}
		else
		{
			ADD_TOKEN_CHAR (hcl, qt->c_acc);
			qt->escaped = 0;
		}
	}
	else if (qt->escaped == 2 || qt->escaped == 4 || qt->escaped == 8)
	{
		if (c >= '0' && c <= '9')
		{
			qt->c_acc = qt->c_acc * 16 + c - '0';
			qt->digit_count++;
			if (qt->digit_count >= qt->escaped)
			{
				ADD_TOKEN_CHAR (hcl, qt->c_acc);
				qt->escaped = 0;
			}
			goto consumed;
		}
		else if (c >= 'A' && c <= 'F')
		{
			qt->c_acc = qt->c_acc * 16 + c - 'A' + 10;
			qt->digit_count++;
			if (qt->digit_count >= qt->escaped)
			{
				ADD_TOKEN_CHAR (hcl, qt->c_acc);
				qt->escaped = 0;
			}
			goto consumed;
		}
		else if (c >= 'a' && c <= 'f')
		{
			qt->c_acc = qt->c_acc * 16 + c - 'a' + 10;
			qt->digit_count++;
			if (qt->digit_count >= qt->escaped)
			{
				ADD_TOKEN_CHAR (hcl, qt->c_acc);
				qt->escaped = 0;
			}
			goto consumed;
		}
		else
		{
			hcl_ooch_t rc;
			rc = (qt->escaped == 2)? 'x':
				 (qt->escaped == 4)? 'u': 'U';
			if (qt->digit_count == 0)
				ADD_TOKEN_CHAR (hcl, rc);
			else ADD_TOKEN_CHAR (hcl, qt->c_acc);

			qt->escaped = 0;
		}
	}

	if (qt->escaped == 0 && c == qt->end_char)
	{
		/* terminating quote */
		FEED_WRAP_UP (hcl, qt->tok_type); /* HCL_IOTOK_STRLIT or HCL_IOTOK_CHARLIT */
		if (TOKEN_NAME_LEN(hcl) < qt->min_len)
		{
			hcl_setsynerr (hcl, qt->synerr_code, TOKEN_LOC(hcl), HCL_NULL);
			return -1;
		}
		goto consumed;
	}

	if (qt->escaped == 0 && c == qt->esc_char)
	{
		qt->escaped = 1;
		goto consumed;
	}

	if (qt->escaped == 1)
	{
		if (c == 'a') c = '\a';
		else if (c == 'b') c = '\b';
		else if (c == 'f') c = '\f';
		else if (c == 'n') c = '\n';
		else if (c == 'r') c = '\r';
		else if (c == 't') c = '\t';
		else if (c == 'v') c = '\v';
		else if (c >= '0' && c <= '7' && !qt->regex)
		{
			/* i don't support the octal notation for a regular expression.
			 * it conflicts with the backreference notation between \1 and \7 inclusive. */
			qt->escaped = 3;
			qt->digit_count = 1;
			qt->c_acc = c - '0';
			goto consumed;
		}
		else if (c == 'x')
		{
			qt->escaped = 2;
			qt->digit_count = 0;
			qt->c_acc = 0;
			goto consumed;
		}
	#if (HCL_SIZEOF_OOCH_T >= 2)
		else if (c == 'u')
		{
			qt->escaped = 4;
			qt->digit_count = 0;
			qt->c_acc = 0;
			goto consumed;
		}
	#endif
	#if (HCL_SIZEOF_OOCH_T >= 4)
		else if (c == 'U')
		{
			qt->escaped = 8;
			qt->digit_count = 0;
			qt->c_acc = 0;
			goto consumed;
		}
	#endif
		else if (qt->regex)
		{
			/* if the following character doesn't compose a proper
			 * escape sequence, keep the escape character.
			 * an unhandled escape sequence can be handled
			 * outside this function since the escape character
			 * is preserved.*/
			ADD_TOKEN_CHAR (hcl, qt->esc_char);
		}

		qt->escaped = 0;
	}

	ADD_TOKEN_CHAR (hcl, c);

consumed:
	if (TOKEN_NAME_LEN(hcl) > qt->max_len)
	{
		hcl_setsynerr (hcl, qt->synerr_code, TOKEN_LOC(hcl), HCL_NULL);
		return -1;
	}
	return 1;
}

static int flx_signed_token (hcl_t* hcl, hcl_ooci_t c)
{
	hcl_flx_st_t* st = FLX_ST(hcl);

	if (st->char_count == 0 && c == '#')
	{
		ADD_TOKEN_CHAR (hcl, c);
		st->hmarked = 1;
		st->char_count++;
		goto consumed;
	}

	if (st->hmarked)
	{
		HCL_ASSERT (hcl, st->char_count == 1);

		if (c == 'b' || c == 'o' || c == 'x')
		{
			init_flx_hn (FLX_HN(hcl), HCL_IOTOK_RADNUMLIT, HCL_SYNERR_NUMLIT, (c == 'b'? 2: (c == 'o'? 8: 16)));
			FEED_CONTINUE_WITH_CHAR (hcl, c, HCL_FLX_HMARKED_NUMBER);
			goto consumed;
		}
		else
		{
			/* at this point, the token name buffer holds +# or -# */
			HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) == 2);
			TOKEN_NAME_LEN(hcl)--; /* remove the ending # from the name buffer */
			FEED_WRAP_UP (hcl, HCL_IOTOK_IDENT);

			/* reset the token information as if it enters HMARKED_TOKEN from START */
			reset_flx_token (hcl);

			/* the current character is on the same line as the hash mark, the column must be greater than 1 */
			HCL_ASSERT (hcl, FLX_LOC(hcl)->colm > 1); 
			FLX_LOC(hcl)->colm--; /* move back one character location by decrementing the column number */
			ADD_TOKEN_CHAR (hcl, '#');
			FEED_CONTINUE (hcl, HCL_FLX_HMARKED_TOKEN);
			goto not_consumed;
		}
	}

	HCL_ASSERT (hcl, st->char_count == 0);
	if (is_digitchar(c))
	{
		init_flx_pn (FLX_PN(hcl)); /* the sign is not part of the pn->digit_count[0] so keep it at 0 here */
		FEED_CONTINUE (hcl, HCL_FLX_PLAIN_NUMBER);
		goto not_consumed;
	}
	else
	{
		init_flx_pi (FLX_PI(hcl));

		/* the sign is already in the token name buffer. 
		 * adjust the state data for the sign. */
		HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) == 1);
		FLX_PI(hcl)->char_count++; 
		FLX_PI(hcl)->seg_len++;

		/* let refeeding of 'c' happen at the next iteration */
		FEED_CONTINUE (hcl, HCL_FLX_PLAIN_IDENT);
		goto not_consumed;
	}

consumed:
	return 1;

not_consumed:
	return 0;
}

/* ------------------------------------------------------------------------ */

static int feed_char (hcl_t* hcl, hcl_ooci_t c)
{
/*hcl_logbfmt (hcl, HCL_LOG_STDERR, "FEED->[%jc] %d STATE->%d\n", c, c, FLX_STATE(hcl));*/
	switch (FLX_STATE(hcl))
	{
		case HCL_FLX_START:            return flx_start(hcl, c);
		case HCL_FLX_COMMENT:          return flx_comment(hcl, c);
		case HCL_FLX_DELIM_TOKEN:      return flx_delim_token(hcl, c);
		case HCL_FLX_HMARKED_TOKEN:    return flx_hmarked_token(hcl, c);
		case HCL_FLX_HMARKED_CHAR:     return flx_hmarked_char(hcl, c);
		case HCL_FLX_HMARKED_IDENT:    return flx_hmarked_ident(hcl, c);
		case HCL_FLX_HMARKED_NUMBER:   return flx_hmarked_number(hcl, c);
		case HCL_FLX_PLAIN_IDENT:      return flx_plain_ident(hcl, c);
		case HCL_FLX_PLAIN_NUMBER:     return flx_plain_number(hcl, c);
		case HCL_FLX_QUOTED_TOKEN:     return flx_quoted_token(hcl, c);
		case HCL_FLX_SIGNED_TOKEN:     return flx_signed_token(hcl, c);

		default:
			/* unknown state */
			break;
	}

	HCL_ASSERT (hcl, !"internal error - this must never happen");
	hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - unknown flx state - %d", FLX_STATE(hcl));
	return -1;
}

static void feed_update_lx_loc (hcl_t* hcl, hcl_ooci_t ch)
{
	if (is_linebreak(ch))
	{
		hcl->c->feed.lx.loc.line++;
		hcl->c->feed.lx.loc.colm = 1;
	}
	else
	{
		hcl->c->feed.lx.loc.colm++;
	}
}

static int feed_from_included (hcl_t* hcl)
{
	int x;

	HCL_ASSERT (hcl, hcl->c->curinp != HCL_NULL && hcl->c->curinp != &hcl->c->inarg);

	do
	{
		if (hcl->c->curinp->b.pos >= hcl->c->curinp->b.len)
		{
			if (hcl->c->reader(hcl, HCL_IO_READ, hcl->c->curinp) <= -1)
			{
				return -1;
			}

			if (hcl->c->curinp->xlen <= 0)
			{
				/* got EOF from an included stream */
				feed_end_include (hcl);
				continue;
			}

			hcl->c->curinp->b.pos = 0;
			hcl->c->curinp->b.len = hcl->c->curinp->xlen;
		}

		x = feed_char(hcl, hcl->c->curinp->buf[hcl->c->curinp->b.pos]);
		if (x <= -1) return -1;
		if (x >= 1) 
		{
			/* consumed */
			feed_update_lx_loc (hcl, hcl->c->curinp->buf[hcl->c->curinp->b.pos]);
			hcl->c->curinp->b.pos += x;
		}

		if (hcl->c->feed.rd.do_include_file)
		{
			/* feed_process_token(), called for the "filename" token for the #include 
			 * directive, sets hcl->c->feed.rd.do_include_file to 1 instead of attepmting
			 * to include the file. the file inclusion is attempted here after the return
			 * value of feed_char() is used to advance the hcl->c->curinp->b.pos pointer. */
			hcl->c->feed.rd.do_include_file = 0; /* clear this regardless of inclusion result */
			if (feed_begin_include(hcl) <= -1) return -1;
		}
	}
	while (hcl->c->curinp != &hcl->c->inarg);

	return 0;
}

void hcl_beginfeed (hcl_t* hcl, hcl_on_cnode_t on_cnode)
{
	init_feed (hcl);
	if (on_cnode) hcl->c->feed.on_cnode = on_cnode;
	/* if you pass HCL_NULL for on_cnode, hcl->c->feed.on_cnode resets 
	 * back to the default handler in init_feed() */
}

int hcl_feed (hcl_t* hcl, const hcl_ooch_t* data, hcl_oow_t len)
{
/* TODO: need to return the number of processed characters?
 *       need to stop after the first complete expression? */

	hcl_oow_t i;
	int x;

	if (data) 
	{
		for (i = 0; i < len; ) 
		{
			x = feed_char(hcl, data[i]);
			if (x <= -1) return -1; /* TODO: return the number of processed characters via an argument? */
			
			if (x > 0)
			{
				/* consumed */
				feed_update_lx_loc (hcl, data[i]);
				i += x; /* x is supposed to be 1. otherwise, some characters may get skipped. */
			}

			if (hcl->c->feed.rd.do_include_file)
			{
				if (feed_begin_include(hcl) <= -1) return -1;
				hcl->c->feed.rd.do_include_file = 0;
			}

			if (hcl->c->curinp != &hcl->c->inarg && feed_from_included(hcl) <= -1) 
			{
				/* TODO: return the number of processed characters via an argument? */
				return -1;
			}

			/* feed data[i] again if not consumed */
		}
	}
	else
	{
		for (i = 0; i < 1;) /* weird loop in case feed_char() returns 0 */
		{
			x = feed_char(hcl, HCL_OOCI_EOF);
			if (x <= -1) 
			{
				if (hcl->c->feed.rd.level <= 0 && hcl_geterrnum(hcl) == HCL_ESYNERR && hcl_getsynerrnum(hcl) == HCL_SYNERR_EOF)
				{
					/* convert this EOF error to success as the caller knows EOF in the feed mode. 
					 * the caller can safely stop feeding after gettting success from hcl_feed(hcl, HCL_NULL, 0); 
					 * in the feed mode, this function doesn't set HCL_EFINIS. */
					x = 1;
				}
				else
				{
					return -1;
				}
			}
			i += x;
		}
	}

	return 0;
}


/*
hcl_setopt (ON_EXPRESSION CALLBACK??? );



hcl_feed (hcl, "(hello) (10)", 12);
	> on_token
	> on_expression
	> on_eof

default callback for on_expression?
	compile
	execute??/ if in the interactive mode? (say it's used as a network protocol. execute each expression when received....)

default callback for on_eof?
 	execute or terminate?


*/


/* ------------------------------------------------------------------------ */

/* TODO: rename compiler to something else that can include reader, printer, and compiler
 * move compiler intialization/finalization here to more common place */

static void gc_compiler (hcl_t* hcl)
{
	hcl->c->r.s = hcl_moveoop(hcl, hcl->c->r.s);
	hcl->c->r.e = hcl_moveoop(hcl, hcl->c->r.e);
}

static void fini_compiler (hcl_t* hcl)
{
	/* called before the hcl object is closed */
	if (hcl->c)
	{
		if (hcl->c->cfs.ptr)
		{
			hcl_freemem (hcl, hcl->c->cfs.ptr);
			hcl->c->cfs.ptr = HCL_NULL;
			hcl->c->cfs.top = -1;
			hcl->c->cfs.capa = 0;
		}

		if (hcl->c->tv.s.ptr)
		{
			hcl_freemem (hcl, hcl->c->tv.s.ptr);
			hcl->c->tv.s.ptr = HCL_NULL;
			hcl->c->tv.s.len = 0;
			hcl->c->tv.capa = 0;
			hcl->c->tv.wcount = 0;
		}
		HCL_ASSERT (hcl, hcl->c->tv.capa == 0);
		HCL_ASSERT (hcl, hcl->c->tv.wcount == 0);

		if (hcl->c->cblk.info)
		{
			hcl_freemem (hcl, hcl->c->cblk.info);
			hcl->c->cblk.info = HCL_NULL;
			hcl->c->cblk.info_capa = 0;
			hcl->c->cblk.depth = -1;
		}

		if (hcl->c->clsblk.info)
		{
			hcl_freemem (hcl, hcl->c->clsblk.info);
			hcl->c->clsblk.info = HCL_NULL;
			hcl->c->clsblk.info_capa = 0;
			hcl->c->clsblk.depth = -1;
		}

		if (hcl->c->fnblk.info)
		{
			hcl_freemem (hcl, hcl->c->fnblk.info);
			hcl->c->fnblk.info = HCL_NULL;
			hcl->c->fnblk.info_capa = 0;
			hcl->c->fnblk.depth = -1;
		}

		clear_io_names (hcl);
		if (hcl->c->tok.name.ptr) hcl_freemem (hcl, hcl->c->tok.name.ptr);

		hcl_detachio (hcl);

		hcl_freemem (hcl, hcl->c);
		hcl->c = HCL_NULL;
	}
}

int hcl_attachio (hcl_t* hcl, hcl_ioimpl_t reader, hcl_ioimpl_t printer)
{
	int n;
	hcl_cb_t* cbp = HCL_NULL;

	if (!reader || !printer)
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "reader and/or printer not supplied");
		return -1;
	}

	if (!hcl->c)
	{
		hcl_cb_t cb;

		HCL_MEMSET (&cb, 0, HCL_SIZEOF(cb));
		cb.gc = gc_compiler;
		cb.fini = fini_compiler;
		cbp = hcl_regcb(hcl, &cb);
		if (!cbp) return -1;

		hcl->c = (hcl_compiler_t*)hcl_callocmem(hcl, HCL_SIZEOF(*hcl->c));
		if (HCL_UNLIKELY(!hcl->c))
		{
			hcl_deregcb (hcl, cbp);
			return -1;
		}

		hcl->c->ilchr_ucs.ptr = &hcl->c->ilchr;
		hcl->c->ilchr_ucs.len = 1;

		hcl->c->r.s = hcl->_nil;
		hcl->c->r.e = hcl->_nil;

		hcl->c->cfs.top = -1;
		hcl->c->cblk.depth = -1;
		hcl->c->clsblk.depth = -1;
		hcl->c->fnblk.depth = -1;

		init_feed (hcl);
	}
	else if (hcl->c->reader || hcl->c->printer)
	{
		hcl_seterrnum (hcl, HCL_EPERM); /* TODO: change this error code */
		return -1;
	}

	/* Some IO names could have been stored in earlier calls to this function.
	 * I clear such names before i begin this function. i don't clear it
	 * at the end of this function because i may be referenced as an error
	 * location */
	clear_io_names (hcl);

	/* initialize some key fields */
	hcl->c->printer = printer;
	hcl->c->reader = reader;
	hcl->c->nungots = 0;

	/* The name field and the includer field are HCL_NULL
	 * for the main stream */
	HCL_MEMSET (&hcl->c->inarg, 0, HCL_SIZEOF(hcl->c->inarg));
	hcl->c->inarg.line = 1;
	hcl->c->inarg.colm = 1;

	/* open the top-level stream */
	n = hcl->c->reader(hcl, HCL_IO_OPEN, &hcl->c->inarg);
	if (n <= -1) goto oops;

	HCL_MEMSET (&hcl->c->outarg, 0, HCL_SIZEOF(hcl->c->outarg));
	n = hcl->c->printer(hcl, HCL_IO_OPEN, &hcl->c->outarg);
	if (n <= -1)
	{
		hcl->c->reader (hcl, HCL_IO_CLOSE, &hcl->c->inarg);
		goto oops;
	}

	/* the stream is open. set it as the current input stream */
	hcl->c->curinp = &hcl->c->inarg;
	return 0;

oops:
	if (cbp)
	{
		hcl_deregcb (hcl, cbp);
		hcl_freemem (hcl, hcl->c);
		hcl->c = HCL_NULL;
	}
	else
	{
		hcl->c->printer = HCL_NULL;
		hcl->c->reader = HCL_NULL;
	}
	return -1;
}

void hcl_flushio (hcl_t* hcl)
{
	if (hcl->c)
	{
		if (hcl->c->printer) hcl->c->printer (hcl, HCL_IO_FLUSH, &hcl->c->outarg);
	}
}

void hcl_detachio (hcl_t* hcl)
{
	/* an error occurred and control has reached here
	 * probably, some included files might not have been
	 * closed. close them */

	if (hcl->c)
	{
		if (hcl->c->reader)
		{
			while (hcl->c->curinp != &hcl->c->inarg)
			{
				hcl_ioinarg_t* prev;

				/* nothing much to do about a close error */
				hcl->c->reader (hcl, HCL_IO_CLOSE, hcl->c->curinp);

				prev = hcl->c->curinp->includer;
				HCL_ASSERT (hcl, hcl->c->curinp->name != HCL_NULL);
				hcl_freemem (hcl, hcl->c->curinp);
				hcl->c->curinp = prev;
			}

			hcl->c->reader (hcl, HCL_IO_CLOSE, hcl->c->curinp);
			hcl->c->reader = HCL_NULL; /* ready for another attachment */
		}

		if (hcl->c->printer)
		{
			hcl->c->printer (hcl, HCL_IO_CLOSE, &hcl->c->outarg);
			hcl->c->printer = HCL_NULL; /* ready for another attachment */
		}
	}
}

hcl_ioinarg_t* hcl_getbaseinarg (hcl_t* hcl)
{
	return &hcl->c->inarg;
}

void hcl_setbaseinloc (hcl_t* hcl, hcl_oow_t line, hcl_oow_t colm)
{
	hcl->c->inarg.line = line;
	hcl->c->inarg.colm = colm;
}

hcl_iolxc_t* hcl_readbaseinchar (hcl_t* hcl)
{
	/* read a character using the base input stream. the caller must care extra 
	 * care when using this function. this function reads the main stream regardless
	 * of the inclusion status and ignores the ungot characters. */
	int n = _get_char(hcl, &hcl->c->inarg);
	if (n <= -1) return HCL_NULL;
	return &hcl->c->inarg.lxc;
}
