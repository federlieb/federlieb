#pragma once

#include "federlieb/federlieb.hxx"
#include "federlieb/vtab.hxx"

namespace fl = ::federlieb;

class vt_script : public fl::vtab::base<vt_script>
{
public:
  static inline char const* const name = "fl_script";

  struct cursor
  {
    cursor(vt_script* vtab);
  };

  void xConnect(bool create);
  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
