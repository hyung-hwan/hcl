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
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAfRRANTIES
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

void hcl_dumpsymtab (hcl_t* hcl)
{
	hcl_oow_t i;
	hcl_oop_char_t symbol;

	HCL_DEBUG0 (hcl, "--------------------------------------------\n");
	HCL_DEBUG1 (hcl, "Stix Symbol Table %zu\n", HCL_OBJ_GET_SIZE(hcl->symtab->bucket));
	HCL_DEBUG0 (hcl, "--------------------------------------------\n");

	for (i = 0; i < HCL_OBJ_GET_SIZE(hcl->symtab->bucket); i++)
	{
		symbol = (hcl_oop_char_t)hcl->symtab->bucket->slot[i];
 		if ((hcl_oop_t)symbol != hcl->_nil)
		{
			HCL_DEBUG2 (hcl, " %07zu %O\n", i, symbol);
		}
	}

	HCL_DEBUG0 (hcl, "--------------------------------------------\n");
}

void hcl_dumpdic (hcl_t* hcl, hcl_oop_dic_t dic, const hcl_bch_t* title)
{
	hcl_oow_t i;
	hcl_oop_cons_t ass;

	HCL_DEBUG0 (hcl, "--------------------------------------------\n");
	HCL_DEBUG2 (hcl, "%s %zu\n", title, HCL_OBJ_GET_SIZE(dic->bucket));
	HCL_DEBUG0 (hcl, "--------------------------------------------\n");

	for (i = 0; i < HCL_OBJ_GET_SIZE(dic->bucket); i++)
	{
		ass = (hcl_oop_cons_t)dic->bucket->slot[i];
		if ((hcl_oop_t)ass != hcl->_nil)
		{
			HCL_DEBUG2 (hcl, " %07zu %O\n", i, ass->car);
		}
	}
	HCL_DEBUG0 (hcl, "--------------------------------------------\n");
}



