#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

class fx_median : public fl::fx::base<fx_median>
{
public:
  static inline auto const name = "fl_median";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  void xStep(const double value);
  std::optional<double> xFinal();

protected:
  std::vector<double> data_;
};
