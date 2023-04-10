#include "vt_stmt.hxx"
#include "federlieb/federlieb.hxx"

#include <atomic>

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

// TODO: needs an out=... parameter (repeatable) that causes xBestIndex to
// reject query plans with an eq constraint on that column. Maybe. Would that
// actually help? There is nothing in that that would inherently stop SQLite
// from running a JOIN in the wrong order.

// TODO: make this threadsafe?

// TODO: do the reference counting not in the db but outside

// TODO: Would be easier actually if we just keep the meta table and use
// sqlite3_serialize to put the whole database there, which would avoid any
// reader/writer conflicts, since we would then leave any transaction inside
// xFilter.

static std::atomic<int> finding_out_column_names = 0;
static std::atomic<int> doing_fake_insert = 0;

struct finding_out_column_name_guard {
  finding_out_column_name_guard() {
    finding_out_column_names++;
  }
  ~finding_out_column_name_guard() {
    finding_out_column_names--;
  }
};

struct doing_fake_insert_guard {
  doing_fake_insert_guard() {
    doing_fake_insert++;
  }
  ~doing_fake_insert_guard() {
    doing_fake_insert--;
  }
};

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
  auto quoted_table_name = fl::detail::quote_identifier(table_name);
  auto index_name = (
    "fl_stmt_auto_index_" + quoted_table_name + "(" + cols + ")"); 

  if (!cols.empty()) {
    
    cols = "id," + cols;

    ss << "create index if not exists "
      << fl::detail::quote_identifier(index_name)
      << " on "
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
  auto key_sql = vtab->kwarg("key").value_or("(select randomblob(16))");

  key_ = vtab->db().select_scalar(key_sql);

  if (!doing_fake_insert) {
    // TODO: maybe do this only when actually writing new data?
    // doing_fake_insert_guard g;
    // vtab->db().execute_script(vtab->cache_->fake_insert_sql);
  }

}

auto
vt_stmt::init_cache(fl::stmt& stmt)
{

  auto db = fl::db(":memory:");

  fl::error::raise_if(stmt.column_count() < 1, "zero columns");

  db.execute_script(R"SQL(
    
    drop table if exists meta;
    drop table if exists data;

    -- fixme: temporarily off (since this never deletes anything)
    pragma foreign_keys = 0;
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

  auto nx =
    std::views::iota(1, 1 + stmt.column_count() + stmt.bind_parameter_count() - 1) |
    std::views::transform([](auto&& e) { return std::string("null"); });

  auto create_meta_stmt = db.prepare(fl::detail::format(
    R"SQL(

      create table meta( /* {} */
          "id"       integer primary key
        , "refcount" integer
        , "key"      any [blob]
        {}                 /* , p1 type, p2 type... */
        , data blob
        , unique("key" {}) /* , p1, p2, p3...       */
      )

    )SQL",
    fl::detail::mangle_for_multiline_comment(table_name_),
    fl::detail::str(px | fl::detail::prefix(", ") |
                    fl::detail::suffix(" any [blob]")),
    fl::detail::str(px | fl::detail::prefix(", "))));

  auto create_data_stmt = db.prepare(fl::detail::format(
    R"SQL(

      create table data( /* {} */
        id integer references meta(id) on delete cascade
        {} /* , c1 type, c2 type... */
      )

    )SQL",
    fl::detail::mangle_for_multiline_comment(table_name_),
    fl::detail::str(cx | fl::detail::prefix(", ") |
                    fl::detail::suffix(" any [blob]"))));

  // Need tables to prepare statements
  create_meta_stmt.execute();
  create_data_stmt.execute();

  auto insert_meta_stmt = db.prepare(fl::detail::format(
    R"SQL(

      insert into /* {} */ "meta"({} "key", "refcount") /* p1, p2, ... */
      values({} :key, -1)                      /* ?1, ?2, ... */
      on conflict do
      update set "refcount" = "refcount" + 1
      returning "id", "refcount"

    )SQL",
    fl::detail::mangle_for_multiline_comment(table_name_),
    fl::detail::str(px | fl::detail::suffix(", ")),
    fl::detail::str(pqx | fl::detail::suffix(", "))));

  auto insert_data_stmt = db.prepare(fl::detail::format(
    R"SQL(

      insert into "data"({} id) /* c1, c2, ... */
      values({} :id)            /* ?1, ?2, ... */

    )SQL",
    fl::detail::str(cx | fl::detail::suffix(", ")),
    fl::detail::str(cqx | fl::detail::suffix(", "))));

  auto select_sql = (fl::detail::format(
    R"SQL(

      select /* {} */
        {}   /* meta.p1, meta.p2, ..., */
        {}   /* data.c1, data.c2, ..., */
        null /* todo: unfortunate due to formatting limitations */
      from "meta", "data" using(id)
      where
        (meta.id = :id)

    )SQL",
    fl::detail::mangle_for_multiline_comment(table_name_),
    fl::detail::str(px | fl::detail::prefix("meta.") |
                    fl::detail::suffix(", ")),
    fl::detail::str(cx | fl::detail::prefix("data.") |
                    fl::detail::suffix(", "))));

  auto update_refcount_stmt = db.prepare(fl::detail::format(
    R"SQL(

      update "meta" /* {} */
      set "refcount" = "refcount" + :diff
      where "id" = :id

    )SQL", fl::detail::mangle_for_multiline_comment(table_name_)));

  auto fake_insert_sql = fl::detail::format(R"SQL(

      insert into {}.{} select null {} where false
 
    )SQL",
    fl::detail::quote_identifier(schema_name_),
    fl::detail::quote_identifier(table_name_),
    fl::detail::str(nx | fl::detail::prefix(", "))
    );

  return cache{
    db, insert_meta_stmt, insert_data_stmt, update_refcount_stmt,
    fake_insert_sql, select_sql, stmt.bind_parameter_count()
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

  // If the statement does not have bind parameters, xBestIndex would materialize
  // the statement. But in order to read out the column names from the statement,
  // it has to be prepared, which does trigger xBestIndex. So while we just want
  // the column names, disable materialization in xBestIndex in this scope.
  finding_out_column_name_guard g;

  auto stmt = db().prepare("select * from " + args.front());

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

  auto view_prefix = kwarg("view_prefix").value_or("");

  if (view_prefix != "") {

    fl::error::raise_if(!table_name_.starts_with(view_prefix), "bad view_prefix");

    std::string view_name = table_name_.substr(view_prefix.size());

    db().execute_script(fl::detail::format(R"SQL(

      drop view if exists {}.{};
      create view {}.{} as
      {}
      ;

      )SQL",
      fl::detail::quote_identifier(schema_name_),
      fl::detail::quote_identifier(view_name),
      fl::detail::quote_identifier(schema_name_),
      fl::detail::quote_identifier(view_name),
      fl::detail::extract_query( args.front() )
    ));

  }

  declare(create_table_sql);
}

void vt_stmt::xBegin() {

  // NOTE: This should probably only use nested transactions

  // std::cerr << "begin transaction" << " " << table_name_ << std::endl;
  cache_->db.execute_script("savepoint 'transaction'");
}

void vt_stmt::xSync() {
  // std::cerr << "xSync" << std::endl;
}

void vt_stmt::xCommit() {
  // std::cerr << "commit transaction" << " " << table_name_ << std::endl;

  // NOTE: as of SQLite 3.41.0 xCommit will be called without prior call to
  // xBegin when the virtual table is created within a transaction. Since we
  // never started a transaction in the temporary database, we cannot commit
  // it either.
  if (cache_->db.txn_state("main") != SQLITE_TXN_NONE) {
    cache_->db.execute_script("commit transaction");
  }
}

void vt_stmt::xRollback() {
  // std::cerr << "rollback transaction" << std::endl;
  cache_->db.execute_script("rollback to savepoint 'transaction'");
  cache_->db.execute_script("release savepoint 'transaction'");
}

void vt_stmt::xSavepoint(int savepoint) {
  // std::cerr << "savepoint" << " " << savepoint << std::endl;
  cache_->db.execute_script(fl::detail::format("savepoint {}",
    fl::detail::quote_string(std::to_string(savepoint))));
}

void vt_stmt::xRelease(int savepoint) {
  // std::cerr << "release" << " " << savepoint << std::endl;
  cache_->db.execute_script(fl::detail::format("release savepoint {}",
    fl::detail::quote_string(std::to_string(savepoint))));
}

void vt_stmt::xRollbackTo(int savepoint) {
  // std::cerr << "rollback to" << " " << savepoint << std::endl;
  cache_->db.execute_script(fl::detail::format("rollback to savepoint {}",
    fl::detail::quote_string(std::to_string(savepoint))));
}

bool
schema_has_index_named(fl::db db, std::string index_name) {
  return !db.prepare(R"SQL(

    select 1 from sqlite_schema where type is 'index' and name = 

  )SQL" + fl::detail::quote_string(index_name)).execute().empty();
}

bool
schema_has_table_named(fl::db db, std::string table_name) {
  return !db.prepare(R"SQL(

    select 1 from sqlite_schema where type is 'table' and name = 

  )SQL" + fl::detail::quote_string(table_name)).execute().empty();
}

int
estimate_rows_by_table_name(fl::db db, std::string table_name) {
  return 100;
}

int
estimate_rows_by_index_name(fl::db db, std::string index_name) {

  if (schema_has_table_named(db, "sqlite_stat1")) {

    auto stat1_stmt = db.prepare(R"SQL(

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
        return std::stoi(row_count);
      }
    }
  }

  return 100;
}

int
estimate_rows_by_constraints(fl::db db, const fl::vtab::index_info& info) {
  return 1000;
}

bool
vt_stmt::xBestIndex(fl::vtab::index_info& info)
{

  bool rowid_eq_null = false;

  // Check for a `WHERE rowid = NULL` constraint as a signal that the rowset
  // is not actually needed. This allows `EXPLAIN` or `EXPLAIN QUERY PLAN`
  // without the cost of materializing the statement first. Note though that
  // without materializing the statement there cannot be any statistics and
  // the query plan might be different from what one would get otherwise.

  if (!info.columns[0].constraints.empty()) {
    auto&& con = info.columns[0].constraints[0];
    if (con.op == SQLITE_INDEX_CONSTRAINT_EQ
      && con.rhs.has_value()
      && std::holds_alternative<fl::value::null>(con.rhs.value())) {
      // `rowid = NULL` is never true
      rowid_eq_null = true;
    }
  }

  // Materialize now to gather statistics if there are no parameters
  if (1 > cache_->bind_parameter_count && !finding_out_column_names && !rowid_eq_null) {
    auto copy = info;

    for (auto&& column : copy.columns) {
      column.constraints.resize(0);
    }

    vt_stmt::cursor c(this);
    xFilter(copy, &c);
  }

  // By default the base class will request the values for all `ÈQ`
  // constraints for all columns marked required. We can use more
  // constraints when retrieving a result from the cache.

  for (auto&& column : info.columns) {
    for (auto&& constraint : column.constraints) {
      if (constraint.usable && column.column_index >= 0) {

        // TODO: is this still a good idea?
        info.estimated_cost = 1e3;
        if (!constraint.argv_index) {
          constraint.argv_index = info.next_argv_index++;
        }
      }
    }
  }

  auto idx = to_create_index("data", info);
  auto create_index_sql = idx.first;
  auto index_name = idx.second;

  // TODO: even with no index, table row count can be gathered from sqlite_stat1

  if (!create_index_sql.empty()) {

    if (!schema_has_index_named(cache_->db, index_name)) {
      cache_->db.prepare(create_index_sql).execute();
    }

    // TODO: Does not really make sense here
    // TODO: do not analyze if index already existed
    // TODO: instead, analyze after running the statement and adding new data
    // TODO: ... but what if we are adding an index 
    cache_->db.prepare("analyze " + fl::detail::quote_identifier(index_name)).execute();

    auto est = estimate_rows_by_index_name(cache_->db, index_name);
    info.estimated_rows = est;
    info.estimated_cost = 6 * est;

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

    auto user_stmt = db().prepare(
      fl::detail::format(
        "select * from /* {} */ " + arguments().front(),
        fl::detail::mangle_for_multiline_comment(table_name_)));

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

  cache->insert_meta_stmt.reset();

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
