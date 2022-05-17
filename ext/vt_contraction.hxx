#pragma once

#include <set>
#include <stack>

#include "federlieb/federlieb.hxx"
#include "federlieb/vtab.hxx"

namespace fl = ::federlieb;

class vt_contraction : public fl::vtab::base<vt_contraction>
{
public:
  static inline char const* const name = "fl_iterated_contraction";

  struct cursor
  {
    fl::db tmpdb_;
    void import_edges(vt_contraction* vtab);
    void contract_vertices(vt_contraction* vtab);
    void contract_edges(vt_contraction* vtab);
    cursor(vt_contraction* vtab);
  };

  void xConnect(bool create);
  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
