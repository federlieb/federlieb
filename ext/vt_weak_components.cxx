#include "federlieb/federlieb.hxx"

#include "vt_weak_components.hxx"

namespace fl = ::federlieb;

vt_weak_components::cursor::cursor(vt_weak_components* vtab)
{
  g_.import(vtab);
}

void
vt_weak_components::xConnect(bool create)
{

  declare(R"SQL(

    CREATE TABLE fl_weakly_connected_components(
      vertex              ANY [BLOB],
      component           INT NOT NULL,
      representative      ANY [BLOB]
    )

  )SQL");
}

fl::stmt
vt_weak_components::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  std::vector<size_t> component(boost::num_vertices(cursor->g_.graph_));
  size_t num_components =
    boost::connected_components(cursor->g_.graph_, &component[0]);

  return cursor->g_.to_component_stmt(component);
}
