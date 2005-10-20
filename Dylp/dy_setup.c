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
  This file contains routines to establish options and tolerances for the dylp
  package.
*/

#define DYLP_INTERNAL

#include "dylib_strrtns.h"
#include "dy_cmdint.h"
#include "dylib_bnfrdr.h"
#include "dylib_keytab.h"
#include "dylp.h"
#include <float.h>
#include <limits.h>

static char sccsid[] UNUSED = "@(#)dy_setup.c	4.7	10/15/05" ;

/*
  We need only the options and tolerances structure from the main milp code,
  so a solitary declaration seems preferable to dragging in all of milp.h
  At best, though, this is a hack, and I need to deal with it properly, to
  retain dylp as a self-contained package.
*/

extern lpopts_struct *main_lpopts ;
extern lptols_struct *main_lptols ;


/*
  dyopts_dflt

  This structure holds the default values assigned prior to processing the
  user's option specifications. A value of -1 indicates that there is no
  default. Either we need to know more about the constraint system before
  choosing a default, or it's something only the user can tell us.
  
  The companion structures dyopts_lb and dyopts_ub give allowable ranges
  (where relevant) and are used to make sure the user doesn't give us nonsense
  values. They're sparsely populated -- it's not possible (or useful) to give
  bounds for a fair number of the values. An (r) indicates a recommended
  value -- the code will gripe about a violation, but won't enforce the limit.

  `Chvatal' is Chvatal, V., Linear Programming, W.H. Freeman, 1983.

  Some specific comments:

  scan:         Controls the number of variables scanned when the incoming
		variable selected during PSE updating is rejected due to
		pivoting problems and we need to scan for alternates.
  iterlim:      The value is set to 10000 <= 5*concnt in the absence of help
		from the user. Comments on pp. 45-46 of Chvatal indicate that
		3*concnt should be sufficient for all but the worst problems.
		A value of 0 means no limit is enforced.
  idlelim:      The value is set to 1000 <= 5*concnt <= 10000 in the absence
		of help from the user. (During dual simplex, it's enforced at
		80% of that, since the dual doesn't have antidegeneracy
		installed.)
  factor:	The default choice of 20 comes from the analysis on
		pp. 111 - 113 of Chvatal, "Linear Programming". The upper
		bound of 100 comes from a passing mention in the XMP
		documentation that factor > 100 shouldn't be used except
		for network problems. Given upgrades since the original version
		of the code, this should be relaxed, but I need to do some
		experiments to see just how far.
  check:	Check actually defaults to factor/2, with an upper limit of
		factor.
  active.*:	The value is for efficiency, not a limit. If it's too small,
		the dylp constraint system will have to do more expansions.
		25% is a pure guess. I'll try and refine it with experience.
  print.*:	The majority of the print controls are set to 0. The output
		is really only of interest if you're looking at some detail
		of the lp implementation.
*/

static
lpopts_struct dyopts_dflt = { -1,	/* scan */
			      -1,	/* iterlim */
			      -1,	/* idlelim */
			    { -1,	/* dpsel.strat */
			      TRUE,	/* dpsel.flex */
			      FALSE },	/* dpsel.allownopiv */
			    {  1 },	/* ppsel.strat */
			      50,	/* factor */
			      -1,	/* check */
			       1,	/* groom */
			     { 1,	/* con.actlvl */
			       0,	/* con.actlim */
			       0 },	/* con.deactlvl */
			       0,	/* addvar */
			       3,	/* dualadd */
			       5000,	/* coldvars */
			      FALSE,	/* forcecold */
			      FALSE,	/* forcewarm */
			      TRUE,	/* usedual */
			      TRUE,	/* degen */
			      1,	/* degenpivlim */
			      1,	/* degenlite */
			      TRUE,	/* patch */
			      FALSE,	/* fullsys */
			      FALSE,	/* copyorigsys */
			      2,	/* scaling */
			      { .25,	/* active.vars */
				.25 },	/* active.cons */
			      { .5,	/* initcons.frac */
				TRUE,	/* initcons.i1lopen */
				90,	/* initcons.i1l */
				FALSE,	/* initcons.i1uopen */
				180,	/* initcons.i1u */
				TRUE,	/* initcons.i2valid */
				FALSE,	/* initcons.i2lopen */
				0,	/* initcons.i2l */
				TRUE,	/* initcons.i2uopen */
				90 },	/* initcons.i2u */
			      1,	/* initbasis */
			      { TRUE,	/* finpurge.cons */
				TRUE },	/* finpurge.vars */
			      { FALSE,	/* heroics.d2p */
				FALSE,	/* heroics.p2d */
				FALSE }, /* heroics.flips */
			      { 0,	/* print.major */
			        0,	/* print.scaling */
			        0,	/* print.setup */
				0,	/* print.crash */
				0,	/* print.pricing */
				0,	/* print.pivoting */
				0,	/* print.pivreject */
				0,	/* print.degen */
				1,	/* print.phase1 */
				1,	/* print.phase2 */
				1,	/* print.dual */
				1,	/* print.basis */
				0,	/* print.conmgmt */
				0	/* print.varmgmt */
			      }
			    } ;

static
lpopts_struct dyopts_lb = { 200,	/* scan */
			    0,		/* iterlim */
			    0,		/* idlelim */
			    { 0,	/* dpsel.strat */
			      FALSE,	/* dpsel.flex */
			      FALSE },	/* dpsel.allownopiv */
			    {  0 },	/* ppsel.strat */
			    1,		/* factor */
			    1,		/* check */
			    0,		/* groom */
			     { 0,	/* con.actlvl */
			       0,	/* con.actlim */
			       0 },	/* con.deactlvl */
			    0,		/* addvar */
			    0,		/* dualadd */
			    0,		/* coldvars */
			    FALSE,	/* forcecold */
			    FALSE,	/* forcewarm */
			    FALSE,	/* usedual */
			    FALSE,	/* degen */
			    0,		/* degenpivlim */
			    0,		/* degenlite */
			    FALSE,	/* patch */
			    FALSE,	/* fullsys */
			    FALSE,	/* copyorigsys */
			    0,		/* scaling */
			    { 0.0,	/* active.vars */
			      0.0 },	/* active.cons */
			    { 0.0,	/* initcons.frac */
			      FALSE,	/* initcons.i1lopen */
			      0,	/* initcons.i1l */
			      FALSE,	/* initcons.i1uopen */
			      0,	/* initcons.i1u */
			      FALSE,	/* initcons.i2valid */
			      FALSE,	/* initcons.i2lopen */
			      0,	/* initcons.i2l */
			      FALSE,	/* initcons.i2uopen */
			      0 },	/* initcons.i2u */
			      1,	/* initbasis */
			      { FALSE,	/* finpurge.cons */
				FALSE }, /* finpurge.vars */
			    { FALSE,	/* heroics.d2p */
			      FALSE,	/* heroics.p2d */
			      FALSE },	/* heroics.flips */
			    { 0,	/* print.major */
			      0,	/* print.scaling */
			      0,	/* print.setup */
			      0,	/* print.crash */
			      0, 	/* print.pricing */
			      0, 	/* print.pivoting */
			      0,	/* print.pivreject */
			      0,	/* print.degen */
			      0,	/* print.phase1 */
			      0,	/* print.phase2 */
			      0,	/* print.dual */
			      0,	/* print.basis */
			      0,	/* print.conmgmt */
			      0		/* print.varmgmt */
			    }
			  } ;

/*
  Roughly, MAXITERLIM = MAXINT/4, so we can set the overall iteration limit
  to 3*iterlim without getting into integer overflow.
*/

#define MAXITERLIM (int) (((unsigned) 1<<(sizeof(dyopts_ub.iterlim)*8-3))-1)

static
lpopts_struct dyopts_ub = { 1000,	/* scan */
			    MAXITERLIM,	/* iterlim */
			    MAXITERLIM,	/* idlelim */
			    { 3,	/* dpsel.strat */
			      TRUE,	/* dpsel.flex */
			      TRUE },	/* dpsel.allownopiv */
			    {  1 },	/* ppsel.strat */
			    100,	/* factor (r) */
			    -1,		/* check == factor */
			     2,		/* groom */
			    {  1,	/* con.actlvl */
			      -1,	/* con.actlim */
			       2 },	/* con.deactlvl */
			    -1,		/* addvar */
			     3,		/* dualadd */
			    100000,	/* coldvars (r) */
			    TRUE,	/* forcecold */
			    TRUE,	/* forcewarm */
			    TRUE,	/* usedual */
			    TRUE,	/* degen */
			    -1,		/* degenpivlim */
			    5,		/* degenlite */
			    TRUE,	/* patch */
			    TRUE,	/* fullsys */
			    TRUE,	/* copyorigsys */
			    2,		/* scaling */
			    { 1.0,	/* active.vars */
			      1.0 },	/* active.cons */
			    { 1.0,	/* initcons.frac */
			      TRUE,	/* initcons.i1lopen */
			      180,	/* initcons.i1l */
			      TRUE,	/* initcons.i1uopen */
			      180,	/* initcons.i1u */
			      TRUE,	/* initcons.i2valid */
			      TRUE,	/* initcons.i2lopen */
			      180,	/* initcons.i2l */
			      TRUE,	/* initcons.i2uopen */
			      180 },	/* initcons.i2u */
			      3,	/* initbasis */
			      { TRUE,	/* finpurge.cons */
				TRUE },	/* finpurge.vars */
			    { TRUE,	/* heroics.d2p */
			      TRUE,	/* heroics.p2d */
			      TRUE },	/* heroics.flips */
			    { 1,	/* print.major */
			      2,	/* print.scaling */
			      5,	/* print.setup */
			      4,	/* print.crash */
			      3,	/* print.pricing */
			      5,	/* print.pivoting */
			      2,	/* print.pivreject */
			      5,	/* print.degen */
			      7,	/* print.phase1 */
			      7,	/* print.phase2 */
			      7,	/* print.dual */
			      5,	/* print.basis */
			      5,	/* print.conmgmt */
			      4		/* print.varmgmt */
			    }
			  } ;


/*
  dytols_dflt

  This structure holds the default values assigned prior to processing the
  user's tolerance specifications.

  Some specific comments:

  inf:          Infinity. Dylp can work with either IEEE infinity or a
		`finite' infinity (most often, DBL_MAX). The default is
		HUGE_VAL, which will resolve to IEEE infinity in a
		Sun/Solaris environment.  HUGE_VAL isn't always a
		compile-time constant, so we load it in dy_defaults. If the
		client code has a finite infinity, you surely want to pass
		this in to dylp. Otherwise, dylp will hand back IEEE
		infinity, and finite and infinite infinities do not play well
		together.
  zero:         Defaults to 1.0e-11. Historically I've seen it between
		1.0e-10 and 1.0e-12.  A colleague offers the following rule
		of thumb: ``The zero tolerance should be the product of the
		machine accuracy and the largest number you expect to
		encounter during processing.'' Since the 64 bit IEEE floating
		point format has a 52 bit mantissa, the machine precision is
		2^-52, or about 10^-15. 1.0e-11 is reasonable by this rule.
  pfeas:	This value will be scaled by an amount proportional to the
		1-norm of the basic variables, with a minimum value of zero.
		It's set to (pfeas_scale)*(zero tolerance) initially (right
		at the start of dylp), so that we have something that's valid
		when establishing the basis.
  pfeas_scale:	Allows decoupling of pfeas from zero. Defaults to 10, but
		can be adjusted by the user.
  cost:		The moral equivalent of 0 for things related to the objective
		function, reduced costs, dual variables, etc. Defaults to
		1.0e-10.
  dfeas:	This value will be scaled by an amount proportional to the
		1-norm of the dual variables, with a minimum value of cost.
		There is no default held here.
  dfeas_scale:	As pfeas_scale, for dfeas.
  pivot:        This is the pivot acceptance tolerance, as a fraction of the
		pivot selection tolerance used by the basis package during
		factoring. Defaults to 1.0e-5.
  bogus:        This multiplier is used to detect `bogus' values for reduced
		costs and variable values. Bogus values are values which
		exceed a zero tolerance but are less than bogus*tolerance.
		The idea is that, generally speaking, numbers close to a zero
		tolerance are problematic, and may well be the result of
		accumulated numerical inaccuracy.  dylp attempts to nip this
		problem in the bud by watching for numbers such that tol <
		|val| < bogus*tol for reduced costs and variable values and
		requesting refactorisation when it sees one. Defaults to 1.
		Higher values prompt more refactoring.
  swing:	When (new primal value)/(old primal value) > swing, dylp
		takes it as an indication that the problem needs more
		constraints (pseudo-unbounded).
  reframe:      Multiplier used to trigger a PSE or DSE reference framework
		reset. The default is .1, based on the computational results
		reported for the primal simplex in Forrest & Goldfarb,
		"Steepest Edge Algorithms for Linear Programming",
		Mathematical Programming v.57, pp. 341--374, 1992.
*/

lptols_struct dytols_dflt = { 0,		/* inf = HUGE_VAL */
			      1.0e-11,		/* zero */
			      1.0e-5, 		/* pchk */
			      -1, 		/* pfeas */
			      100, 		/* pfeas_scale */
			      1.0e-11,		/* cost	*/
			      1.0e-4,		/* dchk */
			      -1,		/* dfeas */
			      100,		/* dfeas_scale */
			      1.0e-5,		/* pivot */
			      1,		/* bogus */
			      1.0e15,		/* swing */
			      1.0e30,		/* toobig */
			      1.0e-4,		/* purge */
			      .5,		/* purgevar */
			      .1		/* reframe */
			    } ;




void dy_defaults (lpopts_struct **opts, lptols_struct **tols)

/*
  This routine establishes defaults for the various tolerances and options
  which are used by the dylp package.

  Parameters:
    opts	(i) lpopts structure; allocated if NULL
		(o) returns an lpopts structure
    tols	(i) lptols structure ; allocated if NULL
		(o) returns an lptols structure

  Returns: undefined
*/

{ 

#ifdef PARANOIA
  char *rtnnme = "dy_defaults" ;

  if (opts == NULL)
  { errmsg(2,rtnnme,"&opts") ;
    return ; }
  if (tols == NULL)
  { errmsg(2,rtnnme,"&tols") ;
    return ; }
#endif

  if (*opts == NULL)
  { (*opts) = (lpopts_struct *) MALLOC(sizeof(lpopts_struct)) ; }
  memcpy(*opts,&dyopts_dflt,sizeof(lpopts_struct)) ;

  if (*tols == NULL)
  { (*tols) = (lptols_struct *) MALLOC(sizeof(lptols_struct)) ; }
  memcpy(*tols,&dytols_dflt,sizeof(lptols_struct)) ;
  (*tols)->inf = HUGE_VAL ;

  return ; }



void dy_checkdefaults (consys_struct *sys,
		       lpopts_struct *opts, lptols_struct *tols)

/*
  This routine looks over various option and tolerance settings with an eye
  toward setting or adjusting values based on the size of the constraint
  system or other options that might be set by the user.

  The default values here are, by and large, grossly larger than required.
  The more outrageous ones are motivated by the Netlib examples.

  Parameters:
    sys:	a constraint system
    opts:	an options structure; may be modified on return
  
  Returns: undefined
*/

{ int scalefactor ;

  if (opts->check < 0) opts->check = opts->factor/2 ;
  if (opts->check <= 0) opts->check = 1 ;

  if (opts->scan < 0)
  { opts->scan = maxx(dyopts_lb.scan,sys->archvcnt/2) ;
    opts->scan = minn(opts->scan,dyopts_ub.scan) ; }

  if (opts->iterlim < 0)
  { opts->iterlim = minn(5*(sys->concnt+sys->varcnt),100000) ;
    opts->iterlim = maxx(opts->iterlim,10000) ; }

  if (opts->idlelim < 0)
  { opts->idlelim = minn(2*(sys->concnt+sys->varcnt),50000) ;
    opts->idlelim = maxx(opts->idlelim,1000) ; }

  if (opts->degenpivlim < 0)
  { opts->degenpivlim = minn(1000,sys->concnt/2) ;
    opts->degenpivlim = maxx(100,opts->degenpivlim) ; }

/*
  If the user has specified a dual pivot strategy, observe it. If not, start
  with strategy 1 (max dual objective improvement).
*/
  if (opts->dpsel.strat >= 0)
  { opts->dpsel.flex = FALSE ; }
  else
  { opts->dpsel.strat = 1 ;
    opts->dpsel.flex = TRUE ; }

  if (opts->fullsys == TRUE)
  { opts->active.vars = 1.0 ;
    opts->active.cons = 1.0 ; }
/*
  Loosen the base primal and dual accuracy check values for larger systems,
  and put a little more distance between the zero tolerances and feasibility
  tolerances.
*/
  scalefactor = ((int) (.5 + log10((double) sys->varcnt))) - 2 ;
  if (scalefactor > 0)
  { tols->pchk *= pow(10,(double) scalefactor) ;
    tols->pfeas_scale *= pow(10,(double) scalefactor) ; }
  scalefactor = ((int) (.5 + log10((double) sys->concnt))) - 2 ;
  if (scalefactor > 0)
  { tols->dchk *= pow(10,(double) scalefactor) ;
    tols->dfeas_scale *= pow(10,(double) scalefactor) ; }

/*
  XX_DEBUG_XX

  There's no good way to control this print statement, given the timing and
  purpose of this call. But it's occasionally handy for debugging.

  outfmt(dy_logchn,TRUE,"\nPTOLS: pzero = %g, pscale = %g, pchk = %g",
	 tols->zero,tols->pfeas_scale,tols->pchk) ;
  outfmt(dy_logchn,TRUE,"\nDTOLS: dzero = %g, dscale = %g, dchk = %g",
	 tols->cost,tols->dfeas_scale,tols->dchk) ;
*/

  return ; }



void dy_setprintopts (int lvl, lpopts_struct *opts)

/*
  This routine tweaks the lp print level options based on a single integer
  code. It's intended to allow clients of dylp to set up overall print
  levels. Just a big case statement.
  
  Level 0 forces dylp to shut up.
  
  Level 1 assumes the normal dylp defaults (phase1, phase2, dual, and basis
  print levels set to 1, which allows messages about extraordinary events).

  Levels 2--5 provide increasing amounts of information. At level 1 and
  above, a specific setting of a dylp value to a higher value will override
  the value here.

  Levels less then 0 are forced to 0, and anything over 4 is interpreted as 5.

  Parameters:
    lvl:	overall print level
    opts:	options structure; should be preloaded with defaults
  
  Returns: undefined
*/

{ if (lvl < 0) lvl = 0 ;

  switch (lvl)
  { case 0:
    { opts->print.major = 0 ;
      opts->print.setup = 0 ;
      opts->print.crash = 0 ;
      opts->print.pricing = 0 ;
      opts->print.pivoting = 0 ;
      opts->print.pivreject = 0 ;
      opts->print.degen = 0 ;
      opts->print.phase1 = 0 ;
      opts->print.phase2 = 0 ;
      opts->print.dual = 0 ;
      opts->print.basis = 0 ;
      opts->print.conmgmt = 0 ;
      opts->print.varmgmt = 0 ;
      break ; }
    case 1:
    { opts->print.major = maxx(opts->print.major,dyopts_dflt.print.major) ;
      opts->print.setup = maxx(opts->print.setup,dyopts_dflt.print.setup) ;
      opts->print.crash = maxx(opts->print.crash,dyopts_dflt.print.crash) ;
      opts->print.pricing = maxx(opts->print.pricing,
				 dyopts_dflt.print.pricing) ;
      opts->print.pivoting = maxx(opts->print.pivoting,
				  dyopts_dflt.print.pivoting) ;
      opts->print.pivreject = maxx(opts->print.pivreject,
				  dyopts_dflt.print.pivreject) ;
      opts->print.degen = maxx(opts->print.degen,dyopts_dflt.print.degen) ;
      opts->print.phase1 = maxx(opts->print.phase1,dyopts_dflt.print.phase1) ;
      opts->print.phase2 = maxx(opts->print.phase2,dyopts_dflt.print.phase2) ;
      opts->print.dual = maxx(opts->print.dual,dyopts_dflt.print.dual) ;
      opts->print.basis = maxx(opts->print.basis,dyopts_dflt.print.basis) ;
      opts->print.conmgmt = maxx(opts->print.conmgmt,
				 dyopts_dflt.print.conmgmt) ;
      opts->print.varmgmt = maxx(opts->print.varmgmt,
				 dyopts_dflt.print.varmgmt) ;
      break ; }
    case 2:
    { opts->print.major = maxx(opts->print.major,1) ;
      opts->print.setup = maxx(opts->print.setup,1) ;
      opts->print.crash = maxx(opts->print.crash,1) ;
      opts->print.pricing = maxx(opts->print.pricing,0) ;
      opts->print.pivoting = maxx(opts->print.pivoting,0) ;
      opts->print.pivreject = maxx(opts->print.pivreject,0) ;
      opts->print.degen = maxx(opts->print.degen,1) ;
      opts->print.phase1 = maxx(opts->print.phase1,1) ;
      opts->print.phase2 = maxx(opts->print.phase2,1) ;
      opts->print.dual = maxx(opts->print.dual,1) ;
      opts->print.basis = maxx(opts->print.basis,1) ;
      opts->print.conmgmt = maxx(opts->print.conmgmt,1) ;
      opts->print.varmgmt = maxx(opts->print.varmgmt,1) ;
      break ; }
    case 3:
    { opts->print.major = maxx(opts->print.major,1) ;
      opts->print.setup = maxx(opts->print.setup,2) ;
      opts->print.crash = maxx(opts->print.crash,2) ;
      opts->print.pricing = maxx(opts->print.pricing,0) ;
      opts->print.pivoting = maxx(opts->print.pivoting,0) ;
      opts->print.pivreject = maxx(opts->print.pivreject,0) ;
      opts->print.degen = maxx(opts->print.degen,1) ;
      opts->print.phase1 = maxx(opts->print.phase1,3) ;
      opts->print.phase2 = maxx(opts->print.phase2,3) ;
      opts->print.dual = maxx(opts->print.dual,3) ;
      opts->print.basis = maxx(opts->print.basis,2) ;
      opts->print.conmgmt = maxx(opts->print.conmgmt,2) ;
      opts->print.varmgmt = maxx(opts->print.varmgmt,2) ;
      break ; }
    case 4:
    { opts->print.major = maxx(opts->print.major,1) ;
      opts->print.setup = maxx(opts->print.setup,3) ;
      opts->print.crash = maxx(opts->print.crash,3) ;
      opts->print.pricing = maxx(opts->print.pricing,0) ;
      opts->print.pivoting = maxx(opts->print.pivoting,0) ;
      opts->print.pivreject = maxx(opts->print.pivreject,0) ;
      opts->print.degen = maxx(opts->print.degen,2) ;
      opts->print.phase1 = maxx(opts->print.phase1,4) ;
      opts->print.phase2 = maxx(opts->print.phase2,4) ;
      opts->print.dual = maxx(opts->print.dual,4) ;
      opts->print.basis = maxx(opts->print.basis,3) ;
      opts->print.conmgmt = maxx(opts->print.conmgmt,3) ;
      opts->print.varmgmt = maxx(opts->print.varmgmt,2) ;
      break ; }
    default:
    { opts->print.major = maxx(opts->print.major,1) ;
      opts->print.setup = maxx(opts->print.setup,5) ;
      opts->print.crash = maxx(opts->print.crash,4) ;
      opts->print.pricing = maxx(opts->print.pricing,1) ;
      opts->print.pivoting = maxx(opts->print.pivoting,1) ;
      opts->print.pivreject = maxx(opts->print.pivreject,1) ;
      opts->print.degen = maxx(opts->print.degen,2) ;
      opts->print.phase1 = maxx(opts->print.phase1,5) ;
      opts->print.phase2 = maxx(opts->print.phase2,5) ;
      opts->print.dual = maxx(opts->print.dual,5) ;
      opts->print.basis = maxx(opts->print.basis,5) ;
      opts->print.conmgmt = maxx(opts->print.conmgmt,3) ;
      opts->print.varmgmt = maxx(opts->print.varmgmt,2) ;
      break ; } }
  
  return ; }



/*
  Option processing routines. These are exported only to cmdint.c:docmds
  where they are called. In turn, they will often make use of one of the set
  of generic option processing routines from options.c. The rationale behind
  this arrangement is primarily data hiding -- I wanted to keep knowledge of
  the option defaults in one place.
*/

cmd_retval dy_printopt (const char *keywd)

/*
  This routine handles the manifold parameters that control just how much dylp
  tells you while it's working. This routine doesn't return an error, even
  when it can't parse the command, on the general theory that print levels
  aren't really critical. It will complain, however.

  The bnf for the print option command is:
    <printopt> ::= lpprint <what> <level>
    <what> ::= conmgmt | crash | degen | dual | basis | major |
	       phase1 | phase2 | pivoting | pivreject | pricing |
	       setup | varmgmt
    <level> ::= <integer>

  Parameters:
    keywd:	The command keyword

  Returns: cmdOK
*/

{ char *what,cmdstr[50] ;
  int code,dflt,lb,ub,*opt ;
  char *rtnnme = "dy_printopt" ;

  extern bool string_opt(char **str),		/* options.c */
	      integer_opt(int *iloc) ;

/*
  A lookup table with the various <what> keywords recognised by the print
  command.
*/

  enum prntcodes { poINV = 0, poCONMGMT, poCRASH, poDUAL, poBASIS, poMAJOR,
		   poSCALING, poSETUP, poPRICING, poPHASE1, poPHASE2,
		   poPIVOTING, poPIVREJ, poDEGEN, poVARMGMT } prntcode ;

  static keytab_entry prntkwds[] = { { "basis", 1, (int) poBASIS },
				     { "conmgmt", 2, (int) poCONMGMT },
				     { "crash", 2, (int) poCRASH },
				     { "degen", 2, (int) poDEGEN },
				     { "dual", 2, (int) poDUAL },
				     { "major", 1, (int) poMAJOR },
				     { "phase1", 6, (int) poPHASE1 },
				     { "phase2", 6, (int) poPHASE2 },
				     { "pivoting", 4, (int) poPIVOTING },
				     { "pivreject", 4, (int) poPIVREJ },
				     { "pricing", 2, (int) poPRICING },
				     { "scaling", 2, (int) poSCALING },
				     { "setup", 2, (int) poSETUP },
				     { "varmgmt", 1, (int) poVARMGMT }
				   } ;

  static int numprntcodes = (sizeof prntkwds/sizeof (keytab_entry)) ;

/*
  Now to work. Parse off the <what> keyword and see if we can look it up.
*/
  prntcode = poINV ;
  if (string_opt(&what) == TRUE)
  { code = ambig(what,prntkwds,numprntcodes) ;
    if (code < 0) 
    { if (code < -1)
	errmsg(233,rtnnme,what) ;
      else
	errmsg(234,rtnnme,what) ; }
    else
      prntcode = (enum prntcodes) code ; }
/*
  Set the various variables for each command.
*/
  outfxd(cmdstr,-((int) (sizeof(cmdstr)-1)),'l',"%s %s",keywd,what) ;
  switch (prntcode)
  { case poCONMGMT:
    { opt = &main_lpopts->print.conmgmt ;
      dflt = dyopts_dflt.print.conmgmt ;
      lb = dyopts_lb.print.conmgmt ;
      ub = dyopts_ub.print.conmgmt ;
      break ; }
    case poCRASH:
    { opt = &main_lpopts->print.crash ;
      dflt = dyopts_dflt.print.crash ;
      lb = dyopts_lb.print.crash ;
      ub = dyopts_ub.print.crash ;
      break ; }
    case poDEGEN:
    { opt = &main_lpopts->print.degen ;
      dflt = dyopts_dflt.print.degen ;
      lb = dyopts_lb.print.degen ;
      ub = dyopts_ub.print.degen ;
      break ; }
    case poBASIS:
    { opt = &main_lpopts->print.basis ;
      dflt = dyopts_dflt.print.basis ;
      lb = dyopts_lb.print.basis ;
      ub = dyopts_ub.print.basis ;
      break ; }
    case poMAJOR:
    { opt = &main_lpopts->print.major ;
      dflt = dyopts_dflt.print.major ;
      lb = dyopts_lb.print.major ;
      ub = dyopts_ub.print.major ;
      break ; }
    case poPHASE1:
    { opt = &main_lpopts->print.phase1 ;
      dflt = dyopts_dflt.print.phase1 ;
      lb = dyopts_lb.print.phase1 ;
      ub = dyopts_ub.print.phase1 ;
      break ; }
    case poPHASE2:
    { opt = &main_lpopts->print.phase2 ;
      dflt = dyopts_dflt.print.phase2 ;
      lb = dyopts_lb.print.phase2 ;
      ub = dyopts_ub.print.phase2 ;
      break ; }
    case poDUAL:
    { opt = &main_lpopts->print.dual ;
      dflt = dyopts_dflt.print.dual ;
      lb = dyopts_lb.print.dual ;
      ub = dyopts_ub.print.dual ;
      break ; }
    case poPIVOTING:
    { opt = &main_lpopts->print.pivoting ;
      dflt = dyopts_dflt.print.pivoting ;
      lb = dyopts_lb.print.pivoting ;
      ub = dyopts_ub.print.pivoting ;
      break ; }
    case poPIVREJ:
    { opt = &main_lpopts->print.pivreject ;
      dflt = dyopts_dflt.print.pivreject ;
      lb = dyopts_lb.print.pivreject ;
      ub = dyopts_ub.print.pivreject ;
      break ; }
    case poPRICING:
    { opt = &main_lpopts->print.pricing ;
      dflt = dyopts_dflt.print.pricing ;
      lb = dyopts_lb.print.pricing ;
      ub = dyopts_ub.print.pricing ;
      break ; }
    case poSCALING:
    { opt = &main_lpopts->print.scaling ;
      dflt = dyopts_dflt.print.scaling ;
      lb = dyopts_lb.print.scaling ;
      ub = dyopts_ub.print.scaling ;
      break ; }
    case poSETUP:
    { opt = &main_lpopts->print.setup ;
      dflt = dyopts_dflt.print.setup ;
      lb = dyopts_lb.print.setup ;
      ub = dyopts_ub.print.setup ;
      break ; }
    case poVARMGMT:
    { opt = &main_lpopts->print.varmgmt ;
      dflt = dyopts_dflt.print.varmgmt ;
      lb = dyopts_lb.print.varmgmt ;
      ub = dyopts_ub.print.varmgmt ;
      break ; }
    default:
    { errmsg(236,rtnnme,"<what>","keyword",keywd) ;
      return (cmdOK) ; } }
/*
  Last but not least, the actual work. A negative value is taken as a request
  from the user to be told the default value.
*/
  if (integer_opt(opt) == TRUE)
  { if (*opt >= 0)
    { if (*opt > ub)
      { warn(241,rtnnme,lb,cmdstr,ub,*opt,ub) ;
	*opt  = ub ; } }
    else
    { warn(243,rtnnme,cmdstr,dflt) ; } }
  else
  { errmsg(236,rtnnme,"<level>","parameter",keywd) ; }

  STRFREE(what) ;

  return (cmdOK) ; }



static bool lpctl_active (void)

/*
  This routine processes the 'active' subcommand, which sets values that
  determine the initial size of dylp's copy of the constraint system. The
  values are expressed in terms of fractions of the number of inequalities
  and number of variables in the original system.

  The bnf for the active subcommand is:
    <activeopt> ::= lpcontrol active  <fracspec-LIST(,)> ;
    <fracspec> ::= variables <float> | constraints <float>
  By the time this routine is called, `lpcontrol active' is already parsed.

  Parameters: none

  Returns: TRUE if the remainder of the command parses without error, FALSE
	   otherwise.
*/

{ struct actfrac_struct { int var_seen ;
			  float varfrac ;
			  int con_seen ;
			  float confrac ; } *actfrac ;
  parse_any result ;

  char *rtnnme = "lpctl_active" ;

/*
  BNF for the active command.
*/
  static tdef(zcomma,bnfttD,NULL,",") ;
  static tref(zcomma_ref,zcomma,NULL,NULL) ;
  static tdef(zdnum,bnfttN,10,NULL) ;
  static tref(zactfrac_varfrac,zdnum,bnfstore|bnfflt,
	      mkoff(struct actfrac_struct,varfrac)) ;
  static tref(zactfrac_confrac,zdnum,bnfstore|bnfflt,
	      mkoff(struct actfrac_struct,confrac)) ;
  static tdef(zvar,bnfttID,NULL,"variables") ;
  static tref(zvar_ref,zvar,bnfmin,NULL) ;
  static tdef(zcon,bnfttID,NULL,"constraints") ;
  static tref(zcon_ref,zcon,bnfmin,NULL) ;
  static idef(ziTRUE,TRUE) ;
  static iref(zactfrac_varseen,ziTRUE,mkoff(struct actfrac_struct,var_seen)) ;
  static iref(zactfrac_conseen,ziTRUE,mkoff(struct actfrac_struct,con_seen)) ;

  static comphd(zactfrac_alt1) = { compcnt(3),
    mkcref(zvar_ref),mkcref(zactfrac_varseen),mkcref(zactfrac_varfrac) } ;
  static comphd(zactfrac_alt2) = { compcnt(3),
    mkcref(zcon_ref),mkcref(zactfrac_conseen),mkcref(zactfrac_confrac) } ;
  static althd(zactfrac_alts) = { altcnt(2),
    mkaref(zactfrac_alt1),mkaref(zactfrac_alt2) } ;
  static npdef(zactfrac,zactfrac_alts) ;
  static npref(zactfrac_list,zactfrac,bnflst,zcomma_ref) ;

  static comphd(zactfracs_alt) = { compcnt(1), mkcref(zactfrac_list) } ;
  static gdef(zactfracs_int,sizeof(struct actfrac_struct),NULL,
	      zactfracs_alt) ;
  static gref(zactfracs,zactfracs_int,NULL,NULL,NULLP) ;

/*
  Now to work.  Initialise the reader, attempt to parse the command, and
  check for errors.
*/
  rdrinit() ;
  if (parse(dy_cmdchn,&zactfracs,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zactfracs") ;
    return (FALSE) ; }
  actfrac = (struct actfrac_struct *) result.g ;
  rdrclear() ;
/*
  Process the results.
*/
  if (actfrac->var_seen == TRUE)
  { outfmt(dy_logchn,dy_gtxecho," variables %.2f",actfrac->varfrac) ;
    if (actfrac->con_seen == TRUE)
      outchr(dy_logchn,dy_gtxecho,',') ;
    if (actfrac->varfrac >= 0)
    { if (actfrac->varfrac > dyopts_ub.active.vars)
      { warn(244,rtnnme,dyopts_lb.active.vars,"variables",
	     dyopts_ub.active.vars,actfrac->varfrac,dyopts_ub.active.vars) ;
	main_lpopts->active.vars = dyopts_ub.active.vars ; }
      else
	main_lpopts->active.vars = actfrac->varfrac ; }
    else
    { warn(245,rtnnme,"variables",dyopts_dflt.active.vars) ; } }

  if (actfrac->con_seen == TRUE)
  { outfmt(dy_logchn,dy_gtxecho," constraints %.2f",actfrac->confrac) ;
    if (actfrac->confrac >= 0)
    { if (actfrac->confrac > dyopts_ub.active.cons)
      { warn(244,rtnnme,dyopts_lb.active.cons,"constraints",
	     dyopts_ub.active.cons,actfrac->confrac,dyopts_ub.active.cons) ;
	main_lpopts->active.cons = dyopts_ub.active.cons ; }
      else
	main_lpopts->active.cons = actfrac->confrac ; }
    else
    { warn(245,rtnnme,"constraints",dyopts_dflt.active.cons) ; } }

  FREE(actfrac) ;

  return (TRUE) ; }



static bool lpctl_finpurge (void)

/*
  This routine processes the "final" subcommand, which currently controls
  only whether dylp does a final round of variable and/or constraint
  deactivation after finding an optimal solution. Note that this option works
  even when the problem has been solved with the fullsys option. This is
  intended for use in branch&cut, where we usually want to solve the initial
  lp with fullsys, then continue with dynamic lp.

  The bnf for the final purge subcommand is
    <final> ::= lpcontrol final purge <purgespec-LIST(,)> ;
    <purgespec> ::= <what> <sense>
    <what> ::= variables | constraints
    <sense> ::= true | false
  By the time this routine is called, `lpcontrol final' is already parsed.

  Parameters: none

  Returns: TRUE if the remainder of the command parses without error, FALSE
	   otherwise.
*/

{ struct finpurge_struct { int cons ;
			   int vars ; } ;
  struct finpurge_struct *optvals ;

  parse_any result ;

  char *rtnnme = "lpctl_finpurge" ;


/*
  BNF for the final purge command.
*/
  static tdef(znil,bnfttNIL,NULL,NULL) ;
  static tref(znil_ref,znil,NULL,NULL) ;
  static tdef(zcomma,bnfttD,NULL,",") ;
  static tref(zcomma_ref,zcomma,NULL,NULL) ;

  static tdef(zpurge,bnfttID,NULL,"purge") ;
  static tref(zpurge_ref,zpurge,bnfmin,NULL) ;
  static tdef(zvar,bnfttID,NULL,"variables") ;
  static tref(zvar_ref,zvar,bnfmin,NULL) ;
  static tdef(zcon,bnfttID,NULL,"constraints") ;
  static tref(zcon_ref,zcon,bnfmin,NULL) ;

  static idef(ziminus1,-1) ;
  static idef(ziTRUE,TRUE) ;
  static idef(ziFALSE,FALSE) ;
  static tdef(zTRUE,bnfttID,NULL,"TRUE") ;
  static tref(zTRUE_ref,zTRUE,bnfmin,NULL) ;
  static tdef(zFALSE,bnfttID,NULL,"FALSE") ;
  static tref(zFALSE_ref,zFALSE,bnfmin,NULL) ;

  static iref(zwhatvar_ziTRUE,ziTRUE,mkoff(struct finpurge_struct,vars)) ;
  static iref(zwhatvar_ziFALSE,ziFALSE,mkoff(struct finpurge_struct,vars)) ;
  static comphd(zwhat_alt1) = { compcnt(3),
      mkcref(zvar_ref),mkcref(zTRUE_ref),mkcref(zwhatvar_ziTRUE) } ;
  static comphd(zwhat_alt2) = { compcnt(3),
      mkcref(zvar_ref),mkcref(zFALSE_ref),mkcref(zwhatvar_ziFALSE) } ;
  static iref(zwhatcon_ziTRUE,ziTRUE,mkoff(struct finpurge_struct,cons)) ;
  static iref(zwhatcon_ziFALSE,ziFALSE,mkoff(struct finpurge_struct,cons)) ;
  static comphd(zwhat_alt3) = { compcnt(3),
      mkcref(zcon_ref),mkcref(zFALSE_ref),mkcref(zwhatcon_ziFALSE) } ;
  static comphd(zwhat_alt4) = { compcnt(3),
      mkcref(zcon_ref),mkcref(zTRUE_ref),mkcref(zwhatcon_ziTRUE) } ;
  static comphd(zwhat_alt5) = { compcnt(1), mkcref(znil_ref) } ;
  static althd(zwhat_alts) = { altcnt(5),
      mkaref(zwhat_alt1),mkaref(zwhat_alt2),mkaref(zwhat_alt3),
      mkaref(zwhat_alt4),mkaref(zwhat_alt5) } ;
  static npdef(zwhat,zwhat_alts) ;

  static npref(zfinpurge_what,zwhat,bnflst,zcomma_ref) ;

  static iref(zfinpurge_varminus1,ziminus1,
	      mkoff(struct finpurge_struct,vars)) ;
  static iref(zfinpurge_conminus1,ziminus1,
	      mkoff(struct finpurge_struct,cons)) ;
  static comphd(zfinpurge_alt) = { compcnt(4),mkcref(zfinpurge_varminus1),
      mkcref(zfinpurge_conminus1),mkcref(zpurge_ref),mkcref(zfinpurge_what) } ;
  static gdef(zfinpurge_def,
	      sizeof(struct finpurge_struct),NULL,zfinpurge_alt) ;
  static gref(zfinpurge,zfinpurge_def,bnfdebug,NULL,NULLP) ;
/*
  Now to work.  Initialise the reader, attempt to parse the command, and
  check for errors.
*/
  rdrinit() ;
  if (parse(dy_cmdchn,&zfinpurge,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zfinspec") ;
    return (FALSE) ; }
  optvals = (struct finpurge_struct *) result.g ;
  rdrclear() ;
/*
  Process the results. An empty command is taken as a request for the default
  value.
*/
  if (optvals->vars < 0 && optvals->cons < 0)
  { warn(246,rtnnme,"final variable deactivation",
	 (dyopts_dflt.finpurge.vars == TRUE)?"true":"false") ;
    warn(246,rtnnme,"final constraint deactivation",
	 (dyopts_dflt.finpurge.cons == TRUE)?"true":"false") ;
    return (TRUE) ; }
/*
  Something is specified. Check variables, then constraints.
*/
  if (optvals->vars >= 0)
  { main_lpopts->finpurge.vars = (bool) optvals->vars ;
    outfmt(dy_logchn,dy_gtxecho,"variables %s",
	   (main_lpopts->finpurge.vars == TRUE)?"true":"false") ; }
  if (optvals->cons >= 0)
  { main_lpopts->finpurge.cons = (bool) optvals->cons ;
    if (optvals->vars >= 0) outfmt(dy_logchn,dy_gtxecho,", ") ;
    outfmt(dy_logchn,dy_gtxecho,"constraints %s",
	   (main_lpopts->finpurge.cons == TRUE)?"true":"false") ; }

  FREE(optvals) ;

  return (TRUE) ; }



static bool lpctl_load  (void)

/*
  This routine processes the 'load' subcommand, which sets values that
  control how dylp populates the constraint system in a cold start.

  The bnf for the load subcommand is:
    <loadopt> ::= lpcontrol load [<fraction>] [<interval-LIST(,)>] ;
    <fraction> ::= <float>
    <interval> ::= <open-delim> <ub> <lb> <close-delim>
    <ub> ::= <float>
    <lb> ::= <float>
    <open-delim> ::= [ | )
    <close-delim> ::= [ | )
  By the time this routine is called, `lpcontrol load' is already parsed.

  If only the fraction is specified, the interval defaults are used. Similarly,
  if there's only an interval specification, the fraction default remains. But
  if only one interval is specified, the second interval is marked invalid. If
  you want two, you have to specify two.

  Parameters: none

  Returns: TRUE if the remainder of the command parses without error, FALSE
	   otherwise.
*/

{ struct interval_struct { struct interval_struct *nxt ;
			   char ldelim ;
			   double ub ;
			   double lb ;
			   char rdelim ; } ;
  struct interval_struct *intv,*temp ;
  struct load_struct { int frac_valid ;
		       double frac ;
		       struct interval_struct *intervals ; } ;
  struct load_struct *loadspec ;

  char intvstr[50] ;
  int intvndx,intvlen ;

  parse_any result ;

  char *rtnnme = "lpctl_load" ;


/*
  BNF for the load command.
*/
  static tdef(znil,bnfttNIL,NULL,NULL) ;
  static tref(znil_ref,znil,NULL,NULL) ;
  static tdef(zcomma,bnfttD,NULL,",") ;
  static tref(zcomma_ref,zcomma,NULL,NULL) ;
  static tdef(zlsq,bnfttD,NULL,"[") ;
  static tref(zlsq_ref,zlsq,NULL,NULL) ;
  static tdef(zlpar,bnfttD,NULL,"(") ;
  static tref(zlpar_ref,zlpar,NULL,NULL) ;
  static tdef(zrsq,bnfttD,NULL,"]") ;
  static tref(zrsq_ref,zrsq,NULL,NULL) ;
  static tdef(zrpar,bnfttD,NULL,")") ;
  static tref(zrpar_ref,zrpar,NULL,NULL) ;
  static tdef(zdnum,bnfttN,10,NULL) ;

  static comphd(zldelim_alt1) = { compcnt(1),mkcref(zlsq_ref) } ;
  static comphd(zldelim_alt2) = { compcnt(1),mkcref(zlpar_ref) } ;
  static althd(zldelim_alts) = { altcnt(2),
    mkaref(zldelim_alt1),mkaref(zldelim_alt2) } ;
  static pdef(zldelim,zldelim_alts) ;

  static comphd(zrdelim_alt1) = { compcnt(1),mkcref(zrsq_ref) } ;
  static comphd(zrdelim_alt2) = { compcnt(1),mkcref(zrpar_ref) } ;
  static althd(zrdelim_alts) = { altcnt(2),
    mkaref(zrdelim_alt1),mkaref(zrdelim_alt2) } ;
  static pdef(zrdelim,zrdelim_alts) ;

  static pref(zinterval_ldelim,zldelim,bnfstore|bnfexact,
	      mkoff(struct interval_struct,ldelim),NULLP) ;
  static tref(zinterval_ub,zdnum,bnfstore|bnfdbl,
	      mkoff(struct interval_struct,ub)) ;
  static tref(zinterval_lb,zdnum,bnfstore|bnfdbl,
	      mkoff(struct interval_struct,lb)) ;
  static pref(zinterval_rdelim,zrdelim,bnfstore|bnfexact,
	      mkoff(struct interval_struct,rdelim),NULLP) ;
  static comphd(zinterval_alt) = { compcnt(4),
    mkcref(zinterval_ldelim),
    mkcref(zinterval_ub),mkcref(zinterval_lb),
    mkcref(zinterval_rdelim) } ;

  static gdef(zinterval,sizeof(struct interval_struct),
	      mkoff(struct interval_struct,nxt),zinterval_alt) ;

  static tref(zfraction_dnum,zdnum,bnfstore|bnfdbl,
	      mkoff(struct load_struct,frac)) ;
  static idef(ziTRUE,TRUE) ;
  static iref(zfrac_valid,ziTRUE,mkoff(struct load_struct,frac_valid)) ;
  static comphd(zfraction_alt1) = { compcnt(2),
    mkcref(zfraction_dnum),mkcref(zfrac_valid) } ;
  static comphd(zfraction_alt2) = { compcnt(1),mkcref(znil_ref) } ;
  static althd(zfraction_alts) = { altcnt(2),
    mkaref(zfraction_alt1),mkaref(zfraction_alt2) } ;
  static pdef(zfraction,zfraction_alts) ;

  static pref(zloadbody_fraction,zfraction,NULL,NULL,NULLP) ;
  static gref(zloadbody_interval,zinterval,bnfstore|bnflst,
	      mkoff(struct load_struct,intervals),zcomma_ref) ;

  static comphd(zloadbody_alt1) = { compcnt(2),
    mkcref(zloadbody_fraction),mkcref(zloadbody_interval) } ;
  static comphd(zloadbody_alt2) = { compcnt(1),mkcref(zloadbody_fraction) } ;

  static althd(zloadbody_alts) = { altcnt(2),
    mkaref(zloadbody_alt1),mkaref(zloadbody_alt2) } ;
  static npdef(zloadbody,zloadbody_alts) ;
  static npref(zloadbody_ref,zloadbody,NULL,NULLP) ;

  static comphd(zload_alt) = { compcnt(1),mkcref(zloadbody_ref) } ;
  static gdef(zload_def,sizeof(struct load_struct),NULL,zload_alt) ;
  static gref(zload,zload_def,NULL,NULL,NULLP) ;
/*
  Now to work.  Initialise the reader, attempt to parse the command, and
  check for errors.
*/
  rdrinit() ;
  if (parse(dy_cmdchn,&zload,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zload") ;
    return (FALSE) ; }
  loadspec = (struct load_struct *) result.g ;
  rdrclear() ;
/*
  Process the results. An empty command is taken as a request for the default
  value.
*/
  if (loadspec->frac_valid == FALSE && loadspec->intervals == NULL)
  { warn(245,rtnnme,"initial load fraction",dyopts_dflt.initcons.frac) ;
    intvndx = 0 ;
    intvlen = sizeof(intvstr)-1 ;
    if (dyopts_dflt.initcons.i1uopen == TRUE)
      intvstr[intvndx] = '(' ;
    else
      intvstr[intvndx] = '[' ;
    intvndx++ ;
    intvndx += outfxd(&intvstr[intvndx],-(intvlen-intvndx),'l',"%.5f %.5f",
		      dyopts_dflt.initcons.i1u,dyopts_dflt.initcons.i1l) ;
    if (dyopts_dflt.initcons.i1lopen == TRUE)
      intvstr[intvndx] = ')' ;
    else
      intvstr[intvndx] = ']' ;
    intvndx++ ;
    if (dyopts_dflt.initcons.i2valid == TRUE)
    { if (dyopts_dflt.initcons.i2uopen == TRUE)
	intvstr[intvndx] = '(' ;
      else
	intvstr[intvndx] = '[' ;
      intvndx++ ;
      intvndx += outfxd(&intvstr[intvndx],-(intvlen-intvndx),'l',"%.5f %.5f",
			dyopts_dflt.initcons.i2u,dyopts_dflt.initcons.i2l) ;
      if (dyopts_dflt.initcons.i2lopen == TRUE)
	intvstr[intvndx] = ')' ;
      else
	intvstr[intvndx] = ']' ;
      intvndx++ ; }
    intvstr[intvndx] = '\0' ;
    warn(246,rtnnme,"load interval",intvstr) ;
    return (TRUE) ; }
/*
  The load fraction first.
*/
  if (loadspec->frac_valid == TRUE)
  { outfmt(dy_logchn,dy_gtxecho," %.2f",loadspec->frac) ;
    if (loadspec->frac < dyopts_lb.initcons.frac ||
	loadspec->frac > dyopts_ub.initcons.frac)
    { warn(244,rtnnme,dyopts_lb.initcons.frac,"initial load fraction",
	   dyopts_ub.initcons.frac,loadspec->frac,dyopts_ub.initcons.frac) ;
      main_lpopts->initcons.frac = dyopts_ub.initcons.frac ; }
    else
    { main_lpopts->initcons.frac = loadspec->frac ; } }
/*
  Now process the intervals, if present. For each interval, check the values
  and load main_lpopts->initcons.
*/
  intv = loadspec->intervals ;
  FREE(loadspec) ;
  if (intv == NULL) return (TRUE) ;

  outfmt(dy_logchn,dy_gtxecho," %c %.5f %.5f %c",
	 intv->ldelim,intv->ub,intv->lb,intv->rdelim) ;
  if (intv->ldelim == '(')
    main_lpopts->initcons.i1uopen = TRUE ;
  else
    main_lpopts->initcons.i1uopen = FALSE ;
  if (intv->ub > dyopts_ub.initcons.i1u || intv->ub < dyopts_lb.initcons.i1u)
  { warn(244,rtnnme,dyopts_lb.initcons.i1u,"initial load angle bound",
	   dyopts_ub.initcons.i1u,intv->ub,dyopts_ub.initcons.i1u) ;
    main_lpopts->initcons.i1u = dyopts_ub.initcons.i1u ; }
  else
  { main_lpopts->initcons.i1u = intv->ub ; }
  if (intv->lb > dyopts_ub.initcons.i1l || intv->lb < dyopts_lb.initcons.i1l)
  { warn(244,rtnnme,dyopts_lb.initcons.i1l,"initial load angle bound",
	   dyopts_ub.initcons.i1l,intv->lb,dyopts_lb.initcons.i1l) ;
    main_lpopts->initcons.i1l = dyopts_lb.initcons.i1l ; }
  else
  { main_lpopts->initcons.i1l = intv->lb ; }
  if (intv->rdelim == ')')
    main_lpopts->initcons.i1lopen = TRUE ;
  else
    main_lpopts->initcons.i1lopen = FALSE ;
/*
  If we have only one interval, we're done, otherwise repeat the whole thing
  for the second interval.
*/
  temp = intv->nxt ;
  FREE(intv) ;
  if (temp == NULL)
  { main_lpopts->initcons.i2valid = FALSE ;
    return (TRUE) ; }

  intv = temp ;
  outfmt(dy_logchn,dy_gtxecho,", %c %.5f %.5f %c",
	 intv->ldelim,intv->ub,intv->lb,intv->rdelim) ;
  if (intv->ldelim == '(')
    main_lpopts->initcons.i2uopen = TRUE ;
  else
    main_lpopts->initcons.i2uopen = FALSE ;
  if (intv->ub > dyopts_ub.initcons.i2u || intv->ub < dyopts_lb.initcons.i2u)
  { warn(244,rtnnme,dyopts_lb.initcons.i2u,"initial load angle bound",
	   dyopts_ub.initcons.i2u,intv->ub,dyopts_ub.initcons.i2u) ;
    main_lpopts->initcons.i2u = dyopts_ub.initcons.i2u ; }
  else
  { main_lpopts->initcons.i2u = intv->ub ; }
  if (intv->lb > dyopts_ub.initcons.i2l || intv->lb < dyopts_lb.initcons.i2l)
  { warn(244,rtnnme,dyopts_lb.initcons.i2l,"initial load angle bound",
	   dyopts_ub.initcons.i2l,intv->lb,dyopts_lb.initcons.i2l) ;
    main_lpopts->initcons.i2l = dyopts_lb.initcons.i2l ; }
  else
  { main_lpopts->initcons.i2l = intv->lb ; }
  if (intv->rdelim == ')')
    main_lpopts->initcons.i2lopen = TRUE ;
  else
    main_lpopts->initcons.i2lopen = FALSE ;

  FREE(intv) ;

  return (TRUE) ; }



static bool lpctl_infinity (void)

/*
  This routine handles the `lpcontrol infinity' command. The reason it's broken
  out is so that we can use the strings IEEE and DBL_MAX to specify the most
  common values for infinite and finite infinity. The syntax will also accept
  an arbitrary (double) value.

  The bnf for the infinity subcommand is:
    <loadopt> ::= lpcontrol infinity [<value>]
    <value> ::= IEEE | DBL_MAX | <double>
  By the time this routine is called, `lpcontrol infinity' is already parsed.
  A value of 0 is taken as a request for the current value.

  Parameters: none

  Returns: TRUE if the remainder of the command parses without error, FALSE
	   otherwise.
*/

{ struct infinity_struct { int code ;
			   double val ; } ;
  struct infinity_struct *infinityspec ;

  double infinity ;

  parse_any result ;

  char *rtnnme = "lpctl_infinity" ;


/*
  BNF for the infinity command.
*/
  static tdef(zIEEE,bnfttID,NULL,"IEEE") ;
  static tref(zIEEE_ref,zIEEE,bnfmin,NULL) ;
  static idef(ziOne,1) ;
  static tdef(zDBLMAX,bnfttID,NULL,"DBL_MAX") ;
  static tref(zDBLMAX_ref,zDBLMAX,bnfmin,NULL) ;
  static idef(ziTwo,2) ;
  static tdef(zdnum,bnfttN,10,NULL) ;
  static idef(ziThree,3) ;

  static iref(zinfbody_IEEE,ziOne,mkoff(struct infinity_struct,code)) ;
  static comphd(zinfbody_alt1) = { compcnt(2),
    mkcref(zIEEE_ref),mkcref(zinfbody_IEEE) } ;

  static iref(zinfbody_DBLMAX,ziTwo,mkoff(struct infinity_struct,code)) ;
  static comphd(zinfbody_alt2) = { compcnt(2),
    mkcref(zDBLMAX_ref),mkcref(zinfbody_DBLMAX) } ;

  static iref(zinfbody_dbl,ziThree,mkoff(struct infinity_struct,code)) ;
  static tref(zinfbody_dblval,zdnum,bnfstore|bnfdbl,
	      mkoff(struct infinity_struct,val)) ;
  static comphd(zinfbody_alt3) = { compcnt(2),
    mkcref(zinfbody_dblval),mkcref(zinfbody_dbl) } ;

  static althd(zinfbody_alts) = { altcnt(3),
    mkaref(zinfbody_alt1),mkaref(zinfbody_alt2), mkaref(zinfbody_alt3) } ;
  static npdef(zinfbody,zinfbody_alts) ;
  static npref(zinfbody_ref,zinfbody,NULL,NULLP) ;

  static comphd(zinfinitydef_alt) = { compcnt(1),mkcref(zinfbody_ref) } ;
  static gdef(zinfinity_def,
	      sizeof(struct infinity_struct),NULL,zinfinitydef_alt) ;
  static gref(zinfinity,zinfinity_def,NULL,NULL,NULLP) ;
/*
  Now to work.  Initialise the reader, attempt to parse the command, and
  check for errors.
*/
  rdrinit() ;
  if (parse(dy_cmdchn,&zinfinity,&result) == FALSE)
  { rdrclear() ;
    errmsg(240,rtnnme,"zinfinity") ;
    return (FALSE) ; }
  infinityspec = (struct infinity_struct *) result.g ;
  rdrclear() ;
/*
  Process the results. A value of 0 is taken as a request for the default.
  Note that we expect HUGE_VAL to be IEEE infinity. We get the default value
  from main_lptols, because the default isn't set until we call dy_defaults.
*/
  switch (infinityspec->code)
  { case 1:
    { outfmt(dy_logchn,dy_gtxecho," IEEE (%g)",HUGE_VAL) ;
      infinity = HUGE_VAL ;
      if (finite(infinity))
      { warn(314,rtnnme,infinity) ; }
      break ; }
    case 2:
    { outfmt(dy_logchn,dy_gtxecho," DBL_MAX (%g)",DBL_MAX) ;
      infinity = DBL_MAX ;
      break ; }
    case 3:
    { outfmt(dy_logchn,dy_gtxecho," %g",infinityspec->val) ;
      infinity = infinityspec->val ;
      if (infinity == 0)
      { infinity = main_lptols->inf ;
	warn(245,rtnnme,"infinity",infinity) ; }
      else
      if (infinity < 0)
      { errmsg(242,rtnnme,infinity,"infinity") ;
	infinity = dytols_dflt.inf ; }
      break ; }
    default:
    { FREE(infinityspec) ;
      return (FALSE) ; } }

  main_lptols->inf = infinity ;
  FREE(infinityspec) ;

  return (TRUE) ; }




cmd_retval dy_ctlopt (const char *keywd)

/*
  This routine handles various control options, each taking a single
  parameter (TRUE/FALSE, a number, etc.) If the routine can't understand
  the option, it returns an error.

  The bnf for the control option command is:
    <ctlopt> ::= lpcontrol <what> <value>
    <what> ::= actconlim | actconlvl | active | actvarlim | bogus |
	       check | cold | coldbasis | coldvars | costz |
	       dchk | deactconlvl | degen | degenlite | degenpivs | dfeas |
	       dualacttype | dualmultipiv |
	       factor | final | forcecopy | fullsys |
	       groom | idle | infinity | iters | load |
	       patch | pchk | pfeas | pivot | primmultipiv |
	       purgecon | purgevar | reframe |
	       scaling | scan | swing | usedual | warm | zero
    <value> ::= <bool> | <integer> | <double> | <string>
  
  The tolerances bogus, costz, dchk, pchk, pivot, purgecon, purgevar, reframe,
  swing, and zero expect a double. Infinity is a bit more complicated, and
  further parsing is handled by its own routine.

  The options cold, degen, forcecopy, fullsys, patch, usedual, and warm expect
  a boolean. Coldbasis, deactconlvl, degenlite, and groom expect
  a string from the appropriate list of defined keywords (see below), and the
  others expect an integer.

  Active, final, and load are more complicated, and are handled by private
  parsing routines.

  Parameters:
    keywd:	The command keyword

  Returns: cmdOK or cmdHALTERROR
*/

{ char *what,cmdstr[50] ;
  int code,intdflt,intlb,intub,*intopt,numkwds ;
  double *toler,tolerdflt ;
  double dblopt ;
  bool booldflt,*boolopt ;
  keytab_entry *kwds ;
  cmd_retval retval ;
  char *rtnnme = "dy_ctlopt" ;

  extern bool string_opt(char **str),		/* options.c */
	      integer_opt(int *iloc),
	      bool_opt(bool *bloc),
	      double_opt(double *rloc) ;

/*
  A lookup table with the various <what> keywords recognised by the control
  command.
*/

  enum ctlcodes { ctlINV = 0, ctlACTIVESZE, ctlADDVARLIM,
		  ctlBOGUS,
		  ctlCHECK, ctlCOLD, ctlCOLDBASIS, ctlCOLDVARS,
		  ctlCONACTLIM, ctlCONACTLVL, ctlCONDEACTLVL,
		  ctlCPYORIG, ctlCOSTZ,
		  ctlDCHK, ctlDEGEN, ctlDEGENLITE, ctlDEGENPIVS, ctlDFEAS,
		  ctlDUALADD, ctlDUALMULTIPIV,
		  ctlFACTOR, ctlFINAL, ctlFULLSYS, ctlGROOM,
		  ctlINFINITY, ctlITER, ctlIDLE, ctlLOAD,
		  ctlPATCH, ctlPCHK, ctlPFEAS,
		  ctlPIVOT, ctlPRIMMULTIPIV,
		  ctlPURGE, ctlPURGEVAR, ctlREFRAME,
		  ctlSCALING, ctlSCAN, ctlSWING,
		  ctlUSEDUAL, ctlWARM, ctlZERO } ctlcode ;

  static keytab_entry ctlkwds[] = { { "actconlim", 8, (int) ctlCONACTLIM },
				    { "actconlvl", 8, (int) ctlCONACTLVL },
				    { "active", 4, (int) ctlACTIVESZE },
				    { "actvarlim", 4, (int) ctlADDVARLIM },
				    { "antidegen", 2, (int) ctlDEGEN },
				    { "bogus", 1, (int) ctlBOGUS },
				    { "check", 2, (int) ctlCHECK },
				    { "cold", 5, (int) ctlCOLD },
				    { "coldbasis", 5, (int) ctlCOLDBASIS },
				    { "coldvars", 5, (int) ctlCOLDVARS },
				    { "costz", 3, (int) ctlCOSTZ },
				    { "dchk", 2, (int) ctlDCHK },
				    { "deactconlvl", 3, (int) ctlCONDEACTLVL },
				    { "degenlite", 6, (int) ctlDEGENLITE },
				    { "degenpivs", 6, (int) ctlDEGENPIVS },
				    { "dfeas", 2, (int) ctlDFEAS },
				    { "dualacttype", 5, (int) ctlDUALADD },
				    { "dualmultipiv", 5,
				      (int) ctlDUALMULTIPIV },
				    { "factor", 2, (int) ctlFACTOR },
				    { "final", 2, (int) ctlFINAL },
				    { "forcecopy", 2, (int) ctlCPYORIG },
				    { "fullsys", 2, (int) ctlFULLSYS },
				    { "groom", 1, (int) ctlGROOM },
				    { "idle", 2, (int) ctlIDLE },
				    { "infinity", 2, (int) ctlINFINITY },
				    { "iters", 2, (int) ctlITER },
				    { "load", 1, (int) ctlLOAD },
				    { "patch", 2, (int) ctlPATCH },
				    { "pchk", 2, (int) ctlPCHK },
				    { "pfeas", 2, (int) ctlPFEAS },
				    { "pivot", 2, (int) ctlPIVOT },
				    { "primmultipiv", 5,
				      (int) ctlPRIMMULTIPIV },
				    { "purgecon", 6, (int) ctlPURGE },
				    { "purgevar", 6, (int) ctlPURGEVAR },
				    { "reframe", 1, (int) ctlREFRAME },
				    { "scaling", 4, (int) ctlSCALING },
				    { "scan", 4, (int) ctlSCAN },
				    { "swing", 2, (int) ctlSWING },
				    { "usedual", 1, (int) ctlUSEDUAL },
				    { "warm", 1, (int) ctlWARM },
				    { "zero", 1, (int) ctlZERO }
				   } ;

  static int numctlkwds = (sizeof ctlkwds/sizeof (keytab_entry)) ;

/*
  Keywords used by the groom option; the option value is used only by the
  groombasis routine, so there's no real motivation for symbolic codes.
*/

  static keytab_entry groomkwds[] = { { "abort", 1, 2 },
				      { "silent", 1, 0 },
				      { "warn", 1, 1 } } ;

  static int numgroomkwds = (sizeof groomkwds/sizeof (keytab_entry)) ;

/*
  Keywords used by the coldbasis option; the option value is used only by the
  ib_populatebasis routine, so there's no real motivation for symbolic codes.
*/

  static keytab_entry basiskwds[] = { { "architectural", 1, 3 },
				      { "logical", 1, 1 },
				      { "slack", 1, 2 } } ;

  static int numbasiskwds = (sizeof basiskwds/sizeof (keytab_entry)) ;

/*
  Keywords used by the degenlite option; the option value is used only by
  the primalout and dualin routines, so there's no real motivation for
  symbolic codes.
*/

  static keytab_entry litekwds[] = { { "alignedge", 6, 3 },
				     { "alignobj", 6, 2 },
				     { "perpedge", 5, 5 },
				     { "perpobj", 5, 4 },
				     { "pivot", 6, 1 },
				     { "pivotabort", 6, 0 } } ;

  static int numlitekwds = (sizeof litekwds/sizeof (keytab_entry)) ;

/*
  Keywords used by the deactconlvl option; the option value is used only by
  the dy_purgecon routine, so there's no real motivation for symbolic codes.
*/
  
  static keytab_entry deactkwds[] = { { "aggressive", 1, 1 },
				      { "fanatic", 1, 2 },
				      { "normal", 1, 0 } } ;

  static int numdeactkwds = (sizeof deactkwds/sizeof (keytab_entry)) ;

/*
  Initialisation, so gcc doesn't complain.
*/
  kwds = NULL ;
  numkwds = 0 ;
  intdflt = -INT_MAX ;
  intlb = INT_MAX ;
  intub = -INT_MAX ;
  intopt = NULL ;
  toler = NULL ;
  tolerdflt = quiet_nan(0) ;
  boolopt = NULL ;
/*
  Now to work. Parse off the <what> keyword and see if we can look it up.
*/
  ctlcode = ctlINV ;
  if (string_opt(&what) == TRUE)
  { code = ambig(what,ctlkwds,numctlkwds) ;
    if (code < 0) 
    { if (code < -1)
	errmsg(233,rtnnme,what) ;
      else
	errmsg(234,rtnnme,what) ; }
    else
      ctlcode = (enum ctlcodes) code ; }
/*
  Set the various variables for each command. There are a few exceptions to the
  pattern, down at the end of the switch, which call command-specific routines
  for more complicated processing.
*/
  outfxd(cmdstr,-((int) (sizeof(cmdstr)-1)),'l',"%s %s",keywd,what) ;
  switch (ctlcode)
  { case ctlADDVARLIM:
    { intopt = &main_lpopts->addvar ;
      intdflt = dyopts_dflt.addvar ;
      intlb = dyopts_lb.addvar ;
      intub = dyopts_ub.addvar ;
      break ; }
    case ctlBOGUS:
    { toler = &main_lptols->bogus ;
      tolerdflt = dytols_dflt.bogus ;
      break ; }
    case ctlCHECK:
    { intopt = &main_lpopts->check ;
      intdflt = dyopts_dflt.check ;
      intlb = dyopts_lb.check ;
      intub = dyopts_ub.check ;
      break ; }
    case ctlCOLD:
    { boolopt = &main_lpopts->forcecold ;
      booldflt = dyopts_dflt.forcecold ;
      break ; }
    case ctlCOLDBASIS:
    { intopt = &main_lpopts->initbasis ;
      intdflt = dyopts_dflt.initbasis ;
      intlb = dyopts_lb.initbasis ;
      intub = dyopts_ub.initbasis ;
      numkwds = numbasiskwds ;
      kwds = basiskwds ;
      break ; }
    case ctlCOLDVARS:
    { intopt = &main_lpopts->coldvars ;
      intdflt = dyopts_dflt.coldvars ;
      intlb = dyopts_lb.coldvars ;
      intub = dyopts_ub.coldvars ;
      break ; }
    case ctlCONACTLIM:
    { intopt = &main_lpopts->con.actlim ;
      intdflt = dyopts_dflt.con.actlim ;
      intlb = dyopts_lb.con.actlim ;
      intub = dyopts_ub.con.actlim ;
      break ; }
    case ctlCONACTLVL:
    { intopt = &main_lpopts->con.actlvl ;
      intdflt = dyopts_dflt.con.actlvl ;
      intlb = dyopts_lb.con.actlvl ;
      intub = dyopts_ub.con.actlvl ;
      break ; }
    case ctlCOSTZ:
    { toler = &main_lptols->cost ;
      tolerdflt = dytols_dflt.cost ;
      break ; }
    case ctlCPYORIG:
    { boolopt = &main_lpopts->copyorigsys ;
      booldflt = dyopts_dflt.copyorigsys ;
      break ; }
    case ctlDCHK:
    { toler = &main_lptols->dchk ;
      tolerdflt = dytols_dflt.dchk ;
      break ; }
    case ctlDEGEN:
    { boolopt = &main_lpopts->degen ;
      booldflt = dyopts_dflt.degen ;
      break ; }
    case ctlDEGENLITE:
    { intopt = &main_lpopts->degenlite ;
      intdflt = dyopts_dflt.degenlite ;
      intlb = dyopts_lb.degenlite ;
      intub = dyopts_ub.degenlite ;
      numkwds = numlitekwds ;
      kwds = litekwds ;
      break ; }
    case ctlDEGENPIVS:
    { intopt = &main_lpopts->degenpivlim ;
      intdflt = dyopts_dflt.degenpivlim ;
      intlb = dyopts_lb.degenpivlim ;
      intub = dyopts_ub.degenpivlim ;
      break ; }
    case ctlDFEAS:
    { toler = &main_lptols->dfeas_scale ;
      tolerdflt = dytols_dflt.dfeas_scale ;
      break ; }
    case ctlDUALADD:
    { intopt = &main_lpopts->dualadd ;
      intdflt = dyopts_dflt.dualadd ;
      intlb = dyopts_lb.dualadd ;
      intub = dyopts_ub.dualadd ;
      break ; }
    case ctlFACTOR:
    { intopt = &main_lpopts->factor ;
      intdflt = dyopts_dflt.factor ;
      intlb = dyopts_lb.factor ;
      intub = dyopts_ub.factor ;
      break ; }
    case ctlFULLSYS:
    { boolopt = &main_lpopts->fullsys ;
      booldflt = dyopts_dflt.fullsys ;
      break ; }
    case ctlGROOM:
    { intopt = &main_lpopts->groom ;
      intdflt = dyopts_dflt.groom ;
      intlb = dyopts_lb.groom ;
      intub = dyopts_ub.groom ;
      numkwds = numgroomkwds ;
      kwds = groomkwds ;
      break ; }
    case ctlITER:
    { intopt = &main_lpopts->iterlim ;
      intdflt = dyopts_dflt.iterlim ;
      intlb = dyopts_lb.iterlim ;
      intub = dyopts_ub.iterlim ;
      break ; }
    case ctlIDLE:
    { intopt = &main_lpopts->idlelim ;
      intdflt = dyopts_dflt.idlelim ;
      intlb = dyopts_lb.idlelim ;
      intub = dyopts_ub.idlelim ;
      break ; }
    case ctlDUALMULTIPIV:
    { intopt = &main_lpopts->dpsel.strat ;
      intdflt = dyopts_dflt.dpsel.strat ;
      intlb = dyopts_lb.dpsel.strat ;
      intub = dyopts_ub.dpsel.strat ;
      break ; }
    case ctlPRIMMULTIPIV:
    { intopt = &main_lpopts->ppsel.strat ;
      intdflt = dyopts_dflt.ppsel.strat ;
      intlb = dyopts_lb.ppsel.strat ;
      intub = dyopts_ub.ppsel.strat ;
      break ; }
    case ctlPATCH:
    { boolopt = &main_lpopts->patch ;
      booldflt = dyopts_dflt.patch ;
      break ; }
    case ctlPCHK:
    { toler = &main_lptols->pchk ;
      tolerdflt = dytols_dflt.pchk ;
      break ; }
    case ctlPFEAS:
    { toler = &main_lptols->pfeas_scale ;
      tolerdflt = dytols_dflt.pfeas_scale ;
      break ; }
    case ctlPIVOT:
    { toler = &main_lptols->pivot ;
      tolerdflt = dytols_dflt.pivot ;
      break ; }
    case ctlPURGE:
    { toler = &main_lptols->purge ;
      tolerdflt = dytols_dflt.purge ;
      break ; }
    case ctlCONDEACTLVL:
    { intopt = &main_lpopts->con.deactlvl ;
      intdflt = dyopts_dflt.con.deactlvl ;
      intlb = dyopts_lb.con.deactlvl ;
      intub = dyopts_ub.con.deactlvl ;
      numkwds = numdeactkwds ;
      kwds = deactkwds ;
      break ; }
    case ctlPURGEVAR:
    { toler = &main_lptols->purgevar ;
      tolerdflt = dytols_dflt.purgevar ;
      break ; }
    case ctlREFRAME:
    { toler = &main_lptols->reframe ;
      tolerdflt = dytols_dflt.reframe ;
      break ; }
    case ctlSCALING:
    { intopt = &main_lpopts->scaling ;
      intdflt = dyopts_dflt.scaling ;
      intlb = dyopts_lb.scaling ;
      intub = dyopts_ub.scaling ;
      break ; }
    case ctlSCAN:
    { intopt = &main_lpopts->scan ;
      intdflt = dyopts_dflt.scan ;
      intlb = dyopts_lb.scan ;
      intub = dyopts_ub.scan ;
      break ; }
    case ctlSWING:
    { toler = &main_lptols->swing ;
      tolerdflt = dytols_dflt.swing ;
      break ; }
    case ctlUSEDUAL:
    { boolopt = &main_lpopts->usedual ;
      booldflt = dyopts_dflt.usedual ;
      break ; }
    case ctlWARM:
    { boolopt = &main_lpopts->forcewarm ;
      booldflt = dyopts_dflt.forcewarm ;
      break ; }
    case ctlZERO:
    { toler = &main_lptols->zero ;
      tolerdflt = dytols_dflt.zero ;
      break ; }
    case ctlACTIVESZE:
    { booldflt = lpctl_active() ;
      if (booldflt == TRUE)
      { return (cmdOK) ; }
      else
      { return (cmdHALTERROR) ; } }
    case ctlINFINITY:
    { booldflt = lpctl_infinity() ;
      if (booldflt == TRUE)
      { return (cmdOK) ; }
      else
      { return (cmdHALTERROR) ; } }
    case ctlLOAD:
    { booldflt = lpctl_load() ;
      if (booldflt == TRUE)
      { return (cmdOK) ; }
      else
      { return (cmdHALTERROR) ; } }
    case ctlFINAL:
    { booldflt = lpctl_finpurge() ;
      if (booldflt == TRUE)
      { return (cmdOK) ; }
      else
      { return (cmdHALTERROR) ; } }
    default:
    { errmsg(236,rtnnme,"<what>","keyword",keywd) ;
      STRFREE(what) ;
      return (cmdHALTERROR) ; } }
/*
  Last but not least, the actual work. For an integer option or a tolerance,
  a negative value is taken as a request from the user to be told the default
  value. Further, for tolerances, 0 is not acceptable. For coldvars and factor,
  gripe about a violation of the upper bound, but don't enforce it. An upper
  bound of -1 is `no upper bound' for the integer-valued options.
  
  Assume the worst, so that we have to explicitly set a successful return code.
*/
  retval = cmdHALTERROR ;
  switch (ctlcode)
  { case ctlADDVARLIM:
    case ctlCHECK:
    case ctlCONACTLIM:
    case ctlCONACTLVL:
    case ctlDEGENPIVS:
    case ctlITER:
    case ctlIDLE:
    case ctlSCALING:
    case ctlSCAN:
    { if (integer_opt(intopt) == TRUE)
      { if (*intopt >= 0)
	{ if (intub > 0 && *intopt > intub)
	  { warn(241,rtnnme,intlb,cmdstr,intub,*intopt,intub) ;
	    *intopt = intub ; } }
	else
	{ warn(243,rtnnme,cmdstr,intdflt) ; }
	retval = cmdOK ; }
      else
      { errmsg(236,rtnnme,"<integer>","parameter",keywd) ; }
      break ; }
    case ctlCOLDVARS:
    case ctlDUALADD:
    case ctlFACTOR:
    case ctlDUALMULTIPIV:
    case ctlPRIMMULTIPIV:
    { if (integer_opt(intopt) == TRUE)
      { if (*intopt >= 0)
	{ if (intub > 0 && *intopt > intub)
	  { warn(241,rtnnme,intlb,cmdstr,intub,*intopt,intub) ; } }
	else
	{ warn(243,rtnnme,cmdstr,intdflt) ; }
	retval = cmdOK ; }
      else
      { errmsg(236,rtnnme,"<integer>","parameter",keywd) ; }
      break ; }
    case ctlCOLD:
    case ctlCPYORIG:
    case ctlDEGEN:
    case ctlFULLSYS:
    case ctlPATCH:
    case ctlUSEDUAL:
    case ctlWARM:
    { if (bool_opt(boolopt) == TRUE)
      { retval = cmdOK ; }
      else
      { errmsg(236,rtnnme,"<bool>","parameter",keywd) ; }
      break ; }
    case ctlBOGUS:
    case ctlCOSTZ:
    case ctlDCHK:
    case ctlDFEAS:
    case ctlPCHK:
    case ctlPFEAS:
    case ctlPIVOT:
    case ctlPURGE:
    case ctlPURGEVAR:
    case ctlREFRAME:
    case ctlSWING:
    case ctlZERO:
    { if (double_opt(&dblopt) == TRUE)
      { if (dblopt <= 0)
	{ warn(245,rtnnme,cmdstr,tolerdflt) ; }
	else
	{ *toler = dblopt ; }
	retval = cmdOK ; }
      else
      { errmsg(236,rtnnme,"<double>","parameter",keywd) ; }
      break ; }
    case ctlGROOM:
    case ctlCOLDBASIS:
    case ctlDEGENLITE:
    case ctlCONDEACTLVL:
    { if (string_opt(&what) == TRUE)
      { code = ambig(what,kwds,numkwds) ;
	if (code < 0)
	{ if (code < -1)
	    errmsg(233,rtnnme,what) ;
	  else
	    errmsg(234,rtnnme,what) ; }
	else
	{ *intopt = code ;
	  retval = cmdOK ; } }
      else
      { errmsg(236,rtnnme,"<string>","parameter",cmdstr) ; }
      break ; }
    default:
    { errmsg(1,rtnnme,__LINE__) ;
      break ; } }

  STRFREE(what) ;

  return (retval) ; }

