#ifndef _KEYTAB_H
#define _KEYTAB_H

/*
  This file is part of a package of keyword recognition routines.

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
  Data structure for keyword tables searched by find and ambig
  
  @(#)keytab.h	1.2	08/31/99
*/

/*
  Field		Contents
  -----		--------
  keyword	Character string for the keyword.
  min		Minimum number of characters which must be matched before
		cimstrcmp will report a match.
  token		Value returned when the keyword is matched.
*/

typedef struct keytab_entry_internal { char *keyword ;
				       int min ;
				       int token ; } keytab_entry ;


/*
  binsrch.c
*/

extern int find(char *word, keytab_entry keytab[], int numkeys),
	   ambig(char *word, keytab_entry keytab[], int numkeys) ;

#endif /* _KEYTAB_H */
