
h2_size_t XFUN(strcpy) (xchar_t* dst, const xchar_t* src)
{
	const xchar_t* p = src;
	while (*p != XCHAR('\0')) *dst++ = *p++;
	return p - src;
}
