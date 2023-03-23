#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

class fx_infinity : public fl::fx::base<fx_infinity>
{
public:
  static inline auto const name = "fl_infinity";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  double xFunc(double which = 0);
};
