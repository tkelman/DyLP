/*
  This file is a portion of the bonsaiG MILP code.

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

#ifndef _BONSAI_H
#define _BONSAI_H

/*
  @(#)bonsai.h	3.3	06/22/04

  Common definitions used throughout bonsai. Start by hauling in a bunch of
  standard definitions.
*/

#include "loustd.h"
#include "io.h"
#include "errs.h"

/*
  bonsai.c

  Variables which control i/o operations.

  bonsai_version:	program version
  bonsai_time:		time the run started

  cmdchn		i/o id for command input
  logchn		i/o id for the execution log file

  cmdecho		controls echoing of command input to stdout
  gtxecho		controls echoing of generated text to stdout
*/

extern char *bonsai_version, *bonsai_time ;

extern ioid cmdchn,logchn ;

extern bool cmdecho,gtxecho ;


/*
  cmdint.c
*/

/*
  Return codes for command execution routines called from the command
  interpreter:

    cmdOK	execution of the command was adequately successful, further
		command interpretation should continue.
    cmdHALTNOERROR execution of the command was adequately successful, but break
		out of the command interpretation loop.
    cmdHALTERROR an error occurred during execution of the command, break
		out of the command interpretation loop.

  As return codes for process_cmds, the interpretation is slightly different:
    cmdOK	command interpretation was ended by an eof on the top level
		command channel (this is the normal case when command execution
		completes without error).
    cmdHALTNOERROR some command returned a cmdHALTNOERROR return code.
    cmdHALTERROR either a command returned a cmdHALTERROR return code, or a
		fatal error occurred in process_cmds.
*/

typedef enum { cmdOK, cmdHALTERROR, cmdHALTNOERROR } cmd_retval ;

cmd_retval process_cmds(bool silent) ;

#endif	/* _BONSAI_H */
