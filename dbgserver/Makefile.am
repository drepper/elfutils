## Makefile.am for libdbgserver library subdirectory in elfutils.
##
## Process this file with automake to create Makefile.in
##
## Copyright (C) 2019 Red Hat, Inc.
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of either
##
##   * the GNU Lesser General Public License as published by the Free
##     Software Foundation; either version 3 of the License, or (at
##     your option) any later version
##
## or
##
##   * the GNU General Public License as published by the Free
##     Software Foundation; either version 2 of the License, or (at
##     your option) any later version
##
## or both in parallel, as here.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received copies of the GNU General Public License and
## the GNU Lesser General Public License along with this program.  If
## not, see <http://www.gnu.org/licenses/>.
##
include $(top_srcdir)/config/eu.am
AM_CPPFLAGS += -I$(srcdir) -I$(srcdir)/../libelf -I$(srcdir)/../libebl \
	   -I$(srcdir)/../libdw -I$(srcdir)/../libdwelf -Wno-format-truncation
VERSION = 1

if BUILD_STATIC
libasm = ../libasm/libasm.a
libdw = ../libdw/libdw.a -lz $(zip_LIBS) $(libelf) $(libebl) -ldl
libelf = ../libelf/libelf.a -lz
libdbgserver = ./libdbgserver.a
else
libasm = ../libasm/libasm.so
libdw = ../libdw/libdw.so
libelf = ../libelf/libelf.so
libdbgserver = ./libdbgserver.so
endif
libebl = ../libebl/libebl.a
libeu = ../lib/libeu.a

AM_LDFLAGS = -Wl,-rpath-link,../libelf:../libdw:.

bin_PROGRAMS = dbgserver
dbgserver_SOURCES = dbgserver.cxx
dbgserver_LDADD = $(libdw) $(libelf) $(libeu) $(libdbgserver) $(libmicrohttpd_LIBS) $(libcurl_LIBS) $(sqlite3_LIBS) $(libarchive_LIBS) -lpthread -ldl

noinst_LIBRARIES = libdbgserver.a
noinst_LIBRARIES += libdbgserver_pic.a

libdbgserver_a_SOURCES = dbgserver-client.c
libdbgserver_pic_a_SOURCES = dbgserver-client.c
am_libdbgserver_pic_a_OBJECTS = $(libdbgserver_a_SOURCES:.c=.os)

pkginclude_HEADERS = dbgserver-client.h

libdbgserver_so_LIBS = libdbgserver_pic.a
libdbgserver_so_LDLIBS = $(libcurl_LIBS)
libdbgserver_so_SOURCES = dbgserver-client.c
libdbgserver.so$(EXEEXT): $(srcdir)/libdbgserver.map $(libdbgserver_so_LIBS)
	$(AM_V_CCLD)$(LINK) $(dso_LDFLAGS) -o $@ \
		-Wl,--soname,$@.$(VERSION) \
		-Wl,--version-script,$<,--no-undefined \
		-Wl,--whole-archive $(libdbgserver_so_LIBS) -Wl,--no-whole-archive \
		$(libdbgserver_so_LDLIBS)
	@$(textrel_check)
	$(AM_V_at)ln -fs $@ $@.$(VERSION)

install: install-am libdbgserver.so
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	$(INSTALL_PROGRAM) libdbgserver.so $(DESTDIR)$(libdir)/libdbgserver-$(PACKAGE_VERSION).so
	ln -fs libdbgserver-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libdbgserver.so.$(VERSION)
	ln -fs libdbgserver.so.$(VERSION) $(DESTDIR)$(libdir)/libdbgserver.so

uninstall: uninstall-am
	rm -f $(DESTDIR)$(libdir)/libdbgserver-$(PACKAGE_VERSION).so
	rm -f $(DESTDIR)$(libdir)/libdbgserver.so.$(VERSION)
	rm -f $(DESTDIR)$(libdir)/libdbgserver.so
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(includedir)/elfutils

EXTRA_DIST = libdbgserver.map
MOSTLYCLEANFILES = $(am_libdbgserver_pic_a_OBJECTS) libdbgserver.so.$(VERSION)
CLEANFILES += $(am_libdbgserver_pic_a_OBJECTS) libdbgserver.so
