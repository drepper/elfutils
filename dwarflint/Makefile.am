## Process this file with automake to create Makefile.in
##
## Copyright (C) 1996-2011 Red Hat, Inc.
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

bin_PROGRAMS = dwarflint locstats
noinst_PROGRAMS = tests/test-coverage tests/test-wrap tests/test-all-dies-it

dwarflint_SOURCES = \
	addr-record.cc addr-record.hh \
	all-dies-it.hh \
	check_registrar.hh check_registrar_i.hh \
	checkdescriptor.cc checkdescriptor.hh checkdescriptor_i.hh \
	checked_read.cc checked_read.hh \
	checkrule.cc checkrule.hh \
	checks.hh checks_i.hh \
	coverage.cc coverage.hh \
	cu_coverage.cc cu_coverage.hh cu_coverage_i.hh \
	dwarf_2.cc dwarf_2.hh \
	dwarf_3.cc dwarf_3.hh \
	dwarf_4.cc dwarf_4.hh \
	dwarf_gnu.cc dwarf_gnu.hh \
	dwarf_mips.cc dwarf_mips.hh \
	dwarf_version-imp.cc dwarf_version-imp.hh \
	dwarf_version.cc dwarf_version.hh dwarf_version_i.hh \
	dwarflint.cc dwarflint.hh dwarflint_i.hh \
	elf_file.hh elf_file_i.hh \
	expected-at.cc expected.hh \
	files.cc files.hh \
	highlevel_check.cc highlevel_check.hh highlevel_check_i.hh \
	main.cc \
	messages.cc messages.hh \
	misc.cc misc.hh \
	option.cc option.hh option_i.hh \
	pri.cc pri.hh \
	readctx.cc readctx.hh \
	reloc.cc reloc.hh \
	section_id.cc section_id.hh \
	sections.cc sections.hh sections_i.hh \
	where.c where.h \
	wrap.cc wrap.hh \
	\
	check_debug_abbrev.cc check_debug_abbrev.hh check_debug_abbrev_i.hh \
	check_debug_aranges.cc check_debug_aranges.hh check_debug_aranges_i.hh \
	check_debug_info.cc check_debug_info.hh check_debug_info_i.hh \
	check_debug_line.cc check_debug_line.hh check_debug_line_i.hh \
	check_debug_loc_range.cc check_debug_loc_range.hh check_debug_loc_range_i.hh \
	check_debug_pub.cc check_debug_pub.hh \
	check_die_tree.cc check_die_tree.hh check_die_tree_i.hh \
	check_duplicate_DW_tag_variable.cc \
	check_dups_abstract_origin.cc \
	check_expected_trees.cc \
	check_matching_ranges.cc \
	check_nodebug.cc \
	check_range_out_of_scope.cc \
	check_self_referential_die.cc \
	check_linkage_external_die.cc \
	check_die_decl_call.cc \
	lowlevel_checks.cc lowlevel_checks.hh \
	\
	../src/dwarfstrings.c

locstats_SOURCES = \
	locstats.cc \
	files.cc files.hh \
	option.cc option.hh option_i.hh \
	pri.cc pri.hh \
	where.c where.h

tests_test_coverage_SOURCES = tests/test-coverage.cc coverage.cc pri.cc \
	../src/dwarfstrings.c

tests_test_wrap_SOURCES = tests/test-wrap.cc wrap.cc
tests_test_all_dies_it_SOURCES = tests/test-all-dies-it.cc

EXTRA_TESTS = tests/run-debug_abbrev-duplicate-attribute.sh \
	tests/run-check_duplicate_DW_tag_variable.sh \
	tests/run-location-leaks.sh \
	tests/run-nodebug.sh \
	tests/run-check_range_out_of_scope.sh \
	tests/run-check_debug_info_refs.sh \
	tests/run-aranges_terminate_early.sh \
	tests/run-libdl-2.12.so.debug.sh \
	tests/run-test-all-dies-it.sh \
	tests/run-bad.sh \
	tests/run-check_self_referential_die.sh \
	tests/run-DW_AT_high_pc-relative.sh \
	tests/run-DW_AT_high_pc-below.sh \
	tests/run-DW_AT-later-version.sh \
	tests/run-upper.sh

TESTS = $(EXTRA_TESTS) \
	tests/test-coverage \
	tests/test-wrap

EXTRA_DIST = $(EXTRA_TESTS) \
	tests/debug_abbrev-duplicate-attribute.bz2 \
	tests/crc7.ko.debug.bz2 \
	tests/location-leaks.bz2 \
	tests/nodebug.bz2 \
	tests/check_range_out_of_scope-1.bz2 \
	tests/check_debug_info_refs-1.bz2 \
	tests/aranges_terminate_early.bz2
	tests/libdl-2.12.so.debug.bz2 \
	tests/hello.bad-1.bz2 \
	tests/hello.bad-3.bz2 \
	tests/empty-1.bz2 \
	tests/garbage-1.bz2 \
	tests/garbage-2.bz2 \
	tests/garbage-3.bz2 \
	tests/garbage-4.bz2 \
	tests/garbage-5.bz2 \
	tests/garbage-6.bz2 \
	tests/garbage-7.bz2 \
	tests/garbage-8.bz2 \
	tests/garbage-9.bz2 \
	tests/garbage-10.bz2 \
	tests/garbage-11.bz2 \
	tests/garbage-12.bz2 \
	tests/check_self_referential_die.bz2 \
	tests/DW_AT_high_pc-relative.bz2 \
	tests/DW_AT_high_pc-below.bz2 \
	tests/DW_AT-later-version.bz2 \
	tests/upper.bz2

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
locstats_LDADD  = $(libebl) $(libelf) $(libdwpp) $(libeu) $(libmudflap) -ldl
tests_test_coverage_LDADD  = $(libebl) $(libelf) $(libdwpp) $(libeu) $(libmudflap) -ldl
tests_test_all_dies_it_LDADD = $(libdwpp)

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
