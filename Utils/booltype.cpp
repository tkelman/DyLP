/*
  This file is a part of the OsiDylp LP distribution.

        Copyright (C) 2005 Lou Hafer

        School of Computing Science
        Simon Fraser University
        Burnaby, B.C., V5A 1S6, Canada
        lou@cs.sfu.ca

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St., Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
  Simple C++ program to determine the size of a bool.
*/
/*
  Rather than go through the bother required to get this string into the
  binary, we'll settle for a comment.

  sccs: sccsid[] = "@(#)booltype.cpp	1.2	09/25/04" ;
  svn/cvs: svnid[] = "$Id$" ;
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
