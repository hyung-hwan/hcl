
h2_size_t XFUN(strlen) (const xchar_t* str)
{
	const xchar_t* p = str;
	while (*p != XCHAR('\0')) p++;
	return p - str;
}

