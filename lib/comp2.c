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

enum
{
	VAR_NAMED,
	VAR_INDEXED
};

#define TV_BUFFER_ALIGN 256
#define BLK_INFO_BUFFER_ALIGN 128

/* --------------------------------------------

                                                     
(defun plus(x y) 
	(printf "plus %d %d\n" x y)
	(defun minus(x y)
		(printf "minus %d %d\n" x y)              
		(- x y)
	)
	(+ x y)
)

(defun dummy(q)
	(printf "%s\n" q)
)

(plus 10 20)
    <---- minus is now available

(minus 10 1)
 
literals -->
//
// characeter 'A'
// "string"
// B"byte string"
// array ---> #[   ] or [ ] ? constant or not?  dynamic???
// hash table - dictionary  ---> #{   } or { } <--- ambuguity with blocks...
// the rest must be manipulated with code...

------------------------------ */

static int copy_string_to (hcl_t* hcl, const hcl_oocs_t* src, hcl_oocs_t* dst, hcl_oow_t* dstcapa, int append, hcl_ooch_t delim_char)
{
	hcl_oow_t len, pos;

	if (append)
	{
		pos = dst->len;
		len = dst->len + src->len;
		if (delim_char != '\0') len++;
	}
	else
	{
		pos = 0;
		len = src->len;
	}

	if (len >= *dstcapa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t capa;

		capa = HCL_ALIGN(len + 1, TV_BUFFER_ALIGN);

		tmp = (hcl_ooch_t*)hcl_reallocmem(hcl, dst->ptr, HCL_SIZEOF(*tmp) * capa);
		if (HCL_UNLIKELY(!tmp)) return -1;

		dst->ptr = tmp;
		*dstcapa = capa - 1;
	}

	if (append && delim_char != '\0') dst->ptr[pos++] = delim_char;
	hcl_copy_oochars (&dst->ptr[pos], src->ptr, src->len);
	dst->ptr[len] = '\0';
	dst->len = len;
	return 0;
}

static int __find_word_in_string (const hcl_oocs_t* haystack, const hcl_oocs_t* name, int last, hcl_oow_t* xindex)
{
	/* this function is inefficient. but considering the typical number
	 * of arguments and temporary variables, the inefficiency can be 
	 * ignored in my opinion. the overhead to maintain the reverse lookup
	 * table from a name to an index should be greater than this simple
	 * inefficient lookup */

	hcl_ooch_t* t, * e;
	hcl_oow_t index, i, found;

	t = haystack->ptr;
	e = t + haystack->len;
	index = 0;
	found = HCL_TYPE_MAX(hcl_oow_t);

	while (t < e)
	{
		while (t < e && *t == ' ') t++;

		for (i = 0; i < name->len; i++)
		{
			if (t >= e || name->ptr[i] != *t) goto unmatched;
			t++;
		}
		if (t >= e || *t == ' ') 
		{
			if (last)
			{
				found = index;
			}
			else
			{
				if (xindex) *xindex = index;
				return 0; /* found */
			}
		}

	unmatched:
		while (t < e)
		{
			if (*t == ' ')
			{
				t++;
				break;
			}
			t++;
		}

		index++;
	}

	if (found != HCL_TYPE_MAX(hcl_oow_t)) 
	{
		if (xindex) *xindex = found;
		return 0; /* foudn */
	}

	return -1; /* not found */
}


static int add_temporary_variable (hcl_t* hcl, const hcl_oocs_t* name, hcl_oow_t dup_check_start)
{
	hcl_oocs_t s;
	int x;

	s.ptr = hcl->c->tv2.s.ptr + dup_check_start;
	s.len = hcl->c->tv2.s.len - dup_check_start;
	if (__find_word_in_string(&s, name, 0, HCL_NULL) >= 0)
	{
		hcl_seterrnum (hcl, HCL_EEXIST);
		return -1;
	}
	x = copy_string_to(hcl, name, &hcl->c->tv2.s, &hcl->c->tv2.capa, 1, ' ');
	if (HCL_LIKELY(x >= 0)) hcl->c->tv2.wcount++;
	return x;
}

static int find_temporary_variable_backward (hcl_t* hcl, const hcl_oocs_t* name, hcl_oow_t* index)
{
	/* find the last element */
	return __find_word_in_string(&hcl->c->tv2.s, name, 1, index);
}

static int store_temporary_variable_count_for_block (hcl_t* hcl, hcl_oow_t tmpr_count, hcl_oow_t tmpr_len, hcl_oow_t lfbase)
{
	HCL_ASSERT (hcl, hcl->c->blk.depth >= 0);

	if (hcl->c->blk.depth >= hcl->c->blk.info_capa)
	{
		hcl_blk_info_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN(hcl->c->blk.depth + 1, BLK_INFO_BUFFER_ALIGN);
		tmp = (hcl_blk_info_t*)hcl_reallocmem(hcl, hcl->c->blk.info, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->c->blk.info_capa = newcapa;
		hcl->c->blk.info = tmp;
	}

	hcl->c->blk.info[hcl->c->blk.depth].tmprlen = tmpr_len;
	hcl->c->blk.info[hcl->c->blk.depth].tmprcnt = tmpr_count;
	hcl->c->blk.info[hcl->c->blk.depth].lfbase = lfbase;
	return 0;
}

/* ========================================================================= */

static int add_literal (hcl_t* hcl, hcl_oop_t obj, hcl_oow_t* index)
{
	hcl_oow_t capa, i, lfbase = 0;


	lfbase = (hcl->option.trait & HCL_TRAIT_INTERACTIVE)? hcl->c->blk.info[hcl->c->blk.depth].lfbase: 0;

	/* TODO: speed up the following duplicate check loop */
	for (i = lfbase; i < hcl->code.lit.len; i++)
	{
		/* this removes redundancy of symbols, characters, and integers. */
		if (((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i] == obj)
		{
			*index = i - lfbase;
			return i;
		}
	}

	capa = HCL_OBJ_GET_SIZE(hcl->code.lit.arr);
	if (hcl->code.lit.len >= capa)
	{
		hcl_oop_t tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN(capa + 1, HCL_LIT_BUFFER_ALIGN);
		tmp = hcl_remakengcarray(hcl, (hcl_oop_t)hcl->code.lit.arr, newcapa);
		if (!tmp) return -1;

		hcl->code.lit.arr = (hcl_oop_oop_t)tmp;
	}

	*index = hcl->code.lit.len - lfbase;

	((hcl_oop_oop_t)hcl->code.lit.arr)->slot[hcl->code.lit.len++] = obj;
	return 0;
}


/* ========================================================================= */

static HCL_INLINE void patch_instruction (hcl_t* hcl, hcl_oow_t index, hcl_oob_t bc)
{
	HCL_ASSERT (hcl, index < hcl->code.bc.len);
	hcl->code.bc.ptr[index] = bc;
}

static int emit_byte_instruction (hcl_t* hcl, hcl_oob_t bc, const hcl_ioloc_t* srcloc)
{
	/* the context object has the ip field. it should be representable
	 * in a small integer. for simplicity, limit the total byte code length
	 * to fit in a small integer. because 'ip' points to the next instruction
	 * to execute, the upper bound should be (max - 1) so that 'ip' stays
	 * at the max when incremented */
	if (hcl->code.bc.len == HCL_SMOOI_MAX - 1)
	{
		hcl_seterrnum (hcl, HCL_EBCFULL); /* byte code full/too big */
		return -1;
	}

	if (hcl->code.bc.len >= hcl->code.bc.capa)
	{
		hcl_oow_t newcapa;
		hcl_oob_t* tmp;
		hcl_oow_t* tmp2;

		newcapa = HCL_ALIGN(hcl->code.bc.capa + 1, HCL_BC_BUFFER_ALIGN);
		tmp = (hcl_oob_t*)hcl_reallocmem(hcl, hcl->code.bc.ptr, HCL_SIZEOF(*tmp) * newcapa);
		if (HCL_UNLIKELY(!tmp)) return -1;

		tmp2 = (hcl_oow_t*)hcl_reallocmem(hcl, hcl->code.locptr, HCL_SIZEOF(*tmp2) * newcapa);
		if (HCL_UNLIKELY(!tmp2))
		{
			hcl_freemem (hcl, tmp);
			return -1;
		}
		HCL_MEMSET (&tmp2[hcl->code.bc.capa], 0, HCL_SIZEOF(*tmp2) * (newcapa - hcl->code.bc.capa));

		hcl->code.bc.ptr = tmp;
		hcl->code.bc.capa = newcapa;
		hcl->code.locptr = tmp2;
	}

	hcl->code.bc.ptr[hcl->code.bc.len] = bc;

	if (srcloc)
	{
		hcl->code.locptr[hcl->code.bc.len] = srcloc->line;
	}

	hcl->code.bc.len++;
	return 0;
}

/* 
COMMENTED OUT TEMPORARILY
int hcl_emitbyteinstruction (hcl_t* hcl, hcl_oob_t bc)
{
	return emit_byte_instruction(hcl, bc, HCL_NULL);
}*/

static int emit_single_param_instruction (hcl_t* hcl, int cmd, hcl_oow_t param_1)
{
	hcl_oob_t bc;

	switch (cmd)
	{
		case HCL_CODE_PUSH_INSTVAR_0:
		case HCL_CODE_STORE_INTO_INSTVAR_0:
		case HCL_CODE_POP_INTO_INSTVAR_0:
		case HCL_CODE_PUSH_TEMPVAR_0:
		case HCL_CODE_STORE_INTO_TEMPVAR_0:
		case HCL_CODE_POP_INTO_TEMPVAR_0:
			if (param_1 < 8)
			{
				/* low 3 bits to hold the parameter */
				bc = (hcl_oob_t)(cmd & 0xF8) | (hcl_oob_t)param_1;
				goto write_short;
			}
			else
			{
				/* convert the code to a long version */
				bc = cmd | 0x80;
				goto write_long;
			}

		case HCL_CODE_PUSH_LITERAL_0:
			if (param_1 < 8)
			{
				/* low 3 bits to hold the parameter */
				bc = (hcl_oob_t)(cmd & 0xF8) | (hcl_oob_t)param_1;
				goto write_short;
			}
			else if (param_1 <= MAX_CODE_PARAM)
			{
				bc = HCL_CODE_PUSH_LITERAL_X; /* cmd | 0x80 */
				goto write_long;
			}
			else
			{
				bc = HCL_CODE_PUSH_LITERAL_X2; /* HCL_CODE_PUSH_LITERAL_4 | 0x80 */
				goto write_long2;
			}

		case HCL_CODE_PUSH_OBJECT_0:
		case HCL_CODE_STORE_INTO_OBJECT_0:
		case HCL_CODE_POP_INTO_OBJECT_0:
		case HCL_CODE_JUMP_FORWARD_0:
		case HCL_CODE_JUMP_BACKWARD_0:
		case HCL_CODE_CALL_0:
			if (param_1 < 4)
			{
				/* low 2 bits to hold the parameter */
				bc = (hcl_oob_t)(cmd & 0xFC) | (hcl_oob_t)param_1;
				goto write_short;
			}
			else
			{
				/* convert the code to a long version */
				bc = cmd | 0x80;
				goto write_long;
			}

		case HCL_CODE_JUMP_FORWARD_IF_TRUE:
		case HCL_CODE_JUMP_FORWARD_IF_FALSE:
		case HCL_CODE_JUMP2_FORWARD_IF_TRUE:
		case HCL_CODE_JUMP2_FORWARD_IF_FALSE:
		case HCL_CODE_JUMP2_FORWARD:
		case HCL_CODE_JUMP_BACKWARD_IF_TRUE:
		case HCL_CODE_JUMP_BACKWARD_IF_FALSE:
		case HCL_CODE_JUMP2_BACKWARD_IF_TRUE:
		case HCL_CODE_JUMP2_BACKWARD_IF_FALSE:
		case HCL_CODE_JUMP2_BACKWARD:
		case HCL_CODE_PUSH_INTLIT:
		case HCL_CODE_PUSH_NEGINTLIT:
		case HCL_CODE_PUSH_CHARLIT:

		case HCL_CODE_MAKE_DIC: /* TODO: don't these need write_long2? */
		case HCL_CODE_MAKE_ARRAY:
		case HCL_CODE_MAKE_BYTEARRAY:
		case HCL_CODE_POP_INTO_ARRAY:
		case HCL_CODE_POP_INTO_BYTEARRAY:
			bc = cmd;
			goto write_long;
	}

	hcl_seterrnum (hcl, HCL_EINVAL);
	return -1;

write_short:
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1) return -1;
	return 0;

write_long:
	if (param_1 > MAX_CODE_PARAM) 
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_HCL_CODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 8) & 0xFF, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF, HCL_NULL) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1, HCL_NULL) <= -1) return -1;
#endif
	return 0;

write_long2:
	if (param_1 > MAX_CODE_PARAM2) 
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_HCL_CODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 24) & 0xFF, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 16) & 0xFF, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >>  8) & 0xFF, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF, HCL_NULL) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 8) & 0xFF, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF, HCL_NULL) <= -1) return -1;
#endif
	return 0;
}

static int emit_double_param_instruction (hcl_t* hcl, int cmd, hcl_oow_t param_1, hcl_oow_t param_2)
{
	hcl_oob_t bc;

	switch (cmd)
	{
		case HCL_CODE_STORE_INTO_CTXTEMPVAR_0:
		case HCL_CODE_POP_INTO_CTXTEMPVAR_0:
		case HCL_CODE_PUSH_CTXTEMPVAR_0:
		case HCL_CODE_PUSH_OBJVAR_0:
		case HCL_CODE_STORE_INTO_OBJVAR_0:
		case HCL_CODE_POP_INTO_OBJVAR_0:
		case HCL_CODE_SEND_MESSAGE_0:
		case HCL_CODE_SEND_MESSAGE_TO_SUPER_0:
			if (param_1 < 4 && param_2 < 0xFF)
			{
				/* low 2 bits of the instruction code is the first parameter */
				bc = (hcl_oob_t)(cmd & 0xFC) | (hcl_oob_t)param_1;
				goto write_short;
			}
			else
			{
				/* convert the code to a long version */
				bc = cmd | 0x80; 
				goto write_long;
			}

		/* MAKE_FUNCTION is a quad-parameter instruction. 
		 * The caller must emit two more parameters after the call to this function.
		 * however the instruction format is the same up to the second
		 * parameters between MAKE_FUNCTION and MAKE_BLOCK.
		 */
		case HCL_CODE_MAKE_FUNCTION: 
		case HCL_CODE_MAKE_BLOCK:
			bc = cmd;
			goto write_long;
	}

	hcl_seterrnum (hcl, HCL_EINVAL);
	return -1;

write_short:
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_2, HCL_NULL) <= -1) return -1;
	return 0;

write_long:
	if (param_1 > MAX_CODE_PARAM || param_2 > MAX_CODE_PARAM)
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_HCL_CODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1 >> 8, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_2 >> 8, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_2 & 0xFF, HCL_NULL) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_1, HCL_NULL) <= -1 ||
	    emit_byte_instruction(hcl, param_2, HCL_NULL) <= -1) return -1;
#endif
	return 0;
}

static HCL_INLINE int emit_long_param (hcl_t* hcl, hcl_oow_t param)
{
	if (param > MAX_CODE_PARAM)
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}

#if (HCL_HCL_CODE_LONG_PARAM_SIZE == 2)
	return (emit_byte_instruction(hcl, param >> 8, HCL_NULL) <= -1 ||
	        emit_byte_instruction(hcl, param & 0xFF, HCL_NULL) <= -1)? -1: 0;
#else
	return emit_byte_instruction(hcl, param_1, HCL_NULL);
#endif
}

static int emit_push_literal (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oow_t index;

	if (HCL_OOP_IS_SMOOI(obj))
	{
		hcl_ooi_t i;

		i = HCL_OOP_TO_SMOOI(obj);
		switch (i)
		{
			case -1:
				return emit_byte_instruction(hcl, HCL_CODE_PUSH_NEGONE, HCL_NULL);

			case 0:
				return emit_byte_instruction(hcl, HCL_CODE_PUSH_ZERO, HCL_NULL);

			case 1:
				return emit_byte_instruction(hcl, HCL_CODE_PUSH_ONE, HCL_NULL);

			case 2:
				return emit_byte_instruction(hcl, HCL_CODE_PUSH_TWO, HCL_NULL);
		}

		if (i >= 0 && i <= MAX_CODE_PARAM)
		{
			return emit_single_param_instruction(hcl, HCL_CODE_PUSH_INTLIT, i);
		}
		else if (i < 0 && i >= -(hcl_ooi_t)MAX_CODE_PARAM)
		{
			return emit_single_param_instruction(hcl, HCL_CODE_PUSH_NEGINTLIT, -i);
		}
	}
	else if (HCL_OOP_IS_CHAR(obj))
	{
		hcl_ooch_t i;

		i = HCL_OOP_TO_CHAR(obj);

		if (i >= 0 && i <= MAX_CODE_PARAM)
			return emit_single_param_instruction(hcl, HCL_CODE_PUSH_CHARLIT, i);
	}

	if (add_literal(hcl, obj, &index) <= -1 ||
	    emit_single_param_instruction(hcl, HCL_CODE_PUSH_LITERAL_0, index) <= -1) return -1;

	return 0;
}

static HCL_INLINE void patch_long_jump (hcl_t* hcl, hcl_ooi_t jip, hcl_ooi_t jump_offset)
{
	if (jump_offset > MAX_CODE_JUMP)
	{
		/* switch to JUMP2 instruction to allow a bigger jump offset.
		 * up to twice MAX_CODE_JUMP only */
		
		HCL_ASSERT (hcl, jump_offset <= MAX_CODE_JUMP * 2);

		HCL_ASSERT (hcl, hcl->code.bc.ptr[jip] == HCL_CODE_JUMP_FORWARD_X ||
		                 hcl->code.bc.ptr[jip] == HCL_CODE_JUMP_FORWARD_IF_TRUE ||
		                 hcl->code.bc.ptr[jip] == HCL_CODE_JUMP_FORWARD_IF_FALSE ||
		                 hcl->code.bc.ptr[jip] == HCL_CODE_JUMP_BACKWARD_X ||
		                 hcl->code.bc.ptr[jip] == HCL_CODE_JUMP_BACKWARD_IF_TRUE ||
		                 hcl->code.bc.ptr[jip] == HCL_CODE_JUMP_BACKWARD_IF_FALSE);

		/* JUMP2 instructions are chosen to be greater than its JUMP counterpart by 1 */
		patch_instruction (hcl, jip, hcl->code.bc.ptr[jip] + 1); 
		jump_offset -= MAX_CODE_JUMP;
	}

#if (HCL_HCL_CODE_LONG_PARAM_SIZE == 2)
	patch_instruction (hcl, jip + 1, jump_offset >> 8);
	patch_instruction (hcl, jip + 2, jump_offset & 0xFF);
#else
	patch_instruction (hcl, jip + 1, jump_offset);
#endif
}

static HCL_INLINE void patch_long_param (hcl_t* hcl, hcl_ooi_t ip, hcl_oow_t param)
{
#if (HCL_HCL_CODE_LONG_PARAM_SIZE == 2)
	patch_instruction (hcl, ip, param >> 8);
	patch_instruction (hcl, ip + 1, param & 0xFF);
#else
	patch_instruction (hcl, ip, param);
#endif
}

static int emit_indexed_variable_access (hcl_t* hcl, hcl_oow_t index, hcl_oob_t baseinst1, hcl_oob_t baseinst2)
{
	if (hcl->c->blk.depth >= 0)
	{
		hcl_oow_t i;

		/* if a temporary variable is accessed inside a block,
		 * use a special instruction to indicate it */
		HCL_ASSERT (hcl, index < hcl->c->blk.info[hcl->c->blk.depth].tmprcnt);
		for (i = hcl->c->blk.depth; i > 0; i--) /* excluded the top level -- TODO: change this code depending on global variable handling */
		{
			if (index >= hcl->c->blk.info[i - 1].tmprcnt)
			{
				hcl_oow_t ctx_offset, index_in_ctx;
				ctx_offset = hcl->c->blk.depth - i;
				index_in_ctx = index - hcl->c->blk.info[i - 1].tmprcnt;
				/* ctx_offset 0 means the current context.
				 *            1 means current->home.
				 *            2 means current->home->home. 
				 * index_in_ctx is a relative index within the context found.
				 */
				if (emit_double_param_instruction(hcl, baseinst1, ctx_offset, index_in_ctx) <= -1) return -1;
				return 0;
			}
		}
	}

	/* TODO: top-level... verify this. this will vary depending on how i implement the top-level and global variables... */
	if (emit_single_param_instruction (hcl, baseinst2, index) <= -1) return -1;
	return 0;
}

/* ========================================================================= */
static HCL_INLINE int _insert_cframe (hcl_t* hcl, hcl_ooi_t index, int opcode, hcl_cnode_t* operand)
{
	hcl_cframe2_t* tmp;

	HCL_ASSERT (hcl, index >= 0);
	
	hcl->c->cfs2.top++;
	HCL_ASSERT (hcl, hcl->c->cfs2.top >= 0);
	HCL_ASSERT (hcl, index <= hcl->c->cfs2.top);

	if ((hcl_oow_t)hcl->c->cfs2.top >= hcl->c->cfs2.capa)
	{
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN (hcl->c->cfs2.top + 256, 256); /* TODO: adjust this capacity */
		tmp = (hcl_cframe2_t*)hcl_reallocmem (hcl, hcl->c->cfs2.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (HCL_UNLIKELY(!tmp)) 
		{
			hcl->c->cfs2.top--;
			return -1;
		}

		hcl->c->cfs2.capa = newcapa;
		hcl->c->cfs2.ptr = tmp;
	}

	if (index < hcl->c->cfs2.top)
	{
		HCL_MEMMOVE (&hcl->c->cfs2.ptr[index + 1], &hcl->c->cfs2.ptr[index], (hcl->c->cfs2.top - index) * HCL_SIZEOF(*tmp));
	}

	tmp = &hcl->c->cfs2.ptr[index];
	tmp->opcode = opcode;
	tmp->operand = operand;
	/* leave tmp->u untouched/uninitialized */
	return 0;
}

static int insert_cframe (hcl_t* hcl, hcl_ooi_t index, int opcode, hcl_cnode_t* operand)
{
	if (hcl->c->cfs2.top == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl_seterrnum (hcl, HCL_EFRMFLOOD);
		return -1;
	}

	return _insert_cframe(hcl, index, opcode, operand);
}

static int push_cframe (hcl_t* hcl, int opcode, hcl_cnode_t* operand)
{
	if (hcl->c->cfs2.top == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl_seterrnum (hcl, HCL_EFRMFLOOD);
		return -1;
	}

	return _insert_cframe(hcl, hcl->c->cfs2.top + 1, opcode, operand);
}

static HCL_INLINE void pop_cframe (hcl_t* hcl)
{
	HCL_ASSERT (hcl, hcl->c->cfs2.top >= 0);
	hcl->c->cfs2.top--;
}

#define PUSH_CFRAME(hcl,opcode,operand) \
	do { if (push_cframe(hcl,opcode,operand) <= -1) return -1; } while(0)

#define INSERT_CFRAME(hcl,index,opcode,operand) \
	do { if (insert_cframe(hcl,index,opcode,operand) <= -1) return -1; } while(0)

#define POP_CFRAME(hcl) pop_cframe(hcl)

#define POP_ALL_CFRAMES(hcl) (hcl->c->cfs2.top = -1)

#define GET_TOP_CFRAME_INDEX(hcl) (hcl->c->cfs2.top)

#define GET_TOP_CFRAME(hcl) (&hcl->c->cfs2.ptr[hcl->c->cfs2.top])

#define GET_CFRAME(hcl,index) (&hcl->c->cfs2.ptr[index])

#define SWITCH_TOP_CFRAME(hcl,_opcode,_operand) \
	do { \
		hcl_cframe2_t* _cf = GET_TOP_CFRAME(hcl); \
		_cf->opcode = _opcode; \
		_cf->operand = _operand; \
	} while (0);

#define SWITCH_CFRAME(hcl,_index,_opcode,_operand) \
	do { \
		hcl_cframe2_t* _cf = GET_CFRAME(hcl,_index); \
		_cf->opcode = _opcode; \
		_cf->operand = _operand; \
	} while (0);

static int push_subcframe (hcl_t* hcl, int opcode, hcl_cnode_t* operand)
{
	hcl_cframe2_t* cf, tmp;

	cf = GET_TOP_CFRAME(hcl);
	tmp = *cf;
	cf->opcode = opcode;
	cf->operand = operand;

	return push_cframe(hcl, tmp.opcode, tmp.operand);
}

static HCL_INLINE hcl_cframe2_t* find_cframe_from_top (hcl_t* hcl, int opcode)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t i;

	for (i = hcl->c->cfs2.top; i >= 0; i--)
	{
		cf = &hcl->c->cfs2.ptr[i];
		if (cf->opcode == opcode) return cf;
	}

	return HCL_NULL;
}

#define PUSH_SUBCFRAME(hcl,opcode,operand) \
	do { if (push_subcframe(hcl,opcode,operand) <= -1) return -1; } while(0)

#define GET_SUBCFRAME(hcl) (&hcl->c->cfs2.ptr[hcl->c->cfs2.top - 1])

enum 
{
	COP_COMPILE_OBJECT,

	COP_COMPILE_OBJECT_LIST,
	COP_COMPILE_IF_OBJECT_LIST,
	COP_COMPILE_ARGUMENT_LIST,
	COP_COMPILE_OBJECT_LIST_TAIL,
	COP_COMPILE_IF_OBJECT_LIST_TAIL,

	COP_COMPILE_ARRAY_LIST,
	COP_COMPILE_BYTEARRAY_LIST,
	COP_COMPILE_DIC_LIST,
	COP_COMPILE_QLIST, /* compile data list */

	COP_SUBCOMPILE_ELIF,
	COP_SUBCOMPILE_ELSE,

	COP_EMIT_CALL,

	COP_EMIT_MAKE_ARRAY,
	COP_EMIT_MAKE_BYTEARRAY,
	COP_EMIT_MAKE_DIC,
	COP_EMIT_MAKE_CONS,
	COP_EMIT_POP_INTO_ARRAY,
	COP_EMIT_POP_INTO_BYTEARRAY,
	COP_EMIT_POP_INTO_DIC,
	COP_EMIT_POP_INTO_CONS,
	COP_EMIT_POP_INTO_CONS_END,
	COP_EMIT_POP_INTO_CONS_CDR,

	COP_EMIT_LAMBDA,
	COP_EMIT_POP_STACKTOP,
	COP_EMIT_RETURN,
	COP_EMIT_SET,

	COP_SUBCOMPILE_AND_EXPR,
	COP_SUBCOMPILE_OR_EXPR,
	
	COP_POST_AND_EXPR,
	COP_POST_OR_EXPR,

	COP_POST_IF_COND,
	COP_POST_IF_BODY,

	COP_POST_UNTIL_BODY,
	COP_POST_UNTIL_COND,
	COP_POST_WHILE_BODY,
	COP_POST_WHILE_COND,

	COP_UPDATE_BREAK
};

/* ========================================================================= */

static int compile_and (hcl_t* hcl, hcl_cnode_t* src)
{
	hcl_cnode_t* obj, * expr;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_AND));

	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no expression specified in and");
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in and");
		return -1;
	}

/* TODO: optimization - eat away all true expressions */
	expr = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CNODE_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, expr); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_SUBCOMPILE_AND_EXPR, obj); /* 2 */

	return 0;
}

static int compile_or (hcl_t* hcl, hcl_cnode_t* src)
{
	hcl_cnode_t* obj, * expr;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_OR));

	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no expression specified in or");
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in and");
		return -1;
	}

/* TODO: optimization - eat away all false expressions */
	expr = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CNODE_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, expr); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_SUBCOMPILE_OR_EXPR, obj); /* 2 */

	return 0;
}

static int compile_break (hcl_t* hcl, hcl_cnode_t* src)
{
	/* (break) */
	hcl_cnode_t* obj;
	hcl_ooi_t i;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_BREAK));

	obj = HCL_CNODE_CONS_CDR(src);
	if (obj)
	{
		if (HCL_CNODE_IS_CONS(obj))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(obj), HCL_NULL, "redundant argument in break");
		}
		else
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in break");
		}
		return -1;
	}

	for (i = hcl->c->cfs2.top; i >= 0; --i)
	{
		const hcl_cframe2_t* tcf;
		tcf = &hcl->c->cfs2.ptr[i];

		if (tcf->opcode == COP_EMIT_LAMBDA) break; /* seems to cross lambda boundary */

		if (tcf->opcode == COP_POST_UNTIL_BODY || tcf->opcode == COP_POST_WHILE_BODY)
		{
			hcl_ooi_t jump_inst_pos;

			/* (break) is not really a function call. but to make it look like a
			 * function call, i generate PUSH_NIL so nil becomes a return value.
			 *     (set x (until #f (break)))
			 * x will get nill. */
			if (emit_byte_instruction (hcl, HCL_CODE_PUSH_NIL, HCL_NULL) <= -1) return -1;

/* TODO: study if supporting expression after break is good like return. (break (+ 10 20)) */
			HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
			jump_inst_pos = hcl->code.bc.len;
			
			if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;
			INSERT_CFRAME (hcl, i, COP_UPDATE_BREAK, HCL_SMOOI_TO_OOP(jump_inst_pos));

			POP_CFRAME (hcl);
			return 0;
		}
	}

	hcl_setsynerrbfmt (hcl, HCL_SYNERR_BREAK, HCL_CNODE_GET_LOC(src), HCL_NULL, "break outside loop");
	return -1;
}

static int compile_if (hcl_t* hcl, hcl_cnode_t* src)
{
	hcl_cnode_t* cmd, * obj, * cond;
	hcl_cframe2_t* cf;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_IF));

	/* (if (< 20 30) 
	 *   (perform this)
	 *   (perform that)
	 * elif (< 20 30)
	 *   (perform it)
	 * else
	 *   (perform this finally)
	 * )
	 */
	cmd = HCL_CNODE_CONS_CAR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no condition specified in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	cond = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CNODE_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_POST_IF_COND, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_if.body_pos = -1; /* unknown yet */
	cf->u.post_if.start_loc = *HCL_CNODE_GET_LOC(src);
/* TODO: OPTIMIZATION:
 *       pass information on the conditional if it's an absoluate true or absolute false to
 *       eliminate some code .. i can't eliminate code because there can be else or elif... 
 *       if absoluate true, don't need else or other elif part
 *       if absoluate false, else or other elif part is needed.
 */
	return 0;
}

static int compile_lambda (hcl_t* hcl, hcl_cnode_t* src, int defun)
{
	hcl_cnode_t* cmd, * obj, * args;
	hcl_oow_t nargs, ntmprs;
	hcl_ooi_t jump_inst_pos, lfbase_pos, lfsize_pos;
	hcl_oow_t saved_tv_wcount, tv_dup_start;
	hcl_cnode_t* defun_name;
	hcl_cframe2_t* cf;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));

	saved_tv_wcount = hcl->c->tv2.wcount; 
	cmd = HCL_CNODE_CONS_CAR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (defun)
	{
		HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(cmd, HCL_SYNCODE_DEFUN));

		if (!obj)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMELIST, HCL_CNODE_GET_LOC(src), HCL_NULL, "no name in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
			return -1;
		}
		else if (!HCL_CNODE_IS_CONS(obj))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
			return -1;
		}

		defun_name = HCL_CNODE_CONS_CAR(obj);
		if (!HCL_CNODE_IS_SYMBOL(defun_name))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_CNODE_GET_LOC(defun_name), HCL_CNODE_GET_TOK(defun_name), "name not a symbol in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
			return -1;
		}

		if (HCL_CNODE_SYMBOL_SYNCODE(defun_name)) /*|| HCL_OBJ_GET_FLAGS_KERNEL(defun_name) >= 1) */
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_CNODE_GET_LOC(defun_name), HCL_CNODE_GET_TOK(defun_name), "special symbol not to be used as a defun name");
			return -1;
		}

		obj = HCL_CNODE_CONS_CDR(obj);
	}
	else
	{
		HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(cmd, HCL_SYNCODE_LAMBDA));
	}

	if (!obj)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMELIST, HCL_CNODE_GET_LOC(src), HCL_NULL, "no argument list in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in argument list in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	args = HCL_CNODE_CONS_CAR(obj);
	if (!args)
	{
		/* no argument - (lambda () (+ 10 20)) */
		nargs = 0;
	}
	else
	{
		hcl_cnode_t* arg, * dcl;

		if (!HCL_CNODE_IS_CONS(args))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMELIST, HCL_CNODE_GET_LOC(args), HCL_CNODE_GET_TOK(args), "not an argument list in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
			return -1;
		}

		tv_dup_start = hcl->c->tv2.s.len;
		nargs = 0;
		dcl = args;
		do
		{
			arg = HCL_CNODE_CONS_CAR(dcl);
			if (!HCL_CNODE_IS_SYMBOL(arg))
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAME, HCL_CNODE_GET_LOC(arg), HCL_CNODE_GET_TOK(arg), "argument not a symbol in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
				return -1;
			}

			if (HCL_CNODE_IS_SYMBOL(arg) && HCL_CNODE_SYMBOL_SYNCODE(arg) /* || HCL_OBJ_GET_FLAGS_KERNEL(arg) >= 2 */)
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDARGNAME, HCL_CNODE_GET_LOC(arg), HCL_CNODE_GET_TOK(arg), "special symbol not to be declared as an argument in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
				return -1;
			}

			if (add_temporary_variable(hcl, HCL_CNODE_GET_TOK(arg), tv_dup_start) <= -1) 
			{
				if (hcl->errnum == HCL_EEXIST)
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMEDUP, HCL_CNODE_GET_LOC(arg), HCL_CNODE_GET_TOK(arg), "argument duplicate in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
				}
				return -1;
			}
			nargs++;

			dcl = HCL_CNODE_CONS_CDR(dcl);
			if (!dcl) break;

			if (!HCL_CNODE_IS_CONS(dcl)) 
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(dcl), HCL_CNODE_GET_TOK(dcl), "redundant cdr in argument list in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
				return -1;
			}
		}
		while (1);
	}

	HCL_ASSERT (hcl, nargs == hcl->c->tv2.wcount - saved_tv_wcount);
	if (nargs > MAX_CODE_NBLKARGS) /*TODO: change this limit to max call argument count */
	{
		/* while an integer object is pused to indicate the number of
		 * block arguments, evaluation which is done by message passing
		 * limits the number of arguments that can be passed. so the
		 * check is implemented */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_CNODE_GET_LOC(args), HCL_NULL, "too many(%zu) arguments in %.*js", nargs, HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd)); 
		return -1;
	}

	ntmprs = nargs;  
	obj = HCL_CNODE_CONS_CDR(obj);

	tv_dup_start = hcl->c->tv2.s.len;
	while (obj && HCL_CNODE_IS_CONS(obj))
	{
		hcl_cnode_t* dcl;

		dcl = HCL_CNODE_CONS_CAR(obj);
		if (HCL_CNODE_IS_CONS_CONCODED(dcl, HCL_CONCODE_VLIST))
		{
			hcl_cnode_t* var;
			do
			{
				var = HCL_CNODE_CONS_CAR(dcl);
			#if 0
				if (!HCL_CNODE_IS_SYMBOL(var))
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAME, HCL_CNODE_GET_LOC(var), HCL_CNODE_GET_TOK(var), "local variable not a symbol");
					return -1;
				}

				if (HCL_CNODE_IS_SYMBOL(var) && HCL_CNODE_SYMBOL_SYNCODE(var) /* || HCL_OBJ_GET_FLAGS_KERNEL(var) >= 2 */)
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDARGNAME, HCL_CNODE_GET_LOC(var), HCL_CNODE_GET_TOK(var), "special symbol not to be declared as a local variable");
					return -1;
				}
			#else
				/* the above checks are not needed as the reader guarantees the followings */
				HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL(var) && !HCL_CNODE_SYMBOL_SYNCODE(var));
			#endif

				if (add_temporary_variable(hcl, HCL_CNODE_GET_TOK(var), tv_dup_start) <= -1) 
				{
					if (hcl->errnum == HCL_EEXIST)
					{
						hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMEDUP, HCL_CNODE_GET_LOC(var), HCL_CNODE_GET_TOK(var), "duplicate local variable");
					}
					return -1;
				}
				ntmprs++;

				dcl = HCL_CNODE_CONS_CDR(dcl);
				if (!dcl) break;

				if (!HCL_CNODE_IS_CONS(dcl)) 
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(dcl), HCL_CNODE_GET_TOK(dcl), "redundant cdr in local variable list");
					return -1;
				}
			}
			while (1);

			obj = HCL_CNODE_CONS_CDR(obj);
		}
		else break;
	}

	/* ntmprs: number of temporary variables including arguments */
	HCL_ASSERT (hcl, ntmprs == hcl->c->tv2.wcount - saved_tv_wcount);
	if (ntmprs > MAX_CODE_NBLKTMPRS)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARFLOOD, HCL_CNODE_GET_LOC(args), HCL_NULL, "too many(%zu) variables in %.*js", ntmprs, HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd)); 
		return -1;
	}

	if (hcl->c->blk.depth == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BLKDEPTH, HCL_CNODE_GET_LOC(src), HCL_NULL, "lambda block depth too deep in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd)); 
		return -1;
	}
	hcl->c->blk.depth++;
	if (store_temporary_variable_count_for_block(hcl, hcl->c->tv2.wcount, hcl->c->tv2.s.len, hcl->code.lit.len) <= -1) return -1;


	if (hcl->option.trait & HCL_TRAIT_INTERACTIVE)
	{
		/* make_function nargs ntmprs lfbase lfsize */
		if (emit_double_param_instruction(hcl, HCL_CODE_MAKE_FUNCTION, nargs, ntmprs) <= -1) return -1;
		lfbase_pos = hcl->code.bc.len;
		if (emit_long_param(hcl, hcl->code.lit.len - hcl->c->blk.info[hcl->c->blk.depth - 1].lfbase) <= -1) return -1; /* literal frame base */
		lfsize_pos = hcl->code.bc.len; /* literal frame size */
		if (emit_long_param(hcl, 0) <= -1) return -1;
	}
	else
	{
		if (emit_double_param_instruction(hcl, HCL_CODE_MAKE_BLOCK, nargs, ntmprs) <= -1) return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);  /* guaranteed in emit_byte_instruction() */
	jump_inst_pos = hcl->code.bc.len;
	/* specifying MAX_CODE_JUMP causes emit_single_param_instruction() to 
	 * produce the long jump instruction (HCL_CODE_JUMP_FORWARD_X) */
	if (emit_single_param_instruction(hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj);

	if (defun)
	{
		hcl_oow_t index;
		hcl_cframe2_t* cf;

		if (find_temporary_variable_backward(hcl, HCL_CNODE_GET_TOK(defun_name), &index) <= -1)
		{
			PUSH_SUBCFRAME (hcl, COP_EMIT_SET, defun_name); /* set doesn't evaluate the variable name */
			cf = GET_SUBCFRAME(hcl);
			cf->u.set.var_type = VAR_NAMED;
		}
		else
		{
			/* the check in compile_lambda() must ensure this condition */
			HCL_ASSERT (hcl, index <= HCL_SMOOI_MAX); 

			PUSH_SUBCFRAME (hcl, COP_EMIT_SET, HCL_SMOOI_TO_OOP(index)); 
			cf = GET_SUBCFRAME(hcl);
			cf->u.set.var_type = VAR_INDEXED;
		}
	}

	PUSH_SUBCFRAME (hcl, COP_EMIT_LAMBDA, HCL_SMOOI_TO_OOP(jump_inst_pos));

	cf = GET_SUBCFRAME (hcl);
	cf->u.lambda.start_loc = *HCL_CNODE_GET_LOC(src);

	if (hcl->option.trait & HCL_TRAIT_INTERACTIVE)
	{
		cf->u.lambda.lfbase_pos = lfbase_pos;
		cf->u.lambda.lfsize_pos = lfsize_pos;
	}

	return 0;
}

static int compile_return (hcl_t* hcl, hcl_cnode_t* src, int mode)
{
	hcl_cnode_t* obj, * val;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_RETURN) || 
	                 HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_RETURN_FROM_HOME));

	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
/* TODO: should i allow (return)? does it return the last value on the stack? */
		/* no value */
		hcl_cnode_t* tmp = HCL_CNODE_CONS_CAR(src);
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no value specified in %.*js", HCL_CNODE_GET_TOKLEN(tmp), HCL_CNODE_GET_TOKPTR(tmp));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_cnode_t* tmp = HCL_CNODE_CONS_CAR(src);
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(tmp), HCL_CNODE_GET_TOKPTR(tmp));
		return -1;
	}

	val = HCL_CNODE_CONS_CAR(obj);

	obj = HCL_CNODE_CONS_CDR(obj);
	if (obj)
	{
		hcl_cnode_t* tmp = HCL_CNODE_CONS_CAR(src);
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "more than 1 argument in %.*js", HCL_CNODE_GET_TOKLEN(tmp), HCL_CNODE_GET_TOKPTR(tmp));
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, val);

	PUSH_SUBCFRAME (hcl, COP_EMIT_RETURN, HCL_SMOOI_TO_OOP(mode));

	return 0;
}

static int compile_set (hcl_t* hcl, hcl_cnode_t* src)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* cmd, * obj, * var, * val;
	hcl_oow_t index;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_SET));

	cmd = HCL_CNODE_CONS_CAR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_CNODE_GET_LOC(src), HCL_NULL, "no variable name in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	var = HCL_CNODE_CONS_CAR(obj);
	if (!HCL_CNODE_IS_SYMBOL(var))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_CNODE_GET_LOC(var), HCL_CNODE_GET_TOK(var), "variable name not a symbol in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	if (HCL_CNODE_SYMBOL_SYNCODE(var)/* || HCL_OBJ_GET_FLAGS_KERNEL(var) >= 2*/)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_CNODE_GET_LOC(var), HCL_CNODE_GET_TOK(var), "special symbol not to be used as a variable name in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	obj = HCL_CNODE_CONS_CDR(obj);
	if (!obj)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no value specified in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	val = HCL_CNODE_CONS_CAR(obj);

	obj = HCL_CNODE_CONS_CDR(obj);
	if (obj)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "too many arguments to %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, val);

	if (find_temporary_variable_backward(hcl, HCL_CNODE_GET_TOK(var), &index) <= -1)
	{
		PUSH_SUBCFRAME (hcl, COP_EMIT_SET, var); /* set doesn't evaluate the variable name */
		cf = GET_SUBCFRAME(hcl);
		cf->u.set.var_type = VAR_NAMED;
	}
	else
	{
		/* the check in compile_lambda() must ensure this condition */
		HCL_ASSERT (hcl, index <= HCL_SMOOI_MAX); 

		PUSH_SUBCFRAME (hcl, COP_EMIT_SET, HCL_SMOOI_TO_OOP(index)); 
		cf = GET_SUBCFRAME(hcl);
		cf->u.set.var_type = VAR_INDEXED;
	}

	return 0;
}


static int compile_do (hcl_t* hcl, hcl_cnode_t* src)
{
	hcl_cnode_t* cmd, * obj;

	/* (do 
	 *   (+ 10 20)
	 *   (* 2 30)
	 *  ...
	 * ) 
	 * you can use this to combine multiple expressions to a single expression
	 */

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_DO));

	cmd = HCL_CNODE_CONS_CDR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no expression specified in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd)); 
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj); 
	return 0;
}

static int compile_while (hcl_t* hcl, hcl_cnode_t* src, int next_cop)
{
	/* (while (xxxx) ... ) 
	 * (until (xxxx) ... ) */
	hcl_cnode_t* cmd, * obj, * cond;
	hcl_oow_t cond_pos;
	hcl_cframe2_t* cf;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_UNTIL) ||
	                 HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_WHILE));
	HCL_ASSERT (hcl, next_cop == COP_POST_UNTIL_COND || next_cop == COP_POST_WHILE_COND);

	cmd = HCL_CNODE_CONS_CAR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no loop condition specified in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %*.js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	cond_pos = hcl->code.bc.len; /* position where the bytecode for the conditional is emitted */

	cond = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CNODE_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, next_cop, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_while.cond_pos = cond_pos;
	cf->u.post_while.body_pos = -1; /* unknown yet*/
	cf->u.post_while.start_loc = *HCL_CNODE_GET_LOC(src);

	return 0;
}
/* ========================================================================= */

static int compile_cons_array_expression (hcl_t* hcl, hcl_cnode_t* obj)
{
	/* [ ] */
	hcl_ooi_t nargs;
	hcl_cframe2_t* cf;

	/* NOTE: cframe management functions don't use the object memory.
	 *       many operations can be performed without taking GC into account */
	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_ARRAY, HCL_SMOOI_TO_OOP(0));

	nargs = hcl_countcnodecons(hcl, obj);
	if (nargs > MAX_CODE_PARAM) 
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_CNODE_GET_LOC(obj), HCL_NULL, "too many(%zd) elements in array", nargs); 
		return -1;
	}

	/* redundant cdr check is performed inside compile_object_list() */
	PUSH_SUBCFRAME (hcl, COP_COMPILE_ARRAY_LIST, obj);
	cf = GET_SUBCFRAME(hcl);
	cf->u.array_list.index = 0;

	/* patch the argument count in the operand field of the COP_EMIT_MAKE_ARRAY frame */
	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_ARRAY);
	cf->operand = HCL_SMOOI_TO_OOP(nargs);

	return 0;
}

static int compile_cons_bytearray_expression (hcl_t* hcl, hcl_cnode_t* obj)
{
	/* #[ ] - e.g. #[1, 2, 3] or #[ 1 2 3 ] */
	hcl_ooi_t nargs;
	hcl_cframe2_t* cf;

	/* NOTE: cframe management functions don't use the object memory.
	 *       many operations can be performed without taking GC into account */
	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_BYTEARRAY, HCL_SMOOI_TO_OOP(0));

	nargs = hcl_countcnodecons(hcl, obj);
	if (nargs > MAX_CODE_PARAM) 
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_CNODE_GET_LOC(obj), HCL_NULL, "too many(%zd) elements in byte-array", nargs); 
		return -1;
	}

	/* redundant cdr check is performed inside compile_object_list() */
	PUSH_SUBCFRAME (hcl, COP_COMPILE_BYTEARRAY_LIST, obj);
	cf = GET_SUBCFRAME(hcl);
	cf->u.bytearray_list.index = 0;

	/* patch the argument count in the operand field of the COP_EMIT_MAKE_BYTEARRAY frame */
	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_BYTEARRAY);
	cf->operand = HCL_SMOOI_TO_OOP(nargs);

	return 0;
}

static int compile_cons_dic_expression (hcl_t* hcl, hcl_cnode_t* obj)
{
	/* { } - e.g. {1:2, 3:4,"abc":def, "hwaddr":"00:00:00:01"} or { 1 2 3 4 } */
	hcl_ooi_t nargs;
	hcl_cframe2_t* cf;

	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_DIC, HCL_SMOOI_TO_OOP(0));

	nargs = hcl_countcnodecons(hcl, obj);
	if (nargs > MAX_CODE_PARAM) 
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_CNODE_GET_LOC(obj), HCL_NULL, "too many(%zd) elements in dictionary", nargs); 
		return -1;
	}

	/* redundant cdr check is performed inside compile_object_list() */
	PUSH_SUBCFRAME (hcl, COP_COMPILE_DIC_LIST, obj);

	/* patch the argument count in the operand field of the COP_EMIT_MAKE_DIC frame */
	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_DIC);
	cf->operand = HCL_SMOOI_TO_OOP(nargs);

	return 0;
}

static int compile_cons_qlist_expression (hcl_t* hcl, hcl_cnode_t* obj)
{
	/* #( 1 2  3 ) 
	 * #(1 (+ 2 3) 5) --> #(1 5 5)
	 * */
	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_CONS, HCL_NULL);
	PUSH_SUBCFRAME (hcl, COP_COMPILE_QLIST, obj);
	return 0;
}

static int compile_cons_xlist_expression (hcl_t* hcl, hcl_cnode_t* obj)
{
	hcl_cnode_t* car;
	int syncode; /* syntax code of the first element */

	/* a valid function call
	 * (function-name argument-list)
	 *   function-name can be:
	 *     a symbol.
	 *     another function call.
	 * if the name is another function call, i can't know if the 
	 * function name will be valid at the compile time.
	 */
	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS_CONCODED(obj, HCL_CONCODE_XLIST));

	car = HCL_CNODE_CONS_CAR(obj);
	if (HCL_CNODE_IS_SYMBOL(car) && (syncode = HCL_CNODE_SYMBOL_SYNCODE(car)))
	{
		switch (syncode)
		{
			case HCL_SYNCODE_AND:
				if (compile_and(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_BREAK:
				/* (break) */
				if (compile_break(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_DEFUN:
				if (compile_lambda(hcl, obj, 1) <= -1) return -1;
				break;

			case HCL_SYNCODE_DO:
				if (compile_do(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_ELSE:
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ELSE, HCL_CNODE_GET_LOC(car), HCL_CNODE_GET_TOK(car), "else without if");
				return -1;

			case HCL_SYNCODE_ELIF:
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ELIF, HCL_CNODE_GET_LOC(car), HCL_CNODE_GET_TOK(car), "elif without if");
				return -1;

			case HCL_SYNCODE_IF:
				if (compile_if(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_LAMBDA:
				/* (lambda (x y) (+ x y)) */
				if (compile_lambda(hcl, obj, 0) <= -1) return -1;
				break;

			case HCL_SYNCODE_OR:
				if (compile_or(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_SET:
				/* (set x 10) 
				 * (set x (lambda (x y) (+ x y)) */
				if (compile_set(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_RETURN:
				/* (return 10)
				 * (return (+ 10 20)) */
				if (compile_return(hcl, obj, 0) <= -1) return -1;
				break;

			case HCL_SYNCODE_RETURN_FROM_HOME:
				if (compile_return(hcl, obj, 1) <= -1) return -1;
				break;

			case HCL_SYNCODE_UNTIL:
				if (compile_while(hcl, obj, COP_POST_UNTIL_COND) <= -1) return -1;
				break;

			case HCL_SYNCODE_WHILE:
				if (compile_while(hcl, obj, COP_POST_WHILE_COND) <= -1) return -1;
				break;

			default:
				HCL_DEBUG3 (hcl, "Internal error - unknown syncode %d at %s:%d\n", syncode, __FILE__, __LINE__);
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_INTERN, HCL_CNODE_GET_LOC(car), HCL_NULL, "internal error - unknown syncode %d", syncode);
				return -1;
		}
	}
	else if (HCL_CNODE_IS_SYMBOL(car)  || HCL_CNODE_IS_DSYMBOL(car) || HCL_CNODE_IS_CONS_CONCODED(car, HCL_CONCODE_XLIST))
	{
		/* normal function call 
		 *  (<operator> <operand1> ...) */
		hcl_ooi_t nargs;
		hcl_ooi_t oldtop;
		hcl_cframe2_t* cf;
		hcl_cnode_t* cdr;
		hcl_cnode_t* sdc;

		/* NOTE: cframe management functions don't use the object memory.
		 *       many operations can be performed without taking GC into account */

		/* store the position of COP_EMIT_CALL to be produced with
		 * SWITCH_TOP_CFRAME() in oldtop for argument count patching 
		 * further down */
		oldtop = GET_TOP_CFRAME_INDEX(hcl); 
		HCL_ASSERT (hcl, oldtop >= 0);

		SWITCH_TOP_CFRAME (hcl, COP_EMIT_CALL, HCL_SMOOI_TO_OOP(0));

		/* compile <operator> */
		PUSH_CFRAME (hcl, COP_COMPILE_OBJECT, car);

		/* compile <operand1> ... etc */
		cdr = HCL_CNODE_CONS_CDR(obj);

		if (!cdr) 
		{
			nargs = 0;
		}
		else
		{
			if (!HCL_CNODE_IS_CONS(cdr))
			{
				/* (funname . 10) */
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(cdr), HCL_CNODE_GET_TOK(cdr), "redundant cdr in function call");
				return -1;
			}

			nargs = hcl_countcnodecons(hcl, cdr);
			if (nargs > MAX_CODE_PARAM) 
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_CNODE_GET_LOC(cdr), HCL_NULL, "too many(%zd) parameters in function call", nargs); 
				return -1;
			}
		}

		if (HCL_CNODE_IS_SYMBOL(car) || HCL_CNODE_IS_DSYMBOL(car))
		{
			/* only symbols are added to the system dictionary. 
			 * perform this lookup only if car is a symbol */
			sdc = hcl_lookupsysdicforsymbol_noseterr(hcl, HCL_CNODE_GET_TOK(car));
			if (sdc)
			{
				hcl_oop_word_t sdv;
				sdv = (hcl_oop_word_t)HCL_CONS_CDR(sdc);
				if (HCL_IS_PRIM(hcl, sdv))
				{
					if (nargs < sdv->slot[1] || nargs > sdv->slot[2])
					{
						hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(car), HCL_NULL, 
							"parameters count(%zd) mismatch in function call - %.*js - expecting %zu-%zu parameters", nargs, HCL_CNODE_GET_TOKLEN(car), HCL_CNODE_GET_TOKPTR(car), sdv->slot[1], sdv->slot[2]); 
						return -1;
					}
				}
			}
		}

		/* redundant cdr check is performed inside compile_object_list() */
		PUSH_SUBCFRAME (hcl, COP_COMPILE_ARGUMENT_LIST, cdr);

		/* patch the argument count in the operand field of the COP_EMIT_CALL frame */
		cf = GET_CFRAME(hcl, oldtop);
		HCL_ASSERT (hcl, cf->opcode == COP_EMIT_CALL);
		cf->operand = HCL_SMOOI_TO_OOP(nargs);
	}
	else
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_CALLABLE, HCL_CNODE_GET_LOC(car), HCL_CNODE_GET_TOK(car), "invalid callable in function call");
		return -1;
	}

	return 0;
}

static HCL_INLINE int compile_symbol (hcl_t* hcl, hcl_cnode_t* obj)
{
	hcl_oow_t index;

	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL(obj));

	if (HCL_CNODE_SYMBOL_SYNCODE(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "special symbol not to be used as a variable name");
		return -1;
	}

	/* check if a symbol is a local variable */
	if (find_temporary_variable_backward(hcl, HCL_CNODE_GET_TOK(obj), &index) <= -1)
	{
		hcl_oop_t sym, cons;
/* TODO: if i require all variables to be declared, this part is not needed and should handle it as an error */
/* TODO: change the scheme... allow declaration??? */
		/* global variable */
		sym = hcl_makesymbol(hcl, HCL_CNODE_GET_TOKPTR(obj), HCL_CNODE_GET_TOKLEN(obj));
		if (HCL_UNLIKELY(!sym)) return -1;

		cons = (hcl_oop_t)hcl_getatsysdic(hcl, sym);
		if (!cons) 
		{
			cons = (hcl_oop_t)hcl_putatsysdic(hcl, sym, hcl->_nil);
			if (HCL_UNLIKELY(!cons)) return -1;
		}

		/* add the entire cons pair to the literal frame */

		if (add_literal(hcl, cons, &index) <= -1 ||
		    emit_single_param_instruction(hcl, HCL_CODE_PUSH_OBJECT_0, index) <= -1) return -1;

		return 0;
	}
	else
	{
		return emit_indexed_variable_access(hcl, index, HCL_CODE_PUSH_CTXTEMPVAR_0, HCL_CODE_PUSH_TEMPVAR_0);
	}
}

static HCL_INLINE int compile_dsymbol (hcl_t* hcl, hcl_cnode_t* obj)
{
	hcl_oop_t sym, cons;
	hcl_oow_t index;

/* TODO: need a total revamp on the dotted symbols.
 *       must differentiate module access and dictioary member access...
 *       must implementate dictionary member access syntax... */

	sym = hcl_makesymbol(hcl, HCL_CNODE_GET_TOKPTR(obj), HCL_CNODE_GET_TOKLEN(obj));
	if (HCL_UNLIKELY(!sym)) return -1;
	cons = (hcl_oop_t)hcl_getatsysdic(hcl, sym);
	if (!cons)
	{
		/* query the module for information if it is the first time
		 * when the dotted symbol is seen */

		hcl_pfbase_t* pfbase;
		hcl_mod_t* mod;
		hcl_oop_t val;
		unsigned int kernel_bits;

		pfbase = hcl_querymod(hcl, HCL_CNODE_GET_TOKPTR(obj), HCL_CNODE_GET_TOKLEN(obj), &mod);
		if (!pfbase)
		{
			/* TODO switch to syntax error */
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "unknown dotted symbol");
			return -1;
		}

		hcl_pushvolat (hcl, &sym);
		switch (pfbase->type)
		{
			case HCL_PFBASE_FUNC:
				kernel_bits = 2;
				val = hcl_makeprim(hcl, pfbase->handler, pfbase->minargs, pfbase->maxargs, mod);
				break;

			case HCL_PFBASE_VAR:
				kernel_bits = 1;
				val = hcl->_nil;
				break;

			case HCL_PFBASE_CONST:
				/* TODO: create a value from the pfbase information. it needs to get extended first
				 * can i make use of pfbase->handler type-cast to a differnt type? */
				kernel_bits = 2;
				val = hcl->_nil;
				break;

			default:
				hcl_popvolat (hcl);
				hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid pfbase type - %d\n", pfbase->type);
				return -1;
		}

		if (!val || !(cons = (hcl_oop_t)hcl_putatsysdic(hcl, sym, val)))
		{
			hcl_popvolat (hcl);
			return -1;
		}
		hcl_popvolat (hcl);

		/* make this dotted symbol special that it can't get changed
		 * to a different value */
		HCL_OBJ_SET_FLAGS_KERNEL (sym, kernel_bits);
	}

	if (add_literal(hcl, cons, &index) <= -1 ||
	    emit_single_param_instruction(hcl, HCL_CODE_PUSH_OBJECT_0, index) <= -1) return -1;

	return 0;
}

static hcl_oop_t string_to_num (hcl_t* hcl, hcl_oocs_t* str, const hcl_ioloc_t* loc, int radixed)
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
		/* 16r1234, 2r1111 */
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
		/* #xFF80, #b1111 */
		HCL_ASSERT (hcl, ptr < end);

		if (*ptr != '#')
		{
			hcl_setsynerrbfmt(hcl, HCL_SYNERR_RADIX, loc, str, "radixed number not starting with #");
			return HCL_NULL;
		}
		ptr++; /* skip '#' */

		if (*ptr == 'x') base = 16;
		else if (*ptr == 'o') base = 8;
		else if (*ptr == 'b') base = 2;
		else
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_RADIX, loc, str, "invalid radix specifier %c", *ptr);
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
	if (HCL_UNLIKELY(!v)) return HCL_NULL;

	return hcl_makefpdec(hcl, v, scale);
}

static int compile_object (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* oprnd;
	hcl_oop_t lit;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_OBJECT);

	oprnd = cf->operand;
redo:
	switch (HCL_CNODE_GET_TYPE(oprnd))
	{
		case HCL_CNODE_NIL:
			if (emit_byte_instruction(hcl, HCL_CODE_PUSH_NIL, HCL_NULL) <= -1) return -1;
			goto done;

		case HCL_CNODE_TRUE:
			if (emit_byte_instruction(hcl, HCL_CODE_PUSH_TRUE, HCL_NULL) <= -1) return -1;
			goto done;

		case HCL_CNODE_FALSE:
			if (emit_byte_instruction(hcl, HCL_CODE_PUSH_FALSE, HCL_NULL) <= -1) return -1;
			goto done;

		case HCL_CNODE_CHARLIT:
			lit = HCL_CHAR_TO_OOP(oprnd->u.charlit.v);
			goto literal;

		case HCL_CNODE_STRLIT:
			lit = hcl_makestring(hcl, HCL_CNODE_GET_TOKPTR(oprnd), HCL_CNODE_GET_TOKLEN(oprnd), 0);
			if (HCL_UNLIKELY(!lit)) return -1;
			goto literal;

		case HCL_CNODE_NUMLIT:
			lit = string_to_num(hcl, HCL_CNODE_GET_TOK(oprnd), HCL_CNODE_GET_LOC(oprnd), 0);
			if (HCL_UNLIKELY(!lit)) return -1;
			goto literal;

		case HCL_CNODE_RADNUMLIT:
			lit = string_to_num(hcl, HCL_CNODE_GET_TOK(oprnd), HCL_CNODE_GET_LOC(oprnd), 1);
			if (HCL_UNLIKELY(!lit)) return -1;
			goto literal;

		case HCL_CNODE_FPDECLIT:
			lit = string_to_fpdec(hcl, HCL_CNODE_GET_TOK(oprnd), HCL_CNODE_GET_LOC(oprnd));
			if (HCL_UNLIKELY(!lit)) return -1;
			goto literal;

		case HCL_CNODE_SMPTRLIT:
			lit = HCL_SMPTR_TO_OOP(oprnd->u.smptrlit.v);
			goto literal;

		case HCL_CNODE_ERRLIT:
			lit = HCL_ERROR_TO_OOP(oprnd->u.errlit.v);
			goto literal;

		case HCL_CNODE_SYMBOL:
			if (compile_symbol(hcl, oprnd) <= -1) return -1;
			goto done;

		case  HCL_CNODE_DSYMBOL:
			if (compile_dsymbol(hcl, oprnd) <= -1) return -1;
			goto done;

		case HCL_CNODE_CONS:
		{
			switch (HCL_CNODE_CONS_CONCODE(oprnd))
			{
				case HCL_CONCODE_XLIST:
					if (compile_cons_xlist_expression(hcl, oprnd) <= -1) return -1;
					break;
				case HCL_CONCODE_ARRAY:
					if (compile_cons_array_expression(hcl, oprnd) <= -1) return -1;
					break;
				case HCL_CONCODE_BYTEARRAY:
					if (compile_cons_bytearray_expression(hcl, oprnd) <= -1) return -1;
					break;
				case HCL_CONCODE_DIC:
					if (compile_cons_dic_expression(hcl, oprnd) <= -1) return -1;
					break;
				case HCL_CONCODE_QLIST:
				#if 1
					if (compile_cons_qlist_expression(hcl, oprnd) <= -1) return -1;
					break;
				#else
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_INTERN, HCL_CNODE_GET_LOC(oprnd), HCL_NULL, "internal error - qlist not implemented");
					return -1;
				#endif
				case HCL_CONCODE_VLIST:
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARDCLBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_NULL, "variable declaration disallowed");
					return -1;

				default:
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_INTERN, HCL_CNODE_GET_LOC(oprnd), HCL_NULL, "internal error - unknown cons type %d", HCL_CNODE_CONS_CONCODE(oprnd));
					return -1;
			}

			break;
		}

		case HCL_CNODE_ELIST:
		{
			/* empty list */
			switch (HCL_CNODE_ELIST_CONCODE(oprnd))
			{
				case HCL_CONCODE_XLIST:
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARDCLBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_NULL, "empty executable list");
					return -1;

				case HCL_CONCODE_ARRAY:
					if (emit_single_param_instruction(hcl, HCL_CODE_MAKE_ARRAY, 0) <= -1) return -1;
					goto done;

				case HCL_CONCODE_BYTEARRAY:
					if (emit_single_param_instruction(hcl, HCL_CODE_MAKE_BYTEARRAY, 0) <= -1) return -1;
					goto done;

				case HCL_CONCODE_DIC:
					if (emit_single_param_instruction(hcl, HCL_CODE_MAKE_DIC, 16) <= -1) return -1;
					goto done;

				case HCL_CONCODE_QLIST:
					if (emit_byte_instruction(hcl, HCL_CODE_PUSH_NIL, HCL_NULL) <= -1) return -1;
					goto done;

				case HCL_CONCODE_VLIST:
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARDCLBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_NULL, "variable declaration disallowed");
					return -1;

				default:
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_INTERN, HCL_CNODE_GET_LOC(oprnd), HCL_NULL, "internal error - unknown list type %d", HCL_CNODE_CONS_CONCODE(oprnd));
					return -1;
			}

			break;
		}

		case HCL_CNODE_SHELL:
			/* a shell node is just a wrapper of an actual node */
			oprnd = oprnd->u.shell.obj;
			goto redo;

		default:
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_INTERN, HCL_CNODE_GET_LOC(oprnd), HCL_CNODE_GET_TOK(oprnd), "internal error - unexpected object type %d", HCL_CNODE_GET_TYPE(oprnd));
			return -1;
	}

	return 0;

literal:
	if (emit_push_literal(hcl, lit) <= -1) return -1;

done:
	POP_CFRAME (hcl);
	return 0;
}

static int compile_object_list (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* oprnd;
	int cop;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_OBJECT_LIST ||
	                 cf->opcode == COP_COMPILE_IF_OBJECT_LIST ||
	                 cf->opcode == COP_COMPILE_ARGUMENT_LIST ||
	                 cf->opcode == COP_COMPILE_IF_OBJECT_LIST_TAIL ||
	                 cf->opcode == COP_COMPILE_OBJECT_LIST_TAIL);

	cop = cf->opcode;
	oprnd = cf->operand;

	if (!oprnd)
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_cnode_t* car, * cdr;

		if (cop != COP_COMPILE_ARGUMENT_LIST)
		{
			/* eliminate unnecessary non-function calls. keep the last one */
			while (HCL_CNODE_IS_CONS(oprnd))
			{
				cdr = HCL_CNODE_CONS_CDR(oprnd);
				if (!cdr) break; /* keep the last one */

				if (HCL_CNODE_IS_CONS(cdr))
				{
					/* look ahead */
					/* keep the last one before elif or else... */
					car = HCL_CNODE_CONS_CAR(cdr);
					if (HCL_CNODE_IS_SYMBOL(car) && HCL_CNODE_SYMBOL_SYNCODE(car)) break;
				}

				car = HCL_CNODE_CONS_CAR(oprnd);
				if (HCL_CNODE_IS_CONS(car) || (HCL_CNODE_IS_SYMBOL(car) && HCL_CNODE_SYMBOL_SYNCODE(car))) break;
				oprnd = cdr;
			}

			HCL_ASSERT (hcl, oprnd != HCL_NULL);
		}

		if (!HCL_CNODE_IS_CONS(oprnd))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_CNODE_GET_TOK(oprnd), "redundant cdr in the object list");
			return -1;
		}

		car = HCL_CNODE_CONS_CAR(oprnd);
		cdr = HCL_CNODE_CONS_CDR(oprnd);

		if (cop == COP_COMPILE_IF_OBJECT_LIST || cop == COP_COMPILE_IF_OBJECT_LIST_TAIL)
		{
			if (HCL_CNODE_IS_SYMBOL_SYNCODED(car, HCL_SYNCODE_ELIF))
			{
				SWITCH_TOP_CFRAME (hcl, COP_SUBCOMPILE_ELIF, oprnd);
				goto done;
			}
			else if (HCL_CNODE_IS_SYMBOL_SYNCODED(car, HCL_SYNCODE_ELSE))
			{
				SWITCH_TOP_CFRAME (hcl, COP_SUBCOMPILE_ELSE, oprnd);
				goto done;
			}
		}

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);

		if (cdr)
		{
			/* there is a next statement to compile 
			 *
			 * (+ 1 2 3) - argument list. 1, 2, 3 pushed must remain in 
			 *             the stack until the function '+' is called.
			 *
			 * (lambda (x y) (+ x 10) (+ y 20)) 
			 *    - the result of (+ x 10) should be popped before (+ y 20)
			 *      is executed 
			 *
			 * for the latter, inject POP_STACKTOP after each object evaluation
			 * except the last.
			 */
			int nextcop;
			nextcop = (cop == COP_COMPILE_OBJECT_LIST)?    COP_COMPILE_OBJECT_LIST_TAIL:
			          (cop == COP_COMPILE_IF_OBJECT_LIST)? COP_COMPILE_IF_OBJECT_LIST_TAIL: cop;
			PUSH_SUBCFRAME (hcl, nextcop, cdr);
		}

		if (cop == COP_COMPILE_OBJECT_LIST_TAIL ||
		    cop == COP_COMPILE_IF_OBJECT_LIST_TAIL)
		{
			/* emit POP_STACKTOP before evaluating the second objects 
			 * and onwards. this goes above COP_COMPILE_OBJECT */
			PUSH_CFRAME (hcl, COP_EMIT_POP_STACKTOP, HCL_NULL);
		}
	}

done:
	return 0;
}

static int compile_array_list (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* oprnd;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_ARRAY_LIST);

	oprnd = cf->operand;

	if (!oprnd)
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_cnode_t* car, * cdr;
		hcl_ooi_t oldidx;

		if (!HCL_CNODE_IS_CONS(oprnd))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_CNODE_GET_TOK(oprnd), "redundant cdr in the array list");
			return -1;
		}

		car = HCL_CNODE_CONS_CAR(oprnd);
		cdr = HCL_CNODE_CONS_CDR(oprnd);

		oldidx = cf->u.array_list.index;

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);
		if (cdr)
		{
			PUSH_SUBCFRAME (hcl, COP_COMPILE_ARRAY_LIST, cdr);
			cf = GET_SUBCFRAME(hcl);
			cf->u.array_list.index = oldidx + 1;
		}

		PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_ARRAY, HCL_SMOOI_TO_OOP(oldidx));
	}

	return 0;
}


static int compile_bytearray_list (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* oprnd;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_BYTEARRAY_LIST);

	oprnd = cf->operand;

	if (!oprnd)
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_cnode_t* car, * cdr;
		hcl_ooi_t oldidx;

		if (!HCL_CNODE_IS_CONS(oprnd))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_CNODE_GET_TOK(oprnd), "redundant cdr in the byte-array list");
			return -1;
		}

		car = HCL_CNODE_CONS_CAR(oprnd);
		cdr = HCL_CNODE_CONS_CDR(oprnd);

		oldidx = cf->u.bytearray_list.index;

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);
		if (cdr)
		{
			PUSH_SUBCFRAME (hcl, COP_COMPILE_BYTEARRAY_LIST, cdr);
			cf = GET_SUBCFRAME(hcl);
			cf->u.bytearray_list.index = oldidx + 1;
		}

		PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_BYTEARRAY, HCL_SMOOI_TO_OOP(oldidx));
	}

	return 0;
}


static int compile_dic_list (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* oprnd;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_DIC_LIST);

	oprnd = cf->operand;

	if (!oprnd)
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_cnode_t* car, * cdr, * cadr, * cddr;

		if (!HCL_CNODE_IS_CONS(oprnd))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(oprnd), HCL_CNODE_GET_TOK(oprnd), "redundant cdr in the dictionary list");
			return -1;
		}

		car = HCL_CNODE_CONS_CAR(oprnd);
		cdr = HCL_CNODE_CONS_CDR(oprnd);

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);
		if (!cdr)
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_UNBALKV, HCL_CNODE_GET_LOC(car), HCL_NULL, "no value for key %.*js", HCL_CNODE_GET_TOKLEN(car), HCL_CNODE_GET_TOKPTR(car));
			return -1;
		}
		
		cadr = HCL_CNODE_CONS_CAR(cdr);
		cddr = HCL_CNODE_CONS_CDR(cdr);

		if (cddr)
		{
			PUSH_SUBCFRAME (hcl, COP_COMPILE_DIC_LIST, cddr);
		}

		PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_DIC, HCL_SMOOI_TO_OOP(0));
		PUSH_SUBCFRAME(hcl, COP_COMPILE_OBJECT, cadr);
	}

	return 0;
}

static int compile_qlist (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_cnode_t* oprnd;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_QLIST);

	oprnd = cf->operand;

	if (!oprnd)
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_cnode_t* car, * cdr;
		hcl_ooi_t oldidx;

		if (!HCL_CNODE_IS_CONS(oprnd))
		{
			/* the last element after . */
			SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, oprnd);
			PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_CONS_CDR, HCL_NULL);
		}
		else
		{
			car = HCL_CNODE_CONS_CAR(oprnd);
			cdr = HCL_CNODE_CONS_CDR(oprnd);

			SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car); /* 1 */
			if (cdr)
			{
				PUSH_SUBCFRAME (hcl, COP_COMPILE_QLIST, cdr); /* 3 */
				PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_CONS, HCL_NULL); /* 2 */
			}
			else
			{
				/* the last element */
				PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_CONS_END, HCL_NULL); /* 2 */
			}
		}
	}

	return 0;
}


/* ========================================================================= */

static HCL_INLINE int patch_nearest_post_if_body (hcl_t* hcl, hcl_cnode_t* cmd)
{
	hcl_ooi_t jump_inst_pos, body_pos;
	hcl_ooi_t jip, jump_offset;
	hcl_cframe2_t* cf;

	cf = find_cframe_from_top (hcl, COP_POST_IF_BODY);
	HCL_ASSERT (hcl, cf != HCL_NULL);

	/* jump instruction position of the JUMP_FORWARD_IF_FALSE after the conditional of the previous if or elif*/
	jip = HCL_OOP_TO_SMOOI(cf->operand); 

	if (hcl->code.bc.len <= cf->u.post_if.body_pos)
	{
		/* the if body is empty. */
		if (emit_byte_instruction (hcl, HCL_CODE_PUSH_NIL, HCL_NULL) <= -1) return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	/* emit jump_forward before the beginning of the else block.
	 * this is to make the earlier if or elif block to skip
	 * the else part. it is to be patched in post_else_body(). */
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);

	if (jump_offset > MAX_CODE_JUMP * 2)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_IFFLOOD, HCL_CNODE_GET_LOC(cmd), HCL_NULL, "code in %.*js too big - size %zu", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd), jump_offset);
		return -1;
	}
	patch_long_jump (hcl, jip, jump_offset);

	/* beginning of the elif/else block code */
	/* to drop the result of the conditional when the conditional is false */
	if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) return -1; 

	/* this is the actual beginning */
	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	body_pos = hcl->code.bc.len;

	/* modify the POST_IF_BODY frame */
	HCL_ASSERT (hcl, cf->opcode == COP_POST_IF_BODY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));
	cf->operand = HCL_SMOOI_TO_OOP(jump_inst_pos); 
	cf->u.post_if.body_pos = body_pos;

	return 0;
}

static HCL_INLINE int subcompile_elif (hcl_t* hcl)
{
	hcl_cnode_t* cmd, * obj, * cond, * src;
	hcl_cframe2_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_SUBCOMPILE_ELIF);

	src = cf->operand;
	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_ELIF));

	cmd = HCL_CNODE_CONS_CAR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no condition in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	cond = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_POST_IF_COND, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_if.body_pos = -1; /* unknown yet */
	cf->u.post_if.start_loc = *HCL_CNODE_GET_LOC(src);

	return patch_nearest_post_if_body(hcl, cmd);
}

static HCL_INLINE int subcompile_else (hcl_t* hcl)
{
	hcl_cnode_t* cmd, * obj, * src;
	hcl_cframe2_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_SUBCOMPILE_ELSE);

	src = cf->operand;
	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(src));
	HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL_SYNCODED(HCL_CNODE_CONS_CAR(src), HCL_SYNCODE_ELSE));

	cmd = HCL_CNODE_CONS_CAR(src);
	obj = HCL_CNODE_CONS_CDR(src);

	if (!obj)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_CNODE_GET_LOC(src), HCL_NULL, "no condition in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}
	else	if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in %.*js", HCL_CNODE_GET_TOKLEN(cmd), HCL_CNODE_GET_TOKPTR(cmd));
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj);

	return patch_nearest_post_if_body(hcl, cmd);
}

/* ========================================================================= */

static HCL_INLINE int subcompile_and_expr (hcl_t* hcl)
{
	hcl_cnode_t* obj, * expr;
	hcl_cframe2_t* cf;
	hcl_ooi_t jump_inst_pos;
	
	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_SUBCOMPILE_AND_EXPR);

	obj = cf->operand;

/* TODO: optimization - eat away all true expressions */
	if (!obj)
	{
		/* no more */
		POP_CFRAME (hcl);
		return 0;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in and");
		return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	if (emit_single_param_instruction(hcl, HCL_CODE_JUMP_FORWARD_IF_FALSE, MAX_CODE_JUMP) <= -1) return -1;
	if (emit_byte_instruction(hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) return -1; 

	expr = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CNODE_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, expr); /* 1 */
	
	PUSH_SUBCFRAME (hcl, COP_POST_AND_EXPR, obj); /* 3 */
	cf = GET_SUBCFRAME(hcl);
	cf->operand = HCL_SMOOI_TO_OOP(jump_inst_pos);
	
	PUSH_SUBCFRAME (hcl, COP_SUBCOMPILE_AND_EXPR, obj); /* 2 */

	return 0;
}

static HCL_INLINE int post_and_expr (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jip;
	hcl_oow_t jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_AND_EXPR);

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jip = HCL_OOP_TO_SMOOI(cf->operand);

	/* patch the jump insruction emitted after each expression inside the 'and' expression */
	jump_offset = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME(hcl);
	return 0;
}

/* ========================================================================= */

static HCL_INLINE int subcompile_or_expr (hcl_t* hcl)
{
	hcl_cnode_t* obj, * expr;
	hcl_cframe2_t* cf;
	hcl_ooi_t jump_inst_pos;
	
	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_SUBCOMPILE_OR_EXPR);

	obj = cf->operand;

/* TODO: optimization - eat away all false expressions */
	if (!obj)
	{
		/* no more */
		POP_CFRAME (hcl);
		return 0;
	}
	else if (!HCL_CNODE_IS_CONS(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_CNODE_GET_LOC(obj), HCL_CNODE_GET_TOK(obj), "redundant cdr in or");
		return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	if (emit_single_param_instruction(hcl, HCL_CODE_JUMP_FORWARD_IF_TRUE, MAX_CODE_JUMP) <= -1) return -1;
	if (emit_byte_instruction(hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) return -1; 

	expr = HCL_CNODE_CONS_CAR(obj);
	obj = HCL_CNODE_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, expr); /* 1 */
	
	PUSH_SUBCFRAME (hcl, COP_POST_OR_EXPR, obj); /* 3 */
	cf = GET_SUBCFRAME(hcl);
	cf->operand = HCL_SMOOI_TO_OOP(jump_inst_pos);
	
	PUSH_SUBCFRAME (hcl, COP_SUBCOMPILE_OR_EXPR, obj); /* 2 */

	return 0;
}

static HCL_INLINE int post_or_expr (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jip;
	hcl_oow_t jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_OR_EXPR);

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jip = HCL_OOP_TO_SMOOI(cf->operand);

	/* patch the jump insruction emitted after each expression inside the 'and' expression */
	jump_offset = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME(hcl);
	return 0;
}

/* ========================================================================= */

static HCL_INLINE int post_if_cond (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jump_inst_pos;
	hcl_ooi_t body_pos;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_IF_COND);

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_IF_FALSE, MAX_CODE_JUMP) <= -1) return -1;

	/* to drop the result of the conditional when it is true */
	if (emit_byte_instruction(hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) return -1; 

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	body_pos = hcl->code.bc.len;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_IF_OBJECT_LIST, cf->operand); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_POST_IF_BODY, HCL_SMOOI_TO_OOP(jump_inst_pos)); /* 2 */
	cf = GET_SUBCFRAME(hcl);
	cf->u.post_if.body_pos = body_pos;
	return 0;
}

static HCL_INLINE int post_if_body (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jip;
	hcl_oow_t jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_IF_BODY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	if (hcl->code.bc.len <= cf->u.post_if.body_pos)
	{
		/* if body is empty */
		if (emit_byte_instruction (hcl, HCL_CODE_PUSH_NIL, HCL_NULL) <= -1) return -1;
	}

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD_IF_FALSE instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);

	if (jump_offset > MAX_CODE_JUMP * 2)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_IFFLOOD, &cf->u.post_if.start_loc, HCL_NULL, "code too big - size %zu", jump_offset);
		return -1;
	}
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */
static HCL_INLINE int post_while_cond (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jump_inst_pos;
	hcl_ooi_t cond_pos, body_pos;
	hcl_ioloc_t start_loc;
	int jump_inst, next_cop;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_UNTIL_COND || cf->opcode == COP_POST_WHILE_COND);

	cond_pos = cf->u.post_while.cond_pos;
	start_loc = cf->u.post_while.start_loc;
	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	if (cf->opcode == COP_POST_UNTIL_COND)
	{
		jump_inst = HCL_CODE_JUMP_FORWARD_IF_TRUE;
		next_cop = COP_POST_UNTIL_BODY;
	}
	else
	{
		jump_inst = HCL_CODE_JUMP_FORWARD_IF_FALSE;
		next_cop = COP_POST_WHILE_BODY;
	}

	if (emit_single_param_instruction (hcl, jump_inst, MAX_CODE_JUMP) <= -1) return -1;
	if (emit_byte_instruction(hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) return -1;

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	body_pos = hcl->code.bc.len;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, cf->operand); /* 1 */
	PUSH_SUBCFRAME (hcl, next_cop, HCL_SMOOI_TO_OOP(jump_inst_pos)); /* 2 */
	cf = GET_SUBCFRAME(hcl);
	cf->u.post_while.cond_pos = cond_pos; 
	cf->u.post_while.body_pos = body_pos;
	cf->u.post_while.start_loc = start_loc;
	return 0;
}

static HCL_INLINE int post_while_body (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jip;
	hcl_ooi_t jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_UNTIL_BODY || cf->opcode == COP_POST_WHILE_BODY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	HCL_ASSERT (hcl, hcl->code.bc.len >= cf->u.post_while.cond_pos);
	if (hcl->code.bc.len > cf->u.post_while.body_pos)
	{
		/* some code exist after POP_STACKTOP after JUMP_FORWARD_IF_TRUE/FALSE.
		 * (until #f) =>
		 *   push_false
		 *   jump_forward_if_true XXXX
		 *   pop_stacktop            <-- 1) emitted in post_while_cond();
		 *   jump_backward YYYY      <-- 2) emitted below
		 *   pop_stacktop
		 * this check prevents another pop_stacktop between 1) and 2)
		 */
		if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_offset = hcl->code.bc.len - cf->u.post_while.cond_pos + 1;
	if (jump_offset > 3) jump_offset += HCL_HCL_CODE_LONG_PARAM_SIZE;
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_BACKWARD_0, jump_offset) <= -1) return -1;

	jip = HCL_OOP_TO_SMOOI(cf->operand);
	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD_IF_FALSE/JUMP_FORWARD_IF_TRUE instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);
	if (jump_offset > MAX_CODE_JUMP * 2)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BLKFLOOD, &cf->u.post_while.start_loc, HCL_NULL, "code too big - size %zu", jump_offset);
		return -1;
	}
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */

static int update_break (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_ooi_t jip, jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_UPDATE_BREAK);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);

	/* no explicit about jump_offset. because break can only place inside
	 * a loop, the same check in post_while_body() must assert
	 * this break jump_offset to be small enough */
	HCL_ASSERT (hcl, jump_offset <= MAX_CODE_JUMP * 2);
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */

static HCL_INLINE int emit_call (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_CALL);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction(hcl, HCL_CODE_CALL_0, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_array (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_ARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction(hcl, HCL_CODE_MAKE_ARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_bytearray (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_BYTEARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction(hcl, HCL_CODE_MAKE_BYTEARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_dic (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_DIC);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction(hcl, HCL_CODE_MAKE_DIC, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_cons (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_CONS);
	HCL_ASSERT (hcl, cf->operand == HCL_NULL);

	n = emit_byte_instruction(hcl, HCL_CODE_MAKE_CONS, HCL_NULL);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_array (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_ARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction(hcl, HCL_CODE_POP_INTO_ARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_bytearray (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_BYTEARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction(hcl, HCL_CODE_POP_INTO_BYTEARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_dic (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_DIC);

	n = emit_byte_instruction(hcl, HCL_CODE_POP_INTO_DIC, HCL_NULL);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_cons (hcl_t* hcl, int cmd)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_CONS ||
	                 cf->opcode == COP_EMIT_POP_INTO_CONS_END ||
	                 cf->opcode == COP_EMIT_POP_INTO_CONS_CDR);
	HCL_ASSERT (hcl, cf->operand == HCL_NULL);

	n = emit_byte_instruction (hcl, cmd, HCL_NULL);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_lambda (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	hcl_oow_t block_code_size, lfsize;
	hcl_ooi_t jip;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_LAMBDA);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	if (hcl->option.trait & HCL_TRAIT_INTERACTIVE) 
		lfsize = hcl->code.lit.len - hcl->c->blk.info[hcl->c->blk.depth].lfbase;

	hcl->c->blk.depth--;
	hcl->c->tv2.s.len = hcl->c->blk.info[hcl->c->blk.depth].tmprlen;
	hcl->c->tv2.wcount = hcl->c->blk.info[hcl->c->blk.depth].tmprcnt;

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	block_code_size = hcl->code.bc.len - jip - (HCL_HCL_CODE_LONG_PARAM_SIZE + 1);

	if (block_code_size == 0)
 	{
		/* no body in lambda - (lambda (a b c)) */
/* TODO: is this correct??? */
		if (emit_byte_instruction(hcl, HCL_CODE_PUSH_NIL, HCL_NULL) <= -1) return -1;
		block_code_size++;
	}

	if (emit_byte_instruction(hcl, HCL_CODE_RETURN_FROM_BLOCK, HCL_NULL) <= -1) return -1;
	block_code_size++;

	if (block_code_size > MAX_CODE_JUMP * 2)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BLKFLOOD, &cf->u.lambda.start_loc, HCL_NULL, "code too big - size %zu", block_code_size);
		return -1;
	}
	patch_long_jump (hcl, jip, block_code_size);

	if (hcl->option.trait & HCL_TRAIT_INTERACTIVE) 
		patch_long_param (hcl, cf->u.lambda.lfsize_pos, lfsize);

	POP_CFRAME (hcl);
	return 0;
}

static HCL_INLINE int emit_pop_stacktop (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_STACKTOP);
	HCL_ASSERT (hcl, cf->operand == HCL_NULL);

	n = emit_byte_instruction(hcl, HCL_CODE_POP_STACKTOP, HCL_NULL);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_return (hcl_t* hcl)
{
	hcl_cframe2_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_RETURN);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_byte_instruction(hcl, (HCL_OOP_TO_SMOOI(cf->operand) == 0? HCL_CODE_RETURN_FROM_BLOCK: HCL_CODE_RETURN_STACKTOP), HCL_NULL);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_set (hcl_t* hcl)
{
	hcl_cframe2_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_SET);

	if (cf->u.set.var_type == VAR_NAMED)
	{
		hcl_oow_t index;
		hcl_oop_t cons, sym;

		HCL_ASSERT (hcl, HCL_CNODE_IS_SYMBOL(cf->operand));

		sym = hcl_makesymbol(hcl, HCL_CNODE_GET_TOKPTR(cf->operand), HCL_CNODE_GET_TOKLEN(cf->operand));
		if (HCL_UNLIKELY(!sym)) return -1;

		cons = (hcl_oop_t)hcl_getatsysdic(hcl, sym);
		if (!cons) 
		{
			cons = (hcl_oop_t)hcl_putatsysdic(hcl, sym, hcl->_nil);
			if (!cons) return -1;
		}

		if (add_literal(hcl, cons, &index) <= -1 ||
		    emit_single_param_instruction(hcl, HCL_CODE_STORE_INTO_OBJECT_0, index) <= -1) return -1;
	}
	else
	{
		hcl_oow_t index;
		HCL_ASSERT (hcl, cf->u.set.var_type == VAR_INDEXED);
		HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

		index = (hcl_oow_t)HCL_OOP_TO_SMOOI(cf->operand);
		if (emit_indexed_variable_access(hcl, index, HCL_CODE_STORE_INTO_CTXTEMPVAR_0, HCL_CODE_STORE_INTO_TEMPVAR_0) <= -1) return -1;
	}

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */

int hcl_compile2 (hcl_t* hcl, hcl_cnode_t* obj)
{
	hcl_oow_t saved_bc_len, saved_lit_len;
	hcl_bitmask_t log_default_type_mask;

	HCL_ASSERT (hcl, GET_TOP_CFRAME_INDEX(hcl) < 0);

	saved_bc_len = hcl->code.bc.len;
	saved_lit_len = hcl->code.lit.len;

	log_default_type_mask = hcl->log.default_type_mask;
	hcl->log.default_type_mask |= HCL_LOG_COMPILER;

	HCL_ASSERT (hcl, hcl->c->tv2.s.len == 0);
	HCL_ASSERT (hcl, hcl->c->tv2.wcount == 0);
	HCL_ASSERT (hcl, hcl->c->blk.depth == -1);

/* TODO: in case i implement all global variables as block arguments at the top level...what should i do? */

	hcl->c->blk.depth++; /* this must be 0 here */

	/* 
	 * In the non-INTERACTIVE mode, the literal frame base doesn't matter.
	 * Only the initial function object contains the literal frame. 
	 * No other function objects are created. All lambda defintions are
	 * translated to base context objects instead.
	 * 
	 * In the INTERACTIVE mode, the literal frame base plays a key role.
	 * hcl_compile() is called for the top-level expression andthe literal
	 * frame base can be 0. The means it is ok for a top-level code to 
	 * reference part of the literal frame reserved for a lambda function.
	 *
	 *  (set b 1)
	 *  (defun set-a(x) (set a x))
	 *  (set a 2)
	 *  (set-a 4)
	 *  (printf "%d\n" a)
	 * 
	 * the global literal frame looks like this:
	 *  @0         (b)
	 *  @1         (a)
	 *  @2         (set-a)
	 *  @3         (printf . #<PRIM>)
	 *  @4         "%d\n"
	 *
	 * @1 to @2 will be copied to a function object when defun is executed.
	 * The literal frame of the created function object for set-a looks 
	 * like this
	 *  @0         (a)
	 *  @1         (set-a) 
	 */
	if (store_temporary_variable_count_for_block(hcl, hcl->c->tv2.wcount, hcl->c->tv2.s.len, 0) <= -1) return -1;

	PUSH_CFRAME (hcl, COP_COMPILE_OBJECT, obj);

	while (GET_TOP_CFRAME_INDEX(hcl) >= 0)
	{
		hcl_cframe2_t* cf;

		cf = GET_TOP_CFRAME(hcl);

/* TODO: tabulate this switch-based dispatch */
		switch (cf->opcode)
		{
			case COP_COMPILE_OBJECT:
				if (compile_object(hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_OBJECT_LIST:
			case COP_COMPILE_OBJECT_LIST_TAIL:
			case COP_COMPILE_IF_OBJECT_LIST:
			case COP_COMPILE_IF_OBJECT_LIST_TAIL:
			case COP_COMPILE_ARGUMENT_LIST:
				if (compile_object_list(hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_ARRAY_LIST:
				if (compile_array_list(hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_BYTEARRAY_LIST:
				if (compile_bytearray_list(hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_DIC_LIST:
				if (compile_dic_list(hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_QLIST:
				if (compile_qlist(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_CALL:
				if (emit_call(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_MAKE_ARRAY:
				if (emit_make_array(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_MAKE_BYTEARRAY:
				if (emit_make_bytearray(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_MAKE_DIC:
				if (emit_make_dic(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_MAKE_CONS:
				if (emit_make_cons(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_ARRAY:
				if (emit_pop_into_array(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_BYTEARRAY:
				if (emit_pop_into_bytearray(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_DIC:
				if (emit_pop_into_dic(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_CONS:
				if (emit_pop_into_cons(hcl, HCL_CODE_POP_INTO_CONS) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_CONS_END:
				if (emit_pop_into_cons(hcl, HCL_CODE_POP_INTO_CONS_END) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_CONS_CDR:
				if (emit_pop_into_cons(hcl, HCL_CODE_POP_INTO_CONS_CDR) <= -1) goto oops;
				break;

			case COP_EMIT_LAMBDA:
				if (emit_lambda(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_STACKTOP:
				if (emit_pop_stacktop(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_RETURN:
				if (emit_return(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_SET:
				if (emit_set(hcl) <= -1) goto oops;
				break;

			case COP_SUBCOMPILE_AND_EXPR:
				if (subcompile_and_expr(hcl) <= -1) goto oops;
				break;

			case COP_SUBCOMPILE_OR_EXPR:
				if (subcompile_or_expr(hcl) <= -1) goto oops;
				break;

			case COP_POST_AND_EXPR:
				if (post_and_expr(hcl) <= -1) goto oops;
				break;
				
			case COP_POST_OR_EXPR:
				if (post_or_expr(hcl) <= -1) goto oops;
				break;

			case COP_POST_IF_COND:
				if (post_if_cond(hcl) <= -1) goto oops;
				break;

			case COP_POST_IF_BODY:
				if (post_if_body(hcl) <= -1) goto oops;
				break;

			case COP_POST_UNTIL_BODY:
			case COP_POST_WHILE_BODY:
				if (post_while_body(hcl) <= -1) goto oops;
				break;

			case COP_POST_UNTIL_COND:
			case COP_POST_WHILE_COND:
				if (post_while_cond(hcl) <= -1) goto oops;
				break;

			case COP_SUBCOMPILE_ELIF:
				if (subcompile_elif(hcl) <= -1) goto oops;
				break;

			case COP_SUBCOMPILE_ELSE:
				if (subcompile_else(hcl) <= -1) goto oops;
				break;

			case COP_UPDATE_BREAK:
				if (update_break(hcl) <= -1) goto oops;
				break;

			default:
				HCL_DEBUG1 (hcl, "Internal error - invalid compiler opcode %d\n", cf->opcode);
				hcl_seterrbfmt (hcl, HCL_EINTERN, "invalid compiler opcode %d", cf->opcode);
				goto oops;
		}
	}

	/* emit the pop instruction to clear the final result */
/* TODO: for interactive use, this value must be accessible by the executor... how to do it? */
	if (emit_byte_instruction(hcl, HCL_CODE_POP_STACKTOP, HCL_NULL) <= -1) goto oops;

	HCL_ASSERT (hcl, GET_TOP_CFRAME_INDEX(hcl) < 0);
	HCL_ASSERT (hcl, hcl->c->tv2.s.len == 0);
	HCL_ASSERT (hcl, hcl->c->tv2.wcount == 0);
	HCL_ASSERT (hcl, hcl->c->blk.depth == 0);
	hcl->c->blk.depth--;

	hcl ->log.default_type_mask = log_default_type_mask;
	return 0;

oops:
	POP_ALL_CFRAMES (hcl);

	hcl->log.default_type_mask = log_default_type_mask;

	/* rollback any bytecodes or literals emitted so far */
	hcl->code.bc.len = saved_bc_len;
	hcl->code.lit.len = saved_lit_len;

	hcl->c->tv2.s.len = 0;
	hcl->c->tv2.wcount = 0;
	hcl->c->blk.depth = -1;
	return -1;
}