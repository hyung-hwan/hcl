/*
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


#include <stdio.h>
#define PRINT_STACK_ALIGN 128

enum
{
	PRINT_STACK_CONS,
	PRINT_STACK_ARRAY,
	PRINT_STACK_ARRAY_END,
	PRINT_STACK_DIC,
	PRINT_STACK_DIC_END
};

typedef struct print_stack_t print_stack_t;
struct print_stack_t
{
	int type;
	hcl_oop_t obj;
	hcl_oop_t obj2;
	hcl_oow_t idx;
	hcl_oow_t idx2;
};

static HCL_INLINE int push (hcl_t* hcl, print_stack_t* info)
{
	if (hcl->p.s.size >= hcl->p.s.capa)
	{
		print_stack_t* tmp;
		hcl_oow_t new_capa;

		new_capa = HCL_ALIGN (hcl->p.s.capa + 1, PRINT_STACK_ALIGN);
		tmp = (print_stack_t*)hcl_reallocmem(hcl, hcl->p.s.ptr, new_capa * HCL_SIZEOF(*info));
		if (!tmp) return -1;

		hcl->p.s.ptr = tmp;
		hcl->p.s.capa = new_capa;
	}

	((print_stack_t*)hcl->p.s.ptr)[hcl->p.s.size] = *info;
	hcl->p.s.size++;

	return 0;
}

static HCL_INLINE void pop (hcl_t* hcl, print_stack_t* info)
{
	HCL_ASSERT (hcl, hcl->p.s.size > 0);
	hcl->p.s.size--;
	*info = ((print_stack_t*)hcl->p.s.ptr)[hcl->p.s.size];
}

enum
{
	WORD_UNDEF,
	WORD_NIL,
	WORD_TRUE,
	WORD_FALSE,

	WORD_SET,
	WORD_PRIM,

	WORD_FUNCTION,
	WORD_BLOCK,
	WORD_CONTEXT,
	WORD_PROCESS,
	WORD_PROCESS_SCHEDULER,
	WORD_SEMAPHORE,
	WORD_SEMAPHORE_GROUP,
	WORD_CLASS,
	WORD_INSTANCE
};

static struct
{
	hcl_oow_t  len;
	hcl_ooch_t ptr[20];
} word[] =
{
	{  8,  { '#','<','U','N','D','E','F','>' } },
	{  3,  { 'n','i','l' } },
	{  4,  { 't','r','u','e' } },
	{  5,  { 'f','a','l','s','e' } },

	{  6,  { '#','<','S','E','T','>' } },
	{  7,  { '#','<','P','R','I','M','>' } },

	{  11, { '#','<','F','U','N','C','T','I','O','N','>' } },
	{  8,  { '#','<','B','L','O','C','K','>' } },
	{  10, { '#','<','C','O','N','T','E','X','T','>' } },
	{  10, { '#','<','P','R','O','C','E','S','S','>' } },
	{  20, { '#','<','P','R','O','C','E','S','S','-','S','C','H','E','D','U','L','E','R','>' } },
	{  12, { '#','<','S','E','M','A','P','H','O','R','E','>' } },
	{  18, { '#','<','S','E','M','A','P','H','O','R','E','-','G','R','O','U','P','>' } },

	{  8,  { '#','<','C','L','A','S','S','>' } },
	{  11, { '#','<','I','N','S','T','A','N','C','E','>' } }
};

static HCL_INLINE int print_single_char (hcl_t* hcl, hcl_fmtout_t* fmtout, hcl_ooch_t ch)
{
	hcl_oochu_t chu = (hcl_oochu_t)ch;
	if (chu == '\\' || chu == '\"')
	{
		if (hcl_bfmt_out(hcl, fmtout, "\\%jc", chu) <= -1) return -1;
	}
#if defined(HCL_OOCH_IS_UCH)
	else if (chu < ' ')
#else
	else if (chu < ' ' || chu >= 0x80)
#endif
	{
		hcl_oochu_t escaped;

		switch (chu)
		{
			case '\0':
				escaped = '0';
				break;
			case '\n':
				escaped = 'n';
				break;
			case '\r':
				escaped = 'r';
				break;
			case '\t':
				escaped = 't';
				break;
			case '\f':
				escaped = 'f';
				break;
			case '\b':
				escaped = 'b';
				break;
			case '\v':
				escaped = 'v';
				break;
			case '\a':
				escaped = 'a';
				break;
			default:
				escaped = chu;
				break;
		}

		if (escaped == chu)
		{
		#if (HCL_SIZEOF_OOCH_T >= 4)
			if (chu >= 0x10000u)
			{
				if (hcl_bfmt_out(fmtout, "\\U%08X", chu) <= -1) return -1;
			}
			else
		#endif
			{
		#if (HCL_SIZEOF_OOCH_T >= 2)
				if (chu >= 0x100u)
				{
					if (hcl_bfmt_out(hcl, fmtout, "\\u%04X", chu) <= -1) return -1;
				}
				else
		#endif
				{
					if (hcl_bfmt_out(hcl, fmtout, "\\x%02X", chu) <= -1) return -1;
				}
			}
		}
		else
		{
			if (hcl_bfmt_out(hcl, fmtout, "\\%jc", escaped) <= -1) return -1;
		}
	}
	else
	{
		if (hcl_bfmt_out(hcl, fmtout, "%jc", ch) <= -1) return -1;
	}

	return 0;
}

static HCL_INLINE int print_single_byte_char (hcl_t* hcl, hcl_fmtout_t* fmtout, hcl_bch_t ch)
{
	hcl_bchu_t chu = (hcl_bchu_t)ch;
	if (chu == '\\' || chu == '\"')
	{
		if (hcl_bfmt_out(hcl, fmtout, "\\%hc", chu) <= -1) return -1;
	}
#if defined(HCL_OOCH_IS_UCH)
	else if (chu < ' ')
#else
	else if (chu < ' ' || chu >= 0x80)
#endif
	{
		hcl_bchu_t escaped;

		switch (chu)
		{
			case '\0':
				escaped = '0';
				break;
			case '\n':
				escaped = 'n';
				break;
			case '\r':
				escaped = 'r';
				break;
			case '\t':
				escaped = 't';
				break;
			case '\f':
				escaped = 'f';
				break;
			case '\b':
				escaped = 'b';
				break;
			case '\v':
				escaped = 'v';
				break;
			case '\a':
				escaped = 'a';
				break;
			default:
				escaped = chu;
				break;
		}

		if (escaped == chu)
		{
			if (hcl_bfmt_out(hcl, fmtout, "\\x%02X", chu) <= -1) return -1;
		}
		else
		{
			if (hcl_bfmt_out(hcl, fmtout, "\\%hc", escaped) <= -1) return -1;
		}
	}
	else
	{
		if (hcl_bfmt_out(hcl, fmtout, "%hc", ch) <= -1) return -1;
	}

	return 0;
}

int hcl_fmt_object (hcl_t* hcl, hcl_fmtout_t* fmtout, hcl_oop_t obj)
{
	hcl_oop_t cur;
	print_stack_t ps;
	hcl_oop_class_t _class;
	int brand;
	int word_index;
	int json;

	static const hcl_bch_t *opening_parens[][2] =
	{
		                            /* navtive   json */
		HCL_AID(HCL_CONCODE_XLIST)     { "(",     "(" },
		HCL_AID(HCL_CONCODE_MLIST)     { "(",     "(" },
		HCL_AID(HCL_CONCODE_ALIST)     { "(",     "(" },
		HCL_AID(HCL_CONCODE_BLIST)     { "(",     "(" },
		HCL_AID(HCL_CONCODE_BLOCK)     { "{",     "{" },
		HCL_AID(HCL_CONCODE_ARRAY)     { "#[",    "[" },
		HCL_AID(HCL_CONCODE_BYTEARRAY) { "#b[",   "[" },
		HCL_AID(HCL_CONCODE_CHARARRAY) { "#c[",   "[" },
		HCL_AID(HCL_CONCODE_DIC)       { "#{",    "{" },
		HCL_AID(HCL_CONCODE_QLIST)     { "#(",    "[" },
		HCL_AID(HCL_CONCODE_TUPLE)     { "[",     "[" }
	};

	static const hcl_bch_t *closing_parens[][2] =
	{
		HCL_AID(HCL_CONCODE_XLIST)     { ")",     ")" },
		HCL_AID(HCL_CONCODE_MLIST)     { ")",     ")" },
		HCL_AID(HCL_CONCODE_ALIST)     { ")",     ")" },
		HCL_AID(HCL_CONCODE_BLIST)     { ")",     ")" },
		HCL_AID(HCL_CONCODE_BLOCK)     { "}",     "}" },
		HCL_AID(HCL_CONCODE_ARRAY)     { "]",     "]" },
		HCL_AID(HCL_CONCODE_BYTEARRAY) { "]",     "]" },
		HCL_AID(HCL_CONCODE_CHARARRAY) { "]",     "]" },
		HCL_AID(HCL_CONCODE_DIC)       { "}",     "}" },
		HCL_AID(HCL_CONCODE_QLIST)     { ")",     "]" },
		HCL_AID(HCL_CONCODE_TUPLE)     { "]",     "]" }
	};

	static const hcl_bch_t* breakers[][2] =
	{
		{ " ",       "," }, /* item breaker */
		{ " ",       ":" }  /* key value breaker */
	};

	json = !!(fmtout->mask & HCL_LOG_PREFER_JSON);

next:
	_class = (hcl_oop_class_t)HCL_CLASSOF(hcl, obj);
	brand = HCL_OOP_TO_SMOOI(_class->ibrand);
	switch (brand)
	{
		case HCL_BRAND_SMOOI:
			if (hcl_bfmt_out(hcl, fmtout, "%zd", HCL_OOP_TO_SMOOI(obj)) <= -1) return -1;
			goto done;

		case HCL_BRAND_SMPTR:
			if (hcl_bfmt_out(hcl, fmtout, "#p%zX", (hcl_oow_t)HCL_OOP_TO_SMPTR(obj)) <= -1) return -1;
			goto done;

		case HCL_BRAND_ERROR:
			if (hcl_bfmt_out(hcl, fmtout, "#e%zd", (hcl_ooi_t)HCL_OOP_TO_ERROR(obj)) <= -1) return -1;
			goto done;

		case HCL_BRAND_CHARACTER:
		{
			hcl_ooch_t ch = HCL_OOP_TO_CHAR(obj);
			if (hcl_bfmt_out(hcl, fmtout, "\'") <= -1 ||
			    print_single_char(hcl, fmtout, ch) <= -1 ||
			    hcl_bfmt_out(hcl, fmtout, "\'") <= -1) return -1;
			goto done;
		}

		case HCL_BRAND_UNDEF:
			word_index = WORD_UNDEF;
			goto print_word;

		case HCL_BRAND_NIL:
			word_index = WORD_NIL;
			goto print_word;

		case HCL_BRAND_TRUE:
			word_index = WORD_TRUE;
			goto print_word;

		case HCL_BRAND_FALSE:
			word_index = WORD_FALSE;
			goto print_word;

		case HCL_BRAND_PBIGINT:
		case HCL_BRAND_NBIGINT:
		{
			hcl_oop_t tmp;

			/* -1 to drive hcl_inttostr() to not create a new string object.
			 * not using the object memory. the result stays in the temporary
			 * buffer */
			tmp = hcl_inttostr(hcl, obj, 10 | HCL_INTTOSTR_NONEWOBJ);
			if (!tmp) return -1;

			HCL_ASSERT (hcl, (hcl_oop_t)tmp == hcl->_nil);
			if (hcl_bfmt_out(hcl, fmtout, "%.*js", hcl->inttostr.xbuf.len, hcl->inttostr.xbuf.ptr) <= -1) return -1;
			break;
		}

		case HCL_BRAND_FPDEC:
		{
			hcl_oop_fpdec_t f = (hcl_oop_fpdec_t)obj;
			hcl_ooi_t scale;

			scale = HCL_OOP_TO_SMOOI(f->scale);

			if (f->value == HCL_SMOOI_TO_OOP(0))
			{
				if (scale == 0)
				{
					if (hcl_bfmt_out(hcl, fmtout, "0.") <= -1) return -1;
				}
				else
				{
					if (hcl_bfmt_out(hcl, fmtout, "0.%0*d", scale, 0) <= -1) return -1;
				}
			}
			else
			{
				hcl_oop_t tmp;
				hcl_oow_t len, adj;

				tmp = hcl_inttostr(hcl, f->value, 10 | HCL_INTTOSTR_NONEWOBJ);
				if (!tmp) return -1;

				adj = (hcl->inttostr.xbuf.ptr[0] == '-');
				len = hcl->inttostr.xbuf.len - adj;

				if (len <= scale)
				{
					if (scale == len)
					{
						if (hcl_bfmt_out(hcl, fmtout, "%.*js0.%.*js",
							adj, hcl->inttostr.xbuf.ptr,
							len, &hcl->inttostr.xbuf.ptr[adj]) <= -1) return -1;
					}
					else
					{
						if (hcl_bfmt_out(hcl, fmtout, "%.*js0.%0*d%.*js",
							adj, hcl->inttostr.xbuf.ptr,
							scale - len, 0,
							len, &hcl->inttostr.xbuf.ptr[adj]) <= -1) return -1;
					}
				}
				else
				{
					hcl_ooi_t ndigits;
					ndigits = hcl->inttostr.xbuf.len - scale;
					if (hcl_bfmt_out(hcl, fmtout, "%.*js.%.*js", ndigits, hcl->inttostr.xbuf.ptr, scale, &hcl->inttostr.xbuf.ptr[ndigits]) <= -1) return -1;
				}
			}
			break;
		}

#if 0
		case HCL_BRAND_REAL:
		{
			qse_char_t buf[256];
			hcl->prm.sprintf (
				hcl->prm.ctx,
				buf, HCL_COUNTOF(buf),
				HCL_T("%Lf"),
			#ifdef __MINGW32__
				(double)HCL_RVAL(obj)
			#else
				(long double)HCL_RVAL(obj)
			#endif
			);

			OUTPUT_STR (hcl, buf);
			break;
		}
#endif

		case HCL_BRAND_SYMBOL:
			/* Any needs for special action if SYNT(obj) is true?
			 * I simply treat the syntax symbol as a normal symbol
			 * for printing currently. */
			/* TODO: escaping if needed */
			/*if (hcl_bfmt_out(hcl, fmtout, "#\"%.*js\"", HCL_OBJ_GET_SIZE(obj), HCL_OBJ_GET_CHAR_SLOT(obj)) <= -1) return -1;*/
			if (hcl_bfmt_out(hcl, fmtout, "%.*js", HCL_OBJ_GET_SIZE(obj), HCL_OBJ_GET_CHAR_SLOT(obj)) <= -1) return -1;
			break;

		case HCL_BRAND_STRING:
		{
			hcl_ooch_t ch;
			hcl_oow_t i;
			int escape = 0;

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				ch = ((hcl_oop_char_t)obj)->slot[i];
				if (ch < ' ' || ch == '\"' || ch == '\\')
				{
					escape = 1;
					break;
				}
			}

			if (escape)
			{
				if (hcl_bfmt_out(hcl, fmtout, "\"") <= -1) return -1;
				for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
				{
					ch = ((hcl_oop_char_t)obj)->slot[i];
					if (print_single_char(hcl, fmtout, ch) <= -1) return -1;
				}
				if (hcl_bfmt_out(hcl, fmtout, "\"") <= -1) return -1;
			}
			else
			{
				if (hcl_bfmt_out(hcl, fmtout, "\"%.*js\"", HCL_OBJ_GET_SIZE(obj), HCL_OBJ_GET_CHAR_SLOT(obj)) <= -1) return -1;
			}
			break;
		}

		case HCL_BRAND_BYTE_STRING:
		{
			hcl_bch_t ch;
			hcl_oow_t i;
			int escape = 0;

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				ch = ((hcl_oop_byte_t)obj)->slot[i];
				if (ch < ' ' || ch == '\"' || ch == '\\')
				{
					escape = 1;
					break;
				}
			}

			if (escape)
			{
				if (hcl_bfmt_out(hcl, fmtout, "b\"") <= -1) return -1;
				for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
				{
					ch = ((hcl_oop_byte_t)obj)->slot[i];
					if (print_single_byte_char(hcl, fmtout, ch) <= -1) return -1;
				}
				if (hcl_bfmt_out(hcl, fmtout, "\"") <= -1) return -1;
			}
			else
			{
				if (hcl_bfmt_out(hcl, fmtout, "b\"%.*hs\"", HCL_OBJ_GET_SIZE(obj), HCL_OBJ_GET_BYTE_SLOT(obj)) <= -1) return -1;
			}
			break;
		}


		case HCL_BRAND_CONS:
		{
			int concode;

			/* this part is to print a linked list of cells. ignore the
			 * request to output in the json format */

			concode = HCL_OBJ_GET_FLAGS_CONCODE(obj);
			if (hcl_bfmt_out(hcl, fmtout, opening_parens[concode][0]) <= -1) return -1;
			cur = obj;

			/* TODO: for MLIST, print : after the first element.
			 *       for ALIST, print := after the first element */
			do
			{
				int x;

				/* Push what to print next on to the stack
				 * the variable p is */
				ps.type = PRINT_STACK_CONS;
				ps.obj = HCL_CONS_CDR(cur);
				ps.idx = concode; /* this is not an index but use this field to restore concode */
				x = push(hcl, &ps);
				if (x <= -1) return -1;

				obj = HCL_CONS_CAR(cur);
				/* Jump to the 'next' label so that the object
				 * pointed to by 'obj' is printed. Once it
				 * ends, a jump back to the 'resume' label
				 * is made at the at of this function. */
				goto next;

			resume_cons:
				HCL_ASSERT (hcl, ps.type == PRINT_STACK_CONS);
				cur = ps.obj; /* Get back the CDR pushed */
				concode = ps.idx; /* restore the concode */
				if (HCL_IS_NIL(hcl,cur))
				{
					/* The CDR part points to a NIL object, which
					 * indicates the end of a list. break the loop */
					break;
				}
				if (!HCL_OOP_IS_POINTER(cur) || HCL_OBJ_GET_CLASS(cur) != (hcl_oop_t)hcl->c_cons)
				{
					/* The CDR part does not point to a pair. */
					if (hcl_bfmt_out(hcl, fmtout, " . ") <= -1) return -1;

					/* Push NIL so that the HCL_IS_NIL(hcl,p) test in
					 * the 'if' statement above breaks the loop
					 * after the jump is maded back to the 'resume'
					 * label. */
					ps.type = PRINT_STACK_CONS;
					ps.obj = hcl->_nil;
					x = push(hcl, &ps);
					if (x <= -1) return -1;

					/* Make a jump to 'next' to print the CDR part */
					obj = cur;
					goto next;
				}

				/* The CDR part points to a pair. proceed to it */
				if (hcl_bfmt_out(hcl, fmtout, breakers[0][0]) <= -1) return -1;
			}
			while (1);

			if (hcl_bfmt_out(hcl, fmtout, closing_parens[concode][0]) <= -1) return -1;
			break;
		}

		case HCL_BRAND_ARRAY:
		{
			hcl_oow_t arridx;

			if (hcl_bfmt_out(hcl, fmtout, opening_parens[HCL_CONCODE_ARRAY][json]) <= -1) return -1;

			if (HCL_OBJ_GET_SIZE(obj) <= 0)
			{
				if (hcl_bfmt_out(hcl, fmtout, closing_parens[HCL_CONCODE_ARRAY][json]) <= -1) return -1;
				break;
			}
			arridx = 0;
			ps.type = PRINT_STACK_ARRAY;

			do
			{
				int x;

				/* Push what to print next on to the stack */
				ps.idx = arridx + 1;
				if (ps.idx >= HCL_OBJ_GET_SIZE(obj))
				{
					ps.type = PRINT_STACK_ARRAY_END;
				}
				else
				{
					HCL_ASSERT (hcl, ps.type == PRINT_STACK_ARRAY);
					ps.obj = obj;
				}

				x = push (hcl, &ps);
				if (x <= -1) return -1;

				obj = ((hcl_oop_oop_t)obj)->slot[arridx];
				if (arridx > 0)
				{
					if (hcl_bfmt_out(hcl, fmtout, breakers[0][json]) <= -1) return -1;
				}
				/* Jump to the 'next' label so that the object
				 * pointed to by 'obj' is printed. Once it
				 * ends, a jump back to the 'resume' label
				 * is made at the end of this function. */
				goto next;

			resume_array:
				HCL_ASSERT (hcl, ps.type == PRINT_STACK_ARRAY);
				arridx = ps.idx;
				obj = ps.obj;
			}
			while (1);
			break;
		}

		case HCL_BRAND_BYTE_ARRAY:
		{
			hcl_oow_t i;
			if (hcl_bfmt_out(hcl, fmtout, opening_parens[HCL_CONCODE_BYTEARRAY][json]) <= -1) return -1;
			if (HCL_OBJ_GET_SIZE(obj) > 0)
			{
				if (hcl_bfmt_out(hcl, fmtout, "%d", ((hcl_oop_byte_t)obj)->slot[0]) <= -1) return -1;
				for (i = 1; i < HCL_OBJ_GET_SIZE(obj); i++)
				{
					if (hcl_bfmt_out(hcl, fmtout, "%hs%d", breakers[0][json], ((hcl_oop_byte_t)obj)->slot[i]) <= -1) return -1;
				}
			}
			if (hcl_bfmt_out(hcl, fmtout, closing_parens[HCL_CONCODE_BYTEARRAY][json]) <= -1) return -1;
			break;
		}

		case HCL_BRAND_DIC:
		{
			hcl_oow_t bucidx, bucsize, buctally;
			hcl_oop_dic_t dic;

			if (hcl_bfmt_out(hcl, fmtout, opening_parens[HCL_CONCODE_DIC][json]) <= -1) return -1;

			dic = (hcl_oop_dic_t)obj;
			HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(dic->tally));
			if (HCL_OOP_TO_SMOOI(dic->tally) <= 0)
			{
				if (hcl_bfmt_out(hcl, fmtout, closing_parens[HCL_CONCODE_DIC][json]) <= -1) return -1;
				break;
			}
			bucidx = 0;
			bucsize = HCL_OBJ_GET_SIZE(dic->bucket);
			buctally = 0;
			ps.type = PRINT_STACK_DIC;
			ps.obj2 = (hcl_oop_t)dic;

			do
			{
				int x;

				if ((buctally & 1) == 0)
				{
					while (bucidx < bucsize)
					{
						/* skip an unoccupied slot in the bucket array */
						obj = dic->bucket->slot[bucidx];
						if (!HCL_IS_NIL(hcl,obj)) break;
						bucidx++;
					}

					if (bucidx >= bucsize)
					{
						/* done. scanned the entire bucket */
						if (hcl_bfmt_out(hcl, fmtout, closing_parens[HCL_CONCODE_DIC][json]) <= -1) return -1;
						break;
					}

					ps.idx = bucidx; /* no increment yet */
					HCL_ASSERT (hcl, ps.idx < bucsize);
					HCL_ASSERT (hcl, ps.type == PRINT_STACK_DIC);

					ps.obj = dic->bucket->slot[ps.idx];
					ps.idx2 = buctally + 1;

					x = push (hcl, &ps);
					if (x <= -1) return -1;

					HCL_ASSERT (hcl, HCL_IS_CONS(hcl,obj));
					obj = HCL_CONS_CAR(obj);
				}
				else
				{
					/* Push what to print next on to the stack */
					ps.idx = bucidx + 1;
					if (ps.idx >= bucsize)
					{
						ps.type = PRINT_STACK_DIC_END;
					}
					else
					{
						HCL_ASSERT (hcl, ps.type == PRINT_STACK_DIC);
						ps.obj = dic->bucket->slot[ps.idx];
					}
					ps.idx2 = buctally + 1;

					x = push (hcl, &ps);
					if (x <= -1) return -1;

					HCL_ASSERT (hcl, HCL_IS_CONS(hcl,obj));
					obj = HCL_CONS_CDR(obj);
				}

				if (buctally > 0)
				{
					if (hcl_bfmt_out(hcl, fmtout, breakers[buctally & 1][json]) <= -1) return -1;
				}

				/* Jump to the 'next' label so that the object
				 * pointed to by 'obj' is printed. Once it
				 * ends, a jump back to the 'resume' label
				 * is made at the end of this function. */
				goto next;

			resume_dic:
				HCL_ASSERT (hcl, ps.type == PRINT_STACK_DIC);
				bucidx = ps.idx;
				buctally = ps.idx2;
				obj = ps.obj;
				dic = (hcl_oop_dic_t)ps.obj2;
				bucsize = HCL_OBJ_GET_SIZE(dic->bucket);
			}
			while (1);

			break;
		}

		case HCL_BRAND_PRIM:
			word_index = WORD_PRIM;
			goto print_word;

		case HCL_BRAND_FUNCTION:
			word_index = WORD_FUNCTION;
			goto print_word;

		case HCL_BRAND_BLOCK:
			word_index = WORD_BLOCK;
			goto print_word;

		case HCL_BRAND_CONTEXT:
			word_index = WORD_CONTEXT;
			goto print_word;

		case HCL_BRAND_PROCESS:
			word_index = WORD_PROCESS;
			goto print_word;

		case HCL_BRAND_PROCESS_SCHEDULER:
			word_index = WORD_PROCESS_SCHEDULER;
			goto print_word;

		case HCL_BRAND_SEMAPHORE:
			word_index = WORD_SEMAPHORE;
			goto print_word;

		case HCL_BRAND_SEMAPHORE_GROUP:
			word_index = WORD_SEMAPHORE_GROUP;
			goto print_word;

		case HCL_BRAND_CLASS:
		{
			hcl_oop_class_t _class = (hcl_oop_class_t)obj;
			if (HCL_IS_NIL(hcl, _class->name))
			{
				word_index = WORD_CLASS;
				goto print_word;
			}
			HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, _class->name));
			if (hcl_bfmt_out(hcl, fmtout, "%.*js", HCL_OBJ_GET_SIZE(_class->name), HCL_OBJ_GET_CHAR_SLOT(_class->name)) <= -1) return -1;
			break;
		}

		case HCL_BRAND_INSTANCE:
		{
			hcl_oop_class_t _class = (hcl_oop_class_t)HCL_CLASSOF(hcl, obj);
			HCL_ASSERT (hcl, HCL_IS_CLASS(hcl, _class));
			if (HCL_IS_NIL(hcl, _class->name))
			{
				word_index = WORD_INSTANCE;
				goto print_word;
			}
			HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, _class->name));
			if (hcl_bfmt_out(hcl, fmtout, "#INSTANCE OF %.*js", HCL_OBJ_GET_SIZE(_class->name), HCL_OBJ_GET_CHAR_SLOT(_class->name)) <= -1) return -1;
			break;
		}

		default:
			HCL_DEBUG3 (hcl, "Internal error - unknown object brand %d at %s:%d\n", (int)brand, __FILE__, __LINE__);
			HCL_ASSERT (hcl, "Unknown object brand" == HCL_NULL);
			hcl_seterrbfmt (hcl, HCL_EINTERN, "unknown object brand %d", (int)brand);
			return -1;

		print_word:
			if (hcl_bfmt_out(hcl, fmtout, "%.*js", word[word_index].len, word[word_index].ptr) <= -1) return -1;
			break;
	}

done:
	/* if the printing stack is not empty, we still got more to print */
	while (hcl->p.s.size > 0)
	{
		pop (hcl, &ps);
		switch (ps.type)
		{
			case PRINT_STACK_CONS:
				goto resume_cons;

			case PRINT_STACK_ARRAY:
				goto resume_array;

			case PRINT_STACK_ARRAY_END:
				if (hcl_bfmt_out(hcl, fmtout, closing_parens[HCL_CONCODE_ARRAY][json]) <= -1) return -1;
				break;

			case PRINT_STACK_DIC:
				goto resume_dic;

			case PRINT_STACK_DIC_END:
				if (hcl_bfmt_out(hcl, fmtout, closing_parens[HCL_CONCODE_DIC][json]) <= -1) return -1;
				break;

			default:
				HCL_DEBUG3 (hcl, "Internal error - unknown print stack type %d at %s:%d\n", (int)ps.type, __FILE__, __LINE__);
				hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - unknown print stack type %d", (int)ps.type);
				return -1;
		}
	}
	return 0;
}

#if 0
int hcl_outfmtobj (hcl_t* hcl, hcl_bitmask_t mask, hcl_oop_t obj, hcl_outbfmt_t outbfmt)
{
	int n;

	/* the printer stack must be empty. buggy if not. */
	HCL_ASSERT (hcl, hcl->p.s.size == 0);

	hcl->p.e = obj; /* remember the head of the object to print */
	n = hcl_proutbfmt(hcl, mask, obj);
	hcl->p.e = hcl->_nil; /* reset what's remembered */

	/* clear the printing stack if an error has occurred for GC not to keep
	 * the objects in the stack */
	if (n <= -1) hcl->p.s.size = 0;

	/* the printer stack must get empty when done. buggy if not */
	HCL_ASSERT (hcl, hcl->p.s.size == 0);

	return n;
}
#endif

int hcl_print (hcl_t* hcl, hcl_oop_t obj)
{
	HCL_ASSERT (hcl, hcl->io.udo_wrtr != HCL_NULL);
	/*return hcl_outfmtobj(hcl, HCL_LOG_APP | HCL_LOG_FATAL, obj);*/
	return hcl_prbfmt(hcl, "%O", obj);
}

void hcl_dumpcnode (hcl_t* hcl, hcl_cnode_t* cnode, int newline)
{
	int t;

	/* TODO: this is incomplete function. make it complete */
	if (cnode)
	{
		t = HCL_CNODE_GET_TYPE(cnode);
		switch (t)
		{
			case HCL_CNODE_CHARLIT:
			case HCL_CNODE_BCHRLIT:
			case HCL_CNODE_SYMBOL:
			case HCL_CNODE_DSYMBOL:
			case HCL_CNODE_STRLIT:
			case HCL_CNODE_BSTRLIT:
			case HCL_CNODE_SYMLIT:
			case HCL_CNODE_NUMLIT:
			case HCL_CNODE_RADNUMLIT:
			case HCL_CNODE_FPDECLIT:
			case HCL_CNODE_SMPTRLIT:
			case HCL_CNODE_ERRLIT:

			case HCL_CNODE_NIL:
			case HCL_CNODE_TRUE:
			case HCL_CNODE_FALSE:
			case HCL_CNODE_SELF:
			case HCL_CNODE_SUPER:

			case HCL_CNODE_CLASS:
			case HCL_CNODE_FUN:
			case HCL_CNODE_DO:
			case HCL_CNODE_IF:
			case HCL_CNODE_ELIF:
			case HCL_CNODE_ELSE:
			case HCL_CNODE_THROW:
			case HCL_CNODE_TRY:
			case HCL_CNODE_CATCH:
			case HCL_CNODE_BREAK:
			case HCL_CNODE_CONTINUE:
			case HCL_CNODE_UNTIL:
			case HCL_CNODE_WHILE:
			case HCL_CNODE_RETURN:
			case HCL_CNODE_REVERT:
			case HCL_CNODE_AND:
			case HCL_CNODE_OR:
			case HCL_CNODE_PLUS:
			case HCL_CNODE_SET:
			case HCL_CNODE_SET_R:

			case HCL_CNODE_ELLIPSIS:
			case HCL_CNODE_TRPCOLONS:
			case HCL_CNODE_DBLCOLONS:
			case HCL_CNODE_COLON:
			case HCL_CNODE_COLONGT:
			case HCL_CNODE_COLONLT:
				hcl_logbfmt (hcl, HCL_LOG_FATAL, " %.*js ", HCL_CNODE_GET_TOKLEN(cnode), HCL_CNODE_GET_TOKPTR(cnode));
				break;

			case HCL_CNODE_CONS:
			{
				hcl_concode_t cc;

				hcl_logbfmt (hcl, HCL_LOG_FATAL, " (");
				hcl_dumpcnode (hcl, HCL_CNODE_CONS_CAR(cnode), 0);

				cc = HCL_CNODE_CONS_CONCODE(cnode);
				switch (cc)
				{
					case HCL_CONCODE_ALIST:
						hcl_logbfmt (hcl, HCL_LOG_FATAL, " := ");
						break;
					case HCL_CONCODE_BLIST:
					case HCL_CONCODE_MLIST:
						hcl_logbfmt (hcl, HCL_LOG_FATAL, ":");
						break;
				}

				hcl_dumpcnode (hcl, HCL_CNODE_CONS_CDR(cnode),0);
				hcl_logbfmt (hcl, HCL_LOG_FATAL, ") ");
				break;
			}

			case HCL_CNODE_ELIST:
				hcl_logbfmt (hcl, HCL_LOG_FATAL, " () ", HCL_CNODE_GET_TOKLEN(cnode), HCL_CNODE_GET_TOKPTR(cnode));
				break;

			case HCL_CNODE_SHELL:
				hcl_logbfmt (hcl, HCL_LOG_FATAL, " () ", HCL_CNODE_GET_TOKLEN(cnode), HCL_CNODE_GET_TOKPTR(cnode));
				break;
		}
	}

	if  (newline) hcl_logbfmt (hcl, HCL_LOG_FATAL, "\n");
}


