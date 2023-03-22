#include <bit>

#include "federlieb/federlieb.hxx"

#include "fx_bits.hxx"

namespace fl = ::federlieb;

int64_t
fx_bit_ceil::xFunc(uint64_t value)
{
  return std::bit_ceil(value);
}
