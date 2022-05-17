#pragma once
#include <boost/graph/connected_components.hpp>
#include <boost/graph/strong_components.hpp>

#include "federlieb/vtab.hxx"
#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_strong_components : public fl::vtab::base<vt_strong_components>
{
public:
  static inline char const* const name = "fl_strongly_connected_components";

  struct cursor
  {
    cursor(vt_strong_components* vtab);
    variant_graph<Directed> g_;
  };

  void xConnect(bool create);

  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
