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
		tmp = hcl_reallocmem (hcl, hcl->p.s.ptr, new_capa * HCL_SIZEOF(*info));
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
	WORD_NIL,
	WORD_TRUE,
	WORD_FALSE,

	WORD_SET,
	WORD_CFRAME,
	WORD_PRIM,

	WORD_CONTEXT,
	WORD_PROCESS,
	WORD_PROCESS_SCHEDULER,
	WORD_SEMAPHORE
};

static struct 
{
	hcl_oow_t  len;
	hcl_ooch_t ptr[20];
} word[] =
{
	{  4,  { '#','n', 'i', 'l' } },
	{  5,  { '#','t', 'r', 'u', 'e' } },
	{  6,  { '#','f', 'a', 'l', 's', 'e' } },

	{  6,  { '#','<','S','E','T','>' } },
	{  9,  { '#','<','C','F','R','A','M','E','>' } },
	{  7,  { '#','<','P','R','I','M','>' } },

	{  10, { '#','<','C','O','N','T','E','X','T','>' } },
	{  10, { '#','<','P','R','O','C','E','S','S','>' } },
	{  20, { '#','<','P','R','O','C','E','S','S','-','S','C','H','E','D','U','L','E','R','>' } },
	{  12, { '#','<','S','E','M','A','P','H','O','R','E','>' } }
};


static HCL_INLINE int print_single_char (hcl_t* hcl, hcl_oow_t mask, hcl_ooch_t ch, hcl_outbfmt_t outbfmt)
{
	if (ch < ' ') 
	{
		hcl_ooch_t escaped;

		switch (ch)
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
				escaped = ch;
				break;
		}

		if (escaped == ch)
		{
			if (outbfmt(hcl, mask, "\\x%X", ch) <= -1) return -1;
		}
		else
		{
			if (outbfmt(hcl, mask, "\\%jc", escaped) <= -1) return -1;
		}
	}
	else
	{
		if (outbfmt(hcl, mask, "%jc", ch) <= -1) return -1;
	}

	return 0;
}

int hcl_outfmtobj (hcl_t* hcl, hcl_oow_t mask, hcl_oop_t obj, hcl_outbfmt_t outbfmt)
{
	hcl_oop_t cur;
	print_stack_t ps;
	int brand;
	int word_index;

next:
	if (HCL_OOP_IS_SMOOI(obj))
	{
		if (outbfmt(hcl, mask, "%zd", HCL_OOP_TO_SMOOI(obj)) <= -1) return -1;
		goto done;
	}
	else if (HCL_OOP_IS_CHAR(obj))
	{
		hcl_ooch_t ch = HCL_OOP_TO_CHAR(obj);
		if (outbfmt(hcl, mask, "\'") <= -1 ||
		    print_single_char(hcl, mask, ch, outbfmt) <= -1 ||
		    outbfmt(hcl, mask, "\'") <= -1) return -1;
		goto done;
	}
	else if (HCL_OOP_IS_SMPTR(obj))
	{
		if (outbfmt(hcl, mask, "#\\p%zu", (hcl_oow_t)HCL_OOP_TO_SMPTR(obj)) <= -1) return -1;
		goto done;
	}
	else if (HCL_OOP_IS_ERROR(obj))
	{
		if (outbfmt(hcl, mask, "#\\e%zd", (hcl_ooi_t)HCL_OOP_TO_ERROR(obj)) <= -1) return -1;
		goto done;
	}
 
	switch ((brand = HCL_OBJ_GET_FLAGS_BRAND(obj))) 
	{
		case HCL_BRAND_NIL:
			word_index = WORD_NIL;
			goto print_word;

		case HCL_BRAND_TRUE:
			word_index = WORD_TRUE;
			goto print_word;

		case HCL_BRAND_FALSE:
			word_index = WORD_FALSE;
			goto print_word;

		case HCL_BRAND_SMOOI:
			/* this type should not appear here as the actual small integer is 
			 * encoded in an object pointer */
			hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - unexpected object type %d", (int)brand);
			return -1;

		case HCL_BRAND_PBIGINT:
		case HCL_BRAND_NBIGINT:
		{
			hcl_oop_t tmp;

			/* -1 to drive hcl_inttostr() to not create a new string object.
			 * not using the object memory. the result stays in the temporary
			 * buffer */
			tmp = hcl_inttostr(hcl, obj, 10, -1); 
			if (!tmp) return -1;

			HCL_ASSERT (hcl, (hcl_oop_t)tmp == hcl->_nil); 
			if (outbfmt(hcl, mask, "%.*js", hcl->inttostr.xbuf.len, hcl->inttostr.xbuf.ptr) <= -1) return -1;
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
			if (outbfmt(hcl, mask, "%.*js", HCL_OBJ_GET_SIZE(obj), HCL_OBJ_GET_CHAR_SLOT(obj)) <= -1) return -1;
			break;

		case HCL_BRAND_STRING:
		{
			hcl_ooch_t ch;
			hcl_oow_t i;
			int escape = 0;

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				ch = ((hcl_oop_char_t)obj)->slot[i];
				if (ch < ' ') 
				{
					escape = 1;
					break;
				}
			}

			if (escape)
			{
				if (outbfmt(hcl, mask, "\"") <= -1) return -1;
				for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
				{
					
					ch = ((hcl_oop_char_t)obj)->slot[i];
					if (print_single_char(hcl, mask, ch, outbfmt) <= -1) return -1;
				}
				if (outbfmt(hcl, mask, "\"") <= -1) return -1;
			}
			else
			{
				if (outbfmt(hcl, mask, "\"%.*js\"", HCL_OBJ_GET_SIZE(obj), HCL_OBJ_GET_CHAR_SLOT(obj)) <= -1) return -1;
			}
			break;
		}

		case HCL_BRAND_CONS:
		{
			static hcl_bch_t *opening_paren[] =
			{
				"(",   /*HCL_CONCODE_XLIST */
				"#(",  /*HCL_CONCODE_ARRAY */
				"#[",  /*HCL_CONCODE_BYTEARRAY */
				"#{",  /*HCL_CONCODE_DIC */
				"'("   /*HCL_CONCODE_QLIST */
			};

			static hcl_bch_t *closing_paren[] =
			{
				")",   /*HCL_CONCODE_XLIST */
				")",   /*HCL_CONCODE_ARRAY */
				"]",   /*HCL_CONCODE_BYTEARRAY */
				"}",   /*HCL_CONCODE_DIC */
				")"    /*HCL_CONCODE_QLIST */
			};

			int concode;

			concode = HCL_OBJ_GET_FLAGS_SYNCODE(obj);
			if (outbfmt(hcl, mask, opening_paren[concode]) <= -1) return -1;
			cur = obj;

			do
			{
				int x;

				/* Push what to print next on to the stack 
				 * the variable p is */
				ps.type = PRINT_STACK_CONS;
				ps.obj = HCL_CONS_CDR(cur);
				ps.idx = concode; /* this is not an index but use this field to restore concode */
				x = push (hcl, &ps);
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
				if (!HCL_OOP_IS_POINTER(cur) || HCL_OBJ_GET_FLAGS_BRAND(cur) != HCL_BRAND_CONS) 
				{
					/* The CDR part does not point to a pair. */
					if (outbfmt(hcl, mask, " . ") <= -1) return -1;

					/* Push NIL so that the HCL_IS_NIL(hcl,p) test in 
					 * the 'if' statement above breaks the loop
					 * after the jump is maded back to the 'resume' 
					 * label. */
					ps.type = PRINT_STACK_CONS;
					ps.obj = hcl->_nil;
					x = push (hcl, &ps);
					if (x <= -1) return -1;

					/* Make a jump to 'next' to print the CDR part */
					obj = cur;
					goto next;
				}

				/* The CDR part points to a pair. proceed to it */
				if (outbfmt(hcl, mask, " ") <= -1) return -1;
			}
			while (1);

			if (outbfmt(hcl, mask, closing_paren[concode]) <= -1) return -1;
			break;
		}

		case HCL_BRAND_ARRAY:
		{
			hcl_oow_t arridx;

			if (outbfmt(hcl, mask, "#(") <= -1) return -1;

			if (HCL_OBJ_GET_SIZE(obj) <= 0) 
			{
				if (outbfmt(hcl, mask, ")") <= -1) return -1;
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
					if (outbfmt(hcl, mask, " ") <= -1) return -1;
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

			if (outbfmt(hcl, mask, "#[") <= -1) return -1;

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				if (outbfmt(hcl, mask, "%hs%d", ((i > 0)? " ": ""), ((hcl_oop_byte_t)obj)->slot[i]) <= -1) return -1;
			}
			if (outbfmt(hcl, mask, "]") <= -1) return -1;
			break;
		}

		case HCL_BRAND_DIC:
		{
			hcl_oow_t bucidx, bucsize, buctally;
			hcl_oop_dic_t dic;

			if (outbfmt(hcl, mask, "#{") <= -1) return -1;

			dic = (hcl_oop_dic_t)obj;
			HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(dic->tally));
			if (HCL_OOP_TO_SMOOI(dic->tally) <= 0) 
			{
				if (outbfmt(hcl, mask, "}") <= -1) return -1;
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
						if (outbfmt(hcl, mask, "}") <= -1) return -1;
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
					if (outbfmt(hcl, mask, " ") <= -1) return -1;
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

		case HCL_BRAND_SYMBOL_ARRAY:
		{
			hcl_oow_t i;

			if (outbfmt(hcl, mask, "|") <= -1) return -1;

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				hcl_oop_t s;
				s = ((hcl_oop_oop_t)obj)->slot[i];
				if (outbfmt(hcl, mask, " %.*js", HCL_OBJ_GET_SIZE(s), HCL_OBJ_GET_CHAR_SLOT(s)) <= -1) return -1;
			}
			if (outbfmt(hcl, mask, " |") <= -1) return -1;
			break;
		}

		case HCL_BRAND_CFRAME:
			word_index = WORD_CFRAME;
			goto print_word;

		case HCL_BRAND_PRIM:
			word_index = WORD_PRIM;
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

		default:
			HCL_DEBUG3 (hcl, "Internal error - unknown object type %d at %s:%d\n", (int)brand, __FILE__, __LINE__);
			HCL_ASSERT (hcl, "Unknown object type" == HCL_NULL);
			hcl_seterrbfmt (hcl, HCL_EINTERN, "unknown object type %d", (int)brand);
			return -1;

		print_word:
			if (outbfmt(hcl, mask, "%.*js", word[word_index].len, word[word_index].ptr) <= -1) return -1;
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
				if (outbfmt(hcl, mask, ")") <= -1) return -1;
				break;

			case PRINT_STACK_DIC:
				goto resume_dic;

			case PRINT_STACK_DIC_END:
				if (outbfmt(hcl, mask, "}") <= -1) return -1;
				break;

			default:
				HCL_DEBUG3 (hcl, "Internal error - unknown print stack type %d at %s:%d\n", (int)ps.type, __FILE__, __LINE__);
				hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - unknown print stack type %d", (int)ps.type);
				return -1;
		}
	}
	return 0;
}

int hcl_print (hcl_t* hcl, hcl_oop_t obj)
{
	int n;

	HCL_ASSERT (hcl, hcl->c->printer != HCL_NULL);

	/* the printer stack must be empty. buggy if not. */
	HCL_ASSERT (hcl, hcl->p.s.size == 0); 

	hcl->p.e = obj; /* remember the head of the object to print */
	n = hcl_outfmtobj (hcl, HCL_LOG_APP | HCL_LOG_FATAL, obj, hcl_proutbfmt);
	hcl->p.e = hcl->_nil; /* reset what's remembered */

	/* clear the printing stack if an error has occurred for GC not to keep
	 * the objects in the stack */
	if (n <= -1) hcl->p.s.size = 0;

	/* the printer stack must get empty when done. buggy if not */
	HCL_ASSERT (hcl, hcl->p.s.size == 0); 

	return n;
}
