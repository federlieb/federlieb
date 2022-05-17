#include "federlieb/fx.hxx"
#include "federlieb/json.hxx"

namespace fl = ::federlieb;

class fx_sha1 : public fl::fx::base<fx_sha1>
{
public:
  static inline auto const name = "fl_sha1";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  fl::value::blob xFunc(const fl::blob_type& value);
};
