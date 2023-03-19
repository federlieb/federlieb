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
    fl::db tmpdb_;
  };

  void xDisconnect(bool destroy);
  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);

  struct ref_counted_memory {
    // NOTE: This is probably not threadsafe.
    std::atomic<int> rc = 1;
    std::vector<fl::value::blob> dfas;
  };

  ref_counted_memory* memory_ = nullptr;
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
