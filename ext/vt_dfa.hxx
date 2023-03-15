#pragma once

#include "federlieb/vtab.hxx"
#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_dfa
  : public fl::vtab::base<vt_dfa>
{
public:
  static inline char const* const name = "fl_dfa";

  using result_type = fl::stmt;

  struct cursor
  {
    cursor(vt_dfa* vtab);
  };

  void xConnect(bool create);
  bool xBestIndex(fl::vtab::index_info& info);
  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);

  std::string path_;
  fl::db tmpdb_;
};
