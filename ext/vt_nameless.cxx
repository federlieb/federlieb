#include <regex>

#include "vt_nameless.hxx"

namespace fl = ::federlieb;

void
vt_nameless::xConnect(bool create)
{
  declare(R"SQL(
      CREATE TABLE fl_nameless(
        source TEXT VT_REQUIRED HIDDEN NOT NULL,
        p1 ANY [BLOB] HIDDEN,
        p2 ANY [BLOB] HIDDEN,
        p3 ANY [BLOB] HIDDEN,
        p4 ANY [BLOB] HIDDEN,
        p5 ANY [BLOB] HIDDEN,
        c1 ANY [BLOB],
        c2 ANY [BLOB],
        c3 ANY [BLOB],
        c4 ANY [BLOB],
        c5 ANY [BLOB],
        c6 ANY [BLOB],
        c7 ANY [BLOB],
        c8 ANY [BLOB],
        c9 ANY [BLOB]
      )
    )SQL");
}

static const std::array<std::string, 5> params_ = { "p1",
                                                    "p2",
                                                    "p3",
                                                    "p4",
                                                    "p5" };

static const std::array<std::string, 9> cols_ = { "c1", "c2", "c3", "c4", "c5",
                                                  "c6", "c7", "c8", "c9" };
bool
vt_nameless::xBestIndex(fl::vtab::index_info& info)
{
  for (auto&& e : params_) {
    info.mark_wanted(e, SQLITE_INDEX_CONSTRAINT_EQ);
  }

  for (auto&& e : cols_) {
    info.mark_transferables(e);
  }

  return true;
}

fl::stmt
vt_nameless::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto source_variant =
    info.get("source", SQLITE_INDEX_CONSTRAINT_EQ)->current_value.value();

  fl::error::raise_if(!std::holds_alternative<fl::value::text>(source_variant),
                      "bad source");

  auto source = std::get<fl::value::text>(source_variant).value;

  std::regex re(R"(^\w+$)");

  std::string source_sql =
    std::regex_match(source, re)
      ? fl::detail::format("SELECT * FROM {}",
                           fl::detail::quote_identifier(source))
      : source;

  auto inner_stmt = db().prepare(source_sql);

  auto cols = (inner_stmt.columns() | std::views::take(9) |
               std::views::transform([](const fl::column& e) {
                 return fl::detail::quote_identifier(e.name()) + " AS c" +
                        std::to_string(1 + e.index());
               }));

  auto nulls = (std::views::iota(inner_stmt.column_count() + 1, 10) |
                std::views::transform(
                  [](int const e) { return "NULL AS c" + std::to_string(e); }));

  auto sql = fl::detail::format(
    R"SQL(
      SELECT * FROM (

        SELECT
          :source AS "source",
          :p1 AS "p1",
          :p2 AS "p2",
          :p3 AS "p3",
          :p4 AS "p4",
          :p5 AS "p5"
          {}
          {}
        FROM
          ({})
      
      ) t

    )SQL",
    fl::detail::str(cols | fl::detail::prefix(", ")),
    fl::detail::str(nulls | fl::detail::prefix(", ")),
    source);

  auto pred = fl::vtab::usable_constraints_to_where_fragment("t", info);

  if (!pred.empty()) {
    sql += "WHERE " + pred;
  }

  auto stmt = db().prepare(sql);

  stmt.bind(":source", source);

  for (auto p : params_) {
    auto constraint = info.get(p, SQLITE_INDEX_CONSTRAINT_EQ);
    if (constraint) {
      stmt.bind(":" + p, constraint->current_value.value());
    }
  }

  stmt.execute();

  return stmt;
}
