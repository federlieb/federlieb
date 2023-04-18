#pragma GCC diagnostic ignored "-Wconversion"

#include "federlieb/federlieb.hxx"

#include "vt_transitive_closure.hxx"

namespace fl = ::federlieb;

vt_transitive_closure::cursor::cursor(vt_transitive_closure* vtab)
{
  g_.import(vtab);
}

void
vt_transitive_closure::xConnect(bool create)
{

  declare(R"SQL(

    CREATE TABLE fl_transitive_closure(
      src ANY [BLOB],
      dst ANY [BLOB]
    )

  )SQL");
}

vt_transitive_closure::result_type
vt_transitive_closure::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  decltype(cursor->g_.graph_) tc_graph;

  boost::transitive_closure(cursor->g_.graph_, tc_graph);

  result_type result;

  for (auto e : boost::make_iterator_range(boost::edges(tc_graph))) {
    result.push_back({ cursor->g_.variant(boost::source(e, tc_graph)),
                       cursor->g_.variant(boost::target(e, tc_graph)) });
  }

  return result;
}
