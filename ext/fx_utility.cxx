#include "federlieb/federlieb.hxx"

#include "fx_utility.hxx"

namespace fl = ::federlieb;

double
fx_infinity::xFunc(double which)
{
  static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");  

  if (which < 0) {
    return -std::numeric_limits<double>::infinity();
  }

  return std::numeric_limits<double>::infinity();
}
