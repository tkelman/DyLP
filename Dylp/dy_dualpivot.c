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
  This file contains the routines which select entering and leaving variables
  for the dual simplex, and perform a dual pivot. See the comments at the head
  of dy_dual.c for an overview of the dual simplex algorithm implemented here.

  Some words about anti-degen lite in the dual context.

  Analogous to the primal case, the notion is that we want to choose leaving
  dual variables based on the alignment of the hyperplane coming tight with
  the pivot, relative to the direction we want to go.  One candidate for
  `direction we want to go' is the dual objective.  Another good candidate is
  the dual direction of motion.  But we're running dual simplex in the primal
  data structure and handling upper and lower bounds on the primal variables
  algorithmically, and that makes the explanation of why it all works just a
  little tricky.

  Architecturals nonbasic at bound represent tight constraints of the form
  x<k> <= u<k> or -x<k> <= -l<k>. There should be nonzero duals associated with
  these constraints, and we need to account for them when formulating dual
  constraints and when determining alignment to the dual objective, but
  carefully keep our head in the sand when it comes to alignment with the
  direction of motion. The unfortunate fact of the matter is that there's
  just one column for x<k> in the primal nonbasic partition, but it can
  represent any of three dual variables, depending on your point of view.
  Fortunately, it only needs to be one of them at any one time.

  We need to account for the coefficients of any finite upper and lower
  bounds. The system we want is really A'x ? b', which expands to
     Ax  =  b
    -Ix <= -l
     Ix <=  u
  where x has expanded to include slacks as well as the original
  architectural variables, and -Ix <= -l contains explicit lower bound
  constraints for the slacks as well as the original architectural
  variables.  All primal variables are `free' variables with explicit bound
  constraints.

  We're interested in y'a'<k> = [y w v][a<k> -e<k> e<k>]. The dual constraint
  is the equality ya<k> - w<k> + v<k> = -c<k>.  The objective is augmented to
  become y'b', where b' = [b -l u] (the order of l and u isn't important, as
  long as we choose one and stick with it). The dual variables y are free
  variables (to match the equalities Ax = b). (But note that the columns for
  the slack variables introduce explicit dual constraints y<k> - w<k> = 0.
  These serve to enforce the usual y >= 0. I'll leave it to you to extend
  the interpretation to upper-bounded slacks for range constraints.)

  Now comes the part where we abridge the explanation (see the accompanying
  technical doc'n for the gory details). Clearly a primal variable can be
  in one of three states: at its lower bound, at its upper bound, or in
  between. Interpreting ya<k> - w<k> + v<k> = -c<k> in this light, we have:

    Primal 	       w	 v	  constraint
   x<k> = lb<k>	    nonzero	zero	  ya<k> - w<k> = 0 => ya<k> >= c<k>
   x<k> = ub<k>	     zero      nonzero	  ya<k> + v<k> = 0 => ya<k> <= c<k>
     between	     zero	zero	  ya<k> = c<k>
 
  So, for a dual pivot where w<k> decreases to 0 and goes nonbasic (x<k>
  increases from lb<k> to some intermediate value), the dual inequality
  ya<k> >= 0 is made tight. When v<k> decreases to 0 and goes nonbasic
  (x<k> decreases from ub<k>), the dual inequality ya<k> <= 0 is made tight.
  While x<k> is strictly between bounds, ya<k> = c<k> must hold.

  What's the dual direction of motion, zeta<k>? The portion matching the
  basic dual variables (all of y, those w for variables at lb, those v for
  variables at ub) is the vector [ inv(B) inv(B)N ]; there's another chunk
  for the nonbasic variables. Again, you should see the accompanying
  technical doc'n for details. The upshot of all the math is that
  dot(zeta<i>,a<j>) = abar<ij> for x<i> leaving to become NBLB, and -abar<ij>
  for x<i> leaving to become NBUB.

  How do we calculate alignment with the objective?  Since we're minimising,
  we want to head in the direction -b'. The direction we attribute to the
  normal of the dual constraint depends on the type of pivot.

  All of the above translates nicely into the real world of revised simplex
  with implicit primal bounds and dual variables with implicit lower bounds
  of 0. Again, it's in the accompanying documentation. Probably the least
  obvious point is that a shortage of coefficients of appropriate sign
  (remember, one column serves for three!) results in the v's taking on
  negative values. Translating from primal space to dual space and back again
  still makes my head hurt, though I'm getting better at it. Chant this
  mantra:  nonbasic primal architectural variables match up with basic w or v
  dual variables, and nonbasic primal slack variables match up with basic y
  dual variables.
*/

#define DYLP_INTERNAL

#include "bonsai.h"
#include "dylp.h"

static char sccsid[] UNUSED = "@(#)dy_dualpivot.c	4.7	10/15/05" ;

/*
  Define this symbol to enable a thorough check of the updates to cbar and
  rho, but be aware that it'll almost certainly trigger fatal errors if
  the LP is numerically ill-conditioned.

  #define CHECK_DSE_UPDATES
*/


#if defined(DYLP_STATISTICS) || !defined(NDEBUG)

/*
  Pivot counting structure for the antidegeneracy mechanism. This structure
  is sufficient for simple debugging; more complicated stats are collected
  when DYLP_STATISTICS is defined. DYSTATS_MAXDEGEN is defined in dylp.h.

  Field		Definition
  -----		----------
  iterin	iterin[i] is the value of tot.pivs when degeneracy level i was
		entered.
*/

typedef struct { int iterin[DYSTATS_MAXDEGEN] ; } degenstats_struct ;
static degenstats_struct degenstats ;

#endif		/* DYLP_STATISTICS || !NDEBUG */



dyret_enum dy_confirmDualPivot (int i, int j, double *abari,
				double maxabari, double **p_abarj)
/*
  The routines that select the entering variable x<j> for a dual pivot use
  the row vector abar<i> when evaluating candidates. Abar<i> is calculated
  as dot(beta<i>,a<k>), k = 1, ..., n, by dualpivrow. See dualpivrow
  for an explanation of why dylp takes this approach.
  
  This routine calculates the column vector abar<j> = inv(B)a<j> and then
  compares the two values obtained for the pivot abar<ij>.

  The reason we need this routine is that numerical inaccuracy happens. In
  particular, we're looking to avoid the situation where the value from
  abar<i> appears suitable, but the value from abar<j> is not suitable. The
  value from abar<j> more accurately reflects calculation in the basis
  package, and if it's no good, the basis update is liable to fail. For dual
  multipivot, where we may flip variables prior to the final pivot, this is a
  sure fire way to lose dual feasibility. Hence this routine, which is called
  by both dualmultiin and dualin.

  There's no real loss in terms of efficiency, because we can instruct the
  basis package to remember the intermediate result from the calculation of
  inv(B)a<j> that it will subsequently use for the basis update.

  Returning dyrMADPIV for the case where the values of abar<ij> do not agree
  and the basis was just refactored doesn't address the numeric problem, but
  it will get x<i> onto the rejected pivot list, and we can hope that some
  other x<i> will result in a different choice of x<j>.

  Parameters:
    i:		The index of the variable x<i> selected to leave.
    j:		The index of the variable x<j> selected to enter.
    abari:	The dual pivot row, calculated as dot(beta<i>,a<k>),
		k = 1, ..., n
    maxabari:	The maximum absolute value in abari.
    p_abarj:	(o) used to return abar<j>

  Returns: dyrOK if the values agree.
	   dyrREQCHK if the values differ and there's been at least one
		pivot since the last refactor of the basis. At least we
		can try to fix the problem.
	   dyrMADPIV if the values agree and are honestly mad, or if the
		values disagree and the value from abar<j> is mad.
	   dyrFATAL if the calculation fails, or (paranoid only) if the
	       difference is large
*/

{ int xipos ;
  double *abarj ;
  double abari_j,abarj_i,tol ;
  dyret_enum retval ;

# ifndef NDEBUG
  int cnt,xkpos ;
# endif

  char *rtnnme = "confirmDualPivot" ;

  retval = dyrINV ;

  *p_abarj = NULL ;
  xipos = dy_var2basis[i] ;
  abari_j = abari[j] ;

/*
  Fetch a<j> and calculate abar<j> = inv(B)a<j>.
*/
  abarj = NULL ;
  if (consys_getcol_ex(dy_sys,j,&abarj) == FALSE)
  { errmsg(122,rtnnme,dy_sys->nme,
	   "column",consys_nme(dy_sys,'v',j,TRUE,NULL),j) ;
    if (abarj != NULL) FREE(abarj) ;
    return (dyrFATAL) ; }

# ifndef NDEBUG
  if (dy_opts->print.pivoting >= 4)
  { outfmt(logchn,gtxecho,"\n\tentering column a<%d>:",j) ;
    cnt = 1 ;
    for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
    { if (abarj[xkpos] == 0) continue ;
      cnt = (cnt+1)%2 ;
      if (cnt == 0) outchr(logchn,gtxecho,'\n') ;
      outfmt(logchn,gtxecho,"\ta<%d,%d> = %g",xkpos,j,abarj[xkpos]) ; } }
# endif

  dy_ftran(abarj,TRUE) ;
  abarj_i = abarj[xipos] ;

# ifndef NDEBUG
  if (dy_opts->print.pivoting >= 4)
  { outfmt(logchn,gtxecho,"\n\tentering column abar<%d> = inv(B)a<%d>:",j,j) ;
    cnt = 1 ;
    for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
    { if (abarj[xkpos] == 0) continue ;
      cnt = (cnt+1)%2 ;
      if (cnt == 0) outchr(logchn,gtxecho,'\n') ;
      outfmt(logchn,gtxecho,"\ta<%d,%d> = %g",xkpos,j,abarj[xkpos]) ; } }
# endif

/*
  Well, are we equal? If so, we can simply check abarj_i. Scaling can tighten
  tols.zero, which we don't want here, so hardwire the default zero tolerance.
*/
  tol = 1.0e-11*(1+fabs(abarj_i)) ;
  if (withintol(abarj_i,abari_j,tol))
  { if (dy_chkpiv(abarj_i,maxabari) < 1.0)
    { retval = dyrMADPIV ; }
    else
    { retval = dyrOK ; } }
/*
  Nope, not equal. If we're paranoid, print a warning.
*/
  else
  { 
#   ifdef PARANOIA
    if (!withintol(abarj_i,abari_j,1000*tol))
    { warn(385,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	   dy_lp->tot.iters+1,i,j,abari_j,abarj_i,
	   fabs(abarj_i-abarj_i),tol) ; }
#   endif
/*
  The return value should be REQCHK if we've pivoted since the last refactor.
  Otherwise, put x<i> on the reject list.
*/
    if (dy_lp->iterf > 1)
    { retval = dyrREQCHK ; }
    else
    if (dy_chkpiv(abarj_i,maxabari) < 1.0)
    { retval = dyrMADPIV ; }
    else
    { retval = dyrOK ; }

#   ifndef NDEBUG
    if (dy_opts->print.dual >= 3)
    { outfmt(logchn,gtxecho,"\n      dual pivot numeric drift: ") ;
      outfmt(logchn,gtxecho,"abari<j> = %g, abarj<i> = %g, diff = %g; ",
	     abari_j,abarj_i,fabs(abari_j-abarj_i)) ;
      outfmt(logchn,gtxecho,"recommending %s.",dy_prtdyret(retval)) ; }
#   endif
  }

  *p_abarj = abarj ;
  return (retval) ; }



bool dualpivrow (int xipos, double *betai, double *abari, double *maxabari)

/*
  This routine calculates row i of the basis inverse, beta<i>, and also the
  dual pivot row abar<i>, row i of inv(B)N. beta<i> is easy -- we simply
  premultiply inv(B) by the unit vector e<i>. Calculating abar<i> is not so
  easy. We'll need to do individual dot products for each nonbasic column
  a<k> in the nonbasic partition.

  Arguably, we'd be better off doing dot(beta<i>,a<k>) as needed during the
  selection of the entering (primal) variable by dualin. But I'm interested
  in this approach to see if we do better on numerical accuracy by using
  the max of the pivot row when checking for a suitable pivot.

  Parameters:
    xipos:	basis position of the leaving variable
    betai:	(i) must be a 0 vector
		(o) row i of the basis inverse
    abari:	(i) must be a 0 vector
		(o) row i of inv(B)N, the pivot row coefficients
    maxabari:	(o) max{j} |abar<ij>|, the largest value in the pivot row
  
  Returns: TRUE if the calculations complete with no difficulty,
	   FALSE otherwise
*/

{ int xkndx ;
  flags xkstatus ;
  double abarik ;
  char *rtnnme = "dualpivrow" ;

# ifdef PARANOIA
  double tol ;
  double *ak ;
# endif
# ifndef NDEBUG
  pkvec_struct *ai ;
# endif

/*
  We can use dy_btran to retrieve beta<i>. The calculation is e<i>inv(B).
*/
  betai[xipos] = 1.0 ;
  dy_btran(betai) ;

# ifndef NDEBUG
/*
  If the user is interested, retrieve and print the nonbasic coefficients
  of row a<i>.
*/
  if (dy_lp->phase != dyADDVAR && dy_opts->print.pivoting >= 4)
  { int ndx,cnt ;
    outfmt(logchn,gtxecho,
	   "\n    nonbasic coefficients of leaving row a<%d>:",xipos) ;
    ai = NULL ;
    if (consys_getrow_pk(dy_sys,xipos,&ai) == FALSE)
    { errmsg(122,rtnnme,dy_sys->nme,"row",
	     consys_nme(dy_sys,'c',xipos,TRUE,NULL),xipos) ;
      if (ai != NULL) pkvec_free(ai) ;
      return (FALSE) ; }
    cnt = 1 ;
    for (ndx = 0 ; ndx < ai->cnt ; ndx++)
    { xkndx = ai->coeffs[ndx].ndx ;
      if (dy_var2basis[xkndx] == 0)
      { cnt = (cnt+1)%2 ;
	if (cnt == 0) outchr(logchn,gtxecho,'\n') ;
	outfmt(logchn,gtxecho,"\ta<%d,%d> = %g",
	       xipos,xkndx,ai->coeffs[ndx].val) ; } }
    pkvec_free(ai) ; }
# endif

/*
  We need to do individual dot products dot(beta<i>,a<j>) to obtain abar<i>.
  We also want max<j> abar<i> so that we can check a potential pivot for
  numerical stability. So, open a loop to walk the columns, doing the
  necessary calculations for each nonbasic column which is eligible for
  entry.

  The paranoid version of the loop body does a lot more work: a consistency
  check on x<k>'s status and a check that dot(beta<i>,a<k>) = (inv(B)a<k>)<i>.
  Here it's just a warning. If we actually choose a pivot with numerical drift,
  corrective action is forced. See dy_confirmDualPivot.

  The base tolerance for this check is hardwired to 1.0e-11, dylp's default
  zero tolerance. We don't want the check here tightening when scaling decides
  to tighten the zero tolerance. Experience says that this check is overly
  conservative --- other error control mechanisms usually take precedence at a
  later point --- hence it only issues a warning.
*/
  *maxabari = 0 ;
  for (xkndx = 1 ; xkndx <= dy_sys->varcnt ; xkndx++)
  { 
#   ifdef PARANOIA
    xkstatus = dy_status[xkndx] ;
    if (dy_chkstatus(xkndx) == FALSE) return (FALSE) ;
    if (flgon(xkstatus,vstatBASIC)) continue ;
    abarik = consys_dotcol(dy_sys,xkndx,betai) ;
    if (isnan(abarik) == TRUE)
    { errmsg(320,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	     dy_lp->tot.iters,"betai",xkndx,"dual pivot row") ;
      return (FALSE) ; }
    ak = NULL ;
    if (consys_getcol_ex(dy_sys,xkndx,&ak) == FALSE)
    { errmsg(122,rtnnme,dy_sys->nme,"column",
	     consys_nme(dy_sys,'c',xkndx,TRUE,NULL),xkndx) ;
      if (ak != NULL) FREE(ak) ;
      return (FALSE) ; }
    dy_ftran(ak,FALSE) ;
    tol = (1.0+fabs(ak[xipos]))*1.0e-11*1000 ;
    if (!withintol(ak[xipos],abarik,tol))
    { warn(385,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	   dy_lp->tot.iters+1,dy_basis[xipos],xkndx,abarik,ak[xipos],
	   fabs(ak[xipos]-abarik),tol) ; }
    FREE(ak) ;
    if (!withintol(abarik,0,dy_tols->zero))
    { abari[xkndx] = abarik ;
      if (fabs(abarik) > *maxabari) *maxabari = fabs(abarik) ; }
#   else
    xkstatus = dy_status[xkndx] ;
    if (flgon(xkstatus,vstatBASIC|vstatNBFX)) continue ;
    abarik = consys_dotcol(dy_sys,xkndx,betai) ;
    if (!withintol(abarik,0,dy_tols->zero))
    { abari[xkndx] = abarik ;
      if (fabs(abarik) > *maxabari) *maxabari = fabs(abarik) ; }
#   endif
  }

# ifndef NDEBUG
/*
  If the user is interested, print the transformed row abar<i>. There should
  be no dirty zeroes here.
*/
  if ((dy_lp->phase != dyADDVAR && dy_opts->print.pivoting >= 4) ||
      (dy_lp->phase == dyADDVAR && dy_opts->print.varmgmt >= 3))
  { int cnt ;
    outfmt(logchn,gtxecho,
	   "\n    nonbasic coefficients of transformed row abar<%d>, max %g:",
	   xipos,*maxabari) ;
    cnt = 1 ;
    for (xkndx = 1 ; xkndx <= dy_sys->varcnt ; xkndx++)
    { if (abari[xkndx] == 0) continue ;
      cnt = (cnt+1)%2 ;
      if (cnt == 0) outchr(logchn,gtxecho,'\n') ;
      outfmt(logchn,gtxecho,
	     "\ta<%d,%d> = %g",xipos,xkndx,abari[xkndx]) ; } }
# endif

/*
  That should do it. We're outta here.
*/
  return (TRUE) ; }




void dualdegenin (void)

/*
  This routine forms a new restricted subproblem, increasing the degeneracy
  level kept in dy_lp->degen. A base perturbation is calculated so
  that the maximum possible perturbation is
     perturb = (base)*(varcnt) <= 1.0e-3
  This is then increased, if necessary, so that the perturbation exceeds the
  dual feasibility tolerance. For each variable, the actual perturbation is
  calculated as base*j.

  The routine should not be called if dual degeneracy isn't present, and will
  do a paranoid check to make sure that's true.

  Parameters: none

  Returns: undefined
*/

{ int j,oldlvl ;
  flags statj ;
  double base,perturb ;

  char *rtnnme = "dualdegenin" ;


# if defined(PARANOID) || defined(DYLP_STATISTICS) || !defined(NDEBUG)
  int degencnt = 0 ;
# endif

/*
  Figure out the appropriate perturbation and bump the degeneracy level.
*/
  base = pow(10,(-3-ceil(log10(dy_sys->concnt)))) ;
  while (base <= dy_tols->pfeas) base *= 10 ;
  oldlvl = dy_lp->degen++ ;

# ifndef NDEBUG
  if (dy_opts->print.degen >= 1)
  { outfmt(logchn,gtxecho,
	   "\n    (%s)%d: antidegeneracy increasing to level %d",
	   dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,dy_lp->degen) ;
    outfmt(logchn,gtxecho,", base perturbation %g%s",
	   base,(dy_opts->print.degen >= 4)?":":".") ; }
# endif
# if defined(DYLP_STATISTICS) || !defined(NDEBUG)
  if (dy_lp->degen < DYSTATS_MAXDEGEN)
    degenstats.iterin[dy_lp->degen] = dy_lp->tot.pivs ;
# endif
# ifdef DYLP_STATISTICS
  if (dy_stats != NULL && dy_lp->degen < DYSTATS_MAXDEGEN)
  { if (dy_stats->ddegen[0].cnt < dy_lp->degen)
      dy_stats->ddegen[0].cnt = dy_lp->degen ;
    dy_stats->ddegen[dy_lp->degen].cnt++ ; }
# endif
/*
  Create the perturbed subproblem. We're only interested in variables that
  are participating in the current subproblem, hence the check of ddegenset.
  Further, we're only interested in basic duals that we can drive to bound
  (0) which amounts to nonbasic primals, except for NBFX. (Put another way,
  driving the dual to 0 amounts to loosening the primal constraint. For
  architecturals, the relevant constraint is the upper/lower bound. For
  logicals, it's the associated architectural constraint. Equalities can't be
  loosened.) And, of course, we're only interested if the value is 0.
*/
  for (j = 1 ; j <= dy_sys->varcnt ; j++)
  { if (dy_ddegenset[j] != oldlvl) continue ;
    statj = dy_status[j] ;
    if (flgon(statj,vstatBASIC|vstatNBFX)) continue ;
    if (fabs(dy_cbar[j]) > dy_tols->dfeas) continue ;
/*
  Make the perturbation. We need to perturb in the correct direction (positive
  for NBLB, negative for NBUB) in order to maintain dual feasibility.

  Note that we're pretending to perturb c<j> by perturbing cbar<j>. Given
  that j is nonbasic,  cbar<j> = c<j> - dot(y,a<j>), and we shouldn't perturb
  the associated dual.
*/
    dy_ddegenset[j] = dy_lp->degen ;
    switch (statj)
    { case vstatNBLB:
      { perturb = base*j ;
	break ; }
      case vstatNBUB:
      { perturb = -base*j ;
	break ; }
      case vstatNBFR:
      case vstatSB:
      { errmsg(346,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	       dy_lp->tot.iters,dy_prtvstat(statj),
	       consys_nme(dy_sys,'v',j,FALSE,NULL),j) ;
	return ; }
      default:
      { errmsg(1,rtnnme,__LINE__) ;
	return ; } }
    dy_cbar[j] = perturb ;
#   if defined(PARANOID) || defined(DYLP_STATISTICS)
    degencnt++ ;
#   endif
#   ifndef NDEBUG
    if (dy_opts->print.degen >= 5)
    { outfmt(logchn,gtxecho,"\n\tcbar<%d> perturbed to %g (%s %s).",
	     j,dy_cbar[j],dy_prtvstat(statj),
	     consys_nme(dy_sys,'v',j,FALSE,NULL)) ; }
#   endif
  }

# ifdef PARANOID
  if (degencnt <= 0)
  { errmsg(327,rtnnme,dy_sys->nme) ;
    return ; }
# endif
# ifndef NDEBUG
  if (dy_opts->print.degen >= 1)
  { outfmt(logchn,gtxecho,"%s%d variables.",
	   (dy_opts->print.degen <= 4)?", ":"\n\ttotal ",degencnt) ; }
# endif
# ifdef DYLP_STATISTICS
  if (dy_stats != NULL && dy_lp->degen < DYSTATS_MAXDEGEN)
  { if (dy_stats->ddegen[dy_lp->degen].maxsiz < degencnt)
      dy_stats->ddegen[dy_lp->degen].maxsiz = degencnt ;
    j = dy_stats->ddegen[dy_lp->degen].cnt-1 ;
    perturb = dy_stats->ddegen[dy_lp->degen].avgsiz ;
    dy_stats->ddegen[dy_lp->degen].avgsiz =
			    (float) ((perturb*j+degencnt)/(j+1)) ; }
# endif

  return ; }



dyret_enum dy_dualdegenout (int level)

/*
  This routine backs out all restricted subproblems to the level given by the
  parameter. The value of the involved dual variables is reset to 0 in dy_cbar,
  and dy_ddegenset and dy_lp->degen are adjusted.

  All variables involved in a restricted subproblem were at bound (hence,
  zero) when they were collected into the subproblem. Numeric inaccuracy can
  cause drift over the course of pivoting. This is tested as part of the
  accuracy checks and the antidegeneracy mechanism will be backed out if a
  problem is detected.  Here, the same tests are just paranoia.

  Parameter:
    level:	The target level for removal of restricted subproblems.
  
  Returns: dyrOK if the restoration goes without problem, dyrREQCHK if
	   there's been too much numerical drift since we began the
	   degenerate subproblem.
*/

{ int j ;
  dyret_enum retval ;

# ifdef DYLP_STATISTICS
  int curlvl,curpivs ;
# endif

# ifndef NDEBUG
  if (dy_opts->print.degen >= 1)
  { outfmt(logchn,gtxecho,
       "\n    (%s)%d: antidegeneracy dropping to level %d after %d pivots.",
	   dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,level,
	   dy_lp->tot.pivs-degenstats.iterin[dy_lp->degen]) ; }
# endif

# ifdef DYLP_STATISTICS
/*
  Record the iteration counts. This needs to be a loop because we can peel
  off multiple levels.
*/
  if (dy_stats != NULL)
  for (curlvl = dy_lp->degen ; curlvl > level ; curlvl--)
  { if (curlvl < DYSTATS_MAXDEGEN)
    { curpivs = dy_lp->tot.pivs-degenstats.iterin[curlvl] ;
      dy_stats->ddegen[curlvl].totpivs += curpivs ;
      dy_stats->ddegen[curlvl].avgpivs =
	((float) dy_stats->ddegen[curlvl].totpivs)/
					dy_stats->ddegen[curlvl].cnt ;
      if (curpivs > dy_stats->ddegen[curlvl].maxpivs)
	dy_stats->ddegen[curlvl].maxpivs = curpivs ; } }
# endif

  retval = dyrOK ;
/*
  Back out restricted subproblems to the level specified by level. By removing
  the perturbation (setting c<i> back to 0), we restore the equality
  y<i> = cbar<i> for logicals.
*/
  for (j = 1 ; j <= dy_sys->varcnt ; j++)
  { 
    if (dy_ddegenset[j] > level)
    { dy_ddegenset[j] = level ;
      dy_cbar[j] = 0 ;
      if (j <= dy_sys->concnt)
      { dy_y[j] = 0 ; }
#     ifndef NDEBUG
      if (dy_opts->print.degen >= 5)
      { outfmt(logchn,gtxecho,"\n\tcbar<%d> restored to %g, (%s %s)",
	       j,0.0,dy_prtvstat(dy_status[j]),
	       consys_nme(dy_sys,'v',j,FALSE,NULL)) ; }
#     endif
    } }
    
  dy_lp->degen = level ;
  
return (retval) ; }



#ifdef CHECK_DSE_UPDATES

static bool check_dse_update (int xkndx, double u_cbark, double u_rhok)

/*
  This routine checks x<k> for consistent status, then does one of the
  following:
    * for nonbasic variables, a `from scratch' calculation of cbar<k>
      as c<k> - c<B>(inv(B)a<k>)
    * for basic variables, a `from scratch' calculation of rho<k> as
      ||e<k>inv(B)||.
  
  Parameters:
    xkndx:	index for the variable
    u_cbark:	updated cbar<k> (valid only if x<k> is nonbasic)
    u_rhok:	updated rho<k> (valid only if x<k> is basic)

  Returns: TRUE if the updates agree with the values calculated from first
	   principles, FALSE otherwise.
*/

{ int xkpos,xindx,xipos ;
  double cbark,*abark,rhok,*betak ;
  flags xkstatus ;
  bool retval ;
  char *rtnnme = "check_dse_update" ;

/*
  Make sure we're ok as far as consistent status and values.
*/
  retval = dy_chkstatus(xkndx) ;
  xkstatus = dy_status[xkndx] ;
/*
  The first case is for a basic variable. We're interested in checking the
  norm of the row of the basis inverse. We load up betak as a unit vector
  with a 1 in the appropriate position, btran it, and then calculate the
  norm.
*/
  if (flgon(xkstatus,vstatBASIC))
  { betak = (double *) CALLOC(dy_sys->concnt+1,sizeof(double)) ;
    xkpos = dy_var2basis[xkndx] ;
    betak[xkpos] = 1.0 ;
    dy_btran(betak) ;
    rhok = exvec_ssq(betak,dy_sys->concnt) ;
    if (!withintol(rhok,u_rhok,dy_tols->reframe*(1+fabs(rhok))))
    { errmsg(388,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	     dy_lp->tot.iters,"rho",xkpos,u_rhok,rhok,fabs(u_rhok-rhok),
	     dy_tols->reframe*(1+fabs(rhok))) ;
      retval = FALSE ; }
    FREE(betak) ; }
/*
  For nonbasic variables, check that the reduced cost c<k> - c<B>abar<k>
  matches u_cbark. If dual anti-degeneracy is active, the value we calculate
  here should be 0, and in general will not match u_cbark (unless we're headed
  for higher-level degeneracy).
*/
  else
  { abark = NULL ;
    if (consys_getcol_ex(dy_sys,xkndx,&abark) == FALSE)
    { errmsg(122,rtnnme,dy_sys->nme,
	     "column",consys_nme(dy_sys,'v',xkndx,TRUE,NULL),xkndx) ;
      if (abark != NULL) FREE(abark) ;
      return (FALSE) ; }
    dy_ftran(abark,FALSE) ;
    cbark = dy_sys->obj[xkndx] ;
    for (xipos = 1 ; xipos <= dy_sys->concnt ; xipos++)
    { xindx = dy_basis[xipos] ;
      cbark -= dy_sys->obj[xindx]*abark[xipos] ; }
    if (dy_ddegenset[xkndx] == 0)
    { if (!withintol(cbark,u_cbark,dy_tols->reframe*(1+fabs(cbark))))
      { errmsg(388,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	       dy_lp->tot.iters,"cbar",xkndx,u_cbark,cbark,fabs(u_cbark-cbark),
	       dy_tols->reframe*(1+fabs(cbark))) ;
	retval = FALSE ; } }
    else
    { if (!withintol(cbark,0.0,dy_tols->dfeas))
      { if (withintol(cbark,0.0,1000*dy_tols->dfeas))
	{ warn(388,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	       dy_lp->tot.iters,"cbar",xkndx,0,cbark,
	       fabs(cbark),dy_tols->dfeas) ; }
	else
	{ errmsg(388,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
		 dy_lp->tot.iters,"cbar",xkndx,0,cbark,
		 fabs(cbark),dy_tols->dfeas) ;
	  retval = FALSE ; } } }
    FREE(abark) ; }
  
  return (retval) ; }

#endif



static void dualpricexk (int xkndx, int *xindx, double *nbbari,
			 bool *pivreject)

/*
  This routine does the pricing calculation for the single variable x<k>.
  For simplicity, assume that x<k> occupies pos'n k of the basis.

  The pricing is dual steepest edge, adapted for bounded variables. Just as
  with primal steepest edge pricing, in dual space we're looking for the
  largest reduced cost, normalised by the norm of the direction of motion.
  Translated into the primal space, we're looking for the largest
  infeasibility, normalised by rho<k> (the norm of row k of the basis
  inverse). If xbasic<k> is the value of x<k> and bnd<k> is the violated
  bound closest to x<k>, the `reduced cost' is |x<k>-bnd<k>|/||beta<k>||.

  Parameters:
    xkndx:	the index of x<k>, the variable to be priced
    xindx:	(o) set to the index of x<k> if x<k> should supplant the
		current candidate x<i>
    nbbari:	(i) the normalised `reduced cost' of x<i> 
    pivreject:	(o) set to TRUE if x<k> would be the winning candidate,
		but it's flagged with the NOPIVOT qualifier

  Returns: TRUE if x<k> supplanted x<i>, FALSE otherwise
*/

{ int xkpos ;
  double deltak,nbbark ;
  flags xkstatus ;
  char *rtnnme = "dualpricexk" ;

/*
  Get the position and status of x<k>.
*/
  xkpos = dy_var2basis[xkndx] ;
  xkstatus = dy_status[xkndx] ;

# ifndef NDEBUG
  if (dy_opts->print.pricing >= 3)
  { outfmt(logchn,gtxecho,"\n\tpricing %s (%d), status %s; ",
	   consys_nme(dy_sys,'v',xkndx,FALSE,NULL),xkndx,
	   dy_prtvstat(xkstatus)) ; }
#  endif
/*
  Calculate the dual `reduced cost', and the normalised distance to bound.
  dualpricexk is only called for vstatBLLB, vstatBUUB, so by definition
  the `reduced cost' is larger than the relevant tolerance (the primal
  feasibility tolerance, as it happens). But we still need to check for
  values in the bogus range, and for the NOPIVOT status qualifier.
*/
  if (flgon(xkstatus,vstatBLLB))
  { deltak = dy_sys->vlb[xkndx]-dy_x[xkndx] ; }
  else
  { deltak = dy_x[xkndx]-dy_sys->vub[xkndx] ; }
  nbbark = deltak/sqrt(dy_rho[xkpos]) ;
# ifndef NDEBUG
  if (dy_opts->print.pricing >= 3)
  { outfmt(logchn,gtxecho,
	   "bbar<k> = %g, ||beta<k>|| = %g, |bbar<k>|/||beta<k>|| = %g.",
	   deltak,sqrt(dy_rho[xkpos]),nbbark) ; }
# endif
  if (withintol(deltak,0,dy_tols->pfeas*dy_tols->bogus))
  { if (withintol(deltak,0,dy_tols->pfeas))
    {
#     ifndef NDEBUG
      if (dy_opts->print.pricing >= 3)
      { outfmt(logchn,gtxecho," << zero!? >>") ;
	errmsg(1,rtnnme,__LINE__) ; }
#     endif
      return ; }
    else
    if (dy_lp->iterf > 1)
    {
#     ifndef NDEBUG
      if (dy_opts->print.pricing >= 3)
	outfmt(logchn,gtxecho," << bogus >>") ;
#     endif
      return ; } }
  if (flgon(xkstatus,vstatNOPIVOT))
  { *pivreject = TRUE ;
#   ifndef NDEBUG
    if (dy_opts->print.pricing >= 3)
      outfmt(logchn,gtxecho," << reject >>") ;
#   endif
    return ; }
/*
  x<k> is a suitable candidate; the only question is whether it's better than
  the current candidate. If it is, x<k> supplants it.
*/
  if (nbbark > *nbbari)
  { *nbbari = nbbark ;
    *xindx = xkndx ;
#   ifndef NDEBUG
    if (dy_opts->print.pricing >= 3)
      outfmt(logchn,gtxecho," << supplant >>") ;
#   endif
    return ; }
  else
  { 
#   ifndef NDEBUG
    if (dy_opts->print.pricing >= 3)
      outfmt(logchn,gtxecho," << inferior >>") ;
#   endif
    return ; } }



dyret_enum dy_dualout (int *xindx)

/*
  In the normal course of events with DSE pricing, the leaving variable for
  the next pivot is chosen during the update of cbar and rho. Still, on
  occasion we'll need to select again because the pivot is unsuitable, and
  we need a way to select the leaving variable for the initial pivot. That's
  where this routine comes in.

  This routine scans the basis and selects a leaving variable x<i> by
  choosing the variable with the greatest normalised infeasibility. (If this
  were viewed in terms of the dual space, we're doing a steepest edge
  algorithm. See the discussion of DSE pricing in the documentation.) The
  routine dualpricexk does the actual work.

  If all infeasible variables are on the pivot reject list (and thus we can't
  bring them to feasibility) we'll return dyrPUNT. In a sense, the situation
  is analogous to true infeasibility --- there is no suitable variable to
  enter --- and the hope is that as dylp reverts to primal phase I it'll be
  sufficiently intelligent to add some variables and find a decent pivot.

  Parameters:
    xindx:	(o) Index of the leaving variable x<i>; 0 if no leaving
		    variable is selected.

  Returns: dyrOK if the leaving variable is selected without incident
	   dyrOPTIMAL if all variables are within bounds
	   dyrPUNT if all infeasible variables are on the pivot reject list
*/

{ int xkndx,xkpos ;
  flags xkstatus ;
  bool pivreject ;
  double candbbari ;
  dyret_enum retval ;
  
  char *rtnnme = "dy_dualout" ;

  retval = dyrINV ;

# ifndef NDEBUG
  if (dy_opts->print.pricing >= 1)
  { outfmt(logchn,gtxecho,
	   "\n%s: pricing %d rows from %d for %d candidate.",
	   rtnnme,dy_sys->concnt,1,1) ; }
# endif

  candbbari = 0 ;
  *xindx = 0 ;
  pivreject = FALSE ;
/*
  Open a loop to walk through the basic variables, looking for the one that
  has the greatest normalised infeasibility and is eligible for pivoting.
*/
  for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
  { xkndx = dy_basis[xkpos] ;
    xkstatus = dy_status[xkndx] ;
    if (flgoff(xkstatus,vstatBLLB|vstatBUUB|vstatBFX))
    { 
#     ifndef NDEBUG
      if (dy_opts->print.pricing >= 3)
      { outfmt(logchn,gtxecho,"\n\tpricing %s (%d), status %s; << status >>",
		 consys_nme(dy_sys,'v',xkndx,TRUE,NULL),xkndx,
		 dy_prtvstat(xkstatus)) ; }
#     endif
      continue ; }
    dualpricexk(xkndx,xindx,&candbbari,&pivreject) ; }
/*
  We're done. Time to set the proper return value. If we have a new candidate
  for entry, set the return value to dyrOK. If we don't have a candidate,
  there are three possible reasons:
   * We have potential pivots on the reject list:
     pivreject == TRUE. We want to return dyrPUNT.
   * We're optimal.
     pivreject == FALSE. dyrOPTIMAL is the proper return value.
   * We saw some infeasible variables, but the infeasibility was within the
     bogus number tolerance.
     pivreject == FALSE. dyrOPTIMAL is still the correct return code (we'll
     end up doing a refactor in preoptimality, and the bogus numbers will
     either disappear, or we'll be back here ready to use them).
*/
  if (*xindx == 0)
  { if (pivreject == TRUE)
      retval = dyrPUNT ;
    else
      retval = dyrOPTIMAL ; }
  else
  { retval = dyrOK ; }
 
# ifndef NDEBUG
  if (dy_opts->print.pricing >= 2)
  { if (*xindx != 0)
    { outfmt(logchn,gtxecho,
	     "\n    (%s)%d: selected %s (%d) %s to leave, DSE price %g.",
	     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
	     consys_nme(dy_sys,'v',*xindx,TRUE,NULL),*xindx,
	     dy_prtvstat(dy_status[*xindx]),candbbari) ; }
    else
    { outfmt(logchn,gtxecho,"\n    (%s)%d: no suitable candidates.",
	     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters) ; } }
  if (dy_opts->print.pricing >= 1)
  { if (retval == dyrPUNT)
    { outfmt(logchn,gtxecho,
	     "\n    (%s)%d: all suitable x<i> on rejected pivot list.",
	     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters) ; } }
# endif

  return (retval) ; }



static double ddirdothyper (int xindx, double *abari, int diri,
			    int xqndx, int dirq)

/*
  This routine is used in the context of dual anti-degen lite, where we're
  trying to choose the leaving dual variable based on the alignment of the
  hyperplane that will become tight relative to the desired dual direction
  of motion zeta<i>. See extensive comments at the top of the file.

  The upshot of all the math is that dot(zeta<i>,a<q>) = abar<iq> for x<i>
  leaving to become NBLB, and -abar<iq> for x<i> leaving to become NBUB.  In
  short, dot(zeta<i>,a<q>) = dir<i>*abar<iq>. All we do here is normalise by
  ||a<q>||.

  For convenience, the signs of the dot products are set as if we were dealing
  with <= constraints (i.e., the maximum positive value indicates a hyperplane
  that perfectly blocks motion in the direction of the edge). This entails
  multiplication of the dot product by -dir<q>.

  There is no paranoid check, unlike the primal routine pdirdothyper. Since
  we calculated each abar<iq> as dot(beta<i>,a<q>), it seems a little
  pointless to repeat it here. We might still encounter empty columns,
  though, so we add 1 to ||a<q>||.

  Parameters:
    xindx:	index of x<i>, the leaving primal variable (hence entering
		dual)
    abari:	e<i>inv(B)N, calculated as beta<i>a<q> for a<q> in N; the
		portions of zeta<i> matching the w and v variables
    diri:	direction of motion of x<i> (matches direction of motion of
		entering dual)
    xqndx:	index of x<q>, the primal candidate to enter (hence candidate
		leaving dual)
    dirq:	direction of motion of x<q> (opposite the direction of motion
		of the leaving dual)

  Returns: -dir<q>*dir<i>*abar<iq>/||a<q>||, or NaN if something goes awry.
*/

{ double normaq,abariq,dotprod ;

  abariq = abari[xqndx] ;
  normaq = consys_2normcol(dy_sys,xqndx) ;
  dotprod = -dirq*diri*abariq/(normaq+1) ;
  setcleanzero(dotprod,dy_tols->zero) ;
  
  return (dotprod) ; }



static double bdothyper (int xqndx, int dirq)

/*
  This routine is used in the context of dual antidegen lite, where we're
  trying to choose the leaving dual variable based on the alignment of
  the hyperplane coming tight with the pivot relative to the direction that 
  minimises the objective. See extensive comments at the head of the file.

  The routine calculates the dot product of -b' = [b -l u] with a constraint
  normal a'<q>, normalised by ||a'<q>||.  ||-b'|| doesn't change over the
  various candidates, so we don't need to include it in the normalisation for
  comparison purposes.
  
  A little care is required to orient the normal of the dual constraint. If
  x<q> is NBLB, we're dealing with ya<q> - w<q> = c<q>, viewed as a >=
  constraint. If x<q> is NBUB, we're dealing with ya<q> + v<q> = c<q>, viewed
  as a <= constraint.

  We want to have the maximum dot product when the hyperplane exactly
  blocks motion in the direction of the objective. Case analysis gives the
  calculation as dir<q>*dot(constraint,[b -l u]), where `constraint' is
  replaced by the appropriate constraint normal.

  If x<q> is a slack, we're talking about a leaving dual y<q>, and the
  relevant constraint boils down to y<q> >= 0 or y<q> <= 0.
  
  Parameters:
    xqndx:	x<q>, the primal candidate for entry (hence the leaving dual)
    dirq:	the direction of motion of x<q>
  
  Returns: dir<q>*dot(b',a'<q>)/||a'<q>||, as described above, or NaN if the
	   calculation goes awry.
*/

{ double dotprod,normq ;

/*
  If a dual is leaving, all we need to do is find the correct value in b.
*/
  if (xqndx <= dy_sys->concnt)
  { dotprod = dy_sys->rhs[xqndx] ; }
/*
  Otherwise, we need to calculate dot(b',a'<q>), which breaks down as
  dot(b,a<q>) plus the contribution due to w or v.
*/
  else
  { dotprod = consys_dotcol(dy_sys,xqndx,dy_sys->rhs) ;
    normq = sqrt(consys_ssqcol(dy_sys,xqndx)+1) ;
    if (dirq > 0)
      dotprod += dy_sys->vlb[xqndx] ;
    else
      dotprod += dy_sys->vub[xqndx] ;
    dotprod = dotprod/normq ; }

  dotprod = dirq*dotprod ;
  setcleanzero(dotprod,dy_tols->zero) ;
  
  return (dotprod) ; }



static dyret_enum dualin (int xindx, int outdir,
		      	     int *xjndx, int *indir,
			     double *abari, double maxabari, double **p_abarj)

/*
  This routine selects the incoming variable x<j> for a dual pivot. Recall
  that in the primal, where x<k> = xbar<k> - abar<k,j>delta_x<j>, we look for
  the limiting xbar<i> s.t. xbar<i>/abar<i,j> is minimum over i to determine
  the leaving variable x<i>.

  Without getting into the technicalities of the algebra (and playing fast
  and loose with the indices), the analogous calculation in the dual problem
  is y<k> = cbar<k> + abar<i,k>delta_y<i>, and we're looking to find the
  limiting cbar<j> s.t. cbar<j>/abar<i,j> is minimum over the nonbasic
  columns. This will give us the leaving dual variable y<j>. But it's
  impossible (short of a few pages) to do the explanation properly in the
  dual context, so I'll switch now to the primal context.

  x<i> has been selected to leave at its upper/lower bound, and the
  corresponding direction of motion and total change have been determined.
  We're now looking for x<j> to enter. After this pivot, the reduced cost of
  x<i> will be cbar<i> = -cbar<j>/abar<i,j>, where |cbar<j>/abar<i,j>| is the
  smallest such value over the nonbasic columns.  cbar<i> also has to have
  the right sign -- if x<i> will end up out at its lower bound, cbar<i> must
  end up positive; if x<i> will be out at its upper bound, cbar<i> must end
  up negative (we're minimising in the dylp primal, eh).

  Returning to the dual context for a second, dualin implements a version
  of anti-degen lite. Analogous to the primal case, the notion
  is that we want to keep tight the hyperplanes most closely aligned with
  where we want to go ([abar<i> e<q>]). But it gets complicated, largely
  because we're only running the dual implicitly, and you have to make up
  some pieces on the spot. See the comments at the head of the file, and
  in ddirdothyper.

  Since dy_cbar is updated in dseupdate, the test for loss of dual feasibility
  is performed there (might as well catch it as it happens).

  Parameters:
    xindx:	index of the leaving variable x<i>
    outdir:	direction of movement of x<i>,
		   1 if rising to lower bound
		  -1 if falling to upper bound
    xjndx:	(o) index of the entering variable x<j>
    indir:	(o) direction of movement for x<j>,
		     1 if rising from lower bound
		    -1 if falling from upper bound
    abari:	row i of inv(B)N (the pivot row)
    maxabari:	maximum value in abar<i>
    p_abarj:	(o) used to return the value of abar<j>
  
  Returns: dyret_enum code, as follows:
    dyrOK:	the pivot is (dual) nondegenerate
    dyrDEGEN:	the pivot is (dual) degenerate
    dyrUNBOUND:	the problem is dual unbounded (primal infeasible) (i.e.,
		no incoming variable can be selected)
    dyrREQCHK:	if the pivot a<ij> is a bogus number
    dyrMADPIV:	if a<ij> fails the numerical stability test
    dyrFATAL:	fatal confusion
*/

{ int reject,xkndx,degencnt,dirk ;
  flags xkstatus ;
  double deltak,abarik,ratioik,deltamax,abarij,ratioij,bdotaj,bdotak ;
  bool newxj ;
  dyret_enum retval,confirm ;
  char *rtnnme = "dualin" ;

  retval = dyrINV ;

# ifndef NDEBUG
  if (dy_opts->print.pivoting >= 1)
  { outfmt(logchn,gtxecho,"\n  (%s)%d: selecting entering variable",
	   dy_prtlpphase(dy_lp->phase, TRUE),dy_lp->tot.iters+1) ;
    if (dy_opts->print.pivoting >= 2)
    { outfmt(logchn,gtxecho,"\n    %s (%d) leaving %s (%d) %s",
	     consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,
	     consys_nme(dy_sys,'c',dy_var2basis[xindx],FALSE,NULL),
	     dy_var2basis[xindx],(outdir > 0)?"increasing":"decreasing") ;
      outfmt(logchn,gtxecho,
	     "\n\tVariable\tcbar<k>\tabar<i,k>\tdelta\tDisp") ; } }
# endif

  deltamax = dy_tols->inf ;
  *xjndx = 0 ;
  *indir = 0 ;
  abarij = 0 ;
  ratioij = quiet_nan(0) ;
  bdotaj = quiet_nan(0) ;
  dirk = 0 ;
/*
  Open a loop to step through the variables. We skip basic variables and
  variables that are nonbasic fixed, but anyone else is eligible. If the
  status is ok, the second easy test is for abar<ik> = 0, in which case we're
  done with this column.

  Note that we shouldn't see superbasics here, as we're primal infeasible
  while running dual simplex and superbasics should not be created. Nor
  should we see nonbasic free variables; they can't be nonbasic in a dual
  feasible solution. dy_chkstatus enforces this when we're paranoid.
*/
  newxj = FALSE ;
  degencnt = 0 ;
  bdotak = 0 ;
  for (xkndx = 1 ; xkndx <= dy_sys->varcnt ; xkndx++)
  { if (dy_lp->degen > 0 && dy_ddegenset[xkndx] != dy_lp->degen) continue ;
    xkstatus = dy_status[xkndx] ;
#   ifdef PARANOIA
    if (dy_chkstatus(xkndx) == FALSE) return (dyrFATAL) ;
#   endif
/*
  Do the tests to determine if this variable is a possible candidate: it cannot
  be basic or NBFX, the pivot abar<ik> must be large enough to be stable, and
  the sign of abar<ik> must be compatible with the direction of motion for
  x<i> and x<k>.

  Case analysis for cbar<i> = -cbar<k>/abar<i,k> yields:
    * x<i> rising to lb<i> and leaving, cbar<i> to be >= 0
      + x<k> nonbasic at l<k>, c<k> >= 0, implies abar<i,k> must be <= 0
      + x<k> nonbasic at u<k>, c<k> <= 0, implies abar<i,k> must be >= 0
    * x<i> dropping to ub<i> and leaving, cbar<i> to be <= 0
      + x<k> nonbasic at l<k>, c<k> >= 0, implies abar<i,k> must be >= 0
      + x<k> nonbasic at u<k>, c<k> <= 0, implies abar<i,k> must be <= 0
*/
    reject = 0 ;
    abarik = abari[xkndx] ;
    if (flgon(xkstatus,vstatBASIC|vstatNBFX))
    { reject = -1 ; }
    else
    if (abarik == 0)
    { reject = -2 ; }
    else
    { if (outdir == -1)
      { if ((flgon(xkstatus,vstatNBUB) && abarik > 0) ||
	    (flgon(xkstatus,vstatNBLB) && abarik < 0))
	{ reject = -3 ; } }
      else
      { if ((flgon(xkstatus,vstatNBUB) && abarik < 0) ||
	    (flgon(xkstatus,vstatNBLB) && abarik > 0))
	{ reject = -3 ; } } }

#   ifndef NDEBUG
    if (dy_opts->print.pivoting >= 3 ||
	(dy_opts->print.pivoting >= 2 && reject >= 0))
    { outfmt(logchn,gtxecho,"\n\t%-8s (%d)",
	     consys_nme(dy_sys,'v',xkndx,FALSE,NULL),xkndx) ;
      outfmt(logchn,gtxecho,"\t%g\t%g",dy_cbar[xkndx],abarik) ; }
    if (dy_opts->print.pivoting >= 3)
    { switch (reject)
      { case -1: /* basic/NBFX */
	{ outfmt(logchn,gtxecho,"\t\trejected -- status %s",
		 dy_prtvstat(xkstatus)) ;
	  break ; }
	case -2: /* zero pivot */
	{ outfmt(logchn,gtxecho,"\t\trejected -- zero pivot") ;
	  break ; }
	case -3: /* incompatible sign */
	{ outfmt(logchn,gtxecho,"\t\trejected -- wrong sign") ;
	  break ; } } }
#   endif
/*
  If we've rejected x<k>, get on to the next candidate, otherwise set the
  direction of entry and continue.
*/
    if (reject < 0) continue ;
    if (flgon(xkstatus,vstatNBUB))
      dirk = -1 ;
    else
      dirk = 1 ;
/*
  Calculate the limit on the allowable delta for the entering dual imposed
  by this leaving dual.
*/
    deltak = fabs(dy_cbar[xkndx]/abarik) ;
    setcleanzero(deltak,dy_tols->cost) ;
#   ifndef NDEBUG
    if (dy_opts->print.pivoting >= 2)
    { outfmt(logchn,gtxecho,"\t%g",deltak) ; }
#   endif

/*
  We have delta<k>. Now, is x<k> a better candidate to enter than the current
  incumbent x<j>? If delta<k> is really smaller, there's no contest.
  
  We do not want a toleranced comparison here --- small differences in delta
  multiplied by large abar<ik> can result in loss of feasibility elsewhere.
*/
    ratioik = quiet_nan(0) ;
    if (deltak < deltamax)
    { newxj = TRUE ;
      ratioik = dy_chkpiv(abarik,maxabari) ;
#     ifndef NDEBUG
      if (dy_opts->print.pivoting >= 2)
      { outfmt(logchn,gtxecho," (%g)",deltamax-deltak) ; }
#     endif
      degencnt = 0 ; }
/*
  If there's a tie, decide it based on the user's choice of antidegen lite
  options. See comments at the head of dy_primalpivot for more details on
  the decision criteria for AlignObj and AlignEdge. But if abar<ik> is an
  unsuitable pivot, don't even bother with further checks.
*/
    else
    if (deltak == deltamax)
    { ratioik = dy_chkpiv(abarik,maxabari) ;
      if (ratioik >= 1.0)
      { switch (dy_opts->degenlite)
	{ case 0: /* pivotabort */
	  { if (ratioik > ratioij) newxj = TRUE ;
	    break ; }
	  case 1: /* pivot */
	  { if (ratioik > ratioij) newxj = TRUE ;
	    degencnt++ ;
	    break ; }
	  case 2: /* alignobj */
	  case 3: /* alignedge */
	  { if (dy_opts->degenlite == 2)
	    { if (degencnt == 0) bdotaj = bdothyper(*xjndx,*indir) ;
	      bdotak = bdothyper(xkndx,dirk) ; }
	    else
	    { if (degencnt == 0)
		bdotaj =  ddirdothyper(xindx,abari,outdir,*xjndx,*indir) ;
	      bdotak =  ddirdothyper(xindx,abari,outdir,xkndx,dirk) ; }
	    degencnt++ ;
	    if (bdotaj > 0 && bdotak <= 0)
	    { /* keep x<j> */ }
	    else
	    if (bdotaj <= 0 && bdotak > 0)
	    { newxj = TRUE ; }
	    else
	    if (fabs(bdotaj) > fabs(bdotak))
	    { newxj = TRUE ; }
	    else
	    if (bdotaj == bdotak)
	    { if (ratioik > ratioij) newxj = TRUE ; }
	    break ; }
	  case 4: /* perpobj */
	  case 5: /* perpedge */
	  { if (dy_opts->degenlite == 4)
	    { if (degencnt == 0) bdotaj = bdothyper(*xjndx,*indir) ;
	      bdotak = bdothyper(xkndx,dirk) ; }
	    else
	    { if (degencnt == 0)
		bdotaj = ddirdothyper(xindx,abari,outdir,*xjndx,*indir) ;
	      bdotak = ddirdothyper(xindx,abari,outdir,xkndx,dirk) ; }
	    degencnt++ ;
	    if (bdotak > bdotaj) newxj = TRUE ;
	    break ; }
	} } }
#   ifndef NDEBUG
    else
    { if (dy_opts->print.pivoting >= 2)
      { outfmt(logchn,gtxecho," (%g)",deltamax-deltak) ; } }
#   endif
/*
  If we've selected a new entering variable, make the changes. If the
  user's choice of antidegen lite option was pivotabort, maybe we can skip
  the rest of the scan.
*/
    if (newxj == TRUE)
    { deltamax = deltak ;
      *xjndx = xkndx ;
      if (flgon(dy_status[*xjndx],vstatNBUB))
	*indir = -1 ;
      else
	*indir = 1 ;
      abarij = abarik ;
      ratioij = ratioik ;
      bdotaj = bdotak ;
      newxj = FALSE ;
#     ifndef NDEBUG
      if (dy_opts->print.pivoting >= 2)
      { outfmt(logchn,gtxecho,"\tentering from %s",
	       (flgon(xkstatus,vstatNBUB))?"ub":"lb") ; }
#     endif
      if (dy_opts->degenlite == 0 && deltak == 0 && ratioij >= 1.0) break ; } }
/*
  Why are we here? With luck, we have a nondegenerate (deltamax > 0) and
  numerically stable pivot abar<ij>. There are two other possibilities:  the
  dual is unbounded (deltamax = infty), or we're degenerate (deltamax = 0).
*/
  switch (retval)
  { case dyrINV:
    { if (deltamax < dy_tols->inf)
      { if (ratioij >= 1.0)
	{ if (dy_lp->iterf > 1 &&
	      withintol(abarij,0,dy_tols->bogus*dy_tols->zero))
	  { retval = dyrREQCHK ;
#  	    ifndef NDEBUG
	    warn(381,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
		 dy_lp->tot.iters+1,"abar",xindx,*xjndx,abarij,
		 dy_tols->bogus*dy_tols->zero,
		 dy_tols->bogus*dy_tols->zero-fabs(abarij)) ;
#  	    endif
	  }
	  else
	  if (deltamax == 0)
	  { retval = dyrDEGEN ; }
	  else
	  { retval = dyrOK ; } }
	else
	{ retval = dyrMADPIV ; } }
      else
      { *xjndx = -1 ;
	retval = dyrUNBOUND ; }
      break ; }
    default:
    { errmsg(1,rtnnme,__LINE__) ;
      return (dyrFATAL) ; } }
/*
  If we think this pivot is ok (including degenerate), confirm it.
*/
  if (retval == dyrOK || retval == dyrDEGEN)
  { confirm = dy_confirmDualPivot(xindx,*xjndx,abari,maxabari,p_abarj) ;
    if (confirm != dyrOK)
      retval = confirm ; }
/*
  We're done, except perhaps for printing some information.
*/
# ifndef NDEBUG
  if (dy_opts->print.pivoting == 1) outfmt(logchn,gtxecho,"...") ;
  if ((retval == dyrOK || retval == dyrDEGEN) && dy_opts->print.pivoting >= 1)
  { outfmt(logchn,gtxecho,"\n    selected %s (%d) to enter from ",
	   consys_nme(dy_sys,'v',*xjndx,FALSE,NULL),*xjndx) ;
    if (*indir > 0)
    { outfmt(logchn,gtxecho," %s = %g, ","lb",dy_sys->vlb[*xjndx]) ; }
    else
    { outfmt(logchn,gtxecho," %s = %g, ","ub",dy_sys->vub[*xjndx]) ; }
    outfmt(logchn,gtxecho,"abar<%d,%d> = %g, cbar<%d> = %g, delta = %g.",
	   xindx,*xjndx,abarij,*xjndx,dy_cbar[*xjndx],deltamax) ; }
  if (retval == dyrDEGEN && dy_opts->print.dual >= 3)
  { outfmt(logchn,gtxecho,
	   "\n   (%s)%d: %s %s %s (%d), cbar<%d> = %g",
	   dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters+1,
	   dy_prtdyret(retval),dy_prtvstat(dy_status[*xjndx]),
	   consys_nme(dy_sys,'v',*xjndx,FALSE,NULL),*xjndx,
	   *xjndx,dy_cbar[*xjndx]) ;
    if (dy_opts->degenlite >= 2 && dy_opts->degenlite <= 5)
    { if (degencnt > 0)
	outfmt(logchn,gtxecho,", align = %g, deg = %d.",bdotaj,degencnt) ; }
    else
    { outchr(logchn,gtxecho,'.') ; } }
# endif

  return (retval) ; }



static dyret_enum dseupdate (int xindx, int xjndx, int *candxi, double *tau,
			     double *betai, double *abari, double *abarj)

/*
  This routine handles the updates of rho<k> = ||beta<k>||^2 required for
  DSE pricing. It also updates cbar<k> for the nonbasic variables, so that
  we don't have to recalculate them each time we return to the primal problem.

  Assuming for simplicity that x<i> occupies row i of the basis, the update
  formula for rho<k> is

    rho'<k> = rho<k> - 2*(abar<kj>/abar<ij>)*tau<k> +
		  (abar<kj>/abar<ij>)*rho<i>			      k != i

    rho'<i> = rho<i>/abar<ij>^2
  
  While we're doing the updates, we also select the next candidate to leave
  the basis. Because we need tau<k> = dot(beta<k>,beta<i>) for all rows of
  the old basis inverse, this is precalculated before the pivot as
  inv(B)beta<i> using dy_ftran, and passed in as the vector tau.

  To update the reduced costs, the formulae are

    cbar'<i> = -cbar<j>/abar<ij>
    cbar'<k> = cbar<k> - cbar<j>*(abar<ik>/abar<ij>)	k != i
  
  The major cost here is the scan of the nonbasic columns --- we've already
  taken the time and trouble to calculate abar<i>. When antidegeneracy is
  active, we need to take care that we update only columns that are included
  in the restricted subproblem.

  REMEMBER that we've already pivoted, so x<j> is now basic in pos'n i,
  and x<i> is now nonbasic.

  Parameters:
    xindx:	index of the leaving variable x<i>
    xjndx:	index of the entering variable x<j>
    candxi:	(o) index of the leaving variable for the next pivot
    tau:	inv(B)beta<i>
    betai:	row i of the basis inverse
    abari:	row i of inv(B)N
    abarj:	column j of inv(B)N
  
  Returns: dyrOK if the update proceeds without error and a new leaving
	   variable is selected
	   dyrOPTIMAL if the update proceeds without error but there are
	   no out-of-bound primal variables
	   dyrPUNT if the update proceeds without error but the only
	   out-of-bound primal variables are flagged as `do not pivot'
	   dyrLOSTDFEAS if we detect loss of dual feasibility while updating
	   the reduced costs
	   dyrFATAL if there's a problem (only when we're paranoid)
*/

{ int xipos,xkndx,xkpos ;
  double abarij,cbarj,abarik,cbark,rhoi,rhok,alphak,candbbari,deltak,tolk ;
  double *betak ;
  flags xjstatus,xkstatus ;
  bool pivreject ;
  dyret_enum retval ;
  char *rtnnme = "dseupdate" ;

/*
  Do a little setup and pull out some common values. If we're feeling
  paranoid, check that a direct calculation of dot(beta<i>,beta<i>) matches
  the value we got from tau = inv(B)beta<i>.
*/
  *candxi = 0 ;
  candbbari = 0 ;
  pivreject = FALSE ;
  retval = dyrINV ;

  xipos = dy_var2basis[xjndx] ;
  abarij = abari[xjndx] ;
  cbarj = dy_cbar[xjndx] ;
  xjstatus = dy_status[xjndx] ;
  rhoi = tau[xipos] ;
# ifdef PARANOIA
  rhok = exvec_ssq(betai,dy_sys->concnt) ;
  setcleanzero(rhok,dy_tols->zero) ;
  if (!withintol(rhoi,rhok,dy_tols->zero*(1+rhok)))
  { if (!withintol(rhoi,rhok,dy_tols->zero+dy_tols->bogus*(1+rhok)))
    { errmsg(394,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	     dy_lp->tot.iters,xipos,xipos,rhoi,xipos,rhok,fabs(rhoi-rhok),
	     dy_tols->zero*dy_tols->bogus*(1+rhok)) ;
      return (dyrFATAL) ; }
    else
    { warn(394,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	   dy_lp->tot.iters,xipos,xipos,rhoi,xipos,rhok,fabs(rhoi-rhok),
	   dy_tols->zero*dy_tols->bogus*(1+rhok)) ; } }
# endif
/*
  Have we drifted so far that we should reset the norms from scratch?  The
  test is that the iteratively updated norm rho<i> is within dy_tols->reframe
  percent of the exact value ||beta<i>||^2. (We use the PSE reframe tolerance
  because it's handy, but note the only guarantee on beta<i> is that it's
  greater than 0.) Then do a simplified loop to price a candidate.
*/
  if (!withintol(dy_rho[xipos],rhoi,dy_tols->reframe*(1+rhoi)))
  { 
#   ifndef NDEBUG
    if (dy_opts->print.pivoting >= 1 || dy_opts->print.dual >= 1)
    { outfmt(logchn,gtxecho,
	     "\n  %s: (%s)%d: resetting DSE norms; trigger %s (%d), pos'n %d",
	     dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
	     consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,xipos) ;
      outfmt(logchn,gtxecho,
	     "\n\texact rho<i> = %g, approx = %g, error = %g, tol = %g.",
	     rhoi,dy_rho[xipos],fabs(rhoi-dy_rho[xipos]),
	     dy_tols->reframe*rhoi) ; }
#   endif
    dy_dseinit() ;
    for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
    { xkndx = dy_basis[xkpos] ;
      xkstatus = dy_status[xkndx] ;
      if (flgoff(xkstatus,vstatBLLB|vstatBUUB))
      { 
#       ifndef NDEBUG
	if (dy_opts->print.pricing >= 3)
	{ outfmt(logchn,gtxecho,"\n\tpricing %s (%d), status %s; << status >>",
		   consys_nme(dy_sys,'v',xkndx,TRUE,NULL),xkndx,
		   dy_prtvstat(xkstatus)) ; }
#       endif
	continue ; }
      dualpricexk(xkndx,candxi,&candbbari,&pivreject) ; } }

/*
  If we didn't recalculate from scratch, open a loop to walk the basis and
  update the row norms rho<k>. After we update each norm, call dualpricexk to
  price the variable and replace the current candidate if that's appropriate.
  Variables that are within bound can be rejected out-of-hand.
*/
  else
  { dy_rho[xipos] = rhoi ;
    betak = NULL ;
    for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
    { xkndx = dy_basis[xkpos] ;
      if (xkpos == xipos)
      { alphak = 1/abarij ;
	setcleanzero(alphak,dy_tols->zero) ;
	rhok = alphak*alphak*rhoi ; }
      else
      { alphak = abarj[xkpos]/abarij ;
	setcleanzero(alphak,dy_tols->zero) ;
	rhok = dy_rho[xkpos] - 2*alphak*tau[xkpos] + alphak*alphak*rhoi ; }
      setcleanzero(rhok,dy_tols->zero) ;
#     ifdef CHECK_DSE_UPDATES
      if (check_dse_update (xkndx,0.0,rhok) == FALSE) return (dyrFATAL) ;
#     endif
/*
  rho<k> should not be less than 0, but as a practical matter, it can take
  some pretty wild swings. When wierdness strikes, just recalculate.  Warn
  the user, if they're interested. A high zero tolerance can trigger lots
  of recalculations if the basis norms get down toward 10^-3 or 10^-4 (in
  which case rho is on the order of 10^-6 to 10^-8).
*/
      if (rhok <= dy_tols->bogus*dy_tols->zero)
      { if (betak == NULL)
	{ betak = (double *) CALLOC(dy_sys->concnt+1,sizeof(double)) ; }
	else
	{ memset(betak,0,(dy_sys->concnt+1)*sizeof(double)) ; }
	betak[xkpos] = 1.0 ;
	dy_btran(betak) ;
	cbark = exvec_ssq(betak,dy_sys->concnt) ;
#       ifndef NDEBUG
	if (dy_opts->print.dual >= 1 &&
	    ( rhok <= 0 || fabs(rhok-cbark) > dy_tols->zero))
	{ warn(393,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	       dy_lp->tot.iters+1,xkpos,rhok,xkpos,dy_rho[xkpos],xkpos,cbark,
	       dy_rho[xkpos]-cbark) ; }
#	endif
	dy_rho[xkpos] = cbark ; }
      else
      { dy_rho[xkpos] = rhok ; }
      xkstatus = dy_status[xkndx] ;
      if (flgoff(xkstatus,vstatBLLB|vstatBUUB))
      { 
#       ifndef NDEBUG
	if (dy_opts->print.pricing >= 3)
	{ outfmt(logchn,gtxecho,"\n\tpricing %s (%d), status %s; << status >>",
		   consys_nme(dy_sys,'v',xkndx,TRUE,NULL),xkndx,
		   dy_prtvstat(xkstatus)) ; }
#       endif
	continue ; }
      dualpricexk(xkndx,candxi,&candbbari,&pivreject) ; }
    if (betak != NULL) FREE(betak) ; }
/*
  That's it for the row norms. On to the primal reduced costs. We have to
  update cbar<k> for all nonbasic variables x<k>.  When antidegeneracy is
  active, we update only the columns involved in the restricted subproblem.
  (If we're nonparanoid, we can also skip the update for NBFX, which will
  never be considered for reentry. But the paranoid check routines get very
  upset.)

  The update formulae are:

    cbar'<i> = -cbar<j>/abar<ij>
    cbar'<k> = cbar<k> - cbar<j>*alpha<k>
  
  where alpha<k> = abar<i,k>/abar<i,j>.

  To avoid a special case for x<i>, now nonbasic, note that prior to the
  pivot abar<i,i> = inv(B)a<i> = 1.0 and cbar<i> = 0.0 (since x<i> was
  basic). Set these values prior to entering the loop, and the general
  update formula for cbar'<k> collapses properly to the special case for
  cbar'<i>, hence we don't need to check for x<i> in the loop.

  For x<j>, now basic, just set the reduced cost to 0.
*/
  dy_cbar[xjndx] = 0.0 ;
  abari[xindx] = 1.0 ;
  dy_cbar[xindx] = 0.0 ;
  for (xkndx = 1 ; xkndx <= dy_sys->varcnt ; xkndx++)
  { if (dy_lp->degen > 0 && dy_ddegenset[xkndx] != dy_lp->degen) continue ;
    xkstatus = dy_status[xkndx] ;
#   ifdef PARANOIA
    if (flgon(xkstatus,vstatBASIC)) continue ;
#   else
    if (flgon(xkstatus,vstatBASIC|vstatNBFX)) continue ;
#   endif
    abarik = abari[xkndx] ;
    deltak = cbarj*abarik/abarij ;
    if (deltak != 0)
    { cbark = dy_cbar[xkndx]-deltak ;
      tolk = snaptol3(dy_tols->cost,0,fabs(deltak)) ;
      setcleanzero(cbark,tolk) ;
#     ifndef NDEBUG
      if ((flgon(xkstatus,vstatNBLB) && cbark < -dy_tols->dfeas) ||
	  (flgon(xkstatus,vstatNBUB) && cbark > dy_tols->dfeas))
      { if (dy_opts->print.dual >= 3)
	{ outfmt(logchn,gtxecho,
		 "\n      lost dual feasibility, %s (%d) %s,",
		 consys_nme(dy_sys,'v',xkndx,FALSE,NULL),xkndx,
		 dy_prtvstat(xkstatus)) ;
	  outfmt(logchn,gtxecho,
		 " old = %g, new = %g, abarij =  %g, delta = %g, tol = %g .",
		 dy_cbar[xkndx],cbark,abarij,deltak,dy_tols->dfeas) ; } }
#     endif
      dy_cbar[xkndx] = cbark ;
      if ((flgon(xkstatus,vstatNBLB) && cbark < -dy_tols->dfeas) ||
	  (flgon(xkstatus,vstatNBUB) && cbark > dy_tols->dfeas))
      { 
#	ifndef NDEBUG
	warn(347,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	     dy_lp->tot.iters,consys_nme(dy_sys,'v',xkndx,FALSE,NULL),xkndx,
	     dy_prtvstat(xkstatus),xkndx,cbark,dy_tols->dfeas) ;
#       endif
	retval = dyrLOSTDFEAS ; } }
#   ifdef CHECK_DSE_UPDATES
    else
    { cbark = dy_cbar[xkndx] ; }
    if (check_dse_update(xkndx,cbark,0.0) == FALSE) return (dyrFATAL) ;
#   endif
  }
/*
  We're done. Time to set the proper return value. One possibility is that
  we've lost dual feasibility, and retval is set to dyrLOSTDFEAS.  If retval
  is still dyrINV, look at x<i>. If we have a new candidate to leave, set the
  return value to dyrOK. If we don't have a candidate, there are three
  possible reasons:
   * We have potential pivots on the reject list:
     pivreject == TRUE. We want to return dyrPUNT.
   * We're optimal.
     pivreject == FALSE. dyrOPTIMAL is the proper return value.
   * We saw some infeasible variables, but the infeasibility was within the
     bogus number tolerance.
     pivreject == FALSE. dyrOPTIMAL is still the correct return code (we'll
     end up doing a refactor in preoptimality, and the bogus numbers will
     either disappear, or we'll be back here ready to use them).

  If we're running multipivoting, we can cope with loss of dual feasibility.
*/
  if (retval == dyrLOSTDFEAS)
  { if (dy_opts->dpsel.strat > 0) retval = dyrINV ; }
  if (retval == dyrINV)
  { if (*candxi == 0)
    { if (pivreject == TRUE)
	retval = dyrPUNT ;
      else
	retval = dyrOPTIMAL ; }
    else
    { retval = dyrOK ; } }
 
# ifndef NDEBUG
  if (dy_opts->print.pricing >= 2)
  { if (*candxi != 0)
    { outfmt(logchn,gtxecho,
	     "\n    (%s)%d: selected %s (%d) %s to leave, DSE price %g.",
	     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
	     consys_nme(dy_sys,'v',*candxi,TRUE,NULL),*candxi,
	     dy_prtvstat(dy_status[*candxi]),candbbari) ; }
    else
    { outfmt(logchn,gtxecho,"\n    (%s)%d: no suitable candidates.",
	     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters) ; } }
  if (dy_opts->print.pricing >= 1)
  { if (retval == dyrPUNT)
    { outfmt(logchn,gtxecho,
	     "\n    (%s)%d: all suitable x<i> on rejected pivot list.",
	     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters) ; } }
# endif

  return (retval) ; }



static dyret_enum dualupdate (int xjndx, int indir,
			      int xindx, int outdir,
			      double *abarj, double *p_delta, double *betai)

/*
  This routine is responsible for updating the various data structures which
  hold the basis, primal variable values and status, and dual variable values.

  Note that both abarj and betai are calculated based on the basis prior to
  pivoting, since this is what is used in the update formulas.

  Unfortunately, one problem we can't get over is dirty degeneracy. It may
  well be that x<i> fails the primal feasibility test, but x<i>/abar<ij>
  is less than the zero tolerance. As in the primal case, we force the pivot
  and let the various error correction mechanisms take over.

  We can't bail out in the middle of this routine if we think we've seen a
  bogus value (the basis is left with inconsistent status and will fail the
  subsequent status check).

  Parameters:
    xjndx:	index of the entering variable x<j>
    indir:	the direction of change of the entering variable
		  +1: rising (usually from lower bound)
		  -1: falling (usually from upper bound)
    xindx:	index of the leaving variable x<i>
    outdir:	the direction of change of the outgoing variable
    abarj:	the ftran'd column abar<j> = inv(B)a<j>
    p_delta:	(o) absolute value of change in x<j>
    betai:	row i of inv(B)

  Returns: dyrOK if there are no glitches, dyrSWING for excessive growth of a
	   primal variable value, dyrREQCHK if one of the newly
	   calculated values looks bogus. dyrFATAL can be returned if we're
	   paranoid and fail a check.
*/

{ int xkpos,xkndx,xipos ;
  double deltai,xi,lbi,ubi ;
  double xj,cbarj,deltaj,abarij,ubj,lbj ;
  double deltak,yk ;
  double epsl,epsu,eps0 ;
  flags stati,statj,statk ;
  dyret_enum retval,upd_retval ;
  bool swing ;
  int swingndx ;
  double swingratio,maxswing ;
  char *rtnnme = "dualupdate" ;


  retval = dyrOK ;
  swing = FALSE ;
  maxswing = 0 ;
  swingndx = -1 ;

  statj = dy_status[xjndx] ;
  xj = dy_x[xjndx] ;
  lbj = dy_sys->vlb[xjndx] ;
  ubj = dy_sys->vub[xjndx] ;
  cbarj = dy_cbar[xjndx] ;

  xipos = dy_var2basis[xindx] ;
  stati = dy_status[xindx] ;
  xi = dy_xbasic[xipos] ;
  lbi = dy_sys->vlb[xindx] ;
  ubi = dy_sys->vub[xindx] ;
  abarij = abarj[xipos] ;

# ifdef PARANOIA
/*
  The incoming variable should have status NBLB or NBUB. The dual simplex
  isn't prepared to deal with SB or NBFR (not dual feasible) or NBFX
  (shouldn't be entering).

  The outgoing variable should have status BUUB or BLLB (i.e., it's primal
  infeasible).
*/
  if (!flgon(statj,vstatNBLB|vstatNBUB))
  { errmsg(355,rtnnme,dy_sys->nme,consys_nme(dy_sys,'v',xjndx,FALSE,NULL),
	   xjndx,dy_prtvstat(statj)) ;
    return (dyrFATAL) ; }
  if (!flgon(stati,vstatBLLB|vstatBUUB))
  { errmsg(349,rtnnme,dy_sys->nme,
	   consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,xi,
	   dy_prtvstat(stati),lbi,ubi) ;
    return (dyrFATAL) ; }
/*
  If x<i> is within bounds, we're confused -- what are we doing trying to
  pivot on a primal variable that's already feasible?
*/
  if (withinbnds(lbi,xi,ubi))
  { errmsg(358,rtnnme,dy_sys->nme,consys_nme(dy_sys,'v',xindx,FALSE,NULL),
	   xindx,lbi,xi,ubi,abarij) ;
    return (dyrFATAL)  ; }
# endif

/*
  A little setup -- extract the pivot and calculate the actual change in
  x<j> required to drive x<i> to bound.
*/
  if (outdir > 0)
  { deltai = lbi-xi ; }
  else
  { deltai = ubi-xi ; }
  setcleanzero(deltai,dy_tols->zero) ;
  if (deltai != 0)
  { deltaj = -deltai/abarij ;
    setcleanzero(deltaj,dy_tols->zero) ;
#   ifndef NDEBUG
    if (deltaj == 0 && dy_opts->print.pivoting >= 1)
    { outfmt(logchn,gtxecho,
	   "\n      %s (%d) = %g, %s, leaving at %s, dirty degenerate pivot.",
	     consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,xi,
	     dy_prtvstat(stati),(outdir < 0)?"ub":"lb") ; }
#   endif
  }
  else
  { deltaj = 0 ; }
  *p_delta = fabs(deltaj) ;

# ifndef NDEBUG
  if (dy_opts->print.dual >= 5)
  { outfmt(logchn,gtxecho,"\n  (%s)%d: dual update:",
	   dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters+1) ;
    outfmt(logchn,gtxecho,
	   "\n\t%s (%d) entering pos'n %d from %s %g, delta %g, cbarj %g.",
	   consys_nme(dy_sys,'v',xjndx,FALSE,NULL),xjndx,xipos,
	   (indir == 1)?"lb ":"ub ",xj,deltaj,cbarj) ;
    outfmt(logchn,gtxecho,"\n\t%s (%d) = %g leaving at ",
	   consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,xi) ;
    if (outdir == 1)
    { outfmt(logchn,gtxecho,"lb %g, pivot %g.",lbi,abarij) ; }
    else
    { outfmt(logchn,gtxecho,"ub %g, pivot %g.",ubi,abarij) ; } }
# endif

/*
  Update the objective and the value and status of the basic variables to
  reflect the change in x<j>. The calculation is straightforward, from the
  formulas:
    z = c<B>inv(B) - (c<j> - c<B>inv(B)a<j>)*delta
    x<B> = inv(B)b - (inv(B)a<j>)*delta
  dy_updateprimals does the heavy lifting. It should not return dyrFATAL here,
  as we're providing abarj, but the code is prepared for it.

  We can suppress the change to the objective if x<j> is in a restricted
  subproblem, because we know it got there with cbar<j> = 0. (Various paranoid
  accuracy checks are happier this way.)
*/
  if (deltaj != 0)
  { if (dy_ddegenset[xjndx] == 0)
    { dy_lp->z += cbarj*deltaj ; }
    upd_retval = dy_updateprimals(xjndx,deltaj,abarj) ;
    switch (upd_retval)
    { case dyrOK:
      { break ; }
      case dyrSWING:
      { swing = TRUE ;
	swingndx = dy_lp->ubnd.ndx ;
	maxswing = dy_lp->ubnd.ratio ;
	break ; }
      case dyrREQCHK:
      { retval = dyrREQCHK ;
	break ; }
      default:
      { errmsg(1,rtnnme,__LINE__) ;
	retval = dyrFATAL ;
	break ; } }
    stati = dy_status[xindx] ;
    xi = dy_x[xindx] ;

#   ifdef PARANOIA
/*
  Consider the result. x<i> should end up at bound (BLB, BUB, or BFX).
*/
    if (flgoff(stati,vstatBLB|vstatBFX|vstatBUB))
    { deltak = abarij*deltaj ;
      if (fabs(ubi-xi) < fabs(lbi-xi))
      { epsu = snaptol2(fabs(deltak),fabs(ubi)) ;
	if (fabs(ubi-xi) < 100*epsu)
	{ warn(357,rtnnme,dy_sys->nme,
	       consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,
	       dy_prtvstat(stati),"ub",ubi,xi,xi-ubi,epsu) ; }
	else
	{ errmsg(357,rtnnme,dy_sys->nme,
		 consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,
		 dy_prtvstat(stati),"ub",ubi,xi,xi-ubi,epsu) ;
	  return (dyrFATAL) ; } }
      else
      { epsl = snaptol2(fabs(deltak),fabs(lbi)) ;
	if (fabs(lbi-xi) < 100*epsl)
	{ warn(357,rtnnme,dy_sys->nme,
	       consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,
	       dy_prtvstat(stati),"lb",lbi,xi,lbi-xi,epsl) ; }
	else
	{ errmsg(357,rtnnme,dy_sys->nme,
		 consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx,
		 dy_prtvstat(stati),"lb",lbi,xi,lbi-xi,epsl) ;
	  return (dyrFATAL) ; } } }
#   endif
  }
/*
  The `dirty degeneracy' case. Force x<i> to the appropriate (basic) status.
  Other basic variables are unchanged.
*/
  else
  { if (lbi == ubi)
    { stati = vstatBFX ; }
    else
    if (outdir > 0)
    { stati = vstatBLB ; }
    else
    { stati = vstatBUB ; }
    dy_status[xindx] = stati ; }

  if (retval == dyrFATAL) return (dyrFATAL) ;

/*
  Deal with the entering and leaving variables.
  
  The first thing we do is update dy_basis and dy_var2basis.  The leaving
  variable x<i> still appeared to be basic as far as updateprimals was
  concerned, so all that remains is to change from basic to nonbasic status
  and force the value exactly to bound. If we've come a long way to get to
  bound, we might miss by a bit. 
*/
  dy_var2basis[xjndx] = xipos ;
  dy_var2basis[xindx] = 0 ;
  dy_basis[xipos] = xjndx ;

  if (flgon(dy_status[xindx],vstatBFX))
  { stati = vstatNBFX ;
    xi = lbi ; }
  else
  if (flgon(dy_status[xindx],vstatBLB))
  { stati = vstatNBLB ;
    xi = lbi ; }
  else
  if (flgon(dy_status[xindx],vstatBUB))
  { stati = vstatNBUB ;
    xi = ubi ; }
  else
  { if (fabs(xi-lbi) < fabs(xi-ubi))
    { stati = vstatNBLB ;
      xi = lbi ; }
    else
    { stati = vstatNBUB ;
      xi = ubi ; }
    retval = dyrREQCHK ; }
  dy_status[xindx] = stati ;
  dy_x[xindx] = xi ;
/*
  For the entering variable x<j>, it's a matter of retrieving the value,
  adding deltaj, and setting the appropriate status. Because dylp
  occasionally wants to make nonstandard dual pivots, we need to allow for
  the possibility that x<j> will enter and go out of bound.
*/
  if (deltaj != 0)
  { if (flgon(statj,vstatNBLB))
    { xj = lbj+deltaj ; }
    else
    { xj = ubj+deltaj ; }
    eps0 = snaptol1(fabs(deltaj)) ;
    setcleanzero(xj,eps0) ;
/*
  Choose a new status for x<j>. The tests for abovebnd and belowbnd will use
  the feasibility tolerance.  If newxk is within the snap tolerance of the
  bound, force it to be exactly at the bound. If the variable should be
  fixed, force it to bound even if it's only within feasibility distance.
*/
    epsu = snaptol2(ubj,fabs(deltaj)) ;
    epsl = snaptol2(lbj,fabs(deltaj)) ;
    if (statj == vstatBFR)
    { statj = vstatBFR ; }
    else
    if (belowbnd(xj,ubj))
    { if (abovebnd(xj,lbj))
      { statj = vstatB ; }
      else
      if (belowbnd(xj,lbj))
      { statj = vstatBLLB ; }
      else
      { statj = vstatBLB ; } }
    else
    { if (abovebnd(xj,ubj))
      { statj = vstatBUUB ; }
      else
      { statj = vstatBUB ; } }
    if (flgon(statj,vstatBLB|vstatBUB) && lbj == ubj)
    { statj = vstatBFX ; }
    if (dy_lp->iterf > 1 && dy_tols->bogus > 1.0 && xj != 0.0 &&
	flgoff(statj,vstatBLB|vstatBFX|vstatBUB))
    { if (fabs(xj) < eps0*dy_tols->bogus)
      { retval = dyrREQCHK ; 
#	ifndef NDEBUG
	if (dy_opts->print.pivoting >= 1)
	  warn(374,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	       dy_lp->tot.iters,"x",xjndx,fabs(xj),eps0*dy_tols->bogus,
	       eps0*dy_tols->bogus-xj) ;
#	endif
      } }
    if (flgon(statj,vstatBLLB|vstatBUUB))
    { swingratio = (fabs(xj)+1)/(fabs(dy_x[xjndx])+1) ;
      if (swingratio > dy_tols->swing)
      { swing = TRUE ;
	if (swingratio > maxswing)
	{ maxswing = swingratio ;
	  swingndx = xjndx ; } } } }
/*
  If we're dealing with a dirty degenerate pivot, the whole affair is much
  simpler --- just change the status of x<j> from nonbasic to basic.
*/
  else
  { if (flgon(statj,vstatNBLB))
    { statj = vstatBLB ;
      xj = lbj ; }
    else
    { statj = vstatBUB ;
      xj = ubj ; } }

  dy_status[xjndx] = statj ;
  dy_x[xjndx] = xj ;
  dy_xbasic[xipos] = xj ;

# ifdef PARANOIA
  { deltak = dy_calcobj() ;
    if (fabs(deltak-dy_lp->z) > fabs(.001*(1+deltak)))
    { warn(405,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	   dy_lp->tot.iters+1,dy_lp->z,deltak,fabs(dy_lp->z-deltak),
	   fabs(.001*deltak)) ; } }
# endif

/*
  Update the dual variables.  The derivation of these formulas is
  straightforward once you've seen it, but more than can be explained here.
  See the technical documentation or a text. The dual variables themselves
  are snapped to zero with the cost zero tolerance (typically stricter than
  the reduced cost tolerance, which is more designed to winnow the candidates
  to enter the basis), scaled by the amount of the change.

  If antidegeneracy is active, we want to be careful to only update duals
  associated with the restricted subproblem. These will be duals for
  constraints whose logicals are part of the subproblem. If we were in dual
  land, maintaining a dual basis, we'd have marked the basis pos'n and
  variables would slide in and out of it. But here, we have to move the
  marker from the leaving dual variable (xjndx) to the entering dual variable
  (xindx).
*/
  if (dy_lp->degen > 0)
  { xkpos = dy_ddegenset[xindx] ;
    dy_ddegenset[xindx] = dy_ddegenset[xjndx] ;
    dy_ddegenset[xjndx] = xkpos ; }

  deltaj =  cbarj/abarij ;
  setcleanzero(deltaj,dy_tols->zero) ;
  if (deltaj != 0)
  { for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
    { xkndx = dy_basis[xkpos] ;
      if (dy_lp->degen > 0 && dy_ddegenset[xkndx] < dy_lp->degen) continue ;
      deltak = deltaj*betai[xkpos] ;
      eps0 = dy_tols->cost*(1.0+fabs(deltak)) ;
      yk = dy_y[xkpos]+deltak ;
      setcleanzero(yk,eps0) ;
      if (yk != 0.0 && dy_lp->iterf > 1 && fabs(yk) < eps0*dy_tols->bogus)
      { retval = dyrREQCHK ;
  #     ifndef NDEBUG
	if (dy_opts->print.pivoting >= 1)
	  warn(374,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	       dy_lp->tot.iters,"y",xkpos,fabs(yk),eps0*dy_tols->bogus,
	       eps0*dy_tols->bogus-yk) ;
  #     endif
      }
      dy_y[xkpos] = yk ; } }
/*
  Decide on a return value. Swing overrides the others, as it'll cause us to
  pop out of simplex. But if there are no loadable constraints or variables,
  well, let's not, eh?
*/
  if (swing == TRUE)
  { if (dy_lp->sys.loadablecons == TRUE || dy_lp->sys.loadablevars == TRUE)
    { retval = dyrSWING ; }
    dy_lp->ubnd.ndx = swingndx ;
    dy_lp->ubnd.ratio = maxswing ;
#   ifndef NDEBUG
    if (dy_opts->print.dual >= 2)
    { xkndx = dy_lp->ubnd.ndx ;
      statk = dy_status[xkndx] ;
      outfmt(logchn,gtxecho,
	     "\n    Pseudo-unbounded: growth %e for %s (%d) %s = %g",
	     dy_lp->ubnd.ratio,consys_nme(dy_sys,'v',xkndx,FALSE,NULL),xkndx,
	     dy_prtvstat(statk),dy_x[xkndx]) ;
      if (flgon(statk,vstatBUUB))
      { outfmt(logchn,gtxecho," > %g.",dy_sys->vub[xkndx]) ; }
      else
      { outfmt(logchn,gtxecho," < %g.",dy_sys->vlb[xkndx]) ; } }
#   endif
  }

# ifndef NDEBUG
/*
  That's it, except for some informational printing.
*/
  if (dy_opts->print.dual >= 5)
  { 
  
    outfmt(logchn,gtxecho,"\n\trevised objective %g.",dy_lp->z) ;
#   ifdef PARANOIA
    deltak = dy_calcobj() ;
    if (!atbnd(deltak,dy_lp->z))
      outfmt(logchn,gtxecho,
	     "\n\tWHOOPS! updated obj - true obj = %g - %g = %g > %g",
	     dy_lp->z,deltak,dy_lp->z-deltak,dy_tols->dchk) ;
#   endif

    if (deltaj != 0)
    { outfmt(logchn,gtxecho,
	   "\n    revised dual variables, cbar tolerance %g",dy_tols->dfeas) ;
      outfmt(logchn,gtxecho,"\n%8s%20s%16s","pos'n","constraint","val") ;
      for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
      { if (betai[xkpos] != 0)
	{ outfmt(logchn,gtxecho,"\n%8d%20s%16.8g",xkpos,
	       consys_nme(dy_sys,'c',xkpos,FALSE,NULL),dy_y[xkpos]) ; } } } }
# if 0
    /* Now printed from dy_updateprimals. Note that decls need to be put
       back at top of block for use.
    */

  bool first,all ;

  if (dy_opts->print.dual >= 6)
  { if (dy_opts->print.dual >= 7)
      all = TRUE ;
    else
      all = FALSE ;
    first = TRUE ;
    for (xkpos = 1 ; xkpos <= dy_sys->concnt ; xkpos++)
      if (abarj[xkpos] != 0 || all == TRUE)
      { if (first == TRUE)
	{ outfmt(logchn,gtxecho,"\n\t%sprimal variables:",
		 (all == TRUE)?"":"revised ") ;
	  outfmt(logchn,gtxecho,"\n%8s%20s%16s%16s%16s %s",
		 "pos'n","var (ndx)","lb","val","ub","status") ;
	  first = FALSE ; }
	xkndx = dy_basis[xkpos] ;
	outfmt(logchn,gtxecho,"\n%8d%14s (%3d)%16.8g%16.8g%16.8g %s",xkpos,
	       consys_nme(dy_sys,'v',xkndx,FALSE,NULL),xkndx,
	       dy_sys->vlb[xkndx],dy_xbasic[xkpos],dy_sys->vub[xkndx],
	       dy_prtvstat(dy_status[xkndx])) ; }
    if (first == TRUE)
      outfmt(logchn,gtxecho,"\n\tno change to primal variables.") ; }
# endif /* 0 */

# endif /* NDEBUG */

  return (retval) ; }




dyret_enum dy_dualpivot (int xindx, int outdir,
			 int *p_xjndx, int *p_indir, double *p_cbarj,
			 double *p_abarij, double *p_delta, int *p_xicand)

/*
  This routine executes a dual pivot given the leaving variable x<i> (assume
  for simplicity that x<i> occupies basis posn i). The first action is to call
  dualpivrow to calculate beta<i> (row i of inv(B)) and abar<i> (row i of
  inv(B)N). Then dualin is called to select the entering variable x<j>. There
  follows some mathematical prep, then dy_pivot performs the actual pivot,
  followed by dualupdate and dseupdate to update the variables and DSE pricing
  information.

  Under normal circumstances, when dy_dualpivot is called x<j> is not
  specified. However, dy_dualpivot can be called from dy_dualaddvars when it
  attempts to activate and immediately pivot a variable that isn't dual
  feasible. In this case, dy_dualaddvars will specify x<j> at the call, and
  it may be the case that the entering variable is rising from its upper
  bound or falling from its lower bound. dualmultiin can also return a pivot
  of this nature --- it provides a way of dealing with the case where the
  reduced cost has the wrong sign, but is within the dual feasibility
  tolerance.


  Parameters:
    xindx:	index of the leaving variable x<i>
    outdir:	direction of motion of x<i>
		  1: rising to lower bound
		 -1: falling to upper bound
    p_xjndx:	(i) index of entering variable x<j>, or <= 0 if the entering
		    variable should be selected here
		(o) index of the entering variable x<j>
    p_indir:	(i) direction of motion of x<j>, if x<j> has been selected
		(o) direction of motion of x<j>
		  1: rising (usually from lower bound)
		 -1: falling (usually from upper bound)
    p_cbarj:	(o) reduced cost of entering variable
    p_abarij:	(o) pivot coefficient
    p_delta:	(o) change in x<j>
    p_xicand:	(o) index of the variable selected to leave on the next pivot
  
  Returns: dyret_enum code, as follows:
    successful pivots:
    dyrOK:	The pivot completed successfully and a new x<i> was selected.
    dyrDEGEN:	(dualin) A dual degenerate pivot completed successfully and
		a new x<i> was selected.
    dyrOPTIMAL:	(dseupdate) The pivot completed successfully, and no candidate
		x<i> could be selected because all variables are primal
		feasible.
    dyrPUNT:	(dseupdate) The pivot completed successfully, but no candidate
		x<i> could be selected because all candidates were flagged as
		NOPIVOT.
    dyrREQCHK:	(dualupdate) The pivot completed successfully, but a bogus
		value was calculated during primal and dual variable updating.

    unsuccessful (aborted) pivots:
    dyrREQCHK:	(dualin) The pivot coefficient is a bogus number, and a
		refactor is requested before it's used.
    dyrMADPIV:	The pivot coefficient was judged (numerically) unstable
		(dualin, possibly dy_pivot).
    dyrRESELECT: (dualmultiin) special circumstances have caused dualmultiin
		to abort the pivot and request reselection of the leaving
		variable.
    dyrUNBOUND:	The problem is dual unbounded (primal infeasible) (dualin).
    dyrLOSTDFEAS: Dual feasibility has been lost (dseupdate).
    dyrSINGULAR: The pivot resulted in a singular basis (dy_pivot).
    dyrBSPACE:	basis package ran out of room to work (dy_pivot).
    dyrFATAL:	Fatal confusion (data structure error, internal confusion,
		etc.) (various sources)
*/

{ int xipos,xjndx,indir,degen_cyclecnt ;
  double *betai,*abari,*tau,maxabari,*abarj ;
  dyret_enum retval,inretval,dseretval,confirm ;
  bool validxj,reselect ;
  flags factorflgs ;
  char *rtnnme = "dy_dualpivot" ;

  extern dyret_enum dualmultiin (int xindx, int outdir,
				 int *p_xjndx, int *p_indir,
				 double *abari, double maxabari,
				 double **p_abarj) ;

# ifdef PARANOIA
  dy_chkdual() ;
# endif

/*
  Setup. If the leaving variable isn't specified, assume a normal dual pivot
  with x<i> leaving at its near bound. If x<j> is specified, indir should also
  be valid.
*/
  retval = dyrINV ;
  if (*p_xjndx <= 0)
  { *p_xjndx = -1 ;
    *p_indir = 0 ;
    validxj = FALSE ; }
  else
  { validxj = TRUE ; }
  *p_cbarj = 0 ;
  *p_abarij = quiet_nan(0) ;
  *p_delta = quiet_nan(0) ;
  *p_xicand = -1 ;

  abarj = NULL ;

  xipos = dy_var2basis[xindx] ;

# ifndef NDEBUG
/*
  Print some information about the pivot, should the user want it.
*/
  if (dy_opts->print.pivoting >= 1)
  { outfmt(logchn,gtxecho,
	   "\n%s: x<%d> (%s) leaving pos'n %d (%s), status %s, %s from %g, ",
	   rtnnme,xindx,consys_nme(dy_sys,'v',xindx,FALSE,NULL),
	   xipos,consys_nme(dy_sys,'c',xipos,FALSE,NULL),
	   dy_prtvstat(dy_status[xindx]),(outdir < 0)?"decreasing":"increasing",
	   dy_x[xindx]) ;
    outfmt(logchn,gtxecho,"lb<i> = %g, ub<i> = %g.",
	   dy_sys->vlb[xindx],dy_sys->vub[xindx]) ; }
# endif
/*
  Do the prep work. Allocate some space for beta<i> and abar<i>, then call
  dualpivrow to do the calculations.
*/
  betai = (double *) CALLOC(dy_sys->concnt+1,sizeof(double)) ;
  abari = (double *) CALLOC(dy_sys->varcnt+1,sizeof(double)) ;
  if (dualpivrow(xipos,betai,abari,&maxabari) == FALSE)
  { errmsg(392,rtnnme,dy_sys->nme,dy_prtlpphase(dy_lp->phase,TRUE),
	   dy_lp->tot.iters,xipos,xipos,
	   consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx) ;
    FREE(betai) ;
    FREE(abari) ;
    return (dyrFATAL) ; }
/*
  If we don't have x<j>, try to select an entering variable. This can be simple
  selection of one variable for a standard pivot (dualin) or a more complex
  process which can do bound-to-bound flips of primal variables on the way to
  a final pivot (dualmultiin).
  
  A loop is required to handle the possible installation/removal of a
  restricted, perturbed subproblem as an antidegeneracy measure. There are some
  pathologies to deal with, hence the need for degen_cyclecnt and the check
  for an existing abarj.

  The possible results from entering variable selection are:
    * dyrOK, dyrDEGEN: a valid (if degenerate) pivot. xjndx and indir
      are valid. dyrDEGEN may cause the installation of a restricted
      perturbed subproblem, in which case the selection loop will iterate
      and we'll try again to select an entering variable.
    * dyrUNBOUND indicates dual unboundedness. If antidegeneracy is active,
      we'll remove one level and try again to select an entering variable. If
      antidegeneracy isn't active, we're really dual unbounded (primal
      infeasible) and will kick the problem back to the caller.
    * dyrREQCHK indicates that the selected pivot abar<ij> is a bogus number,
      and requests a refactor before we actually try to use it. dyrMADPIV
      indicates it failed the pivot stability test. dyrFATAL covers any
      remaining sins. All are returned to the caller.
*/
  if (validxj == FALSE)
  { reselect = TRUE ;
    degen_cyclecnt = 0 ;
    while (reselect)
    { if (abarj != NULL)
      { FREE(abarj) ;
	abarj = NULL ; }
      if (dy_opts->dpsel.strat > 0)
      { inretval = dualmultiin(xindx,outdir,&xjndx,&indir,
			       abari,maxabari,&abarj) ; }
      else
      { inretval = dualin(xindx,outdir,&xjndx,&indir,
			  abari,maxabari,&abarj) ; }
      switch (inretval)
      { 
/*
  dualin returns dyrOK

  We have an uncomplicated, nondegenerate pivot. Yeah!
*/
	case dyrOK:
	{ reselect = FALSE ;
	  break ; }
/*
  dualin returns dyrDEGEN

  Do we want (and are we allowed) to activate the antidegeneracy mechanism?
  If so, set up and perturb the restricted subproblem and then repeat the
  pivot selection. In order to create a restricted subproblem, opts.degen
  must permit it, and we must have executed opts.degenpivlim successive
  degenerate and nonconstructive pivots.

  The idea is to activate the antidegeneracy algorithm only when we have
  serious degeneracy involving dual constraints where we can perturb the
  objective coefficients (which we accomplish by the equivalent action of
  perturbing the values of the reduced costs). To this end, we exclude
  degenerate pivots where a fixed variable is leaving. The rules for selecting
  an incoming variable guarantee it won't come back.

  It's possible, when we fall through to the last case because we're forcing a
  degenerate pivot, that dualmultiin has not set abarj. Make sure we deal with
  it.
*/
	case dyrDEGEN:
	{ if (flgon(dy_status[xindx],vstatBFX))
	  { reselect = FALSE ;
#	    ifndef NDEBUG
	    if (dy_opts->print.degen >= 3)
	    { outfmt(logchn,gtxecho,
		     "\n      (%s)%d: constructive degenerate pivot.",
		     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters) ;
	      outfmt(logchn,gtxecho,"\n\t%s %s (%d) leaving,",
		     dy_prtvstat(dy_status[xindx]),
		     consys_nme(dy_sys,'v',xindx,FALSE,NULL),xindx) ;
	      outfmt(logchn,gtxecho," %s %s (%d) entering.",
		     dy_prtvstat(dy_status[xjndx]),
		     consys_nme(dy_sys,'v',xjndx,FALSE,NULL),xjndx) ; }
#	    endif
	  }
	  else
	  if (dy_opts->degen == TRUE && degen_cyclecnt == 0 &&
	      dy_opts->degenpivlim < dy_lp->degenpivcnt)
	  { 
#	    ifndef NDEBUG
	    if (dy_opts->print.pivoting >= 1 || dy_opts->print.degen >= 1)
	    { outfmt(logchn,gtxecho,
		     "\n  (%s)%d: antidegeneracy increasing to level %d.",
		     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
		     dy_lp->degen+1) ; }
#	    endif
	    dualdegenin() ;
	    degen_cyclecnt++ ; }
	  else
	  { reselect = FALSE ;
	    if (abarj == NULL)
	    { confirm = dy_confirmDualPivot(xindx,xjndx,
					    abari,maxabari,&abarj) ;
	      if (confirm == dyrOK)
	      { inretval = dyrDEGEN ; }
	      else
	      { inretval = confirm ; } }
#	    ifndef NDEBUG
	    if (dy_opts->print.degen >= 2 && degen_cyclecnt > 0)
	    { outfmt(logchn,gtxecho,
		     "\n    (%s)%d: forced degenerate pivot after %d cycles;",
		     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
		     degen_cyclecnt) ;
	      outfmt(logchn,gtxecho,"\n\t%s %s (%d) entering.",
		     dy_prtvstat(dy_status[xjndx]),
		     consys_nme(dy_sys,'v',xjndx,FALSE,NULL),xjndx) ; }
	    else
	    if (dy_opts->print.degen >= 3)
	    { outfmt(logchn,gtxecho,
		     "\n    (%s)%d: degenerate pivot; %s %s (%d) entering.",
		     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
		     dy_prtvstat(dy_status[xjndx]),
		     consys_nme(dy_sys,'v',xjndx,FALSE,NULL),xjndx) ; }
#	    endif
	  }
	  break ; }
/*
  dualin returns dyrUNBOUND

  Are we currently coping with degeneracy? If not, the problem is truly
  unbounded and we need to return to some higher level to deal with it.

  If there's a restricted subproblem installed, we've discovered a breakout
  direction from the degenerate vertex, and need to reselect the leaving
  variable after backing out the restricted subproblem. (Presumably we'll
  find a limiting variable in the full problem.)
*/
	case dyrUNBOUND:
	{ if (dy_lp->degen > 0)
	  {
#	    ifndef NDEBUG
	    if (dy_opts->print.pivoting >= 1 || dy_opts->print.degen >= 1)
	    { outfmt(logchn,gtxecho,
		 "\n  (%s)%d: backing out level %d after %d pivots, unbounded.",
		     dy_prtlpphase(dy_lp->phase,TRUE),dy_lp->tot.iters,
		     dy_lp->degen,
		     dy_lp->tot.pivs-degenstats.iterin[dy_lp->degen]) ; }
#	    endif
	    dy_dualdegenout(dy_lp->degen-1) ;
	    reselect = TRUE ; }
	  else
	  { reselect = FALSE ; }
	  break ; }
/*
  Remaining cases, and end of the pivot selection loop. If dualin returned
  anything other than dyrOK, dyrUNBOUND, or dyrDEGEN, it'll fall through to
  here and we'll punt back to the caller. dyrREQCHK and dyrMADPIV will have
  selected a pivot, but it's got problems. dualmultiin can on occasion request
  reselection of the leaving variable (dyrRESELECT).
*/
	case dyrRESELECT:
	case dyrMADPIV:
	case dyrREQCHK:
	case dyrFATAL:
	{ reselect = FALSE ;
	  break ; }
	default:
	{ errmsg(1,rtnnme,__LINE__) ;
	  reselect = FALSE ;
	  break ; } } }
/*
  We have a pivot (or an error). Set the return values.  We set p_delta to
  NaN here just to give it a value. The call to dualupdate calculates the
  authoritative value.
*/
    if (inretval == dyrOK || inretval == dyrDEGEN ||
	inretval == dyrREQCHK  || inretval == dyrMADPIV ||
	inretval == dyrRESELECT) 
    { *p_xjndx = xjndx ;
      *p_indir = indir ;
      *p_cbarj = dy_cbar[xjndx] ;
      *p_abarij = abari[xjndx] ;
      *p_delta = quiet_nan(0) ; } }
/*
  If dy_dualpivot was called with a valid x<j>, it's just a matter of making
  things look good here. Call dy_confirmDualPivot to get abarj and check the
  pivot's suitabillity
*/
  else
  { xjndx = *p_xjndx ;
    indir = *p_indir ;
    *p_cbarj = dy_cbar[xjndx] ;
    *p_abarij = abari[xjndx] ;
    inretval = dy_confirmDualPivot(xindx,xjndx,abari,maxabari,&abarj) ;
    if (inretval == dyrOK && dy_cbar[xjndx] == 0)
    { inretval = dyrDEGEN ; }
    *p_delta = quiet_nan(0) ; }
/*
  Send the errors back to the caller.
*/
  if (!(inretval == dyrOK || inretval == dyrDEGEN))
  { if (inretval == dyrMADPIV)
    { (void) dy_addtopivrej(xindx,dyrMADPIV,*p_abarij,maxabari) ; }
    FREE(betai) ;
    FREE(abari) ;
    if (abarj != NULL) FREE(abarj) ;
    return (inretval) ; }
/*
  Degenerate pivot? As with the primal, we'll take a pivot that moves a BFX
  variable out of the basis.
*/
  if (inretval == dyrOK)
  { dy_lp->degenpivcnt = 0 ; }
  else
  { if (flgon(dy_status[xindx],vstatBFX))
      dy_lp->degenpivcnt = 0 ;
    else
      dy_lp->degenpivcnt++ ; }

# ifndef NDEBUG
  if (dy_opts->print.pivoting >= 1)
  { outfmt(logchn,gtxecho,
	   "\n    x<%d> (%s) entering, status %s, %s from %s = %g, ",
	   xjndx,consys_nme(dy_sys,'v',xjndx,FALSE,NULL),
	   dy_prtvstat(dy_status[xjndx]),(indir < 0)?"decreasing":"increasing",
	   (flgon(dy_status[xjndx],vstatNBUB))?"ub":"lb",dy_x[xjndx]) ; }
# endif

/*
  Time to attempt the pivot. We first calculate a vector tau = inv(B)beta<i>,
  which we'll use in dseupdate.
*/
  tau = (double *) MALLOC((dy_sys->concnt+1)*sizeof(double)) ;
  memcpy(tau,betai,(dy_sys->concnt+1)*sizeof(double)) ;
  dy_ftran(tau,FALSE) ;
/*
  Attempt the pivot to update the LU factorisation. This can fail for three
  reasons: the pivot element didn't meet the numerical stability criteria
  (but we've already checked this), the pivot produced a singular basis, or
  the basis package ran out of space.

  If we fail, and we've done flips as part of a multipivot, we have a
  problem.  There's no easy way to back out the flips, and without the pivot
  we've almost certainly lost dual feasibility.  For dyrBSPACE, there's one
  thing we can try: refactor and recalculate the primals and duals, then try
  the pivot again. All this worked before, up to the pivot attempt, so assume
  it'll work again.

  For the rest, dylp's error recovery algorithms will cope, but it'll be ugly.
*/
  retval = dy_pivot(xipos,abarj[xipos],maxabari) ;
  if (retval == dyrBSPACE)
  { factorflgs = ladPRIMALS|ladDUALS ;
    dseretval = dy_factor(&factorflgs) ;
    if (dseretval == dyrOK)
    { FREE(abarj) ;
      abarj = NULL ;
      (void) consys_getcol_ex(dy_sys,xjndx,&abarj) ;
      dy_ftran(abarj,TRUE) ;
      retval = dy_pivot(xipos,abarj[xipos],maxabari) ; } }
/*
  The basis is successfully pivoted. Now we need to update the primal and
  dual variables. We'll use dualupdate for the job. If that works, call
  dseupdate to update dual steepest edge information and choose a leaving
  variable for the next iteration.

  In terms of return values, dualupdate can return dyrOK, dyrREQCHK,
  dyrSWING, or dyrFATAL. dyrFATAL will pop us out of simplex, so
  we can skip dseupdate.

  dseupdate can return dyrOK, dyrOPTIMAL, dyrLOSTDFEAS, dyrPUNT, or dyrFATAL.
  The only possibilities for inretval at this point are dyrOK and dyrDEGEN.
  Basically, we're trying to return the most interesting value, resorting to
  the bland dyrOK only if nothing else turns up.
*/
  if (retval == dyrOK)
  { dy_lp->pivok = TRUE ;
    retval = dualupdate(xjndx,indir,xindx,outdir,abarj,p_delta,betai) ;
    if (retval == dyrOK || retval == dyrREQCHK || retval == dyrSWING)
    { dseretval = dseupdate(xindx,xjndx,p_xicand,tau,betai,abari,abarj) ;
      if (dseretval != dyrOK) retval = dseretval ; }
    if (retval == dyrOK) retval = inretval ;
#   ifdef PARANOIA
    dy_chkdual() ;
#   endif
  }


/*
  Tidy up and return.
*/
  FREE(abarj) ;
  FREE(abari) ;
  FREE(betai) ;
  FREE(tau) ;

  return (retval) ; }

