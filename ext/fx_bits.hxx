#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

class fx_bit_ceil : public fl::fx::base<fx_bit_ceil>
{
public:
  static inline auto const name = "fl_bit_ceil";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  int64_t xFunc(uint64_t);
};
