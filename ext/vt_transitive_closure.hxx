#pragma once

#include "federlieb/vtab.hxx"

#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_transitive_closure : public fl::vtab::base<vt_transitive_closure>
{
public:
  static inline char const* const name = "fl_transitive_closure";

  using result_type = std::list<std::array<fl::value::variant, 2>>;

  struct cursor
  {
    cursor(vt_transitive_closure* vtab);
    variant_graph<Directed> g_;
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
