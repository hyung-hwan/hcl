AC_INIT([H2], 0.1.0)

AC_CONFIG_HEADER([lib/config.h])
AC_CONFIG_AUX_DIR([ac])
AC_CONFIG_MACRO_DIR([ac])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_LANG(C)
AC_PROG_CC

AC_ARG_WITH([ada-objdir],
	[AS_HELP_STRING(--with-ada-objdir,specify ada object directory path)],
	[ada_objdir=$withval]
)
if test -n "$ada_objdir"
then
        AC_SUBST(ADA_OBJDIR, "$ada_objdir")
else
        AC_SUBST(ADA_OBJDIR, "objdir")
fi

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	lib/lib.gpr
	lib/libh2.gpr
	cmd/Makefile
	cmd/scheme.gpr
])
AC_OUTPUT
