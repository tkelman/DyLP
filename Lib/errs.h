#ifndef _ERRS_H
#define _ERRS_H

/*
  This file is part of the errmsg package.

        Copyright (C) 1999 Lou Hafer

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
  with this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
  @(#)errs.h	2.3	03/18/04
*/

#include "loustd.h"
#ifdef FORTRAN
#include "fortran.h"
#endif

void errinit(const char *emsgpath, const char *elogpath, bool errecho),
     errterm(void) ;

void errmsg(int errid, ... ),
       warn(int errid, ... ) ;

#ifdef FORTRAN
void errmsg_(integer *errid, char *ident, ... ) ;
void warn_(integer *errid, char *ident, ... ) ;
#endif

#endif /* _ERRS_H */
