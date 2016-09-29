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
	VAR_ARGUMENT
};

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
		tmp = hcl_remakengcarray (hcl, hcl->code.lit.arr, newcapa);
		if (!tmp) return -1;

		hcl->code.lit.arr = tmp;
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
	((hcl_oop_byte_t)hcl->code.bc.arr)->slot[index] = bc;
}

static int emit_byte_instruction (hcl_t* hcl, hcl_oob_t bc)
{
	hcl_oow_t capa;

	capa = HCL_OBJ_GET_SIZE(hcl->code.bc.arr);
	if (hcl->code.bc.len >= capa)
	{
		hcl_oop_t tmp;
		hcl_oow_t newcapa;

		newcapa = capa + 20000; /* TODO: set a better resizing policy */
		tmp = hcl_remakengcbytearray (hcl, hcl->code.bc.arr, newcapa);
		if (!tmp) return -1;

		hcl->code.bc.arr = tmp;
	}

	((hcl_oop_byte_t)hcl->code.bc.arr)->slot[hcl->code.bc.len++] = bc;
	return 0;
}


static int emit_single_param_instruction (hcl_t* hcl, int cmd, hcl_oow_t param_1)
{
	hcl_oob_t bc;

	switch (cmd)
	{
#if 0
		case BCODE_PUSH_INSTVAR_0:
		case BCODE_STORE_INTO_INSTVAR_0:
		case BCODE_POP_INTO_INSTVAR_0:
#endif
		case HCL_CODE_PUSH_TEMPVAR_0:
#if 0
		case BCODE_STORE_INTO_TEMPVAR_0:
		case BCODE_POP_INTO_TEMPVAR_0:
#endif
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
#if 0
		case HCL_CODE_JUMP_IF_TRUE_0:
		case HCL_CODE_JUMP_IF_FALSE_0:
#endif
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
		/*case BCODE_POP_INTO_CTXTEMPVAR_0:*/
		case HCL_CODE_PUSH_CTXTEMPVAR_0:
#if 0
		case HCL_CODE_PUSH_OBJVAR_0:
		case HCL_CODE_STORE_INTO_OBJVAR_0:
		case BCODE_POP_INTO_OBJVAR_0:
		case HCL_CODE_SEND_MESSAGE_0:
		case HCL_CODE_SEND_MESSAGE_TO_SUPER_0:
#endif
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

static int push_cframe (hcl_t* hcl, int opcode, hcl_oop_t operand)
{
	hcl_cframe_t* tmp;

	if (hcl->c->cfs.top == HCL_TYPE_MAX(hcl_ooi_t))
	{
		hcl->errnum = HCL_ETOOBIG;
		return -1;
	}

	hcl->c->cfs.top++;
	HCL_ASSERT (hcl->c->cfs.top >= 0);

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

	tmp = &hcl->c->cfs.ptr[hcl->c->cfs.top];
	tmp->opcode = opcode;
	tmp->operand = operand;
	/* leave tmp->u untouched/uninitialized */
	return 0;
}

static HCL_INLINE void pop_cframe (hcl_t* hcl)
{
	HCL_ASSERT (hcl->c->cfs.top >= 0);
	hcl->c->cfs.top--;
}

#define PUSH_CFRAME(hcl,opcode,operand) \
	do { if (push_cframe(hcl,opcode,operand) <= -1) return -1; } while(0)

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
	COP_EXIT,
	COP_COMPILE_OBJECT,
	COP_COMPILE_OBJECT_LIST,
	COP_COMPILE_ARGUMENT_LIST,
	COP_EMIT_POP,
	COP_EMIT_CALL,
	COP_EMIT_LAMBDA,
	COP_EMIT_SET
};

/* ========================================================================= */

static int compile_lambda (hcl_t* hcl, hcl_oop_t src)
{
	hcl_cframe_t* cf;
	hcl_oop_t obj, args, arg, ptr;
	hcl_oow_t nargs, ntmprs;
	hcl_oow_t jump_inst_pos;
	hcl_oow_t saved_tv_count;

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
	else if (HCL_BRANDOF(hcl, obj) != HCL_BRAND_CONS)
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
		hcl_oow_t tv_dup_start;
		if (HCL_BRANDOF(hcl, args) != HCL_BRAND_CONS)
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
			if (HCL_BRANDOF(hcl, arg) != HCL_BRAND_SYMBOL)
			{
				HCL_DEBUG1 (hcl, "Syntax error - lambda argument not a symbol - %O\n", arg);
				hcl_setsynerr (hcl, HCL_SYNERR_ARGNAME, HCL_NULL, HCL_NULL); /* TODO: error location */
				return -1;
			}
	/* TODO: check duplicates within only the argument list. duplicates against outer-scope are ok.
	 * is this check necessary? */

			if (add_temporary_variable (hcl, arg, tv_dup_start) <= -1) 
			{
				if (hcl->errnum == HCL_EEXIST)
				{
					HCL_DEBUG1 (hcl, "Syntax error - lambda argument duplicate - %O\n", arg);
					hcl_setsynerr (hcl, HCL_SYNERR_ARGNAME, HCL_NULL, HCL_NULL); /* TODO: error location */
					return -1;
				}
				return -1;
			}
			nargs++;

			ptr = HCL_CONS_CDR(ptr);
			if (HCL_BRANDOF(hcl, ptr) != HCL_BRAND_CONS) 
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

	ntmprs = nargs;  
/* TODO: handle local temporary variables */

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

#if 0
/* TODO: block local temporary variables... */
	/* ntmprs: number of temporary variables including arguments */
	HCL_ASSERT (ntmprs == hcl->c->tv.size - saved_tv_count);
	if (ntmprs > MAX_CODE_NBLKTMPRS)
	{
		HCL_DEBUG1 (hcl, "Syntax error - too many local temporary variables - %O\n", args);
		hcl_setsynerr (hcl, HCL_SYNERR_BLKTMPRFLOOD, HCL_NULL, HCL_NULL); 
		return -1;
	}
#endif

	if (hcl->c->blk.depth == HCL_TYPE_MAX(hcl_ooi_t))
	{
		HCL_DEBUG1 (hcl, "Syntax error - lambda block depth too deep - %O\n", src);
		hcl_setsynerr (hcl, HCL_SYNERR_BLKDEPTH, HCL_NULL, HCL_NULL); 
		return -1;
	}
	hcl->c->blk.depth++;
	if (store_temporary_variable_count_for_block (hcl, hcl->c->tv.size) <= -1) return -1;

	if (emit_double_param_instruction (hcl, HCL_CODE_MAKE_BLOCK, nargs, ntmprs) <= -1) return -1;

	/* specifying MAX_CODE_JUMP causes emit_single_param_instruction() to 
	 * produce the long jump instruction (BCODE_JUMP_FORWARD_X) */
	jump_inst_pos = hcl->code.bc.len;
	if (emit_single_param_instruction (hcl, HCL_CODE_JUMP_FORWARD_0, MAX_CODE_JUMP) <= -1) return -1;

	SWITCH_TOP_CFRAME (hcl, COP_COMPILE_OBJECT_LIST, HCL_CONS_CDR(obj));

	PUSH_SUBCFRAME (hcl, COP_EMIT_LAMBDA, hcl->_nil); /* operand field is not used for COP_EMIT_LAMBDA */
	cf = GET_SUBCFRAME (hcl); /* modify the EMIT_LAMBDA frame */
	cf->u.lambda.jip = jump_inst_pos;
	cf->u.lambda.nargs = nargs;
	cf->u.lambda.ntmprs = ntmprs;

	return 0;
}

static int compile_set (hcl_t* hcl, hcl_oop_t src)
{
	hcl_cframe_t* cf;
	hcl_oop_t obj, var, val;

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
	PUSH_SUBCFRAME (hcl, COP_EMIT_SET, var); /* set doesn't evaluate the variable name */
	cf = GET_SUBCFRAME (hcl);
	cf->u.set.var_type = VAR_NAMED;

	return 0;
}

static int compile_cons (hcl_t* hcl, hcl_oop_t obj)
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
			case HCL_SYNCODE_DEFUN:
			case HCL_SYNCODE_IF:
				/* TODO: */
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
		hcl_oow_t oldtop;
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

static HCL_INLINE int compile_symbol (hcl_t* hcl, hcl_oop_t obj)
{
	hcl_oow_t index;

	HCL_ASSERT (HCL_BRANDOF(hcl,obj) == HCL_BRAND_SYMBOL);

	/* check if a symbol is a local variable */
	if (find_temporary_variable_backward (hcl, obj, &index) <= -1)
	{
		/* global variable */
		if (add_literal(hcl, obj, &index) <= -1 ||
		    emit_single_param_instruction (hcl, HCL_CODE_PUSH_OBJECT_0, index) <= -1) return -1;
	}
	else
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
					if (emit_double_param_instruction(hcl, HCL_CODE_PUSH_CTXTEMPVAR_0, ctx_offset, index_in_ctx) <= -1) return -1;
					return 0;
				}
			}
		}
	#endif

		/* TODO: top-level... verify this. this will vary depending on how i implement the top-level and global variables... */
		if (emit_single_param_instruction (hcl, HCL_CODE_PUSH_TEMPVAR_0, index) <= -1) return -1;
	}

	return 0;
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
			if (compile_cons (hcl, cf->operand) <= -1) return -1;
			break;

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

		if (HCL_BRANDOF(hcl, cf->operand) != HCL_BRAND_CONS)
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
			/* (+ 1 2 3) - argument list. 1, 2, 3 pushed must remain in 
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
				PUSH_SUBCFRAME (hcl, COP_EMIT_POP, hcl->_nil);
			}
		}
	}

	return 0;
}

static HCL_INLINE int emit_lambda (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	hcl_oow_t block_code_size;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_LAMBDA);
	HCL_ASSERT (HCL_IS_NIL(hcl, cf->operand));

	hcl->c->blk.depth--;
	hcl->c->tv.size = hcl->c->blk.tmprcnt[hcl->c->blk.depth];

	/* HCL_CODE_LONG_PARAM_SIZE + 1 => size of the long JUMP_FORWARD instruction */
	block_code_size = hcl->code.bc.len - cf->u.lambda.jip - (HCL_BCODE_LONG_PARAM_SIZE + 1);

	if (block_code_size == 0)
 	{
		/* no body in lambda - (lambda (a b c)) */
/* TODO: is this correct??? */
		if (emit_byte_instruction(hcl, HCL_CODE_PUSH_NIL) <= -1) return -1;
	}

	if (emit_byte_instruction (hcl, HCL_CODE_RETURN_FROM_BLOCK) <= -1) return -1;

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
			patch_instruction (hcl, cf->u.lambda.jip, HCL_CODE_JUMP2_FORWARD);
			jump_offset = block_code_size - MAX_CODE_JUMP;
		}
		else
		{
			jump_offset = block_code_size;
		}

	#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
		patch_instruction (hcl, cf->u.lambda.jip + 1, jump_offset >> 8);
		patch_instruction (hcl, cf->u.lambda.jip + 2, jump_offset & 0xFF);
	#else
		patch_instruction (hcl, cf->u.lambda.jip + 1, jump_offset);
	#endif
	}

	POP_CFRAME (hcl);
	return 0;
}

static HCL_INLINE int emit_pop (hcl_t* hcl)
{
	hcl_cframe_t* cf;
	int n;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_POP);
	HCL_ASSERT (HCL_IS_NIL(hcl, cf->operand));

	n = emit_byte_instruction (hcl, HCL_CODE_POP_STACKTOP);

	POP_CFRAME (hcl);
	return n;
}

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

static HCL_INLINE int emit_set (hcl_t* hcl)
{
	hcl_cframe_t* cf;

	cf = GET_TOP_CFRAME(hcl);
	HCL_ASSERT (cf->opcode == COP_EMIT_SET);
	HCL_ASSERT (HCL_IS_SYMBOL(hcl, cf->operand));

	if (cf->u.set.var_type == VAR_NAMED)
	{
		hcl_oow_t index;

		if (add_literal(hcl, cf->operand, &index) <= -1 ||
		    emit_single_param_instruction(hcl, HCL_CODE_STORE_INTO_OBJECT_0, index) <= -1) return -1;
	}
	else
	{
		/* TODO: */
HCL_DEBUG0 (hcl, "EMIT SET NOT IMPLEMENTED YET\n");
hcl->errnum = HCL_ENOIMPL;
return -1;
	}

	POP_CFRAME (hcl);
	return 0;
};

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

		switch (cf->opcode)
		{
			case COP_EXIT:
				goto done;

			case COP_COMPILE_OBJECT:
				if (compile_object (hcl) <= -1) goto oops;
				break;

			case COP_COMPILE_OBJECT_LIST:
			case COP_COMPILE_ARGUMENT_LIST:
				if (compile_object_list (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_POP:
				if (emit_pop (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_CALL:
				if (emit_call (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_LAMBDA:
				if (emit_lambda (hcl) <= -1) goto oops;
				break;

			case COP_EMIT_SET:
				if (emit_set (hcl) <= -1) goto oops;
				break;

			default:
				hcl->errnum = HCL_EINTERN;
				goto oops;
		}
	}

done:
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
	hcl->c->blk.depth = 0;
	return -1;
}
