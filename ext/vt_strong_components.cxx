#include "federlieb/federlieb.hxx"

#include "vt_strong_components.hxx"

namespace fl = ::federlieb;

vt_strong_components::cursor::cursor(vt_strong_components* vtab)
{
  g_.import(vtab);
}

void
vt_strong_components::xConnect(bool create)
{

  declare(R"SQL(

    CREATE TABLE fl_strongly_connected_components(
      vertex              ANY [BLOB],
      component           INT NOT NULL,
      representative      ANY [BLOB]
    )

  )SQL");
}

fl::stmt
vt_strong_components::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  std::vector<size_t> component(boost::num_vertices(cursor->g_.graph_));

  auto sc_pm = boost::make_iterator_property_map(
    component.begin(), boost::get(boost::vertex_index, cursor->g_.graph_));

  size_t num_components = boost::strong_components(cursor->g_.graph_, sc_pm);

  return cursor->g_.to_component_stmt(component);
}
