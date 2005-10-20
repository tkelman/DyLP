
# Makefile for Dylp configuration utilities. You should not need to edit the
# makefiles in this directory.

###############################################################################

export CoinDir := $(shell cd ../.. ; pwd)
export MakefileDir := $(CoinDir)/Makefiles

# Use COIN boilerplate to determine build environment

include ${MakefileDir}/Makefile.coin

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default library clean distclean

# We're not really making a library here, but it's convenient to use lib as the
# target.

default: tools

###############################################################################

tools clean distclean doc:
	${MAKE} -f Makefile.Utils $@
