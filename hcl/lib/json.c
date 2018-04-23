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


typedef struct hcl_json_state_node_t hcl_json_state_node_t;
struct hcl_json_state_node_t
{
	hcl_json_state_t state;
	union
	{
		struct
		{
			int got_value;
		} ia; /* in array */

		struct
		{
			/* 0: ready to get key (at the beginning or got comma), 
			 * 1: got key, 2: got colon, 3: got value */
			int state; 
		} id; /* in dictionary */
		struct
		{
			int escaped;
			int digit_count;
			hcl_ooch_t acc;
		} sv;
		struct
		{
			int escaped;
			int digit_count;
			hcl_ooch_t acc;
		} cv;
		struct
		{
			int dotted;
		} nv;
	} u;
	hcl_json_state_node_t* next;
};

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

	hcl_json_state_node_t state_top;
	hcl_json_state_node_t* state_stack;

	hcl_oocs_t tok;
	hcl_oow_t tok_capa;
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

static void clear_token (hcl_json_t* json)
{
	json->tok.len = 0;
}

static int add_char_to_token (hcl_json_t* json, hcl_ooch_t ch)
{
	if (json->tok.len >= json->tok_capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN_POW2(json->tok.len + 1, HCL_JSON_TOKEN_NAME_ALIGN);
		tmp = hcl_json_reallocmem(json, json->tok.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		json->tok_capa = newcapa;
		json->tok.ptr = tmp;
	}

	json->tok.ptr[json->tok.len++] = ch;
	return 0;
}

static HCL_INLINE int is_token (hcl_json_t* json, const hcl_bch_t* str)
{
	return hcl_comp_oochars_bcstr(json->tok.ptr, json->tok.len, str) == 0;
} 

static HCL_INLINE int is_token_integer (hcl_json_t* json, hcl_oow_t* value)
{
	hcl_oow_t i;
	hcl_oow_t v = 0;

	if (json->tok.len <= 0) return 0;

	for (i = 0; i < json->tok.len; i++)
	{
		if (!is_digitchar(json->tok.ptr[i])) return 0;
		v = v * 10 + (json->tok.ptr[i] - '0');
	}

	*value = v;
	return 1;
} 

static HCL_INLINE hcl_ooch_t unescape (hcl_ooch_t c)
{
	switch (c)
	{
		case 'a': return '\a';
		case 'b': return '\b';
		case 'f': return '\f';
		case 'n': return '\n';
		case 'r': return '\r';
		case 't': return '\t';
		case 'v': return '\v';
		default: return c;
	}
}

/* ========================================================================= */

static int push_state (hcl_json_t* json, hcl_json_state_t state)
{
	hcl_json_state_node_t* ss;

	ss = hcl_json_callocmem(json, HCL_SIZEOF(*ss));
	if (!ss) return -1;

	ss->state = state;
	ss->next = json->state_stack;
	
	json->state_stack = ss;
	return 0;
}

static void pop_state (hcl_json_t* json)
{
	hcl_json_state_node_t* ss;

	ss = json->state_stack;
	HCL_ASSERT (json->dummy_hcl, ss != HCL_NULL && ss != &json->state_top);
	json->state_stack = ss->next;

	if (json->state_stack->state == HCL_JSON_STATE_IN_ARRAY)
	{
		json->state_stack->u.ia.got_value = 1;
	}
	else if (json->state_stack->state == HCL_JSON_STATE_IN_DIC)
	{
		json->state_stack->u.id.state++;
	}

/* TODO: don't free this. move it to the free list? */
	hcl_json_freemem (json, ss);
}

static void pop_all_states (hcl_json_t* json)
{
	while (json->state_stack != &json->state_top) pop_state (json);
}

/* ========================================================================= */

static int invoke_data_inst (hcl_json_t* json, hcl_json_inst_t inst)
{
	if (json->state_stack->state == HCL_JSON_STATE_IN_DIC && json->state_stack->u.id.state == 1) 
	{
		if (inst != HCL_JSON_INST_STRING)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "dictionary key not a string - %.*js", json->tok.len, json->tok.ptr);
			return -1;
		}

		inst = HCL_JSON_INST_KEY;
	}

	if (json->prim.instcb (json, inst, &json->tok) <= -1) return -1;
	return 0;
}

static int handle_string_value_char (hcl_json_t* json, hcl_ooci_t c)
{
	int ret = 1;
	
	
	if (json->state_stack->u.sv.escaped == 3)
	{
		if (c >= '0' && c <= '7')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 8 + c - '0';
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else
		{
			ret = 0;
			goto add_sv_acc;
		}
	}
	else if (json->state_stack->u.sv.escaped >= 2)
	{
		if (c >= '0' && c <= '9')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 16 + c - '0';
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else if (c >= 'a' && c <= 'f')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 16 + c - 'a' + 10;
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else if (c >= 'A' && c <= 'F')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 16 + c - 'A' + 10;
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else
		{
			ret = 0;
		add_sv_acc:
			if (add_char_to_token(json, json->state_stack->u.sv.acc) <= -1) return -1;
			json->state_stack->u.sv.escaped = 0;
		}
	}
	else if (json->state_stack->u.sv.escaped == 1)
	{
		if (c >= '0' && c <= '8') 
		{
			json->state_stack->u.sv.escaped = 3;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = c - '0';
		}
		else if (c == 'x')
		{
			json->state_stack->u.sv.escaped = 2;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = 0;
		}
		else if (c == 'u')
		{
			json->state_stack->u.sv.escaped = 4;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = 0;
		}
		else if (c == 'U')
		{
			json->state_stack->u.sv.escaped = 8;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = 0;
		}
		else
		{
			json->state_stack->u.sv.escaped = 0;
			if (add_char_to_token(json, unescape(c)) <= -1) return -1;
		}
	}
	else if (c == '\\')
	{
		json->state_stack->u.sv.escaped = 1;
	}
	else if (c == '\"')
	{
		pop_state (json);
		if (invoke_data_inst(json, HCL_JSON_INST_STRING) <= -1) return -1;
	}
	else
	{
		if (add_char_to_token(json, c) <= -1) return -1;
	}

	return ret;
}

static int handle_character_value_char (hcl_json_t* json, hcl_ooci_t c)
{
	/* The real JSON dones't support character literal. this is HCL's own extension. */
	int ret = 1;

	if (json->state_stack->u.cv.escaped == 3)
	{
		if (c >= '0' && c <= '7')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 8 + c - '0';
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else
		{
			ret = 0;
			goto add_cv_acc;
		}
	}
	if (json->state_stack->u.cv.escaped >= 2)
	{
		if (c >= '0' && c <= '9')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 16 + c - '0';
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else if (c >= 'a' && c <= 'f')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 16 + c - 'a' + 10;
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else if (c >= 'A' && c <= 'F')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 16 + c - 'A' + 10;
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else
		{
			ret = 0;
		add_cv_acc:
			if (add_char_to_token(json, json->state_stack->u.cv.acc) <= -1) return -1;
			json->state_stack->u.cv.escaped = 0;
		}
	}
	else if (json->state_stack->u.cv.escaped == 1)
	{
		if (c >= '0' && c <= '8') 
		{
			json->state_stack->u.cv.escaped = 3;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = c - '0';
		}
		else if (c == 'x')
		{
			json->state_stack->u.cv.escaped = 2;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = 0;
		}
		else if (c == 'u')
		{
			json->state_stack->u.cv.escaped = 4;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = 0;
		}
		else if (c == 'U')
		{
			json->state_stack->u.cv.escaped = 8;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = 0;
		}
		else
		{
			json->state_stack->u.cv.escaped = 0;
			if (add_char_to_token(json, unescape(c)) <= -1) return -1;
		}
	}
	else if (c == '\\')
	{
		json->state_stack->u.cv.escaped = 1;
	}
	else if (c == '\'')
	{
		pop_state (json);
		
		if (json->tok.len < 1)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "no character in a character literal");
			return -1;
		}
		if (invoke_data_inst(json, HCL_JSON_INST_CHARACTER) <= -1) return -1;
	}
	else
	{
		if (add_char_to_token(json, c) <= -1) return -1;
	}

	if (json->tok.len >= 1) 
	{
		hcl_json_seterrbfmt (json, HCL_EINVAL, "too many characters in a character literal - %.*js", json->tok.len, json->tok.ptr);
		return -1;
	}
	return ret;
}

static int handle_numeric_value_char (hcl_json_t* json, hcl_ooci_t c)
{
	if (is_digitchar(c) || (json->tok.len == 0 && (c == '+' || c == '-')))
	{
		if (add_char_to_token(json, c) <= -1) return -1;
		return 1;
	}
	else if (!json->state_stack->u.nv.dotted && c == '.' &&
	         json->tok.len > 0 && is_digitchar(json->tok.ptr[json->tok.len - 1]))
	{
		if (add_char_to_token(json, c) <= -1) return -1;
		json->state_stack->u.nv.dotted = 1;
		return 1;
	}

	pop_state (json);

	HCL_ASSERT (json->dummy_hcl, json->tok.len > 0);
	if (!is_digitchar(json->tok.ptr[json->tok.len - 1]))
	{
		hcl_json_seterrbfmt (json, HCL_EINVAL, "invalid numeric value - %.*js", json->tok.len, json->tok.ptr);
		return -1;
	}
	if (invoke_data_inst(json, HCL_JSON_INST_NUMBER) <= -1) return -1;
	return 0; /* start over */
}

static int handle_word_value_char (hcl_json_t* json, hcl_ooci_t c)
{
	hcl_json_inst_t inst;

	if (is_alphachar(c))
	{
		if (add_char_to_token(json, c) <= -1) return -1;
		return 1;
	}

	pop_state (json);

	if (hcl_comp_oochars_bcstr(json->tok.ptr, json->tok.len, "null") == 0) inst = HCL_JSON_INST_NIL;
	else if (hcl_comp_oochars_bcstr(json->tok.ptr, json->tok.len, "true") == 0) inst = HCL_JSON_INST_TRUE;
	else if (hcl_comp_oochars_bcstr(json->tok.ptr, json->tok.len, "false") == 0) inst = HCL_JSON_INST_FALSE;
	else
	{
		hcl_json_seterrbfmt (json, HCL_EINVAL, "invalid word value - %.*js", json->tok.len, json->tok.ptr);
		return -1;
	}

	if (invoke_data_inst(json, inst) <= -1) return -1;
	return 0; /* start over */
}

/* ========================================================================= */

static int handle_start_char (hcl_json_t* json, hcl_ooci_t c)
{
	if (c == '[')
	{
		if (push_state(json, HCL_JSON_STATE_IN_ARRAY) <= -1) return -1;
		json->state_stack->u.ia.got_value = 0;
		if (json->prim.instcb (json, HCL_JSON_INST_START_ARRAY, HCL_NULL) <= -1) return -1;
		return 1;
	}
	else if (c == '{')
	{
		if (push_state(json, HCL_JSON_STATE_IN_DIC) <= -1) return -1;
		json->state_stack->u.id.state = 0;
		if (json->prim.instcb (json, HCL_JSON_INST_START_DIC, HCL_NULL) <= -1) return -1;
		return 1;
	}
	else if (is_spacechar(c)) 
	{
		/* do nothing */
		return 1;
	}
	else
	{
		hcl_json_seterrbfmt (json, HCL_EINVAL, "not starting with [ or { - %jc", (hcl_ooch_t)c);
		return -1;
	}
}

static int handle_char_in_array (hcl_json_t* json, hcl_ooci_t c)
{
	if (c == ']')
	{
		if (json->prim.instcb (json, HCL_JSON_INST_END_ARRAY, HCL_NULL) <= -1) return -1;
		pop_state (json);
		return 1;
	}
	else if (c == ',')
	{
		if (!json->state_stack->u.ia.got_value)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "redundant comma in array - %jc", (hcl_ooch_t)c);
			return -1;
		}
		json->state_stack->u.ia.got_value = 0;
		return 1;
	}
	else if (is_spacechar(c))
	{
		/* do nothing */
		return 1;
	}
	else
	{
		if (json->state_stack->u.ia.got_value)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "comma required in array - %jc", (hcl_ooch_t)c);
			return -1;
		}

		if (c == '\"')
		{
			if (push_state(json, HCL_JSON_STATE_IN_STRING_VALUE) <= -1) return -1;
			clear_token (json);
			return 1;
		}
		else if (c == '\'')
		{
			if (push_state(json, HCL_JSON_STATE_IN_CHARACTER_VALUE) <= -1) return -1;
			clear_token (json);
			return 1;
		}
		/* TOOD: else if (c == '#') HCL radixed number
		 */
		else if (is_digitchar(c) || c == '+' || c == '-')
		{
			if (push_state(json, HCL_JSON_STATE_IN_NUMERIC_VALUE) <= -1) return -1;
			clear_token (json);
			json->state_stack->u.nv.dotted = 0;
			return 0; /* start over */
		}
		else if (is_alphachar(c))
		{
			if (push_state(json, HCL_JSON_STATE_IN_WORD_VALUE) <= -1) return -1;
			clear_token (json);
			return 0; /* start over */
		}
		else if (c == '[')
		{
			if (push_state(json, HCL_JSON_STATE_IN_ARRAY) <= -1) return -1;
			json->state_stack->u.ia.got_value = 0;
			if (json->prim.instcb (json, HCL_JSON_INST_START_ARRAY, HCL_NULL) <= -1) return -1;
			return 1;
		}
		else if (c == '{')
		{
			if (push_state(json, HCL_JSON_STATE_IN_DIC) <= -1) return -1;
			json->state_stack->u.id.state = 0;
			if (json->prim.instcb (json, HCL_JSON_INST_START_DIC, HCL_NULL) <= -1) return -1;
			return 1;
		}
		else
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "wrong character inside array - %jc", (hcl_ooch_t)c);
			return -1;
		}
	}
}

static int handle_char_in_dic (hcl_json_t* json, hcl_ooci_t c)
{
	if (c == '}')
	{
		if (json->prim.instcb (json, HCL_JSON_INST_END_DIC, HCL_NULL) <= -1) return -1;
		pop_state (json);
		return 1;
	}
	else if (c == ':')
	{
		if (json->state_stack->u.id.state != 1)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "redundant colon in dictionary - %jc", (hcl_ooch_t)c);
			return -1;
		}
		json->state_stack->u.id.state++;
		return 1;
	}
	else if (c == ',')
	{
		if (json->state_stack->u.id.state != 3)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "redundant comma in dicitonary - %jc", (hcl_ooch_t)c);
			return -1;
		}
		json->state_stack->u.id.state = 0;
		return 1;
	}
	else if (is_spacechar(c))
	{
		/* do nothing */
		return 1;
	}
	else
	{
		if (json->state_stack->u.id.state == 1)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "colon required in dicitonary - %jc", (hcl_ooch_t)c);
			return -1;
		}
		else if (json->state_stack->u.id.state == 3)
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "comma required in dicitonary - %jc", (hcl_ooch_t)c);
			return -1;
		}

		if (c == '\"')
		{
			if (push_state(json, HCL_JSON_STATE_IN_STRING_VALUE) <= -1) return -1;
			clear_token (json);
			return 1;
		}
		else if (c == '\'')
		{
			if (push_state(json, HCL_JSON_STATE_IN_CHARACTER_VALUE) <= -1) return -1;
			clear_token (json);
			return 1;
		}
		/* TOOD: else if (c == '#') HCL radixed number
		 */
		else if (is_digitchar(c) || c == '+' || c == '-')
		{
			if (push_state(json, HCL_JSON_STATE_IN_NUMERIC_VALUE) <= -1) return -1;
			clear_token (json);
			json->state_stack->u.nv.dotted = 0;
			return 0; /* start over */
		}
		else if (is_alphachar(c))
		{
			if (push_state(json, HCL_JSON_STATE_IN_WORD_VALUE) <= -1) return -1;
			clear_token (json);
			return 0; /* start over */
		}
		else if (c == '[')
		{
			if (push_state(json, HCL_JSON_STATE_IN_ARRAY) <= -1) return -1;
			json->state_stack->u.ia.got_value = 0;
			if (json->prim.instcb (json, HCL_JSON_INST_START_ARRAY, HCL_NULL) <= -1) return -1;
			return 1;
		}
		else if (c == '{')
		{
			if (push_state(json, HCL_JSON_STATE_IN_DIC) <= -1) return -1;
			json->state_stack->u.id.state = 0;
			if (json->prim.instcb (json, HCL_JSON_INST_START_DIC, HCL_NULL) <= -1) return -1;
			return 1;
		}
		else
		{
			hcl_json_seterrbfmt (json, HCL_EINVAL, "wrong character inside array - %jc", (hcl_ooch_t)c);
			return -1;
		}
	}
}

/* ========================================================================= */

static int handle_char (hcl_json_t* json, hcl_ooci_t c, hcl_oow_t nbytes)
{
	int x;

start_over:
	if (c == HCL_OOCI_EOF)
	{
		if (json->state_stack->state == HCL_JSON_STATE_START)
		{
			/* no input data */
			return 0;
		}
		else
		{
			hcl_json_seterrbfmt (json, HCL_EFINIS, "unexpected end of data");
			return -1;
		}
	}

	switch (json->state_stack->state)
	{
		case HCL_JSON_STATE_START:
			x = handle_start_char(json, c);
			break;

		case HCL_JSON_STATE_IN_ARRAY:
			x = handle_char_in_array(json, c);
			break;

		case HCL_JSON_STATE_IN_DIC:
			x = handle_char_in_dic(json, c);
			break;

		case HCL_JSON_STATE_IN_WORD_VALUE:
			x = handle_word_value_char(json, c);
			break;

		case HCL_JSON_STATE_IN_STRING_VALUE:
			x = handle_string_value_char(json, c);
			break;
			
		case HCL_JSON_STATE_IN_CHARACTER_VALUE:
			x = handle_character_value_char(json, c);
			break;

		case HCL_JSON_STATE_IN_NUMERIC_VALUE:
			x = handle_numeric_value_char(json, c);
			break;

		default:
			hcl_json_seterrbfmt (json, HCL_EINTERN, "internal error - must not be called for state %d", (int)json->state_stack->state);
			return -1;
	}

	if (x <= -1) return -1;
	if (x == 0) goto start_over;

	return 0;
}

/* ========================================================================= */

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

			/* advance 1 byte without proper conversion */
			uc = *ptr;
			bcslen = 1;
		}

		ptr += bcslen;
		c = uc;
	#else
		bcslen = 1;
		c = *ptr++;
	#endif


		/* handle a signle character */
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


	json->state_top.state = HCL_JSON_STATE_START;
	json->state_top.next = HCL_NULL;
	json->state_stack = &json->state_top;

	return json;
}

void hcl_json_close (hcl_json_t* json)
{
	pop_all_states (json);
	if (json->tok.ptr) hcl_json_freemem (json, json->tok.ptr);
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
	return json->state_stack->state;
}

void hcl_json_reset (hcl_json_t* json)
{
	/* TODO: reset XXXXXXXXXXXXXXXXXXXXXXXXXXXxxxxx */
	pop_all_states (json);
	HCL_ASSERT (json->dummy_hcl, json->state_stack == &json->state_top);
	json->state_stack->state = HCL_JSON_STATE_START;
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
