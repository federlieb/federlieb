#ifndef FEDERLIEB_API_HXX
#define FEDERLIEB_API_HXX

#include <sqlite3.h>

// There are two different modes in which to use the `federlieb` library.
// When developing an application using federlieb to access SQLite, the
// application has to link SQLite directly. When developing an extension,
// like an extension function or a virtual table, the extension library
// does not link directly to SQLite, but rather uses SQLite as already
// loaded by some application. `FEDERLIEB_EXTENSION_API` is used in case
// of the latter. In that case all `sqlite3` API functions are replaced
// by macros that use the API through a global `sqlite3_api`. That symbol
// is declared `extern` by way of `SQLITE_EXTENSION_INIT3` used below.

#ifdef FEDERLIEB_EXTENSION_API
extern "C"
{
#include <sqlite3ext.h>
  SQLITE_EXTENSION_INIT3
};
#endif

#include "federlieb/detail.hxx"
#include "federlieb/error.hxx"

namespace federlieb {

namespace fl = ::federlieb;

template<typename F, std::integral T, typename... Args>
int
api(F f, std::initializer_list<T> expected_codes, sqlite3* db, Args... args)
{
  fl::error::raise_if(nullptr == f, "SQLite3 API function is nullptr");

  sqlite3_mutex* mutex = nullptr;

  bool use_mutex = sqlite3_threadsafe() && db != nullptr;

  if (use_mutex) {
    mutex = sqlite3_db_mutex(db);
    sqlite3_mutex_enter(mutex);
  }

  int result = f(args...);
  auto ok = std::ranges::any_of(
    expected_codes, [&result](auto&& expected) { return expected == result; });

  if (!ok) [[unlikely]] {
    if (nullptr != db) {
      auto message = sqlite3_errmsg(db);
      std::cerr << message << std::endl;
    }

    if (use_mutex) {
      sqlite3_mutex_leave(mutex);
    }

    fl::error::raise("API call returned unexpected code.");
  }

  if (use_mutex) {
    sqlite3_mutex_leave(mutex);
  }

  return result;
}

template<typename F, typename... Args>
auto
api(F f, sqlite3* db, Args... args)
{
  fl::error::raise_if(nullptr == f, "SQLite3 API function is nullptr");
  return f(args...);
}

}

#endif
