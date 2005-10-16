
static char sccsid[] = "@(#)boolequiv.c	1.1	10/18/02" ;

/*
  This routine accepts a single numeric parameter and matches it to the
  appropriate C data type. It's used with boolcheck.cpp to return the variable
  type that should be used to define BOOL in the makefile.
*/

char *getCtype (int bytes)

{ if (bytes == sizeof(char))
    return ("char") ;
  else
  if (bytes == sizeof(int))
    return ("int") ;
  else
  if (bytes == sizeof(short int))
    return ("short int") ;
  else
  if (bytes == sizeof(long int))
    return ("long int") ;
  else
    return ("<error>") ; }

