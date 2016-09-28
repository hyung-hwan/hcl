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

#ifndef _HCL_H_
#define _HCL_H_

#include "hcl-cmn.h"
#include "hcl-rbt.h"

/* TODO: move this macro out to the build files.... */
#define HCL_INCLUDE_COMPILER

/* ========================================================================== */

/**
 * The hcl_errnum_t type defines the error codes.
 */
enum hcl_errnum_t
{
	HCL_ENOERR,  /**< no error */
	HCL_EOTHER,  /**< other error */
	HCL_ENOIMPL, /**< not implemented */
	HCL_ESYSERR, /**< subsystem error */
	HCL_EINTERN, /**< internal error */
	HCL_ESYSMEM, /**< insufficient system memory */
	HCL_EOOMEM,  /**< insufficient object memory */
	HCL_EINVAL,  /**< invalid parameter or data */
	HCL_ETOOBIG, /**< data too large */
	HCL_EPERM,   /**< operation not permitted */
	HCL_ERANGE,  /**< range error. overflow and underflow */
	HCL_ENOENT,  /**< no matching entry */
	HCL_EDFULL,  /**< dictionary full */
	HCL_EPFULL,  /**< processor full */
	HCL_ESHFULL, /**< semaphore heap full */
	HCL_ESLFULL, /**< semaphore list full */
	HCL_EDIVBY0, /**< divide by zero */
	HCL_EIOERR,  /**< I/O error */
	HCL_EECERR,  /**< encoding conversion error */
	HCL_EFINIS,  /**< end of data/input/stream/etc */
	HCL_ESYNERR  /** < syntax error */
};
typedef enum hcl_errnum_t hcl_errnum_t;

enum hcl_synerrnum_t
{
	HCL_SYNERR_NOERR,
	HCL_SYNERR_ILCHR,         /* illegal character */
	HCL_SYNERR_CMTNC,         /* comment not closed */
	HCL_SYNERR_STRNC,         /* string not closed */
	HCL_SYNERR_HASHLIT,       /* wrong hashed literal */
	HCL_SYNERR_CHARLIT,       /* wrong character literal */
	HCL_SYNERR_RADNUMLIT ,    /* invalid numeric literal with radix */
	HCL_SYNERR_INTRANGE,      /* integer range error */

	HCL_SYNERR_EOF,           /* sudden end of input */
	HCL_SYNERR_LPAREN,        /* ( expected */
	HCL_SYNERR_RPAREN,        /* ) expected */
	HCL_SYNERR_RBRACK,        /* ] expected */

	HCL_SYNERR_STRING,        /* string expected */
	HCL_SYNERR_BYTERANGE,     /* byte too small or too large */
	HCL_SYNERR_NESTING,       /* nesting level too deep */

	HCL_SYNERR_DOTBANNED,     /* . disallowed */
	HCL_SYNERR_INCLUDE,       /* #include error */

	HCL_SYNERR_ARGNAMELIST,   /* argument name list expected */
	HCL_SYNERR_ARGNAME,       /* argument name expected */
	HCL_SYNERR_BLKFLOOD,      /* lambda block too big */
	HCL_SYNERR_VARNAME,       /* variable name expected */
	HCL_SYNERR_ARGCOUNT       /* wrong number of arguments */
};
typedef enum hcl_synerrnum_t hcl_synerrnum_t;

enum hcl_option_t
{
	HCL_TRAIT,
	HCL_LOG_MASK,
	HCL_SYMTAB_SIZE,  /* default system table size */
	HCL_SYSDIC_SIZE,  /* default system dictionary size */
	HCL_PROCSTK_SIZE  /* default process stack size */
};
typedef enum hcl_option_t hcl_option_t;

enum hcl_option_dflval_t
{
	HCL_DFL_SYMTAB_SIZE = 5000,
	HCL_DFL_SYSDIC_SIZE = 5000,
	HCL_DFL_PROCSTK_SIZE = 5000
};
typedef enum hcl_option_dflval_t hcl_option_dflval_t;

enum hcl_trait_t
{
	/* perform no garbage collection when the heap is full. 
	 * you still can use hcl_gc() explicitly. */
	HCL_NOGC = (1 << 0),

	/* wait for running process when exiting from the main method */
	HCL_AWAIT_PROCS = (1 << 1)
};
typedef enum hcl_trait_t hcl_trait_t;

typedef struct hcl_obj_t           hcl_obj_t;
typedef struct hcl_obj_t*          hcl_oop_t;

/* these are more specialized types for hcl_obj_t */
typedef struct hcl_obj_oop_t       hcl_obj_oop_t;
typedef struct hcl_obj_char_t      hcl_obj_char_t;
typedef struct hcl_obj_byte_t      hcl_obj_byte_t;
typedef struct hcl_obj_halfword_t  hcl_obj_halfword_t;
typedef struct hcl_obj_word_t      hcl_obj_word_t;

/* these are more specialized types for hcl_oop_t */
typedef struct hcl_obj_oop_t*      hcl_oop_oop_t;
typedef struct hcl_obj_char_t*     hcl_oop_char_t;
typedef struct hcl_obj_byte_t*     hcl_oop_byte_t;
typedef struct hcl_obj_halfword_t* hcl_oop_halfword_t;
typedef struct hcl_obj_word_t*     hcl_oop_word_t;

#define HCL_OOW_BITS  (HCL_SIZEOF_OOW_T * 8)
#define HCL_OOI_BITS  (HCL_SIZEOF_OOI_T * 8)
#define HCL_OOP_BITS  (HCL_SIZEOF_OOP_T * 8)
#define HCL_OOHW_BITS (HCL_SIZEOF_OOHW_T * 8)


/* ========================================================================= */
/* BIGINT TYPES AND MACROS                                                   */
/* ========================================================================= */
#if HCL_SIZEOF_UINTMAX_T > HCL_SIZEOF_OOW_T
#	define HCL_USE_FULL_WORD
#endif

#if defined(HCL_USE_FULL_WORD)
	typedef hcl_oow_t          hcl_liw_t; /* large integer word */
	typedef hcl_uintmax_t      hcl_lidw_t; /* large integer double word */
#	define HCL_SIZEOF_LIW_T    HCL_SIZEOF_OOW_T
#	define HCL_SIZEOF_LIDW_T   HCL_SIZEOF_UINTMAX_T
#	define HCL_LIW_BITS        HCL_OOW_BITS
#	define HCL_LIDW_BITS       (HCL_SIZEOF_UINTMAX_T * 8) 

	typedef hcl_oop_word_t     hcl_oop_liword_t;
#	define HCL_OBJ_TYPE_LIWORD HCL_OBJ_TYPE_WORD

#else
	typedef hcl_oohw_t         hcl_liw_t;
	typedef hcl_oow_t          hcl_lidw_t;
#	define HCL_SIZEOF_LIW_T    HCL_SIZEOF_OOHW_T
#	define HCL_SIZEOF_LIDW_T   HCL_SIZEOF_OOW_T
#	define HCL_LIW_BITS        HCL_OOHW_BITS
#	define HCL_LIDW_BITS       HCL_OOW_BITS

	typedef hcl_oop_halfword_t hcl_oop_liword_t;
#	define HCL_OBJ_TYPE_LIWORD HCL_OBJ_TYPE_HALFWORD

#endif


/* 
 * OOP encoding
 * An object pointer(OOP) is an ordinary pointer value to an object.
 * but some simple numeric values are also encoded into OOP using a simple
 * bit-shifting and masking.
 *
 * A real OOP is stored without any bit-shifting while a non-OOP value encoded
 * in an OOP is bit-shifted to the left by 2 and the 2 least-significant bits
 * are set to 1 or 2.
 * 
 * This scheme works because the object allocators aligns the object size to
 * a multiple of sizeof(hcl_oop_t). This way, the 2 least-significant bits
 * of a real OOP are always 0s.
 */

#define HCL_OOP_TAG_BITS  2
#define HCL_OOP_TAG_SMINT 1
#define HCL_OOP_TAG_CHAR  2

#define HCL_OOP_IS_NUMERIC(oop) (((hcl_oow_t)oop) & (HCL_OOP_TAG_SMINT | HCL_OOP_TAG_CHAR))
#define HCL_OOP_IS_POINTER(oop) (!HCL_OOP_IS_NUMERIC(oop))
#define HCL_OOP_GET_TAG(oop) (((hcl_oow_t)oop) & HCL_LBMASK(hcl_oow_t, HCL_OOP_TAG_BITS))

#define HCL_OOP_IS_SMOOI(oop) (((hcl_ooi_t)oop) & HCL_OOP_TAG_SMINT)
#define HCL_OOP_IS_CHAR(oop) (((hcl_oow_t)oop) & HCL_OOP_TAG_CHAR)
#define HCL_SMOOI_TO_OOP(num) ((hcl_oop_t)((((hcl_ooi_t)(num)) << HCL_OOP_TAG_BITS) | HCL_OOP_TAG_SMINT))
#define HCL_OOP_TO_SMOOI(oop) (((hcl_ooi_t)oop) >> HCL_OOP_TAG_BITS)
#define HCL_CHAR_TO_OOP(num) ((hcl_oop_t)((((hcl_oow_t)(num)) << HCL_OOP_TAG_BITS) | HCL_OOP_TAG_CHAR))
#define HCL_OOP_TO_CHAR(oop) (((hcl_oow_t)oop) >> HCL_OOP_TAG_BITS)

/* SMOOI takes up 62 bit on a 64-bit architecture and 30 bits 
 * on a 32-bit architecture. The absolute value takes up 61 bits and 29 bits
 * respectively for the 1 sign bit. */
#define HCL_SMOOI_BITS (HCL_OOI_BITS - HCL_OOP_TAG_BITS)
#define HCL_SMOOI_ABS_BITS (HCL_SMOOI_BITS - 1)
#define HCL_SMOOI_MAX ((hcl_ooi_t)(~((hcl_oow_t)0) >> (HCL_OOP_TAG_BITS + 1)))
/* Sacrificing 1 bit pattern for a negative SMOOI makes 
 * implementation a lot eaisier in many respect. */
/*#define HCL_SMOOI_MIN (-HCL_SMOOI_MAX - 1)*/
#define HCL_SMOOI_MIN (-HCL_SMOOI_MAX)
#define HCL_IN_SMOOI_RANGE(ooi)  ((ooi) >= HCL_SMOOI_MIN && (ooi) <= HCL_SMOOI_MAX)

/* TODO: There are untested code where smint is converted to hcl_oow_t.
 *       for example, the spec making macro treats the number as hcl_oow_t instead of hcl_ooi_t.
 *       as of this writing, i skip testing that part with the spec value exceeding HCL_SMOOI_MAX.
 *       later, please verify it works, probably by limiting the value ranges in such macros
 */

/*
 * Object structure
 */
enum hcl_obj_type_t
{
	HCL_OBJ_TYPE_OOP,
	HCL_OBJ_TYPE_CHAR,
	HCL_OBJ_TYPE_BYTE,
	HCL_OBJ_TYPE_HALFWORD,
	HCL_OBJ_TYPE_WORD

/*
	HCL_OBJ_TYPE_UINT8,
	HCL_OBJ_TYPE_UINT16,
	HCL_OBJ_TYPE_UINT32,
*/

/* NOTE: you can have HCL_OBJ_SHORT, HCL_OBJ_INT
 * HCL_OBJ_LONG, HCL_OBJ_FLOAT, HCL_OBJ_DOUBLE, etc 
 * type type field being 6 bits long, you can have up to 64 different types.

	HCL_OBJ_TYPE_SHORT,
	HCL_OBJ_TYPE_INT,
	HCL_OBJ_TYPE_LONG,
	HCL_OBJ_TYPE_FLOAT,
	HCL_OBJ_TYPE_DOUBLE */
};
typedef enum hcl_obj_type_t hcl_obj_type_t;

/* =========================================================================
 * Object header structure 
 * 
 * _flags:
 *   type: the type of a payload item. 
 *         one of HCL_OBJ_TYPE_OOP, HCL_OBJ_TYPE_CHAR, 
 *                HCL_OBJ_TYPE_BYTE, HCL_OBJ_TYPE_HALFWORD, HCL_OBJ_TYPE_WORD
 *   unit: the size of a payload item in bytes. 
 *   extra: 0 or 1. 1 indicates that the payload contains 1 more
 *          item than the value of the size field. used for a 
 *          terminating null in a variable-char object. internel
 *          use only.
 *   kernel: 0 or 1. indicates that the object is a kernel object.
 *           VM disallows layout changes of a kernel object.
 *           internal use only.
 *   moved: 0 or 1. used by GC. internal use only.
 *   ngc: 0 or 1, used by GC. internal use only.
 *   trailer: 0 or 1. indicates that there are trailing bytes
 *            after the object payload. internal use only.
 *
 * _size: the number of payload items in an object.
 *        it doesn't include the header size.
 * 
 * The total number of bytes occupied by an object can be calculated
 * with this fomula:
 *    sizeof(hcl_obj_t) + ALIGN((size + extra) * unit), sizeof(hcl_oop_t))
 * 
 * If the type is known to be not HCL_OBJ_TYPE_CHAR, you can assume that 
 * 'extra' is 0. So you can simplify the fomula in such a context.
 *    sizeof(hcl_obj_t) + ALIGN(size * unit), sizeof(hcl_oop_t))
 *
 * The ALIGN() macro is used above since allocation adjusts the payload
 * size to a multiple of sizeof(hcl_oop_t). it assumes that sizeof(hcl_obj_t)
 * is a multiple of sizeof(hcl_oop_t). See the HCL_BYTESOF() macro.
 * 
 * Due to the header structure, an object can only contain items of
 * homogeneous data types in the payload. 
 *
 * It's actually possible to split the size field into 2. For example,
 * the upper half contains the number of oops and the lower half contains
 * the number of bytes/chars. This way, a variable-byte class or a variable-char
 * class can have normal instance variables. On the contrary, the actual byte
 * size calculation and the access to the payload fields become more complex. 
 * Therefore, i've dropped the idea.
 * ========================================================================= */
#define HCL_OBJ_FLAGS_TYPE_BITS     6
#define HCL_OBJ_FLAGS_UNIT_BITS     5
#define HCL_OBJ_FLAGS_EXTRA_BITS    1
#define HCL_OBJ_FLAGS_KERNEL_BITS   2
#define HCL_OBJ_FLAGS_MOVED_BITS    1
#define HCL_OBJ_FLAGS_NGC_BITS      1
#define HCL_OBJ_FLAGS_TRAILER_BITS  1
#define HCL_OBJ_FLAGS_SYNCODE_BITS  4
#define HCL_OBJ_FLAGS_BRAND_BITS    6


#define HCL_OBJ_FLAGS_TYPE_SHIFT    (HCL_OBJ_FLAGS_UNIT_BITS    + HCL_OBJ_FLAGS_UNIT_SHIFT)
#define HCL_OBJ_FLAGS_UNIT_SHIFT    (HCL_OBJ_FLAGS_EXTRA_BITS   + HCL_OBJ_FLAGS_EXTRA_SHIFT)
#define HCL_OBJ_FLAGS_EXTRA_SHIFT   (HCL_OBJ_FLAGS_KERNEL_BITS  + HCL_OBJ_FLAGS_KERNEL_SHIFT)
#define HCL_OBJ_FLAGS_KERNEL_SHIFT  (HCL_OBJ_FLAGS_MOVED_BITS   + HCL_OBJ_FLAGS_MOVED_SHIFT)
#define HCL_OBJ_FLAGS_MOVED_SHIFT   (HCL_OBJ_FLAGS_NGC_BITS     + HCL_OBJ_FLAGS_NGC_SHIFT)
#define HCL_OBJ_FLAGS_NGC_SHIFT     (HCL_OBJ_FLAGS_TRAILER_BITS + HCL_OBJ_FLAGS_TRAILER_SHIFT)
#define HCL_OBJ_FLAGS_TRAILER_SHIFT (HCL_OBJ_FLAGS_SYNCODE_BITS + HCL_OBJ_FLAGS_SYNCODE_SHIFT)
#define HCL_OBJ_FLAGS_SYNCODE_SHIFT (HCL_OBJ_FLAGS_BRAND_BITS   + HCL_OBJ_FLAGS_BRAND_SHIFT)
#define HCL_OBJ_FLAGS_BRAND_SHIFT   (0)

#define HCL_OBJ_GET_FLAGS_TYPE(oop)       HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_TYPE_SHIFT,    HCL_OBJ_FLAGS_TYPE_BITS)
#define HCL_OBJ_GET_FLAGS_UNIT(oop)       HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_UNIT_SHIFT,    HCL_OBJ_FLAGS_UNIT_BITS)
#define HCL_OBJ_GET_FLAGS_EXTRA(oop)      HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_EXTRA_SHIFT,   HCL_OBJ_FLAGS_EXTRA_BITS)
#define HCL_OBJ_GET_FLAGS_KERNEL(oop)     HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_KERNEL_SHIFT,  HCL_OBJ_FLAGS_KERNEL_BITS)
#define HCL_OBJ_GET_FLAGS_MOVED(oop)      HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_MOVED_SHIFT,   HCL_OBJ_FLAGS_MOVED_BITS)
#define HCL_OBJ_GET_FLAGS_NGC(oop)        HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_NGC_SHIFT,     HCL_OBJ_FLAGS_NGC_BITS)
#define HCL_OBJ_GET_FLAGS_TRAILER(oop)    HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_TRAILER_SHIFT, HCL_OBJ_FLAGS_TRAILER_BITS)
#define HCL_OBJ_GET_FLAGS_SYNCODE(oop)    HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_SYNCODE_SHIFT, HCL_OBJ_FLAGS_SYNCODE_BITS)
#define HCL_OBJ_GET_FLAGS_BRAND(oop)      HCL_GETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_BRAND_SHIFT,   HCL_OBJ_FLAGS_BRAND_BITS)

#define HCL_OBJ_SET_FLAGS_TYPE(oop,v)     HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_TYPE_SHIFT,    HCL_OBJ_FLAGS_TYPE_BITS,     v)
#define HCL_OBJ_SET_FLAGS_UNIT(oop,v)     HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_UNIT_SHIFT,    HCL_OBJ_FLAGS_UNIT_BITS,     v)
#define HCL_OBJ_SET_FLAGS_EXTRA(oop,v)    HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_EXTRA_SHIFT,   HCL_OBJ_FLAGS_EXTRA_BITS,    v)
#define HCL_OBJ_SET_FLAGS_KERNEL(oop,v)   HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_KERNEL_SHIFT,  HCL_OBJ_FLAGS_KERNEL_BITS,   v)
#define HCL_OBJ_SET_FLAGS_MOVED(oop,v)    HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_MOVED_SHIFT,   HCL_OBJ_FLAGS_MOVED_BITS,    v)
#define HCL_OBJ_SET_FLAGS_NGC(oop,v)      HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_NGC_SHIFT,     HCL_OBJ_FLAGS_NGC_BITS,      v)
#define HCL_OBJ_SET_FLAGS_TRAILER(oop,v)  HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_TRAILER_SHIFT, HCL_OBJ_FLAGS_TRAILER_BITS,  v)
#define HCL_OBJ_SET_FLAGS_SYNCODE(oop,v)  HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_SYNCODE_SHIFT, HCL_OBJ_FLAGS_SYNCODE_BITS,  v)
#define HCL_OBJ_SET_FLAGS_BRAND(oop,v)    HCL_SETBITS(hcl_oow_t, (oop)->_flags, HCL_OBJ_FLAGS_BRAND_SHIFT,   HCL_OBJ_FLAGS_BRAND_BITS,    v)

#define HCL_OBJ_GET_SIZE(oop) ((oop)->_size)
#define HCL_OBJ_GET_CLASS(oop) ((oop)->_class)

#define HCL_OBJ_SET_SIZE(oop,v) ((oop)->_size = (v))
#define HCL_OBJ_SET_CLASS(oop,c) ((oop)->_class = (c))

/* [NOTE] this macro doesn't include the size of the trailer */
#define HCL_OBJ_BYTESOF(oop) ((HCL_OBJ_GET_SIZE(oop) + HCL_OBJ_GET_FLAGS_EXTRA(oop)) * HCL_OBJ_GET_FLAGS_UNIT(oop))

/* [NOTE] this macro doesn't check the range of the actual value.
 *        make sure that the value of each bit fields given falls within the 
 *        possible range of the defined bits */
#define HCL_OBJ_MAKE_FLAGS(t,u,e,k,m,g,r,b) ( \
	(((hcl_oow_t)(t)) << HCL_OBJ_FLAGS_TYPE_SHIFT) | \
	(((hcl_oow_t)(u)) << HCL_OBJ_FLAGS_UNIT_SHIFT) | \
	(((hcl_oow_t)(e)) << HCL_OBJ_FLAGS_EXTRA_SHIFT) | \
	(((hcl_oow_t)(k)) << HCL_OBJ_FLAGS_KERNEL_SHIFT) | \
	(((hcl_oow_t)(m)) << HCL_OBJ_FLAGS_MOVED_SHIFT) | \
	(((hcl_oow_t)(g)) << HCL_OBJ_FLAGS_NGC_SHIFT) | \
	(((hcl_oow_t)(r)) << HCL_OBJ_FLAGS_TRAILER_SHIFT) | \
	(((hcl_oow_t)(b)) << HCL_OBJ_FLAGS_BRAND_SHIFT) \
)

#define HCL_OBJ_HEADER \
	hcl_oow_t _flags; \
	hcl_oow_t _size; \
	hcl_oop_t _class

struct hcl_obj_t
{
	HCL_OBJ_HEADER;
};

struct hcl_obj_oop_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t slot[1];
};

struct hcl_obj_char_t
{
	HCL_OBJ_HEADER;
	hcl_ooch_t slot[1];
};

struct hcl_obj_byte_t
{
	HCL_OBJ_HEADER;
	hcl_oob_t slot[1];
};

struct hcl_obj_halfword_t
{
	HCL_OBJ_HEADER;
	hcl_oohw_t slot[1];
};

struct hcl_obj_word_t
{
	HCL_OBJ_HEADER;
	hcl_oow_t slot[1];
};

typedef struct hcl_trailer_t hcl_trailer_t;
struct hcl_trailer_t
{
	hcl_oow_t size;
	hcl_oob_t slot[1];
};

#define HCL_SET_NAMED_INSTVARS 2
typedef struct hcl_set_t hcl_set_t;
typedef struct hcl_set_t* hcl_oop_set_t;
struct hcl_set_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t     tally;  /* SmallInteger */
	hcl_oop_oop_t bucket; /* Array */
};

#define HCL_CLASS_NAMED_INSTVARS 11
typedef struct hcl_class_t hcl_class_t;
typedef struct hcl_class_t* hcl_oop_class_t;
struct hcl_class_t
{
	HCL_OBJ_HEADER;

	hcl_oop_t      spec;          /* SmallInteger. instance specification */
	hcl_oop_t      selfspec;      /* SmallInteger. specification of the class object itself */

	hcl_oop_t      superclass;    /* Another class */
	hcl_oop_t      subclasses;    /* Array of subclasses */

	hcl_oop_char_t name;          /* Symbol */

	/* == NEVER CHANGE THIS ORDER OF 3 ITEMS BELOW == */
	hcl_oop_char_t instvars;      /* String */
	hcl_oop_char_t classvars;     /* String */
	hcl_oop_char_t classinstvars; /* String */
	/* == NEVER CHANGE THE ORDER OF 3 ITEMS ABOVE == */

	hcl_oop_char_t pooldics;      /* String */

	/* [0] - instance methods, MethodDictionary
	 * [1] - class methods, MethodDictionary */
	hcl_oop_set_t  mthdic[2];      

	/* indexed part afterwards */
	hcl_oop_t      slot[1];   /* class instance variables and class variables. */
};
#define HCL_CLASS_MTHDIC_INSTANCE 0
#define HCL_CLASS_MTHDIC_CLASS    1


#if defined(HCL_USE_OBJECT_TRAILER)
#	define HCL_METHOD_NAMED_INSTVARS 8
#else
#	define HCL_METHOD_NAMED_INSTVARS 9
#endif
typedef struct hcl_method_t hcl_method_t;
typedef struct hcl_method_t* hcl_oop_method_t;
struct hcl_method_t
{
	HCL_OBJ_HEADER;

	hcl_oop_class_t owner; /* Class */

	hcl_oop_char_t  name; /* Symbol, method name */

	/* primitive number */
	hcl_oop_t       preamble; /* SmallInteger */

	hcl_oop_t       preamble_data[2]; /* SmallInteger */

	/* number of temporaries including arguments */
	hcl_oop_t       tmpr_count; /* SmallInteger */

	/* number of arguments in temporaries */
	hcl_oop_t       tmpr_nargs; /* SmallInteger */

#if defined(HCL_USE_OBJECT_TRAILER)
	/* no code field is used */
#else
	hcl_oop_byte_t  code; /* ByteArray */
#endif

	hcl_oop_t       source; /* TODO: what should I put? */

	/* == variable indexed part == */
	hcl_oop_t       slot[1]; /* it stores literals */
};

/* The preamble field is composed of a 8-bit code and a 16-bit
 * index. 
 *
 * The code can be one of the following values:
 *  0 - no special action
 *  1 - return self
 *  2 - return nil
 *  3 - return true
 *  4 - return false 
 *  5 - return index.
 *  6 - return -index.
 *  7 - return instvar[index] 
 *  8 - do primitive[index]
 *  9 - do named primitive[index]
 * 10 - exception handler
 */
#define HCL_METHOD_MAKE_PREAMBLE(code,index)  ((((hcl_ooi_t)index) << 8) | ((hcl_ooi_t)code))
#define HCL_METHOD_GET_PREAMBLE_CODE(preamble) (((hcl_ooi_t)preamble) & 0xFF)
#define HCL_METHOD_GET_PREAMBLE_INDEX(preamble) (((hcl_ooi_t)preamble) >> 8)

#define HCL_METHOD_PREAMBLE_NONE            0
#define HCL_METHOD_PREAMBLE_RETURN_RECEIVER 1
#define HCL_METHOD_PREAMBLE_RETURN_NIL      2
#define HCL_METHOD_PREAMBLE_RETURN_TRUE     3
#define HCL_METHOD_PREAMBLE_RETURN_FALSE    4
#define HCL_METHOD_PREAMBLE_RETURN_INDEX    5
#define HCL_METHOD_PREAMBLE_RETURN_NEGINDEX 6
#define HCL_METHOD_PREAMBLE_RETURN_INSTVAR  7
#define HCL_METHOD_PREAMBLE_PRIMITIVE       8
#define HCL_METHOD_PREAMBLE_NAMED_PRIMITIVE 9 /* index is an index to the symbol table */
#define HCL_METHOD_PREAMBLE_EXCEPTION       10

/* the index is an 16-bit unsigned integer. */
#define HCL_METHOD_PREAMBLE_INDEX_MIN 0x0000
#define HCL_METHOD_PREAMBLE_INDEX_MAX 0xFFFF
#define HCL_OOI_IN_PREAMBLE_INDEX_RANGE(num) ((num) >= HCL_METHOD_PREAMBLE_INDEX_MIN && (num) <= HCL_METHOD_PREAMBLE_INDEX_MAX)

#define HCL_CONTEXT_NAMED_INSTVARS 8
typedef struct hcl_context_t hcl_context_t;
typedef struct hcl_context_t* hcl_oop_context_t;
struct hcl_context_t
{
	HCL_OBJ_HEADER;

	/* it points to the active context at the moment when
	 * this context object has been activated. a new method context
	 * is activated as a result of normal message sending and a block
	 * context is activated when it is sent 'value'. it's set to
	 * nil if a block context created hasn't received 'value'. */
	hcl_oop_context_t  sender;

	/* SmallInteger, instruction pointer */
	hcl_oop_t          ip;

	/* SmallInteger, stack pointer.
	 * For a method context, this pointer stores the stack pointer
	 * of the active process before it gets activated. */
	hcl_oop_t          sp;

	/* SmallInteger. Number of temporaries.
	 * For a block context, it's inclusive of the temporaries
	 * defined its 'home'. */
	hcl_oop_t          ntmprs;

	/* CompiledMethod for a method context, 
	 * SmallInteger for a block context */
	hcl_oop_t          method_or_nargs;

	/* it points to the receiver of the message for a method context.
	 * a base block context(created but not yet activated) has nil in this 
	 * field. if a block context is activated by 'value', it points 
	 * to the block context object used as a base for shallow-copy. */
	hcl_oop_t          receiver_or_source;

	/* it is set to nil for a method context.
	 * for a block context, it points to the active context at the 
	 * moment the block context was created. that is, it points to 
	 * a method context where the base block has been defined. 
	 * an activated block context copies this field from the source. */
	hcl_oop_t          home;

	/* when a method context is created, it is set to itself. no change is
	 * made when the method context is activated. when a block context is 
	 * created (when MAKE_BLOCK or BLOCK_COPY is executed), it is set to the
	 * origin of the active context. when the block context is shallow-copied
	 * for activation (when it is sent 'value'), it is set to the origin of
	 * the source block context. */
	hcl_oop_context_t  origin; 

	/* variable indexed part */
	hcl_oop_t          slot[1]; /* stack */
};


#define HCL_PROCESS_NAMED_INSTVARS 7
typedef struct hcl_process_t hcl_process_t;
typedef struct hcl_process_t* hcl_oop_process_t;

#define HCL_SEMAPHORE_NAMED_INSTVARS 6
typedef struct hcl_semaphore_t hcl_semaphore_t;
typedef struct hcl_semaphore_t* hcl_oop_semaphore_t;

struct hcl_process_t
{
	HCL_OBJ_HEADER;
	hcl_oop_context_t initial_context;
	hcl_oop_context_t current_context;

	hcl_oop_t         state; /* SmallInteger */
	hcl_oop_t         sp;    /* stack pointer. SmallInteger */

	hcl_oop_process_t prev;
	hcl_oop_process_t next;

	hcl_oop_semaphore_t sem;

	/* == variable indexed part == */
	hcl_oop_t slot[1]; /* process stack */
};

struct hcl_semaphore_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t count; /* SmallInteger */
	hcl_oop_process_t waiting_head;
	hcl_oop_process_t waiting_tail;
	hcl_oop_t heap_index; /* index to the heap */
	hcl_oop_t heap_ftime_sec; /* firing time */
	hcl_oop_t heap_ftime_nsec; /* firing time */
};

#define HCL_PROCESS_SCHEDULER_NAMED_INSTVARS 5
typedef struct hcl_process_scheduler_t hcl_process_scheduler_t;
typedef struct hcl_process_scheduler_t* hcl_oop_process_scheduler_t;
struct hcl_process_scheduler_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t tally; /* SmallInteger, the number of runnable processes */
	hcl_oop_process_t active; /*  pointer to an active process in the runnable process list */
	hcl_oop_process_t runnable_head; /* runnable process list */
	hcl_oop_process_t runnable_tail; /* runnable process list */
	hcl_oop_t sempq; /* SemaphoreHeap */
};

/**
 * The HCL_CLASSOF() macro return the class of an object including a numeric
 * object encoded into a pointer.
 */
#define HCL_CLASSOF(hcl,oop) ( \
	HCL_OOP_IS_SMOOI(oop)? (hcl)->_small_integer: \
	HCL_OOP_IS_CHAR(oop)? (hcl)->_character: HCL_OBJ_GET_CLASS(oop) \
)

/**
 * The HCL_BRANDOF() macro return the brand of an object including a numeric
 * object encoded into a pointer.
 */
#define HCL_BRANDOF(hcl,oop) ( \
	HCL_OOP_IS_SMOOI(oop)? HCL_BRAND_INTEGER: \
	HCL_OOP_IS_CHAR(oop)? HCL_BRAND_CHARACTER: HCL_OBJ_GET_FLAGS_BRAND(oop) \
)

/**
 * The HCL_BYTESOF() macro returns the size of the payload of
 * an object in bytes. If the pointer given encodes a numeric value, 
 * it returns the size of #hcl_oow_t in bytes.
 */
#define HCL_BYTESOF(hcl,oop) \
	(HCL_OOP_IS_NUMERIC(oop)? HCL_SIZEOF(hcl_oow_t): HCL_OBJ_BYTESOF(oop))

/**
 * The HCL_ISTYPEOF() macro is a safe replacement for HCL_OBJ_GET_FLAGS_TYPE()
 */
#define HCL_ISTYPEOF(hcl,oop,type) \
	(!HCL_OOP_IS_NUMERIC(oop) && HCL_OBJ_GET_FLAGS_TYPE(oop) == (type))

typedef struct hcl_heap_t hcl_heap_t;

struct hcl_heap_t
{
	hcl_uint8_t* base;  /* start of a heap */
	hcl_uint8_t* limit; /* end of a heap */
	hcl_uint8_t* ptr;   /* next allocation pointer */
};

typedef struct hcl_t hcl_t;

/* =========================================================================
 * VIRTUAL MACHINE PRIMITIVES
 * ========================================================================= */
#define HCL_MOD_NAME_LEN_MAX 120

typedef void* (*hcl_mod_open_t) (hcl_t* hcl, const hcl_uch_t* name);
typedef void (*hcl_mod_close_t) (hcl_t* hcl, void* handle);
typedef void* (*hcl_mod_getsym_t) (hcl_t* hcl, void* handle, const hcl_uch_t* name);
typedef void (*hcl_log_write_t) (hcl_t* hcl, hcl_oow_t mask, const hcl_ooch_t* msg, hcl_oow_t len);

struct hcl_vmprim_t
{
	hcl_mod_open_t mod_open;
	hcl_mod_close_t mod_close;
	hcl_mod_getsym_t mod_getsym;
	hcl_log_write_t log_write;
};

typedef struct hcl_vmprim_t hcl_vmprim_t;

/* =========================================================================
 * IO MANIPULATION
 * ========================================================================= */

enum hcl_iocmd_t
{
	HCL_IO_OPEN,
	HCL_IO_CLOSE,
	HCL_IO_READ,
	HCL_IO_WRITE
};
typedef enum hcl_iocmd_t hcl_iocmd_t;

struct hcl_ioloc_t
{
	unsigned long int  line; /**< line */
	unsigned long int  colm; /**< column */
	const hcl_ooch_t*  file; /**< file specified in #include */
};
typedef struct hcl_ioloc_t hcl_ioloc_t;

struct hcl_iolxc_t
{
	hcl_ooci_t   c; /**< character */
	hcl_ioloc_t  l; /**< location */
};
typedef struct hcl_iolxc_t hcl_iolxc_t;

typedef struct hcl_ioinarg_t hcl_ioinarg_t;
struct hcl_ioinarg_t
{
	/** 
	 * [IN] I/O object name.
	 * It is #HCL_NULL for the main stream and points to a non-NULL string
	 * for an included stream.
	 */
	const hcl_ooch_t* name;   

	/** 
	 * [OUT] I/O handle set by a handler. 
	 * The source stream handler can set this field when it opens a stream.
	 * All subsequent operations on the stream see this field as set
	 * during opening.
	 */
	void* handle;

	/**
	 * [OUT] place data here 
	 */
	hcl_ooch_t buf[1024];

	/**
	 * [IN] points to the data of the includer. It is #HCL_NULL for the
	 * main stream.
	 */
	hcl_ioinarg_t* includer;

	/*-----------------------------------------------------------------*/
	/*----------- from here down, internal use only -------------------*/
	struct
	{
		int pos, len, state;
	} b;

	unsigned long int line;
	unsigned long int colm;
	hcl_ooci_t nl;

	hcl_iolxc_t lxc;
	/*-----------------------------------------------------------------*/
};

typedef struct hcl_iooutarg_t hcl_iooutarg_t;
struct hcl_iooutarg_t
{
	void*        handle;
	hcl_ooch_t*  ptr;
	hcl_oow_t    len;
};

typedef hcl_ooi_t (*hcl_ioimpl_t) (
	hcl_t*        hcl,
	hcl_iocmd_t   cmd,
	void*         arg /* hcl_ioinarg_t* or hcl_iooutarg_t* */ 
);

/* =========================================================================
 * CALLBACK MANIPULATION
 * ========================================================================= */
typedef void (*hcl_cbimpl_t) (hcl_t* hcl);

typedef struct hcl_cb_t hcl_cb_t;
struct hcl_cb_t
{
	hcl_cbimpl_t gc;
	hcl_cbimpl_t fini;

	/* private below */
	hcl_cb_t*     prev;
	hcl_cb_t*     next;
};


/* =========================================================================
 * PRIMITIVE MODULE MANIPULATION
 * ========================================================================= */
typedef int (*hcl_prim_impl_t) (hcl_t* hcl, hcl_ooi_t nargs);

typedef struct hcl_prim_mod_t hcl_prim_mod_t;

typedef int (*hcl_prim_mod_load_t) (
	hcl_t*          hcl,
	hcl_prim_mod_t* mod
);

typedef hcl_prim_impl_t (*hcl_prim_mod_query_t) (
	hcl_t*           hcl,
	hcl_prim_mod_t*  mod,
	const hcl_uch_t* name
);

typedef void (*hcl_prim_mod_unload_t) (
	hcl_t*          hcl,
	hcl_prim_mod_t* mod
);

struct hcl_prim_mod_t
{
	hcl_prim_mod_unload_t unload;
	hcl_prim_mod_query_t  query;
	void*                  ctx;
};

struct hcl_prim_mod_data_t 
{
	void* handle;
	hcl_prim_mod_t mod;
};
typedef struct hcl_prim_mod_data_t hcl_prim_mod_data_t;

/* =========================================================================
 * HCL VM
 * ========================================================================= */
typedef struct hcl_synerr_t hcl_synerr_t;
struct hcl_synerr_t
{
	hcl_synerrnum_t num;
	hcl_ioloc_t     loc;
	hcl_oocs_t      tgt;
};

#if defined(HCL_INCLUDE_COMPILER)
typedef struct hcl_compiler_t hcl_compiler_t;
#endif

struct hcl_t
{
	hcl_mmgr_t*  mmgr;
	hcl_errnum_t errnum;

	struct
	{
		int trait;
		unsigned int log_mask;
		hcl_oow_t dfl_symtab_size;
		hcl_oow_t dfl_sysdic_size;
		hcl_oow_t dfl_procstk_size; 
	} option;

	hcl_vmprim_t vmprim;

	hcl_cb_t* cblist;
	hcl_rbt_t pmtable; /* primitive module table */

	struct
	{
		hcl_ooch_t* ptr;
		hcl_oow_t len;
		hcl_oow_t capa;
		int last_mask;
	} log;
	/* ========================= */

	hcl_heap_t* permheap; /* TODO: put kernel objects to here */
	hcl_heap_t* curheap;
	hcl_heap_t* newheap;

	/* ========================= */
	hcl_oop_t _nil;  /* pointer to the nil object */
	hcl_oop_t _true;
	hcl_oop_t _false;

	hcl_oop_t _begin; /* symbol */
	hcl_oop_t _defun; /* symbol */
	hcl_oop_t _if;     /* symbol */
	hcl_oop_t _lambda; /* symbol */
	hcl_oop_t _quote; /* symbol */
	hcl_oop_t _set; /* symbol */

	/* == NEVER CHANGE THE ORDER OF FIELDS BELOW == */
	/* hcl_ignite() assumes this order. make sure to update symnames in ignite_3() */
	hcl_oop_t _class; /* Class */
	hcl_oop_t _character;
	hcl_oop_t _small_integer; /* SmallInteger */
	hcl_oop_t _large_positive_integer; /* LargePositiveInteger */
	hcl_oop_t _large_negative_integer; /* LargeNegativeInteger */
	/* == NEVER CHANGE THE ORDER OF FIELDS ABOVE == */


	hcl_oop_set_t symtab; /* system-wide symbol table. */
	hcl_oop_set_t sysdic; /* system dictionary. */
	hcl_oop_process_scheduler_t processor; /* instance of ProcessScheduler */
	hcl_oop_process_t nil_process; /* instance of Process */

	/* pending asynchronous semaphores */
	hcl_oop_semaphore_t* sem_list;
	hcl_oow_t sem_list_count;
	hcl_oow_t sem_list_capa;

	/* semaphores sorted according to time-out */
	hcl_oop_semaphore_t* sem_heap;
	hcl_oow_t sem_heap_count;
	hcl_oow_t sem_heap_capa;

	hcl_oop_t* tmp_stack[256]; /* stack for temporaries */
	hcl_oow_t tmp_count;

	/* == EXECUTION REGISTERS == */
	hcl_oop_context_t active_context;
	hcl_oop_method_t active_method;
	hcl_oob_t* active_code;
	hcl_ooi_t sp;
	hcl_ooi_t ip;
	int proc_switched; /* TODO: this is temporary. implement something else to skip immediate context switching */
	/* == END EXECUTION REGISTERS == */

	/* == BIGINT CONVERSION == */
	struct
	{
		int safe_ndigits;
		hcl_oow_t multiplier;
	} bigint[37];
	/* == END BIGINT CONVERSION == */

	struct
	{
		struct
		{
			hcl_oop_t arr; /* byte code array - not part of object memory */
			hcl_oow_t len;
		} bc;

		struct
		{
			hcl_oop_t arr; /* literal array - not part of object memory */
			hcl_oow_t len;
		} lit;
	} code;

	/* == PRINTER == */
	struct
	{
		struct
		{
			void*        ptr;
			hcl_oow_t    capa;
			hcl_oow_t    size;
		} s;
		hcl_oop_t e; /* top entry being printed */
	} p;
	/* == PRINTER == */

#if defined(HCL_INCLUDE_COMPILER)
	hcl_compiler_t* c;
#endif
};

/* =========================================================================
 * HCL VM LOGGING
 * ========================================================================= */

enum hcl_log_mask_t
{
	HCL_LOG_DEBUG     = (1 << 0),
	HCL_LOG_INFO      = (1 << 1),
	HCL_LOG_WARN      = (1 << 2),
	HCL_LOG_ERROR     = (1 << 3),
	HCL_LOG_FATAL     = (1 << 4),

	HCL_LOG_MNEMONIC  = (1 << 8), /* bytecode mnemonic */
	HCL_LOG_GC        = (1 << 9),
	HCL_LOG_IC        = (1 << 10), /* instruction cycle, fetch-decode-execute */
	HCL_LOG_PRIMITIVE = (1 << 11),
	HCL_LOG_APP       = (1 << 12) /* hcl applications, set by hcl logging primitive */
};
typedef enum hcl_log_mask_t hcl_log_mask_t;

#define HCL_LOG_ENABLED(hcl,mask) ((hcl)->option.log_mask & (mask))

#define HCL_LOG0(hcl,mask,fmt) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt); } while(0)
#define HCL_LOG1(hcl,mask,fmt,a1) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1); } while(0)
#define HCL_LOG2(hcl,mask,fmt,a1,a2) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2); } while(0)
#define HCL_LOG3(hcl,mask,fmt,a1,a2,a3) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3); } while(0)
#define HCL_LOG4(hcl,mask,fmt,a1,a2,a3,a4) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3, a4); } while(0)
#define HCL_LOG5(hcl,mask,fmt,a1,a2,a3,a4,a5) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3, a4, a5); } while(0)

#define HCL_DEBUG0(hcl,fmt) HCL_LOG0(hcl, HCL_LOG_DEBUG, fmt)
#define HCL_DEBUG1(hcl,fmt,a1) HCL_LOG1(hcl, HCL_LOG_DEBUG, fmt, a1)
#define HCL_DEBUG2(hcl,fmt,a1,a2) HCL_LOG2(hcl, HCL_LOG_DEBUG, fmt, a1, a2)
#define HCL_DEBUG3(hcl,fmt,a1,a2,a3) HCL_LOG3(hcl, HCL_LOG_DEBUG, fmt, a1, a2, a3)
#define HCL_DEBUG4(hcl,fmt,a1,a2,a3,a4) HCL_LOG4(hcl, HCL_LOG_DEBUG, fmt, a1, a2, a3, a4)
#define HCL_DEBUG5(hcl,fmt,a1,a2,a3,a4,a5) HCL_LOG5(hcl, HCL_LOG_DEBUG, fmt, a1, a2, a3, a4, a5)

#define HCL_INFO0(hcl,fmt) HCL_LOG0(hcl, HCL_LOG_INFO, fmt)
#define HCL_INFO1(hcl,fmt,a1) HCL_LOG1(hcl, HCL_LOG_INFO, fmt, a1)
#define HCL_INFO2(hcl,fmt,a1,a2) HCL_LOG2(hcl, HCL_LOG_INFO, fmt, a1, a2)
#define HCL_INFO3(hcl,fmt,a1,a2,a3) HCL_LOG3(hcl, HCL_LOG_INFO, fmt, a1, a2, a3)
#define HCL_INFO4(hcl,fmt,a1,a2,a3,a4) HCL_LOG4(hcl, HCL_LOG_INFO, fmt, a1, a2, a3, a4)
#define HCL_INFO5(hcl,fmt,a1,a2,a3,a4,a5) HCL_LOG5(hcl, HCL_LOG_INFO, fmt, a1, a2, a3, a4, a5


/* =========================================================================
 * HCL COMMON OBJECTS
 * ========================================================================= */
enum 
{
	HCL_BRAND_NIL,
	HCL_BRAND_TRUE,
	HCL_BRAND_FALSE,
	HCL_BRAND_CHARACTER,
	HCL_BRAND_INTEGER,
	HCL_BRAND_CONS,
	HCL_BRAND_ARRAY,
	HCL_BRAND_BYTE_ARRAY,
	HCL_BRAND_SYMBOL,
	HCL_BRAND_STRING,
	HCL_BRAND_SET,

	HCL_BRAND_ENVIRONMENT, 
	HCL_BRAND_CFRAME,/* compiler frame */

	HCL_BRAND_PROCESS
};

enum
{
	/* SYNCODE 0 means it's not a syncode object. so it begins with 1 */
	HCL_SYNCODE_BEGIN = 1, 
	HCL_SYNCODE_DEFUN,
	HCL_SYNCODE_IF,
	HCL_SYNCODE_LAMBDA,
	HCL_SYNCODE_QUOTE,
	HCL_SYNCODE_SET,
};

struct hcl_cons_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t car;
	hcl_oop_t cdr;
};
typedef struct hcl_cons_t hcl_cons_t;
typedef struct hcl_cons_t* hcl_oop_cons_t;

#define HCL_IS_NIL(hcl,v) (v == (hcl)->_nil)
#define HCL_IS_SYMBOL(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_SYMBOL)

#define HCL_CONS_CAR(v)  (((hcl_cons_t*)(v))->car)
#define HCL_CONS_CDR(v)  (((hcl_cons_t*)(v))->cdr)

#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_t* hcl_open (
	hcl_mmgr_t*         mmgr,
	hcl_oow_t           xtnsize,
	hcl_oow_t           heapsize,
	const hcl_vmprim_t* vmprim,
	hcl_errnum_t*       errnum
);

HCL_EXPORT void hcl_close (
	hcl_t* vm
);

HCL_EXPORT int hcl_init (
	hcl_t*              vm,
	hcl_mmgr_t*         mmgr,
	hcl_oow_t           heapsize,
	const hcl_vmprim_t* vmprim
);

HCL_EXPORT void hcl_fini (
	hcl_t* vm
);


HCL_EXPORT hcl_mmgr_t* hcl_getmmgr (
	hcl_t* hcl
);

HCL_EXPORT void* hcl_getxtn (
	hcl_t* hcl
);


HCL_EXPORT hcl_errnum_t hcl_geterrnum (
	hcl_t* hcl
);

HCL_EXPORT void hcl_seterrnum (
	hcl_t*       hcl,
	hcl_errnum_t errnum
);

/**
 * The hcl_getoption() function gets the value of an option
 * specified by \a id into the buffer pointed to by \a value.
 *
 * \return 0 on success, -1 on failure
 */
HCL_EXPORT int hcl_getoption (
	hcl_t*        hcl,
	hcl_option_t  id,
	void*          value
);

/**
 * The hcl_setoption() function sets the value of an option 
 * specified by \a id to the value pointed to by \a value.
 *
 * \return 0 on success, -1 on failure
 */
HCL_EXPORT int hcl_setoption (
	hcl_t*       hcl,
	hcl_option_t id,
	const void*   value
);


HCL_EXPORT hcl_cb_t* hcl_regcb (
	hcl_t*    hcl,
	hcl_cb_t* tmpl
);

HCL_EXPORT void hcl_deregcb (
	hcl_t*    hcl,
	hcl_cb_t* cb
);

/**
 * The hcl_gc() function performs garbage collection.
 * It is not affected by #HCL_NOGC.
 */
HCL_EXPORT void hcl_gc (
	hcl_t* hcl
);

HCL_EXPORT hcl_oow_t hcl_getpayloadbytes (
	hcl_t*    hcl,
	hcl_oop_t oop
);

/**
 * The hcl_instantiate() function creates a new object of the class 
 * \a _class. The size of the fixed part is taken from the information
 * contained in the class defintion. The \a vlen parameter specifies 
 * the length of the variable part. The \a vptr parameter points to 
 * the memory area to copy into the variable part of the new object.
 * If \a vptr is #HCL_NULL, the variable part is initialized to 0 or
 * an equivalent value depending on the type.
 */
HCL_EXPORT hcl_oop_t hcl_instantiate (
	hcl_t*          hcl,
	hcl_oop_t       _class,
	const void*      vptr,
	hcl_oow_t       vlen
);

HCL_EXPORT hcl_oop_t hcl_shallowcopy (
	hcl_t*          hcl,
	hcl_oop_t       oop
);

/**
 * The hcl_ignite() function creates key initial objects.
 */
HCL_EXPORT int hcl_ignite (
	hcl_t* hcl
);

/**
 * The hcl_execute() function executes an activated context.
 */
HCL_EXPORT int hcl_execute (
	hcl_t* hcl
);

/**
 * The hcl_invoke() function sends a message named \a mthname to an object
 * named \a objname.
 */
HCL_EXPORT int hcl_invoke (
	hcl_t*            hcl,
	const hcl_oocs_t* objname,
	const hcl_oocs_t* mthname
);


HCL_EXPORT int hcl_attachio (
	hcl_t*       hcl,
	hcl_ioimpl_t reader,
	hcl_ioimpl_t printer
);

HCL_EXPORT void hcl_detachio (
	hcl_t*       hcl
);


HCL_EXPORT hcl_oop_t hcl_read (
	hcl_t*       hcl
);

HCL_EXPORT int hcl_print (
	hcl_t*       hcl,
	hcl_oop_t    obj
);

HCL_EXPORT int hcl_compile (
	hcl_t*       hcl,
	hcl_oop_t    obj
);

/* Temporary OOP management  */
HCL_EXPORT void hcl_pushtmp (
	hcl_t*     hcl,
	hcl_oop_t* oop_ptr
);

HCL_EXPORT void hcl_poptmp (
	hcl_t*     hcl
);

HCL_EXPORT void hcl_poptmps (
	hcl_t*     hcl,
	hcl_oow_t  count
);


HCL_EXPORT int hcl_decode (
	hcl_t*            hcl,
	hcl_oow_t         start,
	hcl_oow_t         end
);

/* Syntax error handling */
HCL_EXPORT void hcl_getsynerr (
	hcl_t*             hcl,
	hcl_synerr_t*      synerr
);

HCL_EXPORT void hcl_setsynerr (
	hcl_t*             hcl,
	hcl_synerrnum_t    num,
	const hcl_ioloc_t* loc,
	const hcl_oocs_t*  tgt
);


/* Memory allocation/deallocation functions using hcl's MMGR */

HCL_EXPORT void* hcl_allocmem (
	hcl_t*     hcl,
	hcl_oow_t size
);

HCL_EXPORT void* hcl_callocmem (
	hcl_t*     hcl,
	hcl_oow_t size
);

HCL_EXPORT void* hcl_reallocmem (
	hcl_t*     hcl,
	void*       ptr,
	hcl_oow_t size
);

HCL_EXPORT void hcl_freemem (
	hcl_t* hcl,
	void*   ptr
);

HCL_EXPORT hcl_ooi_t hcl_logbfmt (
	hcl_t*           hcl,
	hcl_oow_t        mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT hcl_ooi_t hcl_logoofmt (
	hcl_t*            hcl,
	hcl_oow_t         mask,
	const hcl_ooch_t* fmt,
	...
);


HCL_EXPORT hcl_oop_t hcl_makenil (
	hcl_t*     hcl
);

HCL_EXPORT hcl_oop_t hcl_maketrue (
	hcl_t*     hcl
);

HCL_EXPORT hcl_oop_t hcl_makefalse (
	hcl_t*     hcl
);

HCL_EXPORT hcl_oop_t hcl_makeinteger (
	hcl_t*     hcl,
	hcl_ooi_t  v
);

HCL_EXPORT hcl_oop_t hcl_makecons (
	hcl_t*     hcl,
	hcl_oop_t  car,
	hcl_oop_t  cdr
);

HCL_EXPORT hcl_oop_t hcl_makearray (
	hcl_t*     hcl,
	hcl_oow_t  size
);

HCL_EXPORT hcl_oop_t hcl_makebytearray (
	hcl_t*           hcl,
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oop_t hcl_makestring (
	hcl_t*            hcl,
	const hcl_ooch_t* ptr,
	hcl_oow_t         len
);

HCL_EXPORT hcl_oop_t hcl_makeset (
	hcl_t*            hcl, 
	hcl_oow_t         inisize /* initial bucket size */
);


HCL_EXPORT void hcl_freengcobj (
	hcl_t*           hcl,
	hcl_oop_t        obj
);

HCL_EXPORT hcl_oop_t hcl_makengcbytearray (
	hcl_t*           hcl,
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oop_t hcl_remakengcbytearray (
	hcl_t*           hcl,
	hcl_oop_t        obj,
	hcl_oow_t        newsz
);

HCL_EXPORT hcl_oop_t hcl_makengcarray (
	hcl_t*           hcl,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oop_t hcl_remakengcarray (
	hcl_t*           hcl,
	hcl_oop_t        obj,
	hcl_oow_t        newsz
);



HCL_EXPORT hcl_oow_t hcl_countcons (
	hcl_t*           hcl,
	hcl_oop_t        cons
);


HCL_EXPORT hcl_oop_t hcl_getlastconscdr (
	hcl_t*           hcl,
	hcl_oop_t        cons
);

HCL_EXPORT hcl_oop_t hcl_reversecons (
	hcl_t*           hcl,
	hcl_oop_t        cons
);

#if defined(__cplusplus)
}
#endif


#endif
