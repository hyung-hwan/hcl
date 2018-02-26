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

#ifndef _HCL_RBT_H_
#define _HCL_RBT_H_

#include "hcl-cmn.h"

/** \file
 * This file provides a red-black tree encapsulated in the #hcl_rbt_t type that
 * implements a self-balancing binary search tree.Its interface is very close 
 * to #hcl_htb_t.
 *
 * This sample code adds a series of keys and values and print them
 * in descending key order.
 * \code
 * #include <hcl/cmn/rbt.h>
 * #include <hcl/cmn/mem.h>
 * #include <hcl/cmn/sio.h>
 * 
 * static hcl_rbt_walk_t walk (hcl_rbt_t* rbt, hcl_rbt_pair_t* pair, void* ctx)
 * {
 *   hcl_printf (HCL_T("key = %d, value = %d\n"),
 *     *(int*)HCL_RBT_KPTR(pair), *(int*)HCL_RBT_VPTR(pair));
 *   return HCL_RBT_WALK_FORWARD;
 * }
 * 
 * int main ()
 * {
 *   hcl_rbt_t* s1;
 *   int i;
 * 
 *   s1 = hcl_rbt_open (HCL_MMGR_GETDFL(), 0, 1, 1); // error handling skipped
 *   hcl_rbt_setstyle (s1, hcl_getrbtstyle(HCL_RBT_STYLE_INLINE_COPIERS));
 * 
 *   for (i = 0; i < 20; i++)
 *   {
 *     int x = i * 20;
 *     hcl_rbt_insert (s1, &i, HCL_SIZEOF(i), &x, HCL_SIZEOF(x)); // eror handling skipped
 *   }
 * 
 *   hcl_rbt_rwalk (s1, walk, HCL_NULL);
 * 
 *   hcl_rbt_close (s1);
 *   return 0;
 * }
 * \endcode
 */

typedef struct hcl_rbt_t hcl_rbt_t;
typedef struct hcl_rbt_pair_t hcl_rbt_pair_t;

/** 
 * The hcl_rbt_walk_t type defines values that the callback function can
 * return to control hcl_rbt_walk() and hcl_rbt_rwalk().
 */
enum hcl_rbt_walk_t
{
	HCL_RBT_WALK_STOP    = 0,
	HCL_RBT_WALK_FORWARD = 1
};
typedef enum hcl_rbt_walk_t hcl_rbt_walk_t;

/**
 * The hcl_rbt_id_t type defines IDs to indicate a key or a value in various
 * functions
 */
enum hcl_rbt_id_t
{
	HCL_RBT_KEY = 0, /**< indicate a key */
	HCL_RBT_VAL = 1  /**< indicate a value */
};
typedef enum hcl_rbt_id_t hcl_rbt_id_t;

/**
 * The hcl_rbt_copier_t type defines a pair contruction callback.
 */
typedef void* (*hcl_rbt_copier_t) (
	hcl_rbt_t* rbt  /**< red-black tree */,
	void*      dptr /**< pointer to a key or a value */, 
	hcl_oow_t  dlen /**< length of a key or a value */
);

/**
 * The hcl_rbt_freeer_t defines a key/value destruction callback.
 */
typedef void (*hcl_rbt_freeer_t) (
	hcl_rbt_t* rbt,  /**< red-black tree */
	void*      dptr, /**< pointer to a key or a value */
	hcl_oow_t  dlen  /**< length of a key or a value */
);

/**
 * The hcl_rbt_comper_t type defines a key comparator that is called when
 * the rbt needs to compare keys. A red-black tree is created with a default
 * comparator which performs bitwise comparison of two keys.
 * The comparator should return 0 if the keys are the same, 1 if the first
 * key is greater than the second key, -1 otherwise.
 */
typedef int (*hcl_rbt_comper_t) (
	const hcl_rbt_t* rbt,    /**< red-black tree */ 
	const void*      kptr1,  /**< key pointer */
	hcl_oow_t        klen1,  /**< key length */ 
	const void*      kptr2,  /**< key pointer */
	hcl_oow_t        klen2   /**< key length */
);

/**
 * The hcl_rbt_keeper_t type defines a value keeper that is called when 
 * a value is retained in the context that it should be destroyed because
 * it is identical to a new value. Two values are identical if their 
 * pointers and lengths are equal.
 */
typedef void (*hcl_rbt_keeper_t) (
	hcl_rbt_t* rbt,    /**< red-black tree */
	void*       vptr,   /**< value pointer */
	hcl_oow_t vlen    /**< value length */
);

/**
 * The hcl_rbt_walker_t defines a pair visitor.
 */
typedef hcl_rbt_walk_t (*hcl_rbt_walker_t) (
	hcl_rbt_t*      rbt,   /**< red-black tree */
	hcl_rbt_pair_t* pair,  /**< pointer to a key/value pair */
	void*            ctx    /**< pointer to user-defined data */
);

/**
 * The hcl_rbt_cbserter_t type defines a callback function for hcl_rbt_cbsert().
 * The hcl_rbt_cbserter() function calls it to allocate a new pair for the 
 * key pointed to by \a kptr of the length \a klen and the callback context
 * \a ctx. The second parameter \a pair is passed the pointer to the existing
 * pair for the key or #HCL_NULL in case of no existing key. The callback
 * must return a pointer to a new or a reallocated pair. When reallocating the
 * existing pair, this callback must destroy the existing pair and return the 
 * newly reallocated pair. It must return #HCL_NULL for failure.
 */
typedef hcl_rbt_pair_t* (*hcl_rbt_cbserter_t) (
	hcl_rbt_t*      rbt,    /**< red-black tree */
	hcl_rbt_pair_t* pair,   /**< pair pointer */
	void*           kptr,   /**< key pointer */
	hcl_oow_t       klen,   /**< key length */
	void*           ctx     /**< callback context */
);


enum hcl_rbt_pair_color_t
{
	HCL_RBT_RED,
	HCL_RBT_BLACK
};
typedef enum hcl_rbt_pair_color_t hcl_rbt_pair_color_t;

/**
 * The hcl_rbt_pair_t type defines red-black tree pair. A pair is composed 
 * of a key and a value. It maintains pointers to the beginning of a key and 
 * a value plus their length. The length is scaled down with the scale factor 
 * specified in an owning tree. Use macros defined in the 
 */
struct hcl_rbt_pair_t
{
	struct
	{
		void*     ptr;
		hcl_oow_t len;
	} key;

	struct
	{
		void*       ptr;
		hcl_oow_t len;
	} val;

	/* management information below */
	hcl_rbt_pair_color_t color;
	hcl_rbt_pair_t* parent;
	hcl_rbt_pair_t* child[2]; /* left and right */
};

typedef struct hcl_rbt_style_t hcl_rbt_style_t;

/**
 * The hcl_rbt_style_t type defines callback function sets for key/value 
 * pair manipulation. 
 */
struct hcl_rbt_style_t
{
	hcl_rbt_copier_t copier[2]; /**< key and value copier */
	hcl_rbt_freeer_t freeer[2]; /**< key and value freeer */
	hcl_rbt_comper_t comper;    /**< key comparator */
	hcl_rbt_keeper_t keeper;    /**< value keeper */
};

/**
 * The hcl_rbt_style_kind_t type defines the type of predefined
 * callback set for pair manipulation.
 */
enum hcl_rbt_style_kind_t
{
	/** store the key and the value pointer */
	HCL_RBT_STYLE_DEFAULT,
	/** copy both key and value into the pair */
	HCL_RBT_STYLE_INLINE_COPIERS,
	/** copy the key into the pair but store the value pointer */
	HCL_RBT_STYLE_INLINE_KEY_COPIER,
	/** copy the value into the pair but store the key pointer */
	HCL_RBT_STYLE_INLINE_VALUE_COPIER
};

typedef enum hcl_rbt_style_kind_t  hcl_rbt_style_kind_t;

/**
 * The hcl_rbt_t type defines a red-black tree.
 */
struct hcl_rbt_t
{
	hcl_t*                 hcl;
	const hcl_rbt_style_t* style;
	hcl_oob_t              scale[2];  /**< length scale */
	hcl_rbt_pair_t         xnil;      /**< internal nil node */
	hcl_oow_t              size;      /**< number of pairs */
	hcl_rbt_pair_t*        root;      /**< root pair */
};

/**
 * The HCL_RBT_COPIER_SIMPLE macros defines a copier that remembers the
 * pointer and length of data in a pair.
 */
#define HCL_RBT_COPIER_SIMPLE ((hcl_rbt_copier_t)1)

/**
 * The HCL_RBT_COPIER_INLINE macros defines a copier that copies data into
 * a pair.
 */
#define HCL_RBT_COPIER_INLINE ((hcl_rbt_copier_t)2)

#define HCL_RBT_COPIER_DEFAULT (HCL_RBT_COPIER_SIMPLE)
#define HCL_RBT_FREEER_DEFAULT (HCL_NULL)
#define HCL_RBT_COMPER_DEFAULT (hcl_rbt_dflcomp)
#define HCL_RBT_KEEPER_DEFAULT (HCL_NULL)

/**
 * The HCL_RBT_SIZE() macro returns the number of pairs in red-black tree.
 */
#define HCL_RBT_SIZE(m)   ((const hcl_oow_t)(m)->size)
#define HCL_RBT_KSCALE(m) ((const int)(m)->scale[HCL_RBT_KEY])
#define HCL_RBT_VSCALE(m) ((const int)(m)->scale[HCL_RBT_VAL])

#define HCL_RBT_KPTL(p) (&(p)->key)
#define HCL_RBT_VPTL(p) (&(p)->val)

#define HCL_RBT_KPTR(p) ((p)->key.ptr)
#define HCL_RBT_KLEN(p) ((p)->key.len)
#define HCL_RBT_VPTR(p) ((p)->val.ptr)
#define HCL_RBT_VLEN(p) ((p)->val.len)

#define HCL_RBT_NEXT(p) ((p)->next)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hcl_getrbtstyle() functions returns a predefined callback set for
 * pair manipulation.
 */
HCL_EXPORT const hcl_rbt_style_t* hcl_getrbtstyle (
	hcl_rbt_style_kind_t kind
);

/**
 * The hcl_rbt_open() function creates a red-black tree.
 * \return hcl_rbt_t pointer on success, HCL_NULL on failure.
 */
HCL_EXPORT hcl_rbt_t* hcl_rbt_open (
	hcl_t*      hcl,
	hcl_oow_t   xtnsize, /**< extension size in bytes */
	int         kscale,  /**< key scale */
	int         vscale   /**< value scale */
);

/**
 * The hcl_rbt_close() function destroys a red-black tree.
 */
HCL_EXPORT void hcl_rbt_close (
	hcl_rbt_t* rbt   /**< red-black tree */
);

/**
 * The hcl_rbt_init() function initializes a red-black tree
 */
HCL_EXPORT int hcl_rbt_init (
	hcl_rbt_t*  rbt,    /**< red-black tree */
	hcl_t*      hcl,
	int         kscale, /**< key scale */
	int         vscale  /**< value scale */
);

/**
 * The hcl_rbt_fini() funtion finalizes a red-black tree
 */
HCL_EXPORT void hcl_rbt_fini (
	hcl_rbt_t* rbt  /**< red-black tree */
);

HCL_EXPORT void* hcl_rbt_getxtn (
	hcl_rbt_t* rbt
);

/**
 * The hcl_rbt_getstyle() function gets manipulation callback function set.
 */
HCL_EXPORT const hcl_rbt_style_t* hcl_rbt_getstyle (
	const hcl_rbt_t* rbt /**< red-black tree */
);

/**
 * The hcl_rbt_setstyle() function sets internal manipulation callback 
 * functions for data construction, destruction, comparison, etc.
 * The callback structure pointed to by \a style must outlive the tree
 * pointed to by \a htb as the tree doesn't copy the contents of the 
 * structure.
 */
HCL_EXPORT void hcl_rbt_setstyle (
	hcl_rbt_t*             rbt,  /**< red-black tree */
	const hcl_rbt_style_t* style /**< callback function set */
);

/**
 * The hcl_rbt_getsize() function gets the number of pairs in red-black tree.
 */
HCL_EXPORT hcl_oow_t hcl_rbt_getsize (
	const hcl_rbt_t* rbt  /**< red-black tree */
);

/**
 * The hcl_rbt_search() function searches red-black tree to find a pair with a 
 * matching key. It returns the pointer to the pair found. If it fails
 * to find one, it returns HCL_NULL.
 * \return pointer to the pair with a maching key, 
 *         or HCL_NULL if no match is found.
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_search (
	const hcl_rbt_t* rbt,   /**< red-black tree */
	const void*      kptr,  /**< key pointer */
	hcl_oow_t        klen   /**< the size of the key */
);

/**
 * The hcl_rbt_upsert() function searches red-black tree for the pair with a 
 * matching key. If one is found, it updates the pair. Otherwise, it inserts
 * a new pair with the key and the value given. It returns the pointer to the 
 * pair updated or inserted.
 * \return a pointer to the updated or inserted pair on success, 
 *         HCL_NULL on failure. 
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_upsert (
	hcl_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hcl_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hcl_oow_t  vlen   /**< value length */
);

/**
 * The hcl_rbt_ensert() function inserts a new pair with the key and the value
 * given. If there exists a pair with the key given, the function returns 
 * the pair containing the key.
 * \return pointer to a pair on success, HCL_NULL on failure. 
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_ensert (
	hcl_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hcl_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hcl_oow_t  vlen   /**< value length */
);

/**
 * The hcl_rbt_insert() function inserts a new pair with the key and the value
 * given. If there exists a pair with the key given, the function returns 
 * HCL_NULL without channging the value.
 * \return pointer to the pair created on success, HCL_NULL on failure. 
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_insert (
	hcl_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hcl_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hcl_oow_t  vlen   /**< value length */
);

/**
 * The hcl_rbt_update() function updates the value of an existing pair
 * with a matching key.
 * \return pointer to the pair on success, HCL_NULL on no matching pair
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_update (
	hcl_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hcl_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hcl_oow_t  vlen   /**< value length */
);

/**
 * The hcl_rbt_cbsert() function inserts a key/value pair by delegating pair 
 * allocation to a callback function. Depending on the callback function,
 * it may behave like hcl_rbt_insert(), hcl_rbt_upsert(), hcl_rbt_update(),
 * hcl_rbt_ensert(), or totally differently. The sample code below inserts
 * a new pair if the key is not found and appends the new value to the
 * existing value delimited by a comma if the key is found.
 *
 * \code
 * hcl_rbt_walk_t print_map_pair (hcl_rbt_t* map, hcl_rbt_pair_t* pair, void* ctx)
 * {
 *   hcl_printf (HCL_T("%.*s[%d] => %.*s[%d]\n"),
 *     (int)HCL_RBT_KLEN(pair), HCL_RBT_KPTR(pair), (int)HCL_RBT_KLEN(pair),
 *     (int)HCL_RBT_VLEN(pair), HCL_RBT_VPTR(pair), (int)HCL_RBT_VLEN(pair));
 *   return HCL_RBT_WALK_FORWARD;
 * }
 * 
 * hcl_rbt_pair_t* cbserter (
 *   hcl_rbt_t* rbt, hcl_rbt_pair_t* pair,
 *   void* kptr, hcl_oow_t klen, void* ctx)
 * {
 *   hcl_cstr_t* v = (hcl_cstr_t*)ctx;
 *   if (pair == HCL_NULL)
 *   {
 *     // no existing key for the key 
 *     return hcl_rbt_allocpair (rbt, kptr, klen, v->ptr, v->len);
 *   }
 *   else
 *   {
 *     // a pair with the key exists. 
 *     // in this sample, i will append the new value to the old value 
 *     // separated by a comma
 *     hcl_rbt_pair_t* new_pair;
 *     hcl_ooch_t comma = HCL_T(',');
 *     hcl_oob_t* vptr;
 * 
 *     // allocate a new pair, but without filling the actual value. 
 *     // note vptr is given HCL_NULL for that purpose 
 *     new_pair = hcl_rbt_allocpair (
 *       rbt, kptr, klen, HCL_NULL, pair->vlen + 1 + v->len); 
 *     if (new_pair == HCL_NULL) return HCL_NULL;
 * 
 *     // fill in the value space 
 *     vptr = new_pair->vptr;
 *     hcl_memcpy (vptr, pair->vptr, pair->vlen*HCL_SIZEOF(hcl_ooch_t));
 *     vptr += pair->vlen*HCL_SIZEOF(hcl_ooch_t);
 *     hcl_memcpy (vptr, &comma, HCL_SIZEOF(hcl_ooch_t));
 *     vptr += HCL_SIZEOF(hcl_ooch_t);
 *     hcl_memcpy (vptr, v->ptr, v->len*HCL_SIZEOF(hcl_ooch_t));
 * 
 *     // this callback requires the old pair to be destroyed 
 *     hcl_rbt_freepair (rbt, pair);
 * 
 *     // return the new pair 
 *     return new_pair;
 *   }
 * }
 * 
 * int main ()
 * {
 *   hcl_rbt_t* s1;
 *   int i;
 *   hcl_ooch_t* keys[] = { HCL_T("one"), HCL_T("two"), HCL_T("three") };
 *   hcl_ooch_t* vals[] = { HCL_T("1"), HCL_T("2"), HCL_T("3"), HCL_T("4"), HCL_T("5") };
 * 
 *   s1 = hcl_rbt_open (
 *     HCL_MMGR_GETDFL(), 0,
 *     HCL_SIZEOF(hcl_ooch_t), HCL_SIZEOF(hcl_ooch_t)
 *   ); // note error check is skipped 
 *   hcl_rbt_setstyle (s1, &style1);
 * 
 *   for (i = 0; i < HCL_COUNTOF(vals); i++)
 *   {
 *     hcl_cstr_t ctx;
 *     ctx.ptr = vals[i]; ctx.len = hcl_strlen(vals[i]);
 *     hcl_rbt_cbsert (s1,
 *       keys[i%HCL_COUNTOF(keys)], hcl_strlen(keys[i%HCL_COUNTOF(keys)]),
 *       cbserter, &ctx
 *     ); // note error check is skipped
 *   }
 *   hcl_rbt_walk (s1, print_map_pair, HCL_NULL);
 * 
 *   hcl_rbt_close (s1);
 *   return 0;
 * }
 * \endcode
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_cbsert (
	hcl_rbt_t*         rbt,      /**< red-black tree */
	void*              kptr,     /**< key pointer */
	hcl_oow_t          klen,     /**< key length */
	hcl_rbt_cbserter_t cbserter, /**< callback function */
	void*              ctx       /**< callback context */
);

/**
 * The hcl_rbt_delete() function deletes a pair with a matching key 
 * \return 0 on success, -1 on failure
 */
HCL_EXPORT int hcl_rbt_delete (
	hcl_rbt_t*  rbt,   /**< red-black tree */
	const void* kptr,  /**< key pointer */
	hcl_oow_t   klen   /**< key size */
);

/**
 * The hcl_rbt_clear() function empties a red-black tree.
 */
HCL_EXPORT void hcl_rbt_clear (
	hcl_rbt_t* rbt /**< red-black tree */
);

/**
 * The hcl_rbt_walk() function traverses a red-black tree in preorder 
 * from the leftmost child.
 */
HCL_EXPORT void hcl_rbt_walk (
	hcl_rbt_t*       rbt,    /**< red-black tree */
	hcl_rbt_walker_t walker, /**< callback function for each pair */
	void*            ctx     /**< pointer to user-specific data */
);

/**
 * The hcl_rbt_walk() function traverses a red-black tree in preorder 
 * from the rightmost child.
 */
HCL_EXPORT void hcl_rbt_rwalk (
	hcl_rbt_t*       rbt,    /**< red-black tree */
	hcl_rbt_walker_t walker, /**< callback function for each pair */
	void*            ctx     /**< pointer to user-specific data */
);

/**
 * The hcl_rbt_allocpair() function allocates a pair for a key and a value 
 * given. But it does not chain the pair allocated into the red-black tree \a rbt.
 * Use this function at your own risk. 
 *
 * Take note of he following special behavior when the copier is 
 * #HCL_RBT_COPIER_INLINE.
 * - If \a kptr is #HCL_NULL, the key space of the size \a klen is reserved but
 *   not propagated with any data.
 * - If \a vptr is #HCL_NULL, the value space of the size \a vlen is reserved
 *   but not propagated with any data.
 */
HCL_EXPORT hcl_rbt_pair_t* hcl_rbt_allocpair (
	hcl_rbt_t*  rbt,
	void*       kptr, 
	hcl_oow_t   klen,
	void*       vptr,
	hcl_oow_t   vlen
);

/**
 * The hcl_rbt_freepair() function destroys a pair. But it does not detach
 * the pair destroyed from the red-black tree \a rbt. Use this function at your
 * own risk.
 */
HCL_EXPORT void hcl_rbt_freepair (
	hcl_rbt_t*      rbt,
	hcl_rbt_pair_t* pair
);

/**
 * The hcl_rbt_dflcomp() function defines the default key comparator.
 */
HCL_EXPORT int hcl_rbt_dflcomp (
	const hcl_rbt_t* rbt,
	const void*      kptr1,
	hcl_oow_t        klen1,
	const void*      kptr2,
	hcl_oow_t        klen2 
);

#if defined(__cplusplus)
}
#endif

#endif
