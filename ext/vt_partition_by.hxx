#pragma once

#include "federlieb/vtab.hxx"

namespace fl = ::federlieb;

class vt_partition_by : public fl::vtab::base<vt_partition_by>
{
public:
  static inline char const* const name = "fl_partition_by";
  static inline int const needs_sqlite = 3038000;

  bool projections_validated = false;

  struct cursor
  {
    fl::db tmpdb_;
    size_t round_ = 1;

    cursor(vt_partition_by* vtab);
  };

  void xConnect(bool create);
  void refine(cursor* cursor);
  void validate_projections();
  void project(const std::string& sql, cursor* cursor);
  void apply_once_bys(cursor* cursor);
  void apply_then_bys(cursor* cursor);
  auto select_finals(cursor* cursor);
  auto select_history(cursor* cursor);
  bool xBestIndex(fl::vtab::index_info& info);
  fl::stmt subquery(const fl::vtab::index_info& info, cursor* cursor);
  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
