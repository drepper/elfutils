/*
   Copyright (C) 2008,2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#include "config.h"

/* If true, we accept silently files without debuginfo.  */
bool tolerate_nodebug = false;

/* True if no message is to be printed if the run is succesful.  */
bool be_quiet = false; /* -q */
bool be_verbose = false; /* -v */
bool be_strict = false; /* --strict */
bool be_gnu = false; /* --gnu */
bool be_tolerant = false; /* --tolerant */
bool show_refs = false; /* --ref */
bool do_high_level = true; /* ! --nohl */
bool dump_die_offsets = false; /* --dump-offsets */

/* True if coverage analysis of .debug_ranges vs. ELF sections should
   be done.  */
bool do_range_coverage = false; // currently no option
