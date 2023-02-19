#pragma once

#include <boost/json.hpp>

#include "federlieb/vtab.hxx"
#include "federlieb/json.hxx"

namespace fl = ::federlieb;

class vt_json_each : public fl::vtab::base<vt_json_each>
{
public:
  static inline char const* const name = "fl_json_each";
  static inline bool const eponymous = true;

  using result_type = std::list<std::array<fl::value::variant, 2>>;

  struct cursor
  {
    cursor(vt_json_each* vtab) {}
  };

  void xConnect(bool create);
  bool xBestIndex(fl::vtab::index_info& info);
  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
