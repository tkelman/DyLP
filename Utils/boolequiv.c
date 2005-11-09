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
  Rather than go through the bother required to get this string into the
  binary, we'll settle for a comment.

  sccs: sccsid[] = "@(#)boolequiv.c	1.1	10/18/02" ;
  cvs/svn: svnid[] = "$Id$" ;
*/

/*
  This routine accepts a single numeric parameter and matches it to the
  appropriate C data type. It's used with boolcheck.cpp to return the variable
  type that should be used to define BOOL in the makefile.
*/

const char *getCtype (int bytes)

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

