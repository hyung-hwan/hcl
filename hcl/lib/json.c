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

#include "hcl-json.h"
#include "hcl-prv.h"

#include <string.h>
#include <errno.h>

#define HCL_JSON_TOKEN_NAME_ALIGN 64

struct dummy_hcl_xtn_t
{
	hcl_json_t* json;
};
typedef struct dummy_hcl_xtn_t dummy_hcl_xtn_t;

enum hcl_json_reply_attr_type_t
{
	HCL_JSON_REPLY_ATTR_TYPE_UNKNOWN,
	HCL_JSON_REPLY_ATTR_TYPE_DATA
};
typedef enum hcl_json_reply_attr_type_t hcl_json_reply_attr_type_t;

struct hcl_json_t
{
	hcl_mmgr_t* mmgr;
	hcl_cmgr_t* cmgr;
	hcl_json_prim_t prim;
	hcl_t* dummy_hcl;

	hcl_errnum_t errnum;
	struct
	{
		hcl_ooch_t buf[HCL_ERRMSG_CAPA];
		hcl_oow_t len;
	} errmsg;

	struct
	{
		unsigned int trait;
		unsigned int logmask;
	} cfg;

	struct
	{
		hcl_bch_t* ptr;
		hcl_oow_t len;
		hcl_oow_t capa;
	} req;

	hcl_json_state_t state;
	struct
	{
		struct
		{
			hcl_ooch_t* ptr;
			hcl_oow_t len;
			hcl_oow_t capa;
		} tok;

		hcl_json_reply_type_t type;
		hcl_json_reply_attr_type_t last_attr_type;
		struct
		{
			hcl_ooch_t* ptr;
			hcl_oow_t   len;
			hcl_oow_t   capa;
		} last_attr_key; /* the last attr key shown */

		union
		{
			struct
			{
				hcl_oow_t nsplen; /* length remembered when the white space was shown */
			} reply_value_unquoted;

			struct
			{
				int escaped;
			} reply_value_quoted;

			struct
			{
				hcl_oow_t nsplen; /* length remembered when the white space was shown */
			} attr_value_unquoted;

			struct
			{
				int escaped;
			} attr_value_quoted;

			struct
			{
				hcl_oow_t max;
				hcl_oow_t tally;
			} length_bounded_data;

			struct
			{
				int in_data_part;
				int negated;
				hcl_oow_t max; /* chunk length */
				hcl_oow_t tally; 
				hcl_oow_t total;
				hcl_oow_t clcount; 
			} chunked_data;
		} u;
	} rep;
};


/* ========================================================================= */

static void log_write_for_dummy (hcl_t* hcl, unsigned int mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	dummy_hcl_xtn_t* xtn = (dummy_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_json_t* json;

	json = xtn->json;
	json->prim.log_write (json, mask, msg, len);
}

static void syserrstrb (hcl_t* hcl, int syserr, hcl_bch_t* buf, hcl_oow_t len)
{
#if defined(HAVE_STRERROR_R)
	strerror_r (syserr, buf, len);
#else
	/* this may not be thread safe */
	hcl_copy_bcstr (buf, len, strerror(syserr));
#endif
}

/* ========================================================================= */

static HCL_INLINE int is_spacechar (hcl_bch_t c)
{
	/* TODO: handle other space unicode characters */
	switch (c)
	{
		case ' ':
		case '\f': /* formfeed */
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

static void clear_reply_token (hcl_json_t* json)
{
	json->rep.tok.len = 0;
}

static int add_to_reply_token (hcl_json_t* json, hcl_ooch_t ch)
{
	if (json->rep.tok.len >= json->rep.tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN_POW2(json->rep.tok.len + 1, HCL_JSON_TOKEN_NAME_ALIGN);
		tmp = hcl_json_reallocmem(json, json->rep.tok.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		json->rep.tok.capa = newcapa;
		json->rep.tok.ptr = tmp;
	}

	json->rep.tok.ptr[json->rep.tok.len++] = ch;
	return 0;
}

static HCL_INLINE int is_token (hcl_json_t* json, const hcl_bch_t* str)
{
	return hcl_comp_oochars_bcstr(json->rep.tok.ptr, json->rep.tok.len, str) == 0;
} 

static HCL_INLINE int is_token_integer (hcl_json_t* json, hcl_oow_t* value)
{
	hcl_oow_t i;
	hcl_oow_t v = 0;

	if (json->rep.tok.len <= 0) return 0;

	for (i = 0; i < json->rep.tok.len; i++)
	{
		if (!is_digitchar(json->rep.tok.ptr[i])) return 0;
		v = v * 10 + (json->rep.tok.ptr[i] - '0');
	}

	*value = v;
	return 1;
} 

static HCL_INLINE hcl_ooch_t unescape (hcl_ooch_t c)
{
#if 0
	/* as of this writing, the server side only escapes \ and ".
	 * i don't know if n, r, f, t, v should be supported here */
	switch (c)
	{
		case 'n': return '\n';
		case 'r': return '\r';
		case 'f': return '\f';
		case 't': return '\t';
		case 'v': return '\v';
		default: return c;
	}
#else
	return c;
#endif
}

static int handle_char (hcl_json_t* json, hcl_ooci_t c, hcl_oow_t nbytes)
{
	switch (json->state)
	{
		case HCL_JSON_STATE_START:
			if (c == HCL_OOCI_EOF)
			{
				hcl_json_seterrbfmt (json, HCL_EFINIS, "unexpected end before reply name");
				goto oops;
			}
			else if (c == '[')
			{
				json->state = HCL_JSON_STATE_ARRAY_STARTED;
				break;
			}
			else if (c == '{')
			{
				json->state = HCL_JSON_STATE_OBJECT_STARTED;
				break;
			}
			else if (is_spacechar(c)) 
			{
				/* skip whitespaces at the beginning of the start line before the reply name */
				break;
			}
			else
			{
				hcl_json_seterrbfmt (json, HCL_EINVAL, "not starting with [ or { - %jc", (hcl_ooch_t)c);
				goto oops;
			}
			break;

		case HCL_JSON_STATE_ARRAY_STARTED:
			if (c == HCL_OOCI_EOF)
			{
				break;
			}
			else if (c == ']')
			{
				break;
			}
			else if (is_spacechar(c))
			{
				break;
			}
			else if (c == '\"')
			{
				json->state = HCL_JSON_STATE_IN_QUOTED_VALUE;
				break;
			}
			else if (is_alphachar(c) || is_digitchar(c))
			{
				break;
			}
			else
			{
				hcl_json_seterrbfmt (json, HCL_EINVAL, "not starting with [ or { - %jc", (hcl_ooch_t)c);
				goto oops;
			}
			break;

		case HCL_JSON_STATE_OBJECT_STARTED:
			if (c == HCL_OOCI_EOF)
			{
				break;
			}
			else if (c == '}')
			{
				break;
			}
			else if (is_spacechar(c))
			{
				break;
			}
			else if (c == '\"')
			{
				break;
			}
			else if (is_alphachar(c) || is_digitchar(c))
			{
				break;
			}
			else
			{
				hcl_json_seterrbfmt (json, HCL_EINVAL, "not starting with [ or { - %jc", (hcl_ooch_t)c);
				goto oops;
			}
			break;

		case HCL_JSON_STATE_IN_WORD_VALUE:
			if (c == HCL_OOCI_EOF)
			{
				break;
			}
			else
			{
				break;
			}
			break;

		case HCL_JSON_STATE_IN_QUOTED_VALUE:
			
			break;

		case HCL_JSON_STATE_IN_NUMERIC_VALUE:
			break;

		default:
			hcl_json_seterrbfmt (json, HCL_EINTERN, "internal error - must not be called for state %d", (int)json->state);
			goto oops;
	}

	return 0;
	
oops: 
	return -1;
}

static int feed_json_data (hcl_json_t* json, const hcl_bch_t* data, hcl_oow_t len, hcl_oow_t* xlen)
{
	const hcl_bch_t* ptr;
	const hcl_bch_t* end;

	ptr = data;
	end = ptr + len;

	while (ptr < end)
	{
		hcl_ooci_t c;
		hcl_oow_t bcslen;

	#if defined(HCL_OOCH_IS_UCH)
		hcl_oow_t ucslen;
		hcl_ooch_t uc;
		int n;

		bcslen = end - ptr;
		ucslen = 1;

		n = hcl_conv_bchars_to_uchars_with_cmgr(ptr, &bcslen, &uc, &ucslen, json->cmgr, 0);
		if (n <= -1)
		{
			if (n == -3)
			{
				/* incomplete sequence */
				*xlen = ptr - data;
				return 0; /* feed more for incomplete sequence */
			}
		}

		ptr += bcslen;
		c = uc;
	#else
		bcslen = 1;
		c = *ptr++;
	#endif

		if (handle_char(json, c, bcslen) <= -1) goto oops;
	}

	*xlen = ptr - data;
	return 1;

oops:
	/* TODO: compute the number of processed bytes so far and return it via a parameter??? */
/*printf ("feed oops....\n");*/
	return -1;
}


/* ========================================================================= */

hcl_json_t* hcl_json_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_json_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_json_t* json;
	hcl_t* hcl;
	hcl_vmprim_t vmprim;
	dummy_hcl_xtn_t* xtn;

	json = (hcl_json_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*json) + xtnsize);
	if (!json) 
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		return HCL_NULL;
	}

	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.log_write = log_write_for_dummy;
	vmprim.syserrstrb = syserrstrb;

	hcl = hcl_open(mmgr, HCL_SIZEOF(*xtn), 2048, &vmprim, errnum);
	if (!hcl) 
	{
		HCL_MMGR_FREE (mmgr, json);
		return HCL_NULL;
	}

	xtn = (dummy_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->json = json;

	HCL_MEMSET (json, 0, HCL_SIZEOF(*json) + xtnsize);
	json->mmgr = mmgr;
	json->cmgr = hcl_get_utf8_cmgr();
	json->prim = *prim;
	json->dummy_hcl = hcl;

	json->cfg.logmask = ~0u;

	/* the dummy hcl is used for this json to perform primitive operations
	 * such as getting system time or logging. so the heap size doesn't 
	 * need to be changed from the tiny value set above. */
	hcl_setoption (json->dummy_hcl, HCL_LOG_MASK, &json->cfg.logmask);
	hcl_setcmgr (json->dummy_hcl, json->cmgr);

	return json;
}

void hcl_json_close (hcl_json_t* json)
{
	if (json->rep.tok.ptr) hcl_json_freemem (json, json->rep.tok.ptr);
	if (json->rep.last_attr_key.ptr) hcl_json_freemem (json, json->rep.last_attr_key.ptr);
	hcl_close (json->dummy_hcl);
	HCL_MMGR_FREE (json->mmgr, json);
}

int hcl_json_setoption (hcl_json_t* json, hcl_json_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_JSON_TRAIT:
			json->cfg.trait = *(const unsigned int*)value;
			return 0;

		case HCL_JSON_LOG_MASK:
			json->cfg.logmask = *(const unsigned int*)value;
			if (json->dummy_hcl) 
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get 
				 * affected. new hcl instances to be created later 
				 * is supposed to use the new value */
				hcl_setoption (json->dummy_hcl, HCL_LOG_MASK, value);
			}
			return 0;
	}

	hcl_json_seterrnum (json, HCL_EINVAL);
	return -1;
}

int hcl_json_getoption (hcl_json_t* json, hcl_json_option_t id, void* value)
{
	switch (id)
	{
		case HCL_JSON_TRAIT:
			*(unsigned int*)value = json->cfg.trait;
			return 0;

		case HCL_JSON_LOG_MASK:
			*(unsigned int*)value = json->cfg.logmask;
			return 0;
	};

	hcl_json_seterrnum (json, HCL_EINVAL);
	return -1;
}


void* hcl_json_getxtn (hcl_json_t* json)
{
	return (void*)(json + 1);
}

hcl_mmgr_t* hcl_json_getmmgr (hcl_json_t* json)
{
	return json->mmgr;
}

hcl_cmgr_t* hcl_json_getcmgr (hcl_json_t* json)
{
	return json->cmgr;
}

void hcl_json_setcmgr (hcl_json_t* json, hcl_cmgr_t* cmgr)
{
	json->cmgr = cmgr;
}

hcl_errnum_t hcl_json_geterrnum (hcl_json_t* json)
{
	return json->errnum;
}

const hcl_ooch_t* hcl_json_geterrstr (hcl_json_t* json)
{
	return hcl_errnum_to_errstr(json->errnum);
}

const hcl_ooch_t* hcl_json_geterrmsg (hcl_json_t* json)
{
	if (json->errmsg.len <= 0) return hcl_errnum_to_errstr(json->errnum);
	return json->errmsg.buf;
}

void hcl_json_seterrnum (hcl_json_t* json, hcl_errnum_t errnum)
{
	/*if (json->shuterr) return; */
	json->errnum = errnum;
	json->errmsg.len = 0;
}

void hcl_json_seterrbfmt (hcl_json_t* json, hcl_errnum_t errnum, const hcl_bch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrbfmtv (json->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (json->dummy_hcl, HCL_COUNTOF(json->errmsg.buf) == HCL_COUNTOF(json->dummy_hcl->errmsg.buf));
	json->errnum = errnum;
	hcl_copy_oochars (json->errmsg.buf, json->dummy_hcl->errmsg.buf, HCL_COUNTOF(json->errmsg.buf));
	json->errmsg.len = json->dummy_hcl->errmsg.len;
}

void hcl_json_seterrufmt (hcl_json_t* json, hcl_errnum_t errnum, const hcl_uch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrufmtv (json->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (json->dummy_hcl, HCL_COUNTOF(json->errmsg.buf) == HCL_COUNTOF(json->dummy_hcl->errmsg.buf));
	json->errnum = errnum;
	hcl_copy_oochars (json->errmsg.buf, json->dummy_hcl->errmsg.buf, HCL_COUNTOF(json->errmsg.buf));
	json->errmsg.len = json->dummy_hcl->errmsg.len;
}

/* ========================================================================= */

void hcl_json_logbfmt (hcl_json_t* json, unsigned int mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logbfmtv (json->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

void hcl_json_logufmt (hcl_json_t* json, unsigned int mask, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logufmtv (json->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

/* ========================================================================= */

void* hcl_json_allocmem (hcl_json_t* json, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(json->mmgr, size);
	if (!ptr) hcl_json_seterrnum (json, HCL_ESYSMEM);
	return ptr;
}

void* hcl_json_callocmem (hcl_json_t* json, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(json->mmgr, size);
	if (!ptr) hcl_json_seterrnum (json, HCL_ESYSMEM);
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_json_reallocmem (hcl_json_t* json, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC(json->mmgr, ptr, size);
	if (!ptr) hcl_json_seterrnum (json, HCL_ESYSMEM);
	return ptr;
}

void hcl_json_freemem (hcl_json_t* json, void* ptr)
{
	HCL_MMGR_FREE (json->mmgr, ptr);
}

/* ========================================================================= */

hcl_json_state_t hcl_json_getstate (hcl_json_t* json)
{
	return json->state;
}

void hcl_json_reset (hcl_json_t* json)
{
	/* TODO: reset XXXXXXXXXXXXXXXXXXXXXXXXXXXxxxxx */
	json->state = HCL_JSON_STATE_START;
}

int hcl_json_feed (hcl_json_t* json, const void* ptr, hcl_oow_t len, hcl_oow_t* xlen)
{
	int x;
	hcl_oow_t total, ylen;
	const hcl_bch_t* buf;

	buf = (const hcl_bch_t*)ptr;
	total = 0;
	while (total < len)
	{
		x = feed_json_data(json, &buf[total], len - total, &ylen);
		if (x <= -1) return -1;

		total += ylen;
		if (x == 0) break; /* incomplete sequence encountered */
	}

	*xlen = total;
	return 0;
}
