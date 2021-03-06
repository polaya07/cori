## -*- mode: makefile -*-

## 
## This file is part of the Score-P software (http://www.score-p.org)
##
## Copyright (c) 2009-2011, 
##    RWTH Aachen, Germany
##    Gesellschaft fuer numerische Simulation mbH Braunschweig, Germany
##    Technische Universitaet Dresden, Germany
##    University of Oregon, Eugene, USA
##    Forschungszentrum Juelich GmbH, Germany
##    German Research School for Simulation Sciences GmbH, Juelich/Aachen, Germany
##    Technische Universitaet Muenchen, Germany
##
## See the COPYING file in the package base directory for details.
##

## file       build-frontend/Makefile.am 
## maintainer Christian Roessel <c.roessel@fz-juelich.de>


## ACLOCAL_AMFLAGS contains options to pass to aclocal when aclocal.m4
## is to be rebuilt by make. This line is also used by autoreconf to
## run aclocal with suitable options, or by autopoint and gettextize
## to locate the place where Gettext's macros should be installed. So
## even if you do not really care about the rebuild rules, you should
## define ACLOCAL_AMFLAGS.
## For some reason this can't be moved to common.am.
ACLOCAL_AMFLAGS = -I ../vendor/common/build-config/m4

include ../build-includes/common.am

AM_CPPFLAGS += -DFRONTEND_BUILD -DCROSS_BUILD

include ../build-includes/frontend-only.am

clean-local:
	rm -rf ../test/jacobi


if SCOREP_HAVE_F77
if HAVE_OPENMP_SUPPORT
installcheck-local:
	@$(am__cd) $(DESTDIR)$(docdir)/example/ && $(MAKE) $(AM_MAKEFLAGS)
	@$(am__cd) $(DESTDIR)$(docdir)/example_user_instrumentation/ && $(MAKE) $(AM_MAKEFLAGS)
endif
else
if HAVE_OPENMP_SUPPORT
installcheck-local:
	@$(am__cd) $(DESTDIR)$(docdir)/doc/example/ && $(MAKE) $(AM_MAKEFLAGS) c
	@$(am__cd) $(DESTDIR)$(docdir)/doc/example_user_instrumentation/ && $(MAKE) $(AM_MAKEFLAGS) c
endif
endif

if HAVE_OPENMP_SUPPORT
install-example:
	$(MKDIR_P) $(DESTDIR)$(docdir)/example
	$(INSTALL_DATA) $(abs_srcdir)/../doc/example/example.*            $(DESTDIR)$(docdir)/example/
	$(INSTALL_DATA) $(abs_srcdir)/../include/opari2/pomp2_lib.h       $(DESTDIR)$(docdir)/example/
	$(INSTALL_DATA) $(abs_builddir)/../doc/example/Makefile           $(DESTDIR)$(docdir)/example/
	$(INSTALL_DATA) $(abs_builddir)/../build-frontend/.libs/libpomp.* $(DESTDIR)$(docdir)/example/
	rm -f $(DESTDIR)$(docdir)/example/libpomp.la
	$(MKDIR_P) $(DESTDIR)$(docdir)/example_user_instrumentation
	$(INSTALL_DATA) $(abs_srcdir)/../doc/example_user_instrumentation/example_user_instrumentation.*  $(DESTDIR)$(docdir)/example_user_instrumentation/
	$(INSTALL_DATA) $(abs_srcdir)/../include/opari2/pomp2_lib.h                                       $(DESTDIR)$(docdir)/example_user_instrumentation/
	$(INSTALL_DATA) $(abs_builddir)/../doc/example_user_instrumentation/Makefile                      $(DESTDIR)$(docdir)/example_user_instrumentation/
	$(INSTALL_DATA) $(abs_builddir)/../build-frontend/.libs/libpomp.*                                 $(DESTDIR)$(docdir)/example_user_instrumentation/
	rm -f $(DESTDIR)$(docdir)/example_user_instrumentation/libpomp.la
else
install-example:
endif

install-pomp2_region_info:
	$(MKDIR_P) $(DESTDIR)$(datadir)/@PACKAGE@/devel
	$(INSTALL_DATA) $(abs_srcdir)/../src/pomp-lib-dummy/pomp2_region_info.c $(DESTDIR)$(datadir)/@PACKAGE@/devel/
	$(INSTALL_DATA) $(abs_srcdir)/../src/pomp-lib-dummy/pomp2_region_info.h $(DESTDIR)$(datadir)/@PACKAGE@/devel/

install-data-local: install-example install-pomp2_region_info
