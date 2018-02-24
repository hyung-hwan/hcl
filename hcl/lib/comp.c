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

#define CODE_BUFFER_ALIGN 1024 /* TODO: set a bigger value */
#define TV_BUFFER_ALIGN 256
#define BLK_TMPRCNT_BUFFER_ALIGN 128

#define EMIT_BYTE_INSTRUCTION(hcl,code) \
	do { if (emit_byte_instruction(hcl,code) <= -1) return -1; } while(0)

#define EMIT_SINGLE_PARAM_INSTRUCTION(hcl,code) \
	do { if (emit_byte_instruction(hcl,code) <= -1) return -1; } while(0)


static int add_literal (hcl_t* hcl, hcl_oop_t obj, hcl_oow_t* index)
{
	hcl_oow_t capa, i;

/* TODO: speed up the following duplicate check loop */
	for (i = 0; i < hcl->code.lit.len; i++)
	{
		/* this removes redundancy of symbols, characters, and integers. */
		if (((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i] == obj)
		{
			*index = i;
			return i;
		}
	}

	capa = HCL_OBJ_GET_SIZE(hcl->code.lit.arr);
	if (hcl->code.lit.len >= capa)
	{
		hcl_oop_t tmp;
		hcl_oow_t newcapa;

		newcapa = capa + 20000; /* TODO: set a better resizing policy */
		tmp = hcl_remakengcarray (hcl, (hcl_oop_t)hcl->code.lit.arr, newcapa);
		if (!tmp) return -1;

		hcl->code.lit.arr = (hcl_oop_oop_t)tmp;
	}

	*index = hcl->code.lit.len;
	((hcl_oop_oop_t)hcl->code.lit.arr)->slot[hcl->code.lit.len++] = obj;
	return 0;
}

static int add_temporary_variable (hcl_t* hcl, hcl_oop_t name, hcl_oow_t dup_check_start)
{
	hcl_oow_t i;

	HCL_ASSERT (hcl, HCL_IS_SYMBOL (hcl, name));

	for (i = dup_check_start; i < hcl->c->tv.size; i++)
	{
		HCL_ASSERT (hcl, HCL_IS_SYMBOL (hcl, hcl->c->tv.ptr[i]));
		if (hcl->c->tv.ptr[i] == name)
		{
			hcl_seterrnum (hcl, HCL_EEXIST);
			return -1;
		}
	}

	if (hcl->c->tv.size >= hcl->c->tv.capa)
	{
		hcl_oop_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN (hcl->c->tv.capa + 1, TV_BUFFER_ALIGN); /* TODO: set a better resizing policy */
		tmp = hcl_reallocmem (hcl, hcl->c->tv.ptr, newcapa);
		if (!tmp) return -1;

		hcl->c->tv.capa = newcapa;
		hcl->c->tv.ptr = tmp;
	}

	hcl->c->tv.ptr[hcl->c->tv.size++] = name;
	return 0;
}

static int find_temporary_variable_backward (hcl_t* hcl, hcl_oop_t name, hcl_oow_t* index)
{
	hcl_oow_t i;

	HCL_ASSERT (hcl, HCL_IS_SYMBOL (hcl, name));
	for (i = hcl->c->tv.size; i > 0; )
	{
		--i;
		HCL_ASSERT (hcl, HCL_IS_SYMBOL (hcl, hcl->c->tv.ptr[i]));
		if (hcl->c->tv.ptr[i] == name)
		{
			*index = i;
			return 0;
		}
	}
	
	hcl_seterrnum (hcl, HCL_ENOENT);
	return -1;
}

static int store_temporary_variable_count_for_block (hcl_t* hcl, hcl_oow_t tmpr_count)
{
	HCL_ASSERT (hcl, hcl->c->blk.depth >= 0);

	if (hcl->c->blk.depth >= hcl->c->blk.tmprcnt_capa)
	{
		hcl_oow_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN (hcl->c->blk.depth + 1, BLK_TMPRCNT_BUFFER_ALIGN);
		tmp = (hcl_oow_t*)hcl_reallocmem (hcl, hcl->c->blk.tmprcnt, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->c->blk.tmprcnt_capa = newcapa;
		hcl->c->blk.tmprcnt = tmp;
	}

	hcl->c->blk.tmprcnt[hcl->c->blk.depth] = tmpr_count;
	return 0;
}

/* ========================================================================= */

static HCL_INLINE void patch_instruction (hcl_t* hcl, hcl_oow_t index, hcl_oob_t bc)
{
	HCL_ASSERT (hcl, index < hcl->code.bc.len);
	hcl->code.bc.arr->slot[index] = bc;
}

static int emit_byte_instruction (hcl_t* hcl, hcl_oob_t bc)
{
	hcl_oow_t capa;

	/* the context object has the ip field. it should be representable
	 * in a small integer. for simplicity, limit the total byte code length
	 * to fit in a small integer. because 'ip' points to the next instruction
	 * to execute, he upper bound should be (max - 1) so that i stays
	 * at the max when incremented */
	if (hcl->code.bc.len == HCL_SMOOI_MAX - 1)
	{
		hcl_seterrnum (hcl, HCL_EBCFULL); /* byte code full/too big */
		return -1;
	}

	capa = HCL_OBJ_GET_SIZE(hcl->code.bc.arr);
	if (hcl->code.bc.len >= capa)
	{
		hcl_oop_t tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN (capa + 1, CODE_BUFFER_ALIGN);
		tmp = hcl_remakengcbytearray (hcl, (hcl_oop_t)hcl->code.bc.arr, newcapa);
		if (!tmp) return -1;

		hcl->code.bc.arr = (hcl_oop_byte_t)tmp;
	}

	hcl->code.bc.arr->slot[hcl->code.bc.len++] = bc;
	return 0;
}


static int emit_single_param_instruction (hcl_t* hcl, int cmd, hcl_oow_t param_1)
{
	hcl_oob_t bc;

	switch (cmd)
	{
		case BCODE_PUSH_INSTVAR_0:
		case BCODE_STORE_INTO_INSTVAR_0:
		case BCODE_POP_INTO_INSTVAR_0:
		case HCL_CODE_PUSH_TEMPVAR_0:
		case HCL_CODE_STORE_INTO_TEMPVAR_0:
		case BCODE_POP_INTO_TEMPVAR_0:
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
		case BCODE_POP_INTO_OBJECT_0:
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
	if (emit_byte_instruction(hcl, bc) <= -1) return -1;
	return 0;

write_long:
	if (param_1 > MAX_CODE_PARAM) 
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 8) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, param_1) <= -1) return -1;
#endif
	return 0;

write_long2:
	if (param_1 > MAX_CODE_PARAM2) 
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 24) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 16) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >>  8) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 8) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF) <= -1) return -1;
#endif
	return 0;
}

static int emit_double_param_instruction (hcl_t* hcl, int cmd, hcl_oow_t param_1, hcl_oow_t param_2)
{
	hcl_oob_t bc;

	switch (cmd)
	{
		case HCL_CODE_STORE_INTO_CTXTEMPVAR_0:
		case BCODE_POP_INTO_CTXTEMPVAR_0:
		case HCL_CODE_PUSH_CTXTEMPVAR_0:
		case BCODE_PUSH_OBJVAR_0:
		case BCODE_STORE_INTO_OBJVAR_0:
		case BCODE_POP_INTO_OBJVAR_0:
		case BCODE_SEND_MESSAGE_0:
		case BCODE_SEND_MESSAGE_TO_SUPER_0:
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


		case HCL_CODE_MAKE_BLOCK:
			bc = cmd;
			goto write_long;
	}

	hcl_seterrnum (hcl, HCL_EINVAL);
	return -1;

write_short:
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, param_2) <= -1) return -1;
	return 0;

write_long:
	if (param_1 > MAX_CODE_PARAM || param_2 > MAX_CODE_PARAM)
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, param_1 >> 8) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_2 >> 8) <= -1 ||
	    emit_byte_instruction(hcl, param_2 & 0xFF) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, param_1) <= -1 ||
	    emit_byte_instruction(hcl, param_2) <= -1) return -1;
#endif
	return 0;

/*
write_long2:
	if (param_1 > MAX_CODE_PARAM || param_2 > MAX_CODE_PARAM)
	{
		hcl_seterrnum (hcl, HCL_ERANGE);
		return -1;
	}
#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 24) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >> 16) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, (param_1 >>  8) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF) <= -1  ||
	    emit_byte_instruction(hcl, (param_2 >> 24) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, (param_2 >> 16) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, (param_2 >>  8) & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_2 & 0xFF) <= -1) return -1;
#else
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, param_1 >> 8) <= -1 ||
	    emit_byte_instruction(hcl, param_1 & 0xFF) <= -1 ||
	    emit_byte_instruction(hcl, param_2 >> 8) <= -1 ||
	    emit_byte_instruction(hcl, param_2 & 0xFF) <= -1) return -1;
#endif
*/
	return 0;
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
				return emit_byte_instruction (hcl, HCL_CODE_PUSH_NEGONE);

			case 0:
				return emit_byte_instruction (hcl, HCL_CODE_PUSH_ZERO);

			case 1:
				return emit_byte_instruction (hcl, HCL_CODE_PUSH_ONE);

			case 2:
				return emit_byte_instruction (hcl, HCL_CODE_PUSH_TWO);
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

		HCL_ASSERT (hcl, hcl->code.bc.arr->slot[jip] == HCL_CODE_JUMP_FORWARD_X ||
		            hcl->code.bc.arr->slot[jip] == HCL_CODE_JUMP_BACKWARD_X ||
		            hcl->code.bc.arr->slot[jip] == HCL_CODE_JUMP_FORWARD_IF_TRUE ||
		            hcl->code.bc.arr->slot[jip] == HCL_CODE_JUMP_FORWARD_IF_FALSE);

		/* JUMP2 instructions are chosen to be greater than its JUMP counterpart by 1 */
		patch_instruction (hcl, jip, hcl->code.bc.arr->slot[jip] + 1); 
		jump_offset -= MAX_CODE_JUMP;
	}

#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	patch_instruction (hcl, jip + 1, jump_offset >> 8);
	patch_instruction (hcl, jip + 2, jump_offset & 0xFF);
#else
	patch_instruction (hcl, jip + 1, jump_offset);
#endif
}

/* ========================================================================= */
static HCL_INLINE int _insert_cframe (hcl_t* hcl, hcl_ooi_t index, int opcode, hcl_oop_t operand)
{
	hcl_cframe_t* tmp;

	HCL_ASSERT (hcl, index >= 0);
	
	hcl->c->cfs.top++;
	HCL_ASSERT (hcl, hcl->c->cfs.top >= 0);
	HCL_ASSERT (hcl, index <= hcl->c->cfs.top);

	if ((hcl_oow_t)hcl->c->cfs.top >= hcl->c->cfs.capa)
	{
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN (hcl->c->cfs.top + 256, 256); /* TODO: adjust this capacity */
		tmp = hcl_reallocmem (hcl, hcl->c->cfs.ptr, newcapa * HCL_SIZEOF(hcl_cframe_t));
		if (!tmp) 
		{
			hcl->c->cfs.top--;
			return -1;
		}

		hcl->c->cfs.capa = newcapa;
		hcl->c->cfs.ptr = tmp;
	}

	if (index < hcl->c->cfs.top)
	{
		HCL_MEMMOVE (&hcl->c->cfs.ptr[index + 1], &hcl->c->cfs.ptr[index], (hcl->c->cfs.top - index) * HCL_SIZEOF(*tmp));
	}

	tmp = &hcl->c->cfs.ptr[index];
	tmp->opcode = opcode;
	tmp->operand = operand;
	/* leave tmp->u untouched/uninitialized */
	return 0;
}

static int insert_cframe (hcl_t* hcl, hcl_ooi_t index, int opcode, hcl_oop_t operand)
{
	if (hcl->c->cfs.top == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl_seterrnum (hcl, HCL_EFRMFLOOD);
		return -1;
	}

	return _insert_cframe (hcl, index, opcode, operand);
}

static int push_cframe (hcl_t* hcl, int opcode, hcl_oop_t operand)
{
	if (hcl->c->cfs.top == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl_seterrnum (hcl, HCL_EFRMFLOOD);
		return -1;
	}

	return _insert_cframe (hcl, hcl->c->cfs.top + 1, opcode, operand);
}

static HCL_INLINE void pop_cframe (hcl_t* hcl)
{
	HCL_ASSERT (hcl, hcl->c->cfs.top >= 0);
	hcl->c->cfs.top--;
}

#define PUSH_CFRAME(hcl,opcode,operand) \
	do { if (push_cframe(hcl,opcode,operand) <= -1) return -1; } while(0)

#define INSERT_CFRAME(hcl,index,opcode,operand) \
	do { if (insert_cframe(hcl,index,opcode,operand) <= -1) return -1; } while(0)

#define POP_CFRAME(hcl) pop_cframe(hcl)

#define POP_ALL_CFRAMES(hcl) (hcl->c->cfs.top = -1)

#define GET_TOP_CFRAME_INDEX(hcl) (hcl->c->cfs.top)

#define GET_TOP_CFRAME(hcl) (&hcl->c->cfs.ptr[hcl->c->cfs.top])

#define GET_CFRAME(hcl,index) (&hcl->c->cfs.ptr[index])

#define SWITCH_TOP_CFRAME(hcl,_opcode,_operand) \
	do { \
		hcl_cframe_t* _cf = GET_TOP_CFRAME(hcl); \
		_cf->opcode = _opcode; \
		_cf->operand = _operand; \
	} while (0);

#define SWITCH_CFRAME(hcl,_index,_opcode,_operand) \
	do { \
		hcl_cframe_t* _cf = GET_CFRAME(hcl,_index); \
		_cf->opcode = _opcode; \
		_cf->operand = _operand; \
	} while (0);

static int push_subcframe (hcl_t* hcl, int opcode, hcl_oop_t operand)
{
	hcl_cframe_t* cf, tmp;

	cf = GET_TOP_CFRAME(hcl);
	tmp = *cf;
	cf->opcode = opcode;
	cf->operand = operand;

	return push_cframe (hcl, tmp.opcode, tmp.operand);
}

static HCL_INLINE hcl_cframe_t* find_cframe_from_top (hcl_t* hcl, int opcode)
{
	hcl_cframe_t* cf;
	hcl_ooi_t i;

	for (i = hcl->c->cfs.top; i >= 0; i--)
	{
		cf = &hcl->c->cfs.ptr[i];
		if (cf->opcode == opcode) return cf;
	}

	return HCL_NULL;
}

#define PUSH_SUBCFRAME(hcl,opcode,operand) \
	do { if (push_subcframe(hcl,opcode,operand) <= -1) return -1; } while(0)

#define GET_SUBCFRAME(hcl) (&hcl->c->cfs.ptr[hcl->c->cfs.top - 1])

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

	COP_SUBCOMPILE_ELIF,
	COP_SUBCOMPILE_ELSE,

	COP_EMIT_CALL,

	COP_EMIT_MAKE_ARRAY,
	COP_EMIT_MAKE_BYTEARRAY,
	COP_EMIT_MAKE_DIC,
	COP_EMIT_POP_INTO_ARRAY,
	COP_EMIT_POP_INTO_BYTEARRAY,
	COP_EMIT_POP_INTO_DIC,

	COP_EMIT_LAMBDA,
	COP_EMIT_POP_STACKTOP,
	COP_EMIT_RETURN,
	COP_EMIT_SET,

	COP_POST_IF_COND,
	COP_POST_IF_BODY,

	COP_POST_UNTIL_BODY,
	COP_POST_UNTIL_COND,
	COP_POST_WHILE_BODY,
	COP_POST_WHILE_COND,

	COP_UPDATE_BREAK
};

/* ========================================================================= */

static int compile_break (hcl_t* hcl, hcl_oop_t src)
{
	/* (break) */
	hcl_oop_t obj;
	hcl_ooi_t i;

	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_break);

	obj = HCL_CONS_CDR(src);
	if (!HCL_IS_NIL(hcl,obj))
	{
		if (HCL_IS_CONS(hcl,obj))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL,
				"redundant argument in break - %O", src); /* TODO: error location */
		}
		else
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
				"redundant cdr in break - %O", src); /* TODO: error location */
			return -1;
		}
		return -1;
	}

	for (i = hcl->c->cfs.top; i >= 0; --i)
	{
		const hcl_cframe_t* tcf;
		tcf = &hcl->c->cfs.ptr[i];

		if (tcf->opcode == COP_EMIT_LAMBDA) break; /* seems to cross lambda boundary */

		if (tcf->opcode == COP_POST_UNTIL_BODY || tcf->opcode == COP_POST_WHILE_BODY)
		{
			hcl_ooi_t jump_inst_pos;

			/* (break) is not really a function call. but to make it look like a
			 * function call, i generate PUSH_NIL so nil becomes a return value.
			 *     (set x (until #f (break)))
			 * x will get nill. */
			if (emit_byte_instruction (hcl, HCL_CODE_PUSH_NIL) <= -1) return -1;

/* TODO: study if supporting expression after break is good like return. (break (+ 10 20)) */
			HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
			jump_inst_pos = hcl->code.bc.len;
			
			if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;
			INSERT_CFRAME (hcl, i, COP_UPDATE_BREAK, HCL_SMOOI_TO_OOP(jump_inst_pos));

			POP_CFRAME (hcl);
			return 0;
		}
	}

	hcl_setsynerrbfmt (hcl, HCL_SYNERR_BREAK, HCL_NULL, HCL_NULL, 
		"break outside loop - %O", src); /* TODO: error location */
	return -1;
}

static int compile_if (hcl_t* hcl, hcl_oop_t src)
{
	hcl_oop_t obj, cond;
	hcl_cframe_t* cf;

	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_if);

	/* (if (< 20 30) 
	 *   (do this)
	 *   (do that)
	 * elif (< 20 30)
	 *   (do it)
	 * else
	 *   (do this finally)
	 * )
	 */
	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL,
			"no condition specified in if - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
			"redundant cdr in if - %O", src); /* TODO: error location */
		return -1;
	}

	cond = HCL_CONS_CAR(obj);
	obj = HCL_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_POST_IF_COND, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_if.body_pos = -1; /* unknown yet */
/* TODO: OPTIMIZATION:
 *       pass information on the conditional if it's an absoluate true or absolute false to
 *       eliminate some code .. i can't eliminate code because there can be else or elif... 
 *       if absoluate true, don't need else or other elif part
 *       if absoluate false, else or other elif part is needed.
 */
	return 0;
}

static int compile_lambda (hcl_t* hcl, hcl_oop_t src, int defun)
{
	hcl_oop_t obj, args;
	hcl_oow_t nargs, ntmprs;
	hcl_ooi_t jump_inst_pos;
	hcl_oow_t saved_tv_count, tv_dup_start;
	hcl_oop_t defun_name;

	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));

	saved_tv_count = hcl->c->tv.size;
	obj = HCL_CONS_CDR(src);

	if (defun)
	{
		HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_defun);

		if (HCL_IS_NIL(hcl, obj))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMELIST, HCL_NULL, HCL_NULL,
				"no defun name - %O", src); /* TODO: error location */
			return -1;
		}
		else if (!HCL_IS_CONS(hcl, obj))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
				"redundant cdr in defun - %O", src); /* TODO: error location */
			return -1;
		}

		defun_name = HCL_CONS_CAR(obj);
		if (!HCL_IS_SYMBOL(hcl, defun_name))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_NULL, HCL_NULL,
				"defun name not a symbol - %O", defun_name); /* TODO: error location */
			return -1;
		}

		if (HCL_OBJ_GET_FLAGS_SYNCODE(defun_name) || HCL_OBJ_GET_FLAGS_KERNEL(defun_name))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL, 
				"special symbol not to be used as a defun name - %O", defun_name); /* TOOD: error location */
			return -1;
		}

		obj = HCL_CONS_CDR(obj);
	}
	else
	{
		HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_lambda);
	}

	if (HCL_IS_NIL(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMELIST, HCL_NULL, HCL_NULL,
			"no argument list in lambda - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
			"redundant cdr in lambda - %O", src); /* TODO: error location */
		return -1;
	}

	args = HCL_CONS_CAR(obj);
	if (HCL_IS_NIL(hcl, args))
	{
		/* no argument - (lambda () (+ 10 20)) */
		nargs = 0;
	}
	else
	{
		hcl_oop_t arg, ptr;

		if (!HCL_IS_CONS(hcl, args))
		{
			hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMELIST, HCL_NULL, HCL_NULL, 
				"not a lambda argument list - %O", args); /* TODO: error location */
			return -1;
		}

		tv_dup_start = hcl->c->tv.size;
		nargs = 0;
		ptr = args;
		do
		{
			arg = HCL_CONS_CAR(ptr);
			if (!HCL_IS_SYMBOL(hcl, arg))
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAME, HCL_NULL, HCL_NULL,
					"lambda argument not a symbol - %O", arg); /* TODO: error location */
				return -1;
			}

			if (HCL_OBJ_GET_FLAGS_SYNCODE(arg) || HCL_OBJ_GET_FLAGS_KERNEL(arg))
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDARGNAME, HCL_NULL, HCL_NULL,
					"special symbol not to be declared as an argument - %O", arg); /* TOOD: error location */
				return -1;
			}

			if (add_temporary_variable (hcl, arg, tv_dup_start) <= -1) 
			{
				if (hcl->errnum == HCL_EEXIST)
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGNAMEDUP, HCL_NULL, HCL_NULL,
						"lambda argument duplicate - %O", arg); /* TODO: error location */
				}
				return -1;
			}
			nargs++;

			ptr = HCL_CONS_CDR(ptr);
			if (!HCL_IS_CONS(hcl, ptr)) 
			{
				if (!HCL_IS_NIL(hcl, ptr))
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
						"redundant cdr in lambda argument list - %O", args); /* TODO: error location */
					return -1;
				}
				break;
			}
		}
		while (1);
	}

	HCL_ASSERT (hcl, nargs == hcl->c->tv.size - saved_tv_count);
	if (nargs > MAX_CODE_NBLKARGS) /*TODO: change this limit to max call argument count */
	{
		/* while an integer object is pused to indicate the number of
		 * block arguments, evaluation which is done by message passing
		 * limits the number of arguments that can be passed. so the
		 * check is implemented */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_NULL, HCL_NULL, "too many(%zu) arguments - %O", nargs, args); 
		return -1;
	}

	ntmprs = nargs;  
	obj = HCL_CONS_CDR(obj);

	tv_dup_start = hcl->c->tv.size;
	while (HCL_IS_CONS(hcl, obj))
	{
		hcl_oop_t dcl;

		dcl = HCL_CONS_CAR(obj);
		if (HCL_IS_SYMBOL_ARRAY(hcl, dcl))
		{
			hcl_oow_t i, sz;

			sz = HCL_OBJ_GET_SIZE(dcl);
			for (i = 0; i < sz; i++)
			{
				if (HCL_OBJ_GET_FLAGS_SYNCODE(((hcl_oop_oop_t)dcl)->slot[i]) ||
				    HCL_OBJ_GET_FLAGS_KERNEL(((hcl_oop_oop_t)dcl)->slot[i]))
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL, 
						"special symbol not to be declared as a variable - %O", obj); /* TOOD: error location */
					return -1;
				}

				if (add_temporary_variable (hcl, ((hcl_oop_oop_t)dcl)->slot[i], tv_dup_start) <= -1) 
				{
					if (hcl->errnum == HCL_EEXIST)
					{
						hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAMEDUP, HCL_NULL, HCL_NULL,
							"local variable duplicate - %O", ((hcl_oop_oop_t)dcl)->slot[i]); /* TODO: error location */
					}

					return -1;
				}

				ntmprs++;
			}

			obj = HCL_CONS_CDR(obj);
		}
		else break;
	}

	/* ntmprs: number of temporary variables including arguments */
	HCL_ASSERT (hcl, ntmprs == hcl->c->tv.size - saved_tv_count);
	if (ntmprs > MAX_CODE_NBLKTMPRS)
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARFLOOD, HCL_NULL, HCL_NULL, "too many(%zu) variables - %O", ntmprs, args); 
		return -1;
	}

	if (hcl->c->blk.depth == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BLKDEPTH, HCL_NULL, HCL_NULL, "lambda block depth too deep - %O", src); 
		return -1;
	}
	hcl->c->blk.depth++;
	if (store_temporary_variable_count_for_block (hcl, hcl->c->tv.size) <= -1) return -1;

	/* use the accumulated number of temporaries so far when generating
	 * the make_block instruction. at context activation time, the actual 
	 * count of temporaries for this block is derived by subtracting the 
	 * count of temporaries in the home context */
	if (emit_double_param_instruction (hcl, HCL_CODE_MAKE_BLOCK, nargs, hcl->c->tv.size/*ntmprs*/) <= -1) return -1;

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);  /* guaranteed in emit_byte_instruction() */
	jump_inst_pos = hcl->code.bc.len;
	/* specifying MAX_CODE_JUMP causes emit_single_param_instruction() to 
	 * produce the long jump instruction (BCODE_JUMP_FORWARD_X) */
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj);

	if (defun)
	{
		hcl_oow_t index;
		hcl_cframe_t* cf;

		if (find_temporary_variable_backward(hcl, defun_name, &index) <= -1)
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

	return 0;
}

static int compile_return (hcl_t* hcl, hcl_oop_t src)
{
	hcl_oop_t obj, val;

	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_return);

	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
/* TODO: should i allow (return)? does it return the last value on the stack? */
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL, "no value specified in return - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, "redundant cdr in return - %O", src); /* TODO: error location */
		return -1;
	}

	val = HCL_CONS_CAR(obj);

	obj = HCL_CONS_CDR(obj);
	if (!HCL_IS_NIL(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL,  "more than 1 argument to return - %O", src); /* TODO: error location */
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, val);
	PUSH_SUBCFRAME (hcl, COP_EMIT_RETURN, hcl->_nil);

	return 0;
}

static int compile_set (hcl_t* hcl, hcl_oop_t src)
{
	hcl_cframe_t* cf;
	hcl_oop_t obj, var, val;
	hcl_oow_t index;

	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_set);

	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_NULL, HCL_NULL, "no variable name in set - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, "redundant cdr in set - %O", src); /* TODO: error location */
		return -1;
	}

	var = HCL_CONS_CAR(obj);
	if (!HCL_IS_SYMBOL(hcl, var))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_VARNAME, HCL_NULL, HCL_NULL, "variable name not a symbol - %O", var); /* TODO: error location */
		return -1;
	}

	if (HCL_OBJ_GET_FLAGS_SYNCODE(var) || HCL_OBJ_GET_FLAGS_KERNEL(var))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL, "special symbol not to be used as a variable name - %O", var); /* TOOD: error location */
		return -1;
	}

	obj = HCL_CONS_CDR(obj);
	if (HCL_IS_NIL(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL, "no value specified in set - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, "redundant cdr in set - %O", src); /* TODO: error location */
		return -1;
	}

	val = HCL_CONS_CAR(obj);

	obj = HCL_CONS_CDR(obj);
	if (!HCL_IS_NIL(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL, "too many arguments to set - %O", src); /* TODO: error location */
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, val);

	if (find_temporary_variable_backward(hcl, var, &index) <= -1)
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

static int compile_do (hcl_t* hcl, hcl_oop_t src)
{
	hcl_oop_t obj;

	/* (do 
	 *   (+ 10 20)
	 *   (* 2 30)
	 *  ...
	 * ) 
	 * you can use this to combine multiple expressions to a single expression
	 */

	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_do);

	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL,
			"no expression specified in do - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
			"redundant cdr in do - %O", src); /* TODO: error location */
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj); 
	return 0;
}


static int compile_while (hcl_t* hcl, hcl_oop_t src, int next_cop)
{
	/* (while (xxxx) ... ) 
      * (until (xxxx) ... ) */
	hcl_oop_t obj, cond;
	hcl_oow_t cond_pos;
	hcl_cframe_t* cf;

	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_until || HCL_CONS_CAR(src) == hcl->_while);
	HCL_ASSERT (hcl, next_cop == COP_POST_UNTIL_COND || next_cop == COP_POST_WHILE_COND);

	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
		/* no value */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL,
			"no loop condition specified - %O", src); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
			"redundant cdr in loop - %O", src); /* TODO: error location */
		return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	cond_pos = hcl->code.bc.len; /* position where the bytecode for the conditional is emitted */

	cond = HCL_CONS_CAR(obj);
	obj = HCL_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, next_cop, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_while.cond_pos = cond_pos;
	cf->u.post_while.body_pos = -1; /* unknown yet*/

	return 0;
}
/* ========================================================================= */

static int compile_cons_array_expression (hcl_t* hcl, hcl_oop_t obj)
{
	/* #[ ] */
	hcl_ooi_t nargs;
	hcl_cframe_t* cf;

	/* NOTE: cframe management functions don't use the object memory.
	 *       many operations can be performed without taking GC into account */
	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_ARRAY, HCL_SMOOI_TO_OOP(0));

	nargs = hcl_countcons(hcl, obj);
	if (nargs > MAX_CODE_PARAM) 
	{
		/* TODO: change to syntax error */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_NULL, HCL_NULL, "too many(%zd) elements into array - %O", nargs, obj); 
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

static int compile_cons_bytearray_expression (hcl_t* hcl, hcl_oop_t obj)
{
	/* #[ ] */
	hcl_ooi_t nargs;
	hcl_cframe_t* cf;

	/* NOTE: cframe management functions don't use the object memory.
	 *       many operations can be performed without taking GC into account */
	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_BYTEARRAY, HCL_SMOOI_TO_OOP(0));

	nargs = hcl_countcons(hcl, obj);
	if (nargs > MAX_CODE_PARAM) 
	{
		/* TODO: change to syntax error */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_NULL, HCL_NULL, "too many(%zd) elements into byte-array - %O", nargs, obj); 
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

static int compile_cons_dic_expression (hcl_t* hcl, hcl_oop_t obj)
{
	/* #{ } */
	hcl_ooi_t nargs;
	hcl_cframe_t* cf;

	SWITCH_TOP_CFRAME (hcl, COP_EMIT_MAKE_DIC, HCL_SMOOI_TO_OOP(0));

	nargs = hcl_countcons(hcl, obj);
	if (nargs > MAX_CODE_PARAM) 
	{
		/* TODO: change to syntax error */
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_NULL, HCL_NULL, "too many(%zd) elements into dictionary - %O", nargs, obj); 
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

static int compile_cons_xlist_expression (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oop_t car;
	int syncode; /* syntax code of the first element */

	/* a valid function call
	 * (function-name argument-list)
	 *   function-name can be:
	 *     a symbol.
	 *     another function call.
	 * if the name is another function call, i can't know if the 
	 * function name will be valid at the compile time.
	 */
	HCL_ASSERT (hcl, HCL_IS_CONS_XLIST(hcl, obj));

	car = HCL_CONS_CAR(obj);
	if (HCL_IS_SYMBOL(hcl,car) && (syncode = HCL_OBJ_GET_FLAGS_SYNCODE(car)))
	{
		switch (syncode)
		{
			case HCL_SYNCODE_BREAK:
				/* break */
				if (compile_break(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_DEFUN:
				if (compile_lambda(hcl, obj, 1) <= -1) return -1;
				break;

			case HCL_SYNCODE_DO:
				if (compile_do(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_ELSE:
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ELSE, HCL_NULL, HCL_NULL, "else without if - %O", obj); /* error location */
				return -1;

			case HCL_SYNCODE_ELIF:
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ELIF, HCL_NULL, HCL_NULL, "elif without if - %O", obj); /* error location */
				return -1;

			case HCL_SYNCODE_IF:
				if (compile_if(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_LAMBDA:
				/* (lambda (x y) (+ x y)) */
				if (compile_lambda(hcl, obj, 0) <= -1) return -1;
				break;

			case HCL_SYNCODE_SET:
				/* (set x 10) 
				 * (set x (lambda (x y) (+ x y)) */
				if (compile_set(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_RETURN:
				/* (return 10)
				 * (return (+ 10 20)) */
				if (compile_return(hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_UNTIL:
				if (compile_while(hcl, obj, COP_POST_UNTIL_COND) <= -1) return -1;
				break;

			case HCL_SYNCODE_WHILE:
				if (compile_while(hcl, obj, COP_POST_WHILE_COND) <= -1) return -1;
				break;

			default:
				HCL_DEBUG3 (hcl, "Internal error - unknown syncode %d at %s:%d\n", syncode, __FILE__, __LINE__);
				hcl_seterrnum (hcl, HCL_EINTERN);
				return -1;
		}
	}
	else if (HCL_IS_SYMBOL(hcl,car) || HCL_IS_CONS_XLIST(hcl,car))
	{
		/* normal function call 
		 *  (<operator> <operand1> ...) */
		hcl_ooi_t nargs;
		hcl_ooi_t oldtop;
		hcl_cframe_t* cf;
		hcl_oop_t cdr;
		hcl_oop_cons_t sdc;

		/* NOTE: cframe management functions don't use the object memory.
		 *       many operations can be performed without taking GC into account */

		/* store the position of COP_EMIT_CALL to be produced with
		 * SWITCH_TOP_CFRAM() in oldtop for argument count patching 
		 * further down */
		oldtop = GET_TOP_CFRAME_INDEX(hcl); 
		HCL_ASSERT (hcl, oldtop >= 0);

		SWITCH_TOP_CFRAME (hcl, COP_EMIT_CALL, HCL_SMOOI_TO_OOP(0));

		/* compile <operator> */
		PUSH_CFRAME (hcl, COP_COMPILE_OBJECT, car);

		/* compile <operand1> ... etc */
		cdr = HCL_CONS_CDR(obj);

		if (HCL_IS_NIL(hcl, cdr)) 
		{
			nargs = 0;
		}
		else
		{
			if (!HCL_IS_NIL(hcl, cdr) && !HCL_IS_CONS(hcl, cdr))
			{
				/* (funname . 10) */
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, "redundant cdr in function call - %O", obj); /* TODO: error location */
				return -1;
			}

			nargs = hcl_countcons(hcl, cdr);
			if (nargs > MAX_CODE_PARAM) 
			{
				hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGFLOOD, HCL_NULL, HCL_NULL, "too many(%zd) parameters in function call - %O", nargs, obj); 
				return -1;
			}
		}

		sdc = hcl_getatsysdic(hcl, car);
		if (sdc)
		{
			hcl_oop_word_t sdv;
			sdv = (hcl_oop_word_t)HCL_CONS_CDR(sdc);
			if (HCL_IS_PRIM(hcl, sdv))
			{
				if (nargs < sdv->slot[1] || nargs > sdv->slot[2])
				{
					hcl_setsynerrbfmt (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL, 
						"parameters count(%zd) mismatch in function call - %O - expecting %zu-%zu parameters", nargs, obj, sdv->slot[1], sdv->slot[2]); 
					return -1;
				}
			}
		};

		/* redundant cdr check is performed inside compile_object_list() */
		PUSH_SUBCFRAME (hcl, COP_COMPILE_ARGUMENT_LIST, cdr);

		/* patch the argument count in the operand field of the COP_EMIT_CALL frame */
		cf = GET_CFRAME(hcl, oldtop);
		HCL_ASSERT (hcl, cf->opcode == COP_EMIT_CALL);
		cf->operand = HCL_SMOOI_TO_OOP(nargs);
	}
	else
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_CALLABLE, HCL_NULL, HCL_NULL, "invalid callable %O in function call - %O", car, obj); /* error location */
		return -1;
	}

	return 0;
}

static int emit_indexed_variable_access (hcl_t* hcl, hcl_oow_t index, hcl_oob_t baseinst1, hcl_oob_t baseinst2)
{
#if defined(HCL_USE_CTXTEMPVAR)
	if (hcl->c->blk.depth >= 0)
	{
		hcl_oow_t i;

		/* if a temporary variable is accessed inside a block,
		 * use a special instruction to indicate it */
		HCL_ASSERT (hcl, index < hcl->c->blk.tmprcnt[hcl->c->blk.depth]);
		for (i = hcl->c->blk.depth; i > 0; i--) /* excluded the top level -- TODO: change this code depending on global variable handling */
		{
			if (index >= hcl->c->blk.tmprcnt[i - 1])
			{
				hcl_oow_t ctx_offset, index_in_ctx;
				ctx_offset = hcl->c->blk.depth - i;
				index_in_ctx = index - hcl->c->blk.tmprcnt[i - 1];
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
#endif

	/* TODO: top-level... verify this. this will vary depending on how i implement the top-level and global variables... */
	if (emit_single_param_instruction (hcl, baseinst2, index) <= -1) return -1;
	return 0;
}

static HCL_INLINE int compile_symbol (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oow_t index;

	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, obj));

	if (HCL_OBJ_GET_FLAGS_SYNCODE(obj))
	{
		hcl_setsynerrbfmt (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL,
			"special symbol not to be used as a variable name - %O", obj); /* TOOD: error location */
		return -1;
	}

	/* check if a symbol is a local variable */
	if (find_temporary_variable_backward (hcl, obj, &index) <= -1)
	{
		hcl_oop_t cons;
/* TODO: if i require all variables to be declared, this part is not needed and should handle it as an error */
/* TODO: change the scheme... allow declaration??? */
		/* global variable */
		cons = (hcl_oop_t)hcl_getatsysdic (hcl, obj);
		if (!cons) 
		{
			cons = (hcl_oop_t)hcl_putatsysdic (hcl, obj, hcl->_nil);
			if (!cons) return -1;
		}

		if (add_literal(hcl, cons, &index) <= -1 ||
		    emit_single_param_instruction (hcl, HCL_CODE_PUSH_OBJECT_0, index) <= -1) return -1;

		return 0;
	}
	else
	{
		return emit_indexed_variable_access (hcl, index, HCL_CODE_PUSH_CTXTEMPVAR_0, HCL_CODE_PUSH_TEMPVAR_0);
	}
}

static int compile_object (hcl_t* hcl)
{
	hcl_cframe_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_OBJECT);

	if (HCL_OOP_IS_NUMERIC(cf->operand)) goto literal;

	switch (HCL_OBJ_GET_FLAGS_BRAND(cf->operand))
	{
		case HCL_BRAND_NIL:
			EMIT_BYTE_INSTRUCTION (hcl, HCL_CODE_PUSH_NIL);
			goto done;

		case HCL_BRAND_TRUE:
			EMIT_BYTE_INSTRUCTION (hcl, HCL_CODE_PUSH_TRUE);
			goto done;

		case HCL_BRAND_FALSE:
			EMIT_BYTE_INSTRUCTION (hcl, HCL_CODE_PUSH_FALSE);
			goto done;

		case HCL_BRAND_SYMBOL:
			if (compile_symbol(hcl, cf->operand) <= -1) return -1;
			goto done;

		case HCL_BRAND_CONS:
		{
			switch (HCL_OBJ_GET_FLAGS_SYNCODE(cf->operand))
			{
				case HCL_CONCODE_ARRAY:
					if (compile_cons_array_expression(hcl, cf->operand) <= -1) return -1;
					break;

				case HCL_CONCODE_BYTEARRAY:
					if (compile_cons_bytearray_expression (hcl, cf->operand) <= -1) return -1;
					break;

				case HCL_CONCODE_DIC:
					if (compile_cons_dic_expression(hcl, cf->operand) <= -1) return -1;
					break;

				/* TODO: QLIST? */
				default:
					if (compile_cons_xlist_expression (hcl, cf->operand) <= -1) return -1;
					break;
			}
			break;
		}

		case HCL_BRAND_SYMBOL_ARRAY:
			hcl_setsynerrbfmt (
				hcl, HCL_SYNERR_VARDCLBANNED, HCL_NULL, HCL_NULL,
				"variable declaration disallowed - %O", cf->operand); /* TODO: error location */
			return -1;

		default:
			goto literal;
	}

	return 0;

literal:
	if (emit_push_literal (hcl, cf->operand) <= -1) return -1;

done:
	POP_CFRAME (hcl);
	return 0;
}

static int compile_object_list (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_oop_t coperand;
	int cop;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_OBJECT_LIST ||
	                 cf->opcode == COP_COMPILE_IF_OBJECT_LIST ||
	                 cf->opcode == COP_COMPILE_ARGUMENT_LIST ||
	                 cf->opcode == COP_COMPILE_IF_OBJECT_LIST_TAIL ||
	                 cf->opcode == COP_COMPILE_OBJECT_LIST_TAIL);

	cop = cf->opcode;
	coperand = cf->operand;

	if (HCL_IS_NIL(hcl, coperand))
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_oop_t car, cdr;

		if (cop != COP_COMPILE_ARGUMENT_LIST)
		{
			/* eliminate unnecessary non-function calls. keep the last one */
			while (HCL_IS_CONS(hcl, coperand))
			{
				cdr = HCL_CONS_CDR(coperand);
				if (HCL_IS_NIL(hcl,cdr)) break; /* keep the last one */

				if (HCL_IS_CONS(hcl, cdr))
				{
					/* look ahead */
					/* keep the last one before elif or else... */
					car = HCL_CONS_CAR(cdr);
					if (HCL_IS_SYMBOL(hcl, car) && HCL_OBJ_GET_FLAGS_SYNCODE(car)) break;
				}

				car = HCL_CONS_CAR(coperand);
				if (HCL_IS_CONS(hcl, car) || (HCL_IS_SYMBOL(hcl, car) && HCL_OBJ_GET_FLAGS_SYNCODE(car))) break;
				coperand = cdr;
			}

			HCL_ASSERT (hcl, !HCL_IS_NIL(hcl, coperand));
		}

		if (!HCL_IS_CONS(hcl, coperand))
		{
			hcl_setsynerrbfmt (
				hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL,
				"redundant cdr in the object list - %O", coperand); /* TODO: error location */
			return -1;
		}

		car = HCL_CONS_CAR(coperand);
		cdr = HCL_CONS_CDR(coperand);

		if (cop == COP_COMPILE_IF_OBJECT_LIST || cop == COP_COMPILE_IF_OBJECT_LIST_TAIL)
		{
			if (car == hcl->_elif)
			{
				SWITCH_TOP_CFRAME (hcl, COP_SUBCOMPILE_ELIF, coperand);
				goto done;
			}
			else if (car == hcl->_else)
			{
				SWITCH_TOP_CFRAME (hcl, COP_SUBCOMPILE_ELSE, coperand);
				goto done;
			}
		}

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);

		if (!HCL_IS_NIL(hcl, cdr))
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
			PUSH_CFRAME (hcl, COP_EMIT_POP_STACKTOP, hcl->_nil);
		}
	}

done:
	return 0;
}

static int compile_array_list (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_oop_t coperand;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_ARRAY_LIST);

	coperand = cf->operand;

	if (HCL_IS_NIL(hcl, coperand))
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_oop_t car, cdr;
		hcl_ooi_t oldidx;

		if (!HCL_IS_CONS(hcl, coperand))
		{
			hcl_setsynerrbfmt (
				hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, 
 				"redundant cdr in the array list - %O", coperand); /* TODO: error location */
			return -1;
		}

		car = HCL_CONS_CAR(coperand);
		cdr = HCL_CONS_CDR(coperand);

		oldidx = cf->u.array_list.index;

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);
		if (!HCL_IS_NIL(hcl, cdr))
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
	hcl_cframe_t* cf;
	hcl_oop_t coperand;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_BYTEARRAY_LIST);

	coperand = cf->operand;

	if (HCL_IS_NIL(hcl, coperand))
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_oop_t car, cdr;
		hcl_ooi_t oldidx;

		if (!HCL_IS_CONS(hcl, coperand))
		{
			hcl_setsynerrbfmt (
				hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, 
 				"redundant cdr in the byte-array list - %O", coperand); /* TODO: error location */
			return -1;
		}

		car = HCL_CONS_CAR(coperand);
		cdr = HCL_CONS_CDR(coperand);

		oldidx = cf->u.bytearray_list.index;

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);
		if (!HCL_IS_NIL(hcl, cdr))
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
	hcl_cframe_t* cf;
	hcl_oop_t coperand;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_COMPILE_DIC_LIST);

	coperand = cf->operand;

	if (HCL_IS_NIL(hcl, coperand))
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_oop_t car, cdr, cadr, cddr;

		if (!HCL_IS_CONS(hcl, coperand))
		{
			hcl_setsynerrbfmt (
				hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL, 
 				"redundant cdr in the dictionary list - %O", coperand); /* TODO: error location */
			return -1;
		}

		car = HCL_CONS_CAR(coperand);
		cdr = HCL_CONS_CDR(coperand);

		SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, car);
		if (HCL_IS_NIL(hcl, cdr))
		{
			hcl_setsynerrbfmt (
				hcl, HCL_SYNERR_UNBALKV, HCL_NULL, HCL_NULL,
				"no value for key %O", car);
			return -1;
		}
		
		cadr = HCL_CONS_CAR(cdr);
		cddr = HCL_CONS_CDR(cdr);

		if (!HCL_IS_NIL(hcl, cddr))
		{
			PUSH_SUBCFRAME (hcl, COP_COMPILE_DIC_LIST, cddr);
		}

		PUSH_SUBCFRAME (hcl, COP_EMIT_POP_INTO_DIC, HCL_SMOOI_TO_OOP(0));
		PUSH_SUBCFRAME(hcl, COP_COMPILE_OBJECT, cadr);
	}

	return 0;
}

/* ========================================================================= */

static HCL_INLINE int patch_nearest_post_if_body (hcl_t* hcl)
{
	hcl_ooi_t jump_inst_pos, body_pos;
	hcl_ooi_t jip, jump_offset;
	hcl_cframe_t* cf;

	cf = find_cframe_from_top (hcl, COP_POST_IF_BODY);
	HCL_ASSERT (hcl, cf != HCL_NULL);

	/* jump instruction position of the JUMP_FORWARD_IF_FALSE after the conditional of the previous if or elif*/
	jip = HCL_OOP_TO_SMOOI(cf->operand); 

	if (hcl->code.bc.len <= cf->u.post_if.body_pos)
	{
		/* the if body is empty. */
		if (emit_byte_instruction (hcl, HCL_CODE_PUSH_NIL) <= -1) return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	/* emit jump_forward before the beginning of the else block.
	 * this is to make the earlier if or elif block to skip
	 * the else part. it is to be patched in post_else_body(). */
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

	if (jump_offset > MAX_CODE_JUMP * 2)
	{
		HCL_DEBUG1 (hcl, "code in elif/else body too big - size %zu\n", jump_offset);
		hcl_setsynerr (hcl, HCL_SYNERR_IFFLOOD, HCL_NULL, HCL_NULL); /* error location */
		return -1;
	}
	patch_long_jump (hcl, jip, jump_offset);

	/* beginning of the elif/else block code */
	/* to drop the result of the conditional when the conditional is false */
	if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP) <= -1) return -1; 

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
	hcl_oop_t obj, cond, src;
	hcl_cframe_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_SUBCOMPILE_ELIF);

	src = cf->operand;
	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_elif);

	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
		/* no value */
		HCL_DEBUG1 (hcl, "Syntax error - no condition specified in elif - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in elif - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	cond = HCL_CONS_CAR(obj);
	obj = HCL_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, COP_POST_IF_COND, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_if.body_pos = -1; /* unknown yet */

	return patch_nearest_post_if_body (hcl);
}

static HCL_INLINE int subcompile_else (hcl_t* hcl)
{
	hcl_oop_t obj, src;
	hcl_cframe_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_SUBCOMPILE_ELSE);

	src = cf->operand;
	HCL_ASSERT (hcl, HCL_IS_CONS(hcl, src));
	HCL_ASSERT (hcl, HCL_CONS_CAR(src) == hcl->_else);

	obj = HCL_CONS_CDR(src);

	if (!HCL_IS_NIL(hcl, obj) && !HCL_IS_CONS(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in else - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj);

	return patch_nearest_post_if_body (hcl);
}

/* ========================================================================= */

static HCL_INLINE int post_if_cond (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_ooi_t jump_inst_pos;
	hcl_ooi_t body_pos;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_IF_COND);

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_inst_pos = hcl->code.bc.len;

	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_IF_FALSE, MAX_CODE_JUMP) <= -1) return -1;

	/* to drop the result of the conditional when it is true */
	if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP) <= -1) return -1; 

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
	hcl_cframe_t* cf;
	hcl_ooi_t jip;
	hcl_oow_t jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_IF_BODY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	if (hcl->code.bc.len <= cf->u.post_if.body_pos)
	{
		/* if body is empty */
		if (emit_byte_instruction (hcl, HCL_CODE_PUSH_NIL) <= -1) return -1;
	}

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD_IF_FALSE instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

	if (jump_offset > MAX_CODE_JUMP * 2)
	{
		HCL_DEBUG1 (hcl, "code in if-else body too big - size %zu\n", jump_offset);
		hcl_setsynerr (hcl, HCL_SYNERR_IFFLOOD, HCL_NULL, HCL_NULL); /* error location */
		return -1;
	}
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */
static HCL_INLINE int post_while_cond (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_ooi_t jump_inst_pos;
	hcl_ooi_t cond_pos, body_pos;
	int jump_inst, next_cop;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_POST_UNTIL_COND || cf->opcode == COP_POST_WHILE_COND);

	cond_pos = cf->u.post_while.cond_pos;
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
	if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP) <= -1) return -1;

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	body_pos = hcl->code.bc.len;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, cf->operand); /* 1 */
	PUSH_SUBCFRAME (hcl, next_cop, HCL_SMOOI_TO_OOP(jump_inst_pos)); /* 2 */
	cf = GET_SUBCFRAME(hcl);
	cf->u.post_while.cond_pos = cond_pos; 
	cf->u.post_while.body_pos = body_pos;
	return 0;
}

static HCL_INLINE int post_while_body (hcl_t* hcl)
{
	hcl_cframe_t* cf;
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
		if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP) <= -1) return -1;
	}

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX);
	jump_offset = hcl->code.bc.len - cf->u.post_while.cond_pos + 1;
	if (jump_offset > 3) jump_offset += HCL_BCODE_LONG_PARAM_SIZE;
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_BACKWARD_0, jump_offset) <= -1) return -1;

	jip = HCL_OOP_TO_SMOOI(cf->operand);
	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD_IF_FALSE/JUMP_FORWARD_IF_TRUE instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);
	if (jump_offset > MAX_CODE_JUMP * 2)
	{
		HCL_DEBUG1 (hcl, "code in loop body too big - size %zu\n", jump_offset);
		hcl_setsynerr (hcl, HCL_SYNERR_BLKFLOOD, HCL_NULL, HCL_NULL); /* error location */
		return -1;
	}
	patch_long_jump (hcl, jip, jump_offset);

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */

static int update_break (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_ooi_t jip, jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_UPDATE_BREAK);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

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
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_CALL);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_CALL_0, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_array (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_ARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_MAKE_ARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_bytearray (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_BYTEARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_MAKE_BYTEARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_make_dic (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_MAKE_DIC);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_MAKE_DIC, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_array (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_ARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_POP_INTO_ARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_bytearray (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_BYTEARRAY);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_POP_INTO_BYTEARRAY, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_pop_into_dic (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_INTO_DIC);

	n = emit_byte_instruction (hcl, HCL_CODE_POP_INTO_DIC);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_lambda (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_oow_t block_code_size;
	hcl_oow_t jip;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_LAMBDA);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	hcl->c->blk.depth--;
	hcl->c->tv.size = hcl->c->blk.tmprcnt[hcl->c->blk.depth];

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	block_code_size = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

	if (block_code_size == 0)
 	{
		/* no body in lambda - (lambda (a b c)) */
/* TODO: is this correct??? */
		if (emit_byte_instruction(hcl, HCL_CODE_PUSH_NIL) <= -1) return -1;
		block_code_size++;
	}

	if (emit_byte_instruction (hcl, HCL_CODE_RETURN_FROM_BLOCK) <= -1) return -1;
	block_code_size++;

	if (block_code_size > MAX_CODE_JUMP * 2)
	{
		HCL_DEBUG1 (hcl, "Too big a lambda block - size %zu\n", block_code_size);
		hcl_setsynerr (hcl, HCL_SYNERR_BLKFLOOD, HCL_NULL, HCL_NULL); /* error location */
		return -1;
	}
	patch_long_jump (hcl, jip, block_code_size);

	POP_CFRAME (hcl);
	return 0;
}

static HCL_INLINE int emit_pop_stacktop (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_POP_STACKTOP);
	HCL_ASSERT (hcl, HCL_IS_NIL(hcl, cf->operand));

	n = emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_return (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_RETURN);
	HCL_ASSERT (hcl, HCL_IS_NIL(hcl, cf->operand));

	n = emit_byte_instruction (hcl, HCL_CODE_RETURN_FROM_BLOCK);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_set (hcl_t* hcl)
{
	hcl_cframe_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (hcl, cf->opcode == COP_EMIT_SET);


	if (cf->u.set.var_type == VAR_NAMED)
	{
		hcl_oow_t index;
		hcl_oop_t cons;

		HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, cf->operand));

		cons = (hcl_oop_t)hcl_getatsysdic (hcl, cf->operand);
		if (!cons) 
		{
			cons = (hcl_oop_t)hcl_putatsysdic (hcl, cf->operand, hcl->_nil);
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
		if (emit_indexed_variable_access (hcl, index, HCL_CODE_STORE_INTO_CTXTEMPVAR_0, HCL_CODE_STORE_INTO_TEMPVAR_0) <= -1) return -1;
	}

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */


int hcl_compile (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oow_t saved_bc_len, saved_lit_len;
	int log_default_type_mask;

	HCL_ASSERT (hcl, GET_TOP_CFRAME_INDEX(hcl) < 0);

	saved_bc_len = hcl->code.bc.len;
	saved_lit_len = hcl->code.lit.len;

	log_default_type_mask = hcl->log.default_type_mask;
	hcl->log.default_type_mask |= HCL_LOG_COMPILER;

	HCL_ASSERT (hcl, hcl->c->tv.size == 0);
	HCL_ASSERT (hcl, hcl->c->blk.depth == -1);

/* TODO: in case i implement all global variables as block arguments at the top level...what should i do? */
	hcl->c->blk.depth++;
	if (store_temporary_variable_count_for_block(hcl, hcl->c->tv.size) <= -1) return -1;

	PUSH_CFRAME (hcl, COP_COMPILE_OBJECT, obj);

	while (GET_TOP_CFRAME_INDEX(hcl) >= 0)
	{
		hcl_cframe_t* cf;

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

			case COP_EMIT_POP_INTO_ARRAY:
				if (emit_pop_into_array(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_BYTEARRAY:
				if (emit_pop_into_bytearray(hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_INTO_DIC:
				if (emit_pop_into_dic(hcl) <= -1) goto oops;
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
	if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP) <= -1) goto oops;

	HCL_ASSERT (hcl, GET_TOP_CFRAME_INDEX(hcl) < 0);
	HCL_ASSERT (hcl, hcl->c->tv.size == 0);
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

	hcl->c->tv.size = 0;
	hcl->c->blk.depth = -1;
	return -1;
}
