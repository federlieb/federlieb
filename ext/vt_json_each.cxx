#include "vt_json_each.hxx"

namespace fl = ::federlieb;

void
vt_json_each::xConnect(bool create)
{
  declare(R"SQL(
      CREATE TABLE fl_json_each(
        json TEXT VT_REQUIRED HIDDEN NOT NULL,
        value ANY [BLOB]
      )
    )SQL");
}

bool
vt_json_each::xBestIndex(fl::vtab::index_info& info)
{
  auto eq = info.get("value", SQLITE_INDEX_CONSTRAINT_EQ);

  if (nullptr != eq && eq->usable) {
    info.estimated_cost = 1e200;
    // info.estimated_rows = 1e9;
  } else {
    info.estimated_cost = 1;
    info.estimated_rows = 25;
  }

  return true;
}

vt_json_each::result_type
vt_json_each::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto source_variant =
    info.get("json", SQLITE_INDEX_CONSTRAINT_EQ)->current_value.value();

  // FIXME: also accept dicts?

  boost::json::array array;

  // TODO: cleanup

  if (std::holds_alternative<fl::value::text>(source_variant)) {
    array = boost::json::parse(std::get<fl::value::text>(source_variant).value).as_array();
#if 0
  } else if (std::holds_alternative<fl::value::blob>(source_variant)) {
    std::string source;
    auto blob = std::get<fl::value::blob>(source_variant).value;
    std::copy(blob.begin(), blob.end(), source);
    array = boost::json::parse(source).as_array();
#endif
  } else if (std::holds_alternative<fl::value::json>(source_variant)) {
    array = boost::json::parse(std::get<fl::value::json>(source_variant).value).as_array();
  } else {
    fl::error::raise("bad source");
  }

  vt_json_each::result_type result;
  for (auto&& e : array) {

    // TODO: avoid copying source_variant if not requested?
    result.push_back({
        source_variant,
        boost::json::value_to<fl::value::variant>(e)
    });

  }

  return result;

}
