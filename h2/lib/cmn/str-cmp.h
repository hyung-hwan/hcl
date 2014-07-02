

int XFUN(strcmp) (const xchar_t* s1, const xchar_t* s2)
{
	while (*s1 == *s2) 
	{
		if (*s1 == XCHAR('\0')) return 0;
		s1++, s2++;
	}

	return (*s1 > *s2)? 1: -1;
}
