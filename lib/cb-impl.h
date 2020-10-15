
#ifndef _CB_IMPL_H_
#define _CB_IMPL_H_

#include <hcl.h>


#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	define HCL_IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')
#else
#	define HCL_IS_PATH_SEP(c) ((c) == '/')
#endif

/* TODO: handle path with a drive letter or in the UNC notation */
#define HCL_IS_PATH_ABSOLUTE(x) HCL_IS_PATH_SEP(x[0])


#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_errnum_t hcl_vmprim_syserrstrb (
	hcl_t*             hcl,
	int                syserr_type,
	int                syserr_code,
	hcl_bch_t*         buf,
	hcl_oow_t          len
);

HCL_EXPORT void hcl_vmprim_assertfail (
	hcl_t*             hcl,
	const hcl_bch_t*   expr,
	const hcl_bch_t*   file,
	hcl_oow_t          line
);


HCL_EXPORT void* hcl_vmprim_alloc_heap (
	hcl_t*             hcl,
	hcl_oow_t          size
);

HCL_EXPORT void hcl_vmprim_free_heap (
	hcl_t*             hcl,
	void*              ptr
);

HCL_EXPORT void hcl_vmprim_vm_gettime (
	hcl_t*             hcl,
	hcl_ntime_t*       now
);

HCL_EXPORT int hcl_vmprim_vm_sleep (
	hcl_t*             hcl,
	const hcl_ntime_t* dur
);

HCL_EXPORT void hcl_vmprim_dl_startup (
	hcl_t*             hcl
);

HCL_EXPORT void hcl_vmprim_dl_cleanup (
	hcl_t*             hcl
);

HCL_EXPORT void* hcl_vmprim_dl_open (
	hcl_t*             hcl, 
	const hcl_ooch_t*  name,
	int                flags
);

HCL_EXPORT void hcl_vmprim_dl_close (
	hcl_t*             hcl,
	void*              handle
);

HCL_EXPORT void* hcl_vmprim_dl_getsym (
	hcl_t*             hcl,
	void*              handle,
	const hcl_ooch_t*  name
);


#if defined(__cplusplus)
}
#endif

#endif
