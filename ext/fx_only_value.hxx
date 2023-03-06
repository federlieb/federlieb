#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

class fx_only_value : public fl::fx::base<fx_only_value>
{
public:
  static inline auto const name = "fl_only_value";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  void xStep(const fl::value::variant value);
  fl::value::variant xFinal();

protected:
  std::optional<fl::value::variant> data_;
  bool failure_ = false;
};
