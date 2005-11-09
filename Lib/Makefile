# Makefile for dylp's standard support library, libDylpStdLib. 

# svn/cvs: $Id$

# OptLevel and DYLP_OPTIONS are normally set in the top level makefile. (Which
# see for explanations of what they do.) You can override the general setting
# here, if you want. Each variable is conditionally set with `?='. This means
# the setting here will have an effect only if the variable is undefined (as,
# for example, if you run make in this directory, rather than from the top
# level). To override the top level setting, change `?=' to `:='.

export OptLevel ?= -O
export DYLP_OPTIONS ?=

# LibType really needs to be static for libDylpStdLib, so the support routines
# will be sucked into libDylp when it's linked.

export LibType := STATIC

# And the support library name really shouldn't be changed either.

export LIBNAME := DylpStdLib

###############################################################################

# Use the COIN boilerplate to establish the build environment. As explained
# above, CoinDir and MakefileDir will normally be set in the top level
# makefile.

export CoinDir ?= $(shell cd ../.. ; pwd)
export MakefileDir ?= $(CoinDir)/Makefiles

include ${MakefileDir}/Makefile.coin

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default install clean distclean libdepend

# `make library' will build TARGETDIR/library.
# 'make install' will install a symbolic link in the current directory,
# pointing to TARGETDIR/library.

default: install

###############################################################################

# We need the configuration utilities in order to build DylpStdLib

libdepend:
	@ cd ../Utils ; $(MAKE) -f Makefile tools

library install: libdepend
	@ ${MAKE} -f Makefile.DylpLib $@

# We need to set BOOLTYPE nd FPCONFIGFLAGS in order to suppress execution of
# the configuration programs, which may not exist when we're just doing a
# clean.

clean distclean: 
	@ ${MAKE} -f Makefile.DylpLib BOOLTYPE=bogus FPCONFIGFLAGS=bogus $@
