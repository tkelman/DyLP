
# Top level makefile for dylp. Edit as needed below to set the type of library
# optimisation level, and code options.

# svn/cvs: $Id$

# Available targets:
#  * library - builds the dylp library but does not install
#  * install - library, plus installs the dylp library in COIN/lib
#  * samples - install, plus builds a sample main program in Samples
#  * clean - removes binaries for current build settings (e.g., SunOS-g)
#  * distclean - removes all binaries

# Specify a shared (SHARED) or static (STATIC) library. Shared is the preferred
# choice unless you have some reason to want a static library.

export LibType := SHARED

# Select optimisation.

# Ask the compiler to include debugging information (-g) and/or optimisation
# (-O). These are not mutually exclusive, but be sure you understand what
# you're asking for if you ask for both. -O will be automatically bumped up to
# the highest level of optimisation the compiler supports. If want something in
# between then specify the exact level you want, e.g., -O1, -O2, etc.

#export OptLevel := -g
export OptLevel := -O

# Additional dylp compile-time options.

# You can request statistics (stats), paranoia (paranoia), and informational
# printing (info).  See the print documentation for an explanation of what each
# of these entails. NOTE that just changing DYLP_OPTIONS won't trigger a
# rebuild.  You'll need to do a `make clean' or `make distclean' to remove old
# binaries, then rebuild.

# To enable all three, you would say
#   export DYLP_OPTIONS := stats paranoia info
# To disable all three, you would say
#   export DYLP_OPTIONS :=
# A build which is optimised for speed disables all three.  Subsidiary
# makefiles test for the relevant strings and use them to set compilation
# flags. Informational printing and statistics are relatively cheap,
# computationally. Paranoia is expensive, particularly for large constraint
# systems. Informational printing will produce annoying warnings about
# numerical problems for some lps.

export DYLP_OPTIONS :=

# NOTE: Assignments to LibType, OptLevel, and DYLP_OPTIONS in subsidiary
# makefiles are conditional (`?='), so that any definition here (including an
# empty definition) will dominate. If you want finer control, edit the
# subsidiary makefiles.

export LIBNAME := Dylp

###############################################################################

# CoinDir should be the COIN installation directory, and MakefileDir the
# location of the COIN boilerplate makefiles.

export CoinDir := $(shell cd .. ; pwd)
export MakefileDir := $(CoinDir)/Makefiles

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default install clean distclean

default: install

###############################################################################

library install:
	@ cd Dylp ; ${MAKE} -f Makefile $@

samples:
	@ cd Samples ; ${MAKE} -f Makefile

clean distclean:
	@ cd Dylp ; ${MAKE} -f Makefile $@
	@ cd Lib ; ${MAKE} -f Makefile $@
	@ cd Utils ; ${MAKE} -f Makefile $@
	@ cd Samples ; ${MAKE} -f Makefile $@

