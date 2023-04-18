#pragma once

#include "federlieb/federlieb.hxx"
#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_weak_components : public fl::vtab::base<vt_weak_components>
{
public:
  static inline char const* const name = "fl_weakly_connected_components";

  struct cursor
  {
    cursor(vt_weak_components* vtab);
    variant_graph<Undirected> g_;
  };

  void xConnect(bool create);

  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
