#pragma once

#include "federlieb/vtab.hxx"

#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_dominator_tree : public fl::vtab::base<vt_dominator_tree>
{
public:
  static inline char const* const name = "fl_dominator_tree";

  using result_type = std::list<std::array<fl::value::variant, 3>>;

  struct cursor
  {
    cursor(vt_dominator_tree* vtab);
    variant_graph<Directed> g_;
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
