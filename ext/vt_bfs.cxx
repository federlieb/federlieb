#include <boost/graph/copy.hpp>

#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/visitors.hpp>

#include "federlieb/federlieb.hxx"
#include "vt_bfs.hxx"

namespace fl = ::federlieb;

// TODO: Option to stop on hitting certain vertices.

vt_breadth_first_search::cursor::cursor(vt_breadth_first_search* vtab)
{
  g_.import(vtab);
}

void
vt_breadth_first_search::xConnect(bool create)
{

  declare(R"(
      CREATE TABLE fl_breadth_first_search(
        root                ANY [BLOB] HIDDEN VT_REQUIRED,
        vertex              ANY [BLOB],
        distance            INT,
        discovery           INT
      )
    )");
}

vt_breadth_first_search::result_type
vt_breadth_first_search::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto& root = info.columns[1].constraints[0].current_value;

  auto root_vertex = cursor->g_.vertex(*root);

  std::vector<size_t> time(boost::num_vertices(cursor->g_.graph_));

  auto time_map = boost::make_iterator_property_map(
      time.begin(), boost::get(boost::vertex_index, cursor->g_.graph_));

  std::vector<size_t> dist(boost::num_vertices(cursor->g_.graph_));

  auto dist_map = boost::make_iterator_property_map(
      dist.begin(), boost::get(boost::vertex_index, cursor->g_.graph_));

  std::vector<boost::default_color_type> color(boost::num_vertices(cursor->g_.graph_));
  auto color_map = boost::make_iterator_property_map(color.begin(),
                                                     boost::get(boost::vertex_index, cursor->g_.graph_));

  size_t t = 0;

  std::stack<size_t> q;
  boost::breadth_first_visit(
    cursor->g_.graph_,
    cursor->g_[*root],
    q,
    boost::make_bfs_visitor(std::make_pair(
        (boost::stamp_times(time_map, t, boost::on_discover_vertex()))
        ,
        (boost::record_distances(dist_map, boost::on_examine_edge()))
    )),
    color_map
  );

  result_type result;

  for (auto v : boost::make_iterator_range(boost::vertices(cursor->g_.graph_))) {
    result.push_back({
      *root,
      cursor->g_.variant(v),
      fl::value::integer{ fl::detail::safe_to<sqlite3_int64>(dist[v]) },
      fl::value::integer{ fl::detail::safe_to<sqlite3_int64>(time[v]) }
    });
  }

  return result;
}
