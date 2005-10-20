/*
  This file is a portion of the OsiDylp LP distribution.

        Copyright (C) 2004 Lou Hafer

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
  This file contains generic routines to process simple options. In the main,
  these are called by the option processing routines that deal with the
  options file.
*/


#include "bonsai.h"
#include "bnfrdr.h"
#include "strrtns.h"

static char sccsid[] UNUSED = "@(#)options.c	3.5	09/25/04" ;



bool string_opt (char **str)

/*
  Generic (and fairly trivial) routine to parse a string.

  Parameters:
    str: address where the string is to be placed.

  Returns: TRUE, barring some sort of parsing failure.
*/

{ parse_any result ;
  char *rtnnme = "string_opt" ;

/*
  BNF to parse a string, returning (char *).
*/
  static tdef(zid,bnfttID,NULL,NULL) ;
  static tref(zgetstring_zid,zid,bnfstore|bnfatsgn,0) ;

  static comphd(zgetstring_alt) = { compcnt(1), mkcref(zgetstring_zid) } ;
  static gdef(zgetstring_int,sizeof(char *),NULL,zgetstring_alt) ;
  static gref(zgetstring,zgetstring_int,NULL,NULL,NULLP) ;

/*
  Initialise the reader, parse the string, and check for errors.
*/
  rdrinit() ;
  if (parse(cmdchn,&zgetstring,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zgetstring") ;
    return (FALSE) ; }
  outfmt(logchn,cmdecho," %s",*(char **) result.g) ;
  flushio(logchn,cmdecho) ;

  *str = *(char **) result.g ;

/*
  Clean up and return.
*/
  FREE(result.g) ;
  rdrclear() ;

  return (TRUE) ; }



bool integer_opt (int *iloc)

/*
  Generic (and fairly trivial) routine to parse an integer.

  Parameters:
    iloc: address where the integer is to be placed.

  Returns: TRUE, barring some sort of parsing failure.
*/

{ parse_any result ;
  char *rtnnme = "integer_opt" ;

/*
  BNF to parse an integer, returning (int *).
*/
  static tdef(zdnum,bnfttN,10,NULL) ;
  static tref(zgetnum_zdnum,zdnum,bnfstore,0) ;
  static comphd(zgetnum_alt) = { compcnt(1), mkcref(zgetnum_zdnum) } ;
  static gdef(zgetnum_int,sizeof(int),NULL,zgetnum_alt) ;
  static gref(zgetnum,zgetnum_int,NULL,NULL,NULLP) ;

/*
  Initialise the reader, parse the integer, and check for errors.
*/
  rdrinit() ;
  if (parse(cmdchn,&zgetnum,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zgetnum") ;
    return (FALSE) ; }
  outfmt(logchn,cmdecho," %d",*(int *) result.g) ;
  flushio(logchn,cmdecho) ;

  *iloc = *(int *) result.g ;

/*
  Clean up and return.
*/
  FREE(result.g) ;
  rdrclear() ;

  return (TRUE) ; }



bool double_opt (double *rloc)

/*
  Generic (and fairly trivial) routine to parse a real as a double. A double
  will provide about 15 -- 17 significant decimal digits.

  Parameters:
    rloc: address where the real is to be placed.

  Returns: TRUE, barring some sort of parsing failure.
*/

{ parse_any result ;
  char *rtnnme = "double_opt" ;

/*
  BNF to parse a real, returning (double *).
*/
  static tdef(zdnum,bnfttN,10,NULL) ;
  static tref(zgetnum_zdnum,zdnum,bnfdbl|bnfstore,0) ;
  static comphd(zgetnum_alt) = { compcnt(1), mkcref(zgetnum_zdnum) } ;
  static gdef(zgetnum_int,sizeof(double),NULL,zgetnum_alt) ;
  static gref(zgetnum,zgetnum_int,NULL,NULL,NULLP) ;

/*
  Initialise the reader, parse the real, and check for errors.
*/
  rdrinit() ;
  if (parse(cmdchn,&zgetnum,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zgetnum") ;
    return (FALSE) ; }
  outfmt(logchn,cmdecho," %g",*(double *) result.g) ;
  flushio(logchn,cmdecho) ;

  *rloc = *(double *) result.g ;

/*
  Clean up and return.
*/
  FREE(result.g) ;
  rdrclear() ;

  return (TRUE) ; }



bool real_opt (float *rloc)

/*
  Generic (and fairly trivial) routine to parse a real as a float. A float
  will provide about 6 -- 9 significant decimal digits.

  Parameters:
    rloc: address where the real is to be placed.

  Returns: TRUE, barring some sort of parsing failure.
*/

{ parse_any result ;
  char *rtnnme = "real_opt" ;

/*
  BNF to parse a real, returning (float *).
*/
  static tdef(zdnum,bnfttN,10,NULL) ;
  static tref(zgetnum_zdnum,zdnum,bnfflt|bnfstore,0) ;
  static comphd(zgetnum_alt) = { compcnt(1), mkcref(zgetnum_zdnum) } ;
  static gdef(zgetnum_int,sizeof(float),NULL,zgetnum_alt) ;
  static gref(zgetnum,zgetnum_int,NULL,NULL,NULLP) ;

/*
  Initialise the reader, parse the real, and check for errors.
*/
  rdrinit() ;
  if (parse(cmdchn,&zgetnum,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zgetnum") ;
    return (FALSE) ; }
  outfmt(logchn,cmdecho," %g",*(float *) result.g) ;
  flushio(logchn,cmdecho) ;

  *rloc = *(float *) result.g ;

/*
  Clean up and return.
*/
  FREE(result.g) ;
  rdrclear() ;

  return (TRUE) ; }



bool bool_opt (bool *bloc)

/*
  Generic (and fairly trivial) routine to parse a boolean. But ... we have to
  take some care here because of the way bool is handled. If you look at the
  typedef for bool in loustd.h, you'll see that it can change in size --- this
  is necessary for C++ compatibility in the COIN OSI layer implementation. But
  a bnfIdef_struct (an immediate) holds its value as an int, and when
  doimmediate tries to load a field, it casts to an int. To make a long
  story short, the val field in a boolopt_struct must be an int.

  Parameters:
    bloc: address where the boolean is to be placed.

  Returns: TRUE, barring some sort of parsing failure.
*/

{ struct boolopt_struct { char *str ;
			  int val ; } *boolopt ;
  parse_any result ;
  char *rtnnme = "bool_opt" ;

/*
  BNF to parse a boolean, returning the string.
*/
  static idef(ziTRUE,TRUE) ;
  static idef(ziFALSE,FALSE) ;
  static tdef(zTRUE,bnfttID,NULL,"TRUE") ;
  static tdef(zFALSE,bnfttID,NULL,"FALSE") ;

  static iref(zparsebool_ziTRUE,ziTRUE,mkoff(struct boolopt_struct,val)) ;
  static iref(zparsebool_ziFALSE,ziFALSE,mkoff(struct boolopt_struct,val)) ;
  static tref(zparsebool_zTRUE,zTRUE,bnfstore|bnfatsgn|bnfmin,
	      mkoff(struct boolopt_struct,str)) ;
  static tref(zparsebool_zFALSE,zFALSE,bnfstore|bnfatsgn|bnfmin,
	      mkoff(struct boolopt_struct,str)) ;
  static comphd(zparsebool_alt1) = { compcnt(2),
      mkcref(zparsebool_zFALSE), mkcref(zparsebool_ziFALSE) } ;
  static comphd(zparsebool_alt2) = { compcnt(2),
      mkcref(zparsebool_zTRUE), mkcref(zparsebool_ziTRUE) } ;
  static althd(zparsebool_alts) = { altcnt(2),
      mkaref(zparsebool_alt1), mkaref(zparsebool_alt2) } ;
  static npdef(zparsebool,zparsebool_alts) ;
  static npref(zparsebool_ref,zparsebool,NULL,NULLP) ;

  static comphd(zgetbool_alt) = { compcnt(1), mkcref(zparsebool_ref) } ;
  static gdef(zgetbool_int,sizeof(struct boolopt_struct),NULL,zgetbool_alt) ;
  static gref(zgetbool,zgetbool_int,NULL,NULL,NULLP) ;

/*
  Initialise the reader, parse the string, and check for errors.
*/
  rdrinit() ;
  if (parse(cmdchn,&zgetbool,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zgetbool") ;
    return (FALSE) ; }
  boolopt = (struct boolopt_struct *) result.g ;
  rdrclear() ;
  
  outfmt(logchn,cmdecho," %s",boolopt->str) ;
  flushio(logchn,cmdecho) ;

  *bloc = boolopt->val ;

/*
  Clean up and return.
*/
  STRFREE(boolopt->str) ;
  FREE(boolopt) ;

  return (TRUE) ; }
