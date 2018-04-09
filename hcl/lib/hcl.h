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

#ifndef _HCL_H_
#define _HCL_H_

#include "hcl-cmn.h"
#include "hcl-rbt.h"
#include <stdarg.h>

/* TODO: move this macro out to the build files.... */
#define HCL_INCLUDE_COMPILER

/* ========================================================================== */

typedef struct hcl_mod_t hcl_mod_t;

/* ========================================================================== */

/**
 * The hcl_errnum_t type defines the error codes.
 */
enum hcl_errnum_t
{
	HCL_ENOERR,   /**< no error */
	HCL_EGENERIC, /**< generic error */
	HCL_ENOIMPL,  /**< not implemented */
	HCL_ESYSERR,  /**< subsystem error */
	HCL_EINTERN,  /**< internal error */
	
	HCL_ESYSMEM,  /**< insufficient system memory */
	HCL_EOOMEM,   /**< insufficient object memory */
	HCL_ETYPE,    /**< invalid class/type */
	HCL_EINVAL,   /**< invalid parameter or data */
	HCL_ENOENT,   /**< data not found */
	
	HCL_EEXIST,   /**< existing/duplicate data */
	HCL_EBUSY, 
	HCL_EACCES,
	HCL_EPERM,
	HCL_ENOTDIR,
	
	HCL_EINTR,
	HCL_EPIPE,
	HCL_EAGAIN,
	HCL_EBADHND,
	HCL_EFRMFLOOD, /**< too many frames */
	
	HCL_EMSGRCV,   /**< mesasge receiver error */
	HCL_EMSGSND,   /**< message sending error. even doesNotUnderstand: is not found */
	HCL_ENUMARGS,  /**< wrong number of arguments */
	HCL_ERANGE,    /**< range error. overflow and underflow */
	HCL_EBCFULL,   /**< byte-code full */
	
	HCL_EDFULL,    /**< dictionary full */
	HCL_EPFULL,    /**< processor full */
	HCL_EFINIS,    /**< unexpected end of data/input/stream/etc */
	HCL_EFLOOD,    /**< too many items/data */
	HCL_EDIVBY0,   /**< divide by zero */
	
	HCL_EIOERR,    /**< I/O error */
	HCL_EECERR,    /**< encoding conversion error */
	HCL_EBUFFULL,  /**< buffer full */
	HCL_ESYNERR,   /**< syntax error */
	HCL_ECALL,     /**< runtime error - cannot call */
	
	HCL_ERECALL,   /**< runtime error - cannot call again */
	HCL_ECALLARG   /**< runtime error - wrong number of arguments to call */
};
typedef enum hcl_errnum_t hcl_errnum_t;

enum hcl_synerrnum_t
{
	HCL_SYNERR_NOERR,
	HCL_SYNERR_ILCHR,         /* illegal character */
	HCL_SYNERR_ILTOK,         /* invalid token */
	HCL_SYNERR_CMTNC,         /* comment not closed */
	HCL_SYNERR_STRCHRNC,      /* string/character not closed */
	HCL_SYNERR_HASHLIT,       /* wrong hashed literal */
	HCL_SYNERR_CHARLIT,       /* wrong character literal */
	HCL_SYNERR_NUMLIT ,       /* invalid numeric literal */
	HCL_SYNERR_NUMRANGE,      /* number range error */
	HCL_SYNERR_ERRORLIT,      /* wrong error literal */
	HCL_SYNERR_SMPTRLIT,      /* wrong smptr literal */
	HCL_SYNERR_MSEGIDENT,     /* wrong multi-segment identifier */

	HCL_SYNERR_EOF,           /* sudden end of input */
	HCL_SYNERR_LPAREN,        /* ( expected */
	HCL_SYNERR_RPAREN,        /* ) expected */
	HCL_SYNERR_RBRACK,        /* ] expected */
	HCL_SYNERR_RBRACE,        /* } expected */
	HCL_SYNERR_VBAR,          /* | expected */

	HCL_SYNERR_STRING,        /* string expected */
	HCL_SYNERR_BYTERANGE,     /* byte too small or too large */
	HCL_SYNERR_NESTING,       /* nesting level too deep */

	HCL_SYNERR_COMMA,         /* , expected */
	HCL_SYNERR_VBARBANNED,    /* | disallowed */
	HCL_SYNERR_DOTBANNED,     /* . disallowed */
	HCL_SYNERR_COMMABANNED,   /* , disallowed */
	HCL_SYNERR_COLONBANNED,   /* : disallowed */
	HCL_SYNERR_COMMANOVALUE,  /* no value after , */
	HCL_SYNERR_COLONNOVALUE,  /* no value after : */
	HCL_SYNERR_NOSEP,         /* no seperator between array/dictionary elements */
	HCL_SYNERR_INCLUDE,       /* #include error */

	HCL_SYNERR_LOOPFLOOD,     /* loop body too big */
	HCL_SYNERR_IFFLOOD,       /* if body too big */
	HCL_SYNERR_BLKFLOOD,      /* lambda block too big */
	HCL_SYNERR_BLKDEPTH,      /* lambda block too deep */
	HCL_SYNERR_ARGNAMELIST,   /* argument name list expected */
	HCL_SYNERR_ARGNAME,       /* argument name expected */
	HCL_SYNERR_ARGNAMEDUP,    /* duplicate argument name  */
	HCL_SYNERR_VARNAME,       /* variable name expected */
	HCL_SYNERR_ARGCOUNT,      /* wrong number of arguments */
	HCL_SYNERR_ARGFLOOD,      /* too many arguments defined */
	HCL_SYNERR_VARFLOOD,      /* too many variables defined */
	HCL_SYNERR_VARDCLBANNED,  /* variable declaration disallowed */
	HCL_SYNERR_VARNAMEDUP,    /* duplicate variable name */

	HCL_SYNERR_BANNEDVARNAME, /* disallowed varible name */
	HCL_SYNERR_BANNEDARGNAME, /* disallowed argument name */

	HCL_SYNERR_ELIF,          /* elif without if */
	HCL_SYNERR_ELSE,          /* else without if */
	HCL_SYNERR_BREAK,         /* break outside loop */

	HCL_SYNERR_CALLABLE,      /* invalid callable */
	HCL_SYNERR_UNBALKV,       /* unbalanced key/value pair */
	HCL_SYNERR_UNBALPBB,      /* unbalanced parenthesis/brace/bracket */
	HCL_SYNERR_EMPTYXLIST     /* empty x-list */
};
typedef enum hcl_synerrnum_t hcl_synerrnum_t;

enum hcl_option_t
{
	HCL_TRAIT,
	HCL_LOG_MASK,
	HCL_LOG_MAXCAPA,
	HCL_SYMTAB_SIZE,  /* default system table size */
	HCL_SYSDIC_SIZE,  /* default system dictionary size */
	HCL_PROCSTK_SIZE, /* default process stack size */
	HCL_MOD_INCTX
};
typedef enum hcl_option_t hcl_option_t;

/* [NOTE] ensure that it is a power of 2 */
#define HCL_LOG_CAPA_ALIGN 512

enum hcl_option_dflval_t
{
	HCL_DFL_LOG_MAXCAPA = HCL_LOG_CAPA_ALIGN * 16,
	HCL_DFL_SYMTAB_SIZE = 5000,
	HCL_DFL_SYSDIC_SIZE = 5000,
	HCL_DFL_PROCSTK_SIZE = 5000
};
typedef enum hcl_option_dflval_t hcl_option_dflval_t;

enum hcl_trait_t
{
#if defined(HCL_BUILD_DEBUG)
	HCL_DEBUG_GC     = (1 << 0),
	HCL_DEBUG_BIGINT = (1 << 1),
#endif

	/* perform no garbage collection when the heap is full. 
	 * you still can use hcl_gc() explicitly. */
	HCL_NOGC = (1 << 8),

	/* wait for running process when exiting from the main method */
	HCL_AWAIT_PROCS = (1 << 9)
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
#if (HCL_SIZEOF_UINTMAX_T > HCL_SIZEOF_OOW_T)
#	define HCL_USE_FULL_WORD
#endif

#if defined(HCL_USE_FULL_WORD)
	typedef hcl_oow_t          hcl_liw_t; /* large integer word */
	typedef hcl_ooi_t          hcl_lii_t;
	typedef hcl_uintmax_t      hcl_lidw_t; /* large integer double word */
	typedef hcl_intmax_t       hcl_lidi_t;
#	define HCL_SIZEOF_LIW_T    HCL_SIZEOF_OOW_T
#	define HCL_SIZEOF_LIDW_T   HCL_SIZEOF_UINTMAX_T
#	define HCL_LIW_BITS        HCL_OOW_BITS
#	define HCL_LIDW_BITS       (HCL_SIZEOF_UINTMAX_T * 8) 

	typedef hcl_oop_word_t     hcl_oop_liword_t;
#	define HCL_OBJ_TYPE_LIWORD HCL_OBJ_TYPE_WORD

#else
	typedef hcl_oohw_t         hcl_liw_t;
	typedef hcl_oohi_t         hcl_lii_t;
	typedef hcl_oow_t          hcl_lidw_t;
	typedef hcl_ooi_t          hcl_lidi_t;
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
 * A real OOP is stored without any bit-shifting while a non-pointer value encoded
 * in an OOP is bit-shifted to the left by 2 and the 2 least-significant bits
 * are set to 1 or 2.
 * 
 * This scheme works because the object allocators aligns the object size to
 * a multiple of sizeof(moo_oop_t). This way, the 2 least-significant bits
 * of a real OOP are always 0s.
 *
 * With 2 bits, i can encode only 3 special types except object pointers. 
 * Since I need more than 3 special types, I extend the tag bits up to 4 bits
 * to represent a special data type that doesn't require a range as wide
 * as a small integer. A unicode character, for instance, only requires 21 
 * bits at most. An error doesn't need to be as diverse as a small integer.
 */

#define HCL_OOP_TAG_BITS_LO     2
#define HCL_OOP_TAG_BITS_HI     2

#define HCL_OOP_TAG_SMOOI       1    /* 01 */
#define HCL_OOP_TAG_SMPTR       2    /* 10 */
#define HCL_OOP_TAG_EXTENDED    3    /* 11 - internal use only */
#define HCL_OOP_TAG_CHAR        3    /* 0011 */
#define HCL_OOP_TAG_ERROR       7    /* 0111 */
#define HCL_OOP_TAG_RESERVED0   11   /* 1011 */
#define HCL_OOP_TAG_RESERVED1   15   /* 1111 */

#define HCL_OOP_GET_TAG_LO(oop) (((hcl_oow_t)oop) & HCL_LBMASK(hcl_oow_t, HCL_OOP_TAG_BITS_LO))
#define HCL_OOP_GET_TAG_LOHI(oop) (((hcl_oow_t)oop) & HCL_LBMASK(hcl_oow_t, HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_HI))
#define HCL_OOP_GET_TAG(oop) (HCL_OOP_GET_TAG_LO(oop) == HCL_OOP_TAG_EXTENDED? HCL_OOP_GET_TAG_LOHI(oop): HCL_OOP_GET_TAG_LO(oop))

#define HCL_OOP_IS_NUMERIC(oop) (HCL_OOP_GET_TAG_LO(oop) != 0)
#define HCL_OOP_IS_POINTER(oop) (HCL_OOP_GET_TAG_LO(oop) == 0)

#define HCL_OOP_IS_SMOOI(oop) (HCL_OOP_GET_TAG_LO(oop) == HCL_OOP_TAG_SMOOI)
#define HCL_OOP_IS_SMPTR(oop) (HCL_OOP_GET_TAG_LO(oop) == HCL_OOP_TAG_SMPTR)

#define HCL_SMOOI_TO_OOP(num) ((hcl_oop_t)((((hcl_ooi_t)(num)) << HCL_OOP_TAG_BITS_LO) | HCL_OOP_TAG_SMOOI))
#define HCL_OOP_TO_SMOOI(oop) (((hcl_ooi_t)oop) >> HCL_OOP_TAG_BITS_LO)
/*
#define HCL_SMPTR_TO_OOP(num) ((hcl_oop_t)((((hcl_ooi_t)(num)) << HCL_OOP_TAG_BITS_LO) | HCL_OOP_TAG_SMPTR))
#define HCL_OOP_TO_SMPTR(oop) (((hcl_ooi_t)oop) >> HCL_OOP_TAG_BITS_LO)
*/
#define HCL_SMPTR_TO_OOP(ptr) ((hcl_oop_t)(((hcl_oow_t)ptr) | HCL_OOP_TAG_SMPTR))
#define HCL_OOP_TO_SMPTR(oop) ((void*)(((hcl_oow_t)oop) & ~HCL_LBMASK(hcl_oow_t, HCL_OOP_TAG_BITS_LO)))

#define HCL_OOP_IS_CHAR(oop) (HCL_OOP_GET_TAG(oop) == HCL_OOP_TAG_CHAR)
#define HCL_OOP_IS_ERROR(oop) (HCL_OOP_GET_TAG(oop) == HCL_OOP_TAG_ERROR)

#define HCL_OOP_TO_CHAR(oop) (((hcl_oow_t)oop) >> (HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_LO))
#define HCL_CHAR_TO_OOP(num) ((hcl_oop_t)((((hcl_oow_t)(num)) << (HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_LO)) | HCL_OOP_TAG_CHAR))
#define HCL_OOP_TO_ERROR(oop) (((hcl_oow_t)oop) >> (HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_LO))
#define HCL_ERROR_TO_OOP(num) ((hcl_oop_t)((((hcl_oow_t)(num)) << (HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_LO)) | HCL_OOP_TAG_ERROR))

/* SMOOI takes up 62 bit on a 64-bit architecture and 30 bits 
 * on a 32-bit architecture. The absolute value takes up 61 bits and 29 bits
 * respectively for the 1 sign bit. */
#define HCL_SMOOI_BITS (HCL_OOI_BITS - HCL_OOP_TAG_BITS_LO)
#define HCL_SMOOI_ABS_BITS (HCL_SMOOI_BITS - 1)
#define HCL_SMOOI_MAX ((hcl_ooi_t)(~((hcl_oow_t)0) >> (HCL_OOP_TAG_BITS_LO + 1)))
/* Sacrificing 1 bit pattern for a negative SMOOI makes 
 * implementation a lot eaisier in many respect. */
/*#define HCL_SMOOI_MIN (-HCL_SMOOI_MAX - 1)*/
#define HCL_SMOOI_MIN (-HCL_SMOOI_MAX)
#define HCL_IN_SMOOI_RANGE(ooi)  ((ooi) >= HCL_SMOOI_MIN && (ooi) <= HCL_SMOOI_MAX)


/* SMPTR is a special value which has been devised to encode an address value
 * whose low HCL_OOP_TAG_BITS_LO bits are 0. its class is SmallPointer. A pointer
 * returned by most of system functions would be aligned to the pointer size. 
 * you can use the followings macros when converting such a pointer without loss. */
#define HCL_IN_SMPTR_RANGE(ptr) ((((hcl_oow_t)ptr) & HCL_LBMASK(hcl_oow_t, HCL_OOP_TAG_BITS_LO)) == 0)

#define HCL_CHAR_BITS (HCL_OOI_BITS - HCL_OOP_TAG_BITS_LO - HCL_OOP_TAG_BITS_HI)
#define HCL_CHAR_MIN 0
#define HCL_CHAR_MAX (~((hcl_oow_t)0) >> (HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_HI))

#define HCL_ERROR_BITS (HCL_OOI_BITS - HCL_OOP_TAG_BITS_LO - HCL_OOP_TAG_BITS_HI)
#define HCL_ERROR_MIN 0
#define HCL_ERROR_MAX (~((hcl_oow_t)0) >> (HCL_OOP_TAG_BITS_LO + HCL_OOP_TAG_BITS_HI))

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
 *   kernel: 0 - ordinary object.
 *           1 - kernel object. can survive hcl_reset().
 *           2 - kernel object. can survive hcl_reset(). 
 *               a symbol object with 2 in the kernel bits cannot be assigned a
 *               value with the 'set' special form.
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

#define HCL_OBJ_GET_OOP_SLOT(oop)      (((hcl_oop_oop_t)(oop))->slot)
#define HCL_OBJ_GET_CHAR_SLOT(oop)     (((hcl_oop_char_t)(oop))->slot)
#define HCL_OBJ_GET_BYTE_SLOT(oop)     (((hcl_oop_byte_t)(oop))->slot)
#define HCL_OBJ_GET_HALFWORD_SLOT(oop) (((hcl_oop_halfword_t)(oop))->slot)
#define HCL_OBJ_GET_WORD_SLOT(oop)     (((hcl_oop_word_t)(oop))->slot)

typedef struct hcl_trailer_t hcl_trailer_t;
struct hcl_trailer_t
{
	hcl_oow_t size;
	hcl_oob_t slot[1];
};

#define HCL_CONS_NAMED_INSTVARS 2
typedef struct hcl_cons_t hcl_cons_t;
typedef struct hcl_cons_t* hcl_oop_cons_t;
struct hcl_cons_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t car;
	hcl_oop_t cdr;
};

#define HCL_DIC_NAMED_INSTVARS 2
typedef struct hcl_dic_t hcl_dic_t;
typedef struct hcl_dic_t* hcl_oop_dic_t;
struct hcl_dic_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t     tally;  /* SmallInteger */
	hcl_oop_oop_t bucket; /* Array */
};

#define HCL_FPDEC_NAMED_INSTVARS 2
typedef struct hcl_fpdec_t hcl_fpdec_t;
typedef struct hcl_fpdec_t* hcl_oop_fpdec_t;
struct hcl_fpdec_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t value; /* smooi or bigint */
	hcl_oop_t scale; /* smooi, positive */
};

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


#define HCL_PROCESS_NAMED_INSTVARS 8 /* TODO: RENAME THIS TO SOMETHING ELSE */
typedef struct hcl_process_t hcl_process_t;
typedef struct hcl_process_t* hcl_oop_process_t;

#define HCL_SEMAPHORE_NAMED_INSTVARS 6 /* TODO: RENAME THIS TO SOMETHIGN ELSE */
typedef struct hcl_semaphore_t hcl_semaphore_t;
typedef struct hcl_semaphore_t* hcl_oop_semaphore_t;

struct hcl_process_t
{
	HCL_OBJ_HEADER;
	hcl_oop_context_t initial_context;
	hcl_oop_context_t current_context;

	hcl_oop_t         id; /* SmallInteger */
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

#define HCL_PROCESS_SCHEDULER_NAMED_INSTVARS 4
typedef struct hcl_process_scheduler_t hcl_process_scheduler_t;
typedef struct hcl_process_scheduler_t* hcl_oop_process_scheduler_t;
struct hcl_process_scheduler_t
{
	HCL_OBJ_HEADER;
	hcl_oop_t tally; /* SmallInteger, the number of runnable processes */
	hcl_oop_process_t active; /*  pointer to an active process in the runnable process list */
	hcl_oop_process_t runnable_head; /* runnable process list */
	hcl_oop_process_t runnable_tail; /* runnable process list */
};

/**
 * The HCL_BRANDOF() macro return the brand of an object including a numeric
 * object encoded into a pointer.
 */
#define HCL_BRANDOF(moo,oop) \
	(HCL_OOP_GET_TAG(oop)? ((hcl)->tagged_brands[HCL_OOP_GET_TAG(oop)]): HCL_OBJ_GET_FLAGS_BRAND(oop))

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

/* =========================================================================
 * VIRTUAL MACHINE PRIMITIVES
 * ========================================================================= */

typedef void* (*hcl_alloc_heap_t) (
	hcl_t*             hcl,
	hcl_oow_t          size
);

typedef void (*hcl_free_heap_t) (
	hcl_t*             hcl,
	void*              ptr
);

typedef void (*hcl_log_write_t) (
	hcl_t*             hcl,
	unsigned int       mask,
	const hcl_ooch_t*  msg,
	hcl_oow_t          len
);

typedef void (*hcl_syserrstrb_t) (
	hcl_t*             hcl,
	int                syserr,
	hcl_bch_t*         buf,
	hcl_oow_t          len
);

typedef void (*hcl_syserrstru_t) (
	hcl_t*             hcl,
	int                syserr,
	hcl_uch_t*         buf,
	hcl_oow_t          len
);

enum hcl_vmprim_opendl_flag_t
{
	HCL_VMPRIM_OPENDL_PFMOD = (1 << 0)
};
typedef enum hcl_vmprim_opendl_flag_t hcl_vmprim_opendl_flag_t;

typedef void* (*hcl_vmprim_dlopen_t) (
	hcl_t*             hcl,
	const hcl_ooch_t*  name,
	int                flags
);

typedef void (*hcl_vmprim_dlclose_t) (
	hcl_t*             hcl,
	void*              handle
);

typedef void* (*hcl_vmprim_dlgetsym_t) (
	hcl_t*             hcl,
	void*              handle,
	const hcl_ooch_t*  name
);

typedef void (*hcl_vmprim_gettime_t) (
	hcl_t*             hcl,
	hcl_ntime_t*       now
);

typedef void (*hcl_vmprim_sleep_t) (
	hcl_t*             hcl,
	const hcl_ntime_t* duration);

struct hcl_vmprim_t
{
	/* The alloc_heap callback function is called very earlier
	 * before hcl is fully initialized. so few features are availble
	 * in this callback function. If it's not provided, the default
	 * implementation is used. */
	hcl_alloc_heap_t      alloc_heap; /* optional */

	/* If you customize the heap allocator by providing the alloc_heap
	 * callback, you should implement the heap freer. otherwise the default
	 * implementation doesn't know how to free the heap allocated by 
	 * the allocator callback. */
	hcl_free_heap_t       free_heap; /* optional */

	hcl_log_write_t       log_write; /* required */
	hcl_syserrstrb_t      syserrstrb; /* one of syserrstrb or syserrstru required */
	hcl_syserrstru_t      syserrstru;

	hcl_vmprim_dlopen_t   dl_open; /* required */
	hcl_vmprim_dlclose_t  dl_close; /* required */
	hcl_vmprim_dlgetsym_t dl_getsym; /* requried */

	hcl_vmprim_gettime_t  vm_gettime; /* required */
	hcl_vmprim_sleep_t    vm_sleep; /* required */
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
	 * [OUT] place data here for #HCL_IO_READ
	 */
	hcl_ooch_t buf[2048];

	/**
	 * [OUT] place the number of characters read here for #HCL_IO_READ
	 */
	hcl_oow_t  xlen;

	/**
	 * [IN] points to the data of the includer. It is #HCL_NULL for the
	 * main stream.
	 */
	hcl_ioinarg_t* includer;

	/*-----------------------------------------------------------------*/
	/*----------- from here down, internal use only -------------------*/
	struct
	{
		hcl_oow_t pos;
		hcl_oow_t len;
		int state;
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
	/** 
	 * [OUT] I/O handle set by a handler. 
	 * The source stream handler can set this field when it opens a stream.
	 * All subsequent operations on the stream see this field as set
	 * during opening.
	 */
	void*        handle;

	/**
	 * [IN] the pointer to the beginning of the character string
	 *      to write
	 */
	hcl_ooch_t*  ptr;

	/**
	 * [IN] total number of characters to write
	 */
	hcl_oow_t    len; 

	/**
	 * [OUT] place the number of characters written here for HCL_IO_WRITE
	 */
	hcl_oow_t    xlen;
};

/** 
 * The hcl_ioimpl_t type defines a callback function prototype
 * for I/O operations.
 */
typedef int (*hcl_ioimpl_t) (
	hcl_t*        hcl,
	hcl_iocmd_t   cmd,
	void*         arg /* hcl_ioinarg_t* or hcl_iooutarg_t* */ 
);

/* =========================================================================
 * CALLBACK MANIPULATION
 * ========================================================================= */


typedef void (*hcl_cb_fini_t) (hcl_t* hcl);
typedef void (*hcl_cb_gc_t) (hcl_t* hcl);
typedef int (*hcl_cb_vm_startup_t) (hcl_t* hcl);
typedef void (*hcl_cb_vm_cleanup_t) (hcl_t* hcl);
typedef void (*hcl_cb_vm_checkbc_t) (hcl_t* hcl, hcl_oob_t bcode);

typedef struct hcl_cb_t hcl_cb_t;
struct hcl_cb_t
{
	hcl_cb_gc_t gc;
	hcl_cb_fini_t fini;
	
	hcl_cb_vm_startup_t vm_startup;
	hcl_cb_vm_cleanup_t vm_cleanup;
	hcl_cb_vm_checkbc_t vm_checkbc;

	/* private below */
	hcl_cb_t*     prev;
	hcl_cb_t*     next;
};


/* =========================================================================
 * PRIMITIVE FUNCTIONS
 * ========================================================================= */
enum hcl_pfrc_t
{
	HCL_PF_FAILURE = -1,
	HCL_PF_SUCCESS = 0
};
typedef enum hcl_pfrc_t hcl_pfrc_t;

typedef hcl_pfrc_t (*hcl_pfimpl_t) (
	hcl_t*     hcl,
	hcl_mod_t* mod,
	hcl_ooi_t  nargs
);

enum hcl_pfbase_type_t
{
	HCL_PFBASE_FUNC  = 0,
	HCL_PFBASE_VAR   = 1,
	HCL_PFBASE_CONST = 2
};
typedef enum hcl_pfbase_type_t hcl_pfbase_type_t;

typedef struct hcl_pfbase_t hcl_pfbase_t;
struct hcl_pfbase_t
{
	hcl_pfbase_type_t type;
	hcl_pfimpl_t      handler;
	hcl_oow_t         minargs;
	hcl_oow_t         maxargs;
};

typedef struct hcl_pfinfo_t hcl_pfinfo_t;
struct hcl_pfinfo_t
{
	hcl_ooch_t        mthname[32];
	hcl_pfbase_t      base;
};
/* =========================================================================
 * PRIMITIVE MODULE MANIPULATION
 * ========================================================================= */
#define HCL_MOD_NAME_LEN_MAX 120

typedef int (*hcl_mod_load_t) (
	hcl_t*     hcl,
	hcl_mod_t* mod
);

typedef hcl_pfbase_t* (*hcl_mod_query_t) (
	hcl_t*            hcl,
	hcl_mod_t*        mod,
	const hcl_ooch_t* name,
	hcl_oow_t         namelen
);

typedef void (*hcl_mod_unload_t) (
	hcl_t*     hcl,
	hcl_mod_t* mod
);

typedef void (*hcl_mod_gc_t) (
	hcl_t*     hcl,
	hcl_mod_t* mod
);

struct hcl_mod_t
{
	/* input */
	hcl_ooch_t       name[HCL_MOD_NAME_LEN_MAX + 1];
	void*            inctx;

	/* user-defined data - the module intializer shoudl fill in the following fields. */
	hcl_mod_query_t  query;
	hcl_mod_unload_t unload;
	hcl_mod_gc_t     gc;
	void*            ctx;
};

struct hcl_mod_data_t 
{
	void*           handle;
	hcl_rbt_pair_t* pair; /* internal backreference to hcl->modtab */
	hcl_mod_t       mod;
};
typedef struct hcl_mod_data_t hcl_mod_data_t;

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

#define HCL_ERRMSG_CAPA (2048)

struct hcl_t
{
	hcl_mmgr_t*  mmgr;
	hcl_cmgr_t*  cmgr;

	hcl_errnum_t errnum;
	struct
	{
		union
		{
			hcl_ooch_t ooch[HCL_ERRMSG_CAPA];
			hcl_bch_t bch[HCL_ERRMSG_CAPA];
			hcl_uch_t uch[HCL_ERRMSG_CAPA];
		} tmpbuf;
		hcl_ooch_t buf[HCL_ERRMSG_CAPA];
		hcl_oow_t len;
	} errmsg;
	int shuterr;

	struct
	{
		unsigned int trait;
		unsigned int log_mask;
		hcl_oow_t log_maxcapa;
		hcl_oow_t dfl_symtab_size;
		hcl_oow_t dfl_sysdic_size;
		hcl_oow_t dfl_procstk_size; 
		void* mod_inctx;

	#if defined(HCL_BUILD_DEBUG)
		/* set automatically when trait is set */
		hcl_oow_t karatsuba_cutoff;
	#endif
	} option;

	hcl_vmprim_t vmprim;

	hcl_oow_t vm_checkbc_cb_count;
	hcl_cb_t* cblist;
	hcl_rbt_t modtab; /* primitive module table */

	struct
	{
		hcl_ooch_t* ptr;
		hcl_oow_t len;
		hcl_oow_t capa;
		unsigned int last_mask;
		unsigned int default_type_mask;
	} log;
	/* ========================= */

	hcl_heap_t* permheap; /* TODO: put kernel objects to here */
	hcl_heap_t* curheap;
	hcl_heap_t* newheap;

	/* ========================= */
	hcl_oop_t _nil;  /* pointer to the nil object */
	hcl_oop_t _true;
	hcl_oop_t _false;

	hcl_oop_t _and;    /* symbol */
	hcl_oop_t _break;  /* symbol */
	hcl_oop_t _defun;  /* symbol */
	hcl_oop_t _do;     /* symbol */
	hcl_oop_t _elif;   /* symbol */
	hcl_oop_t _else;   /* symbol */
	hcl_oop_t _if;     /* symbol */
	hcl_oop_t _lambda; /* symbol */
	hcl_oop_t _or;     /* symbol */
	hcl_oop_t _return; /* symbol */
	hcl_oop_t _set;    /* symbol */
	hcl_oop_t _until;  /* symbol */
	hcl_oop_t _while;  /* symbol */

	hcl_oop_dic_t symtab; /* system-wide symbol table. */
	hcl_oop_dic_t sysdic; /* system dictionary. */
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

	hcl_oop_t* proc_map;
	hcl_oow_t proc_map_capa;
	hcl_ooi_t proc_map_free_first;
	hcl_ooi_t proc_map_free_last;

	/* 2 tag bits(lo) + 2 extended tag bits(hi). not all slots are used
	 * because the 2 high extended bits are used only if the low tag bits
	 * are 3 */
	int tagged_brands[16];

	/* == EXECUTION REGISTERS == */
	hcl_oop_context_t initial_context; /* fake initial context */
	hcl_oop_context_t active_context;
	hcl_ooi_t sp;
	hcl_ooi_t ip;
	int proc_switched; /* TODO: this is temporary. implement something else to skip immediate context switching */
	int switch_proc;
	int abort_req;
	hcl_oop_t last_retv;

	hcl_ntime_t exec_start_time;
	hcl_ntime_t exec_end_time;
	/* == END EXECUTION REGISTERS == */

	/* == BIGINT CONVERSION == */
	struct
	{
		int safe_ndigits;
		hcl_oow_t multiplier;
	} bigint[37];

	struct
	{
		struct
		{
			hcl_ooch_t* ptr;
			hcl_oow_t capa;
			hcl_oow_t len;
		} xbuf;
		struct
		{
			hcl_liw_t* ptr;
			hcl_oow_t capa;
		} t;
	} inttostr;
	/* == END BIGINT CONVERSION == */

	struct
	{
		struct
		{
			hcl_ooch_t* ptr;
			hcl_oow_t capa;
			hcl_oow_t len;
		} xbuf; /* buffer to support sprintf */
	} sprintf;

	struct
	{
		struct
		{
			hcl_oop_byte_t arr; /* byte code array - not part of object memory */
			hcl_oow_t len;
		} bc;

		struct
		{
			hcl_oop_oop_t arr; /* literal array - not part of object memory */
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


/* TODO: stack bound check when pushing */
#define HCL_STACK_PUSH(hcl,v) \
	do { \
		(hcl)->sp = (hcl)->sp + 1; \
		(hcl)->processor->active->slot[(hcl)->sp] = v; \
	} while (0)

#define HCL_STACK_GET(hcl,v_sp) ((hcl)->processor->active->slot[v_sp])
#define HCL_STACK_SET(hcl,v_sp,v_obj) ((hcl)->processor->active->slot[v_sp] = v_obj)

#define HCL_STACK_GETTOP(hcl) HCL_STACK_GET(hcl, (hcl)->sp)
#define HCL_STACK_SETTOP(hcl,v_obj) HCL_STACK_SET(hcl, (hcl)->sp, v_obj)

#define HCL_STACK_POP(hcl) ((hcl)->sp = (hcl)->sp - 1)
#define HCL_STACK_POPS(hcl,count) ((hcl)->sp = (hcl)->sp - (count))
#define HCL_STACK_ISEMPTY(hcl) ((hcl)->sp <= -1)

#define HCL_STACK_GETARG(hcl,nargs,idx) HCL_STACK_GET(hcl, (hcl)->sp - ((nargs) - (idx) - 1))
#define HCL_STACK_GETRCV(hcl,nargs) HCL_STACK_GET(hcl, (hcl)->sp - nargs);

/* you can't access arguments and receiver after this macro. 
 * also you must not call this macro more than once */

#define HCL_STACK_SETRET(hcl,nargs,retv) \
	do { \
		HCL_STACK_POPS(hcl, nargs); \
		HCL_STACK_SETTOP(hcl, (retv)); \
	} while(0)

#define HCL_STACK_SETRETTOERRNUM(hcl,nargs) HCL_STACK_SETRET(hcl, nargs, HCL_ERROR_TO_OOP(hcl->errnum))
#define HCL_STACK_SETRETTOERROR(hcl,nargs,ec) HCL_STACK_SETRET(hcl, nargs, HCL_ERROR_TO_OOP(ec))


/* =========================================================================
 * HCL VM LOGGING
 * ========================================================================= */

enum hcl_log_mask_t
{
	HCL_LOG_DEBUG       = (1u << 0),
	HCL_LOG_INFO        = (1u << 1),
	HCL_LOG_WARN        = (1u << 2),
	HCL_LOG_ERROR       = (1u << 3),
	HCL_LOG_FATAL       = (1u << 4),

	HCL_LOG_UNTYPED     = (1u << 6), /* only to be used by HCL_DEBUGx() and HCL_INFOx() */
	HCL_LOG_COMPILER    = (1u << 7),
	HCL_LOG_VM          = (1u << 8),
	HCL_LOG_MNEMONIC    = (1u << 9), /* bytecode mnemonic */
	HCL_LOG_GC          = (1u << 10),
	HCL_LOG_IC          = (1u << 11), /* instruction cycle, fetch-decode-execute */
	HCL_LOG_PRIMITIVE   = (1u << 12),

	HCL_LOG_APP         = (1u << 13), /* hcl applications, set by hcl logging primitive */
	HCL_LOG_APP_X1      = (1u << 14), /* more hcl applications, you can choose to use one of APP_X? randomly */
	HCL_LOG_APP_X2      = (1u << 15),
	HCL_LOG_APP_X3      = (1u << 16),

	HCL_LOG_ALL_LEVELS  = (HCL_LOG_DEBUG  | HCL_LOG_INFO | HCL_LOG_WARN | HCL_LOG_ERROR | HCL_LOG_FATAL),
	HCL_LOG_ALL_TYPES   = (HCL_LOG_UNTYPED | HCL_LOG_COMPILER | HCL_LOG_VM | HCL_LOG_MNEMONIC | HCL_LOG_GC | HCL_LOG_IC | HCL_LOG_PRIMITIVE | HCL_LOG_APP | HCL_LOG_APP_X1 | HCL_LOG_APP_X2 | HCL_LOG_APP_X3),


	HCL_LOG_STDOUT      = (1u << 20),  /* write log messages to stdout without timestamp. HCL_LOG_STDOUT wins over HCL_LOG_STDERR. */
	HCL_LOG_STDERR      = (1u << 21),  /* write log messages to stderr without timestamp. */

	HCL_LOG_PREFER_JSON = (1u << 30)   /* write a object in the json format. don't set this explicitly. use %J instead */
};
typedef enum hcl_log_mask_t hcl_log_mask_t;

/* all bits must be set to get enabled */
#define HCL_LOG_ENABLED(hcl,mask) (((hcl)->option.log_mask & (mask)) == (mask))

#define HCL_LOG0(hcl,mask,fmt) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt); } while(0)
#define HCL_LOG1(hcl,mask,fmt,a1) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1); } while(0)
#define HCL_LOG2(hcl,mask,fmt,a1,a2) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2); } while(0)
#define HCL_LOG3(hcl,mask,fmt,a1,a2,a3) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3); } while(0)
#define HCL_LOG4(hcl,mask,fmt,a1,a2,a3,a4) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3, a4); } while(0)
#define HCL_LOG5(hcl,mask,fmt,a1,a2,a3,a4,a5) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3, a4, a5); } while(0)
#define HCL_LOG6(hcl,mask,fmt,a1,a2,a3,a4,a5,a6) do { if (HCL_LOG_ENABLED(hcl,mask)) hcl_logbfmt(hcl, mask, fmt, a1, a2, a3, a4, a5, a6); } while(0)

#if defined(HCL_BUILD_RELEASE)
	/* [NOTE]
	 *  get rid of debugging message totally regardless of
	 *  the log mask in the release build.
	 */
#	define HCL_DEBUG0(hcl,fmt)
#	define HCL_DEBUG1(hcl,fmt,a1)
#	define HCL_DEBUG2(hcl,fmt,a1,a2)
#	define HCL_DEBUG3(hcl,fmt,a1,a2,a3)
#	define HCL_DEBUG4(hcl,fmt,a1,a2,a3,a4)
#	define HCL_DEBUG5(hcl,fmt,a1,a2,a3,a4,a5)
#	define HCL_DEBUG6(hcl,fmt,a1,a2,a3,a4,a5,a6)
#else
#	define HCL_DEBUG0(hcl,fmt) HCL_LOG0(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt)
#	define HCL_DEBUG1(hcl,fmt,a1) HCL_LOG1(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt, a1)
#	define HCL_DEBUG2(hcl,fmt,a1,a2) HCL_LOG2(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt, a1, a2)
#	define HCL_DEBUG3(hcl,fmt,a1,a2,a3) HCL_LOG3(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt, a1, a2, a3)
#	define HCL_DEBUG4(hcl,fmt,a1,a2,a3,a4) HCL_LOG4(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt, a1, a2, a3, a4)
#	define HCL_DEBUG5(hcl,fmt,a1,a2,a3,a4,a5) HCL_LOG5(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt, a1, a2, a3, a4, a5)
#	define HCL_DEBUG6(hcl,fmt,a1,a2,a3,a4,a5,a6) HCL_LOG6(hcl, HCL_LOG_DEBUG | HCL_LOG_UNTYPED, fmt, a1, a2, a3, a4, a5, a6)
#endif

#define HCL_INFO0(hcl,fmt) HCL_LOG0(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt)
#define HCL_INFO1(hcl,fmt,a1) HCL_LOG1(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt, a1)
#define HCL_INFO2(hcl,fmt,a1,a2) HCL_LOG2(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt, a1, a2)
#define HCL_INFO3(hcl,fmt,a1,a2,a3) HCL_LOG3(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt, a1, a2, a3)
#define HCL_INFO4(hcl,fmt,a1,a2,a3,a4) HCL_LOG4(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt, a1, a2, a3, a4)
#define HCL_INFO5(hcl,fmt,a1,a2,a3,a4,a5) HCL_LOG5(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt, a1, a2, a3, a4, a5)
#define HCL_INFO6(hcl,fmt,a1,a2,a3,a4,a5,a6) HCL_LOG6(hcl, HCL_LOG_INFO | HCL_LOG_UNTYPED, fmt, a1, a2, a3, a4, a5, a6)


/* =========================================================================
 * HCL ASSERTION
 * ========================================================================= */
#if defined(HCL_BUILD_RELEASE)
#	define HCL_ASSERT(hcl,expr) ((void)0)
#else
#	define HCL_ASSERT(hcl,expr) ((void)((expr) || (hcl_assertfailed (hcl, #expr, __FILE__, __LINE__), 0)))
#endif

/* =========================================================================
 * HCL COMMON OBJECTS
 * ========================================================================= */
enum hcl_brand_t
{
	HCL_BRAND_SMOOI = 1, /* never used as a small integer is encoded in an object pointer */
	HCL_BRAND_SMPTR,
	HCL_BRAND_ERROR,
	HCL_BRAND_CHARACTER,

	HCL_BRAND_NIL,
	HCL_BRAND_TRUE,
	HCL_BRAND_FALSE,

	HCL_BRAND_PBIGINT, /* positive big integer */
	HCL_BRAND_NBIGINT, /* negative big integer */
	HCL_BRAND_CONS,
	HCL_BRAND_ARRAY,
	HCL_BRAND_BYTE_ARRAY,
	HCL_BRAND_SYMBOL_ARRAY, /* special. internal use only */
	HCL_BRAND_SYMBOL,
	HCL_BRAND_STRING,
	HCL_BRAND_DIC,
	HCL_BRAND_FPDEC, /* fixed-point decimal */

	HCL_BRAND_CFRAME,/* compiler frame */
	HCL_BRAND_PRIM,

	HCL_BRAND_CONTEXT,
	HCL_BRAND_PROCESS,
	HCL_BRAND_PROCESS_SCHEDULER,
	HCL_BRAND_SEMAPHORE
};
typedef enum hcl_brand_t hcl_brand_t;

enum hcl_syncode_t
{
	/* SYNCODE 0 means it's not a syncode object. so it begins with 1 */
	HCL_SYNCODE_AND = 1,
	HCL_SYNCODE_BREAK,
	HCL_SYNCODE_DEFUN,
	HCL_SYNCODE_DO,
	HCL_SYNCODE_ELIF,
	HCL_SYNCODE_ELSE,
	HCL_SYNCODE_IF,
	HCL_SYNCODE_LAMBDA,
	HCL_SYNCODE_OR,
	HCL_SYNCODE_RETURN,
	HCL_SYNCODE_SET,
	HCL_SYNCODE_UNTIL,
	HCL_SYNCODE_WHILE
};
typedef enum hcl_syncode_t hcl_syncode_t;

enum hcl_concode_t
{
	/* these can be set in the SYNCODE flags for cons cells */
	HCL_CONCODE_XLIST = 0,  /* () - executable list */
	HCL_CONCODE_ARRAY,      /* #() */
	HCL_CONCODE_BYTEARRAY,  /* #[] */
	HCL_CONCODE_DIC,        /* #{} */
	HCL_CONCODE_QLIST       /* [] - data list */
};
typedef enum hcl_concode_t hcl_concode_t;

#define HCL_IS_NIL(hcl,v) (v == (hcl)->_nil)
#define HCL_IS_TRUE(hcl,v) (v == (hcl)->_true)
#define HCL_IS_FALSE(hcl,v) (v == (hcl)->_false)
#define HCL_IS_SYMBOL(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_SYMBOL)
#define HCL_IS_SYMBOL_ARRAY(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_SYMBOL_ARRAY)
#define HCL_IS_CONTEXT(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_CONTEXT)
#define HCL_IS_PROCESS(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_PROCESS)
#define HCL_IS_CONS(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_CONS)
#define HCL_IS_CONS_XLIST(hcl,v) (HCL_IS_CONS(hcl,v) && HCL_OBJ_GET_FLAGS_SYNCODE(v) == HCL_CONCODE_XLIST)
#define HCL_IS_ARRAY(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_ARRAY)
#define HCL_IS_BYTEARRAY(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_BYTE_ARRAY)
#define HCL_IS_DIC(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_DIC)
#define HCL_IS_PRIM(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_PRIM)
#define HCL_IS_PBIGINT(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_PBIGINT)
#define HCL_IS_NBIGINT(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_NBIGINT)
#define HCL_IS_BIGINT(hcl,v) (HCL_OOP_IS_POINTER(v) && (HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_PBIGINT || HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_NBIGINT))
#define HCL_IS_STRING(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_STRING)
#define HCL_IS_FPDEC(hcl,v) (HCL_OOP_IS_POINTER(v) && HCL_OBJ_GET_FLAGS_BRAND(v) == HCL_BRAND_FPDEC)

#define HCL_CONS_CAR(v)  (((hcl_cons_t*)(v))->car)
#define HCL_CONS_CDR(v)  (((hcl_cons_t*)(v))->cdr)

typedef int (*hcl_dic_walker_t) (
	hcl_t*          hcl,
	hcl_oop_dic_t   dic,
	hcl_oop_cons_t  pair,
	void*           ctx
);

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
	hcl_t*              hcl,
	hcl_mmgr_t*         mmgr,
	hcl_oow_t           heapsize,
	const hcl_vmprim_t* vmprim
);

HCL_EXPORT void hcl_fini (
	hcl_t*              hcl
);

/**
 * The hcl_reset() function some internal states back to the initial state.
 * The affected internal states include byte code buffer, literal frame,
 * ordinary global variables. You should take extra precaution as it is
 * a risky function. For instance, a global variable inserted manually
 * with hcl_putatsysdic() gets deleted if the kernel bit is not set on
 * the variable symbol.
 */
HCL_EXPORT void hcl_reset (
	hcl_t*              hcl
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE hcl_mmgr_t* hcl_getmmgr (hcl_t* hcl) { return hcl->mmgr; }
	static HCL_INLINE void* hcl_getxtn (hcl_t* hcl) { return (void*)(hcl + 1); }

	static HCL_INLINE hcl_cmgr_t* hcl_getcmgr (hcl_t* hcl) { return hcl->cmgr; }
	static HCL_INLINE void hcl_setcmgr (hcl_t* hcl, hcl_cmgr_t* cmgr) { hcl->cmgr = cmgr; }

	static HCL_INLINE hcl_errnum_t hcl_geterrnum (hcl_t* hcl) { return hcl->errnum; }
	
#else
#	define hcl_getmmgr(hcl) ((hcl)->mmgr)
#	define hcl_getxtn(hcl) ((void*)((hcl) + 1))

#	define hcl_getcmgr(hcl) ((hcl)->cmgr)
#	define hcl_setcmgr(hcl,mgr) ((hcl)->cmgr = (mgr))

#	define hcl_geterrnum(hcl) ((hcl)->errnum)
#endif

HCL_EXPORT void hcl_seterrnum (
	hcl_t*       hcl, 
	hcl_errnum_t errnum
);

HCL_EXPORT void hcl_seterrwithsyserr (
	hcl_t*       hcl,
	int          syserr
);

HCL_EXPORT void hcl_seterrbfmt (
	hcl_t*           hcl,
	hcl_errnum_t     errnum,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_seterrufmt (
	hcl_t*           hcl,
	hcl_errnum_t     errnum,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void hcl_seterrbfmtv (
	hcl_t*           hcl,
	hcl_errnum_t     errnum,
	const hcl_bch_t* fmt,
	va_list          ap
);

HCL_EXPORT void hcl_seterrufmtv (
	hcl_t*           hcl,
	hcl_errnum_t     errnum,
	const hcl_uch_t* fmt,
	va_list          ap
);

HCL_EXPORT const hcl_ooch_t* hcl_geterrstr (
	hcl_t* hcl
);

HCL_EXPORT const hcl_ooch_t* hcl_geterrmsg (
	hcl_t* hcl
);

HCL_EXPORT const hcl_ooch_t* hcl_backuperrmsg (
	hcl_t* hcl
);

HCL_EXPORT const hcl_ooch_t* hcl_errnum_to_errstr (
	hcl_errnum_t errnum
);

HCL_EXPORT hcl_errnum_t hcl_syserr_to_errnum (
	int syserr
);

/**
 * The hcl_getoption() function gets the value of an option
 * specified by \a id into the buffer pointed to by \a value.
 *
 * \return 0 on success, -1 on failure
 */
HCL_EXPORT int hcl_getoption (
	hcl_t*         hcl,
	hcl_option_t   id,
	void*          value
);

/**
 * The hcl_setoption() function sets the value of an option 
 * specified by \a id to the value pointed to by \a value.
 *
 * \return 0 on success, -1 on failure
 */
HCL_EXPORT int hcl_setoption (
	hcl_t*        hcl,
	hcl_option_t  id,
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


/**
 * The hcl_moveoop() function is used to move a live object to a new
 * location in hcl_gc(). When hcl_gc() invokes registered gc callbacks,
 * you may call this function to protect extra objects you might have
 * allocated manually.
 */
hcl_oop_t hcl_moveoop (
	hcl_t*     hcl,
	hcl_oop_t  oop
);


HCL_EXPORT hcl_oow_t hcl_getpayloadbytes (
	hcl_t*    hcl,
	hcl_oop_t oop
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

HCL_EXPORT int hcl_addbuiltinprims (
	hcl_t*         hcl
);

/**
 * The hcl_execute() function executes an activated context.
 */
HCL_EXPORT hcl_oop_t hcl_execute (
	hcl_t* hcl
);

HCL_EXPORT hcl_oop_t hcl_executefromip (
	hcl_t*    hcl,
	hcl_oow_t initial_ip
);

HCL_EXPORT void hcl_abort (
	hcl_t* hcl
);


#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE void hcl_switchprocess (hcl_t* hcl) { hcl->switch_proc = 1; }
#else
#	define hcl_switchprocess(hcl) ((hcl)->switch_proc = 1)
#endif


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

HCL_EXPORT hcl_ooi_t hcl_proutbfmt (
	hcl_t*           hcl,
	int              mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT hcl_ooi_t hcl_proutufmt (
	hcl_t*           hcl,
	int              mask,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT int hcl_compile (
	hcl_t*       hcl,
	hcl_oop_t    obj
);

/**
 * The hcl_decode() function decodes instructions from the position 
 * \a start to the position \a end - 1, and prints the decoded instructions
 * in the textual form. 
 */
HCL_EXPORT int hcl_decode (
	hcl_t*       hcl,
	hcl_oow_t    start,
	hcl_oow_t    end
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE hcl_oow_t hcl_getbclen (hcl_t* hcl) { return hcl->code.bc.len; }
	static HCL_INLINE hcl_oow_t hcl_getlflen (hcl_t* hcl) { return hcl->code.lit.len; }
	static HCL_INLINE hcl_ooi_t hcl_getip (hcl_t* hcl) { return hcl->ip; }
#else
#	define hcl_getbclen(hcl) ((hcl)->code.bc.len)
#	define hcl_getlflen(hcl) ((hcl)->code.lit.len)
#	define hcl_getip(hcl) ((hcl)->ip)
#endif


/* if you should read charcters from the input stream before hcl_read(), 
 * you can call hcl_readchar() */
HCL_EXPORT hcl_iolxc_t* hcl_readchar (
	hcl_t* hcl
);

/* If you use hcl_readchar() to read the input stream, you may use 
 * hcl_unreadchar() to put back characters read for hcl_readchar() 
 * to return before reading the stream again. */
HCL_EXPORT int hcl_unreadchar (
	hcl_t*             hcl,
	const hcl_iolxc_t* c
);

/* =========================================================================
 * SYNTAX ERROR HANDLING
 * ========================================================================= */
HCL_EXPORT void hcl_getsynerr (
	hcl_t*             hcl,
	hcl_synerr_t*      synerr
);


HCL_EXPORT void hcl_setsynerrbfmt (
	hcl_t*              hcl,
	hcl_synerrnum_t     num,
	const hcl_ioloc_t*  loc,
	const hcl_oocs_t*   tgt,
	const hcl_bch_t*    msgfmt,
	...
);

HCL_EXPORT void hcl_setsynerrufmt (
	hcl_t*              hcl,
	hcl_synerrnum_t     num,
	const hcl_ioloc_t*  loc,
	const hcl_oocs_t*   tgt,
	const hcl_uch_t*    msgfmt,
	...
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE void hcl_setsynerr (hcl_t* hcl, hcl_synerrnum_t num, const hcl_ioloc_t* loc, const hcl_oocs_t* tgt)
	{
		hcl_setsynerrbfmt (hcl, num, loc, tgt, HCL_NULL);
	}
#else
#	define hcl_setsynerr(hcl,num,loc,tgt) hcl_setsynerrbfmt(hcl,num,loc,tgt,HCL_NULL)
#endif

/* =========================================================================
 * TEMPORARY OOP MANAGEMENT FUNCTIONS
 * ========================================================================= */
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

/* =========================================================================
 * SYSTEM MEMORY MANAGEMENT FUCNTIONS VIA MMGR
 * ========================================================================= */
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


/* =========================================================================
 * PRIMITIVE FUNCTION MANIPULATION
 * ========================================================================= */
HCL_EXPORT hcl_pfbase_t* hcl_findpfbase (
	hcl_t*              hcl,
	hcl_pfinfo_t*       pfinfo,
	hcl_oow_t           pfcount,
	const hcl_ooch_t*   name,
	hcl_oow_t           namelen
);


/* =========================================================================
 * LOGGING
 * ========================================================================= */

HCL_EXPORT hcl_ooi_t hcl_logbfmt (
	hcl_t*           hcl,
	int              mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT hcl_ooi_t hcl_logbfmtv (
	hcl_t*           hcl,
	int              mask,
	const hcl_bch_t* fmt,
	va_list          ap
);

HCL_EXPORT hcl_ooi_t hcl_logufmt (
	hcl_t*           hcl,
	int              mask,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT hcl_ooi_t hcl_logufmtv (
	hcl_t*           hcl,
	int              mask,
	const hcl_uch_t* fmt,
	va_list          ap
);

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_logoofmt hcl_logufmt
#	define hcl_logoofmtv hcl_logufmtv
#else
#	define hcl_logoofmt hcl_logbfmt
#	define hcl_logoofmtv hcl_logbfmtv
#endif


/* =========================================================================
 * OBJECT MANAGEMENT
 * ========================================================================= */
HCL_EXPORT hcl_oop_t hcl_makenil (
	hcl_t*     hcl
);

HCL_EXPORT hcl_oop_t hcl_maketrue (
	hcl_t*     hcl
);

HCL_EXPORT hcl_oop_t hcl_makefalse (
	hcl_t*     hcl
);


HCL_EXPORT hcl_oop_t hcl_makecons (
	hcl_t*     hcl,
	hcl_oop_t  car,
	hcl_oop_t  cdr
);

HCL_EXPORT hcl_oop_t hcl_makearray (
	hcl_t*     hcl,
	hcl_oow_t  size,
	int        ngc
);

HCL_EXPORT hcl_oop_t hcl_makebytearray (
	hcl_t*           hcl,
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oop_t hcl_makestring (
	hcl_t*            hcl,
	const hcl_ooch_t* ptr,
	hcl_oow_t         len,
	int               ngc
);

HCL_EXPORT hcl_oop_t hcl_makefpdec (
	hcl_t*            hcl,
	hcl_oop_t         value,
	hcl_ooi_t         scale
);

HCL_EXPORT hcl_oop_t hcl_makedic (
	hcl_t*            hcl, 
	hcl_oow_t         inisize /* initial bucket size */
);

HCL_EXPORT hcl_oop_t hcl_makeprocess (
	hcl_t*            hcl, 
	hcl_oow_t         stksize
);

HCL_EXPORT hcl_oop_t hcl_makecontext (
	hcl_t*            hcl,
	hcl_ooi_t         ntmprs
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

HCL_EXPORT hcl_oop_t hcl_makeprim (
	hcl_t*          hcl,
	hcl_pfimpl_t    primimpl,
	hcl_oow_t       minargs,
	hcl_oow_t       maxargs,
	hcl_mod_t*      mod
);


HCL_EXPORT hcl_oop_t hcl_makebigint (
	hcl_t*           hcl, 
	int              brand,
	const hcl_liw_t* ptr,
	hcl_oow_t        len
);

HCL_EXPORT int hcl_inttooow (
	hcl_t*     hcl,
	hcl_oop_t  x,
	hcl_oow_t* w
);

HCL_EXPORT hcl_oop_t hcl_oowtoint (
	hcl_t*     hcl,
	hcl_oow_t  w
);


HCL_EXPORT int hcl_inttoooi (
	hcl_t*     hcl,
	hcl_oop_t  x,
	hcl_ooi_t* i
);

HCL_EXPORT hcl_oop_t hcl_ooitoint (
	hcl_t*    hcl,
	hcl_ooi_t i
);

/* =========================================================================
 * CONS OBJECT UTILITIES
 * ========================================================================= */
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

/* =========================================================================
 * DICTIONARY ACCESS FUNCTIONS
 * ========================================================================= */
HCL_EXPORT hcl_oop_cons_t hcl_putatsysdic (
	hcl_t*     hcl,
	hcl_oop_t  key,
	hcl_oop_t  value
);

HCL_EXPORT hcl_oop_cons_t hcl_getatsysdic (
	hcl_t*     hcl,
	hcl_oop_t  key
);

HCL_EXPORT int hcl_zapatsysdic (
	hcl_t*     hcl,
	hcl_oop_t  key
);

HCL_EXPORT hcl_oop_cons_t hcl_putatdic (
	hcl_t*        hcl,
	hcl_oop_dic_t dic,
	hcl_oop_t     key,
	hcl_oop_t     value
);

HCL_EXPORT hcl_oop_cons_t hcl_getatdic (
	hcl_t*        hcl,
	hcl_oop_dic_t dic,
	hcl_oop_t     key
);


HCL_EXPORT int hcl_zapatdic (
	hcl_t*        hcl,
	hcl_oop_dic_t dic,
	hcl_oop_t     key
);

HCL_EXPORT int hcl_walkdic (
	hcl_t*           hcl,
	hcl_oop_dic_t    dic,
	hcl_dic_walker_t walker,
	void*            ctx
);



/* =========================================================================
 * OBJECT HASHING AND COMPARISION
 * ========================================================================= */

HCL_EXPORT int hcl_hashobj (
	hcl_t*     hcl, 
	hcl_oop_t  obj, 
	hcl_oow_t* xhv
);

HCL_EXPORT int hcl_equalobjs (
	hcl_t*     hcl,
	hcl_oop_t  rcv,
	hcl_oop_t  arg
);


/* =========================================================================
 * STRING ENCODING CONVERSION
 * ========================================================================= */

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_convootobchars(hcl,oocs,oocslen,bcs,bcslen) hcl_convutobchars(hcl,oocs,oocslen,bcs,bcslen)
#	define hcl_convbtooochars(hcl,bcs,bcslen,oocs,oocslen) hcl_convbtouchars(hcl,bcs,bcslen,oocs,oocslen)
#	define hcl_convootobcstr(hcl,oocs,oocslen,bcs,bcslen) hcl_convutobcstr(hcl,oocs,oocslen,bcs,bcslen)
#	define hcl_convbtooocstr(hcl,bcs,bcslen,oocs,oocslen) hcl_convbtoucstr(hcl,bcs,bcslen,oocs,oocslen)
#else
#	define hcl_convootouchars(hcl,oocs,oocslen,ucs,ucslen) hcl_convbtouchars(hcl,oocs,oocslen,ucs,ucslen)
#	define hcl_convutooochars(hcl,ucs,ucslen,oocs,oocslen) hcl_convutobchars(hcl,ucs,ucslen,oocs,oocslen)
#	define hcl_convootoucstr(hcl,oocs,oocslen,ucs,ucslen) hcl_convbtoucstr(hcl,oocs,oocslen,ucs,ucslen)
#	define hcl_convutooocstr(hcl,ucs,ucslen,oocs,oocslen) hcl_convutobcstr(hcl,ucs,ucslen,oocs,oocslen)
#endif

HCL_EXPORT int hcl_convbtouchars (
	hcl_t*           hcl,
	const hcl_bch_t* bcs,
	hcl_oow_t*       bcslen,
	hcl_uch_t*       ucs,
	hcl_oow_t*       ucslen
);

HCL_EXPORT int hcl_convutobchars (
	hcl_t*           hcl,
	const hcl_uch_t* ucs,
	hcl_oow_t*       ucslen,
	hcl_bch_t*       bcs,
	hcl_oow_t*       bcslen
);


/**
 * The hcl_convbtoucstr() function converts a null-terminated byte string 
 * to a wide string.
 */
HCL_EXPORT int hcl_convbtoucstr (
	hcl_t*           hcl,
	const hcl_bch_t* bcs,
	hcl_oow_t*       bcslen,
	hcl_uch_t*       ucs,
	hcl_oow_t*       ucslen
);


/**
 * The hcl_convutobcstr() function converts a null-terminated wide string
 * to a byte string.
 */
HCL_EXPORT int hcl_convutobcstr (
	hcl_t*           hcl,
	const hcl_uch_t* ucs,
	hcl_oow_t*       ucslen,
	hcl_bch_t*       bcs,
	hcl_oow_t*       bcslen
);

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_dupootobcharswithheadroom(hcl,hrb,oocs,oocslen,bcslen) hcl_duputobcharswithheadroom(hcl,hrb,oocs,oocslen,bcslen)
#	define hcl_dupbtooocharswithheadroom(hcl,hrb,bcs,bcslen,oocslen) hcl_dupbtoucharswithheadroom(hcl,hrb,bcs,bcslen,oocslen)
#	define hcl_dupootobchars(hcl,oocs,oocslen,bcslen) hcl_duputobchars(hcl,oocs,oocslen,bcslen)
#	define hcl_dupbtooochars(hcl,bcs,bcslen,oocslen) hcl_dupbtouchars(hcl,bcs,bcslen,oocslen)

#	define hcl_dupootobcstrwithheadroom(hcl,hrb,oocs,bcslen) hcl_duputobcstrwithheadroom(hcl,hrb,oocs,bcslen)
#	define hcl_dupbtooocstrwithheadroom(hcl,hrb,bcs,oocslen) hcl_dupbtoucstrwithheadroom(hcl,hrb,bcs,oocslen)
#	define hcl_dupootobcstr(hcl,oocs,bcslen) hcl_duputobcstr(hcl,oocs,bcslen)
#	define hcl_dupbtooocstr(hcl,bcs,oocslen) hcl_dupbtoucstr(hcl,bcs,oocslen)
#else
#	define hcl_dupootoucharswithheadroom(hcl,hrb,oocs,oocslen,ucslen) hcl_dupbtoucharswithheadroom(hcl,hrb,oocs,oocslen,ucslen)
#	define hcl_duputooocharswithheadroom(hcl,hrb,ucs,ucslen,oocslen) hcl_duputobcharswithheadroom(hcl,hrb,ucs,ucslen,oocslen)
#	define hcl_dupootouchars(hcl,oocs,oocslen,ucslen) hcl_dupbtouchars(hcl,oocs,oocslen,ucslen)
#	define hcl_duputooochars(hcl,ucs,ucslen,oocslen) hcl_duputobchars(hcl,ucs,ucslen,oocslen)

#	define hcl_dupootoucstrwithheadroom(hcl,hrb,oocs,ucslen) hcl_dupbtoucstrwithheadroom(hcl,hrb,oocs,ucslen)
#	define hcl_duputooocstrwithheadroom(hcl,hrb,ucs,oocslen) hcl_duputobcstrwithheadroom(hcl,hrb,ucs,oocslen)
#	define hcl_dupootoucstr(hcl,oocs,ucslen) hcl_dupbtoucstr(hcl,oocs,ucslen)
#	define hcl_duputooocstr(hcl,ucs,oocslen) hcl_duputobcstr(hcl,ucs,oocslen)
#endif


HCL_EXPORT hcl_uch_t* hcl_dupbtoucharswithheadroom (
	hcl_t*           hcl,
	hcl_oow_t        headroom_bytes,
	const hcl_bch_t* bcs,
	hcl_oow_t        bcslen,
	hcl_oow_t*       ucslen
);

HCL_EXPORT hcl_bch_t* hcl_duputobcharswithheadroom (
	hcl_t*           hcl,
	hcl_oow_t        headroom_bytes,
	const hcl_uch_t* ucs,
	hcl_oow_t        ucslen,
	hcl_oow_t*       bcslen
);

HCL_EXPORT hcl_uch_t* hcl_dupbtouchars (
	hcl_t*           hcl,
	const hcl_bch_t* bcs,
	hcl_oow_t        bcslen,
	hcl_oow_t*       ucslen
);

HCL_EXPORT hcl_bch_t* hcl_duputobchars (
	hcl_t*           hcl,
	const hcl_uch_t* ucs,
	hcl_oow_t        ucslen,
	hcl_oow_t*       bcslen
);


HCL_EXPORT hcl_uch_t* hcl_dupbtoucstrwithheadroom (
	hcl_t*           hcl,
	hcl_oow_t        headroom_bytes,
	const hcl_bch_t* bcs,
	hcl_oow_t*       ucslen
);

HCL_EXPORT hcl_bch_t* hcl_duputobcstrwithheadroom (
	hcl_t*           hcl,
	hcl_oow_t        headroom_bytes,
	const hcl_uch_t* ucs,
	hcl_oow_t* bcslen
);

HCL_EXPORT hcl_uch_t* hcl_dupbtoucstr (
	hcl_t*           hcl,
	const hcl_bch_t* bcs,
	hcl_oow_t*       ucslen /* optional: length of returned string */
);

HCL_EXPORT hcl_bch_t* hcl_duputobcstr (
	hcl_t*           hcl,
	const hcl_uch_t* ucs,
	hcl_oow_t*       bcslen /* optional: length of returned string */
);


#if defined(HCL_OOCH_IS_UCH)
#	define hcl_dupoochars(hcl,oocs,oocslen) hcl_dupuchars(hcl,oocs,oocslen)
#else
#	define hcl_dupoochars(hcl,oocs,oocslen) hcl_dupbchars(hcl,oocs,oocslen)
#endif

HCL_EXPORT hcl_uch_t* hcl_dupuchars (
	hcl_t*           hcl,
	const hcl_uch_t* ucs,
	hcl_oow_t        ucslen
);

HCL_EXPORT hcl_bch_t* hcl_dupbchars (
	hcl_t*           hcl,
	const hcl_bch_t* bcs,
	hcl_oow_t        bcslen
);

/* =========================================================================
 * ASSERTION SUPPORT
 * ========================================================================= */
HCL_EXPORT void hcl_assertfailed (
	hcl_t*           hcl,
	const hcl_bch_t* expr,
	const hcl_bch_t* file,
	hcl_oow_t        line
);

#if defined(__cplusplus)
}
#endif


#endif
