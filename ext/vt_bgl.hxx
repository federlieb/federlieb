#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/copy.hpp>
#include <boost/graph/copy.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/dominator_tree.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/transitive_closure.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/property_map/property_map.hpp>

#if 0
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/topological_sort.hpp>
#endif

#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

using Properties = std::map<std::string, fl::value::variant>;

using Directed = boost::adjacency_list<boost::vecS,
                                       boost::vecS,
                                       boost::bidirectionalS,
                                       Properties,
                                       Properties>;

using Undirected = boost::adjacency_list<boost::vecS,
                                         boost::vecS,
                                         boost::undirectedS,
                                         Properties,
                                         Properties>;

template<typename T>
struct variant_graph
{
  using vertex_type = boost::graph_traits<T>::vertex_descriptor;

  T graph_;

  // This used to use boost::bimap, until hitting cases where insert
  // and lookup returned buggy results for unknown reasons.
  std::map<fl::value::variant, vertex_type> variant_to_vertex_;
  std::map<vertex_type, fl::value::variant> vertex_to_variant_;

  fl::value::variant variant(const T::vertex_descriptor& vertex) const
  {
    return vertex_to_variant_.at(vertex);
  }

  vertex_type operator[](const fl::value::variant& variant)
  {

    if (!variant_to_vertex_.contains(variant)) {
      auto vertex = boost::add_vertex(graph_);
      auto i1 = variant_to_vertex_.insert({ variant, vertex });
      auto i2 = vertex_to_variant_.insert({ vertex, variant });
    }

    return variant_to_vertex_.at(variant);
  }

  vertex_type vertex(const fl::value::variant& variant)
  {
    return operator[](variant);
  }

  auto edge(const fl::value::variant& src, const fl::value::variant& dst)
  {
    return boost::add_edge(
      this->operator[](src), this->operator[](dst), graph_);
  }

  void import(fl::stmt& vertices_stmt, fl::stmt& edges_stmt)
  {
    vertices_stmt.reset().execute();

    for (auto row : vertices_stmt) {
      auto v = vertex(row.at(0).to_variant());
      for (auto prop : row | std::views::drop(1)) {
        graph_[v].insert_or_assign(prop.name(), prop.to_variant());
      }
    }

    fl::error::raise_if(edges_stmt.column_count() < 2, "not enough columns");

    edges_stmt.reset().execute();

    for (auto row : edges_stmt) {
      auto e = edge(row.at(0), row.at(1));

      for (auto prop : row | std::views::drop(2)) {
        graph_[e.first].insert_or_assign(prop.name(), prop.to_variant());
      }
    }
  }

  template<typename Vtab>
  void import(fl::vtab::base<Vtab>* vtab)
  {
    auto vertices_stmt = vtab->db().prepare(fl::detail::format(
      "SELECT * FROM {}",
      vtab->kwarg("vertices").value_or("(SELECT NULL WHERE FALSE)")));

    auto edges_stmt = vtab->db().prepare(
      fl::detail::format("SELECT * FROM {}", vtab->kwarg_or_throw("edges")));

    import(vertices_stmt, edges_stmt);
  }

  fl::stmt to_component_stmt(std::vector<size_t> component) const
  {
    fl::db tmp(":memory:");

    tmp.execute_script(R"SQL(

      CREATE TABLE data(vertex ANY [BLOB], component INT NOT NULL)

    )SQL");

    auto insert_stmt = tmp.prepare(R"SQL(

      INSERT INTO data VALUES(?1, ?2)

    )SQL");

    for (auto&& e : boost::make_iterator_range(boost::vertices(graph_))) {
      insert_stmt.reset().execute(variant(e), component[e]);
    }

    auto result_stmt = tmp.prepare(R"SQL(

      SELECT
        vertex,
        component,
        MIN(vertex) OVER (PARTITION BY component) AS representative
      FROM
        data

    )SQL");

    result_stmt.execute();

    return result_stmt;
  }
};
