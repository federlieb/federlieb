#ifndef FEDERLIEB_DB_HXX
#define FEDERLIEB_DB_HXX

#include <list>

#include "api.hxx"

#include "federlieb/stmt.hxx"
#include "federlieb/value.hxx"

namespace federlieb {

namespace fl = ::federlieb;

class db
{
public:
  db(){};

  db(std::shared_ptr<sqlite3> db)
    : db_(db){};

  template<typename... Args>
  db(Args... args)
  {
    open(args...);
  }

  void open(std::string const location,
            int const flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            std::string const vfs = {});

  fl::stmt prepare(const std::string_view sql);

  void execute_script(const std::string_view script);

  fl::value::variant select_scalar(const std::string sql);

  void register_module(const std::string& name, const sqlite3_module* const p);

  std::shared_ptr<sqlite3> ptr() { return db_; }

protected:
  std::shared_ptr<sqlite3> db_ = nullptr;
};

}

#endif
