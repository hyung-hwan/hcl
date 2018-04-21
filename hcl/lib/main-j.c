#include <hcl-json.h>
#include <stdio.h>
#include <stdlib.h>


static void* sys_alloc (hcl_mmgr_t* mmgr, hcl_oow_t size)
{
	return malloc(size);
}

static void* sys_realloc (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t size)
{
	return realloc(ptr, size);
}

static void sys_free (hcl_mmgr_t* mmgr, void* ptr)
{
	free (ptr);
}

static hcl_mmgr_t sys_mmgr =
{
	sys_alloc,
	sys_realloc,
	sys_free,
	HCL_NULL
};

int main (int argc, char* argv[])
{

	hcl_jsoner_t* jsoner;
	
	
	jsoner = hcl_jsoner_open (&mmgr, 0, NULL, NULL);


	hcl_jsoner_close (jsoner);
	return 0;
}
