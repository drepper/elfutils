#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "checks-high.hh"

using elfutils::dwarf;

namespace
{
  class check_matching_ranges
    : public highlevel_check<check_matching_ranges>
  {
  public:
    explicit check_matching_ranges (dwarflint &lint);
  };

  reg<check_matching_ranges> reg_matching_ranges;
}

check_matching_ranges::check_matching_ranges (dwarflint &lint)
  : highlevel_check<check_matching_ranges> (lint)
{
  if (be_tolerant || be_gnu)
    throw check_base::unscheduled;

  lint.check<check_debug_ranges> ();
  lint.check<check_debug_aranges> ();

  try
    {
      struct where where_ref = WHERE (sec_info, NULL);
      struct where where_ar = WHERE (sec_aranges, NULL);
      where_ar.ref = &where_ref;
      struct where where_r = WHERE (sec_ranges, NULL);
      where_r.ref = &where_ref;
      char buf[128];

      const dwarf::aranges_map &aranges = dw.aranges ();
      for (dwarf::aranges_map::const_iterator i = aranges.begin ();
	   i != aranges.end (); ++i)
	{
	  const dwarf::compile_unit &cu = i->first;
	  where_reset_1 (&where_ref, 0);
	  where_reset_2 (&where_ref, cu.offset ());

	  std::set<dwarf::ranges::key_type>
	    cu_aranges = i->second,
	    cu_ranges = cu.ranges ();

	  typedef std::vector <dwarf::arange_list::value_type>
	    range_vec;
	  range_vec missing;
	  std::back_insert_iterator <range_vec> i_missing (missing);

	  std::set_difference (cu_aranges.begin (), cu_aranges.end (),
			       cu_ranges.begin (), cu_ranges.end (),
			       i_missing);

	  for (range_vec::iterator it = missing.begin ();
	       it != missing.end (); ++it)
	    wr_message (cat (mc_ranges, mc_aranges, mc_impact_3), &where_r,
			": missing range %s, present in .debug_aranges.\n",
			range_fmt (buf, sizeof buf, it->first, it->second));

	  missing.clear ();
	  std::set_difference (cu_ranges.begin (), cu_ranges.end (),
			       cu_aranges.begin (), cu_aranges.end (),
			       i_missing);

	  for (range_vec::iterator it = missing.begin ();
	       it != missing.end (); ++it)
	    wr_message (cat (mc_ranges, mc_aranges, mc_impact_3), &where_ar,
			": missing range %s, present in .debug_ranges.\n",
			range_fmt (buf, sizeof buf, it->first, it->second));
	}
    }
  // XXX more specific class when <dwarf> has it
  catch (std::runtime_error &exc)
    {
      throw check_base::failed
	(std::string ("Error while checking matching ranges:")
	 + exc.what () + ".\n");
    }
}
