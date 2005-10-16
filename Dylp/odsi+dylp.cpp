/*
  This file is a portion of the OsiDylp distribution.

        Copyright (C) 2004 Lou Hafer

        School of Computing Science
        Simon Fraser University
        Burnaby, B.C., V5A 1S6, Canada
        lou@cs.sfu.ca

  This program is free software ; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation ; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY ; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program ; if not, write to the Free Software Foundation, Inc., 59
  Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


/*! \file odsi+dylp.cpp
    \brief An alternate main program for running dylp by way of the
    OsiDylpSolverInterface. No frills. Good for testing when you suspect
    the problem lies not in dylp but in the OsiDylpSolverInterface.

    Requires that the COIN libraries be available.
*/

#include <iostream>
#include <assert.h>
#include "OsiDylpSolverInterface.hpp"
#include "CoinTime.hpp"
#include "CoinError.hpp"

namespace {
  char sccsid[] UNUSED = "@(#)odsi+dylp.cpp	1.5	11/06/04" ;
}

using std::string ;

/*! \brief Run an individual problem

  This routine will run a single problem, using an options file if one is
  specified. The routine will create a log file for the run.
*/

void test_user(const char* mps, const char* spc = 0)

{ OsiDylpSolverInterface* osi = new OsiDylpSolverInterface ;
  CoinMessageHandler *handler = osi->messageHandler() ;
  string mpsnme(mps) ;
  string basenme,ext ;
  bool gzSeen = false ;
  string::size_type pathpos,sfxpos,gzpos ;
  int info ;

/*
  Check for the standard .gz and .mps extensions. The notion is that we want
  a base name that's stripped of trailing .mps and .gz, and any leading path
  components.
*/
  std::cout << "Starting with \"" << mpsnme << "\" ("
	    << mpsnme.length() << ")\n" ;
  pathpos = mpsnme.rfind('/') ;
  if (pathpos == string::npos)
  { pathpos = 0 ; }
  else
  { pathpos++ ; }

  sfxpos = mpsnme.rfind('.') ;
  gzpos = mpsnme.length() ;
  if (sfxpos > pathpos && sfxpos < gzpos)
  { ext = mpsnme.substr(sfxpos) ;
    if (ext == ".gz")
    { gzpos = sfxpos ;
      gzSeen = true ;
      sfxpos = mpsnme.rfind('.',gzpos-1) ; } }
  if (sfxpos > pathpos && sfxpos < gzpos)
  { ext = mpsnme.substr(sfxpos,gzpos-sfxpos) ; }
  else
  { ext = "" ;
    sfxpos = gzpos ; }
  basenme = mpsnme.substr(pathpos,sfxpos-pathpos) ;
  osi->dylp_logfile(basenme.c_str(),false) ;
  osi->dylp_outfile(basenme.c_str()) ;

/*
  std::cout << "File name parsed as path \""
	    << mpsnme.substr(0,pathpos)
	    << "\"\n\t\tbase \"" << basenme << " (" << basenme.length() << ")"
	    << "\"\n\t\text  \"" << ext << " (" << ext.length() << ")"
	    << "\"\n\t\tgz   \"" << mpsnme.substr(gzpos) << "\""
	    << "(" << gzSeen << ")\n" ;
*/
  if (spc != 0)
  { std::cout << "looking for .spc file \"" << spc << "\"\n" ;
    info = 0x10 ;
    osi->setHintParam(OsiDoReducePrint,false,OsiHintTry,&info) ;
    osi->dylp_controlfile(spc,false) ; }
  else
  { info = OsiHintDo ;
    osi->setHintParam(OsiDoReducePrint,false,OsiHintTry,&info) ; }

  handler->setLogLevel(3) ;

  std::cout << "reading .mps file " << mpsnme << "\n" ;
  int errs = osi->readMps(mpsnme.substr(0,sfxpos).c_str(),ext.c_str()) ;
  if (errs != 0)
  { std::cout << errs << " errors while reading " << mps << "\n" ;
    throw CoinError("MPS input error","test_user","osi_test.cpp") ; }

  std::cout << "solving lp\n" ;
  try
  { double startTime = CoinCpuTime();
    osi->setHintParam(OsiDoPresolveInInitial,true,OsiHintTry) ;
    osi->setHintParam(OsiDoReducePrint,false,OsiForceDo) ;
    osi->initialSolve() ;
    double timeOfSolution = CoinCpuTime()-startTime;
    std::cout << "finished solving lp\n" ;
    double val = osi->getObjValue() ;
    int iters = osi->getIterationCount() ;
    std::cout << "obj = " << val
	      << ", iters = " << iters
	      << ", " << timeOfSolution << "secs."
	      << "\n" ;
    osi->dylp_printsoln(true,true) ; }
    catch (CoinError &err)
    { std::cout << err.className() << "::" << err.methodName()
		<< " (throw) " << err.message() << std::endl ;
      std::cout.flush() ; }
  
  delete osi ;

  exit(0) ; }


/*! \brief An alternate main program for testing

  This is an alternate program for testing dylp and the COIN/OSI layer.
  Given an mps file and (optional) options file on the command line, it will
  run the problem and exit.
*/

int main (int argc, char* argv[])

{ std::ios::sync_with_stdio() ;

  if (argc == 3) test_user(argv[1], argv[2]) ;
  if (argc == 2) test_user(argv[1]) ;

  return (0) ; }


