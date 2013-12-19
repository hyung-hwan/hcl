#ifndef _H2SCM_H_
#define _H2SCM_H_

typedef struct h2scm_t h2scm_t;
typedef struct h2scm_obj_t h2scm_obj_t;

#ifdef __cplusplus
extern "C" {
#endif

h2scm_t* h2scm_open (void);

void h2scm_close (h2scm_t* scm);

int h2scm_evaluate (h2scm_t* scm, h2scm_obj_t* src);

#ifdef __cplusplus
}
#endif


#endif
