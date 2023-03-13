#include "vt_stmt.hxx"
#include "federlieb/federlieb.hxx"
#include <boost/algorithm/string/join.hpp>
#include <fmt/core.h>

namespace fl = ::federlieb;

// TODO: major missing feature is garbage collection. The table does reference
// counting to keep cached entries around while there are open cursors using
// them, but there is no logic to dispose of them when no longer needed. That
// includes a lack of logic to dispose of outdated record that are no longer
// needed when new data, due to a new caching key, is materialized. So this
// basically leaks memory until the virtual table is disposed.

// TODO: to aid debugging and profiling, add the name of the table to all of
// the queries (as comment at the beginning).

std::string
to_where_fragment(const fl::vtab::index_info& info, int bind_param_count)
{
  std::stringstream ss;

  for (auto column : info.columns) {

    auto quoted_column_name = column.column_index < bind_param_count
      ? fl::detail::quote_identifier("p" + std::to_string(1 + column.column_index))
      : fl::detail::quote_identifier("c" + std::to_string(1 + column.column_index - bind_param_count));

    for (auto constraint : column.constraints) {

      if (!constraint.usable) {
        continue;
      }

      auto constraint_string = to_sql_with_bind_parameters(constraint, 1);

      if (!constraint_string.empty()) {
        if (ss.tellp() > 0) {
          ss << " AND ";
        }
        ss << quoted_column_name
           << " "
           << constraint_string;
      }
    }
  }

  return ss.str();
}

std::pair<std::string, std::string>
to_create_index(const std::string& table_name, const fl::vtab::index_info& info)
{
  std::stringstream ss;

  auto quoted_table_name = fl::detail::quote_identifier(table_name);

  std::list<std::string> columns;

  for (auto column : info.columns) {
    auto quoted_column_name = fl::detail::quote_identifier(
      "c" + std::to_string(1 + column.column_index));

    for (auto constraint : column.constraints) {

      if (!constraint.usable) {
        continue;
      }

      if (column.column_index < 0) {
        continue;
      }

      columns.push_back(quoted_column_name);
      break;

    }
  }

  auto cols = boost::algorithm::join(columns, ",");

  auto index_name = (
    "fl_stmt_auto_index_" + quoted_table_name + "(" + cols + ")"); 

  if (!cols.empty()) {
    
    cols = "id," + cols;

    ss << "CREATE INDEX IF NOT EXISTS "
      << fl::detail::quote_identifier(index_name)
      << " ON "
      << quoted_table_name
      << "(" << cols << ")";
  }

  return std::make_pair( ss.str(), index_name );
}

fl::stmt
do_stuff_with_cache_and_info(vt_stmt::cache& cache, const fl::vtab::index_info& info) {
  auto constrained_select = cache.select_sql;

  auto constraints = to_where_fragment(info, cache.bind_parameter_count);

  if (!constraints.empty()) {
    constrained_select += " AND (" + constraints + ")";
  }

  // std::cerr << constrained_select << std::endl;

  auto stmt = cache.db.prepare(constrained_select);

  return stmt;

}

void
vt_stmt::cache::change_meta_refcount(int64_t const id, int64_t const diff)
{
  update_refcount_stmt
    .reset() //
    .bind(1, diff)
    .bind(2, id)
    .execute();
}

vt_stmt::cursor::cursor(vt_stmt* vtab)
{
  auto key_sql = vtab->kwarg("key").value_or("(SELECT RANDOMBLOB(16))");

  key_ = vtab->db().select_scalar(key_sql);
}

auto
vt_stmt::init_cache(fl::stmt& stmt)
{

  auto db = fl::db(":memory:");

  fl::error::raise_if(stmt.column_count() < 1, "zero columns");

  db.execute_script(R"SQL(
    drop table if exists meta;
    drop table if exists data;
    pragma foreign_keys = 1;
    pragma synchronous = off;
    pragma journal_mode = memory;
  )SQL");

  auto px =
    std::views::iota(1, 1 + stmt.bind_parameter_count()) |
    std::views::transform([](auto&& e) { return "p" + std::to_string(e); });

  auto pqx =
    std::views::iota(1, 1 + stmt.bind_parameter_count()) |
    std::views::transform([](auto&& e) { return "?" + std::to_string(e); });

  auto cx =
    std::views::iota(1, 1 + stmt.column_count()) |
    std::views::transform([](auto&& e) { return "c" + std::to_string(e); });

  auto cqx =
    std::views::iota(1, 1 + stmt.column_count()) |
    std::views::transform([](auto&& e) { return "?" + std::to_string(e); });

  auto create_meta_stmt = db.prepare(fl::detail::format(
    R"SQL(

      CREATE TABLE meta(
          "id"       INTEGER PRIMARY KEY
        , "refcount" INTEGER
        , "key"      ANY [BLOB]
        {}                 /* , p1 type, p2 type... */
        , UNIQUE("key" {}) /* , p1, p2, p3...       */
      )

    )SQL",
    fl::detail::str(px | fl::detail::prefix(", ") |
                    fl::detail::suffix(" ANY [BLOB]")),
    fl::detail::str(px | fl::detail::prefix(", "))));

  auto create_data_stmt = db.prepare(fl::detail::format(
    R"SQL(

      CREATE TABLE data(
        id INTEGER REFERENCES meta(id) ON DELETE CASCADE
        {} /* , c1 type, c2 type... */
      )

    )SQL",
    fl::detail::str(cx | fl::detail::prefix(", ") |
                    fl::detail::suffix(" ANY [BLOB]"))));

  // Need tables to prepare statements
  create_meta_stmt.execute();
  create_data_stmt.execute();

  auto insert_meta_stmt = db.prepare(fl::detail::format(
    R"SQL(

      INSERT INTO "meta"({} "key", "refcount") /* p1, p2, ... */
      VALUES({} :key, -1)                      /* ?1, ?2, ... */
      ON CONFLICT DO
      UPDATE SET "refcount" = "refcount" + 1
      RETURNING "id", "refcount"

    )SQL",
    fl::detail::str(px | fl::detail::suffix(", ")),
    fl::detail::str(pqx | fl::detail::suffix(", "))));

  auto insert_data_stmt = db.prepare(fl::detail::format(
    R"SQL(

      INSERT INTO "data"({} id) /* c1, c2, ... */
      VALUES({} :id)            /* ?1, ?2, ... */

    )SQL",
    fl::detail::str(cx | fl::detail::suffix(", ")),
    fl::detail::str(cqx | fl::detail::suffix(", "))));

  auto select_sql = (fl::detail::format(
    R"SQL(

      SELECT /* {} */
        {}   /* meta.p1, meta.p2, ..., */
        {}   /* data.c1, data.c2, ..., */
        NULL /* TODO: unfortunate due to formatting limitations */
      FROM "meta", "data" USING(id)
      WHERE
        (meta.id = :id)

    )SQL",
    table_name_, // FIXME: needs escaping for use in comment
    fl::detail::str(px | fl::detail::prefix("meta.") |
                    fl::detail::suffix(", ")),
    fl::detail::str(cx | fl::detail::prefix("data.") |
                    fl::detail::suffix(", "))));

  auto update_refcount_stmt = db.prepare(
    R"SQL(

      UPDATE "meta"
      SET "refcount" = "refcount" + :diff
      WHERE "id" = :id

    )SQL");

  return cache{
    db, insert_meta_stmt, insert_data_stmt, update_refcount_stmt, select_sql,
    stmt.bind_parameter_count()
  };
}

void
vt_stmt::xDisconnect(bool const destroy)
{}

void
vt_stmt::xConnect(bool const create)
{
  auto args = arguments();

  fl::error::raise_if(args.empty(), "missing argument");

  auto stmt = db().prepare("SELECT * FROM " + args.front());

  std::vector<std::string> column_defs;

  for (auto ix = 0; ix < stmt.bind_parameter_count(); ++ix) {
    auto name = stmt.bind_parameter_name(ix + 1);
    auto id =
      fl::detail::quote_identifier(name.value_or("?" + std::to_string(ix + 1)));
    column_defs.push_back(id + " ANY [BLOB] HIDDEN VT_REQUIRED");
  }

  for (auto&& col : stmt.columns()) {
    auto id = fl::detail::quote_identifier(col.name());
    auto type = col.declared_type().size() > 0 ? col.declared_affinity() : "";
    column_defs.push_back(id + " " + type);
  }

  auto create_table_sql =
    "CREATE TABLE fl_stmt(" + boost::algorithm::join(column_defs, ", ") + ")";

  cache_ = init_cache(stmt);

  declare(create_table_sql);
}

bool
vt_stmt::xBestIndex(fl::vtab::index_info& info)
{
  // By default the base class will request the values for all `ÈQ`
  // constraints for all columns marked required. We can use more
  // constraints when retrieving a result from the cache.

  for (auto&& column : info.columns) {
    for (auto&& constraint : column.constraints) {
      if (constraint.usable && column.column_index >= 0) {
        info.estimated_cost = 1e3;
        if (!constraint.argv_index) {
          constraint.argv_index = info.next_argv_index++;
        }
      }
    }
  }

  // TODO: if the statement does not have unbound parameters it would be possible
  // to materialize the statement here and report good estimates. 

  auto idx = to_create_index("data", info);
  auto create_index_sql = idx.first;
  auto index_name = idx.second;

  // TODO: even with no index, table row count can be gathered from sqlite_stat1

  if (!create_index_sql.empty()) {
    cache_->db.prepare(create_index_sql).execute();

    // TODO: Does not really make sense here
    // TODO: do not analyze if index already existed
    cache_->db.prepare("analyze " + fl::detail::quote_identifier(index_name)).execute();

    // TODO: Move this logic to a better place.

    auto has_stmt1 = cache_->db.prepare(R"SQL(

      select 1 from pragma_table_list where name = 'sqlite_stat1'

    )SQL").execute();

    if (!has_stmt1.empty()) {

      auto stat1_stmt = cache_->db.prepare(R"SQL(

        select tbl, idx, stat
        from sqlite_stat1
        where
          idx is not null
          and
          tbl is not null
          and
          stat is not null
          -- todo: where idx is :name

      )SQL").execute();

      struct sqlite_stat1 {
        std::string tbl;
        std::string idx;
        std::string stat;
      };

      for (auto&& row : stat1_stmt | fl::as<sqlite_stat1>()) {
        auto pos = row.stat.find_last_not_of("0123456789");
        if (row.idx == index_name && std::string::npos != pos) {
          auto row_count = row.stat.substr(pos + 1);
          info.estimated_rows = std::stoi(row_count);
          info.estimated_cost = 6 * info.estimated_rows;
        }

      }
    }

  }

  info.detail = vt_stmt::index_info_detail{ do_stuff_with_cache_and_info(*cache_, info) };

  return true;
}

void
vt_stmt::xClose(cursor* cursor)
{
  cache_->change_meta_refcount(cursor->id_, -cursor->used_);
}


fl::stmt
vt_stmt::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto& cache = cache_;

  // TODO: transaction?

  auto inputs =
    columns_ | std::views::filter(&fl::vtab::column::required) |
    std::views::transform([&info](auto&& e) {
      return info.columns[e.index + 1].constraints[0].current_value.value();
    });

  cache->insert_meta_stmt
    .reset() //
    .bind(":key", cursor->key_)
    .execute(inputs);

  // log("D: INSERT INTO meta SQL:\n{}",
  // cache->insert_meta_stmt.expanded_sql());

  struct insert_meta_data
  {
    int64_t id;
    int64_t refcount;
  };

  auto first = cache->insert_meta_stmt | fl::as<insert_meta_data>();

  auto insert_meta_row = *first.begin();

  cursor->id_ = insert_meta_row.id;

  // log("D: INSERT INTO meta RETURNING id {}, refcount {}",
  //     insert_meta_row.id,
  //     insert_meta_row.refcount);

  if (insert_meta_row.refcount < 0) {

    auto user_stmt = db().prepare("SELECT * FROM " + arguments().front());
    user_stmt.execute(inputs);

    // log("D: SELECT user SQL:\n{}", user_stmt.expanded_sql());

    cache->insert_data_stmt
      .reset() //
      .clear_bindings()
      .bind(":id", insert_meta_row.id)
      .executemany(user_stmt);

    cache->db.execute_script("analyze");

    cache->change_meta_refcount(insert_meta_row.id, +1);
  }

  // TODO: Can't this be done with the insert_meta_stmt? Document why not.
  cache->change_meta_refcount(insert_meta_row.id, +1);

  // xClose will later undo the refcount update, but xClose might also
  // be called if we did not get to update the refcount to begin with.
  cursor->used_ = 1;

  auto constrained_select = cache->select_sql;

#if 1

  fl::stmt stmt;

  if (!info.detail) {
    // FIXME: needs better index info caching in vtab.hxx
    stmt = do_stuff_with_cache_and_info(*cache, info);
  } else {
    stmt = std::any_cast<index_info_detail>(info.detail.value()).select_stmt;
  }

#endif

  // log("D: vt_stmt xFilter called {}.\n", constrained_select);

  stmt.reset().bind(":id", insert_meta_row.id);

  // log("D: Cache retrieval SQL:\n{}\n(for {})\n{}\n", stmt.sql(), arguments().front());

  for (auto&& column : info.columns) {
    for (auto&& constraint : column.constraints) {
      if (constraint.argv_index) {
        if (constraint.op != SQLITE_INDEX_CONSTRAINT_ISNOTNULL && constraint.op != SQLITE_INDEX_CONSTRAINT_ISNULL) {
          fl::error::raise_if(!constraint.current_value, "impossible");
          stmt.bind(constraint.argv_index.value() + 1, constraint.current_value.value());
        } else {
          stmt.bind(constraint.argv_index.value() + 1, nullptr);
        }
      }
    }
  }

  // log("D: Cache retrieval SQL:\n{}\n(for {})\n{}\n", stmt.sql(), arguments().front());

  stmt.execute();

  return stmt;
}
