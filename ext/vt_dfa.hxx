#pragma once

#include <mutex>

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
    fl::db tmpdb_;
  };

  void xDisconnect(bool destroy);
  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);

  struct ref_counted_memory {
    int rc = 1;
    std::vector<fl::value::blob> dfas;
  };

  ref_counted_memory* memory_ = nullptr;

  static inline std::array<std::string, 8> shadow_tables = {
    "dfa", "nfastate", "dfastate", "nfatrans",
    "dfatrans", "via", "nfatrans_via", "dfatrans_via"
  };

};

class vt_dfa_view
  : public fl::vtab::base<vt_dfa_view>
{
public:
  static inline char const* const name = "fl_dfa_view";

  using result_type = fl::stmt;

  struct cursor
  {
    cursor(vt_dfa_view* vtab);
  };

  bool xBestIndex(fl::vtab::index_info& info);

  void xDisconnect(bool destroy);
  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);

  vt_dfa::ref_counted_memory* memory_ = nullptr;
};
