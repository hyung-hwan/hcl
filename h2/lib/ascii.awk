# This script requires QSEAWK.

BEGIN {
	printf ("-- Generated with ascii.txt and ascii.awk\n");
	printf ("-- Run qseawk -f ascii.awk ascii.txt > h2-ascii.ads for regeneration\n\n");
	printf ("generic\n\ttype Character_Type is (<>);\npackage H2.Ascii is\n\n");
	printf ("\tpackage Pos is\n");
}

{
	t = sprintf ("%c", NR - 1);
	if (str::isprint(t)) t = " -- " t;
	else t="";
	printf ("\t\t%-20s: constant := %d;%s\n", $1, NR-1, t);
	X[NR - 1] = $1; 
}

END {
	printf ("\tend Pos;\n\n");
	for (i = 0; i < length(X); i++)
	{
		printf ("\t%-20s: constant Character_Type := Character_Type'Val(Pos.%s);\n", X[i], X[i]);
	}
	printf ("\nend H2.Ascii;\n");
}
