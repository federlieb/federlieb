#pragma once

#include <boost/json.hpp>

#include "federlieb/vtab.hxx"
#include "federlieb/as.hxx"

namespace fl = ::federlieb;

class vt_stat1 : public fl::vtab::base<vt_stat1>
{
public:
  static inline char const* const name = "fl_stat1";
  static inline bool const eponymous = true;

  using result_type = std::list<std::array<fl::value::variant, 5>>;

  struct cursor
  {
    cursor(vt_stat1* vtab) {}
  };

  void xConnect(bool create);
  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
