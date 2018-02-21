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
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAfRRANTIES
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
} vocas[] = {
	
	{  6, { '#','f','a','l','s','e'                                       } },
	{  8, { '#','i','n','c','l','u','d','e'                               } },
	{  4, { '#','n','i','l'                                               } },
	{  5, { '#','t','r','u','e'                                           } },
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
	{  5, { '<','E','O','F','>'                                           } }
};

enum voca_id_t
{
	VOCA_FALSE,
	VOCA_INCLUDE,
	VOCA_NIL,
	VOCA_TRUE,

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

	VOCA_EOF
};
typedef enum voca_id_t voca_id_t;


enum list_flag_t
{
	QUOTED     = (1 << 0),
	DOTTED     = (1 << 1),
	CLOSED     = (1 << 2)
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
			hcl_seterrnum (hcl, HCL_EINVAL);
			return -1;
		}
		ptr++; /* skip '#' */
		
		if (*ptr == 'x') base = 16;
		else if (*ptr == 'o') base = 8;
		else if (*ptr == 'b') base = 2;
		else
		{
			hcl_seterrnum (hcl, HCL_EINVAL);
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
			hcl_seterrnum (hcl, HCL_ERANGE);
			return -1;
		}
		old_value = value;
		ptr++;
	}

	if (ptr < end)
	{
		/* trailing garbage? */
		hcl_seterrnum (hcl, HCL_EINVAL);
		return -1;
	}

	if (value > HCL_TYPE_MAX(hcl_ooi_t) + (negsign? 1: 0)) /* assume 2's complement */
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
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
			hcl_seterrnum (hcl, HCL_EINVAL);
			return HCL_NULL;
		}
		ptr++; /* skip '#' */
		
		if (*ptr == 'x') base = 16;
		else if (*ptr == 'o') base = 8;
		else if (*ptr == 'b') base = 2;
		else
		{
			hcl_seterrnum (hcl, HCL_EINVAL);
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

static HCL_INLINE int is_delimiter (hcl_ooci_t c)
{
	return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '\"' || c == '\'' || c == '#' || c == ';' || c == '|' || c == '.' || is_spacechar(c) || c == HCL_UCI_EOF;
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

		tmp = hcl_reallocmem (hcl, dst->ptr, HCL_SIZEOF(*tmp) * capa);
		if (!tmp)  return -1;

		dst->ptr = tmp;
		*dst_capa = capa;
	}

	if (append && add_delim) dst->ptr[pos++] = add_delim;
	hcl_copyoochars (&dst->ptr[pos], src->ptr, src->len);
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

#define ADD_TOKEN_STR(hcl,s,l) \
	do { if (add_token_str(hcl, s, l) <= -1) return -1; } while (0)

#define ADD_TOKEN_CHAR(hcl,c) \
	do { if (add_token_char(hcl, c) <= -1) return -1; } while (0)

#define CLEAR_TOKEN_NAME(hcl) ((hcl)->c->tok.name.len = 0)
#define SET_TOKEN_TYPE(hcl,tv) ((hcl)->c->tok.type = (tv))

#define TOKEN_TYPE(hc) ((hcl)->c->tok.type)
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
	return copy_string_to (hcl, &tmp, TOKEN_NAME(hcl), &TOKEN_NAME_CAPA(hcl), 1, '\0');
}

static HCL_INLINE int does_token_name_match (hcl_t* hcl, voca_id_t id)
{
	return hcl->c->tok.name.len == vocas[id].len &&
	       hcl_equaloochars(hcl->c->tok.name.ptr, vocas[id].str, vocas[id].len);
}

static HCL_INLINE int add_token_char (hcl_t* hcl, hcl_ooch_t c)
{
	hcl_oocs_t tmp;

	tmp.ptr = &c;
	tmp.len = 1;
	return copy_string_to (hcl, &tmp, TOKEN_NAME(hcl), &TOKEN_NAME_CAPA(hcl), 1, '\0');
}

static HCL_INLINE void unget_char (hcl_t* hcl, const hcl_iolxc_t* c)
{
	/* Make sure that the unget buffer is large enough */
	HCL_ASSERT (hcl, hcl->c->nungots < HCL_COUNTOF(hcl->c->ungot));
	hcl->c->ungot[hcl->c->nungots++] = *c;
}

static int get_char (hcl_t* hcl)
{
	hcl_ooi_t n;
	hcl_ooci_t lc;

	if (hcl->c->nungots > 0)
	{
		/* something in the unget buffer */
		hcl->c->lxc = hcl->c->ungot[--hcl->c->nungots];
		return 0;
	}

	if (hcl->c->curinp->b.state == -1) 
	{
		hcl->c->curinp->b.state = 0;
		return -1;
	}
	else if (hcl->c->curinp->b.state == 1) 
	{
		hcl->c->curinp->b.state = 0;
		goto return_eof;
	}

	if (hcl->c->curinp->b.pos >= hcl->c->curinp->b.len)
	{
		n = hcl->c->reader (hcl, HCL_IO_READ, hcl->c->curinp);
		if (n <= -1) return -1;
		
		if (n == 0)
		{
		return_eof:
			hcl->c->curinp->lxc.c = HCL_UCI_EOF;
			hcl->c->curinp->lxc.l.line = hcl->c->curinp->line;
			hcl->c->curinp->lxc.l.colm = hcl->c->curinp->colm;
			hcl->c->curinp->lxc.l.file = hcl->c->curinp->name;
			hcl->c->lxc = hcl->c->curinp->lxc;

			/* indicate that EOF has been read. lxc.c is also set to EOF. */
			return 0; 
		}

		hcl->c->curinp->b.pos = 0;
		hcl->c->curinp->b.len = n;
	}

	if (hcl->c->curinp->lxc.c == '\n' || hcl->c->curinp->lxc.c == '\r')
	{
		/* hcl->c->curinp->lxc.c is a previous character. the new character
		 * to be read is still in the buffer (hcl->c->curinp->buf).
		 * hcl->cu->curinp->colm has been incremented when the previous
		 * character has been read. */
		if (hcl->c->curinp->line > 1 && hcl->c->curinp->colm == 2 && hcl->c->curinp->nl != hcl->c->curinp->lxc.c) 
		{
			/* most likely, it's the second character in '\r\n' or '\n\r' 
			 * sequence. let's not update the line and column number. */
			/*hcl->c->curinp->colm = 1;*/
		}
		else
		{
			/* if the previous charater was a newline,
			 * increment the line counter and reset column to 1.
			 * incrementing the line number here instead of
			 * updating inp->lxc causes the line number for
			 * TOK_EOF to be the same line as the lxc newline. */
			hcl->c->curinp->line++;
			hcl->c->curinp->colm = 1;
			hcl->c->curinp->nl = hcl->c->curinp->lxc.c;
		}
	}

	lc = hcl->c->curinp->buf[hcl->c->curinp->b.pos++];

	hcl->c->curinp->lxc.c = lc;
	hcl->c->curinp->lxc.l.line = hcl->c->curinp->line;
	hcl->c->curinp->lxc.l.colm = hcl->c->curinp->colm++;
	hcl->c->curinp->lxc.l.file = hcl->c->curinp->name;
	hcl->c->lxc = hcl->c->curinp->lxc;

	return 1; /* indicate that a normal character has been read */
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
			if (c == HCL_UCI_EOF)
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

static int get_string (hcl_t* hcl, hcl_ooch_t end_char, hcl_ooch_t esc_char, int regex, hcl_oow_t preescaped)
{
	hcl_ooci_t c;
	hcl_oow_t escaped = preescaped;
	hcl_oow_t digit_count = 0;
	hcl_ooci_t c_acc = 0;

	SET_TOKEN_TYPE (hcl, HCL_IOTOK_STRLIT);

	while (1)
	{
		GET_CHAR_TO (hcl, c);

		if (c == HCL_UCI_EOF)
		{
			hcl_setsynerr (hcl, HCL_SYNERR_STRNC, TOKEN_LOC(hcl) /*LEXER_LOC(hcl)*/, HCL_NULL);
			return -1;
		}

		if (escaped == 3)
		{
			if (c >= '0' && c <= '7')
			{
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
			if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
			else if (c == 't') c = '\t';
			else if (c == 'f') c = '\f';
			else if (c == 'b') c = '\b';
			else if (c == 'v') c = '\v';
			else if (c == 'a') c = '\a';
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
			else if (c == 'u' && HCL_SIZEOF(hcl_ooch_t) >= 2) 
			{
				escaped = 4;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
			else if (c == 'U' && HCL_SIZEOF(hcl_ooch_t) >= 4) 
			{
				escaped = 8;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
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

static int get_radix_number (hcl_t* hcl, hcl_ooci_t rc, int radix)
{
	hcl_ooci_t c;

	ADD_TOKEN_CHAR (hcl, '#');
	ADD_TOKEN_CHAR (hcl, rc);

	GET_CHAR_TO (hcl, c);

	if (CHAR_TO_NUM(c, radix) >= radix)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_RADNUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl), 
			"no digit after radix specifier in %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
		return -1;
	}

	do
	{
		ADD_TOKEN_CHAR(hcl, c);
		GET_CHAR_TO (hcl, c);
	}
	while (CHAR_TO_NUM(c, radix) < radix);

	if (!is_delimiter(c))
	{
		do
		{
			ADD_TOKEN_CHAR(hcl, c);
			GET_CHAR_TO (hcl, c);
		}
		while (!is_delimiter(c));

		hcl_setsynerrbfmt (hcl, HCL_SYNERR_RADNUMLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
			"invalid digit in radixed number in %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
		return -1;
	}

	unget_char (hcl, &hcl->c->lxc);
	SET_TOKEN_TYPE (hcl, HCL_IOTOK_RADNUMLIT);

	return 0;
}

static int get_quoted_token (hcl_t* hcl)
{
	hcl_ooci_t c;

	HCL_ASSERT (hcl, hcl->c->lxc.c == '\'');

	GET_CHAR_TO (hcl, c);

	switch (c)
	{
		case '(':
			ADD_TOKEN_CHAR (hcl, '\'');
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_QPAREN);
			break;

		default:
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ILCHR, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
				"invalid quoted token character %jc", c);
			return -1;
	}

	return 0;
}

static int get_sharp_token (hcl_t* hcl)
{
	hcl_ooci_t c;
	int radix;

	HCL_ASSERT (hcl, hcl->c->lxc.c == '#');

	GET_CHAR_TO (hcl, c);

	/*
	 * #bBBBB binary
	 * #oOOOO octal 
	 * #xXXXX hexadecimal
	 * #nil
	 * #true
	 * #false
	 * #include
	 * #\C      character
	 * #\XHHHH  unicode character
	 * #\xHHHH  unicode character
	 * #\UHHHH  unicode character
	 * #\uHHHH  unicode character
	 * #\EDDD   error
	 * #\eDDD   error
	 * #\PHHH   smptr
	 * #\pHHH   smptr
	 * #( )     array
	 * #[ ]     byte array
	 * #{ }     dictionary
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
			if (get_radix_number (hcl, c, radix) <= -1) return -1;
			break;

		case '\\': /* character literal */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR (hcl, '\\');

			GET_CHAR_TO (hcl, c);
			if (is_delimiter(c))
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
			while (!is_delimiter(c));

			if (TOKEN_NAME_LEN(hcl) >= 4)
			{
				if (TOKEN_NAME_CHAR(hcl, 2) == 'P' || TOKEN_NAME_CHAR(hcl, 2) == 'p')
				{
					SET_TOKEN_TYPE (hcl, HCL_IOTOK_SMPTRLIT);
					goto hexcharlit;
				}
				else if (TOKEN_NAME_CHAR(hcl, 2) == 'X' || TOKEN_NAME_CHAR(hcl, 2) == 'x' ||
				         TOKEN_NAME_CHAR(hcl, 2) == 'U' || TOKEN_NAME_CHAR(hcl, 2) == 'u')
				{
					hcl_oow_t i;

				hexcharlit:
					c = 0;
					for (i = 3; i < TOKEN_NAME_LEN(hcl); i++)
					{
						if (!is_xdigitchar(TOKEN_NAME_CHAR(hcl, i)))
						{
							hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
								"invalid hexadecimal character in %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
							return -1;
						}
						c = c * 16 + CHAR_TO_NUM(hcl->c->tok.name.ptr[i], 16); /* don't care if it is for 'p' */
					}
				}
				else if (TOKEN_NAME_CHAR(hcl, 2) == 'E' || TOKEN_NAME_CHAR(hcl, 2) == 'e')
				{
					hcl_oow_t i;
					for (i = 3; i < TOKEN_NAME_LEN(hcl); i++)
					{
						if (!is_digitchar(TOKEN_NAME_CHAR(hcl, i)))
						{
							hcl_setsynerrbfmt (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
								"invalid decimal character in %.*js", TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
							return -1;
						}
					}
					SET_TOKEN_TYPE (hcl, HCL_IOTOK_ERRORLIT);
				}
				else if (does_token_name_match(hcl, VOCA_SPACE))
				{
					c = ' ';
				}
				else if (does_token_name_match(hcl, VOCA_NEWLINE))
				{
					/* TODO: convert it to host newline convention. how to handle if it's composed of 2 letters like \r\n? */
					c = '\n';
				}
				else if (does_token_name_match(hcl, VOCA_BACKSPACE))
				{
					c = '\b';
				}
				else if (does_token_name_match(hcl, VOCA_TAB))
				{
					c = '\t';
				}
				else if (does_token_name_match(hcl, VOCA_LINEFEED))
				{
					c = '\n';
				}
				else if (does_token_name_match(hcl, VOCA_PAGE))
				{
					c = '\f';
				}
				else if (does_token_name_match(hcl, VOCA_RETURN))
				{
					c = '\r';
				}
				else if (does_token_name_match(hcl, VOCA_NUL)) /* null character. not #nil */
				{
					c = '\0';
				}
				else if (does_token_name_match(hcl, VOCA_VTAB))
				{
					c = '\v';
				}
				else if (does_token_name_match(hcl, VOCA_RUBOUT))
				{
					c = '\x7F'; /* DEL */
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

		case '(': /* #( - array opener */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_APAREN);
			break;

		case '[': /* #[ - byte array opener */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_BAPAREN);
			break;

		case '{': /* #{ - dictionary opener */
			ADD_TOKEN_CHAR (hcl, '#');
			ADD_TOKEN_CHAR(hcl, c);
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_DPAREN);
			break;

		default:
			if (is_delimiter(c)) 
			{
				/* EOF, whitespace, etc */
				hcl_setsynerr (hcl, HCL_SYNERR_HASHLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				return -1;
			}

			ADD_TOKEN_CHAR (hcl, '#');
		long_name:
			do
			{
				ADD_TOKEN_CHAR (hcl, c);
				GET_CHAR_TO (hcl, c);
			} 
			while (!is_delimiter(c));

			if (does_token_name_match (hcl, VOCA_INCLUDE))
			{
				SET_TOKEN_TYPE (hcl, HCL_IOTOK_INCLUDE);
			}
			else if (does_token_name_match (hcl, VOCA_NIL))
			{
				SET_TOKEN_TYPE (hcl, HCL_IOTOK_NIL);
			}
			else if (does_token_name_match (hcl, VOCA_TRUE))
			{
				SET_TOKEN_TYPE (hcl, HCL_IOTOK_TRUE);
			}
			else if (does_token_name_match (hcl, VOCA_FALSE))
			{
				SET_TOKEN_TYPE (hcl, HCL_IOTOK_FALSE);
			}
			else
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_HASHLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl),
					"invalid hashed literal name %.*js", hcl->c->tok.name.len, hcl->c->tok.name.ptr);
				return -1;
			}

			unget_char (hcl, &hcl->c->lxc);
			break;
	}

	return 0;
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
	hcl->c->tok.loc = hcl->c->lxc.l; /* set token location */

	c = hcl->c->lxc.c;

	switch (c)
	{
		case HCL_UCI_EOF:
		{
			int n;

			n = end_include (hcl);
			if (n <= -1) return -1;
			if (n >= 1) goto retry;

			SET_TOKEN_TYPE (hcl, HCL_IOTOK_EOF);
			ADD_TOKEN_STR(hcl, vocas[VOCA_EOF].str, vocas[VOCA_EOF].len);
			break;
		}

		case '(':
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
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_DOT);
			ADD_TOKEN_CHAR (hcl, c);
			break;

		case '\"':
			if (get_string(hcl, '\"', '\\', 0, 0) <= -1) return -1;
			break;

		case '\'':
		#if 0
			if (get_quoted_token(hcl) <= -1) return -1;
		#else
			if (get_string(hcl, '\'', '\\', 0, 0) <= -1) return -1;
			if (hcl->c->tok.name.len != 1)
			{
				hcl_setsynerr (hcl, HCL_SYNERR_CHARLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				return -1;
			}
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_CHARLIT);
		#endif
			break;

		case '#':  
			if (get_sharp_token(hcl) <= -1) return -1;
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
						if (get_radix_number (hcl, c, radix) <= -1) return -1;
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
			/* TODO: floating-pointer number */
			SET_TOKEN_TYPE (hcl, HCL_IOTOK_NUMLIT);
			while (1)
			{
				ADD_TOKEN_CHAR (hcl, c);
				GET_CHAR_TO (hcl, c);
				if (!is_digitchar(c))
				{
					unget_char (hcl, &hcl->c->lxc);
					break;
				}
			}
			break;

		default:
		ident:
			if (is_delimiter(c))
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

					period = hcl->c->lxc;

				read_more_seg:
					GET_CHAR_TO (hcl, c);
					if (!is_delimiter(c))
					{
						SET_TOKEN_TYPE (hcl, HCL_IOTOK_IDENT_DOTTED);
						ADD_TOKEN_CHAR (hcl, '.');
						do
						{
							ADD_TOKEN_CHAR (hcl, c);
							GET_CHAR_TO (hcl, c);
						}
						while (!is_delimiter(c));

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
				else if (is_delimiter(c))
				{
					unget_char (hcl, &hcl->c->lxc);
					break;
				}
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

	link = (hcl_iolink_t*) hcl_callocmem (hcl, HCL_SIZEOF(*link) + HCL_SIZEOF(hcl_ooch_t) * (name->len + 1));
	if (!link) return HCL_NULL;

	ptr = (hcl_ooch_t*)(link + 1);

	hcl_copyoochars (ptr, name->ptr, name->len);
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

	io_name = add_io_name (hcl, TOKEN_NAME(hcl));
	if (!io_name) return -1;

	arg = (hcl_ioinarg_t*) hcl_callocmem (hcl, HCL_SIZEOF(*arg));
	if (!arg) goto oops;

	arg->name = io_name;
	arg->line = 1;
	arg->colm = 1;
	/*arg->nl = '\0';*/
	arg->includer = hcl->c->curinp;

	if (hcl->c->reader (hcl, HCL_IO_OPEN, arg) <= -1) 
	{
		hcl_setsynerr (hcl, HCL_SYNERR_INCLUDE, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		goto oops;
	}

#if 0
	GET_TOKEN (hcl);
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

	x = hcl->c->reader (hcl, HCL_IO_CLOSE, hcl->c->curinp);

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

static HCL_INLINE hcl_oop_t push (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oop_t cons;

	cons = hcl_makecons (hcl, obj, hcl->c->r.s);
	if (cons == HCL_NULL) return HCL_NULL;

	hcl->c->r.s = cons;

	/* return the top of the stack which is the containing cons cell */
	return cons;
}

static HCL_INLINE void pop (hcl_t* hcl)
{
	/* the stack is empty. called pop() more than push()? */
	HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,hcl->c->r.s));
	hcl->c->r.s = HCL_CONS_CDR(hcl->c->r.s);
}

static HCL_INLINE hcl_oop_t enter_list (hcl_t* hcl, int flagv)
{
	/* upon entering a list, it pushes three cells into a stack.
	 *
	 * stack(hcl->c->r.s)--+
	 *                  V
	 *             +---cons--+    
	 *         +------  |  -------+
	 *      car|   +---------+    |cdr
	 *         V                  |
	 *        nil#1               V
	 *                          +---cons--+
	 *                      +------  |  --------+
	 *                   car|   +---------+     |cdr
	 *                      v                   |
	 *                     nil#2                V
	 *                                       +---cons--+
	 *                                   +------  | --------+
	 *                                car|   +---------+    |cdr
	 *                                   V                  |
	 *                                flag number           V
	 *                                                  previous stack top
	 *
	 * nil#1 to store the first element in the list.
	 * nil#2 to store the last element in the list.
	 * both to be updated in chain_to_list() as items are added.
	 */

/* TODO: change to push array of 3 cells instead? or don't use the object memory for stack. use compiler's own memory... */
	return (push(hcl, HCL_SMOOI_TO_OOP(flagv)) == HCL_NULL ||
	        push(hcl, hcl->_nil) == HCL_NULL ||
	        push(hcl, hcl->_nil) == HCL_NULL)? HCL_NULL: hcl->c->r.s;
}

static HCL_INLINE hcl_oop_t leave_list (hcl_t* hcl, int* flagv, int* oldflagv)
{
	hcl_oop_t head;
	int fv, concode;

	/* the stack must not be empty - cannot leave a list without entering it */
	HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,hcl->c->r.s));
/* TODO: fix bug here. unblanced ) cause assertion failure here */

	/*head = HCL_CONS_CAR(HCL_CONS_CDR(hcl->c->r.s));*/

	/* upon leaving a list, it pops the three cells off the stack */
	pop (hcl);

	head = HCL_CONS_CAR(hcl->c->r.s);
	pop (hcl);

	fv = HCL_OOP_TO_SMOOI(HCL_CONS_CAR(hcl->c->r.s));
	concode = LIST_FLAG_GET_CONCODE(fv);
	pop (hcl);

#if 0
	/* TODO: literalize the list if all the elements are all literals */
	if (concode == HCL_CONCODE_ARRAY || concode == HCL_CONCODE_BYTEARRAY /*|| concode == HCL_CONCODE_DIC*/)
	{
		/* convert a list to an array */
		hcl_oop_oop_t arr;
		hcl_oop_t ptr;
		hcl_oow_t count;

		ptr = head;
		count = 0;
		while (ptr != hcl->_nil)
		{
			hcl_oop_t car;
			HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_BRAND(ptr) == HCL_BRAND_CONS);
			car = HCL_CONS_CAR(ptr);

			if (!HCL_OOP_IS_NUMERIC(car)) goto done;  /* TODO: check if the element is a literal properly here */

			ptr = HCL_CONS_CDR(ptr);
			count++;
		}

		hcl_pushtmp (hcl, &head);
		arr = (hcl_oop_oop_t)hcl_makearray(hcl, count);
		hcl_poptmp (hcl);
		if (!arr) return HCL_NULL;

		ptr = head;
		count = 0;
		while (ptr != hcl->_nil)
		{
			arr->slot[count++] = HCL_CONS_CAR(ptr);
			ptr = HCL_CONS_CDR(ptr);
		}

		head = (hcl_oop_t)arr;
	}
done:
#endif

	*oldflagv = fv;
	if (HCL_IS_NIL(hcl,hcl->c->r.s))
	{
		/* the stack is empty after popping. 
		 * it is back to the top level. 
		 * the top level can never be quoted. */
		*flagv = 0;
	}
	else
	{
		/* restore the flag for the outer returning level */
		hcl_oop_t flag = HCL_CONS_CDR(HCL_CONS_CDR(hcl->c->r.s));
		HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(HCL_CONS_CAR(flag)));
		*flagv = HCL_OOP_TO_SMOOI(HCL_CONS_CAR(flag));
	}

	/* return the head of the list being left */
	if (HCL_IS_NIL(hcl,head))
	{
		/* the list is empty. literalize the empty list according to
		 * the list opener. for a list, it is same as #nil. */
		switch (concode)
		{
			case HCL_CONCODE_ARRAY:
				return (hcl_oop_t)hcl_makearray(hcl, 0);
			case HCL_CONCODE_BYTEARRAY:
				return (hcl_oop_t)hcl_makebytearray(hcl, HCL_NULL, 0); 
			case HCL_CONCODE_DIC:
				return (hcl_oop_t)hcl_makedic(hcl, 100); /* TODO: default dictionary size for empty definition? */

			/* NOTE: empty xlist will get translated to #nil.
			 *       this is useful when used in the lambda expression to express an empty argument. also in defun.
			 *      (lambda () ...) is equivalent to  (lambda #nil ...) 
			 *      (defun x() ...) */
		}
	}

	if (HCL_IS_CONS(hcl,head)) HCL_OBJ_SET_FLAGS_SYNCODE(head, concode);
	return head;
}

static HCL_INLINE int can_dot_list (hcl_t* hcl)
{
	hcl_oop_t cons;
	int flagv;

	HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,hcl->c->r.s));

	/* mark the state that a dot has appeared in the list */
	cons = HCL_CONS_CDR(HCL_CONS_CDR(hcl->c->r.s));
	flagv = HCL_OOP_TO_SMOOI(HCL_CONS_CAR(cons));

	if (LIST_FLAG_GET_CONCODE(flagv) != HCL_CONCODE_QLIST) return 0;

	flagv |= DOTTED;
	HCL_CONS_CAR(cons) = HCL_SMOOI_TO_OOP(flagv);
	return 1;
}

static hcl_oop_t chain_to_list (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oop_t head, tail, flag;
	int flagv;

	/* the stack top is the pair pointing to the list tail */
	tail = hcl->c->r.s;
	HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,tail));

	/* the pair pointing to the list head is below the tail cell
	 * connected via cdr. */
	head = HCL_CONS_CDR(tail);
	HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,head));

	/* the pair pointing to the flag is below the head cell
	 * connected via cdr */
	flag = HCL_CONS_CDR(head);

	/* retrieve the numeric flag value */
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(HCL_CONS_CAR(flag)));
	flagv = (int)HCL_OOP_TO_SMOOI(HCL_CONS_CAR(flag));

	if (flagv & CLOSED)
	{
		/* the list has already been closed and cannot add more items */
		hcl_setsynerr (hcl, HCL_SYNERR_RPAREN, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		return HCL_NULL;
	}
	else if (flagv & DOTTED)
	{
		/* the list must not be empty to have reached the dotted state */
		HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,HCL_CONS_CAR(tail)));

		/* chain the object via 'cdr' of the tail cell */
		HCL_CONS_CDR(HCL_CONS_CAR(tail)) = obj;

		/* update the flag to CLOSED so that you can have more than
		 * one item after the dot. */
		flagv |= CLOSED;
		HCL_CONS_CAR(flag) = HCL_SMOOI_TO_OOP(flagv);
	}
	else
	{
		hcl_oop_t cons;

		hcl_pushtmp (hcl, &head);
		hcl_pushtmp (hcl, &tail);
		cons = hcl_makecons (hcl, obj, hcl->_nil);
		hcl_poptmps (hcl, 2);
		if (cons == HCL_NULL) return HCL_NULL;

		if (HCL_IS_NIL(hcl, HCL_CONS_CAR(head)))
		{
			/* the list head is not set yet. it is the first
			 * element added to the list. let both head and tail
			 * point to the new cons cell */
			HCL_ASSERT (hcl, HCL_IS_NIL (hcl, HCL_CONS_CAR(tail)));
			HCL_CONS_CAR(head) = cons; 
			HCL_CONS_CAR(tail) = cons;
		}
		else
		{
			/* the new cons cell is not the first element.
			 * append it to the list */
			HCL_CONS_CDR(HCL_CONS_CAR(tail)) = cons;
			HCL_CONS_CAR(tail) = cons;
		}
	}

	return obj;
}

static HCL_INLINE int is_list_empty (hcl_t* hcl)
{
	/* the stack must not be empty */
	HCL_ASSERT (hcl, !HCL_IS_NIL(hcl,hcl->c->r.s));

	/* if the tail pointer is pointing to nil, the list is empty */
	return HCL_IS_NIL(hcl,HCL_CONS_CAR(hcl->c->r.s));
}

static int add_to_byte_array_literal_buffer (hcl_t* hcl, hcl_oob_t b)
{
	if (hcl->c->r.balit.size >= hcl->c->r.balit.capa)
	{
		hcl_oob_t* tmp;
		hcl_oow_t new_capa;

		new_capa = HCL_ALIGN (hcl->c->r.balit.size + 1, BALIT_BUFFER_ALIGN);
		tmp = (hcl_oob_t*)hcl_reallocmem (hcl, hcl->c->r.balit.ptr, new_capa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->c->r.balit.capa = new_capa;
		hcl->c->r.balit.ptr = tmp;
	}

/* TODO: overflow check of hcl->c->r.balit.size itself */
	hcl->c->r.balit.ptr[hcl->c->r.balit.size++] = b;
	return 0;
}

static int get_byte_array_literal (hcl_t* hcl, hcl_oop_t* xlit)
{
	hcl_ooi_t tmp;
	hcl_oop_t ba;

	HCL_ASSERT (hcl, hcl->c->r.balit.size == 0);

	HCL_ASSERT (hcl, TOKEN_TYPE(hcl) == HCL_IOTOK_BAPAREN);
	GET_TOKEN(hcl); /* skip #[ */

	while (TOKEN_TYPE(hcl) == HCL_IOTOK_NUMLIT || TOKEN_TYPE(hcl) == HCL_IOTOK_RADNUMLIT)
	{
		/* TODO: check if the number is an integer */

		if (string_to_ooi(hcl, TOKEN_NAME(hcl), TOKEN_TYPE(hcl) == HCL_IOTOK_RADNUMLIT, &tmp) <= -1)
		{
			/* the token reader reads a valid token. no other errors
			 * than the range error must not occur */
			HCL_ASSERT (hcl, hcl->errnum == HCL_ERANGE);

			/* if the token is out of the SMOOI range, it's too big or 
			 * to small to be a byte */
			hcl_setsynerr (hcl, HCL_SYNERR_BYTERANGE, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			return -1;
		}
		else if (tmp < 0 || tmp > 255)
		{
			hcl_setsynerr (hcl, HCL_SYNERR_BYTERANGE, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
			return -1;
		}

		if (add_to_byte_array_literal_buffer(hcl, tmp) <= -1) return -1;
		GET_TOKEN (hcl);
	}

	if (TOKEN_TYPE(hcl) != HCL_IOTOK_RBRACK)
	{
		hcl_setsynerr (hcl, HCL_SYNERR_RBRACK, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		return -1;
	}

	ba = hcl_makebytearray (hcl, hcl->c->r.balit.ptr, hcl->c->r.balit.size);
	if (!ba) 
	{
		hcl->c->r.balit.size = 0; /* reset literal count... */
		return -1;
	}

	*xlit = ba;

	hcl->c->r.balit.size = 0; /* reset literal count... */
	return 0;
}


static int add_to_symbol_array_literal_buffer (hcl_t* hcl, hcl_oop_t b)
{
	if (hcl->c->r.salit.size >= hcl->c->r.salit.capa)
	{
		hcl_oop_t* tmp;
		hcl_oow_t new_capa;

		new_capa = HCL_ALIGN (hcl->c->r.salit.size + 1, SALIT_BUFFER_ALIGN);
		tmp = (hcl_oop_t*)hcl_reallocmem (hcl, hcl->c->r.salit.ptr, new_capa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->c->r.salit.capa = new_capa;
		hcl->c->r.salit.ptr = tmp;
	}

/* TODO: overflow check of hcl->c->r.tvlit_count itself */
	hcl->c->r.salit.ptr[hcl->c->r.salit.size++] = b;
	return 0;
}

static int get_symbol_array_literal (hcl_t* hcl, hcl_oop_t* xlit)
{
	hcl_oop_t sa, sym;
	hcl_oow_t i;

	HCL_ASSERT (hcl, hcl->c->r.salit.size == 0);

	HCL_ASSERT (hcl, TOKEN_TYPE(hcl) == HCL_IOTOK_VBAR);
	GET_TOKEN(hcl); /* skip #[ */

	while (TOKEN_TYPE(hcl) == HCL_IOTOK_IDENT /* || TOKEN_TYPE(hcl) == HCL_IOTOK_IDENT_DOTTED */)
	{
		sym = hcl_makesymbol(hcl, TOKEN_NAME_PTR(hcl), TOKEN_NAME_LEN(hcl));
		if (!sym) return -1;

		if (HCL_OBJ_GET_FLAGS_SYNCODE(sym) || HCL_OBJ_GET_FLAGS_KERNEL(sym))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL,
				"special symbol not to be declared as a variable - %O", sym); /* TOOD: error location */
			return -1;
		}

		if (add_to_symbol_array_literal_buffer(hcl, sym) <= -1) return -1;
		GET_TOKEN (hcl);
	}

	if (TOKEN_TYPE(hcl) != HCL_IOTOK_VBAR)
	{
		hcl_setsynerr (hcl, HCL_SYNERR_VBAR, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
		return -1;
	}

	sa = hcl_makearray (hcl, hcl->c->r.salit.size);
	if (!sa) 
	{
		hcl->c->r.salit.size = 0; /* reset literal count... */
		return -1;
	}

	for (i = 0; i < hcl->c->r.salit.size; i++)
		((hcl_oop_oop_t)sa)->slot[i] = hcl->c->r.salit.ptr[i];

	/* switch array to symbol array. this is special-purpose. */
	HCL_OBJ_SET_FLAGS_BRAND (sa, HCL_BRAND_SYMBOL_ARRAY);

	*xlit = sa;

	hcl->c->r.salit.size = 0; /* reset literal count... */
	return 0;
}

static int read_object (hcl_t* hcl)
{
	/* this function read an s-expression non-recursively
	 * by manipulating its own stack. */

	int level = 0, array_level = 0, flagv = 0; 
	hcl_oop_t obj;

	while (1)
	{
	redo:
		switch (TOKEN_TYPE(hcl)) 
		{
			default:
				hcl_seterrbfmt (hcl, HCL_EINTERN, "invalid token encountered - %d %.*js", 
					TOKEN_TYPE(hcl), TOKEN_NAME_LEN(hcl), TOKEN_NAME_PTR(hcl));
				return -1;

			case HCL_IOTOK_EOF:
				hcl_setsynerr (hcl, HCL_SYNERR_EOF, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
				return -1;

			case HCL_IOTOK_INCLUDE:
				/* TODO: should i limit where #include can be specified?
				 *       disallow it inside a list literal or an array literal? */
				GET_TOKEN (hcl);
				if (TOKEN_TYPE(hcl) != HCL_IOTOK_STRLIT)
				{
					hcl_setsynerr (hcl, HCL_SYNERR_STRING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					return -1;
				}
				if (begin_include(hcl) <= -1) return -1;
				goto redo;

#if 0
			case HCL_IOTOK_QUOTE:
				if (level >= HCL_TYPE_MAX(int))
				{
					/* the nesting level has become too deep */
					hcl_setsynerr (hcl, HCL_SYNERR_NESTING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					return -1;
				}

				/* enter a quoted string */
				flagv |= QUOTED;
				if (enter_list (hcl, flagv) == HCL_NULL) return -1;
				level++;

				/* force-chain the quote symbol to the new list entered */
				if (chain_to_list (hcl, hcl->_quote) == HCL_NULL) return -1;

				/* read the next token */
				GET_TOKEN (hcl);
				goto redo;
#endif

			case HCL_IOTOK_APAREN:
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_ARRAY);
				goto start_list;
			case HCL_IOTOK_BAPAREN:
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_BYTEARRAY);
				goto start_list;
			case HCL_IOTOK_DPAREN:
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_DIC);
				goto start_list;
#if 0
			case HCL_IOTOK_QPAREN:
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_QLIST);
				goto start_list;
#endif
			case HCL_IOTOK_LPAREN:
				flagv = 0;
				LIST_FLAG_SET_CONCODE (flagv, HCL_CONCODE_XLIST);
			start_list:
				if (level >= HCL_TYPE_MAX(int))
				{
					/* the nesting level has become too deep */
					hcl_setsynerr (hcl, HCL_SYNERR_NESTING, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					return -1;
				}

				/* push some data to simulate recursion into 
				 * a list literal or an array literal */
				if (enter_list(hcl, flagv) == HCL_NULL) return -1;
				level++;
				if (LIST_FLAG_GET_CONCODE(flagv) == HCL_CONCODE_ARRAY) array_level++;

				/* read the next token */
				GET_TOKEN (hcl);
				goto redo;

			case HCL_IOTOK_DOT:
				if (level <= 0 || is_list_empty(hcl) || !can_dot_list(hcl))
				{
					/* cannot have a period:
					 *   1. at the top level - not inside ()
					 *   2. at the beginning of a list 
					 *   3. inside an  #(), #[], #{}, () */
					hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, TOKEN_LOC(hcl), HCL_NULL); 
					return -1;
				}

				GET_TOKEN (hcl);
				goto redo;

			case HCL_IOTOK_RPAREN: /* xlist (), array #(), qlist '() */
			case HCL_IOTOK_RBRACK: /* byte array #[] */
			case HCL_IOTOK_RBRACE: /* dictionary #{} */
			{
				static struct 
				{
					int closer;
					int synerr;
				} req[] =
				{
					{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }, /* XLIST */
					{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }, /* ARRAY */
					{ HCL_IOTOK_RBRACK, HCL_SYNERR_RBRACK }, /* BYTEARRAY */
					{ HCL_IOTOK_RBRACE, HCL_SYNERR_RBRACE }, /* DIC */
					{ HCL_IOTOK_RPAREN, HCL_SYNERR_RPAREN }  /* QLIST */
				};

				int oldflagv;
				int concode;

				if (level <= 0)
				{
					hcl_setsynerr (hcl, HCL_SYNERR_UNBALPBB, TOKEN_LOC(hcl), HCL_NULL); 
					return -1;
				}

				concode = LIST_FLAG_GET_CONCODE(flagv);

				if (req[concode].closer != TOKEN_TYPE(hcl))
				{
					hcl_setsynerr (hcl, req[concode].synerr, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					return -1;
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
					return -1;
				}
#endif

				obj = leave_list (hcl, &flagv, &oldflagv);

				level--;
				if (LIST_FLAG_GET_CONCODE(oldflagv) == HCL_CONCODE_ARRAY) array_level--;
				break;
			}

#if 0
			case HCL_IOTOK_BAPAREN:
				if (get_byte_array_literal(hcl, &obj) <= -1) return -1;
				break;
#endif

			case HCL_IOTOK_VBAR:
/* TODO: think wheter to allow | | inside a quoted list... */

/* TODO: revise this part ... */
				if (array_level > 0)
				{
					hcl_setsynerr (hcl, HCL_SYNERR_VBARBANNED, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					return -1;
				}
				if (get_symbol_array_literal(hcl, &obj) <= -1) return -1;
				break;

			case HCL_IOTOK_NIL:
				obj = hcl->_nil;
				break;

			case HCL_IOTOK_TRUE:
				obj = hcl->_true;
				break;

			case HCL_IOTOK_FALSE:
				obj = hcl->_false;
				break;

			case HCL_IOTOK_SMPTRLIT:
			{
				hcl_oow_t i;
				hcl_oow_t v = 0;

				HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) >= 4);
				for (i = 3; i < TOKEN_NAME_LEN(hcl); i++)
				{
					HCL_ASSERT (hcl, is_xdigitchar(TOKEN_NAME_CHAR(hcl, i)));
					v = v * 16 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 16);
				}

				if (!HCL_IN_SMPTR_RANGE(v))
				{
					hcl_setsynerr (hcl, HCL_SYNERR_SMPTRLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
					return -1;
				}


				obj = HCL_SMPTR_TO_OOP(v);
				break;
			}

			case HCL_IOTOK_ERRORLIT:
			{
				hcl_oow_t i;
				hcl_ooi_t v = 0;

				HCL_ASSERT (hcl, TOKEN_NAME_LEN(hcl) >= 4);
				for (i = 3; i < TOKEN_NAME_LEN(hcl); i++)
				{
					HCL_ASSERT (hcl, is_digitchar(TOKEN_NAME_CHAR(hcl, i)));
					v = v * 10 + CHAR_TO_NUM(TOKEN_NAME_CHAR(hcl, i), 10);

					if (v > HCL_ERROR_MAX)
					{
						hcl_setsynerr (hcl, HCL_SYNERR_ERRORLIT, TOKEN_LOC(hcl), TOKEN_NAME(hcl));
						return -1;
					}
				}

				obj = HCL_ERROR_TO_OOP(v);
				break;
			}

			case HCL_IOTOK_CHARLIT:
				obj = HCL_CHAR_TO_OOP(TOKEN_NAME_CHAR(hcl, 0));
				break;

			case HCL_IOTOK_NUMLIT:
			case HCL_IOTOK_RADNUMLIT:
				obj = string_to_num(hcl, TOKEN_NAME(hcl), TOKEN_TYPE(hcl) == HCL_IOTOK_RADNUMLIT);
				break;

			/*
			case HCL_IOTOK_REAL:
				obj = hcl_makerealnum(hcl, HCL_IOTOK_RVAL(hcl));
				break;
			*/

			case HCL_IOTOK_STRLIT:
				obj = hcl_makestring(hcl, TOKEN_NAME_PTR(hcl), TOKEN_NAME_LEN(hcl), 0);
				break;

			case HCL_IOTOK_IDENT:
				obj = hcl_makesymbol(hcl, TOKEN_NAME_PTR(hcl), TOKEN_NAME_LEN(hcl));
				break;

			case HCL_IOTOK_IDENT_DOTTED:
				obj = hcl_makesymbol(hcl, TOKEN_NAME_PTR(hcl), TOKEN_NAME_LEN(hcl));
				if (obj)
				{
					hcl_pfbase_t* pfbase;
					hcl_oop_t prim;

					pfbase = hcl_querymod(hcl, TOKEN_NAME_PTR(hcl), TOKEN_NAME_LEN(hcl));
					if (!pfbase)
					{
						/* TODO switch to syntax error */
						return -1;
					}

					hcl_pushtmp (hcl, &obj);
					prim = hcl_makeprim(hcl, pfbase->handler, pfbase->minargs, pfbase->maxargs);

					if (!prim || !hcl_putatsysdic(hcl, obj, prim))
					{
						hcl_poptmp (hcl);
						return -1;
					}

					hcl_poptmp (hcl);

					HCL_OBJ_SET_FLAGS_KERNEL (obj, 1);
				}
				break;
		}

		if (!obj) return -1;

#if 0
		/* check if the element is read for a quoted list */
		while (flagv & QUOTED)
		{
			int oldflagv;

			HCL_ASSERT (hcl, level > 0);

			/* if so, append the element read into the quote list */
			if (chain_to_list (hcl, obj) == HCL_NULL) return -1;

			/* exit out of the quoted list. the quoted list can have 
			 * one element only. */
			obj = leave_list (hcl, &flagv, &oldflagv);

			/* one level up toward the top */
			level--;

			if (LIST_FLAG_GET_CONCODE(oldflagv) == HCL_CONCODE_ARRAY) array_level--;
		}
#endif

		/* check if we are at the top level */
		if (level <= 0) break; /* yes */

		/* if not, append the element read into the current list.
		 * if we are not at the top level, we must be in a list */
		if (chain_to_list(hcl, obj) == HCL_NULL) return -1;

		/* read the next token */
		GET_TOKEN (hcl);
	}

	/* upon exit, we must be at the top level */
	HCL_ASSERT (hcl, level == 0);
	HCL_ASSERT (hcl, array_level == 0);

	hcl->c->r.e = obj; 
	return 0;
}

static HCL_INLINE int __read (hcl_t* hcl)
{
	if (get_token(hcl) <= -1) return -1;
	if (TOKEN_TYPE(hcl) == HCL_IOTOK_EOF)
	{
		hcl_seterrnum (hcl, HCL_EFINIS);
		return -1;
	}
	return read_object (hcl);
}

hcl_oop_t hcl_read (hcl_t* hcl)
{
	HCL_ASSERT (hcl, hcl->c && hcl->c->reader); 
	if (__read(hcl) <= -1) return HCL_NULL;
	return hcl->c->r.e;
}

/* ========================================================================= */

/* TODO: rename compiler to something else that can include reader, printer, and compiler
 * move compiler intialization/finalization here to more common place */

static void gc_compiler (hcl_t* hcl)
{
	hcl_ooi_t i;

	hcl->c->r.s = hcl_moveoop (hcl, hcl->c->r.s);
	hcl->c->r.e = hcl_moveoop (hcl, hcl->c->r.e);


	for (i = 0; i <= hcl->c->cfs.top; i++)
	{
		hcl->c->cfs.ptr[i].operand = hcl_moveoop(hcl, hcl->c->cfs.ptr[i].operand);
	}

	for (i = 0; i < hcl->c->tv.size; i++)
	{
		hcl->c->tv.ptr[i] = hcl_moveoop (hcl, hcl->c->tv.ptr[i]);
	}

	for (i = 0; i < hcl->c->r.salit.size; i++)
	{
		hcl->c->r.salit.ptr[i] = hcl_moveoop (hcl, hcl->c->r.salit.ptr[i]);
	}
}

static void fini_compiler (hcl_t* hcl)
{
	/* called before the hcl object is closed */
	if (hcl->c)
	{
		if (hcl->c->r.balit.ptr)
		{
			hcl_freemem (hcl, hcl->c->r.balit.ptr);
			hcl->c->r.balit.ptr = HCL_NULL;
			hcl->c->r.balit.size = 0;
			hcl->c->r.balit.capa = 0;
		}

		if (hcl->c->r.salit.ptr)
		{
			hcl_freemem (hcl, hcl->c->r.salit.ptr);
			hcl->c->r.salit.ptr = HCL_NULL;
			hcl->c->r.salit.size = 0;
			hcl->c->r.salit.capa = 0;
		}

		if (hcl->c->cfs.ptr)
		{
			hcl_freemem (hcl, hcl->c->cfs.ptr);
			hcl->c->cfs.ptr = HCL_NULL;
			hcl->c->cfs.top = -1;
			hcl->c->cfs.capa = 0;
		}

		if (hcl->c->tv.ptr)
		{
			hcl_freemem (hcl, hcl->c->tv.ptr);
			hcl->c->tv.ptr = HCL_NULL;
			hcl->c->tv.size = 0;
			hcl->c->tv.capa = 0;
		}

		if (hcl->c->blk.tmprcnt)
		{
			hcl_freemem (hcl, hcl->c->blk.tmprcnt);
			hcl->c->blk.tmprcnt = HCL_NULL;
			hcl->c->blk.tmprcnt_capa = 0;
			hcl->c->blk.depth = -1;
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
		hcl_seterrnum (hcl, HCL_EINVAL);
		return -1;
	}

	if (!hcl->c)
	{
		hcl_cb_t cb;

		HCL_MEMSET (&cb, 0, HCL_SIZEOF(cb));
		cb.gc = gc_compiler;
		cb.fini = fini_compiler;
		cbp = hcl_regcb (hcl, &cb);
		if (!cbp) return -1;

		hcl->c = hcl_callocmem (hcl, HCL_SIZEOF(*hcl->c));
		if (!hcl->c) 
		{
			hcl_deregcb (hcl, cbp);
			return -1;
		}

		hcl->c->ilchr_ucs.ptr = &hcl->c->ilchr;
		hcl->c->ilchr_ucs.len = 1;

		hcl->c->r.s = hcl->_nil;
		hcl->c->r.e = hcl->_nil;

		hcl->c->cfs.top = -1;
		hcl->c->blk.depth = -1;
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
	n = hcl->c->reader (hcl, HCL_IO_OPEN, &hcl->c->inarg);
	if (n <= -1) goto oops;

	HCL_MEMSET (&hcl->c->outarg, 0, HCL_SIZEOF(hcl->c->outarg));
	n = hcl->c->printer (hcl, HCL_IO_OPEN, &hcl->c->outarg);
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
				HCL_MMGR_FREE (hcl->mmgr, hcl->c->curinp);
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
