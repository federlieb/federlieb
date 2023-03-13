#include "federlieb/federlieb.hxx"

#include "vt_articulation_points.hxx"

namespace fl = ::federlieb;

vt_articulation_points::cursor::cursor(vt_articulation_points* vtab)
{
  g_.import(vtab);
}

void
vt_articulation_points::xConnect(bool create)
{

  declare(R"SQL(

    CREATE TABLE fl_articulation_points(
      vertex              ANY [BLOB]
    )

  )SQL");
}

vt_articulation_points::result_type
vt_articulation_points::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  std::set<size_t> points;

  boost::articulation_points(cursor->g_.graph_, std::inserter(points, points.end()));

  result_type result;

  for (auto e : points) {
    result.push_back({cursor->g_.variant( e )});
  }

  return result;
}
