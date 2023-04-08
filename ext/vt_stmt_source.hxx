#pragma once

#include <boost/json.hpp>

#include "federlieb/vtab.hxx"
#include "federlieb/json.hxx"

namespace fl = ::federlieb;

class vt_stmt_source : public fl::vtab::base<vt_stmt_source>
{
public:
  static inline char const* const name = "fl_stmt_source";
  static inline bool const eponymous = true;

  using result_type = std::list<std::array<fl::value::variant, 6>>;

  struct cursor
  {
    cursor(vt_stmt_source* vtab) {}
  };

  void xConnect(bool create);
  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
