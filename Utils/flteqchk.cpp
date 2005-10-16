
/*
  Simple C++ program to check the FltEql method.
*/

static char sccsid[] = "@(#)flteqchk.cpp	1.1	10/18/02" ;

#include <iostream.h>
#include <list>
#include <utility>
#include <math.h>
#include "../../COIN/Osi/include/OsiFloatEqual.hpp"

int main ()

{ ios::sync_with_stdio() ;

  typedef std::pair<double,double> dblpair ;
  typedef std::list<dblpair> dbllist ;
  dbllist tstvals ;

/*
  If we have honest IEEE infty, then infty-infty = NaN
*/
  double NaN = HUGE_VAL-HUGE_VAL ;
  
  OsiAbsFltEq abseq ;
  OsiRelFltEq releq ;
  
  tstvals.push_back(dblpair(0.0,0.0)) ;
  tstvals.push_back(dblpair(0.0,1.0)) ;
  tstvals.push_back(dblpair(0.0,HUGE_VAL)) ;
  tstvals.push_back(dblpair(HUGE_VAL,HUGE_VAL)) ;
  tstvals.push_back(dblpair(NaN,HUGE_VAL)) ;

  for (dbllist::iterator tstval = tstvals.begin() ;
       tstval != tstvals.end() ;
       tstval++)
  { double v1 = (*tstval).first ;
    double v2 = (*tstval).second ;
  cout << "v1 is " << v1 << ", v2 is " << v2 << ".\n" ;
  cout << "v1 - v2 is " << v1-v2 << ".\n" ;
  cout << "v1 == v2 is " << (v1 == v2) << ".\n" ;
  cout << "abseq(v1,v2) is " << abseq(v1,v2)
       << ".\n" ;
  cout << "releq(v1,v2) is " << releq(v1,v2)
       << ".\n" ; }
}

