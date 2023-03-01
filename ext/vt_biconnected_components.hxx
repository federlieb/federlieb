#pragma once
#include <boost/graph/biconnected_components.hpp>

#include "federlieb/vtab.hxx"
#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_biconnected_components : public fl::vtab::base<vt_biconnected_components>
{
public:
  static inline char const* const name = "fl_biconnected_components";

  using result_type = std::list<std::array<fl::value::variant, 3>>;

  struct cursor
  {
    cursor(vt_biconnected_components* vtab);
    variant_graph<Undirected> g_;
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
