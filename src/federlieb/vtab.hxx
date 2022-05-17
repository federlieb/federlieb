#ifndef FEDERLIEB_VTAB_HXX
#define FEDERLIEB_VTAB_HXX

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <boost/json.hpp>

#include "federlieb/concepts.hxx"
#include "federlieb/context.hxx"
#include "federlieb/db.hxx"
#include "federlieb/detail.hxx"
#include "federlieb/error.hxx"
#include "federlieb/pragma.hxx"
#include "federlieb/value.hxx"

namespace federlieb::vtab {

namespace fl = ::federlieb;

struct constraint_info
{
  size_t id;
  int op;
  bool usable;
  bool omit_check;
  std::optional<size_t> argv_index;
  std::optional<bool> many_at_once;
  std::optional<fl::value::variant> rhs;
  std::string collation;
  std::optional<fl::value::variant> current_value;
  sqlite3_value* current_raw;
};

struct column_info
{
  int column_index;
  std::string column_name;
  std::vector<constraint_info> constraints;
  bool used;
  std::optional<size_t> order_by_pos;
  std::optional<bool> order_by_desc;
};

struct index_info
{
  std::vector<column_info> columns;
  std::optional<int> offset;
  std::optional<int> limit;
  int distinct_mode;
  bool unique;
  long long int estimated_rows;
  double estimated_cost;
  bool order_by_consumed;
  size_t next_argv_index = 1;

  void mark_wanted(const std::string& name, int const op);
  void mark_transferables(const std::string& column_name);
  constraint_info const* get(const std::string& name, int const op) const;
};

struct column
{
  int index;
  std::string name;
  std::string type;
  bool hidden;
  bool required;
};

std::string
to_sql(const fl::value::null& v);

std::string
to_sql(const fl::value::integer& v);

std::string
to_sql(const fl::value::real& v);

std::string
to_sql(const fl::value::text& v);

std::string
to_sql(const fl::value::blob& v);

std::string
to_sql(const fl::value::variant& v);

constexpr char const*
constraint_op_to_string(int const op);

std::string
to_sql(const fl::vtab::constraint_info& constraint,
       bool const include_collation = false);

fl::vtab::index_info
index_info_import(std::vector<fl::vtab::column> columns_,
                  sqlite3_index_info* theirs);

void
index_info_export(const fl::vtab::index_info& ours, sqlite3_index_info* theirs);

bool
mark_constraints(std::vector<fl::vtab::column> columns_,
                 fl::vtab::index_info& ours);

std::string
usable_constraints_to_where_fragment(const std::string& table_name,
                                     const fl::vtab::index_info& info);

struct standard_layout_cursor
{
  sqlite3_vtab_cursor base;
  void* cursor;
};

struct standard_layout_vtab
{
  sqlite3_vtab base;
  void* vtab;
};

static_assert(
  std::is_standard_layout_v<typename fl::vtab::standard_layout_vtab>);
static_assert(
  std::is_standard_layout_v<typename fl::vtab::standard_layout_cursor>);

template<typename Vtab>
struct base
{
public:
  std::shared_ptr<sqlite3> db_ = nullptr;
  std::vector<std::string> argv_;

  std::string module_name_;
  std::string schema_name_;
  std::string table_name_;
  std::vector<fl::vtab::column> columns_;

  struct cursor_state
  {
    Vtab::cursor* cursor;
    sqlite_int64 rowid;
    decltype(std::declval<Vtab>().xFilter({}, {})) result;
    std::ranges::iterator_t<decltype(cursor_state::result)> it;
    std::ranges::sentinel_t<decltype(cursor_state::result)> end;
  };

  struct cursor_pointers
  {
    fl::vtab::standard_layout_cursor* sl_cursor;
    fl::vtab::standard_layout_vtab* sl_vtab;
    Vtab* vtab;
    typename Vtab::cursor* cursor;
    cursor_state* state;
  };

  struct vtab_pointers
  {
    fl::vtab::standard_layout_vtab* sl_vtab;
    Vtab* vtab;
  };

  static vtab_pointers unbox(sqlite3_vtab* p)
  {
    vtab_pointers d;
    d.sl_vtab = reinterpret_cast<fl::vtab::standard_layout_vtab*>(p);
    d.vtab = reinterpret_cast<Vtab*>(d.sl_vtab->vtab);
    return d;
  }

  static cursor_pointers unbox(sqlite3_vtab_cursor* p)
  {
    cursor_pointers d;
    d.sl_cursor = reinterpret_cast<fl::vtab::standard_layout_cursor*>(p);
    d.sl_vtab = reinterpret_cast<fl::vtab::standard_layout_vtab*>(
      d.sl_cursor->base.pVtab);
    d.vtab = reinterpret_cast<Vtab*>(d.sl_vtab->vtab);
    d.state = reinterpret_cast<cursor_state*>(d.sl_cursor->cursor);
    d.cursor = d.state->cursor;
    return d;
  }

  auto arguments() const
  {
    return std::ranges::subrange(argv_.begin() + 3, argv_.end());
  }

  auto kwarguments() const
  {
    using T = std::optional<std::pair<std::string, std::string>>;

    std::regex kweq("^(\\w+)=([^]*)$");

    auto split = [kweq](auto&& e) -> T {
      std::smatch m;
      if (std::regex_match(e, m, kweq)) {
        return std::make_optional(std::make_pair(m[1], m[2]));
      }
      return std::nullopt;
    };

    return (arguments() | std::views::transform(split) |
            std::views::filter([](auto&& e) { return e.has_value(); }) |
            std::views::transform([](auto&& e) { return *e; }));
  }

  auto db() const { return fl::db(db_); }

  std::optional<std::string> kwarg(const std::string& keyword) const
  {
    auto kwargs = kwarguments();
    auto result = std::ranges::find_if(
      kwargs, [&keyword](auto&& e) { return e.first == keyword; });

    if (std::ranges::end(kwargs) == result) {
      return std::nullopt;
    }

    return (*result).second;
  }

  std::string kwarg_or_throw(const std::string& keyword) const
  {
    auto result = kwarg(keyword);
    fl::error::raise_if(!result, "missing kwarg");
    return result.value();
  }

  template<typename... Ts>
  void log(const fl::detail::capture_location& fmt, Ts&&... args)
  {
    fl::detail::log(fmt, args...);
  }

  void declare(const std::string& sql) requires fl::concepts::VirtualTable<Vtab>
  {
    int rc = SQLITE_OK;

    auto tmpdb = fl::db(":memory:");
    auto declare_stmt = tmpdb.prepare(sql);
    declare_stmt.execute();

    auto xinfo = fl::pragma::table_xinfo(tmpdb, Vtab::name);

    auto columns =
      fl::detail::to_vector(std::views::transform(xinfo, [](auto&& e) {
        fl::vtab::column column;

        column.index = fl::detail::safe_to<int>(e.cid);
        column.name = e.name;
        column.type = e.type;

        auto re = std::regex(R"(\b(HIDDEN|VT_\w+)\b)");
        auto options = fl::detail::regex_split(column.type, re, 0);

        column.required =
          (std::ranges::find(options, "VT_REQUIRED") != options.end());

        column.hidden = (std::ranges::find(options, "HIDDEN") != options.end());

        return column;
      }));

    fl::error::raise_if(columns.size() < 1, "no columns in declare()");

    columns.push_back(fl::vtab::column{ .index = -1,
                                        .name = "_rowid_",
                                        .type = "INTEGER",
                                        .hidden = false,
                                        .required = false });

    std::ranges::sort(columns, {}, &fl::vtab::column::index);

    fl::error::raise_if(!std::in_range<int>(columns.size()),
                        "too many columns");

    auto is_continuous = std::ranges::equal(
      std::views::drop(columns, 1),
      std::views::iota(int(0), fl::detail::safe_to<int>(columns.size() - 1)),
      {},
      &fl::vtab::column::index);

    fl::error::raise_if(!is_continuous, "bad result from pragma table_xinfo");

    columns_ = columns;

    fl::api(
      sqlite3_declare_vtab, { SQLITE_OK }, db_.get(), db_.get(), sql.c_str());
  }

protected
  :

  void
  connect(bool const create,
          sqlite3* db,
          void* pAux,
          int argc,
          const char* const* argv,
          sqlite3_vtab** ppVTab,
          char** pzErr)
  {
    auto sl_vtab = new fl::vtab::standard_layout_vtab();
    sl_vtab->vtab = this;
    *ppVTab = std::addressof(sl_vtab->base);

    db_ = std::shared_ptr<sqlite3>(db, [](auto) {});
    argv_ = std::vector<std::string>(argv, argv + argc);
    module_name_ = argv_.at(0);
    schema_name_ = argv_.at(1);
    table_name_ = argv_.at(2);
  }

public:
  static sqlite3_module _module()
  {

    sqlite3_module module;
    memset(&module, 0, sizeof(module));

    module.xConnect = [](sqlite3* db,
                         void* pAux,
                         int argc,
                         const char* const* argv,
                         sqlite3_vtab** ppVTab,
                         char** pzErr) noexcept {
      try {
        auto vtab = new Vtab();

        vtab->connect(false, db, pAux, argc, argv, ppVTab, pzErr);
        vtab->xConnect(false);

      } catch (std::bad_alloc const& e) {
        return SQLITE_NOMEM;
      } catch (std::invalid_argument const& e) {
        return SQLITE_MISUSE;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xCreate = [](sqlite3* db,
                        void* pAux,
                        int argc,
                        const char* const* argv,
                        sqlite3_vtab** ppVTab,
                        char** pzErr) noexcept {
      try {
        auto vtab = new Vtab();
        vtab->connect(true, db, pAux, argc, argv, ppVTab, pzErr);
        vtab->xConnect(true);

      } catch (std::bad_alloc const& e) {
        return SQLITE_NOMEM;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xDestroy = [](sqlite3_vtab* pVTab) noexcept {
      try {
        auto p = unbox(pVTab);

        if constexpr (requires { p.vtab->xDisconnect(true); }) {
          p.vtab->xDisconnect(true);
        }

        delete p.vtab;
        delete p.sl_vtab;
      } catch (std::bad_alloc const& e) {
        return SQLITE_NOMEM;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    if constexpr (requires { bool(Vtab::eponymous); }) {
      if (Vtab::eponymous) {
        module.xCreate = nullptr;
        module.xDestroy = nullptr;
      }
    }

    module.xDisconnect = [](sqlite3_vtab* pVTab) noexcept {
      try {
        auto p = unbox(pVTab);

        if constexpr (requires { p.vtab->xDisconnect(false); }) {
          p.vtab->xDisconnect(false);
        }

        delete p.vtab;
        delete p.sl_vtab;
      } catch (std::bad_alloc const& e) {
        return SQLITE_NOMEM;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xBestIndex = [](sqlite3_vtab* pVTab,
                           sqlite3_index_info* theirs) noexcept {
      try {

        auto p = unbox(pVTab);

        auto ours = index_info_import(p.vtab->columns_, theirs);
        auto check = mark_constraints(p.vtab->columns_, ours);

        if (false == check) {

#if 0
          std::string serialized =
            boost::json::serialize(boost::json::value_from(ours));
          p.vtab->log("D: xBestIndex returns SQLITE_CONSTRAINT for {}",
                      serialized);
#endif

          return SQLITE_CONSTRAINT;
        }

        // p.vtab->log("D: BEFORE index_info\n{}", json(info).dump(2));

        if constexpr (requires { p.vtab->xBestIndex(ours); }) {
          check = p.vtab->xBestIndex(ours);
        }

        if (false == check) {
          return SQLITE_CONSTRAINT;
        }

        // p.vtab->log("D: AFTER index_info\n{}", json(info).dump(2));

        index_info_export(ours, theirs);

        std::string serialized =
          boost::json::serialize(boost::json::value_from(ours));

        theirs->idxStr = static_cast<char*>(
          fl::api(sqlite3_malloc64, p.vtab->db_.get(), 1 + serialized.size()));

        memset(theirs->idxStr, 0, 1 + serialized.size());
        memcpy(theirs->idxStr, serialized.c_str(), 1 + serialized.size());

        theirs->needToFreeIdxStr = 1;

      } catch (std::bad_alloc const& e) {
        return SQLITE_NOMEM;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xFilter = [](sqlite3_vtab_cursor* c,
                        int idxNum,
                        const char* idxStr,
                        int argc,
                        sqlite3_value** argv) noexcept {
      try {
        auto p = unbox(c);

        // TODO: it would be possible to cache the object and just
        // look it up here, possibly only parsing the JSON when the
        // stored object has expired.

        fl::vtab::index_info info = boost::json::value_to<fl::vtab::index_info>(
          boost::json::parse(idxStr));

        for (auto&& column : info.columns) {
          for (auto&& constraint : column.constraints) {
            if (constraint.argv_index) {
              constraint.current_raw = argv[*constraint.argv_index - 1];
              constraint.current_value =
                fl::value::from(constraint.current_raw);
            }
          }
        }

        //        p.vtab->log("D: xFilter with {}",
        //                    fl::vtab::usable_constraints_to_where_fragment("t",
        //                    info));

        p.state->result = p.vtab->xFilter(info, p.cursor);
        p.state->end = std::ranges::end(p.state->result);
        p.state->it = std::ranges::begin(p.state->result);

      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xOpen = [](sqlite3_vtab* pVTab, sqlite3_vtab_cursor** c) noexcept {
      try {
        auto p = unbox(pVTab);
        cursor_pointers d;

        auto sl_cursor = new fl::vtab::standard_layout_cursor;
        auto state = new cursor_state;
        auto cursor = new Vtab::cursor(p.vtab);
        sl_cursor->cursor = state;
        state->cursor = cursor;
        *c = std::addressof(sl_cursor->base);

      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xClose = [](sqlite3_vtab_cursor* c) noexcept {
      try {
        auto p = unbox(c);

        if constexpr (requires { p.vtab->xClose(p.cursor); }) {
          p.vtab->xClose(p.cursor);
        }

        delete p.state->cursor;
        delete p.state;
        delete p.sl_cursor;

      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xNext = [](sqlite3_vtab_cursor* c) noexcept {
      try {
        auto p = unbox(c);
        p.state->it++; // TODO: std::next
        p.state->rowid++;

      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

    module.xEof = [](sqlite3_vtab_cursor* c) noexcept {
      auto p = unbox(c);
      return int(p.state->it == p.state->end);
    };

    module.xColumn =
      [](sqlite3_vtab_cursor* c, sqlite3_context* ctx, int id) noexcept {
        try {

          auto p = unbox(c);
          auto&& d = *(p.state->it);
          auto it = d.begin() + id; // TODO: std::next?
          fl::context(ctx, p.vtab->db_.get()).result(*it);

        } catch (...) {
          return SQLITE_INTERNAL;
        }

        return SQLITE_OK;
      };

    module.xRowid = [](sqlite3_vtab_cursor* c, sqlite_int64* pRowid) noexcept {
      try {
        auto p = unbox(c);
        *pRowid = p.state->rowid;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };

#if 0
    module.xFindFunction =
      [](sqlite3_vtab* pVtab,
         int nArg,
         const char* zName,
         void (**pxFunc)(sqlite3_context*, int, sqlite3_value**),
         void** ppArg) noexcept { return SQLITE_OK; };

    module.xUpdate = [](sqlite3_vtab* pVtab,
                        int argc,
                        sqlite3_value** argv,
                        sqlite_int64* pRowid) noexcept {
      try {
        if (argc == 1 && argv[0] != nullptr) {
          // DELETE
        } else if (argc > 1 && argv[0] == nullptr) {
          // INSERT
        } else if (argc > 1 && argv[0] != nullptr && argv[0] == argv[1]) {
          // UPDATE
        } else if (argc > 1 && argv[0] != nullptr && argv[0] != argv[1]) {
          // UPDATE primary key
        } else {
        }

      } catch (std::exception const& e) {
        return SQLITE_INTERNAL;
      } catch (...) {
        return SQLITE_INTERNAL;
      }

      return SQLITE_OK;
    };
#endif

#if 0
    module.xBegin = [](sqlite3_vtab* pVTab) noexcept { return SQLITE_OK; };

    module.xSync = [](sqlite3_vtab* pVTab) noexcept { return SQLITE_OK; };

    module.xCommit = [](sqlite3_vtab* pVTab) noexcept { return SQLITE_OK; };

    module.xRollback = [](sqlite3_vtab* pVTab) noexcept { return SQLITE_OK; };

    module.xRename = [](sqlite3_vtab* pVTab, const char* zNew) noexcept {
      return SQLITE_OK;
    };

    module.xSavepoint = [](sqlite3_vtab* pVTab, int iSavepoint) noexcept {
      return SQLITE_OK;
    };

    module.xRelease = [](sqlite3_vtab* pVTab, int iSavepoint) noexcept {
      return SQLITE_OK;
    };

    module.xRollbackTo = [](sqlite3_vtab* pVTab, int iSavepoint) noexcept {
      return SQLITE_OK;
    };

    module.xShadowName = [](char const* const name) noexcept { return 0; };

#endif

    return module;
  }

public:
  inline static sqlite3_module const module = _module();

  static void register_module(fl::db& db)
  {
    db.register_module(Vtab::name, &Vtab::module);
  }

private:
};
}

#endif
