#pragma once

#include "federlieb/vtab.hxx"

#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_breadth_first_search : public fl::vtab::base<vt_breadth_first_search>
{
public:
  static inline char const* const name = "fl_breadth_first_search";

  using result_type = std::list<std::array<fl::value::variant, 4>>;

  struct cursor
  {
    cursor(vt_breadth_first_search* vtab);
    variant_graph<Directed> g_;
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
