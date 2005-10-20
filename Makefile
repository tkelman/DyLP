
# Top level makefile for dylp. Edit as needed below to set the type of library
# and optimisation level.

# Available targets:
#  * library - builds the dylp library but does not install
#  * install - builds and installs the dylp library
#  * clean - removes binaries for current build settings (e.g., SunOS-g)
#  * distclean - removes all binaries

# Uncomment one line only to build a static or shared library. Shared is the
# preferred choice unless you have some reason to want a static library.

LibType := SHARED
#LibType := STATIC

export LibType

# Select optimization (-O or -g).

# Uncomment one line only to create a debugging (-g) or optimised (-O) build.
# -O will be automatically bumped up to the highest level of optimization the
# compiler supports. If want something in between then specify the exact level
# you want, e.g., -O1, -O2, etc.

OptLevel := -g
#OptLevel := -O

export OptLevel

# NOTE: Assignments to LibType and OptLevel in subsidiary makefiles are
# conditional (`?='). If you want finer control, do not assign values here
# and edit the subsidiary makefiles.

LIBNAME := Dylp

export LIBNAME

###############################################################################

# CoinDir should be the COIN installation directory. It's assumed that Dylp
# is at $(CoinDir)/Dylp.

export CoinDir := $(shell cd .. ; pwd)
export MakefileDir := $(CoinDir)/Makefiles

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default install clean distclean

default: install

###############################################################################

library install:
	cd Dylp ; ${MAKE} -f Makefile $@

clean distclean:
	cd Dylp ; ${MAKE} -f Makefile $@
	cd Lib ; ${MAKE} -f Makefile $@
	cd Utils ; ${MAKE} -f Makefile $@

