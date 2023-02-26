#include <tuple>

#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

class fx_ordered_concat_agg : public fl::fx::base<fx_ordered_concat_agg>
{
public:
  static inline auto const name = "fl_ordered_concat_agg";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  void xStep(const std::string& value,
             const fl::value::variant& sort_key,
             const std::string& separator = ",");
  std::optional<std::string> xFinal();

protected:
  std::vector<std::tuple<std::string, fl::value::variant, std::string>> data_;
};
