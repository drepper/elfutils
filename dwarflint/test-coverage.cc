#include <iostream>
#include <cstdlib>
#include <cassert>
#include "coverage.hh"
#include "pri.hh"

bool fail = false;

void
cmpfmt (coverage const &cov,
	std::string const &exp)
{
  std::string act = cov::format_ranges (cov);
  if (act != exp)
    std::cerr << "FAIL: expected: " << exp << std::endl
	      << "           got: " << act << std::endl;
}

void
cmpholes (coverage const &cov,
	  std::string const &exp)
{
  uint64_t start = cov.front ().start;
  uint64_t len = cov.back ().end () - start;
  std::string act = cov::format_holes (cov, start, len);
  if (act != exp)
    std::cerr << "FAIL: expected: " << exp << std::endl
	      << "           got: " << act << std::endl;
}

void
chkcov (coverage const &cov, uint64_t start, uint64_t length)
{
  assert (cov.is_covered (start, length));
  assert (cov.is_overlap (start, length));
  for (uint64_t i = start; i < start + length; ++i)
    {
      assert (cov.is_covered (i, 1));
      for (unsigned k = 0; k < 100; ++k)
	{
	  assert (cov.is_overlap (i, k + 1));
	  if (i >= k)
	    {
	      assert (cov.is_overlap (i - k, k + 1));
	      assert (cov.is_overlap (i - k, 2*k + 1));
	    }
	}
    }
}

void
chkncov (coverage const &cov, uint64_t start, uint64_t length)
{
  assert (!cov.is_overlap (start, length));
  for (uint64_t i = start; i < start + length; ++i)
    {
      assert (!cov.is_covered (i, 1));
      assert (!cov.is_overlap (i, 1));
    }
}

class check_assert_used
{
  bool _m_used;

public:
  check_assert_used ()
    : _m_used (false)
  {
    assert (_m_used = true);
    if (!_m_used)
      abort ();
  }
};

int
main ()
{
  check_assert_used ();

  coverage cov;
  assert (cov.empty ());
  cmpfmt(cov, "");
  chkncov (cov, 0x0, 0x100);

  cov.add (0x10, 0x20);
  chkncov (cov, 0x0, 0x10);
  chkcov (cov, 0x10, 0x20);
  chkncov (cov, 0x30, 0x100);
  cmpfmt(cov, "[0x10, 0x30)");
  cmpholes(cov, "");

  cov.add (0x40, 0x20);
  chkncov (cov, 0x0, 0x10);
  chkcov (cov, 0x10, 0x20);
  chkncov (cov, 0x30, 0x10);
  chkcov (cov, 0x40, 0x20);
  chkncov (cov, 0x60, 0x100);
  cmpfmt(cov, "[0x10, 0x30), [0x40, 0x60)");
  cmpholes(cov, "[0x30, 0x40)");

  cov.add (0x50, 0x20);
  cmpfmt(cov, "[0x10, 0x30), [0x40, 0x70)");
  cmpholes(cov, "[0x30, 0x40)");

  cov.add (5, 1);
  cmpfmt(cov, "[0x5, 0x6), [0x10, 0x30), [0x40, 0x70)");
  cmpholes(cov, "[0x6, 0x10), [0x30, 0x40)");

  cov.add (5, 1);
  cmpfmt(cov, "[0x5, 0x6), [0x10, 0x30), [0x40, 0x70)");
  cmpholes(cov, "[0x6, 0x10), [0x30, 0x40)");

  cov.add (0, 5);
  cmpfmt(cov, "[0x0, 0x6), [0x10, 0x30), [0x40, 0x70)");
  cmpholes(cov, "[0x6, 0x10), [0x30, 0x40)");

  {
    coverage cov2 = cov;
    cov2.add (0, 0x40);
    cmpfmt(cov2, "[0x0, 0x70)");
  }
  cov.add (0, 0x30);
  cmpfmt(cov, "[0x0, 0x30), [0x40, 0x70)");
  cov.add (0x31, 5);
  cmpfmt(cov, "[0x0, 0x30), [0x31, 0x36), [0x40, 0x70)");

  assert (cov.remove (0x40, 0x30));
  cmpfmt(cov, "[0x0, 0x30), [0x31, 0x36)");
  assert (!cov.remove (0x30, 1));
  cmpfmt(cov, "[0x0, 0x30), [0x31, 0x36)");
  assert (cov.remove (0x2f, 3));
  cmpfmt(cov, "[0x0, 0x2f), [0x32, 0x36)");
  assert (cov.remove (0x10, 0x10));
  cmpfmt(cov, "[0x0, 0x10), [0x20, 0x2f), [0x32, 0x36)");
  assert (cov.remove (0x2, 3));
  cmpfmt(cov, "[0x0, 0x2), [0x5, 0x10), [0x20, 0x2f), [0x32, 0x36)");
  cmpholes(cov, "[0x2, 0x5), [0x10, 0x20), [0x2f, 0x32)");
  assert (cov.remove (0x1, 0x40));
  cmpfmt(cov, "[0x0, 0x1)");
  assert (cov.remove (0x0, 0x40));
  assert (cov.empty ());
  cmpfmt(cov, "");

  cov.add (0, 10);
  assert (cov == cov);
  assert (cov == coverage (cov));
  assert (cov == coverage (cov) + coverage (cov));
  assert ((coverage (cov) - coverage (cov)).empty ());

  cov.add (20, 0);
  cmpfmt (cov, "[0x0, 0xa), [0x14, 0x14)");
  chkcov (cov, 20, 0);
  chkcov (cov, 0, 0);
  chkcov (cov, 9, 0);
  chkcov (cov, 10, 0);
  chkncov (cov, 11, 0);
  chkncov (cov, 19, 1);
  chkncov (cov, 20, 1);
  chkncov (cov, 30, 0);
  cov.add (30, 10);
  cmpholes(cov, "[0xa, 0x14), [0x14, 0x1e)");

  if (fail)
    std::exit (1);
}
