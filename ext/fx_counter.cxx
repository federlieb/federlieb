#include "federlieb/federlieb.hxx"

#include "fx_counter.hxx"

namespace fl = ::federlieb;

int64_t
fx_counter::xFunc(const std::string& key, int64_t diff)
{
  auto& value = map_[key];

  // NOTE: does not detect overflow
  value += diff;

  if (0 == value) {
    map_.erase(key);
  }

  return value;
}
