#pragma once

#include "federlieb/vtab.hxx"

#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_dijkstra_shortest_paths
  : public fl::vtab::base<vt_dijkstra_shortest_paths>
{
public:
  static inline char const* const name = "fl_dijkstra_shortest_paths";

  using result_type = std::list<std::array<fl::value::variant, 5>>;

  struct cursor
  {
    cursor(vt_dijkstra_shortest_paths* vtab);
    variant_graph<Directed> g_;
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
