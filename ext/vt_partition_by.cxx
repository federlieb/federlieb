#include "vt_partition_by.hxx"

namespace fl = ::federlieb;

void
vt_partition_by::xConnect(bool create)
{

  declare(R"SQL(

      CREATE TABLE fl_partition_by(
        cursor_ptr ANY [BLOB] HIDDEN,
        element    ANY [BLOB],
        previous   INT,
        current    ANY [BLOB] NOT NULL,
        round      INT,
        history    INT HIDDEN
      )

    )SQL");
}

vt_partition_by::cursor::cursor(vt_partition_by* vtab)
  : tmpdb_(":memory:")
{

  tmpdb_.execute_script(R"SQL(

    DROP TABLE IF EXISTS history;
    CREATE TABLE history(
      element BLOB NOT NULL,
      src BLOB,
      dst BLOB NOT NULL,
      round INT NOT NULL,
      UNIQUE(element, round)
    );

    DROP TABLE IF EXISTS tracking;
    CREATE TABLE tracking(
      element BLOB PRIMARY KEY NOT NULL,
      representative BLOB,
      round INT NOT NULL DEFAULT 1
    );

    DROP TABLE IF EXISTS signature;
    CREATE TABLE signature(
      element BLOB PRIMARY KEY NOT NULL,
      signature BLOB
    );

    CREATE INDEX tracking_element
      ON tracking(element);

    CREATE INDEX tracking_representative
      ON tracking(representative);

    CREATE TRIGGER trigger_tracking_update
    AFTER UPDATE ON tracking
    BEGIN
      INSERT INTO history(element, src, dst, round)
      VALUES(
        old.element,
        old.representative,
        new.representative,
        new.round
      );
    END;

  )SQL");
}

fl::stmt
vt_partition_by::subquery(const fl::vtab::index_info& info,
                          vt_partition_by::cursor* cursor)
{

  auto sub_stmt = cursor->tmpdb_.prepare(R"SQL(

    SELECT
      NULL, element, NULL, representative, round, NULL
    FROM
      tracking
    WHERE
      element = :element

  )SQL");

  auto element_eq = info.get("element", SQLITE_INDEX_CONSTRAINT_EQ);
  sub_stmt.bind(":element", element_eq->current_value.value());

  sub_stmt.execute();

  return sub_stmt;
}

void
vt_partition_by::refine(vt_partition_by::cursor* cursor)
{
  auto refine_stmt = cursor->tmpdb_.prepare(R"SQL(

    WITH
    refinement AS (
      SELECT
        element,
        MIN(tracking.element) OVER w AS representative
      FROM
        tracking INNER JOIN signature USING(element)
      WINDOW
        w AS (
          ORDER BY signature.signature, tracking.representative
          GROUPS CURRENT ROW
        )
    )
    UPDATE
      tracking
    SET
      round = (SELECT MAX(round) FROM tracking) + 1,
      representative = refinement.representative
    FROM
      refinement
    WHERE
      refinement.element = tracking.element
      AND
      refinement.representative IS NOT tracking.representative

  )SQL");

  refine_stmt.execute();
}

void
vt_partition_by::project(const std::string& sql,
                         vt_partition_by::cursor* cursor)
{

  // TODO: ought to be cached for then_bys
  auto projection_stmt = db().prepare(fl::detail::format(
    R"SQL(

      WITH
      {} AS MATERIALIZED (
        SELECT
          element, current
        FROM
          {}.{}
        WHERE
          (cursor_ptr IS :cursor_ptr)
      )
      SELECT * FROM {}

    )SQL",
    fl::detail::quote_identifier(table_name_),
    fl::detail::quote_identifier(schema_name_),
    fl::detail::quote_identifier(table_name_),
    sql));

  projection_stmt.bind_pointer(
    ":cursor_ptr", "partition_by:cursor_ptr", static_cast<void*>(cursor));

  projection_stmt.execute();

  cursor->tmpdb_.prepare("DELETE FROM signature").execute();

  cursor->tmpdb_
    .prepare("INSERT INTO signature(element, signature) VALUES(?1, ?2)")
    .executemany(projection_stmt);

  refine(cursor);
}

void
vt_partition_by::validate_projections()
{
  if (!projections_validated) {

    // The virtual table accepts named parameters `once_by` and `then_by`
    // that contain SQL statements. They will be executed with a special
    // named parameter `:cursor_ptr` bound to a pointer using the pointer
    // passing interface. To prevent malicious use of that pointer, the
    // user-supplied statements must not reference this parameter.
    //
    // This check ought to occur past `xConnect` to allow references to
    // objects that have not been created yet, but prior to `xFilter`, as
    // that is usually called a lot more often. If the user-supplied
    // statements reference this table as `[schema].[table_name]`, this
    // will likely go into an infinite loop.

    for (auto kwarg : kwarguments()) {
      if (kwarg.first == "once_by" || kwarg.first == "then_by") {

        auto stmt = db().prepare(fl::detail::format(
          R"SQL(
              WITH {} AS (SELECT 1 AS element, 1 AS current)
              SELECT * FROM {}
            )SQL",
          fl::detail::quote_identifier(table_name_),
          kwarg.second));

        fl::error::raise_if(0 != stmt.bind_parameter_index(":cursor_ptr"),
                            ":cursor_ptr in query");
      }
    }
    projections_validated = true;
  }
}

void
vt_partition_by::apply_once_bys(vt_partition_by::cursor* cursor)
{

  for (auto kwarg : kwarguments()) {
    if (kwarg.first == "once_by") {
      project(kwarg.second, cursor);
    }
  }
}

void
vt_partition_by::apply_then_bys(vt_partition_by::cursor* cursor)
{
  auto then_bys =
    fl::detail::to_vector(kwarguments() | std::views::filter([](auto e) {
                            return e.first == "then_by";
                          }));

  while (true) {
    auto before = cursor->tmpdb_.select_scalar(
      "SELECT COUNT(DISTINCT representative) FROM tracking");

    for (auto&& current : then_bys) {
      project(current.second, cursor);
    }

    auto after = cursor->tmpdb_.select_scalar(
      "SELECT COUNT(DISTINCT representative) FROM tracking");

    if (after <= before) {
      break;
    }
  }
}

auto
vt_partition_by::select_finals(vt_partition_by::cursor* cursor)
{
  auto result_stmt = cursor->tmpdb_.prepare(R"SQL(

    SELECT
      NULL,
      element,
      NULL,
      representative,
      round,
      FALSE
    FROM
      tracking

  )SQL");

  result_stmt.execute();

  return result_stmt;
}

auto
vt_partition_by::select_history(vt_partition_by::cursor* cursor)
{
  auto result_stmt = cursor->tmpdb_.prepare(R"SQL(

    SELECT
      NULL,
      element,
      src,
      dst,
      round,
      TRUE
    FROM
      history

  )SQL");

  result_stmt.execute();

  return result_stmt;
}

bool
vt_partition_by::xBestIndex(fl::vtab::index_info& info)
{
  validate_projections();
  info.mark_wanted("cursor_ptr", SQLITE_INDEX_CONSTRAINT_IS);
  info.mark_wanted("history", SQLITE_INDEX_CONSTRAINT_EQ);
  info.mark_wanted("element", SQLITE_INDEX_CONSTRAINT_EQ);

  auto cursor_ptr_is = info.get("cursor_ptr", SQLITE_INDEX_CONSTRAINT_IS);
  auto element_eq = info.get("element", SQLITE_INDEX_CONSTRAINT_EQ);

  if (cursor_ptr_is != nullptr && element_eq == nullptr) {
    return false;
  }

  return true;
}

fl::stmt
vt_partition_by::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto cursor_ptr_is = info.get("cursor_ptr", SQLITE_INDEX_CONSTRAINT_IS);

  if (nullptr != cursor_ptr_is) {

    // If there is a `cursor_ptr IS :value` constraint, the table is
    // likely asking for intermediate results from itself. Using the
    // pointer passing interface, we obtain a pointer to the running
    // cursor and hand off generation of results to `subquery`.

    auto ptr = fl::api(sqlite3_value_pointer,
                       db_.get(),
                       cursor_ptr_is->current_raw,
                       "partition_by:cursor_ptr");

    fl::error::raise_if(nullptr == ptr, "invalid pointer");

    return subquery(info, static_cast<vt_partition_by::cursor*>(ptr));
  }

  auto elements_stmt = db().prepare(
    fl::detail::format("SELECT * FROM {}", kwarg_or_throw("elements")));

  elements_stmt.execute();

  cursor
    ->tmpdb_ //
    .prepare("INSERT INTO tracking(element) VALUES(?1)")
    .executemany(elements_stmt);

  apply_once_bys(cursor);
  apply_then_bys(cursor);

  auto history_eq = info.get("history", SQLITE_INDEX_CONSTRAINT_EQ);

  if (nullptr != history_eq &&
      fl::value::as<int>(history_eq->current_raw) != 0) {

    return select_history(cursor);

  } else {

    return select_finals(cursor);
  }
}
