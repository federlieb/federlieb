#include <boost/graph/dominator_tree.hpp>
#include <boost/graph/copy.hpp>

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/visitors.hpp>

#include "federlieb/federlieb.hxx"
#include "vt_dominator_tree.hxx"

namespace fl = ::federlieb;

vt_dominator_tree::cursor::cursor(vt_dominator_tree* vtab)
{
  g_.import(vtab);
}

void
vt_dominator_tree::xConnect(bool create)
{

  declare(R"(
      CREATE TABLE fl_dominator_tree(
        root                ANY [BLOB] HIDDEN VT_REQUIRED,
        vertex              ANY [BLOB],
        immediate_dominator ANY [BLOB]
      )
    )");
}

vt_dominator_tree::result_type
vt_dominator_tree::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto& root = info.columns[1].constraints[0].current_value;

  using vertex_descriptor = decltype(cursor->g_.graph_)::vertex_descriptor;

  std::map<vertex_descriptor, vertex_descriptor> dom_map;
  auto dom_pm = boost::associative_property_map(dom_map);

  Directed copy;
  boost::copy_graph(cursor->g_.graph_, copy);

  std::vector<size_t> seen(boost::num_vertices(copy));

  auto seen_map = boost::make_iterator_property_map(
      seen.begin(), boost::get(boost::vertex_index, copy));

  std::vector<boost::default_color_type> color(boost::num_vertices(copy));
  auto color_map = boost::make_iterator_property_map(color.begin(),
                                                     boost::get(boost::vertex_index, copy));

  size_t t = 0;

  boost::depth_first_visit(copy, cursor->g_[*root],
                           boost::make_dfs_visitor(
                               boost::stamp_times(seen_map, t, boost::on_discover_vertex())),
                           color_map);

  for (size_t ix = 0; ix < seen.size(); ++ix) {
    if (!seen[ix]) {
      // std::cout << "clearing vertex " << ix << std::endl;
      boost::clear_vertex(ix, copy);
    }
  }

  lengauer_tarjan_dominator_tree(copy, cursor->g_[*root], dom_pm);

  result_type result;

  for (auto e : dom_map) {
    result.push_back(
      { *root, cursor->g_.variant(e.first), cursor->g_.variant(e.second) });
  }

  return result;
}
