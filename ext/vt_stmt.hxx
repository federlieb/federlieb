#pragma once

#include <mutex>

#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

/*

CREATE VIRTUAL TABLE USING stmt((
  SELECT ...
)
, cache=(:memory:) -- not implemented
, key=(SELECT ...) 
, expire=...       -- not implemented
, mode=cursor|vtab -- not implemented
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
    std::string fake_insert_sql;
    std::string select_sql;
    int bind_parameter_count;

    void change_meta_refcount(int64_t const id, int64_t const diff);
  };

  struct index_info_detail
  {
    fl::stmt select_stmt;
  };

  std::optional<cache> cache_;

  struct {
    std::mutex mutex;
    std::vector<int64_t> vector;
  } meta_id_refcount;

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

  void xBegin();
  void xSync();
  void xCommit();
  void xRollback();
  void xSavepoint(int savepoint);
  void xRelease(int savepoint);
  void xRollbackTo(int savepoint);

  bool xBestIndex(fl::vtab::index_info& info);

  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor);

  void xClose(cursor* cursor);
};
