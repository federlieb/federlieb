#include "vt_stmt_source.hxx"

namespace fl = ::federlieb;

void
vt_stmt_source::xConnect(bool create)
{
  declare(R"SQL(
      CREATE TABLE fl_stmt_source(
        stmt ANY [BLOB] VT_REQUIRED HIDDEN,
        cid INT,
        name TEXT,
        source_schema TEXT,
        source_table_name TEXT,
        source_column_name TEXT
      )
    )SQL");
}

fl::value::variant
text_or_null(char const* const string) {
    if (string) {
        return fl::value::text{ string };
    }
    return fl::value::null{};
}

vt_stmt_source::result_type
vt_stmt_source::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto stmt_variant =
    info.get("stmt", SQLITE_INDEX_CONSTRAINT_EQ)->current_value.value();

  vt_stmt_source::result_type result;

  // TODO: support pointer passing interface to pass existing stmt?

  if (std::holds_alternative<fl::value::text>(stmt_variant)) {
    auto sql = std::get<fl::value::text>(stmt_variant).value;
    auto stmt = db().prepare(sql);

    for (auto&& column : stmt.columns()) {

        // TODO: what if the API is not available? No query solution?

        auto table_name = fl::api(sqlite3_column_table_name,
            db().ptr().get(), stmt.ptr().get(), column.index());

        auto database_name = fl::api(sqlite3_column_database_name,
            db().ptr().get(), stmt.ptr().get(), column.index());

        auto column_name = fl::api(sqlite3_column_origin_name,
            db().ptr().get(), stmt.ptr().get(), column.index());

        result.push_back({
            stmt_variant,
            fl::value::integer{ column.index() },
            fl::value::text{ column.name() },
            text_or_null(database_name),
            text_or_null(table_name),
            text_or_null(column_name)
        });

    }

  } else {
    // TODO: error handling?
  }

  return result;
}
