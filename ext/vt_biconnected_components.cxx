#include "federlieb/federlieb.hxx"

#include "vt_biconnected_components.hxx"

namespace fl = ::federlieb;

vt_biconnected_components::cursor::cursor(vt_biconnected_components* vtab)
{
  g_.import(vtab);
}

void
vt_biconnected_components::xConnect(bool create)
{

  declare(R"SQL(

    CREATE TABLE fl_biconnected_components(
      src                 ANY [BLOB],
      dst                 ANY [BLOB],
      component           INT NOT NULL
    )

  )SQL");
}

vt_biconnected_components::result_type
vt_biconnected_components::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  using E = decltype(cursor->g_.graph_)::edge_descriptor;

  std::map< E, size_t > components;
  boost::associative_property_map<decltype(components)> pm(components);
  
  boost::biconnected_components(cursor->g_.graph_, pm);

  result_type result;

  for (auto e : boost::make_iterator_range(boost::edges(cursor->g_.graph_))) {

    result.push_back({ cursor->g_.variant(boost::source(e, cursor->g_.graph_)),
                       cursor->g_.variant(boost::target(e, cursor->g_.graph_)),
                       fl::value::from( 1 + pm[e]) });
  }

  return result;


}
