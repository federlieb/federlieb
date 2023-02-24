#include "vt_stmt.hxx"
#include "federlieb/federlieb.hxx"
#include <boost/algorithm/string/join.hpp>
#include <fmt/core.h>

namespace fl = ::federlieb;

std::string
to_where_fragment(const std::string& table_name,
                                              const fl::vtab::index_info& info)
{
  std::stringstream ss;

  auto quoted_table_name = fl::detail::quote_identifier(table_name);

  for (auto column : info.columns) {
    auto quoted_column_name = fl::detail::quote_identifier(
      "c" + std::to_string(1 + column.column_index));

    for (auto constraint : column.constraints) {

      if (!constraint.usable) {
        continue;
      }

      auto constraint_string = to_sql(constraint);

      if (!constraint_string.empty() && column.column_index >= 0) {
        if (ss.tellp() > 0) {
          ss << " AND ";
        }
        ss << quoted_table_name << "." << quoted_column_name << " "
           << constraint_string;
      }
    }
  }

  return ss.str();
}

std::string
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

  if (!cols.empty()) {
    
    cols = "id," + cols;

    ss << "CREATE INDEX IF NOT EXISTS "
      << fl::detail::quote_identifier("auto_index_" + quoted_table_name + "(" + cols + ")")
      << " ON "
      << quoted_table_name
      << "(" << cols << ")";
  }

  return ss.str();
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

  db.prepare("DROP TABLE IF EXISTS meta").execute();
  db.prepare("DROP TABLE IF EXISTS data").execute();
  db.prepare("PRAGMA foreign_keys = 1").execute();

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

      SELECT
        {}   /* meta.p1, meta.p2, ..., */
        {}   /* data.c1, data.c2, ..., */
        NULL /* TODO: unfortunate due to formatting limitations */
      FROM "meta", "data" USING(id)
      WHERE
        (meta.id = :id)

    )SQL",
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
    db, insert_meta_stmt, insert_data_stmt, update_refcount_stmt, select_sql
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

  auto create_index_sql = to_create_index("data", info);
  if (!create_index_sql.empty()) {
    cache_->db.prepare(create_index_sql).execute();
  }

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

    cache->change_meta_refcount(insert_meta_row.id, +1);
  }

  cache->change_meta_refcount(insert_meta_row.id, +1);

  // xClose will later undo the refcount update, but xClose might also
  // be called if we did not get to update the refcount to begin with.
  cursor->used_ = 1;

  auto constrained_select = cache->select_sql;

#if 1
  // TODO: move this to bestindex

  auto constraints = to_where_fragment("data", info);

  if (!constraints.empty()) {
    constrained_select += " AND (" + constraints + ")";
  }
#endif

  // log("D: vt_stmt xFilter called {}.\n", constrained_select);

  auto stmt = cache->db.prepare(constrained_select);

  stmt.bind(":id", insert_meta_row.id);

  // log("D: Cache retrieval SQL:\n{}\n(for {})\n{}\n", stmt.expanded_sql(), arguments().front());

  stmt.execute();

  return stmt;
}
