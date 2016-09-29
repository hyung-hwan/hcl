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

#ifndef _HCL_PRV_H_
#define _HCL_PRV_H_

#include "hcl.h"
#include "hcl-utl.h"

/* you can define this to either 1 or 2 */
#define HCL_BCODE_LONG_PARAM_SIZE 2

/* this is useful for debugging. hcl_gc() can be called 
 * while hcl has not been fully initialized when this is defined*/
#define HCL_SUPPORT_GC_DURING_IGNITION

/* define this to generate XXXX_CTXTEMVAR instructions */
#define HCL_USE_CTXTEMPVAR

/* define this to use the MAKE_BLOCK instruction instead of
 * PUSH_CONTEXT, PUSH_INTLIT, PUSH_INTLIT, SEND_BLOCK_COPY */
#define HCL_USE_MAKE_BLOCK

/* define this to allow an pointer(OOP) object to have trailing bytes 
 * this is used to embed bytes codes into the back of a compile method
 * object instead of putting in in a separate byte array. */
#define HCL_USE_OBJECT_TRAILER

/* this is for gc debugging */
/*#define HCL_DEBUG_PROCESSOR*/
#define HCL_DEBUG_GC


/* limit the maximum object size such that:
 *   1. an index to an object field can be represented in a small integer.
 *   2. the maximum number of bits including bit-shifts can be represented
 *      in the hcl_oow_t type.
 */
#define HCL_LIMIT_OBJ_SIZE

#include <stdio.h> /* TODO: delete these header inclusion lines */
#include <string.h>
#include <assert.h>

#if defined(__has_builtin)
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
#	define HCL_MEMSET(dst,src,size)  memset(dst,src,size)
#	define HCL_MEMCPY(dst,src,size)  memcpy(dst,src,size)
#	define HCL_MEMMOVE(dst,src,size) memmove(dst,src,size)
#	define HCL_MEMCMP(dst,src,size)  memcmp(dst,src,size)
#endif

#define HCL_ASSERT(x)             assert(x)

#define HCL_ALIGN(x,y) ((((x) + (y) - 1) / (y)) * (y))


/* ========================================================================= */
/* CLASS SPEC ENCODING                                                       */
/* ========================================================================= */

/*
 * The spec field of a class object encodes the number of the fixed part
 * and the type of the indexed part. The fixed part is the number of
 * named instance variables. If the spec of a class is indexed, the object
 * of the class can be i nstantiated with the size of the indexed part.
 *
 * For example, on a platform where sizeof(hcl_oow_t) is 4, 
 * the layout of the spec field of a class as an OOP value looks like this:
 * 
 *  31                           10 9   8 7 6 5 4 3  2           1 0 
 * |number of named instance variables|indexed-type|indexability|oop-tag|
 *
 * the number of named instance variables is stored in high 23 bits.
 * the indexed type takes up bit 3 to bit 8 (assuming HCL_OBJ_TYPE_BITS is 6. 
 * HCL_OBJ_TYPE_XXX enumerators are used to represent actual values).
 * and the indexability is stored in bit 2.
 *
 * The maximum number of named(fixed) instance variables for a class is:
 *     2 ^ ((BITS-IN-OOW - HCL_OOP_TAG_BITS) - HCL_OBJ_TYPE_BITS - 1) - 1
 *
 * HCL_OOP_TAG_BITS are decremented from the number of bits in OOW because
 * the spec field is always encoded as a small integer.
 *
 * The number of named instance variables can be greater than 0 if the
 * class spec is not indexed or if it's a pointer indexed class
 * (indexed_type == HCL_OBJ_TYPE_OOP)
 * 
 * indexed_type is one of the #hcl_obj_type_t enumerators.
 */

/*
 * The HCL_CLASS_SPEC_MAKE() macro creates a class spec value.
 *  _class->spec = HCL_SMOOI_TO_OOP(HCL_CLASS_SPEC_MAKE(0, 1, HCL_OBJ_TYPE_CHAR));
 */
#define HCL_CLASS_SPEC_MAKE(named_instvar,is_indexed,indexed_type) ( \
	(((hcl_oow_t)(named_instvar)) << (HCL_OBJ_FLAGS_TYPE_BITS + 1)) |  \
	(((hcl_oow_t)(indexed_type)) << 1) | (((hcl_oow_t)is_indexed) & 1) )

/* what is the number of named instance variables? 
 *  HCL_CLASS_SPEC_NAMED_INSTVAR(HCL_OOP_TO_SMOOI(_class->spec))
 */
#define HCL_CLASS_SPEC_NAMED_INSTVAR(spec) \
	(((hcl_oow_t)(spec)) >> (HCL_OBJ_FLAGS_TYPE_BITS + 1))

/* is it a user-indexable class? 
 * all objects can be indexed with basicAt:.
 * this indicates if an object can be instantiated with a dynamic size
 * (new: size) and and can be indexed with at:.
 */
#define HCL_CLASS_SPEC_IS_INDEXED(spec) (((hcl_oow_t)(spec)) & 1)

/* if so, what is the indexing type? character? pointer? etc? */
#define HCL_CLASS_SPEC_INDEXED_TYPE(spec) \
	((((hcl_oow_t)(spec)) >> 1) & HCL_LBMASK(hcl_oow_t, HCL_OBJ_FLAGS_TYPE_BITS))

/* What is the maximum number of named instance variables?
 * This limit is set so because the number must be encoded into the spec field
 * of the class with limited number of bits assigned to the number of
 * named instance variables. the trailing -1 in the calculation of number of
 * bits is to consider the sign bit of a small-integer which is a typical
 * type of the spec field in the class object.
 */
/*
#define HCL_MAX_NAMED_INSTVARS \
	HCL_BITS_MAX(hcl_oow_t, HCL_OOW_BITS - HCL_OOP_TAG_BITS - (HCL_OBJ_FLAGS_TYPE_BITS + 1) - 1)
*/
#define HCL_MAX_NAMED_INSTVARS \
	HCL_BITS_MAX(hcl_oow_t, HCL_SMOOI_ABS_BITS - (HCL_OBJ_FLAGS_TYPE_BITS + 1))

/* Given the number of named instance variables, what is the maximum number 
 * of indexed instance variables? The number of indexed instance variables
 * is not stored in the spec field of the class. It only affects the actual
 * size of an object(obj->_size) selectively combined with the number of 
 * named instance variables. So it's the maximum value of obj->_size minus
 * the number of named instance variables.
 */
#define HCL_MAX_INDEXED_INSTVARS(named_instvar) (HCL_OBJ_SIZE_MAX - named_instvar)

/*
#define HCL_CLASS_SELFSPEC_MAKE(class_var,classinst_var) \
	(((hcl_oow_t)class_var) << ((HCL_OOW_BITS - HCL_OOP_TAG_BITS) / 2)) | ((hcl_oow_t)classinst_var)
*/
#define HCL_CLASS_SELFSPEC_MAKE(class_var,classinst_var) \
	(((hcl_oow_t)class_var) << (HCL_SMOOI_BITS / 2)) | ((hcl_oow_t)classinst_var)

/*
#define HCL_CLASS_SELFSPEC_CLASSVAR(spec) ((hcl_oow_t)spec >> ((HCL_OOW_BITS - HCL_OOP_TAG_BITS) / 2))
#define HCL_CLASS_SELFSPEC_CLASSINSTVAR(spec) (((hcl_oow_t)spec) & HCL_LBMASK(hcl_oow_t, (HCL_OOW_BITS - HCL_OOP_TAG_BITS) / 2))
*/
#define HCL_CLASS_SELFSPEC_CLASSVAR(spec) ((hcl_oow_t)spec >> (HCL_SMOOI_BITS / 2))
#define HCL_CLASS_SELFSPEC_CLASSINSTVAR(spec) (((hcl_oow_t)spec) & HCL_LBMASK(hcl_oow_t, (HCL_SMOOI_BITS / 2)))

/*
 * yet another -1 in the calculation of the bit numbers for signed nature of
 * a small-integer
 */
/*
#define HCL_MAX_CLASSVARS      HCL_BITS_MAX(hcl_oow_t, (HCL_OOW_BITS - HCL_OOP_TAG_BITS - 1) / 2)
#define HCL_MAX_CLASSINSTVARS  HCL_BITS_MAX(hcl_oow_t, (HCL_OOW_BITS - HCL_OOP_TAG_BITS - 1) / 2)
*/
#define HCL_MAX_CLASSVARS      HCL_BITS_MAX(hcl_oow_t, HCL_SMOOI_ABS_BITS / 2)
#define HCL_MAX_CLASSINSTVARS  HCL_BITS_MAX(hcl_oow_t, HCL_SMOOI_ABS_BITS / 2)


#if defined(HCL_LIMIT_OBJ_SIZE)
/* limit the maximum object size such that:
 *   1. an index to an object field can be represented in a small integer.
 *   2. the maximum number of bit shifts can be represented in the hcl_oow_t type.
 */
#	define HCL_OBJ_SIZE_MAX ((hcl_oow_t)HCL_SMOOI_MAX)
#	define HCL_OBJ_SIZE_BITS_MAX (HCL_OBJ_SIZE_MAX * 8)
#else
#	define HCL_OBJ_SIZE_MAX ((hcl_oow_t)HCL_TYPE_MAX(hcl_oow_t))
#	define HCL_OBJ_SIZE_BITS_MAX (HCL_OBJ_SIZE_MAX * 8)
#endif


#if defined(HCL_INCLUDE_COMPILER)

/* ========================================================================= */
/* SOURCE CODE I/O FOR COMPILER                                              */
/* ========================================================================= */
typedef struct hcl_iotok_t hcl_iotok_t;
struct hcl_iotok_t
{
	enum
	{
		HCL_IOTOK_EOF,
		HCL_IOTOK_CHARLIT,
		HCL_IOTOK_STRLIT,
		HCL_IOTOK_NUMLIT,
		HCL_IOTOK_RADNUMLIT,
		HCL_IOTOK_NIL,
		HCL_IOTOK_TRUE,
		HCL_IOTOK_FALSE,

		HCL_IOTOK_IDENT,
		HCL_IOTOK_DOT,
		HCL_IOTOK_QUOTE,
		HCL_IOTOK_LPAREN,
		HCL_IOTOK_RPAREN,
		HCL_IOTOK_ARPAREN,
		HCL_IOTOK_BAPAREN,
		HCL_IOTOK_LBRACK,
		HCL_IOTOK_RBRACK,

		HCL_IOTOK_INCLUDE
	} type;

	hcl_oocs_t name;
	hcl_oow_t name_capa;

	hcl_ioloc_t loc;
};


typedef struct hcl_iolink_t hcl_iolink_t;
struct hcl_iolink_t
{
	hcl_iolink_t* link;
};

/* NOTE: hcl_cframe_t used by the built-in compiler is not an OOP object */
struct hcl_cframe_t
{
	int       opcode;
	hcl_oop_t operand;
	union
	{
		struct 
		{
			hcl_oow_t nargs;
			hcl_oow_t ntmprs;
			hcl_oow_t jip; /* jump instruction position */
		} lambda;

		struct
		{
			int var_type;
		} set;
	} u;
};

typedef struct hcl_cframe_t hcl_cframe_t;

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

		hcl_oow_t balit_capa;
		hcl_oow_t balit_count;
		hcl_oob_t* balit;
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
		hcl_oop_t* ptr;
		hcl_oow_t size;
		hcl_oow_t capa;
	} tv; /* temporary variables including arguments */

	struct
	{
		hcl_ooi_t depth;
		hcl_oow_t* tmprcnt;
		hcl_oow_t  tmprcnt_capa;
	} blk; /* lambda block */
};

#endif

#if defined(HCL_USE_OBJECT_TRAILER)
	/* let it point to the trailer of the method */
#	define SET_ACTIVE_METHOD_CODE(hcl) ((hcl)->active_code = (hcl_oob_t*)&(hcl)->active_method->slot[HCL_OBJ_GET_SIZE((hcl)->active_method) + 1 - HCL_METHOD_NAMED_INSTVARS])
#else
	/* let it point to the payload of the code byte array */
#	define SET_ACTIVE_METHOD_CODE(hcl) ((hcl)->active_code = (hcl)->active_method->code->slot)
#endif

#if defined(HCL_BCODE_LONG_PARAM_SIZE) && (HCL_BCODE_LONG_PARAM_SIZE == 1)
#	define MAX_CODE_INDEX               (0xFFu)
#	define MAX_CODE_NTMPRS              (0xFFu)
#	define MAX_CODE_NARGS               (0xFFu)
#	define MAX_CODE_NBLKARGS            (0xFFu)
#	define MAX_CODE_NBLKTMPRS           (0xFFu)
#	define MAX_CODE_JUMP                (0xFFu)
#	define MAX_CODE_PARAM               (0xFFu)
#	define MAX_CODE_PARAM2              (0xFFFFu)
#elif defined(HCL_BCODE_LONG_PARAM_SIZE) && (HCL_BCODE_LONG_PARAM_SIZE == 2)
#	define MAX_CODE_INDEX               (0xFFFFu)
#	define MAX_CODE_NTMPRS              (0xFFFFu)
#	define MAX_CODE_NARGS               (0xFFFFu)
#	define MAX_CODE_NBLKARGS            (0xFFFFu)
#	define MAX_CODE_NBLKTMPRS           (0xFFFFu)
#	define MAX_CODE_JUMP                (0xFFFFu)
#	define MAX_CODE_PARAM               (0xFFFFu)
#	define MAX_CODE_PARAM2              (0xFFFFFFFFu)
#else
#	error Unsupported HCL_BCODE_LONG_PARAM_SIZE
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
72-75    0100 10XX JUMP_BACKWARD                              200  1100 1000 XXXXXXXX JUMP_BACKWARD_X
76-79    0100 11XX JUMP_IF_TRUE                               204  1100 1100 XXXXXXXX JUMP_IF_TRUE_X
80-83    0101 00XX JUMP_IF_FALSE                              208  1101 0000 XXXXXXXX JUMP_IF_FALSE_X

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
	BCODE_STORE_INTO_INSTVAR_0     = 0x00,
	BCODE_STORE_INTO_INSTVAR_1     = 0x01,
	BCODE_STORE_INTO_INSTVAR_2     = 0x02,
	BCODE_STORE_INTO_INSTVAR_3     = 0x03,

	BCODE_STORE_INTO_INSTVAR_4     = 0x04,
	BCODE_STORE_INTO_INSTVAR_5     = 0x05,
	BCODE_STORE_INTO_INSTVAR_6     = 0x06,
	BCODE_STORE_INTO_INSTVAR_7     = 0x07,

	BCODE_POP_INTO_INSTVAR_0       = 0x08,
	BCODE_POP_INTO_INSTVAR_1       = 0x09,
	BCODE_POP_INTO_INSTVAR_2       = 0x0A,
	BCODE_POP_INTO_INSTVAR_3       = 0x0B,

	BCODE_POP_INTO_INSTVAR_4       = 0x0C,
	BCODE_POP_INTO_INSTVAR_5       = 0x0D,
	BCODE_POP_INTO_INSTVAR_6       = 0x0E,
	BCODE_POP_INTO_INSTVAR_7       = 0x0F,

	BCODE_PUSH_INSTVAR_0           = 0x10,
	BCODE_PUSH_INSTVAR_1           = 0x11,
	BCODE_PUSH_INSTVAR_2           = 0x12,
	BCODE_PUSH_INSTVAR_3           = 0x13,

	BCODE_PUSH_INSTVAR_4           = 0x14,
	BCODE_PUSH_INSTVAR_5           = 0x15,
	BCODE_PUSH_INSTVAR_6           = 0x16,
	BCODE_PUSH_INSTVAR_7           = 0x17,

	HCL_CODE_PUSH_TEMPVAR_0        = 0x18,
	HCL_CODE_PUSH_TEMPVAR_1        = 0x19,
	HCL_CODE_PUSH_TEMPVAR_2        = 0x1A,
	HCL_CODE_PUSH_TEMPVAR_3        = 0x1B,

	HCL_CODE_PUSH_TEMPVAR_4        = 0x1C,
	HCL_CODE_PUSH_TEMPVAR_5        = 0x1D,
	HCL_CODE_PUSH_TEMPVAR_6        = 0x1E,
	HCL_CODE_PUSH_TEMPVAR_7        = 0x1F,

	HCL_CODE_STORE_INTO_TEMPVAR_0  = 0x20,
	HCL_CODE_STORE_INTO_TEMPVAR_1  = 0x21,
	HCL_CODE_STORE_INTO_TEMPVAR_2  = 0x22,
	HCL_CODE_STORE_INTO_TEMPVAR_3  = 0x23,

	HCL_CODE_STORE_INTO_TEMPVAR_4  = 0x24,
	HCL_CODE_STORE_INTO_TEMPVAR_5  = 0x25,
	HCL_CODE_STORE_INTO_TEMPVAR_6  = 0x26,
	HCL_CODE_STORE_INTO_TEMPVAR_7  = 0x27,

	BCODE_POP_INTO_TEMPVAR_0       = 0x28,
	BCODE_POP_INTO_TEMPVAR_1       = 0x29,
	BCODE_POP_INTO_TEMPVAR_2       = 0x2A,
	BCODE_POP_INTO_TEMPVAR_3       = 0x2B,

	BCODE_POP_INTO_TEMPVAR_4       = 0x2C,
	BCODE_POP_INTO_TEMPVAR_5       = 0x2D,
	BCODE_POP_INTO_TEMPVAR_6       = 0x2E,
	BCODE_POP_INTO_TEMPVAR_7       = 0x2F,

	HCL_CODE_PUSH_LITERAL_0        = 0x30,
	HCL_CODE_PUSH_LITERAL_1        = 0x31,
	HCL_CODE_PUSH_LITERAL_2        = 0x32,
	HCL_CODE_PUSH_LITERAL_3        = 0x33,

	HCL_CODE_PUSH_LITERAL_4        = 0x34,
	HCL_CODE_PUSH_LITERAL_5        = 0x35,
	HCL_CODE_PUSH_LITERAL_6        = 0x36,
	HCL_CODE_PUSH_LITERAL_7        = 0x37,

	/* -------------------------------------- */

	HCL_CODE_STORE_INTO_OBJECT_0      = 0x38,
	HCL_CODE_STORE_INTO_OBJECT_1      = 0x39,
	HCL_CODE_STORE_INTO_OBJECT_2      = 0x3A,
	HCL_CODE_STORE_INTO_OBJECT_3      = 0x3B,

	BCODE_POP_INTO_OBJECT_0        = 0x3C,
	BCODE_POP_INTO_OBJECT_1        = 0x3D,
	BCODE_POP_INTO_OBJECT_2        = 0x3E,
	BCODE_POP_INTO_OBJECT_3        = 0x3F,

	HCL_CODE_PUSH_OBJECT_0         = 0x40,
	HCL_CODE_PUSH_OBJECT_1         = 0x41,
	HCL_CODE_PUSH_OBJECT_2         = 0x42,
	HCL_CODE_PUSH_OBJECT_3         = 0x43,

	HCL_CODE_JUMP_FORWARD_0        = 0x44, /* 68 */
	HCL_CODE_JUMP_FORWARD_1        = 0x45, /* 69 */
	HCL_CODE_JUMP_FORWARD_2        = 0x46, /* 70 */
	HCL_CODE_JUMP_FORWARD_3        = 0x47, /* 71 */

	HCL_CODE_JUMP_BACKWARD_0       = 0x48,
	HCL_CODE_JUMP_BACKWARD_1       = 0x49,
	HCL_CODE_JUMP_BACKWARD_2       = 0x4A,
	HCL_CODE_JUMP_BACKWARD_3       = 0x4B,

	BCODE_JUMP_IF_TRUE_0           = 0x4C,
	BCODE_JUMP_IF_TRUE_1           = 0x4D,
	BCODE_JUMP_IF_TRUE_2           = 0x4E,
	BCODE_JUMP_IF_TRUE_3           = 0x4F,

	BCODE_JUMP_IF_FALSE_0          = 0x50, /* 80 */
	BCODE_JUMP_IF_FALSE_1          = 0x51, /* 81 */
	BCODE_JUMP_IF_FALSE_2          = 0x52, /* 82 */
	BCODE_JUMP_IF_FALSE_3          = 0x53, /* 83 */

	HCL_CODE_CALL_0                = 0x54, /* 84 */
	HCL_CODE_CALL_1                = 0x55, /* 85 */
	HCL_CODE_CALL_2                = 0x56, /* 86 */
	HCL_CODE_CALL_3                = 0x57, /* 87 */

	HCL_CODE_STORE_INTO_CTXTEMPVAR_0  = 0x58, /* 88 */
	HCL_CODE_STORE_INTO_CTXTEMPVAR_1  = 0x59, /* 89 */
	HCL_CODE_STORE_INTO_CTXTEMPVAR_2  = 0x5A, /* 90 */
	HCL_CODE_STORE_INTO_CTXTEMPVAR_3  = 0x5B, /* 91 */

	BCODE_POP_INTO_CTXTEMPVAR_0    = 0x5C, /* 92 */
	BCODE_POP_INTO_CTXTEMPVAR_1    = 0x5D, /* 93 */
	BCODE_POP_INTO_CTXTEMPVAR_2    = 0x5E, /* 94 */
	BCODE_POP_INTO_CTXTEMPVAR_3    = 0x5F, /* 95 */

	HCL_CODE_PUSH_CTXTEMPVAR_0     = 0x60, /* 96 */
	HCL_CODE_PUSH_CTXTEMPVAR_1     = 0x61, /* 97 */
	HCL_CODE_PUSH_CTXTEMPVAR_2     = 0x62, /* 98 */
	HCL_CODE_PUSH_CTXTEMPVAR_3     = 0x63, /* 99 */

	BCODE_PUSH_OBJVAR_0            = 0x64,
	BCODE_PUSH_OBJVAR_1            = 0x65,
	BCODE_PUSH_OBJVAR_2            = 0x66,
	BCODE_PUSH_OBJVAR_3            = 0x67,

	BCODE_STORE_INTO_OBJVAR_0      = 0x68,
	BCODE_STORE_INTO_OBJVAR_1      = 0x69,
	BCODE_STORE_INTO_OBJVAR_2      = 0x6A,
	BCODE_STORE_INTO_OBJVAR_3      = 0x6B,

	BCODE_POP_INTO_OBJVAR_0        = 0x6C,
	BCODE_POP_INTO_OBJVAR_1        = 0x6D,
	BCODE_POP_INTO_OBJVAR_2        = 0x6E,
	BCODE_POP_INTO_OBJVAR_3        = 0x6F,

	BCODE_SEND_MESSAGE_0           = 0x70,
	BCODE_SEND_MESSAGE_1           = 0x71,
	BCODE_SEND_MESSAGE_2           = 0x72,
	BCODE_SEND_MESSAGE_3           = 0x73,

	BCODE_SEND_MESSAGE_TO_SUPER_0  = 0x74,
	BCODE_SEND_MESSAGE_TO_SUPER_1  = 0x75,
	BCODE_SEND_MESSAGE_TO_SUPER_2  = 0x76,
	BCODE_SEND_MESSAGE_TO_SUPER_3  = 0x77,

	/* UNUSED 0x78 - 0x7F */

	BCODE_STORE_INTO_INSTVAR_X     = 0x80, /* 128 */
	BCODE_POP_INTO_INSTVAR_X       = 0x88, /* 136 */
	BCODE_PUSH_INSTVAR_X           = 0x90, /* 144 */

	HCL_CODE_PUSH_TEMPVAR_X        = 0x98, /* 152 */
	HCL_CODE_STORE_INTO_TEMPVAR_X  = 0xA0, /* 160 */
	BCODE_POP_INTO_TEMPVAR_X       = 0xA8, /* 168 */

	HCL_CODE_PUSH_LITERAL_X        = 0xB0, /* 176 */
	HCL_CODE_PUSH_LITERAL_X2       = 0xB1, /* 177 */

	/* SEE FURTHER DOWN FOR SPECIAL CODES - 0xB2 - 0xB7  */

	HCL_CODE_STORE_INTO_OBJECT_X   = 0xB8, /* 184 */
	BCODE_POP_INTO_OBJECT_X        = 0xBC, /* 188 */
	HCL_CODE_PUSH_OBJECT_X         = 0xC0, /* 192 */

	HCL_CODE_JUMP_FORWARD_X        = 0xC4, /* 196 */
	HCL_CODE_JUMP_BACKWARD_X       = 0xC8, /* 200 */
	BCODE_JUMP_IF_TRUE_X           = 0xCC, /* 204 */
	BCODE_JUMP_IF_FALSE_X          = 0xD0, /* 208 */

	HCL_CODE_CALL_X                = 0xD4, /* 212 */

	HCL_CODE_STORE_INTO_CTXTEMPVAR_X  = 0xD8, /* 216 */
	BCODE_POP_INTO_CTXTEMPVAR_X    = 0xDC, /* 220 */
	HCL_CODE_PUSH_CTXTEMPVAR_X        = 0xE0, /* 224 */

	BCODE_PUSH_OBJVAR_X            = 0xE4, /* 228 */
	BCODE_STORE_INTO_OBJVAR_X      = 0xE8, /* 232 */
	BCODE_POP_INTO_OBJVAR_X        = 0xEC, /* 236 */

	BCODE_SEND_MESSAGE_X           = 0xF0, /* 240 */
	BCODE_SEND_MESSAGE_TO_SUPER_X  = 0xF4, /* 244 */

	/* -------------------------------------- */

	HCL_CODE_JUMP2_FORWARD         = 0xC5, /* 197 */
	HCL_CODE_JUMP2_BACKWARD        = 0xC9, /* 201 */

	BCODE_PUSH_RECEIVER            = 0x81, /* 129 */
	HCL_CODE_PUSH_NIL              = 0x82, /* 130 */
	HCL_CODE_PUSH_TRUE             = 0x83, /* 131 */
	HCL_CODE_PUSH_FALSE            = 0x84, /* 132 */
	BCODE_PUSH_CONTEXT             = 0x85, /* 133 */
	BCODE_PUSH_PROCESS             = 0x86, /* 134 */
	HCL_CODE_PUSH_NEGONE           = 0x87, /* 135 */
	HCL_CODE_PUSH_ZERO             = 0x89, /* 137 */
	HCL_CODE_PUSH_ONE              = 0x8A, /* 138 */
	HCL_CODE_PUSH_TWO              = 0x8B, /* 139 */

	HCL_CODE_PUSH_INTLIT           = 0xB2, /* 178 */
	HCL_CODE_PUSH_NEGINTLIT        = 0xB3, /* 179 */
	HCL_CODE_PUSH_CHARLIT          = 0xB4, /* 180 */

	/* UNUSED 0xE8 - 0xF7 */

	BCODE_DUP_STACKTOP             = 0xF8,
	HCL_CODE_POP_STACKTOP          = 0xF9,
	BCODE_RETURN_STACKTOP          = 0xFA, /* ^something */
	BCODE_RETURN_RECEIVER          = 0xFB, /* ^self */
	HCL_CODE_RETURN_FROM_BLOCK     = 0xFC, /* return the stack top from a block */
	HCL_CODE_MAKE_BLOCK            = 0xFD,
	BCODE_SEND_BLOCK_COPY          = 0xFE,
	HCL_CODE_NOOP                  = 0xFF
};

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
 * The hcl_allocheapmem() function allocates \a size bytes in the heap pointed
 * to by \a heap.
 *
 * \return memory pointer on success and #HCL_NULL on failure.
 */
void* hcl_allocheapmem (
	hcl_t*      hcl,
	hcl_heap_t* heap,
	hcl_oow_t   size
);


/* ========================================================================= */
/* gc.c                                                                     */
/* ========================================================================= */
hcl_oop_t hcl_moveoop (
	hcl_t*     hcl,
	hcl_oop_t  oop
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
	hcl_oow_t size
);

#if defined(HCL_USE_OBJECT_TRAILER)
hcl_oop_t hcl_allocoopobjwithtrailer (
	hcl_t*           hcl,
	hcl_oow_t        size,
	const hcl_oob_t* tptr,
	hcl_oow_t        tlen
);
#endif

hcl_oop_t hcl_alloccharobj (
	hcl_t*            hcl,
	const hcl_ooch_t* ptr,
	hcl_oow_t         len
);

hcl_oop_t hcl_allocbyteobj (
	hcl_t*           hcl,
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

hcl_oop_t hcl_allochalfwordobj (
	hcl_t*            hcl,
	const hcl_oohw_t* ptr,
	hcl_oow_t         len
);

hcl_oop_t hcl_allocwordobj (
	hcl_t*           hcl,
	const hcl_oow_t* ptr,
	hcl_oow_t        len
);

#if defined(HCL_USE_OBJECT_TRAILER)
hcl_oop_t hcl_instantiatewithtrailer (
	hcl_t*           hcl, 
	hcl_oop_t        _class,
	hcl_oow_t        vlen,
	const hcl_oob_t* tptr,
	hcl_oow_t        tlen
);
#endif

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
/* dic.c                                                                     */
/* ========================================================================= */
hcl_oop_cons_t hcl_putatsysdic (
	hcl_t*     hcl,
	hcl_oop_t  key,
	hcl_oop_t  value
);

hcl_oop_cons_t hcl_getatsysdic (
	hcl_t*     hcl,
	hcl_oop_t  key
);

hcl_oop_cons_t hcl_lookupsysdic (
	hcl_t*            hcl,
	const hcl_oocs_t* name
);

hcl_oop_cons_t hcl_putatdic (
	hcl_t*        hcl,
	hcl_oop_set_t dic,
	hcl_oop_t     key,
	hcl_oop_t     value
);

hcl_oop_cons_t hcl_getatdic (
	hcl_t*        hcl,
	hcl_oop_set_t dic,
	hcl_oop_t     key
);

hcl_oop_cons_t hcl_lookupdic (
	hcl_t*            hcl,
	hcl_oop_set_t     dic,
	const hcl_oocs_t* name
);

hcl_oop_set_t hcl_makedic (
	hcl_t*    hcl,
	hcl_oow_t size
);

/* ========================================================================= */
/* proc.c                                                                    */
/* ========================================================================= */
hcl_oop_process_t hcl_makeproc (
	hcl_t* hcl
);

/* ========================================================================= */
/* utf8.c                                                                    */
/* ========================================================================= */
hcl_oow_t hcl_uctoutf8 (
	hcl_uch_t    uc,
	hcl_bch_t*   utf8,
	hcl_oow_t    size
);

hcl_oow_t hcl_utf8touc (
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
 * The hcl_utf8toucs() function converts a UTF8 string to a uncide string.
 *
 * It never returns -2 if \a ucs is #HCL_NULL.
 *
 * \code
 *  const hcl_bch_t* bcs = "test string";
 *  hcl_uch_t ucs[100];
 *  hcl_oow_t ucslen = HCL_COUNTOF(buf), n;
 *  hcl_oow_t bcslen = 11;
 *  int n;
 *  n = hcl_utf8toucs (bcs, &bcslen, ucs, &ucslen);
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
int hcl_utf8toucs (
	const hcl_bch_t*   bcs,
	hcl_oow_t*         bcslen,
	hcl_uch_t*         ucs,
	hcl_oow_t*         ucslen
);

/* ========================================================================= */
/* bigint.c   TODO: remove bigint                                            */
/* ========================================================================= */
int hcl_isint (
	hcl_t*    hcl,
	hcl_oop_t x
);

int hcl_inttooow (
	hcl_t*     hcl,
	hcl_oop_t  x,
	hcl_oow_t* w
);

hcl_oop_t hcl_oowtoint (
	hcl_t*     hcl,
	hcl_oow_t  w
);

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
	int         modulo,
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

hcl_oop_t hcl_strtoint (
	hcl_t*            hcl,
	const hcl_ooch_t* str,
	hcl_oow_t         len,
	int                radix
);

hcl_oop_t hcl_inttostr (
	hcl_t*      hcl,
	hcl_oop_t   num,
	int         radix
);


/* ========================================================================= */
/* print.c                                                                   */
/* ========================================================================= */
int hcl_printobj (
	hcl_t*          hcl,
	hcl_oop_t       obj,
	hcl_ioimpl_t    printer,
	hcl_iooutarg_t* outarg
);

/* ========================================================================= */
/* comp.c                                                                    */
/* ========================================================================= */
HCL_EXPORT int hcl_compile (
	hcl_t*         hcl,
	hcl_oop_t      expr
);

/* ========================================================================= */
/* exec.c                                                                    */
/* ========================================================================= */
int hcl_getprimno (
	hcl_t*            hcl,
	const hcl_oocs_t* name
);

/* TODO: remove debugging functions */
/* ========================================================================= */
/* debug.c                                                                   */
/* ========================================================================= */
void dump_symbol_table (hcl_t* hcl);
void dump_dictionary (hcl_t* hcl, hcl_oop_set_t dic, const char* title);

#if defined(__cplusplus)
}
#endif


#endif