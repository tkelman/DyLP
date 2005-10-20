# Makefile for dylp's standard support library, libDylpStdLib. 

# Uncomment one line only to build a static or shared library. The preferred
# choice here is static, so that it will be incorporated into the main dylp
# shared library.

#LibType := SHARED
LibType := STATIC

# Select optimization (-O or -g).

# Uncomment one line only to create a debugging (-g) or optimised (-O) build.
# -O will be automatically bumped up to the highest level of optimization the
# compiler supports. If want something in between then specify the exact level
# you want, e.g., -O1, -O2, etc.

OptLevel := -g
#OptLevel := -O

LIBNAME := DylpStdLib
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
	cd ../Utils ; $(MAKE) -f Makefile tools

library install: libdepend
	${MAKE} -f Makefile.DylpLib $@

# We need to set BOOLTYPE in order to suppress any attempt by Makefile.DylpLib
# to run booltype, which may not exist when we're just doing a clean.

clean distclean: 
	${MAKE} -f Makefile.DylpLib BOOLTYPE=bogus $@
