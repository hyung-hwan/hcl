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




/* TODO: hcl_loaddbgifromimage() -> load debug information from compiled image?
hcl_storedbgitoimage()? -> store debug information to compiled image?
hcl_compactdbgi()? -> compact debug information by scaning dbgi data. find class and method. if not found, drop the portion.
*/

int hcl_initdbgi (hcl_t* hcl, hcl_oow_t capa)
{
	hcl_dbgi_t* tmp;

	if (capa < HCL_SIZEOF(*tmp)) capa = HCL_SIZEOF(*tmp);

	tmp = (hcl_dbgi_t*)hcl_callocmem(hcl, capa);
	if (!tmp) return -1;

	tmp->_capa = capa;
	tmp->_len = HCL_SIZEOF(*tmp);
	/* tmp->_last_file = 0;
	tmp->_last_class = 0;
	tmp->_last_text = 0;
	tmp->_last_method = 0; */

	hcl->dbgi = tmp;
	return 0;
}

void hcl_finidbgi (hcl_t* hcl)
{
	if (hcl->dbgi)
	{
		hcl_freemem (hcl, hcl->dbgi);
		hcl->dbgi = HCL_NULL;
	}
}

static HCL_INLINE hcl_uint8_t* secure_dbgi_space (hcl_t* hcl, hcl_oow_t req_bytes)
{
	if (hcl->dbgi->_capa - hcl->dbgi->_len < req_bytes)
	{
		hcl_dbgi_t* tmp;
		hcl_oow_t newcapa;

		newcapa = hcl->dbgi->_len + req_bytes;
		newcapa = HCL_ALIGN_POW2(newcapa, 65536); /* TODO: make the align value configurable */
		tmp = hcl_reallocmem(hcl, hcl->dbgi, newcapa);
		if (!tmp) return HCL_NULL;

		hcl->dbgi = tmp;
		hcl->dbgi->_capa = newcapa;
	}

	return &((hcl_uint8_t*)hcl->dbgi)[hcl->dbgi->_len];
}

int hcl_addfiletodbgi (hcl_t* hcl, const hcl_ooch_t* file_name, hcl_oow_t* start_offset)
{
	hcl_oow_t name_len, name_bytes, name_bytes_aligned, req_bytes;
	hcl_dbgi_file_t* di;

	if (!hcl->dbgi) 
	{
		if (start_offset) *start_offset = 0;
		return 0; /* debug information is disabled*/
	}

	if (hcl->dbgi->_last_file > 0)
	{
		/* TODO: avoid linear search. need indexing for speed up */
		hcl_oow_t offset = hcl->dbgi->_last_file;
		do
		{
			di = (hcl_dbgi_file_t*)&((hcl_uint8_t*)hcl->dbgi)[offset];
			if (hcl_comp_oocstr((hcl_ooch_t*)(di + 1), file_name) == 0) 
			{
				if (start_offset) *start_offset = offset;
				return 0;
			}
			offset = di->_next;
		}
		while (offset > 0);
	}

	name_len = hcl_count_oocstr(file_name);
	name_bytes = (name_len + 1) * HCL_SIZEOF(*file_name);
	name_bytes_aligned = HCL_ALIGN_POW2(name_bytes, HCL_SIZEOF_OOW_T);
	req_bytes = HCL_SIZEOF(hcl_dbgi_file_t) + name_bytes_aligned;

	di = (hcl_dbgi_file_t*)secure_dbgi_space(hcl, req_bytes);
	if (!di) return -1;

	di->_type = HCL_DBGI_MAKE_TYPE(HCL_DBGI_TYPE_CODE_FILE, 0);
	di->_len = req_bytes;
	di->_next = hcl->dbgi->_last_file;
	hcl_copy_oocstr ((hcl_ooch_t*)(di + 1), name_len + 1, file_name);

	hcl->dbgi->_last_file = hcl->dbgi->_len;
	hcl->dbgi->_len += req_bytes;

	if (start_offset) *start_offset = hcl->dbgi->_last_file;
	return 0;
}

int hcl_addclasstodbgi (hcl_t* hcl, const hcl_ooch_t* class_name, hcl_oow_t file_offset, hcl_oow_t file_line, hcl_oow_t* start_offset)
{
	hcl_oow_t name_len, name_bytes, name_bytes_aligned, req_bytes;
	hcl_dbgi_class_t* di;

	if (!hcl->dbgi) return 0; /* debug information is disabled*/

	if (hcl->dbgi->_last_class > 0)
	{
		/* TODO: avoid linear search. need indexing for speed up */
		hcl_oow_t offset = hcl->dbgi->_last_class;
		do
		{
			di = (hcl_dbgi_class_t*)&((hcl_uint8_t*)hcl->dbgi)[offset];
			if (hcl_comp_oocstr((hcl_ooch_t*)(di + 1), class_name) == 0 && di->_file == file_offset && di->_line == file_line) 
			{
				if (start_offset) *start_offset = offset;
				return 0;
			}
			offset = di->_next;
		}
		while (offset > 0);
	}

	name_len = hcl_count_oocstr(class_name);
	name_bytes = (name_len + 1) * HCL_SIZEOF(*class_name);
	name_bytes_aligned = HCL_ALIGN_POW2(name_bytes, HCL_SIZEOF_OOW_T);
	req_bytes = HCL_SIZEOF(hcl_dbgi_class_t) + name_bytes_aligned;

	di = (hcl_dbgi_class_t*)secure_dbgi_space(hcl, req_bytes);
	if (!di) return -1;

	di->_type = HCL_DBGI_MAKE_TYPE(HCL_DBGI_TYPE_CODE_CLASS, 0);
	di->_len = req_bytes;
	di->_next = hcl->dbgi->_last_class;
	di->_file = file_offset;
	di->_line = file_line;
	hcl_copy_oocstr ((hcl_ooch_t*)(di + 1), name_len + 1, class_name);

	hcl->dbgi->_last_class = hcl->dbgi->_len;
	hcl->dbgi->_len += req_bytes;

	if (start_offset) *start_offset = hcl->dbgi->_last_class;
	return 0;
}

int hcl_addmethodtodbgi (hcl_t* hcl, hcl_oow_t file_offset, hcl_oow_t class_offset, const hcl_ooch_t* method_name, hcl_oow_t start_line, const hcl_oow_t* code_loc_ptr, hcl_oow_t code_loc_len, const hcl_ooch_t* text_ptr, hcl_oow_t text_len, hcl_oow_t* start_offset)
{
	hcl_oow_t name_len, name_bytes, name_bytes_aligned, code_loc_bytes, code_loc_bytes_aligned, text_bytes, text_bytes_aligned, req_bytes;
	hcl_dbgi_method_t* di;
	hcl_uint8_t* curptr;

	if (!hcl->dbgi) return 0; /* debug information is disabled*/

	name_len = hcl_count_oocstr(method_name);
	name_bytes = (name_len + 1) * HCL_SIZEOF(*method_name);
	name_bytes_aligned = HCL_ALIGN_POW2(name_bytes, HCL_SIZEOF_OOW_T);
	code_loc_bytes = code_loc_len * HCL_SIZEOF(*code_loc_ptr);
	code_loc_bytes_aligned = HCL_ALIGN_POW2(code_loc_bytes, HCL_SIZEOF_OOW_T);
	text_bytes = text_len * HCL_SIZEOF(*text_ptr);
	text_bytes_aligned = HCL_ALIGN_POW2(text_bytes, HCL_SIZEOF_OOW_T);
	req_bytes = HCL_SIZEOF(hcl_dbgi_method_t) + name_bytes_aligned + code_loc_bytes_aligned + text_bytes_aligned;

	di = (hcl_dbgi_method_t*)secure_dbgi_space(hcl, req_bytes);
	if (HCL_UNLIKELY(!di)) return -1;

	di->_type = HCL_DBGI_MAKE_TYPE(HCL_DBGI_TYPE_CODE_METHOD, 0);
	di->_len = req_bytes;
	di->_next = hcl->dbgi->_last_method;
	di->_file = file_offset;
	di->_class = class_offset;
	di->start_line = start_line;
	di->code_loc_start = name_bytes_aligned; /* distance from the beginning of the variable payload */
	di->code_loc_len = code_loc_len;
	di->text_start = name_bytes_aligned + code_loc_bytes_aligned; /* distance from the beginning of the variable payload */
	di->text_len = text_len;

	curptr = (hcl_uint8_t*)(di + 1);
	hcl_copy_oocstr ((hcl_ooch_t*)curptr, name_len + 1, method_name);

	curptr += name_bytes_aligned;
	HCL_MEMCPY (curptr, code_loc_ptr, code_loc_bytes);

	if (text_len > 0)
	{
		curptr += code_loc_bytes_aligned;
		hcl_copy_oochars ((hcl_ooch_t*)curptr, text_ptr, text_len);
	}

	hcl->dbgi->_last_method = hcl->dbgi->_len;
	hcl->dbgi->_len += req_bytes;

	if (start_offset) *start_offset = hcl->dbgi->_last_method;
	return 0;
}
