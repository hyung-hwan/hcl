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

#ifndef _HCL_PRV_H_
#define _HCL_PRV_H_

#include <hcl.h>
#include <hcl-fmt.h>
#include <hcl-utl.h>

/* you can define this to either 1 or 2 */
#define HCL_CODE_LONG_PARAM_SIZE 2

/* this is useful for debugging. hcl_gc() can be called 
 * while hcl has not been fully initialized when this is defined*/
#define HCL_SUPPORT_GC_DURING_IGNITION

/* define this to enable karatsuba multiplication in bigint */
#define HCL_ENABLE_KARATSUBA
#define HCL_KARATSUBA_CUTOFF 32
#define HCL_KARATSUBA_CUTOFF_DEBUG 3

/* enable floating-pointer number support in the basic formatting functions */
#define HCL_ENABLE_FLTFMT

#if defined(HCL_BUILD_DEBUG)
/*#define HCL_DEBUG_LEXER 1*/
#define HCL_DEBUG_VM_PROCESSOR 1
#define HCL_DEBUG_VM_EXEC 1
/*#define HCL_PROFILE_VM 1*/
#endif

/* allow the caller to drive process switching by calling
 * stix_switchprocess(). */
#define HCL_EXTERNAL_PROCESS_SWITCH

/* limit the maximum object size such that:
 *   1. an index to an object field can be represented in a small integer.
 *   2. the maximum number of bits including bit-shifts can be represented
 *      in the hcl_oow_t type.
 */
#define HCL_LIMIT_OBJ_SIZE


#define HCL_BC_BUFFER_INIT  10240
#define HCL_BC_BUFFER_ALIGN 10240

#define HCL_LIT_BUFFER_INIT 1024
#define HCL_LIT_BUFFER_ALIGN 1024

#if defined(__has_builtin)

#	if (!__has_builtin(__builtin_memset) || !__has_builtin(__builtin_memcpy) || !__has_builtin(__builtin_memmove) || !__has_builtin(__builtin_memcmp))
#	include <string.h>
#	endif

#	if __has_builtin(__builtin_memset)
#		define HCL_MEMSET(dst,src,size)  __builtin_memset(dst,src,size)
#	else
#		define HCL_MEMSET(dst,src,size)  memset(dst,src,size)
#	endif
#	if __has_builtin(__builtin_memcpy)
#		define HCL_MEMCPY(dst,src,size)  __builtin_memcpy(dst,src,size)
#	else
#		define HCL_MEMCPY(dst,src,size)  memcpy(dst,src,size)
#	endif
#	if __has_builtin(__builtin_memmove)
#		define HCL_MEMMOVE(dst,src,size)  __builtin_memmove(dst,src,size)
#	else
#		define HCL_MEMMOVE(dst,src,size)  memmove(dst,src,size)
#	endif
#	if __has_builtin(__builtin_memcmp)
#		define HCL_MEMCMP(dst,src,size)  __builtin_memcmp(dst,src,size)
#	else
#		define HCL_MEMCMP(dst,src,size)  memcmp(dst,src,size)
#	endif

#elif defined(__GNUC__) && (__GNUC__  >= 3 || (defined(__GNUC_MINOR) && __GNUC__ == 2 && __GNUC_MINOR__ >= 91))
	/* gcc 2.91 or higher */
#	define HCL_MEMSET(dst,src,size)  __builtin_memset(dst,src,size)
#	define HCL_MEMCPY(dst,src,size)  __builtin_memcpy(dst,src,size)
#	define HCL_MEMMOVE(dst,src,size) __builtin_memmove(dst,src,size)
#	define HCL_MEMCMP(dst,src,size)  __builtin_memcmp(dst,src,size)

#else
#	include <string.h>
#	define HCL_MEMSET(dst,src,size)  memset(dst,src,size)
#	define HCL_MEMCPY(dst,src,size)  memcpy(dst,src,size)
#	define HCL_MEMMOVE(dst,src,size) memmove(dst,src,size)
#	define HCL_MEMCMP(dst,src,size)  memcmp(dst,src,size)
#endif

#if defined(HCL_LIMIT_OBJ_SIZE)
/* limit the maximum object size such that:
 *   1. an index to an object field can be represented in a small integer.
 *   2. the maximum number of bit shifts can be represented in the hcl_oow_t type.
 */
#	define HCL_OBJ_SIZE_MAX ((hcl_oow_t)HCL_SMOOI_MAX)
#	define HCL_OBJ_SIZE_BITS_MAX (HCL_OBJ_SIZE_MAX * HCL_BITS_PER_BYTE)
#else
#	define HCL_OBJ_SIZE_MAX ((hcl_oow_t)HCL_TYPE_MAX(hcl_oow_t))
#	define HCL_OBJ_SIZE_BITS_MAX (HCL_OBJ_SIZE_MAX * HCL_BITS_PER_BYTE)
#endif


#if defined(HCL_INCLUDE_COMPILER)

/* ========================================================================= */
/* SOURCE CODE I/O FOR COMPILER                                              */
/* ========================================================================= */
enum hcl_iotok_type_t
{
	HCL_IOTOK_EOF,
	HCL_IOTOK_CHARLIT,
	HCL_IOTOK_STRLIT,
	HCL_IOTOK_NUMLIT,
	HCL_IOTOK_RADNUMLIT,
	HCL_IOTOK_FPDECLIT,
	HCL_IOTOK_SMPTRLIT,
	HCL_IOTOK_ERRLIT,
	HCL_IOTOK_NIL,
	HCL_IOTOK_TRUE,
	HCL_IOTOK_FALSE,

	HCL_IOTOK_IDENT,
	HCL_IOTOK_IDENT_DOTTED,
	HCL_IOTOK_DOT,
	HCL_IOTOK_COLON,
	HCL_IOTOK_COMMA,
	HCL_IOTOK_LPAREN,
	HCL_IOTOK_RPAREN,
	HCL_IOTOK_BAPAREN,  /* #[ */
	HCL_IOTOK_QLPAREN,  /* #( */
	HCL_IOTOK_LBRACK,   /* [ */
	HCL_IOTOK_RBRACK,   /* ] */
	HCL_IOTOK_LBRACE,   /* { */
	HCL_IOTOK_RBRACE,   /* } */
	HCL_IOTOK_VBAR,
	HCL_IOTOK_EOL,      /* end of line */

	HCL_IOTOK_INCLUDE
};
typedef enum hcl_iotok_type_t hcl_iotok_type_t;

typedef struct hcl_iotok_t hcl_iotok_t;
struct hcl_iotok_t
{
	hcl_iotok_type_t type;
	hcl_oocs_t name;
	hcl_oow_t name_capa;
	hcl_ioloc_t loc;
};


typedef struct hcl_iolink_t hcl_iolink_t;
struct hcl_iolink_t
{
	hcl_iolink_t* link;
};

enum hcl_cnode_type_t
{
	HCL_CNODE_CHARLIT,
	HCL_CNODE_SYMBOL,
	HCL_CNODE_DSYMBOL, /* dotted symbol */
	HCL_CNODE_STRLIT,
	HCL_CNODE_NUMLIT,
	HCL_CNODE_RADNUMLIT,
	HCL_CNODE_FPDECLIT,
	HCL_CNODE_SMPTRLIT,
	HCL_CNODE_ERRLIT,
	HCL_CNODE_NIL,
	HCL_CNODE_TRUE,
	HCL_CNODE_FALSE,

	HCL_CNODE_CONS,
	HCL_CNODE_ELIST, /* empty list */
	HCL_CNODE_SHELL  /* pseudo-node to hold another actual node */
};
typedef enum hcl_cnode_type_t hcl_cnode_type_t;

#define HCL_CNODE_GET_TYPE(x) ((x)->cn_type)
#define HCL_CNODE_GET_LOC(x) (&(x)->cn_loc)
#define HCL_CNODE_GET_TOK(x) (&(x)->cn_tok)
#define HCL_CNODE_GET_TOKPTR(x) ((x)->cn_tok.ptr)
#define HCL_CNODE_GET_TOKLEN(x) ((x)->cn_tok.len)

#define HCL_CNODE_IS_SYMBOL(x) ((x)->cn_type == HCL_CNODE_SYMBOL)
#define HCL_CNODE_IS_SYMBOL_SYNCODED(x, code) ((x)->cn_type == HCL_CNODE_SYMBOL && (x)->u.symbol.syncode == (code))
#define HCL_CNODE_SYMBOL_SYNCODE(x) ((x)->u.symbol.syncode)

#define HCL_CNODE_IS_DSYMBOL(x) ((x)->cn_type == HCL_CNODE_DSYMBOL)

#define HCL_CNODE_IS_CONS(x) ((x)->cn_type == HCL_CNODE_CONS)
#define HCL_CNODE_IS_CONS_CONCODED(x, code) ((x)->cn_type == HCL_CNODE_CONS && (x)->u.cons.concode == (code))
#define HCL_CNODE_CONS_CONCODE(x) ((x)->u.cons.concode)
#define HCL_CNODE_CONS_CAR(x) ((x)->u.cons.car)
#define HCL_CNODE_CONS_CDR(x) ((x)->u.cons.cdr)


#define HCL_CNODE_IS_ELIST(x) ((x)->cn_type == HCL_CNODE_ELIST)
#define HCL_CNODE_IS_ELIST_CONCODED(x, code) ((x)->cn_type == HCL_CNODE_ELIST && (x)->u.elist.concode == (code))
#define HCL_CNODE_ELIST_CONCODE(x) ((x)->u.elist.concode)

/* NOTE: hcl_cnode_t used by the built-in compiler is not an OOP object */
struct hcl_cnode_t
{
	hcl_cnode_type_t cn_type;
	hcl_ioloc_t cn_loc;
	hcl_oocs_t cn_tok;

	union
	{
		struct
		{
			hcl_ooch_t v;
		} charlit;

		struct
		{
			hcl_syncode_t syncode; /* special if non-zero */
		} symbol;

		struct
		{
			hcl_oow_t v;
		} smptrlit;
		struct
		{
			hcl_ooi_t v;
		} errlit;
		struct
		{
			hcl_concode_t concode;
			hcl_cnode_t* car;
			hcl_cnode_t* cdr;
		} cons;
		struct
		{
			hcl_concode_t concode;
		} elist;
		struct
		{
			hcl_cnode_t* obj;
		} shell;
	} u;
};

/* NOTE: hcl_cframe_t used by the built-in compiler is not an OOP object */
struct hcl_cframe_t
{
	int          opcode;
	hcl_cnode_t* operand;

	union
	{
		/* COP_EMIT_CALL */
		struct
		{
			hcl_ooi_t index;
		} call;

		/* COP_EMIT_SET */
		struct
		{
			int var_type;
			hcl_ooi_t index;
		} set;

		struct
		{
			hcl_ooi_t cond_pos;
			hcl_ooi_t body_pos;
			hcl_ooi_t jump_inst_pos;
			hcl_ioloc_t start_loc;
		} post_while;

		struct
		{
			hcl_ooi_t body_pos;
			hcl_ooi_t jump_inst_pos;
			hcl_ioloc_t start_loc;
		} post_if;

		struct
		{
			hcl_ooi_t jump_inst_pos;
		} post_and;

		struct
		{
			hcl_ooi_t jump_inst_pos;
		} post_or;

		/* COP_COMPILE_ARRAY_LIST, COP_POP_INTO_ARRAY, COP_EMIT_MAKE_ARRAY */
		struct
		{
			hcl_ooi_t index;
		} array_list;

		/* COP_COMPILE_BYTEARRAY_LIST, COP_POP_INTO_BYTEARRAY, COP_EMIT_MAKE_BYTEARRAY */
		struct
		{
			hcl_ooi_t index;
		} bytearray_list;

		/* COP_EMIT_MAKE_DIC */
		struct
		{
			hcl_ooi_t index;
		} dic_list;

		/* COP_EMIT_LAMBDA */
		struct
		{
			hcl_oow_t jump_inst_pos;
			hcl_ooi_t lfbase_pos;
			hcl_ooi_t lfsize_pos;
		} lambda;

		/* COP_EMIT_RETURN */
		struct
		{
			int from_home;
		} _return;

		/* COP_POST_BREAK */
		struct
		{
			hcl_ooi_t jump_inst_pos;
		} _break;
	} u;
};
typedef struct hcl_cframe_t hcl_cframe_t;

struct hcl_blk_info_t
{
	hcl_oow_t tmprlen;
	hcl_oow_t tmprcnt;
	hcl_oow_t lfbase;
};
typedef struct hcl_blk_info_t hcl_blk_info_t;

typedef struct hcl_rstl_t hcl_rstl_t;
struct hcl_rstl_t /* reader stack for list reading */
{
	hcl_cnode_t* head;
	hcl_cnode_t* tail;
	hcl_ioloc_t loc;
	int flagv;
	hcl_oow_t count;
	hcl_rstl_t* prev;
};

struct hcl_compiler_t
{
	/* output handler */
	hcl_ioimpl_t printer;
	hcl_iooutarg_t outarg;

	/* input handler */
	hcl_ioimpl_t reader;

	/* static input data buffer */
	hcl_ioinarg_t  inarg;    

	/* pointer to the current input data. initially, it points to &inarg */
	hcl_ioinarg_t* curinp;

	/* information about the last meaningful character read.
	 * this is a copy of curinp->lxc if no ungetting is performed.
	 * if there is something in the unget buffer, this is overwritten
	 * by a value from the buffer when the request to read a character
	 * is served */
	hcl_iolxc_t  lxc;

	/* unget buffer */
	hcl_iolxc_t  ungot[10];
	int          nungots;

	/* the last token read */
	hcl_iotok_t  tok;
	hcl_iolink_t* io_names;

	hcl_synerr_t synerr;

	/* temporary space to handle an illegal character */
	hcl_ooch_t ilchr;
	hcl_oocs_t ilchr_ucs;

	/* == READER == */
	struct
	{
		hcl_oop_t s;  /* stack for reading */
		hcl_oop_t e;  /* last object read */
		hcl_rstl_t* st;
	} r; /* reading */
	/* == END READER == */

	/* == COMPILER STACK == */
	struct
	{
		hcl_cframe_t* ptr;
		hcl_ooi_t     top;
		hcl_oow_t     capa;
	} cfs;
	/* == END COMPILER STACK == */

	struct
	{
		hcl_oocs_t s; /* buffer */
		hcl_oow_t capa; /* bufer capacity */
		hcl_oow_t wcount; /* word count */
	} tv; /* temporary variables including arguments */

	struct
	{
		hcl_ooi_t depth;
		hcl_blk_info_t* info;
		hcl_oow_t  info_capa;
	} blk; /* lambda block */
};
#endif


#if defined(HCL_CODE_LONG_PARAM_SIZE) && (HCL_CODE_LONG_PARAM_SIZE == 1)
#	define MAX_CODE_INDEX               (0xFFu)
#	define MAX_CODE_NTMPRS              (0xFFu)
#	define MAX_CODE_NARGS               (0xFFu)
#	define MAX_CODE_NBLKARGS            (0xFFu)
#	define MAX_CODE_NBLKTMPRS           (0xFFu)
#	define MAX_CODE_JUMP                (0xFFu)
#	define MAX_CODE_PARAM               (0xFFu)
#	define MAX_CODE_PARAM2              (0xFFFFu)
#elif defined(HCL_CODE_LONG_PARAM_SIZE) && (HCL_CODE_LONG_PARAM_SIZE == 2)
#	define MAX_CODE_INDEX               (0xFFFFu)
#	define MAX_CODE_NTMPRS              (0xFFFFu)
#	define MAX_CODE_NARGS               (0xFFFFu)
#	define MAX_CODE_NBLKARGS            (0xFFFFu)
#	define MAX_CODE_NBLKTMPRS           (0xFFFFu)
#	define MAX_CODE_JUMP                (0xFFFFu)
#	define MAX_CODE_PARAM               (0xFFFFu)
#	define MAX_CODE_PARAM2              (0xFFFFFFFFu)
#else
#	error Unsupported HCL_CODE_LONG_PARAM_SIZE
#endif


/*
----------------------------------------------------------------------------------------------------------------
SHORT INSTRUCTION CODE                                        LONG INSTRUCTION CODE
----------------------------------------------------------------------------------------------------------------
                                                                      v v
0-3      0000 00XX STORE_INTO_INSTVAR                         128  1000 0000 XXXXXXXX STORE_INTO_INSTVAR_X                    (bit 4 off, bit 3 off) 
4-7      0000 01XX STORE_INTO_INSTVAR
8-11     0000 10XX POP_INTO_INSTVAR                           136  1000 1000 XXXXXXXX POP_INTO_INSTVAR_X                      (bit 4 off, bit 3 on)
12-15    0000 11XX POP_INTO_INSTVAR
16-19    0001 00XX PUSH_INSTVAR                               144  1001 0000 XXXXXXXX PUSH_INSTVAR_X                          (bit 4 on)
20-23    0001 01XX PUSH_INSTVAR

                                                                      v v
24-27    0001 10XX PUSH_TEMPVAR                               152  1001 1000 XXXXXXXX PUSH_TEMPVAR_X                          (bit 4 on)
28-31    0001 11XX PUSH_TEMPVAR
32-35    0010 00XX STORE_INTO_TEMPVAR                         160  1010 0000 XXXXXXXX STORE_INTO_TEMPVAR_X                    (bit 4 off, bit 3 off)
36-39    0010 01XX STORE_INTO_TEMPVAR
40-43    0010 10XX POP_INTO_TEMPVAR                           168  1010 1000 XXXXXXXX POP_INTO_TEMPVAR_X                      (bit 4 off, bit 3 on)
44-47    0010 11XX POP_INTO_TEMPVAR

48-51    0011 00XX PUSH_LITERAL                               176  1011 0000 XXXXXXXX PUSH_LITERAL_X
52-55    0011 01XX PUSH_LITERAL                               177  1011 0001 XXXXXXXX XXXXXXXX PUSH_LITERAL_X2

                                                                        vv
56-59    0011 10XX STORE_INTO_OBJECT                          184  1011 1000 XXXXXXXX STORE_INTO_OBJECT                       (bit 3 on, bit 2 off)
60-63    0011 11XX POP_INTO_OBJECT                            188  1011 1100 XXXXXXXX POP_INTO_OBJECT                         (bit 3 on, bit 2 on)
64-67    0100 00XX PUSH_OBJECT                                192  1100 0000 XXXXXXXX PUSH_OBJECT                             (bit 3 off)


68-71    0100 01XX JUMP_FORWARD                               196  1100 0100 XXXXXXXX JUMP_FORWARD_X
                                                              197  1100 0101 XXXXXXXX JUMP2_FORWARD
72-75    0100 10XX JUMP_BACKWARD                              200  1100 1000 XXXXXXXX JUMP_BACKWARD_X
                                                              201  1100 1001 XXXXXXXX JUMP2_BACKWARD
76-79    0100 11XX UNUSED                                     204  1100 1100 XXXXXXXX JUMP_FORWARD_IF_TRUE
                                                              205  1100 1101 XXXXXXXX JUMP2_FORWARD_IF_TRUE
                                                              206  1100 1110 XXXXXXXX JUMP_BACKWARD_IF_TRUE
                                                              207  1100 1111 XXXXXXXX JUMP2_BACKWARD_IF_TRUE
80-83    0101 00XX UNUSED                                     208  1101 0000 XXXXXXXX JUMP_FORWARD_IF_FALSE
                                                              209  1101 0001 XXXXXXXX JUMP2_FORWARD_IF_FALSE
                                                              210  1101 0010 XXXXXXXX JUMP_BACKWARD_IF_FALSE
                                                              211  1101 0011 XXXXXXXX JUMP2_BACKWARD_IF_FALSE

84-87    0101 01XX CALL                                       212  1101 0100 XXXXXXXX CALL_X

                                                                        vv
88-91    0101 10XX YYYYYYYY STORE_INTO_CTXTEMPVAR             216  1101 1000 XXXXXXXX YYYYYYYY STORE_INTO_CTXTEMPVAR_X        (bit 3 on, bit 2 off)
92-95    0101 11XX YYYYYYYY POP_INTO_CTXTEMPVAR               220  1101 1100 XXXXXXXX YYYYYYYY POP_INTO_CTXTEMPVAR_X          (bit 3 on, bit 2 on)
96-99    0110 00XX YYYYYYYY PUSH_CTXTEMPVAR                   224  1110 0000 XXXXXXXX YYYYYYYY PUSH_CTXTEMPVAR_X              (bit 3 off)
# XXXth outer-frame, YYYYYYYY local variable

100-103  0110 01XX YYYYYYYY PUSH_OBJVAR                       228  1110 0100 XXXXXXXX YYYYYYYY PUSH_OBJVAR_X                  (bit 3 off)
104-107  0110 10XX YYYYYYYY STORE_INTO_OBJVAR                 232  1110 1000 XXXXXXXX YYYYYYYY STORE_INTO_OBJVAR_X            (bit 3 on, bit 2 off)
108-111  0110 11XX YYYYYYYY POP_INTO_OBJVAR                   236  1110 1100 XXXXXXXX YYYYYYYY POP_INTO_OBJVAR_X              (bit 3 on, bit 2 on)
# XXXth instance variable of YYYYYYYY object

                                                                         v
112-115  0111 00XX YYYYYYYY SEND_MESSAGE                      240  1111 0000 XXXXXXXX YYYYYYYY SEND_MESSAGE_X                 (bit 2 off)
116-119  0111 01XX YYYYYYYY SEND_MESSAGE_TO_SUPER             244  1111 0100 XXXXXXXX YYYYYYYY SEND_MESSAGE_TO_SUPER_X        (bit 2 on)
# XXX args, YYYYYYYY message

120-123  0111 10XX  UNUSED
124-127  0111 11XX  UNUSED

##
## "SHORT_CODE_0 | 0x80" becomes "LONG_CODE_X".
## A special single byte instruction is assigned an unused number greater than 128.
##
*/

enum hcl_bcode_t
{
	HCL_CODE_STORE_INTO_INSTVAR_0     = 0x00,
	HCL_CODE_STORE_INTO_INSTVAR_1     = 0x01,
	HCL_CODE_STORE_INTO_INSTVAR_2     = 0x02,
	HCL_CODE_STORE_INTO_INSTVAR_3     = 0x03,

	HCL_CODE_STORE_INTO_INSTVAR_4     = 0x04,
	HCL_CODE_STORE_INTO_INSTVAR_5     = 0x05,
	HCL_CODE_STORE_INTO_INSTVAR_6     = 0x06,
	HCL_CODE_STORE_INTO_INSTVAR_7     = 0x07,

	HCL_CODE_POP_INTO_INSTVAR_0       = 0x08,
	HCL_CODE_POP_INTO_INSTVAR_1       = 0x09,
	HCL_CODE_POP_INTO_INSTVAR_2       = 0x0A,
	HCL_CODE_POP_INTO_INSTVAR_3       = 0x0B,

	HCL_CODE_POP_INTO_INSTVAR_4       = 0x0C,
	HCL_CODE_POP_INTO_INSTVAR_5       = 0x0D,
	HCL_CODE_POP_INTO_INSTVAR_6       = 0x0E,
	HCL_CODE_POP_INTO_INSTVAR_7       = 0x0F,

	HCL_CODE_PUSH_INSTVAR_0           = 0x10,
	HCL_CODE_PUSH_INSTVAR_1           = 0x11,
	HCL_CODE_PUSH_INSTVAR_2           = 0x12,
	HCL_CODE_PUSH_INSTVAR_3           = 0x13,

	HCL_CODE_PUSH_INSTVAR_4           = 0x14,
	HCL_CODE_PUSH_INSTVAR_5           = 0x15,
	HCL_CODE_PUSH_INSTVAR_6           = 0x16,
	HCL_CODE_PUSH_INSTVAR_7           = 0x17,

	HCL_CODE_PUSH_TEMPVAR_0           = 0x18,
	HCL_CODE_PUSH_TEMPVAR_1           = 0x19,
	HCL_CODE_PUSH_TEMPVAR_2           = 0x1A,
	HCL_CODE_PUSH_TEMPVAR_3           = 0x1B,

	HCL_CODE_PUSH_TEMPVAR_4           = 0x1C,
	HCL_CODE_PUSH_TEMPVAR_5           = 0x1D,
	HCL_CODE_PUSH_TEMPVAR_6           = 0x1E,
	HCL_CODE_PUSH_TEMPVAR_7           = 0x1F,

	HCL_CODE_STORE_INTO_TEMPVAR_0     = 0x20,
	HCL_CODE_STORE_INTO_TEMPVAR_1     = 0x21,
	HCL_CODE_STORE_INTO_TEMPVAR_2     = 0x22,
	HCL_CODE_STORE_INTO_TEMPVAR_3     = 0x23,

	HCL_CODE_STORE_INTO_TEMPVAR_4     = 0x24,
	HCL_CODE_STORE_INTO_TEMPVAR_5     = 0x25,
	HCL_CODE_STORE_INTO_TEMPVAR_6     = 0x26,
	HCL_CODE_STORE_INTO_TEMPVAR_7     = 0x27,

	HCL_CODE_POP_INTO_TEMPVAR_0       = 0x28,
	HCL_CODE_POP_INTO_TEMPVAR_1       = 0x29,
	HCL_CODE_POP_INTO_TEMPVAR_2       = 0x2A,
	HCL_CODE_POP_INTO_TEMPVAR_3       = 0x2B,

	HCL_CODE_POP_INTO_TEMPVAR_4       = 0x2C,
	HCL_CODE_POP_INTO_TEMPVAR_5       = 0x2D,
	HCL_CODE_POP_INTO_TEMPVAR_6       = 0x2E,
	HCL_CODE_POP_INTO_TEMPVAR_7       = 0x2F,

	HCL_CODE_PUSH_LITERAL_0           = 0x30,
	HCL_CODE_PUSH_LITERAL_1           = 0x31,
	HCL_CODE_PUSH_LITERAL_2           = 0x32,
	HCL_CODE_PUSH_LITERAL_3           = 0x33,

	HCL_CODE_PUSH_LITERAL_4           = 0x34,
	HCL_CODE_PUSH_LITERAL_5           = 0x35,
	HCL_CODE_PUSH_LITERAL_6           = 0x36,
	HCL_CODE_PUSH_LITERAL_7           = 0x37,

	/* -------------------------------------- */

	HCL_CODE_STORE_INTO_OBJECT_0      = 0x38,
	HCL_CODE_STORE_INTO_OBJECT_1      = 0x39,
	HCL_CODE_STORE_INTO_OBJECT_2      = 0x3A,
	HCL_CODE_STORE_INTO_OBJECT_3      = 0x3B,

	HCL_CODE_POP_INTO_OBJECT_0        = 0x3C,
	HCL_CODE_POP_INTO_OBJECT_1        = 0x3D,
	HCL_CODE_POP_INTO_OBJECT_2        = 0x3E,
	HCL_CODE_POP_INTO_OBJECT_3        = 0x3F,

	HCL_CODE_PUSH_OBJECT_0            = 0x40,
	HCL_CODE_PUSH_OBJECT_1            = 0x41,
	HCL_CODE_PUSH_OBJECT_2            = 0x42,
	HCL_CODE_PUSH_OBJECT_3            = 0x43,

	HCL_CODE_JUMP_FORWARD_0           = 0x44, /* 68 */
	HCL_CODE_JUMP_FORWARD_1           = 0x45, /* 69 */
	HCL_CODE_JUMP_FORWARD_2           = 0x46, /* 70 */
	HCL_CODE_JUMP_FORWARD_3           = 0x47, /* 71 */

	HCL_CODE_JUMP_BACKWARD_0          = 0x48, /* 72 */
	HCL_CODE_JUMP_BACKWARD_1          = 0x49, /* 73 */
	HCL_CODE_JUMP_BACKWARD_2          = 0x4A, /* 74 */
	HCL_CODE_JUMP_BACKWARD_3          = 0x4B, /* 75 */

	/* UNUSED 0x4C - 0x53 */

	HCL_CODE_CALL_0                   = 0x54, /* 84 */
	HCL_CODE_CALL_1                   = 0x55, /* 85 */
	HCL_CODE_CALL_2                   = 0x56, /* 86 */
	HCL_CODE_CALL_3                   = 0x57, /* 87 */

	HCL_CODE_STORE_INTO_CTXTEMPVAR_0  = 0x58, /* 88 */
	HCL_CODE_STORE_INTO_CTXTEMPVAR_1  = 0x59, /* 89 */
	HCL_CODE_STORE_INTO_CTXTEMPVAR_2  = 0x5A, /* 90 */
	HCL_CODE_STORE_INTO_CTXTEMPVAR_3  = 0x5B, /* 91 */

	HCL_CODE_POP_INTO_CTXTEMPVAR_0    = 0x5C, /* 92 */
	HCL_CODE_POP_INTO_CTXTEMPVAR_1    = 0x5D, /* 93 */
	HCL_CODE_POP_INTO_CTXTEMPVAR_2    = 0x5E, /* 94 */
	HCL_CODE_POP_INTO_CTXTEMPVAR_3    = 0x5F, /* 95 */

	HCL_CODE_PUSH_CTXTEMPVAR_0        = 0x60, /* 96 */
	HCL_CODE_PUSH_CTXTEMPVAR_1        = 0x61, /* 97 */
	HCL_CODE_PUSH_CTXTEMPVAR_2        = 0x62, /* 98 */
	HCL_CODE_PUSH_CTXTEMPVAR_3        = 0x63, /* 99 */

	HCL_CODE_PUSH_OBJVAR_0            = 0x64,
	HCL_CODE_PUSH_OBJVAR_1            = 0x65,
	HCL_CODE_PUSH_OBJVAR_2            = 0x66,
	HCL_CODE_PUSH_OBJVAR_3            = 0x67,

	HCL_CODE_STORE_INTO_OBJVAR_0      = 0x68,
	HCL_CODE_STORE_INTO_OBJVAR_1      = 0x69,
	HCL_CODE_STORE_INTO_OBJVAR_2      = 0x6A,
	HCL_CODE_STORE_INTO_OBJVAR_3      = 0x6B,

	HCL_CODE_POP_INTO_OBJVAR_0        = 0x6C,
	HCL_CODE_POP_INTO_OBJVAR_1        = 0x6D,
	HCL_CODE_POP_INTO_OBJVAR_2        = 0x6E,
	HCL_CODE_POP_INTO_OBJVAR_3        = 0x6F,

	HCL_CODE_SEND_MESSAGE_0           = 0x70,
	HCL_CODE_SEND_MESSAGE_1           = 0x71,
	HCL_CODE_SEND_MESSAGE_2           = 0x72,
	HCL_CODE_SEND_MESSAGE_3           = 0x73,

	HCL_CODE_SEND_MESSAGE_TO_SUPER_0  = 0x74,
	HCL_CODE_SEND_MESSAGE_TO_SUPER_1  = 0x75,
	HCL_CODE_SEND_MESSAGE_TO_SUPER_2  = 0x76,
	HCL_CODE_SEND_MESSAGE_TO_SUPER_3  = 0x77,

	/* UNUSED 0x78 - 0x7F */

	HCL_CODE_STORE_INTO_INSTVAR_X     = 0x80, /* 128 */


	HCL_CODE_PUSH_RECEIVER            = 0x81, /* 129 */
	HCL_CODE_PUSH_NIL                 = 0x82, /* 130 */
	HCL_CODE_PUSH_TRUE                = 0x83, /* 131 */
	HCL_CODE_PUSH_FALSE               = 0x84, /* 132 */
	HCL_CODE_PUSH_CONTEXT             = 0x85, /* 133 */
	HCL_CODE_PUSH_PROCESS             = 0x86, /* 134 */
	/* UNUSED 135 */

	HCL_CODE_POP_INTO_INSTVAR_X       = 0x88, /* 136 ## */

	HCL_CODE_PUSH_NEGONE              = 0x89, /* 137 */
	HCL_CODE_PUSH_ZERO                = 0x8A, /* 138 */
	HCL_CODE_PUSH_ONE                 = 0x8B, /* 139 */
	HCL_CODE_PUSH_TWO                 = 0x8C, /* 140 */

	HCL_CODE_PUSH_INSTVAR_X           = 0x90, /* 144 ## */
	HCL_CODE_PUSH_TEMPVAR_X           = 0x98, /* 152 ## */
	HCL_CODE_STORE_INTO_TEMPVAR_X     = 0xA0, /* 160 ## */
	HCL_CODE_POP_INTO_TEMPVAR_X       = 0xA8, /* 168 ## */

	HCL_CODE_PUSH_LITERAL_X           = 0xB0, /* 176 ## */
	HCL_CODE_PUSH_LITERAL_X2          = 0xB1, /* 177 */

	HCL_CODE_PUSH_INTLIT              = 0xB2, /* 178 */
	HCL_CODE_PUSH_NEGINTLIT           = 0xB3, /* 179 */
	HCL_CODE_PUSH_CHARLIT             = 0xB4, /* 180 */

	/* UNUSED - 0xB5 - 0xB7 */

	HCL_CODE_STORE_INTO_OBJECT_X      = 0xB8, /* 184 ## */
	HCL_CODE_POP_INTO_OBJECT_X        = 0xBC, /* 188 ## */
	HCL_CODE_PUSH_OBJECT_X            = 0xC0, /* 192 ## */

	/* UNUSED - 0xC1 - 0xC3 */

	HCL_CODE_JUMP_FORWARD_X           = 0xC4, /* 196 ## */
	HCL_CODE_JUMP2_FORWARD            = 0xC5, /* 197 */

	/* UNUSED - 0xC6 - 0xC7 */

	HCL_CODE_JUMP_BACKWARD_X          = 0xC8, /* 200 ## */
	HCL_CODE_JUMP2_BACKWARD           = 0xC9, /* 201 */

	/* UNUSED - 0xCA - 0xCB */

	HCL_CODE_JUMP_FORWARD_IF_TRUE     = 0xCC, /* 204 ## */
	HCL_CODE_JUMP2_FORWARD_IF_TRUE    = 0xCD, /* 205 */
	HCL_CODE_JUMP_BACKWARD_IF_TRUE    = 0xCE, /* 206 ## */
	HCL_CODE_JUMP2_BACKWARD_IF_TRUE   = 0xCF, /* 207 */

	HCL_CODE_JUMP_FORWARD_IF_FALSE    = 0xD0, /* 208 ## */
	HCL_CODE_JUMP2_FORWARD_IF_FALSE   = 0xD1, /* 209 */
	HCL_CODE_JUMP_BACKWARD_IF_FALSE   = 0xD2, /* 210 ## */
	HCL_CODE_JUMP2_BACKWARD_IF_FALSE  = 0xD3, /* 211 */

	HCL_CODE_CALL_X                   = 0xD4, /* 212 */
	/* UNUSED - 0xD5 - 0xD7 */

	HCL_CODE_STORE_INTO_CTXTEMPVAR_X  = 0xD8, /* 216 ## */
	/* UNUSED - 0xD9 - 0xDB */

	HCL_CODE_POP_INTO_CTXTEMPVAR_X    = 0xDC, /* 220 ## */
	/* UNUSED - 0xDD - 0xDF */

	HCL_CODE_PUSH_CTXTEMPVAR_X        = 0xE0, /* 224 ## */
	/* UNUSED - 0xE1 - 0xE3 */

	HCL_CODE_PUSH_OBJVAR_X            = 0xE4, /* 228 ## */
	/* UNUSED - 0xE5 - 0xE7 */

	HCL_CODE_STORE_INTO_OBJVAR_X      = 0xE8, /* 232 ## */

	HCL_CODE_MAKE_ARRAY               = 0xE9, /* 233 ## */
	HCL_CODE_MAKE_BYTEARRAY           = 0xEA, /* 234 ## */
	HCL_CODE_MAKE_DIC                 = 0xEB, /* 235 ## */
	
	HCL_CODE_POP_INTO_OBJVAR_X        = 0xEC, /* 236 ## */

	HCL_CODE_POP_INTO_ARRAY           = 0xED, /* 237 ## */
	HCL_CODE_POP_INTO_BYTEARRAY       = 0xEE, /* 238 ## */
	HCL_CODE_POP_INTO_DIC             = 0xEF, /* 239 */

	HCL_CODE_SEND_MESSAGE_X           = 0xF0, /* 240 ## */

	HCL_CODE_MAKE_CONS                = 0xF1, /* 241 */
	HCL_CODE_POP_INTO_CONS            = 0xF2, /* 242 */
	HCL_CODE_POP_INTO_CONS_END        = 0xF3, /* 243 */

	HCL_CODE_SEND_MESSAGE_TO_SUPER_X  = 0xF4, /* 244 ## */

	HCL_CODE_POP_INTO_CONS_CDR        = 0xF5, /* 245 */
	/* -------------------------------------- */

	/* UNUSED 0xF6 */

	HCL_CODE_DUP_STACKTOP             = 0xF7,
	HCL_CODE_POP_STACKTOP             = 0xF8,
	HCL_CODE_RETURN_STACKTOP          = 0xF9, /* ^something */
	HCL_CODE_RETURN_RECEIVER          = 0xFA, /* ^self */
	HCL_CODE_RETURN_FROM_BLOCK        = 0xFB, /* return the stack top from a block */

	HCL_CODE_MAKE_FUNCTION            = 0xFC, /* 252 */
	HCL_CODE_MAKE_BLOCK               = 0xFD, /* 253 */
	/* UNUSED 254 */
	HCL_CODE_NOOP                     = 0xFF  /* 255 */
};



typedef hcl_ooi_t (*hcl_outbfmt_t) (
	hcl_t*           hcl,
	hcl_bitmask_t  mask,
	const hcl_bch_t* fmt,
	...
);

/* i don't want an error raised inside the callback to override 
 * the existing error number and message. */
#define vmprim_log_write(hcl,mask,ptr,len) do { \
		int shuterr = (hcl)->shuterr; \
		(hcl)->shuterr = 1; \
		(hcl)->vmprim.log_write (hcl, mask, ptr, len); \
		(hcl)->shuterr = shuterr; \
	} while(0)

#if defined(__cplusplus)
extern "C" {
#endif

/* ========================================================================= */
/* heap.c                                                                    */
/* ========================================================================= */

/**
 * The hcl_makeheap() function creates a new heap of the \a size bytes.
 *
 * \return heap pointer on success and #HCL_NULL on failure.
 */
hcl_heap_t* hcl_makeheap (
	hcl_t*     hcl, 
	hcl_oow_t  size
);

/**
 * The hcl_killheap() function destroys the heap pointed to by \a heap.
 */
void hcl_killheap (
	hcl_t*      hcl, 
	hcl_heap_t* heap
);

/** 
 * The hcl_allocheapmem() function allocates \a size bytes from the given heap
 * and clears it with zeros.
 */
void* hcl_callocheapmem (
	hcl_t*       hcl,
	hcl_heap_t*  heap,
	hcl_oow_t    size
);

void* hcl_callocheapmem_noseterr (
	hcl_t*       hcl,
	hcl_heap_t*  heap,
	hcl_oow_t    size
);

void hcl_freeheapmem (
	hcl_t*       hcl,
	hcl_heap_t*  heap,
	void*        ptr
);

/* ========================================================================= */
/* obj.c                                                                     */
/* ========================================================================= */
void* hcl_allocbytes (
	hcl_t*     hcl,
	hcl_oow_t  size
);

/**
 * The hcl_allocoopobj() function allocates a raw object composed of \a size
 * pointer fields excluding the header.
 */
hcl_oop_t hcl_allocoopobj (
	hcl_t*    hcl,
	int       brand,
	hcl_oow_t size
);

hcl_oop_t hcl_allocoopobjwithtrailer (
	hcl_t*           hcl,
	int              brand,
	hcl_oow_t        size,
	const hcl_oob_t* tptr,
	hcl_oow_t        tlen
);

hcl_oop_t hcl_alloccharobj (
	hcl_t*            hcl,
	int               brand,
	const hcl_ooch_t* ptr,
	hcl_oow_t         len
);

hcl_oop_t hcl_allocbyteobj (
	hcl_t*            hcl,
	int               brand,
	const hcl_oob_t*  ptr,
	hcl_oow_t         len
);

hcl_oop_t hcl_allochalfwordobj (
	hcl_t*            hcl,
	int               brand,
	const hcl_oohw_t* ptr,
	hcl_oow_t         len
);

hcl_oop_t hcl_allocwordobj (
	hcl_t*           hcl,
	int               brand,
	const hcl_oow_t* ptr,
	hcl_oow_t        len
);

/* ========================================================================= */
/* sym.c                                                                     */
/* ========================================================================= */
hcl_oop_t hcl_makesymbol (
	hcl_t*             hcl,
	const hcl_ooch_t*  ptr,
	hcl_oow_t          len
);

hcl_oop_t hcl_findsymbol (
	hcl_t*             hcl,
	const hcl_ooch_t*  ptr,
	hcl_oow_t          len
);


/* ========================================================================= */
/* proc.c                                                                    */
/* ========================================================================= */
hcl_oop_process_t hcl_makeproc (
	hcl_t* hcl
);

/* ========================================================================= */
/* gc.c                                                                    */
/* ========================================================================= */

hcl_oow_t hcl_getobjpayloadbytes (
	hcl_t*    hcl,
	hcl_oop_t oop
);

void hcl_gc_ms_sweep_lazy (
	hcl_t*    hcl,
	hcl_oow_t allocsize
);

int hcl_getsyncodebyoocs_noseterr (
	hcl_t*            hcl,
	const hcl_oocs_t* name
);

int hcl_getsyncode_noseterr (
	hcl_t*            hcl,
	const hcl_ooch_t* ptr,
	const hcl_oow_t   len
);

const hcl_ooch_t* hcl_getsyncodename_noseterr (
	hcl_t*        hcl,
	hcl_syncode_t syncode
);


/* ========================================================================= */
/* utf8.c                                                                    */
/* ========================================================================= */
hcl_oow_t hcl_uc_to_utf8 (
	hcl_uch_t    uc,
	hcl_bch_t*   utf8,
	hcl_oow_t    size
);

hcl_oow_t hcl_utf8_to_uc (
	const hcl_bch_t* utf8,
	hcl_oow_t        size,
	hcl_uch_t*       uc
);

int hcl_ucstoutf8 (
	const hcl_uch_t*    ucs,
	hcl_oow_t*          ucslen,
	hcl_bch_t*          bcs,
	hcl_oow_t*          bcslen
);

/**
 * The hcl_utf8_to_ucs() function converts a UTF8 string to a uncide string.
 *
 * It never returns -2 if \a ucs is #HCL_NULL.
 *
 * \code
 *  const hcl_bch_t* bcs = "test string";
 *  hcl_uch_t ucs[100];
 *  hcl_oow_t ucslen = HCL_COUNTOF(buf), n;
 *  hcl_oow_t bcslen = 11;
 *  int n;
 *  n = hcl_utf8_to_ucs (bcs, &bcslen, ucs, &ucslen);
 *  if (n <= -1) { invalid/incomplenete sequence or buffer to small }
 * \endcode
 *
 * For a null-terminated string, you can specify ~(hcl_oow_t)0 in
 * \a bcslen. The destination buffer \a ucs also must be large enough to
 * store a terminating null. Otherwise, -2 is returned.
 * 
 * The resulting \a ucslen can still be greater than 0 even if the return
 * value is negative. The value indiates the number of characters converted
 * before the error has occurred.
 *
 * \return 0 on success.
 *         -1 if \a bcs contains an illegal character.
 *         -2 if the wide-character string buffer is too small.
 *         -3 if \a bcs is not a complete sequence.
 */
int hcl_utf8_to_ucs (
	const hcl_bch_t*   bcs,
	hcl_oow_t*         bcslen,
	hcl_uch_t*         ucs,
	hcl_oow_t*         ucslen
);

/* ========================================================================= */
/* bigint.c                                                                  */
/* ========================================================================= */
static HCL_INLINE int hcl_isbigint (hcl_t* hcl, hcl_oop_t x)
{
	return HCL_IS_BIGINT(hcl, x);
}

static HCL_INLINE int hcl_isint (hcl_t* hcl, hcl_oop_t x)
{
	return HCL_OOP_IS_SMOOI(x) || HCL_IS_BIGINT(hcl, x);
}

hcl_oop_t hcl_addints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_subints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_mulints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_divints (
	hcl_t*     hcl,
	hcl_oop_t  x,
	hcl_oop_t  y,
	int        modulo,
	hcl_oop_t* rem
);

hcl_oop_t hcl_negateint (
	hcl_t*    hcl,
	hcl_oop_t x
);

hcl_oop_t hcl_bitatint (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_bitandints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_bitorints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_bitxorints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_bitinvint (
	hcl_t*    hcl,
	hcl_oop_t x
);

hcl_oop_t hcl_bitshiftint (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_eqints (
	hcl_t* hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_neints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_gtints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_geints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_ltints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_leints (
	hcl_t*    hcl,
	hcl_oop_t x,
	hcl_oop_t y
);

hcl_oop_t hcl_sqrtint (
	hcl_t*    hcl,
	hcl_oop_t x
);

hcl_oop_t hcl_absint (
	hcl_t*    hcl,
	hcl_oop_t x
);

hcl_oop_t hcl_strtoint (
	hcl_t*            hcl,
	const hcl_ooch_t* str,
	hcl_oow_t         len,
	int               radix
);


#define HCL_INTTOSTR_RADIXMASK (0xFF)
#define HCL_INTTOSTR_LOWERCASE (1 << 8)
#define HCL_INTTOSTR_NONEWOBJ  (1 << 9)
/**
 * The hcl_inttostr() function converts an integer object to a string object
 * printed in the given radix. If HCL_INTTOSTR_NONEWOBJ is set in flags_radix,
 * it returns hcl->_nil but keeps the result in the buffer pointed to by 
 * hcl->inttostr.xbuf.ptr with the length stored in hcl->inttostr.xbuf.len.
 * If the function fails, it returns #HCL_NULL.
 */
hcl_oop_t hcl_inttostr (
	hcl_t*      hcl,
	hcl_oop_t   num,
	int         flagged_radix /* radix between 2 and 36 inclusive, optionally bitwise ORed of HCL_INTTOSTR_XXX bits */
);

/* ========================================================================= */
/* number.c                                                                    */
/* ========================================================================= */
hcl_oop_t hcl_addnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_subnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_mulnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_mltnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_divnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_truncfpdecval (
	hcl_t*       hcl,
	hcl_oop_t    iv, /* integer */
	hcl_ooi_t    cs, /* current scale */
	hcl_ooi_t    ns  /* new scale */
);

hcl_oop_t hcl_gtnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_genums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_ltnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_lenums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_eqnums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_nenums (
	hcl_t*       hcl,
	hcl_oop_t    x,
	hcl_oop_t    y
);

hcl_oop_t hcl_sqrtnum (
	hcl_t*       hcl,
	hcl_oop_t    x
);

hcl_oop_t hcl_absnum (
	hcl_t*       hcl,
	hcl_oop_t    x
);

/* ========================================================================= */
/* hcl.c                                                                     */
/* ========================================================================= */

hcl_mod_data_t* hcl_openmod (
	hcl_t*            hcl,
	const hcl_ooch_t* name,
	hcl_oow_t         namelen
);

void hcl_closemod (
	hcl_t*            hcl,
	hcl_mod_data_t*   mdp
);

/*
 * The hcl_querymod() function finds a primitive function in modules
 * with a full primitive identifier.
 */
hcl_pfbase_t* hcl_querymod (
	hcl_t*            hcl,
	const hcl_ooch_t* pfid,
	hcl_oow_t         pfidlen,
	hcl_mod_t**       mod
);

/* ========================================================================= */
/* fmt.c                                                                     */
/* ========================================================================= */
int hcl_fmt_object_ (
	hcl_fmtout_t* fmtout,
	hcl_oop_t     oop
);

int hcl_prfmtcallstack (
	hcl_t*    hcl,
	hcl_ooi_t nargs
);

int hcl_logfmtcallstack (
	hcl_t*    hcl,
	hcl_ooi_t nargs
);

int hcl_strfmtcallstack (
	hcl_t*    hcl,
	hcl_ooi_t nargs
);

/* ========================================================================= */
/* comp.c                                                                    */
/* ========================================================================= */
int hcl_emitbyteinstruction (
	hcl_t*     hcl,
	hcl_oob_t  bc
);

/* ========================================================================= */
/* cnode.c                                                                   */
/* ========================================================================= */
hcl_cnode_t* hcl_makecnodenil (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodetrue (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodefalse (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodecharlit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok, const hcl_ooch_t v);
hcl_cnode_t* hcl_makecnodesymbol (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodedsymbol (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodestrlit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodenumlit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnoderadnumlit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodefpdeclit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok);
hcl_cnode_t* hcl_makecnodesmptrlit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok, hcl_oow_t v);
hcl_cnode_t* hcl_makecnodeerrlit (hcl_t* hcl, const hcl_ioloc_t* loc, const  hcl_oocs_t* tok, hcl_ooi_t v);
hcl_cnode_t* hcl_makecnodecons (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_cnode_t* car, hcl_cnode_t* cdr);
hcl_cnode_t* hcl_makecnodeelist (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_concode_t type);
hcl_cnode_t* hcl_makecnodeshell (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_cnode_t* obj);
void hcl_freesinglecnode (hcl_t* hcl, hcl_cnode_t* c);
void hcl_freecnode (hcl_t* hcl, hcl_cnode_t* c);
hcl_oow_t hcl_countcnodecons (hcl_t* hcl, hcl_cnode_t* cons);


/* ========================================================================= */
/* exec.c                                                                    */
/* ========================================================================= */
hcl_pfrc_t hcl_pf_process_fork (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_process_resume (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_process_suspend (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_process_yield (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);

hcl_pfrc_t hcl_pf_semaphore_new (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_wait (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_signal (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_signal_timed (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_signal_on_input (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_signal_on_output (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
/*hcl_pfrc_t hcl_pf_semaphore_signal_on_gcfin (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);*/
hcl_pfrc_t hcl_pf_semaphore_unsignal (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);

hcl_pfrc_t hcl_pf_semaphore_group_new (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_group_add_semaphore (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_group_remove_semaphore (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);
hcl_pfrc_t hcl_pf_semaphore_group_wait (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs);

#if defined(__cplusplus)
}
#endif




#endif
