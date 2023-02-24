#include <boost/graph/dijkstra_shortest_paths.hpp>

#include "federlieb/federlieb.hxx"
#include "vt_dijkstra_shortest_paths.hxx"

namespace fl = ::federlieb;

vt_dijkstra_shortest_paths::cursor::cursor(vt_dijkstra_shortest_paths* vtab)
{
  g_.import(vtab);
}

void
vt_dijkstra_shortest_paths::xConnect(bool create)
{

  declare(R"(
      CREATE TABLE fl_dijkstra_shortest_paths(
        source        ANY [BLOB] HIDDEN VT_REQUIRED,
        column        TEXT HIDDEN,
        vertex        ANY [BLOB],
        predecessor   ANY [BLOB],
        distance      REAL
      )
    )");
}

vt_dijkstra_shortest_paths::result_type
vt_dijkstra_shortest_paths::xFilter(const fl::vtab::index_info& info,
                                    cursor* cursor)
{

  auto source =
    info.get("source", SQLITE_INDEX_CONSTRAINT_EQ)->current_value.value();
  auto weights = info.get("column", SQLITE_INDEX_CONSTRAINT_EQ);

  std::string weight_column_name =
    (weights && weights->current_raw)
      ? fl::value::as<std::string>(weights->current_raw)
      : "weight";

  auto& g = cursor->g_.graph_;

  using graph_type = std::remove_reference_t<decltype(g)>;
  using vertex_descriptor = graph_type::vertex_descriptor;
  using edge_descriptor = graph_type::edge_descriptor;

  std::vector<vertex_descriptor> predecessors(boost::num_vertices(g));
  std::vector<double> distances(boost::num_vertices(g));
  std::map<edge_descriptor, double> weight_map;

  for (auto&& e : boost::make_iterator_range(boost::edges(g))) {
    auto w = g[e].at(weight_column_name);
    // TODO: This could be friendlier
    weight_map.insert({ e, std::get<fl::value::real>(w).value });
  }

  auto weight_pmap = boost::make_assoc_property_map(weight_map);
  auto distance_pmap = boost::make_iterator_property_map(
    distances.begin(), boost::get(boost::vertex_index, g));
  auto predecessor_pmap = boost::make_iterator_property_map(
    predecessors.begin(), boost::get(boost::vertex_index, g));

  boost::dijkstra_shortest_paths(
    g,
    cursor->g_.vertex(source),
    predecessor_pmap,
    distance_pmap,
    weight_pmap,
    boost::get(boost::vertex_index, g),
    std::less<double>(),
    boost::closed_plus<double>(),
    std::numeric_limits<double>::infinity(),
    0,
    boost::dijkstra_visitor<boost::null_visitor>());

  result_type result;

  for (auto v : boost::make_iterator_range(boost::vertices(g))) {
    result.push_back({ source,
                       fl::value::from(weight_column_name),
                       cursor->g_.variant(v),
                       cursor->g_.variant(predecessors[v]),
                       std::isinf(distances[v])
                         ? fl::value::null{}
                         : fl::value::from(distances[v]) });
  }

  return result;
}
