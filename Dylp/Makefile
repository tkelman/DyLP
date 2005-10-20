# Makefile for the dylp library, libDylp.

# Uncomment one line only to build a static or shared library. Shared is the
# preferred choice unless you have some reason to want a static library.

LibType := SHARED
#LibType := STATIC

# Select optimization (-O or -g).

# Uncomment one line only to create a debugging (-g) or optimised (-O) build.
# -O will be automatically bumped up to the highest level of optimization the
# compiler supports. If want something in between then specify the exact level
# you want, e.g., -O1, -O2, etc.

OptLevel := -g
#OptLevel := -O

LIBNAME := Dylp
export LIBNAME

###############################################################################

# Use the COIN boilerplate to establish the build environment.

export CoinDir := $(shell cd ../.. ; pwd)
export MakefileDir := $(CoinDir)/Makefiles

include ${MakefileDir}/Makefile.coin
include ${MakefileDir}/Makefile.location

TARGETDIR := $(UNAME)$(OptLevel)
export TARGETDIR
export OptLevel

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default install clean distclean libdepend

# `make library' will build TARGETDIR/library.
# 'make install' will install a symbolic link in the current directory,
# pointing to TARGETDIR/library.

default: install

###############################################################################

libdepend:
	cd ../Lib ; $(MAKE) -f Makefile install

library install: libdepend
	${MAKE} -f Makefile.Dylp $@

# We need to set BOOLTYPE in order to suppress any attempt by Makefile.Dylp
# to run booltype, which may not exist when we're just doing a clean. The
# same for FPCONFIGFLAGS and fpchecks.

clean distclean: 
	${MAKE} -f Makefile.Dylp BOOLTYPE=bogus FPCONFIGFLAGS=bogus $@
