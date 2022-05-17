#include <boost/graph/dominator_tree.hpp>

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

  lengauer_tarjan_dominator_tree(cursor->g_.graph_, cursor->g_[*root], dom_pm);

  result_type result;

  for (auto e : dom_map) {
    result.push_back(
      { *root, cursor->g_.variant(e.first), cursor->g_.variant(e.second) });
  }

  return result;
}
