## Process this file with automake to create Makefile.in
##
## Copyright (C) 1996-2010 Red Hat, Inc.
## This file is part of Red Hat elfutils.
##
## Red Hat elfutils is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by the
## Free Software Foundation; version 2 of the License.
##
## Red Hat elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with Red Hat elfutils; if not, write to the Free Software Foundation,
## Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.
##
## Red Hat elfutils is an included package of the Open Invention Network.
## An included package of the Open Invention Network is a package for which
## Open Invention Network licensees cross-license their patents.  No patent
## license is granted, either expressly or impliedly, by designation as an
## included package.  Should you wish to participate in the Open Invention
## Network licensing program, please visit www.openinventionnetwork.com
## <http://www.openinventionnetwork.com>.
##
include $(top_srcdir)/config/eu.am
DEFS += $(YYDEBUG) -DDEBUGPRED=@DEBUGPRED@ \
	-DSRCDIR=\"$(shell cd $(srcdir);pwd)\" -DOBJDIR=\"$(shell pwd)\"
INCLUDES += -I$(srcdir)/../libelf -I$(srcdir)/../libebl \
	    -I$(srcdir)/../libdw -I$(srcdir)/../libdwfl \
	    -I$(srcdir)/../libasm

AM_LDFLAGS = -Wl,-rpath-link,../libelf:../libdw

no_mudflap.os = -fmudflap

bin_PROGRAMS = dwarflint

dwarflint_SOURCES = \
	main.cc \
	checkdescriptor.cc checkdescriptor.hh checkdescriptor.ii \
	dwarflint.cc dwarflint.hh misc.h \
	low.c low.h \
	expected-at.cc expected.hh \
	coverage.cc coverage.h \
	readctx.c  readctx.h \
	pri.cc pri.hh \
	messages.cc messages.h \
	section_id.cc section_id.h \
	where.c where.h \
	options.cc options.h \
	addr-record.cc addr-record.h \
	reloc.cc reloc.h \
	tables.cc tables.hh tables.h \
	all-dies-it.hh \
	checks.cc checks.hh checks.ii \
	sections.cc sections.hh sections.ii \
	highlevel_check.cc highlevel_check.hh \
	cu_coverage.cc cu_coverage.hh cu_coverage.ii \
	check_debug_abbrev.cc check_debug_abbrev.hh check_debug_abbrev.ii \
	check_debug_info.cc check_debug_info.hh check_debug_info.ii \
	check_debug_line.cc \
	check_debug_pub.cc \
	check_debug_loc_range.cc check_debug_loc_range.hh check_debug_loc_range.ii \
	check_debug_aranges.cc check_debug_aranges.hh \
	check_matching_ranges.cc \
	check_range_out_of_scope.cc \
	check_expected_trees.cc \
	check_dups_abstract_origin.cc \
	check_duplicate_DW_tag_variable.cc \
	../src/dwarfstrings.c

TESTS = tests/run-debug_abbrev-duplicate-attribute.sh \
	tests/run-check_duplicate_DW_tag_variable.sh

EXTRA_DIST = tests/run-debug_abbrev-duplicate-attribute.sh tests/run-debug_abbrev-duplicate-attribute.bz2

installed_TESTS_ENVIRONMENT = libdir=$(DESTDIR)$(libdir) \
			      bindir=$(DESTDIR)$(bindir) \
			      $(top_srcdir)/tests/test-wrapper.sh \
			      installed $(tests_rpath) \
			      $(program_transform_name)
if STANDALONE
TESTS_ENVIRONMENT = $(installed_TESTS_ENVIRONMENT)
else !STANDALONE
TESTS_ENVIRONMENT = $(top_srcdir)/tests/test-wrapper.sh \
		    ../libdw:../backends:../libelf:../libasm

installcheck-local:
	$(MAKE) $(AM_MAKEFLAGS) \
		TESTS_ENVIRONMENT='$(installed_TESTS_ENVIRONMENT)' check-TESTS
endif !STANDALONE

if BUILD_STATIC
libasm = ../libasm/libasm.a
libdw = ../libdw/libdw.a $(zip_LIBS) $(libelf) $(libebl) -ldl
libelf = ../libelf/libelf.a
else
libasm = ../libasm/libasm.so
libdw = ../libdw/libdw.so
libelf = ../libelf/libelf.so
endif
libebl = ../libebl/libebl.a
libeu = ../lib/libeu.a

# XXX later the C++ stuff will be in libdw.so directly
libdwpp = ../libdw/libdwpp.a $(libdw)

dwarflint_LDADD  = $(libebl) $(libelf) $(libdwpp) $(libeu) $(libmudflap) -ldl

installcheck-binPROGRAMS: $(bin_PROGRAMS)
	bad=0; pid=$$$$; list="$(bin_PROGRAMS)"; for p in $$list; do \
	  case ' $(AM_INSTALLCHECK_STD_OPTIONS_EXEMPT) ' in \
	   *" $$p "* | *" $(srcdir)/$$p "*) continue;; \
	  esac; \
	  f=`echo "$$p" | \
	     sed 's,^.*/,,;s/$(EXEEXT)$$//;$(transform);s/$$/$(EXEEXT)/'`; \
	  for opt in --help --version; do \
	    if LD_LIBRARY_PATH=$(DESTDIR)$(libdir) \
	       $(DESTDIR)$(bindir)/$$f $$opt > c$${pid}_.out 2> c$${pid}_.err \
	         && test -n "`cat c$${pid}_.out`" \
	         && test -z "`cat c$${pid}_.err`"; then :; \
	    else echo "$$f does not support $$opt" 1>&2; bad=1; fi; \
	  done; \
	done; rm -f c$${pid}_.???; exit $$bad

CLEANFILES += *.gconv

MAINTAINERCLEANFILES = 
