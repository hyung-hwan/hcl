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

#include "hcl-c.h"
#include "hcl-prv.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define HCL_CLIENT_TOKEN_NAME_ALIGN 64
#define HCL_CLIENT_WID_MAP_ALIGN 512
#define HCL_CLIENT_PROTO_REPLY_BUF_SIZE 1300

#define HCL_CLIENT_REPLY_MAX_HDRKEY_LEN 128

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	if defined(HCL_HAVE_CFG_H) && defined(HCL_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	endif
#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__MSDOS__)
#	include <dos.h>
#	include <time.h>
#elif defined(macintosh)
#	include <Timer.h>
#else
#	if defined(HAVE_TIME_H)
#		include <time.h>
#	endif
#	if defined(HAVE_SYS_TIME_H)
#		include <sys/time.h>
#	endif
#	if defined(HAVE_SIGNAL_H)
#		include <signal.h>
#	endif
#	if defined(HAVE_SYS_MMAN_H)
#		include <sys/mman.h>
#	endif
#	if defined(HAVE_SYS_UIO_H)
#		include <sys/uio.h>
#	endif

#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <pthread.h>
#	include <poll.h>
#endif

struct dummy_hcl_xtn_t
{
	hcl_client_t* client;
};
typedef struct dummy_hcl_xtn_t dummy_hcl_xtn_t;

enum hcl_client_reply_attr_type_t
{
	HCL_CLIENT_REPLY_ATTR_TYPE_UNKNOWN,
	HCL_CLIENT_REPLY_ATTR_TYPE_DATA
};
typedef enum hcl_client_reply_attr_type_t hcl_client_reply_attr_type_t;

enum hcl_client_state_t
{
	HCL_CLIENT_STATE_START,
	HCL_CLIENT_STATE_IN_REPLY_NAME,
	HCL_CLIENT_STATE_IN_REPLY_VALUE_START,
	HCL_CLIENT_STATE_IN_REPLY_VALUE_UNQUOTED,
	HCL_CLIENT_STATE_IN_REPLY_VALUE_QUOTED,
	HCL_CLIENT_STATE_IN_REPLY_VALUE_QUOTED_TRAILER,
	HCL_CLIENT_STATE_IN_ATTR_KEY,
	HCL_CLIENT_STATE_IN_ATTR_VALUE_START,
	HCL_CLIENT_STATE_IN_ATTR_VALUE_UNQUOTED,
	HCL_CLIENT_STATE_IN_ATTR_VALUE_QUOTED,
	HCL_CLIENT_STATE_IN_ATTR_VALUE_QUOTED_TRAILER,

	HCL_CLIENT_STATE_IN_LENGTH_BOUNDED_DATA,
	HCL_CLIENT_STATE_IN_CHUNKED_DATA
};
typedef enum hcl_client_state_t hcl_client_state_t;

struct hcl_client_t
{
	hcl_mmgr_t* mmgr;
	hcl_cmgr_t* cmgr;
	hcl_client_prim_t prim;
	hcl_t* dummy_hcl;

	hcl_errnum_t errnum;
	struct
	{
		hcl_ooch_t buf[HCL_ERRMSG_CAPA];
		hcl_oow_t len;
	} errmsg;
	int stopreq;

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

	hcl_client_state_t state;
	struct
	{
		struct
		{
			hcl_ooch_t* ptr;
			hcl_oow_t len;
			hcl_oow_t capa;
		} tok;

		hcl_client_reply_type_t type;
		hcl_client_reply_attr_type_t last_attr_type;
		struct
		{
			hcl_ooch_t buf[HCL_CLIENT_REPLY_MAX_HDRKEY_LEN];
			hcl_oow_t  len;
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
	hcl_client_t* client;

	client = xtn->client;
	client->prim.log_write (client, mask, msg, len);
}

static void syserrstrb (hcl_t* hcl, int syserr, hcl_bch_t* buf, hcl_oow_t len)
{
#if defined(HAVE_STRERROR_R)
	strerror_r (syserr, buf, len);
#else
	/* this may not be thread safe */
	hcl_copybcstr (buf, len, strerror(syserr));
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

static void clear_reply_token (hcl_client_t* client)
{
	client->rep.tok.len = 0;
}

static int add_to_reply_token (hcl_client_t* client, hcl_ooch_t ch)
{
	if (client->rep.tok.len >= client->rep.tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN_POW2(client->rep.tok.len + 1, 128);
		tmp = hcl_client_reallocmem(client, client->rep.tok.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		client->rep.tok.capa = newcapa;
		client->rep.tok.ptr = tmp;
	}

	client->rep.tok.ptr[client->rep.tok.len++] = ch;
	return 0;
}

static HCL_INLINE int is_token (hcl_client_t* client, const hcl_bch_t* str)
{
	return hcl_compoocharsbcstr(client->rep.tok.ptr, client->rep.tok.len, str) == 0;
} 

static HCL_INLINE int is_token_integer (hcl_client_t* client, hcl_oow_t* value)
{
	hcl_oow_t i;
	hcl_oow_t v = 0;

	if (client->rep.tok.len <= 0) return 0;

	for (i = 0; i < client->rep.tok.len; i++)
	{
		if (!is_digitchar(client->rep.tok.ptr[i])) return 0;
		v = v * 10 + (client->rep.tok.ptr[i] - '0');
	}

	*value = v;
	return 1;
} 

static int handle_char (hcl_client_t* client, hcl_ooci_t c, hcl_oow_t nbytes)
{
	switch (client->state)
	{
		case HCL_CLIENT_STATE_START:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "unexpected end before reply name");
				goto oops;
			}
			else if (c == '.')
			{
				client->state = HCL_CLIENT_STATE_IN_REPLY_NAME;
				clear_reply_token (client);
				if (add_to_reply_token(client, c) <= -1) goto oops;
				break;
			}
			else if (is_spacechar(c)) 
			{
				/* skip whitespaces at the beginning of the start line before the reply name */
				break;
			}
			else
			{
				hcl_client_seterrbfmt (client, HCL_EINVAL, "reply line not starting with a period");
				goto oops;
			}

		case HCL_CLIENT_STATE_IN_REPLY_NAME:
			if (is_alphachar(c) || (client->rep.tok.len > 2 && c == '-'))
			{
				if (add_to_reply_token(client, c) <= -1) goto oops;
				break;
			}
			else
			{
				if (is_token(client, ".OK"))
				{
					client->rep.type = HCL_CLIENT_REPLY_TYPE_OK;
				}
				else if (is_token(client, ".ERROR"))
				{
					client->rep.type = HCL_CLIENT_REPLY_TYPE_ERROR;
				}
				else
				{
					hcl_client_seterrbfmt (client, HCL_EINVAL, "unknown reply name %.*js", client->rep.tok.len, client->rep.tok.ptr);
					goto oops;
				}

				client->state = HCL_CLIENT_STATE_IN_REPLY_VALUE_START;
				clear_reply_token (client);
				/* [IMPORTANT] fall thru */
			}

		case HCL_CLIENT_STATE_IN_REPLY_VALUE_START:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of reply line without newline");
				goto oops;
			}
			else if (is_spacechar(c))
			{
				/* do nothing. skip it */
				break;
			}
			else if (c == '\n')
			{
				/* no value is specified. even no whitespaces.
				 * switch to a long-format response */
				client->prim.start_reply (client, client->rep.type, HCL_NULL, 0);

				client->state = HCL_CLIENT_STATE_IN_ATTR_KEY;
				HCL_ASSERT (client->dummy_hcl, client->rep.tok.len == 0);
				break;
			}
			else if (c == '\"')
			{
				client->state = HCL_CLIENT_STATE_IN_REPLY_VALUE_QUOTED;
				HCL_ASSERT (client->dummy_hcl, client->rep.tok.len == 0);
				client->rep.u.reply_value_quoted.escaped = 0;
				break;
			}
			else 
			{
				/* the first value character has been encountered */
				client->state = HCL_CLIENT_STATE_IN_REPLY_VALUE_UNQUOTED;
				HCL_ASSERT (client->dummy_hcl, client->rep.tok.len == 0);
				client->rep.u.reply_value_unquoted.nsplen = 0;
				/* [IMPORTANT] fall thru */
			}

		case HCL_CLIENT_STATE_IN_REPLY_VALUE_UNQUOTED:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of reply line without newline");
				goto oops;
			}
			else if (c == '\n')
			{
				client->rep.tok.len = client->rep.u.reply_value_unquoted.nsplen;
				goto reply_value_end;
			}
			else
			{
				if (add_to_reply_token(client, c) <= -1) goto oops;
				if (!is_spacechar(c)) client->rep.u.reply_value_unquoted.nsplen = client->rep.tok.len;
				break;
			}

		case HCL_CLIENT_STATE_IN_REPLY_VALUE_QUOTED:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of reply line without closing quote");
				goto oops;
			}
			else 
			{
				if (client->rep.u.reply_value_quoted.escaped)
				{
					if (c == '\\') c = '\\';
					else if (c == '\"') c = '\"';
					else if (c == 'n') c = '\n';
					else if (c == 'r') c = '\r';
					/* TODO: more escaping handling */
				}
				else if (c == '\\')
				{
					client->rep.u.reply_value_quoted.escaped = 1;
					break;
				}
				else if (c == '\"')
				{
					client->state = HCL_CLIENT_STATE_IN_REPLY_VALUE_QUOTED_TRAILER;
					break;
				}

				client->rep.u.reply_value_quoted.escaped = 0;
				if (add_to_reply_token(client, c) <= -1) goto oops;
				break;
			}

		case HCL_CLIENT_STATE_IN_REPLY_VALUE_QUOTED_TRAILER:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of reply line without newline");
				goto oops;
			}
			else if (c == '\n')
			{
			reply_value_end:
				/* short-form format. the data pointer is passed to the start_reply 
				 * callback. no end_reply callback is invoked. the data is assumed
				 * to be in UTF-8 encoding. this is different from the data in the
				 * long-format reply which is treated as octet stream */
				client->prim.start_reply (client, client->rep.type, client->rep.tok.ptr, client->rep.tok.len);
				/* no end_reply() callback for the short-form reply */

				client->state = HCL_CLIENT_STATE_START;
				clear_reply_token (client);
				break;
			}
			else if (is_spacechar(c))
			{
				/* skip white spaces after the closing quote */
				break;
			}
			else
			{
				hcl_client_seterrbfmt (client, HCL_EINVAL, "garbage after quoted reply value");
				goto oops;
			}

		case HCL_CLIENT_STATE_IN_ATTR_KEY:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of attribute line");
				goto oops;
			}
			else if (client->rep.tok.len == 0)
			{
				if (is_spacechar(c))
				{
					/* skip whitespaces at the beginning of the start line before the reply name */
					break;
				}
				else if (c != '.')
				{
					hcl_client_seterrbfmt (client, HCL_EINVAL, "attribute name not starting with a period");
					goto oops;
				}

				if (add_to_reply_token(client, c) <= -1) goto oops;
				break;
			}
			else	if (is_alphachar(c) || (client->rep.tok.len > 2 && c == '-'))
			{
				if (client->rep.tok.len >= HCL_CLIENT_REPLY_MAX_HDRKEY_LEN)
				{
					hcl_client_seterrbfmt (client, HCL_EINVAL, "attribute name too long");
					goto oops;
				}
				if (add_to_reply_token(client, c) <= -1) goto oops;
				break;
			}
			else
			{
				if (is_token(client, ".DATA"))
				{
					client->rep.last_attr_type = HCL_CLIENT_REPLY_ATTR_TYPE_DATA;
				}
				/* PUT more known attribute handling here */
				else
				{
					client->rep.last_attr_type = HCL_CLIENT_REPLY_ATTR_TYPE_UNKNOWN;
				}

				HCL_ASSERT (client->dummy_hcl, client->rep.tok.len <= HCL_COUNTOF(client->rep.last_attr_key.buf));
				hcl_copyoochars (client->rep.last_attr_key.buf, client->rep.tok.ptr, client->rep.tok.len);
				client->rep.last_attr_key.len = client->rep.tok.len;

				client->state = HCL_CLIENT_STATE_IN_ATTR_VALUE_START;
				clear_reply_token (client);
				/* [IMPORTANT] fall thru */
			}

		case HCL_CLIENT_STATE_IN_ATTR_VALUE_START:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end without attribute value");
				goto oops;
			}
			else if (is_spacechar(c))
			{
				/* do nothing. skip it */
				break;
			}
			else if (c == '\n')
			{
				hcl_client_seterrbfmt (client, HCL_EINVAL, "no attribute value for %.*js\n", client->rep.last_attr_key.len, client->rep.last_attr_key.buf);
				goto oops;
			}
			else if (c == '\"')
			{
				client->state = HCL_CLIENT_STATE_IN_ATTR_VALUE_QUOTED;
				HCL_ASSERT (client->dummy_hcl, client->rep.tok.len == 0);
				client->rep.u.attr_value_quoted.escaped = 0;
				break;
			}
			else 
			{
				/* the first value character has been encountered */
				client->state = HCL_CLIENT_STATE_IN_ATTR_VALUE_UNQUOTED;
				HCL_ASSERT (client->dummy_hcl, client->rep.tok.len == 0);
				client->rep.u.attr_value_unquoted.nsplen = 0;
				/* [IMPORTANT] fall thru */
			}

		case HCL_CLIENT_STATE_IN_ATTR_VALUE_UNQUOTED:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of attribute line without newline");
				goto oops;
			}
			else if (c == '\n')
			{
				client->rep.tok.len = client->rep.u.attr_value_unquoted.nsplen;
				goto attr_value_end;
			}
			else
			{
				if (add_to_reply_token(client, c) <= -1) goto oops;
				if (!is_spacechar(c)) client->rep.u.attr_value_unquoted.nsplen = client->rep.tok.len;
				break;
			}

		case HCL_CLIENT_STATE_IN_ATTR_VALUE_QUOTED:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of attribute value without closing quote");
				goto oops;
			}
			else 
			{
				if (client->rep.u.attr_value_quoted.escaped)
				{
					if (c == '\\') c = '\\';
					else if (c == '\"') c = '\"';
					else if (c == 'n') c = '\n';
					else if (c == 'r') c = '\r';
					/* TODO: more escaping handling */
				}
				else if (c == '\\')
				{
					client->rep.u.attr_value_quoted.escaped = 1;
					break;
				}
				else if (c == '\"')
				{
					client->state = HCL_CLIENT_STATE_IN_ATTR_VALUE_QUOTED_TRAILER;
					break;
				}

				client->rep.u.attr_value_quoted.escaped = 0;
				if (add_to_reply_token(client, c) <= -1) goto oops;
				break;
			}

		case HCL_CLIENT_STATE_IN_ATTR_VALUE_QUOTED_TRAILER:
			if (c == HCL_OOCI_EOF)
			{
				hcl_client_seterrbfmt (client, HCL_EFINIS, "sudden end of attribute line without newline");
				goto oops;
			}
			else if (c == '\n')
			{
			attr_value_end:
				if (client->prim.feed_attr)
				{
					hcl_oocs_t key, val;
					key.ptr = client->rep.last_attr_key.buf;
					key.len = client->rep.last_attr_key.len;
					val.ptr = client->rep.tok.ptr;
					val.len = client->rep.tok.len;
					client->prim.feed_attr (client, &key, &val);
				}

				if (client->rep.last_attr_type == HCL_CLIENT_REPLY_ATTR_TYPE_DATA)
				{
					hcl_oow_t length;

					/* this must be the last attr. the trailing part are all data */
					if (is_token(client, "chunked"))
					{
						client->state = HCL_CLIENT_STATE_IN_CHUNKED_DATA;
						HCL_MEMSET (&client->rep.u.chunked_data, 0, HCL_SIZEOF(client->rep.u.chunked_data));
					}
					else if (is_token_integer(client, &length))
					{
						if (length > 0)
						{
							client->state = HCL_CLIENT_STATE_IN_LENGTH_BOUNDED_DATA;
							/* [NOTE] the max length for the length-bounded transfer scheme is limited
							 *        by the system word size as of this implementation */
							client->rep.u.length_bounded_data.max = length; 
							client->rep.u.length_bounded_data.tally = 0;
						}
						else
						{
							/* .DATA 0 has been received. this should be end of the reply */
							client->state = HCL_CLIENT_STATE_START;
						}
					}
					else
					{
						hcl_client_seterrbfmt (client, HCL_EINVAL, "invalid attribute value for .DATA - %.*js", client->rep.tok.len, client->rep.tok.ptr);
						goto oops;
					}
				}
				else
				{
					/* continue processing the next attr */
					client->state = HCL_CLIENT_STATE_IN_ATTR_KEY;
				}

				clear_reply_token (client);
				break;
			}
			else if (is_spacechar(c))
			{
				/* skip white spaces after the closing quote */
				break;
			}
			else
			{
				hcl_client_seterrbfmt (client, HCL_EINVAL, "garbage after quoted attribute value for %.*js", client->rep.last_attr_key.len, client->rep.last_attr_key.buf);
				goto oops;
			}

		default:
			/* this function must not be called for .DATA */
			hcl_client_seterrbfmt (client, HCL_EINTERN, "internal error - must not be called for state %d", client->state);
			goto oops;
	}

	return 0;
	
oops: 
	return -1;
}

static int feed_reply_data (hcl_client_t* client, const hcl_bch_t* data, hcl_oow_t len, hcl_oow_t* xlen)
{
	const hcl_bch_t* ptr;
	const hcl_bch_t* end;

	ptr = data;
	end = ptr + len;

	while (ptr < end)
	{
		if (client->state == HCL_CLIENT_STATE_IN_LENGTH_BOUNDED_DATA)
		{
			/* the data is treated as raw octets by this client */
			hcl_oow_t capa, avail, taken;

			capa = client->rep.u.length_bounded_data.max - client->rep.u.length_bounded_data.tally;
			avail = end - ptr;
			taken = (avail < capa)? avail: capa;

			if (client->prim.feed_data)
			{
				client->prim.feed_data (client, ptr, taken);
			}

			ptr += taken;
			client->rep.u.length_bounded_data.tally = taken;
			if (taken == capa)
			{
				/* read all data. no more */
				client->state = HCL_CLIENT_STATE_START;
				client->prim.end_reply (client, HCL_CLIENT_END_REPLY_STATE_OK);
			}
		}
		else if (client->state == HCL_CLIENT_STATE_IN_CHUNKED_DATA)
		{
			/* the data is treated as raw octets by this client */
			if (client->rep.u.chunked_data.in_data_part)
			{
				hcl_oow_t capa, avail, taken;

				capa = client->rep.u.chunked_data.max - client->rep.u.chunked_data.tally;
				avail = end - ptr;
				taken = (avail < capa)? avail: capa;

				if (client->prim.feed_data)
				{
					client->prim.feed_data (client, ptr, taken);
				}

				ptr += taken;
				client->rep.u.chunked_data.tally += taken;
				client->rep.u.chunked_data.total += taken;
				if (taken == capa)
				{
					/* finished one chunk */
					client->rep.u.chunked_data.negated = 0;
					client->rep.u.chunked_data.max = 0;
					client->rep.u.chunked_data.tally = 0;
					client->rep.u.chunked_data.clcount = 0;
					client->rep.u.chunked_data.in_data_part = 0;
				}
			}
			else
			{
				while (ptr < end)
				{
					hcl_bchu_t bc = (hcl_bchu_t)*ptr;
					if (bc == '-' && client->rep.u.chunked_data.clcount == 0 && !client->rep.u.chunked_data.negated)
					{
						client->rep.u.chunked_data.negated = 1;
					}
					else if (bc == ':') 
					{
						ptr++;

						if (client->rep.u.chunked_data.clcount == 0)
						{
							hcl_client_seterrbfmt (client, HCL_EINVAL, "clone without valid chunk length");
							goto oops;
						}

						if (client->rep.u.chunked_data.negated)
						{
							client->prim.end_reply (client, HCL_CLIENT_END_REPLY_STATE_REVOKED);
							client->state = HCL_CLIENT_STATE_START;

						}
						if (client->rep.u.chunked_data.max == 0)
						{
							client->prim.end_reply (client, HCL_CLIENT_END_REPLY_STATE_OK);
							client->state = HCL_CLIENT_STATE_START;
						}
						else
						{
							client->rep.u.chunked_data.in_data_part = 1;
							client->rep.u.chunked_data.tally = 0;
						}
						break;
					}
					else if (is_digitchar(bc)) 
					{
						client->rep.u.chunked_data.max = client->rep.u.chunked_data.max * 10 + (bc - '0');
						client->rep.u.chunked_data.clcount++;
						ptr++;
					}
					else
					{
						hcl_client_seterrbfmt (client, HCL_EINVAL, "invalid chunk length character  - code[%#x]", bc);
						goto oops;
					}
				}
			}
		}
		else
		{
			hcl_ooci_t c;
			hcl_oow_t bcslen;

	#if defined(HCL_OOCH_IS_UCH)
			hcl_oow_t ucslen;
			hcl_ooch_t uc;
			int n;

			bcslen = end - ptr;
			ucslen = 1;

			n = hcl_conv_bcsn_to_ucsn_with_cmgr(ptr, &bcslen, &uc, &ucslen, client->cmgr, 0);
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

//printf ("[%lc]\n", c);
			if (handle_char(client, c, bcslen) <= -1) goto oops;
		}
	}

	*xlen = ptr - data;
	return 1;

oops:
	/* TODO: compute the number of processes bytes so far and return it via a parameter??? */
/*printf ("feed oops....\n");*/
	return -1;
}


static int feed_all_reply_data (hcl_client_t* client, hcl_bch_t* buf, hcl_oow_t n, hcl_oow_t* xlen)
{
	int x;
	hcl_oow_t total, ylen;

	total = 0;
	while (total < n)
	{
		x = feed_reply_data(client, &buf[total], n - total, &ylen);
		if (x <= -1) return -1;

		total += ylen;

		/*if (ylen == 0) break;*/
		if (x == 0) break; /* incomplete sequence encountered */
	}

	*xlen = total;
	return 0;
}

/* ========================================================================= */

hcl_client_t* hcl_client_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_client_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_client_t* client;
	hcl_t* hcl;
	hcl_vmprim_t vmprim;
	dummy_hcl_xtn_t* xtn;
/*	unsigned int trait;*/

	client = (hcl_client_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*client) + xtnsize);
	if (!client) 
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
		HCL_MMGR_FREE (mmgr, client);
		return HCL_NULL;
	}

	xtn = (dummy_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->client = client;

	HCL_MEMSET (client, 0, HCL_SIZEOF(*client) + xtnsize);
	client->mmgr = mmgr;
	client->cmgr = hcl_get_utf8_cmgr();
	client->prim = *prim;
	client->dummy_hcl = hcl;

	client->cfg.logmask = ~0u;

	/* the dummy hcl is used for this client to perform primitive operations
	 * such as getting system time or logging. so the heap size doesn't 
	 * need to be changed from the tiny value set above. */
	hcl_setoption (client->dummy_hcl, HCL_LOG_MASK, &client->cfg.logmask);
	hcl_setcmgr (client->dummy_hcl, client->cmgr);

	return client;
}

void hcl_client_close (hcl_client_t* client)
{
	if (client->rep.tok.ptr)
	{
		hcl_client_freemem (client, client->rep.tok.ptr);
		client->rep.tok.ptr = HCL_NULL;
		client->rep.tok.len = 0;
		client->rep.tok.capa = 0;
	}

	hcl_close (client->dummy_hcl);
	HCL_MMGR_FREE (client->mmgr, client);
}

int hcl_client_setoption (hcl_client_t* client, hcl_client_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_CLIENT_TRAIT:
			client->cfg.trait = *(const unsigned int*)value;
			return 0;

		case HCL_CLIENT_LOG_MASK:
			client->cfg.logmask = *(const unsigned int*)value;
			if (client->dummy_hcl) 
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get 
				 * affected. new hcl instances to be created later 
				 * is supposed to use the new value */
				hcl_setoption (client->dummy_hcl, HCL_LOG_MASK, value);
			}
			return 0;
	}

	hcl_client_seterrnum (client, HCL_EINVAL);
	return -1;
}

int hcl_client_getoption (hcl_client_t* client, hcl_client_option_t id, void* value)
{
	switch (id)
	{
		case HCL_CLIENT_TRAIT:
			*(unsigned int*)value = client->cfg.trait;
			return 0;

		case HCL_CLIENT_LOG_MASK:
			*(unsigned int*)value = client->cfg.logmask;
			return 0;
	};

	hcl_client_seterrnum (client, HCL_EINVAL);
	return -1;
}


void* hcl_client_getxtn (hcl_client_t* client)
{
	return (void*)(client + 1);
}

hcl_mmgr_t* hcl_client_getmmgr (hcl_client_t* client)
{
	return client->mmgr;
}

hcl_cmgr_t* hcl_client_getcmgr (hcl_client_t* client)
{
	return client->cmgr;
}

void hcl_client_setcmgr (hcl_client_t* client, hcl_cmgr_t* cmgr)
{
	client->cmgr = cmgr;
}

hcl_errnum_t hcl_client_geterrnum (hcl_client_t* client)
{
	return client->errnum;
}

const hcl_ooch_t* hcl_client_geterrstr (hcl_client_t* client)
{
	return hcl_errnum_to_errstr(client->errnum);
}

const hcl_ooch_t* hcl_client_geterrmsg (hcl_client_t* client)
{
	if (client->errmsg.len <= 0) return hcl_errnum_to_errstr(client->errnum);
	return client->errmsg.buf;
}

void hcl_client_seterrnum (hcl_client_t* client, hcl_errnum_t errnum)
{
	/*if (client->shuterr) return; */
	client->errnum = errnum;
	client->errmsg.len = 0;
}

void hcl_client_seterrbfmt (hcl_client_t* client, hcl_errnum_t errnum, const hcl_bch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrbfmtv (client->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (client->dummy_hcl, HCL_COUNTOF(client->errmsg.buf) == HCL_COUNTOF(client->dummy_hcl->errmsg.buf));
	client->errnum = errnum;
	hcl_copyoochars (client->errmsg.buf, client->dummy_hcl->errmsg.buf, HCL_COUNTOF(client->errmsg.buf));
	client->errmsg.len = client->dummy_hcl->errmsg.len;
}

void hcl_client_seterrufmt (hcl_client_t* client, hcl_errnum_t errnum, const hcl_uch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrufmtv (client->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (client->dummy_hcl, HCL_COUNTOF(client->errmsg.buf) == HCL_COUNTOF(client->dummy_hcl->errmsg.buf));
	client->errnum = errnum;
	hcl_copyoochars (client->errmsg.buf, client->dummy_hcl->errmsg.buf, HCL_COUNTOF(client->errmsg.buf));
	client->errmsg.len = client->dummy_hcl->errmsg.len;
}

/* ========================================================================= */

void hcl_client_logbfmt (hcl_client_t* client, unsigned int mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logbfmtv (client->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

void hcl_client_logufmt (hcl_client_t* client, unsigned int mask, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logufmtv (client->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

/* ========================================================================= */

void* hcl_client_allocmem (hcl_client_t* client, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(client->mmgr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	return ptr;
}

void* hcl_client_callocmem (hcl_client_t* client, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(client->mmgr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_client_reallocmem (hcl_client_t* client, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC(client->mmgr, ptr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	return ptr;
}

void hcl_client_freemem (hcl_client_t* client, void* ptr)
{
	HCL_MMGR_FREE (client->mmgr, ptr);
}

/* ========================================================================= */

int hcl_client_start (hcl_client_t* client, const hcl_bch_t* addrs)
{

	/* connect */
	/* send request */

	hcl_oow_t xlen, offset;
	int x;
	int fd = 0; /* read from stdin for testing */
	hcl_bch_t buf[256];
	ssize_t n;

	client->stopreq = 0;
	offset = 0;

	while (1)
	{
		n = read(fd, &buf[offset], HCL_SIZEOF(buf) - offset); /* switch to recv  */
		if (n <= -1) 
		{
			printf ("error....%s\n", strerror(n));
			return -1;
		}
		if (n == 0) 
		{
			/* TODO: check if there is residue in the receiv buffer */
			break;
		}

		x = feed_all_reply_data (client, buf, n, &xlen);
		if (x <= -1) return -1;

		offset = n - xlen;
		if (offset > 0) HCL_MEMMOVE (&buf[0], &buf[xlen], offset);
	}

	return 0;
}

void hcl_client_stop (hcl_client_t* client)
{
	client->stopreq = 1;
}
