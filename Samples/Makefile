
# Makefile for Dylp sample main programs. You should not need to edit the
# makefiles in this directory.

# svn/cvs: $Id$

# The makefiles in this directory are written with a slightly different
# attitude than other makefiles in the Dylp distribution. The style is a bit
# more suited to building a project using dylp, rather than building dylp
# itself.

# OptLevel and DYLP_OPTIONS are normally set in the top level makefile. (Which
# see for explanations of what they do.) You can override the general setting
# here, if you want.  Each variable is conditionally set with `?='. This means
# the setting here will have an effect only if the variable is unset (as, for
# example, if you do a make in this directory, rather than from the top level).
# To override the top level setting, change `?=' to `:='.

export OptLevel ?= -g
export DYLP_OPTIONS ?= info stats

###############################################################################

# Use the COIN boilerplate to establish the build environment. As explained
# above, CoinDir and MakefileDir will normally be set in the top level
# makefile.

export CoinDir ?= $(shell cd ../.. ; pwd)
export MakefileDir ?= $(CoinDir)/Makefiles

# Among other variables, this will set DylpHome, DylpIncDir, DylpLibDir,
# DylpLibName, and DylpDefine, which will be used in Makefile.Samples.

include ${MakefileDir}/Makefile.coin

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default clean distclean

# Default is to make only the C main program, osi_dylp. If you're doing C++,
# you probably want to look at using the OsiDylp OSI layer.

default: osi_dylp

###############################################################################

osi_dylp osi+dylp: dylplibdepend
	@ ${MAKE} -f Makefile.Samples $@

odsi+dylp: dylplibdepend odsilibdepend
	@ ${MAKE} -f Makefile.Samples $@

# Better have the dylp library built before we try for the samples.

dylplibdepend:
	@ cd ../Dylp ; $(MAKE) -f Makefile install

# For odsi+dylp, we need OsiDylp and all the underlying Coin libraries.

odsilibdepend:
	@ cd $(CoinDir)/Osi/OsiDylp ; $(MAKE) -f Makefile

# We need to set BOOLTYPE nd FPCONFIGFLAGS in order to suppress execution of
# the configuration programs, which may not exist when we're just doing a
# clean.

clean distclean:
	@ ${MAKE} -f Makefile.Samples BOOLTYPE=bogus FPCONFIGFLAGS=bogus $@

