/*
 * $Id$
 *
    Copyright (c) 2014-2016 Chung, Hyung-Hwan. All rights reserved.

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

	HCL_ASSERT (HCL_IS_SYMBOL (hcl, name));

	for (i = dup_check_start; i < hcl->c->tv.size; i++)
	{
		HCL_ASSERT (HCL_IS_SYMBOL (hcl, hcl->c->tv.ptr[i]));
		if (hcl->c->tv.ptr[i] == name)
		{
			hcl->errnum = HCL_EEXIST;
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

	HCL_ASSERT (HCL_IS_SYMBOL (hcl, name));
	for (i = hcl->c->tv.size; i > 0; )
	{
		--i;
		HCL_ASSERT (HCL_IS_SYMBOL (hcl, hcl->c->tv.ptr[i]));
		if (hcl->c->tv.ptr[i] == name)
		{
			*index = i;
			return 0;
		}
	}
	
	hcl->errnum = HCL_ENOENT;
	return -1;
}

static int store_temporary_variable_count_for_block (hcl_t* hcl, hcl_oow_t tmpr_count)
{
	HCL_ASSERT (hcl->c->blk.depth >= 0);

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
	HCL_ASSERT (index < hcl->code.bc.len);
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
		hcl->errnum = HCL_EBCFULL; /* byte code full/too big */
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
			bc = cmd;
			goto write_long;
	}

	hcl->errnum = HCL_EINVAL;
	return -1;

write_short:
	if (emit_byte_instruction(hcl, bc) <= -1) return -1;
	return 0;

write_long:
	if (param_1 > MAX_CODE_PARAM) 
	{
		hcl->errnum = HCL_ETOOBIG;
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
		hcl->errnum = HCL_ETOOBIG;
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

	hcl->errnum = HCL_EINVAL;
	return -1;

write_short:
	if (emit_byte_instruction(hcl, bc) <= -1 ||
	    emit_byte_instruction(hcl, param_2) <= -1) return -1;
	return 0;

write_long:
	if (param_1 > MAX_CODE_PARAM || param_2 > MAX_CODE_PARAM)
	{
		hcl->errnum = HCL_ETOOBIG;
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
		hcl->errnum = HCL_ETOOBIG;
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

/* ========================================================================= */
static HCL_INLINE int _insert_cframe (hcl_t* hcl, hcl_ooi_t index, int opcode, hcl_oop_t operand)
{
	hcl_cframe_t* tmp;

	HCL_ASSERT (index >= 0);
	
	hcl->c->cfs.top++;
	HCL_ASSERT (hcl->c->cfs.top >= 0);
	HCL_ASSERT (index <= hcl->c->cfs.top);

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
		hcl->errnum = HCL_ETOOBIG;
		return -1;
	}

	return _insert_cframe (hcl, index, opcode, operand);
}

static int push_cframe (hcl_t* hcl, int opcode, hcl_oop_t operand)
{
	if (hcl->c->cfs.top == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl->errnum = HCL_ETOOBIG;
		return -1;
	}

	return _insert_cframe (hcl, hcl->c->cfs.top + 1, opcode, operand);
}

static HCL_INLINE void pop_cframe (hcl_t* hcl)
{
	HCL_ASSERT (hcl->c->cfs.top >= 0);
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

#define PUSH_SUBCFRAME(hcl,opcode,operand) \
	do { if (push_subcframe(hcl,opcode,operand) <= -1) return -1; } while(0)

#define GET_SUBCFRAME(hcl) (&hcl->c->cfs.ptr[hcl->c->cfs.top - 1])

enum 
{
	COP_COMPILE_OBJECT,
	COP_COMPILE_OBJECT_LIST,
	COP_COMPILE_ARGUMENT_LIST,

	COP_EMIT_CALL,
	COP_EMIT_LAMBDA,
	COP_EMIT_POP_STACKTOPP,
	COP_EMIT_RETURN,
	COP_EMIT_SET,

	COP_POST_UNTIL_BODY,
	COP_POST_UNTIL_COND,
	COP_POST_WHILE_BODY,
	COP_POST_WHILE_COND,

	COP_UPDATE_BREAK,
};

/* ========================================================================= */

static int compile_break (hcl_t* hcl, hcl_oop_t src)
{
	hcl_oop_t obj;
	hcl_ooi_t i;

	HCL_ASSERT (HCL_BRANDOF(hcl,src) == HCL_BRAND_CONS);
	HCL_ASSERT (HCL_CONS_CAR(src) == hcl->_break);

	obj = HCL_CONS_CDR(src);
	if (!HCL_IS_NIL(hcl,obj))
	{
		if (HCL_IS_CONS(hcl,obj))
		{
			HCL_DEBUG1 (hcl, "Syntax error - redundant argument in break - %O\n", src);
			hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
		}
		else
		{
			HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in break - %O\n", src);
			hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
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
			HCL_ASSERT (hcl->code.bc.len < HCL_SMOOI_MAX);
			jump_inst_pos = hcl->code.bc.len;
			
			if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;
			INSERT_CFRAME (hcl, i, COP_UPDATE_BREAK, HCL_SMOOI_TO_OOP(jump_inst_pos));

			POP_CFRAME (hcl);
			return 0;
		}
	}

	HCL_DEBUG1 (hcl, "Syntax error - break outside loop - %O\n", src);
	hcl_setsynerr (hcl, HCL_SYNERR_BREAK, HCL_NULL, HCL_NULL); /* TODO: error location */
	return -1;
}

static int compile_if (hcl_t* hcl, hcl_oop_t src)
{
/* TODO: NOT IMPLEMENTED */
	return -1;
}

static int compile_lambda (hcl_t* hcl, hcl_oop_t src)
{
	hcl_oop_t obj, args;
	hcl_oow_t nargs, ntmprs;
	hcl_ooi_t jump_inst_pos;
	hcl_oow_t saved_tv_count, tv_dup_start;

	HCL_ASSERT (HCL_BRANDOF(hcl,src) == HCL_BRAND_CONS);
	HCL_ASSERT (HCL_CONS_CAR(src) == hcl->_lambda);

	saved_tv_count = hcl->c->tv.size;
	obj = HCL_CONS_CDR(src);

	if (HCL_IS_NIL(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Syntax error - no argument list in lambda - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGNAMELIST, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}
	else if (!HCL_IS_CONS(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in lambda - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
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
			HCL_DEBUG1 (hcl, "Syntax error - not a lambda argument list - %O\n", args);
			hcl_setsynerr (hcl, HCL_SYNERR_ARGNAMELIST, HCL_NULL, HCL_NULL); /* TODO: error location */
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
				HCL_DEBUG1 (hcl, "Syntax error - lambda argument not a symbol - %O\n", arg);
				hcl_setsynerr (hcl, HCL_SYNERR_ARGNAME, HCL_NULL, HCL_NULL); /* TODO: error location */
				return -1;
			}

			if (HCL_OBJ_GET_FLAGS_SYNCODE(arg))
			{
				HCL_DEBUG1 (hcl, "Syntax error - special symbol not to be declared as an argument name - %O\n", arg); 
				hcl_setsynerr (hcl, HCL_SYNERR_BANNEDARGNAME, HCL_NULL, HCL_NULL); /* TOOD: error location */
				return -1;
			}

	/* TODO: check duplicates within only the argument list. duplicates against outer-scope are ok.
	 * is this check necessary? */

			if (add_temporary_variable (hcl, arg, tv_dup_start) <= -1) 
			{
				if (hcl->errnum == HCL_EEXIST)
				{
					HCL_DEBUG1 (hcl, "Syntax error - lambda argument duplicate - %O\n", arg);
					hcl_setsynerr (hcl, HCL_SYNERR_ARGNAMEDUP, HCL_NULL, HCL_NULL); /* TODO: error location */
				}
				return -1;
			}
			nargs++;

			ptr = HCL_CONS_CDR(ptr);
			if (!HCL_IS_CONS(hcl, ptr)) 
			{
				if (!HCL_IS_NIL(hcl, ptr))
				{
					HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in lambda argument list - %O\n", args);
					hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
					return -1;
				}
				break;
			}
		}
		while (1);
	}

	HCL_ASSERT (nargs == hcl->c->tv.size - saved_tv_count);
	if (nargs > MAX_CODE_NBLKARGS) /*TODO: change this limit to max call argument count */
	{
		/* while an integer object is pused to indicate the number of
		 * block arguments, evaluation which is done by message passing
		 * limits the number of arguments that can be passed. so the
		 * check is implemented */
		HCL_DEBUG1 (hcl, "Syntax error - too many arguments - %O\n", args);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGFLOOD, HCL_NULL, HCL_NULL); 
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
				if (HCL_OBJ_GET_FLAGS_SYNCODE(((hcl_oop_oop_t)dcl)->slot[i]))
				{
					HCL_DEBUG1 (hcl, "Syntax error - special symbol not to be declared as a variable name - %O\n", obj); 
					hcl_setsynerr (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL); /* TOOD: error location */
					return -1;
				}

				if (add_temporary_variable (hcl, ((hcl_oop_oop_t)dcl)->slot[i], tv_dup_start) <= -1) 
				{
					if (hcl->errnum == HCL_EEXIST)
					{
						HCL_DEBUG1 (hcl, "Syntax error - local variable duplicate - %O\n", ((hcl_oop_oop_t)dcl)->slot[i]);
						hcl_setsynerr (hcl, HCL_SYNERR_VARNAMEDUP, HCL_NULL, HCL_NULL); /* TODO: error location */
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
	HCL_ASSERT (ntmprs == hcl->c->tv.size - saved_tv_count);
	if (ntmprs > MAX_CODE_NBLKTMPRS)
	{
		HCL_DEBUG1 (hcl, "Syntax error - too many variables - %O\n", args);
		hcl_setsynerr (hcl, HCL_SYNERR_VARFLOOD, HCL_NULL, HCL_NULL); 
		return -1;
	}

	if (hcl->c->blk.depth == HCL_TYPE_MAX(hcl_ooi_t))
	{
		HCL_DEBUG1 (hcl, "Syntax error - lambda block depth too deep - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_BLKDEPTH, HCL_NULL, HCL_NULL); 
		return -1;
	}
	hcl->c->blk.depth++;
	if (store_temporary_variable_count_for_block (hcl, hcl->c->tv.size) <= -1) return -1;

	/* use the accumulated number of temporaries so far when generating
	 * the make_block instruction. at context activation time, the actual 
	 * count of temporaries for this block is derived by subtracting the 
	 * count of temporaries in the home context */
	if (emit_double_param_instruction (hcl, HCL_CODE_MAKE_BLOCK, nargs, hcl->c->tv.size/*ntmprs*/) <= -1) return -1;

	HCL_ASSERT (hcl->code.bc.len < HCL_SMOOI_MAX);  /* guaranteed in emit_byte_instruction() */
	jump_inst_pos = hcl->code.bc.len;
	/* specifying MAX_CODE_JUMP causes emit_single_param_instruction() to 
	 * produce the long jump instruction (BCODE_JUMP_FORWARD_X) */
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, obj);
	PUSH_SUBCFRAME (hcl, COP_EMIT_LAMBDA, HCL_SMOOI_TO_OOP(jump_inst_pos));

	return 0;
}


static int compile_return (hcl_t* hcl, hcl_oop_t src)
{
	hcl_oop_t obj, val;

	obj = HCL_CONS_CDR(src);

	HCL_ASSERT (HCL_BRANDOF(hcl,src) == HCL_BRAND_CONS);
	HCL_ASSERT (HCL_CONS_CAR(src) == hcl->_return);

	if (HCL_IS_NIL(hcl, obj))
	{
/* TODO: should i allow (return)? does it return the last value on the stack? */
		/* no value */
		HCL_DEBUG1 (hcl, "Syntax error - no value specified in return - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}
	else if (HCL_BRANDOF(hcl, obj) != HCL_BRAND_CONS)
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in return - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	val = HCL_CONS_CAR(obj);

	obj = HCL_CONS_CDR(obj);
	if (!HCL_IS_NIL(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Synatx error - too many arguments to return - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
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

	obj = HCL_CONS_CDR(src);

	HCL_ASSERT (HCL_BRANDOF(hcl,src) == HCL_BRAND_CONS);
	HCL_ASSERT (HCL_CONS_CAR(src) == hcl->_set);

	if (HCL_IS_NIL(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Syntax error - no variable name in set - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_VARNAME, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}
	else if (HCL_BRANDOF(hcl, obj) != HCL_BRAND_CONS)
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in set - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	var = HCL_CONS_CAR(obj);
	if (HCL_BRANDOF(hcl, var) != HCL_BRAND_SYMBOL)
	{
		HCL_DEBUG1 (hcl, "Syntax error - variable name not a symbol - %O\n", var);
		hcl_setsynerr (hcl, HCL_SYNERR_VARNAME, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	if (HCL_OBJ_GET_FLAGS_SYNCODE(var))
	{
		HCL_DEBUG1 (hcl, "Syntax error - special symbol not to be used as a variable name - %O\n", var); 
		hcl_setsynerr (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL); /* TOOD: error location */
		return -1;
	}

	obj = HCL_CONS_CDR(obj);
	if (HCL_IS_NIL(hcl, obj))
	{
		/* no value */
		HCL_DEBUG1 (hcl, "Syntax error - no value specified in set - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}
	else if (HCL_BRANDOF(hcl, obj) != HCL_BRAND_CONS)
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in set - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	val = HCL_CONS_CAR(obj);

	obj = HCL_CONS_CDR(obj);
	if (!HCL_IS_NIL(hcl, obj))
	{
		HCL_DEBUG1 (hcl, "Synatx error - too many arguments to set - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, val);

	if (find_temporary_variable_backward (hcl, var, &index) <= -1)
	{
		PUSH_SUBCFRAME (hcl, COP_EMIT_SET, var); /* set doesn't evaluate the variable name */
		cf = GET_SUBCFRAME (hcl);
		cf->u.set.var_type = VAR_NAMED;
	}
	else
	{
		/* the check in compile_lambda() must ensure this condition */
		HCL_ASSERT (index <= HCL_SMOOI_MAX); 

		PUSH_SUBCFRAME (hcl, COP_EMIT_SET, HCL_SMOOI_TO_OOP(index)); 
		cf = GET_SUBCFRAME (hcl);
		cf->u.set.var_type = VAR_INDEXED;
	}

	return 0;
}

static int compile_while (hcl_t* hcl, hcl_oop_t src, int next_cop)
{
	/* (while (xxxx) ... ) */
	hcl_oop_t obj, cond;
	hcl_oow_t cond_pos;
	hcl_cframe_t* cf;

	obj = HCL_CONS_CDR(src);

	HCL_ASSERT (HCL_BRANDOF(hcl,src) == HCL_BRAND_CONS);
	HCL_ASSERT (HCL_CONS_CAR(src) == hcl->_until || HCL_CONS_CAR(src) == hcl->_while);
	HCL_ASSERT (next_cop == COP_POST_UNTIL_COND || next_cop == COP_POST_WHILE_COND);

	if (HCL_IS_NIL(hcl, obj))
	{
		/* no value */
		HCL_DEBUG1 (hcl, "Syntax error - no condition specified in while - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_ARGCOUNT, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}
	else if (HCL_BRANDOF(hcl, obj) != HCL_BRAND_CONS)
	{
		HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in while - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
		return -1;
	}

	cond_pos = hcl->code.bc.len; /* position where the bytecode for the conditional is emitted */
	HCL_ASSERT (cond_pos < HCL_SMOOI_MAX);

	cond = HCL_CONS_CAR(obj);
	obj = HCL_CONS_CDR(obj);

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT, cond); /* 1 */
	PUSH_SUBCFRAME (hcl, next_cop, obj); /* 2 */
	cf = GET_SUBCFRAME (hcl);
	cf->u.post_while.cond_pos = cond_pos;
	cf->u.post_while.body_pos = 0; /* unknown yet*/

	return 0;
}
/* ========================================================================= */


static int compile_cons_expression (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oop_t car;
	int syncode;

	HCL_ASSERT (HCL_BRANDOF(hcl,obj) == HCL_BRAND_CONS);

	car = HCL_CONS_CAR(obj);
	if (HCL_BRANDOF(hcl,car) == HCL_BRAND_SYMBOL && (syncode = HCL_OBJ_GET_FLAGS_SYNCODE(car)))
	{
		switch (syncode)
		{
			case HCL_SYNCODE_BEGIN:
HCL_DEBUG0 (hcl, "BEGIN NOT IMPLEMENTED...\n");
/* TODO: not implemented yet */
				break;

			case HCL_SYNCODE_BREAK:
				/* break */
				if (compile_break (hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_DEFUN:
HCL_DEBUG0 (hcl, "DEFUN NOT IMPLEMENTED...\n");
/* TODO: not implemented yet */
				break;

			case HCL_SYNCODE_IF:
				if (compile_if (hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_LAMBDA:
				/* (lambda (x y) (+ x y)) */
				if (compile_lambda (hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_SET:
				/* (set x 10) 
				 * (set x (lambda (x y) (+ x y)) */
				if (compile_set (hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_RETURN:
				/* (return 10)
				 * (return (+ 10 20)) */
				if (compile_return (hcl, obj) <= -1) return -1;
				break;

			case HCL_SYNCODE_UNTIL:
				if (compile_while (hcl, obj, COP_POST_UNTIL_COND) <= -1) return -1;
				break;

			case HCL_SYNCODE_WHILE:
				if (compile_while (hcl, obj, COP_POST_WHILE_COND) <= -1) return -1;
				break;

			default:
				HCL_DEBUG3 (hcl, "Internal error - unknown syncode %d at %s:%d\n", syncode, __FILE__, __LINE__);
				hcl->errnum = HCL_EINTERN;
				return -1;
		}
	}
	else
	{
		/* normal function call 
		 *  (<operator> <operand1> ...) */
		hcl_ooi_t nargs;
		hcl_ooi_t oldtop;
		hcl_cframe_t* cf;
		hcl_oop_t cdr;

		/* NOTE: cframe management functions don't use the object memory.
		 *       many operations can be performed without taking GC into account */

		oldtop = GET_TOP_CFRAME_INDEX(hcl);
		HCL_ASSERT (oldtop >= 0);

		SWITCH_TOP_CFRAME (hcl, COP_EMIT_CALL, HCL_SMOOI_TO_OOP(0));

		/* compile <operator> */
		PUSH_CFRAME (hcl, COP_COMPILE_OBJECT, car);
/* TODO: do pre-filtering. if car is a literal, it's not a valid function call - this can also be check in the reader.
 *       if it's a symbol and it evaluates to a literal, it can only be caught in the runtime  
* this check along with the .cdr check, can be done in the reader if i create a special flag (e.g. QUOTED) applicable to CONS.
* what happens if someone likes to manipulate the list as the list is not a single object type unlike array???
*     (define (x y) (10 20 30))
*/

		/* compile <operand1> ... etc */
		cdr = HCL_CONS_CDR(obj);
		if (HCL_IS_NIL(hcl, cdr)) 
		{
			nargs = 0;
		}
		else
		{
			if (HCL_BRANDOF(hcl, cdr) != HCL_BRAND_CONS)
			{
				HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in function call - %O\n", obj);
				hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
				return -1;
			}

			nargs = hcl_countcons (hcl, cdr);
			if (nargs > MAX_CODE_PARAM) 
			{
				hcl->errnum = HCL_ETOOBIG; /* TODO: change the error code to a better one */
				return -1;
			}
		}
		/* redundant cdr check is performed inside compile_object_list() */
		PUSH_SUBCFRAME (hcl, COP_COMPILE_ARGUMENT_LIST, cdr);

		/* patch the argument count in the operand field of the COP_EMIT_CALL frame */
		cf = GET_CFRAME(hcl, oldtop);
		HCL_ASSERT (cf->opcode == COP_EMIT_CALL);
		cf->operand = HCL_SMOOI_TO_OOP(nargs);
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
		HCL_ASSERT (index < hcl->c->blk.tmprcnt[hcl->c->blk.depth]);
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

	HCL_ASSERT (HCL_BRANDOF(hcl,obj) == HCL_BRAND_SYMBOL);

	if (HCL_OBJ_GET_FLAGS_SYNCODE(obj))
	{
		HCL_DEBUG1 (hcl, "Syntax error - special symbol not to be used as a variable name - %O\n", obj); 
		hcl_setsynerr (hcl, HCL_SYNERR_BANNEDVARNAME, HCL_NULL, HCL_NULL); /* TOOD: error location */
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
	HCL_ASSERT (cf->opcode == COP_COMPILE_OBJECT);

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
			if (compile_cons_expression (hcl, cf->operand) <= -1) return -1;
			break;

		case HCL_BRAND_SYMBOL_ARRAY:
			HCL_DEBUG1 (hcl, "Syntax error - variable declartion disallowed - %O\n", cf->operand);
			hcl_setsynerr (hcl, HCL_SYNERR_VARDCLBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
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

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_COMPILE_OBJECT_LIST ||
	            cf->opcode == COP_COMPILE_ARGUMENT_LIST);

	if (HCL_IS_NIL(hcl, cf->operand))
	{
		POP_CFRAME (hcl);
	}
	else
	{
		hcl_oop_t car, cdr;
		int cop;

		if (!HCL_IS_CONS(hcl, cf->operand))
		{
			HCL_DEBUG1 (hcl, "Syntax error - redundant cdr in the object list - %O\n", cf->operand);
			hcl_setsynerr (hcl, HCL_SYNERR_DOTBANNED, HCL_NULL, HCL_NULL); /* TODO: error location */
			return -1;
		}

		cop = cf->opcode;
		car = HCL_CONS_CAR(cf->operand);
		cdr = HCL_CONS_CDR(cf->operand);
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
			 */
			PUSH_SUBCFRAME (hcl, cop, cdr);
			if (cop == COP_COMPILE_OBJECT_LIST)
			{
				/* let's arrange to emit POP before generating code for the rest of the list */
				hcl_oop_t tmp;
				/* look ahead for some special functions */
				tmp = HCL_CONS_CAR(cdr);
				if (!HCL_IS_CONS(hcl, tmp) || HCL_CONS_CAR(tmp) != hcl->_break) /* TODO: other special forms??? */
					PUSH_SUBCFRAME (hcl, COP_EMIT_POP_STACKTOPP, hcl->_nil);
			}
		}
	}

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
	HCL_ASSERT (cf->opcode == COP_POST_UNTIL_COND || cf->opcode == COP_POST_WHILE_COND);

	cond_pos = cf->u.post_while.cond_pos;
	HCL_ASSERT (hcl->code.bc.len < HCL_SMOOI_MAX);
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

	HCL_ASSERT (hcl->code.bc.len < HCL_SMOOI_MAX);
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
	hcl_oow_t jump_offset, code_size;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_POST_UNTIL_BODY || cf->opcode == COP_POST_WHILE_BODY);
	HCL_ASSERT (HCL_OOP_IS_SMOOI(cf->operand));

	HCL_ASSERT (hcl->code.bc.len >= cf->u.post_while.cond_pos);
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

	jump_offset = hcl->code.bc.len - cf->u.post_while.cond_pos + 1;
	if (jump_offset > 3) jump_offset += HCL_BCODE_LONG_PARAM_SIZE;
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_BACKWARD_0, jump_offset) <= -1) return -1;

	jip = HCL_OOP_TO_SMOOI(cf->operand);
	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD_IF_FALSE/JUMP_FORWARD_IF_TRUE instruction */
	code_size = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

	if (code_size > MAX_CODE_JUMP)
	{
		/* switch to JUMP2 instruction to allow a bigger jump offset.
		 * up to twice MAX_CODE_JUMP only */
		patch_instruction (hcl, jip, ((cf->opcode == COP_POST_UNTIL_BODY)? HCL_CODE_JUMP2_FORWARD_IF_TRUE: HCL_CODE_JUMP2_FORWARD_IF_FALSE)); 
		jump_offset = code_size - MAX_CODE_JUMP;
	}
	else
	{
		jump_offset = code_size;
	}

#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	patch_instruction (hcl, jip + 1, jump_offset >> 8);
	patch_instruction (hcl, jip + 2, jump_offset & 0xFF);
	#else
	patch_instruction (hcl, jip + 1, jump_offset);
#endif

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */

static int update_break (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_ooi_t jip, jump_offset;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_UPDATE_BREAK);
	HCL_ASSERT (HCL_OOP_IS_SMOOI(cf->operand));

	jip = HCL_OOP_TO_SMOOI(cf->operand);

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	jump_offset = hcl->code.bc.len - jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

	if (jump_offset > MAX_CODE_JUMP)
	{
		/* switch to JUMP2 instruction to allow a bigger jump offset.
		 * up to twice MAX_CODE_JUMP only */
		patch_instruction (hcl, jip, HCL_CODE_JUMP2_FORWARD); 
		jump_offset -= MAX_CODE_JUMP;
	}

#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
	patch_instruction (hcl, jip + 1, jump_offset >> 8);
patch_instruction (hcl, jip + 2, jump_offset & 0xFF);
	#else
	patch_instruction (hcl, jip + 1, jump_offset);
#endif

	POP_CFRAME (hcl);
	return 0;
}

/* ========================================================================= */

static HCL_INLINE int emit_call (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_CALL);
	HCL_ASSERT (HCL_OOP_IS_SMOOI(cf->operand));

	n = emit_single_param_instruction (hcl, HCL_CODE_CALL_0, HCL_OOP_TO_SMOOI(cf->operand));

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_lambda (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_oow_t block_code_size;
	hcl_oow_t jip;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_LAMBDA);
	HCL_ASSERT (HCL_OOP_IS_SMOOI(cf->operand));

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
	else 
	{
		hcl_oow_t jump_offset;

		if (block_code_size > MAX_CODE_JUMP)
		{
			/* switch to JUMP2 instruction to allow a bigger jump offset.
			 * up to twice MAX_CODE_JUMP only */
			patch_instruction (hcl, jip, HCL_CODE_JUMP2_FORWARD);
			jump_offset = block_code_size - MAX_CODE_JUMP;
		}
		else
		{
			jump_offset = block_code_size;
		}

	#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
		patch_instruction (hcl, jip + 1, jump_offset >> 8);
		patch_instruction (hcl, jip + 2, jump_offset & 0xFF);
	#else
		patch_instruction (hcl, jip + 1, jump_offset);
	#endif
	}

	POP_CFRAME (hcl);
	return 0;
}

static HCL_INLINE int emit_pop_stacktop (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_POP_STACKTOPP);
	HCL_ASSERT (HCL_IS_NIL(hcl, cf->operand));

	n = emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_return (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_RETURN);
	HCL_ASSERT (HCL_IS_NIL(hcl, cf->operand));

	n = emit_byte_instruction (hcl, HCL_CODE_RETURN_FROM_BLOCK);

	POP_CFRAME (hcl);
	return n;
}

static HCL_INLINE int emit_set (hcl_t* hcl)
{
	hcl_cframe_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_SET);


	if (cf->u.set.var_type == VAR_NAMED)
	{
		hcl_oow_t index;
		hcl_oop_t cons;

		HCL_ASSERT (HCL_IS_SYMBOL(hcl, cf->operand));

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
		HCL_ASSERT (cf->u.set.var_type == VAR_INDEXED);
		HCL_ASSERT (HCL_OOP_IS_SMOOI(cf->operand));

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

	HCL_ASSERT (GET_TOP_CFRAME_INDEX(hcl) < 0);

	saved_bc_len = hcl->code.bc.len;
	saved_lit_len = hcl->code.lit.len;

	HCL_ASSERT (hcl->c->tv.size == 0);
	HCL_ASSERT (hcl->c->blk.depth == -1);

/* TODO: in case i implement all global variables as block arguments at the top level... */
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
				if (compile_object (hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_OBJECT_LIST:
			case COP_COMPILE_ARGUMENT_LIST:
				if (compile_object_list (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_CALL:
				if (emit_call (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_LAMBDA:
				if (emit_lambda (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP_STACKTOPP:
				if (emit_pop_stacktop (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_RETURN:
				if (emit_return (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_SET:
				if (emit_set (hcl) <= -1) goto oops;
				break;

			case COP_POST_UNTIL_BODY:
			case COP_POST_WHILE_BODY:
				if (post_while_body (hcl) <= -1) goto oops;
				break;

			case COP_POST_UNTIL_COND:
			case COP_POST_WHILE_COND:
				if (post_while_cond (hcl) <= -1) goto oops;
				break;

			case COP_UPDATE_BREAK:
				if (update_break (hcl) <= -1) goto oops;
				break;

			default:
				HCL_DEBUG1 (hcl, "Internal error - invalid compiler opcode %d\n", cf->opcode);
				hcl->errnum = HCL_EINTERN;
				goto oops;
		}
	}

	/* emit the pop instruction to clear the final result */
/* TODO: for interactive use, this value must be accessible by the executor... how to do it? */
	if (emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP) <= -1) goto oops;

	HCL_ASSERT (GET_TOP_CFRAME_INDEX(hcl) < 0);
	HCL_ASSERT (hcl->c->tv.size == 0);
	HCL_ASSERT (hcl->c->blk.depth == 0);
	hcl->c->blk.depth--;
	return 0;

oops:
	POP_ALL_CFRAMES (hcl);

	/* rollback any bytecodes or literals emitted so far */
	hcl->code.bc.len = saved_bc_len;
	hcl->code.lit.len = saved_lit_len;

	hcl->c->tv.size = 0;
	hcl->c->blk.depth = -1;
	return -1;
}
