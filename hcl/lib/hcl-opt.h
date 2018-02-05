/*
 * $Id$
 *
    Copyright (c) 2014-2017 Chung, Hyung-Hwan. All rights reserved.

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

#ifndef _HCL_OPT_H_
#define _HCL_OPT_H_

#include "hcl-cmn.h"

/** \file
 * This file defines functions and data structures to process 
 * command-line arguments. 
 */

typedef struct hcl_uopt_t hcl_uopt_t;
typedef struct hcl_uopt_lng_t hcl_uopt_lng_t;

struct hcl_uopt_lng_t
{
	const hcl_uch_t* str;
	hcl_uci_t        val;
};

struct hcl_uopt_t
{
	/* input */
	const hcl_uch_t* str; /* option string  */
	hcl_uopt_lng_t*  lng; /* long options */

	/* output */
	hcl_uci_t        opt; /* character checked for validity */
	hcl_uch_t*       arg; /* argument associated with an option */

	/* output */
	const hcl_uch_t* lngopt; 

	/* input + output */
	int              ind; /* index into parent argv vector */

	/* input + output - internal*/
	hcl_uch_t*       cur;
};

typedef struct hcl_bopt_t hcl_bopt_t;
typedef struct hcl_bopt_lng_t hcl_bopt_lng_t;

struct hcl_bopt_lng_t
{
	const hcl_bch_t* str;
	hcl_bci_t        val;
};

struct hcl_bopt_t
{
	/* input */
	const hcl_bch_t* str; /* option string  */
	hcl_bopt_lng_t*  lng; /* long options */

	/* output */
	hcl_bci_t        opt; /* character checked for validity */
	hcl_bch_t*       arg; /* argument associated with an option */

	/* output */
	const hcl_bch_t* lngopt; 

	/* input + output */
	int              ind; /* index into parent argv vector */

	/* input + output - internal*/
	hcl_bch_t*       cur;
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hcl_getopt() function processes the \a argc command-line arguments
 * pointed to by \a argv as configured in \a opt. It can process two 
 * different option styles: a single character starting with '-', and a 
 * long name starting with '--'. 
 *
 * A character in \a opt.str is treated as a single character option. Should
 * it require a parameter, specify ':' after it.
 *
 * Two special returning option characters indicate special error conditions. 
 * - \b ? indicates a bad option stored in the \a opt->opt field.
 * - \b : indicates a bad parameter for an option stored in the \a opt->opt field.
 *
 * @return an option character on success, HCL_CHAR_EOF on no more options.
 */
HCL_EXPORT hcl_uci_t hcl_getuopt (
	int                argc, /* argument count */ 
	hcl_uch_t* const*  argv, /* argument array */
	hcl_uopt_t*        opt   /* option configuration */
);

HCL_EXPORT hcl_bci_t hcl_getbopt (
	int                argc, /* argument count */ 
	hcl_bch_t* const*  argv, /* argument array */
	hcl_bopt_t*        opt   /* option configuration */
);


#if defined(HCL_OOCH_IS_UCH)
#	define hcl_opt_t hcl_uopt_t
#	define hcl_opt_lng_t hcl_uopt_lng_t
#	define hcl_getopt(argc,argv,opt) hcl_getuopt(argc,argv,opt)
#else
#	define hcl_opt_t hcl_bopt_t
#	define hcl_opt_lng_t hcl_bopt_lng_t
#	define hcl_getopt(argc,argv,opt) hcl_getbopt(argc,argv,opt)
#endif

#if defined(__cplusplus)
}
#endif

#endif
