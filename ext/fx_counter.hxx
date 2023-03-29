#include <map>

#include "federlieb/fx.hxx"
#include "federlieb/json.hxx"

namespace fl = ::federlieb;

class fx_counter : public fl::fx::base<fx_counter>
{
public:
  static inline auto const name = "fl_counter";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  int64_t xFunc(const std::string key, int64_t diff);

protected:
  std::map<std::string, int64_t> map_;
};
