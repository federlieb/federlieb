#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

void
fl::db::open(std::string const location, int const flags, std::string const vfs)
{
  sqlite3* db = nullptr;

  fl::api(sqlite3_open_v2,
          { SQLITE_OK },
          db,
          location.c_str(),
          &db,
          flags,
          vfs.empty() ? nullptr : vfs.c_str());

  fl::error::raise_if(nullptr == db, "sqlite3_open_v2 returned a nullptr db");

  db_ = std::shared_ptr<sqlite3>(db, [](sqlite3* db) {
    // NOTE: This must not use fl::api.
    int rc = sqlite3_close_v2(db);
    fl::error::raise_if(SQLITE_OK != rc, "error closing db");
  });
}

fl::stmt
fl::db::prepare(const std::string_view sql)
{
  sqlite3_stmt* stmt = nullptr;

  try {
  fl::api(sqlite3_prepare_v2,
          { SQLITE_OK },
          db_.get(),
          db_.get(),
          sql.data(),
          fl::detail::safe_to<int>(sql.size()),
          &stmt,
          nullptr);
  }
  catch(...) {
    fl::error::raise_if(true, sql);
  }

  fl::error::raise_if(nullptr == stmt,
                      "sqlite3_prepare returned a nullptr stmt");

  return fl::stmt(db_, std::shared_ptr<sqlite3_stmt>(stmt, sqlite3_finalize));
}

void
fl::db::execute_script(const std::string_view script)
{

  for (ptrdiff_t offset = 0; true;) {

    sqlite3_stmt* stmt = nullptr;
    char const* tail;

    auto current_base = script.data() + offset;
    auto current_size = script.size() - offset;

    fl::api(sqlite3_prepare_v2,
            { SQLITE_OK },
            db_.get(),
            db_.get(),
            current_base,
            fl::detail::safe_to<int>(current_size),
            &stmt,
            &tail);

    if (nullptr == stmt) {
      break;
    }

    auto shared =
      fl::stmt(db_, std::shared_ptr<sqlite3_stmt>(stmt, sqlite3_finalize));

    shared.execute();

    auto next_base = static_cast<char const*>(tail);

    offset = next_base - script.data();
  }
}

fl::value::variant
fl::db::select_scalar(const std::string sql)
{
  // TODO: would be nice if this accepted bind parameters?
  auto stmt = prepare("SELECT (" + sql + ")");
  stmt.execute();
  fl::error::raise_if(stmt.begin() == stmt.end(), "no result");
  return (*std::ranges::begin(stmt)).at(0).to_variant();
}

void
fl::db::register_module(const std::string& name, const sqlite3_module* const p)
{
  fl::api(sqlite3_create_module,
          { SQLITE_OK },
          db_.get(),
          db_.get(),
          name.c_str(),
          p,
          nullptr);
}
