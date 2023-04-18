#include <boost/tokenizer.hpp>

#include "vt_stat1.hxx"

namespace fl = ::federlieb;

void
vt_stat1::xConnect(bool create)
{
  declare(R"SQL(
      CREATE TABLE fl_stat1(
        tbl   TEXT,
        idx   TEXT,
        seqno INT,
        cnt   INT,
        arg   TEXT
      )
    )SQL");
}

static bool
schema_has_table_named(fl::db db, std::string table_name) {
  return !db.prepare(R"SQL(

    select 1 from sqlite_schema where type is 'table' and name = 

  )SQL" + fl::detail::quote_string(table_name)).execute().empty();
}

vt_stat1::result_type
vt_stat1::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  if (!schema_has_table_named(db(), "sqlite_stat1")) {
    return {};
  }

  struct sqlite_stat1 {
    fl::value::variant tbl;
    fl::value::variant idx;
    std::string stat;
  };

  auto stmt = db().prepare("select * from sqlite_stat1").execute();

  vt_stat1::result_type result;
  for (auto&& row : stmt | fl::as<sqlite_stat1>()) {

    // TODO: this is just guessing separators
    // TODO: Also, no idea what to do if separators are repeated.
    boost::char_separator<char> separator("\t\r\n ");
    boost::tokenizer< boost::char_separator<char> > tokens(row.stat, separator);
    int seqno = 0;
    for (auto&& token : tokens) {
      auto xpos = token.find_first_not_of("0123456789");

      fl::value::variant cnt = fl::value::null{};
      fl::value::variant arg = fl::value::null{};

      if (xpos != std::string::npos) {
        arg = fl::value::text{ token };
      } else {
        // TODO: what if stoi fails / token is too big?
        cnt = fl::value::integer{ std::stoi(token) };
      }

      result.push_back({
          row.tbl,
          row.idx,
          fl::value::integer{seqno},
          cnt,
          arg
      });

      seqno += 1;

    }

  }

  return result;

}
