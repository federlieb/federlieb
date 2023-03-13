#include "vt_colormap.hxx"

namespace fl = ::federlieb;

void
vt_colormap::xConnect(bool create)
{
  declare(R"SQL(
      CREATE TABLE fl_colormap(
        name TEXT HIDDEN VT_REQUIRED,
        at INT HIDDEN VT_REQUIRED,
        r INT,
        g INT,
        b INT,
        a INT,
        hex_rgb TEXT
       )
    )SQL");
}

bool
vt_colormap::xBestIndex(fl::vtab::index_info& info)
{
  return true;
}

fl::stmt
vt_colormap::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto& name = info.columns[1].constraints[0].current_value.value();
  auto& at = info.columns[2].constraints[0].current_value.value();

  auto name_str = std::get<fl::value::text>(name).value;
  auto at_int = std::get<fl::value::integer>(at).value;

  uint32_t value = 0;

  try {
      value = colormaps.at(name_str).at(at_int);
  }
  catch (const std::out_of_range& oor) {
    auto failure = db()
        .prepare("SELECT :name, :at, null, null, null, null, null WHERE false")
        .execute(name_str, at_int);
    return failure;
  }

  std::array<uint8_t, 4> rgba;

  for (uint8_t ix = 0; ix < rgba.size(); ++ix) {
    rgba[ix] = ((value >> (8 * ix)) & 0xff);
  }

  auto stmt = db()
    .prepare(R"SQL(
        SELECT
            :name,
            :at,
            :r,
            :g,
            :b,
            :a,
            format('#%02x%02x%02x', :r, :g, :b) as hex_rgb
    )SQL")
    .execute(name, at, rgba[0], rgba[1], rgba[2], rgba[3]);

  return stmt;
}
