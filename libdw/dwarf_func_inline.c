/* Convenience functions for handling DWARF descriptions of inline functions.
   Copyright (C) 2005,2006 Red Hat, Inc.
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

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under an Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) and to distribute linked
   combinations including the two.  Non-GPL Code permitted under this
   exception must only link to the code of Red Hat elfutils through those
   well defined interfaces identified in the file named EXCEPTION found in
   the source code files (the "Approved Interfaces").  The files of Non-GPL
   Code may instantiate templates or use macros or inline functions from
   the Approved Interfaces without causing the resulting work to be covered
   by the GNU General Public License.  Only Red Hat, Inc. may make changes
   or additions to the list of Approved Interfaces.  Red Hat's grant of
   this exception is conditioned upon your not adding any new exceptions.
   If you wish to add a new Approved Interface or exception, please contact
   Red Hat.  You must obey the GNU General Public License in all respects
   for all of the Red Hat elfutils code and other code used in conjunction
   with Red Hat elfutils except the Non-GPL Code covered by this exception.
   If you modify this file, you may extend this exception to your version
   of the file, but you are not obligated to do so.  If you do not wish to
   provide this exception without modification, you must delete this
   exception statement from your version and license this file solely under
   the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwP.h"
#include <dwarf.h>

struct visitor_info
{
  void *die_addr;
  int (*callback) (Dwarf_Die *, void *);
  void *arg;
};

static int
scope_visitor (unsigned int depth __attribute__ ((unused)),
	       struct Dwarf_Die_Chain *die, void *arg)
{
  struct visitor_info *const v = arg;

  /* This may relock for cache update.  But we give up the lock before
     handing over to a callback anyway, so it's not a big deal,
     performance-wise, that a couple functions that follow may do the
     relock again unnecessarily.  Correctness-wise, we cache no
     volatile data that would go invalid while the lock is not ours.
     (__libdw_visit_scopes assumes the visitor may relock.)  */
  if (__libdw_tag_rdlock (&die->die) != DW_TAG_inlined_subroutine)
    return DWARF_CB_OK;

  Dwarf_Attribute attr_mem;
  Dwarf_Attribute *attr = __libdw_attr_rdlock (&die->die, DW_AT_abstract_origin,
					       &attr_mem);
  if (attr == NULL)
    return DWARF_CB_OK;

  Dwarf_Die origin_mem;
  Dwarf_Die *origin = __libdw_formref_die_rdlock (attr, &origin_mem);
  if (origin == NULL)
    return DWARF_CB_ABORT;

  if (origin->addr != v->die_addr)
    return DWARF_CB_OK;

  rwlock_unlock (die->die.cu->dbg->lock);
  int retval = (*v->callback) (&die->die, v->arg);
  rwlock_rdlock (die->die.cu->dbg->lock);
  return retval;
}

int
dwarf_func_inline (Dwarf_Die *func)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Word val;
  if (INTUSE(dwarf_formudata) (INTUSE(dwarf_attr) (func, DW_AT_inline,
						   &attr_mem),
			       &val) == 0)
  switch (val)
    {
    case DW_INL_not_inlined:
      return 0;

    case DW_INL_declared_not_inlined:
      return -1;

    case DW_INL_inlined:
    case DW_INL_declared_inlined:
      return 1;
    }

  return 0;
}

int
dwarf_func_inline_instances (Dwarf_Die *func,
			     int (*callback) (Dwarf_Die *, void *),
			     void *arg)
{
  rwlock_rdlock (func->cu->dbg->lock);
  struct visitor_info v = { func->addr, callback, arg };
  struct Dwarf_Die_Chain cu = { .die = CUDIE (func->cu), .parent = NULL };
  int retval = __libdw_visit_scopes (0, &cu, &scope_visitor, NULL, &v);
  rwlock_unlock (func->cu->dbg->lock);

  return retval;
}
