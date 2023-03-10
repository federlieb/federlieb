#pragma once

#include "federlieb/db.hxx"
#include "federlieb/vtab.hxx"

namespace fl = ::federlieb;

/*

CREATE VIRTUAL TABLE USING stmt((
  SELECT ...
)
, cache=(:memory:)
, cache_key=cursor | (SELECT ...)
, expire=...
, mode=cursor|vtab
)

*/

class vt_stmt : public fl::vtab::base<vt_stmt>
{
public:
  static inline char const* const name = "fl_stmt";
  static inline bool const eponymous = false;

  struct cache
  {
    fl::db db;
    fl::stmt insert_meta_stmt;
    fl::stmt insert_data_stmt;
    fl::stmt update_refcount_stmt;
    std::string select_sql;

    void change_meta_refcount(int64_t const id, int64_t const diff);
  };

  std::optional<cache> cache_;

  struct cursor
  {
    fl::value::variant key_;
    int64_t id_ = 0;
    int64_t used_ = 0;

    cursor(vt_stmt* vtab);
  };

  auto init_cache(fl::stmt& stmt);
  void xDisconnect(bool const destroy);

  void xConnect(bool const create);

  bool xBestIndex(fl::vtab::index_info& info);

  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);

  void xClose(cursor* cursor);
};
