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

  static inline std::array<std::pair<std::string, std::string>, 9> shadow_tables = {
    std::make_pair("dfa", "fl_dfa_view"),
    std::make_pair("nfastate", "fl_dfa_view"),
    std::make_pair("dfastate", "fl_dfa_view"),
    std::make_pair("nfatrans", "fl_dfa_view"),
    std::make_pair("dfatrans", "fl_dfa_view"),
    std::make_pair("via", "fl_dfa_view"),
    std::make_pair("nfatrans_via", "fl_dfa_view"),
    std::make_pair("dfatrans_via", "fl_dfa_view"),
    std::make_pair("dfastate_subset", "fl_dfastate_subset")
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

class vt_dfastate_subset
  : public fl::vtab::base<vt_dfastate_subset>
{
public:
  static inline char const* const name = "fl_dfastate_subset";

  using result_type = std::list<std::vector<fl::value::variant>>;

  struct cursor
  {
    cursor(vt_dfastate_subset* vtab);
  };

  bool xBestIndex(fl::vtab::index_info& info);

  void xDisconnect(bool destroy);
  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);

  vt_dfa::ref_counted_memory* memory_ = nullptr;
};
