#pragma once
#include <boost/graph/biconnected_components.hpp>

#include "federlieb/vtab.hxx"
#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_articulation_points : public fl::vtab::base<vt_articulation_points>
{
public:
  static inline char const* const name = "fl_articulation_points";

  using result_type = std::list<std::array<fl::value::variant, 1>>;

  struct cursor
  {
    cursor(vt_articulation_points* vtab);
    variant_graph<Undirected> g_;
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
