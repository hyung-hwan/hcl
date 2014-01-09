BEGIN {
	printf ("-- Generated with ascii.txt and ascii.awk\n");
	printf ("-- Run qseawk -f ascii.awk ascii.txt > h2-ascii.ads for regeneration\n\n");
	printf ("generic\n\ttype Character_Type is (<>);\npackage H2.Ascii is\n\n");

}

{
	t = sprintf ("%c", NR - 1);
	if (str::isprint(t)) t = " -- " t;
	else t="";
	printf ("\t%-20s: constant Character_Type := Character_Type'Val(%d);%s\n", $1, NR - 1, t);
}

END {
	printf ("\nend H2.Ascii;\n");
}
