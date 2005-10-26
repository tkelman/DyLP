
/*
  Simple C++ program to determine the size of a bool.
*/
/*
  Rather than go through the bother required to get this string into the
  binary, we'll settle for a comment.

  sccsid[] = "@(#)booltype.cpp	1.2	09/25/04" ;
  svnid[] = "$Id" ;
*/


#include <iostream>
#include <string>

extern "C" char *getCtype(int bytes) ;

int main (int argc, char *argv[])

{ bool verbose = false ;

  if (argc > 1)
  { if (argc == 2 && argv[1] == static_cast<std::string>("-v"))
      verbose = true ;
    else
    { std::cout << "usage: " << argv[0] << " [-v]\n" << std::endl ; ;
      exit (1) ; } }

  if (verbose)
  { std::cout << "C++ bool is " << sizeof(bool) << " bytes." << std::endl ;
    std::cout << "Matching C type is "
	      << getCtype(sizeof(bool)) << std::endl ; }
  std::cout << getCtype(sizeof(bool)) ;
  
  exit (0) ; }
