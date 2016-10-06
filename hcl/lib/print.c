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

#define PRINT_STACK_ALIGN 128

struct printer_t
{
	hcl_t*          hcl;
	hcl_ioimpl_t    printer;
	hcl_iooutarg_t* outarg;
};
typedef struct printer_t printer_t;

#define OUTPUT_STRX(pr,p,l) \
do { \
	(pr)->outarg->ptr = p; \
	(pr)->outarg->len = l; \
	if ((pr)->printer((pr)->hcl, HCL_IO_WRITE, (pr)->outarg) <= -1) \
	{ \
		(pr)->hcl->errnum = HCL_EIOERR; \
		return -1; \
	} \
} while(0)

#define OUTPUT_STR(pr,p) OUTPUT_STRX(pr,p,hcl_countoocstr(p))

#define OUTPUT_CHAR(pr,ch) do { \
	hcl_ooch_t tmp = ch; \
	OUTPUT_STRX (pr, &tmp, 1); \
} while(0)

#define PRINT_STACK_ARRAY_END    0
#define PRINT_STACK_CONS         1
#define PRINT_STACK_ARRAY        2

typedef struct print_stack_t print_stack_t;
struct print_stack_t
{
	int type;
	hcl_oop_t obj;
	hcl_oow_t idx;
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
	HCL_ASSERT (hcl->p.s.size > 0);
	hcl->p.s.size--;
	*info = ((print_stack_t*)hcl->p.s.ptr)[hcl->p.s.size];
}

static hcl_oow_t long_to_str (
	hcl_ooi_t value, int radix, 
	const hcl_ooch_t* prefix, hcl_ooch_t* buf, hcl_oow_t size)
{
	hcl_ooi_t t, rem;
	hcl_oow_t len, ret, i;
	hcl_oow_t prefix_len;

	prefix_len = (prefix != HCL_NULL)? hcl_countoocstr(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == HCL_NULL) 
		{
			/* if buf is not given, 
			 * return the number of bytes required */
			return prefix_len + 1;
		}

		if (size < prefix_len + 1) 
		{
			/* buffer too small */
			return (hcl_oow_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = '0';
		if (size > prefix_len+1) buf[prefix_len+1] = '\0';
		return prefix_len+1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == HCL_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (hcl_oow_t)-1; /* buffer too small */
	if (size > len) buf[len] = '\0';
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (hcl_ooch_t)rem + 'a' - 10;
		else
			buf[--len] = (hcl_ooch_t)rem + '0';
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = '-';
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}

static HCL_INLINE int print_ooi (printer_t* pr, hcl_ooi_t nval)
{
	hcl_ooch_t tmp[HCL_SIZEOF(hcl_ooi_t)*8+2];
	hcl_oow_t len;

	len = long_to_str (nval, 10, HCL_NULL, tmp, HCL_COUNTOF(tmp));
	OUTPUT_STRX (pr, tmp, len);
	return 0;
}

static HCL_INLINE int print_char (printer_t* pr, hcl_ooch_t ch)
{
	OUTPUT_CHAR (pr, ch);
	return 0;
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

static int print_object (printer_t* pr, hcl_oop_t obj)
{
	hcl_t* hcl;
	hcl_oop_t cur;
	print_stack_t ps;
	int brand;

	hcl = pr->hcl;

next:
	if (HCL_OOP_IS_SMOOI(obj))
	{
		if (print_ooi (pr, HCL_OOP_TO_SMOOI(obj)) <= -1) return -1;
		goto done;
	}
	else if (HCL_OOP_IS_CHAR(obj))
	{
		if (print_char (pr, HCL_OOP_TO_CHAR(obj)) <= -1) return -1;
		goto done;
	}

	switch ((brand = HCL_OBJ_GET_FLAGS_BRAND(obj))) 
	{
		case HCL_BRAND_NIL:
			OUTPUT_STRX (pr, word[WORD_NIL].ptr, word[WORD_NIL].len);
			break;

		case HCL_BRAND_TRUE:
			OUTPUT_STRX (pr, word[WORD_TRUE].ptr, word[WORD_TRUE].len);
			break;

		case HCL_BRAND_FALSE:
			OUTPUT_STRX (pr, word[WORD_FALSE].ptr, word[WORD_FALSE].len);
			break;

		case HCL_BRAND_INTEGER:
			HCL_ASSERT (HCL_OBJ_GET_SIZE(obj) == 1);
			if (print_ooi (pr, ((hcl_oop_word_t)obj)->slot[0]) <= -1) return -1;
			break;

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
			OUTPUT_STRX (pr, ((hcl_oop_char_t)obj)->slot, HCL_OBJ_GET_SIZE(obj));
			break;

		case HCL_BRAND_STRING:
			OUTPUT_CHAR (pr, '\"');
			/* TODO: deescaping */
			OUTPUT_STRX (pr, ((hcl_oop_char_t)obj)->slot, HCL_OBJ_GET_SIZE(obj));
			OUTPUT_CHAR (pr, '\"');
			break;

		case HCL_BRAND_CONS:
		{
			OUTPUT_CHAR (pr, '(');
			cur = obj;

			do
			{
				int x;

				/* Push what to print next on to the stack 
				 * the variable p is */
				ps.type = PRINT_STACK_CONS;
				ps.obj = HCL_CONS_CDR(cur);
				x = push (hcl, &ps);
				if (x <= -1) return -1;

				obj = HCL_CONS_CAR(cur);
				/* Jump to the 'next' label so that the object 
				 * pointed to by 'obj' is printed. Once it 
				 * ends, a jump back to the 'resume' label
				 * is made at the at of this function. */
				goto next; 

			resume_cons:
				HCL_ASSERT (ps.type == PRINT_STACK_CONS);
				cur = ps.obj; /* Get back the CDR pushed */
				if (HCL_IS_NIL(hcl,cur)) 
				{
					/* The CDR part points to a NIL object, which
					 * indicates the end of a list. break the loop */
					break;
				}
				if (!HCL_OOP_IS_POINTER(cur) || HCL_OBJ_GET_FLAGS_BRAND(cur) != HCL_BRAND_CONS) 
				{
					/* The CDR part does not point to a pair. */
					OUTPUT_CHAR (pr, ' ');
					OUTPUT_CHAR (pr, '.');
					OUTPUT_CHAR (pr, ' ');

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
				OUTPUT_CHAR (pr, ' ');
			}
			while (1);
			OUTPUT_CHAR (pr, ')');
			break;
		}

		case HCL_BRAND_ARRAY:
		{
			hcl_oow_t arridx;

			if (brand == HCL_BRAND_ARRAY)
			{
				OUTPUT_CHAR (pr, '#');
				OUTPUT_CHAR (pr, '(');
			}
			else
			{
				OUTPUT_CHAR (pr, '|');
			}

			if (HCL_OBJ_GET_SIZE(obj) <= 0) 
			{
				if (brand == HCL_BRAND_ARRAY)
					OUTPUT_CHAR (pr, ')');
				else
					OUTPUT_CHAR (pr, '|');
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
					HCL_ASSERT (ps.type == PRINT_STACK_ARRAY);
					ps.obj = obj;
				}
				
				x = push (hcl, &ps);
				if (x <= -1) return -1;

				obj = ((hcl_oop_oop_t)obj)->slot[arridx];
				if (arridx > 0) OUTPUT_CHAR (pr, ' ');
				/* Jump to the 'next' label so that the object 
				 * pointed to by 'obj' is printed. Once it 
				 * ends, a jump back to the 'resume' label
				 * is made at the end of this function. */
				goto next; 

			resume_array:
				HCL_ASSERT (ps.type == PRINT_STACK_ARRAY);
				arridx = ps.idx;
				obj = ps.obj;
			} 
			while (1);
			break;
		}

		case HCL_BRAND_BYTE_ARRAY:
		{
			hcl_oow_t i;

			OUTPUT_CHAR (pr, '#');
			OUTPUT_CHAR (pr, '[');

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				if (i > 0) OUTPUT_CHAR (pr, ' ');
				if (print_ooi (pr, ((hcl_oop_byte_t)obj)->slot[i]) <= -1) return -1;
			}
			OUTPUT_CHAR (pr, ']');
			break;
		}

		case HCL_BRAND_SYMBOL_ARRAY:
		{
			hcl_oow_t i;

			OUTPUT_CHAR (pr, '|');

			for (i = 0; i < HCL_OBJ_GET_SIZE(obj); i++)
			{
				hcl_oop_t s;
				s = ((hcl_oop_oop_t)obj)->slot[i];
				OUTPUT_CHAR (pr, ' ');
				OUTPUT_STRX (pr, ((hcl_oop_char_t)s)->slot, HCL_OBJ_GET_SIZE(s));
			}
			OUTPUT_CHAR (pr, ' ');
			OUTPUT_CHAR (pr, '|');
			break;
		}

		case HCL_BRAND_SET:
			OUTPUT_STRX (pr, word[WORD_SET].ptr, word[WORD_SET].len);
			break;

#if 0
		case HCL_BRAND_PROCEDURE:
			OUTPUT_STR (pr, "#<PROCEDURE>");
			break;

		case HCL_BRAND_CLOSURE:
			OUTPUT_STR (pr, "#<CLOSURE>");
			break;
#endif


		case HCL_BRAND_CFRAME:
			OUTPUT_STRX (pr, word[WORD_CFRAME].ptr, word[WORD_CFRAME].len);
			break;

		case HCL_BRAND_PRIM:
			OUTPUT_STRX (pr, word[WORD_PRIM].ptr, word[WORD_PRIM].len);
			break;

		case HCL_BRAND_CONTEXT:
			OUTPUT_STRX (pr, word[WORD_CONTEXT].ptr, word[WORD_CONTEXT].len);
			break;

		case HCL_BRAND_PROCESS:
			OUTPUT_STRX (pr, word[WORD_PROCESS].ptr, word[WORD_PROCESS].len);
			break;

		case HCL_BRAND_PROCESS_SCHEDULER:
			OUTPUT_STRX (pr, word[WORD_PROCESS_SCHEDULER].ptr, word[WORD_PROCESS_SCHEDULER].len);
			break;

		case HCL_BRAND_SEMAPHORE:
			OUTPUT_STRX (pr, word[WORD_SEMAPHORE].ptr, word[WORD_SEMAPHORE].len);
			break;

		default:
			HCL_DEBUG3 (hcl, "Internal error - unknown object type %d at %s:%d\n", (int)brand, __FILE__, __LINE__);
			HCL_ASSERT ("Unknown object type" == HCL_NULL);
			hcl->errnum = HCL_EINTERN;
			return -1;
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
				OUTPUT_CHAR (pr, ')');
				break;

			default:
				HCL_DEBUG3 (hcl, "Internal error - unknown print stack type %d at %s:%d\n", (int)ps.type, __FILE__, __LINE__);
				hcl->errnum = HCL_EINTERN;
				return -1;
		}
	}

	return 0;
}

/* hcl_printobj() is for internal use only. it's called by hcl_print() and a logger. */
HCL_INLINE int hcl_printobj (hcl_t* hcl, hcl_oop_t obj, hcl_ioimpl_t printer, hcl_iooutarg_t* outarg)
{
	int n;
	printer_t pr;

	HCL_ASSERT (hcl->c->printer != HCL_NULL);

	/* the printer stack must be empty. buggy if not. */
	HCL_ASSERT (hcl->p.s.size == 0); 

	hcl->p.e = obj; /* remember the head of the object to print */
	pr.hcl = hcl;
	pr.printer = printer;
	pr.outarg = outarg;
	n = print_object (&pr, obj); /* call the actual printing routine */
	hcl->p.e = hcl->_nil; /* reset what's remembered */

	/* clear the printing stack if an error has occurred for GC not to keep
	 * the objects in the stack */
	if (n <= -1) hcl->p.s.size = 0;

	/* the printer stack must get empty when done. buggy if not */
	HCL_ASSERT (hcl->p.s.size == 0); 

	return n;
}

int hcl_print (hcl_t* hcl, hcl_oop_t obj)
{
	return hcl_printobj (hcl, obj, hcl->c->printer, &hcl->c->outarg);
}
