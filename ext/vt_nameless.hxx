#pragma once

#include "federlieb/vtab.hxx"
#include <regex>

namespace fl = ::federlieb;

/**
 *
 *
 */
class vt_nameless : public fl::vtab::base<vt_nameless>
{
public:
  static inline char const* const name = "fl_nameless";
  static inline bool const eponymous = true;

  struct cursor
  {
    cursor(vt_nameless* vtab) {}
  };

  void xConnect(bool create);
  bool xBestIndex(fl::vtab::index_info& info);
  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
