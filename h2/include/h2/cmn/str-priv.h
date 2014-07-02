/* This is a private header. */


#ifdef __cplusplus
extern "C" {
#endif

int H2_XFUN(strcmp) (const h2_xchar_t* str1, const h2_xchar_t* str2);
h2_size_t H2_XFUN(strcpy) (h2_xchar_t* dst, const h2_xchar_t* src);
h2_size_t H2_XFUN(strfmc) (h2_xchar_t* buf, const h2_xchar_t* fmt, const h2_xchar_t* str[]);
h2_size_t H2_XFUN(strlen) (const h2_xchar_t* str);

#ifdef __cplusplus
}
#endif
