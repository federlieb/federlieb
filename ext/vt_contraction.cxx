#include <set>
#include <stack>

#include "federlieb/federlieb.hxx"
#include "vt_contraction.hxx"

namespace fl = ::federlieb;

// TODO: There isn't really a point in having separate edge and history
// tables.

// TODO: Mode where history is not maintained if not needed afterwards.

void
vt_contraction::cursor::import_edges(vt_contraction* vtab)
{
  tmpdb_.execute_script(R"SQL(

    create table edge(
      src any [blob] not null,
      dst any [blob] not null,
      unique(src, dst),
      unique(dst, src)
    );

    create table history(
      src any [blob] not null,
      mid_src any [blob],
      mid_dst any [blob],
      dst any [blob] not null,
      contracted int not null default false
    );

    create index idx_history_src_dst on history(src,dst);

    create trigger trigger_delete_edge
    after delete on edge
    begin
      update
        history
      set
        contracted = true
      where 
        src = old.src
        and
        dst = old.dst
      ;
    end;
    ;

  )SQL");

  auto insert_edge_stmt =
    tmpdb_.prepare("insert into edge(src, dst) values(:src, :dst)");

  auto edges_stmt = vtab->db().prepare(
    fl::detail::format("select * from {}", vtab->kwarg_or_throw("edges")));

  edges_stmt.execute();

  insert_edge_stmt.executemany(edges_stmt);

  tmpdb_.execute_script(R"SQL(

    insert into history(src, dst) select src, dst from edge

  )SQL");

}

void
vt_contraction::cursor::contract_vertices(vt_contraction* vtab)
{
  if (!vtab->kwarg("contract_vertices").has_value()) {
    return;
  }

  auto contract_vertices_stmt = vtab->db().prepare(fl::detail::format(R"SQL(

      with
      {} as materialized (
        select * from {}.{} where contracted is false and cursor_ptr is :ptr
      ),
      {}(vertex) as materialized (
        select src from {}
        union
        select dst from {}
      )
      select * from {}

    )SQL",
    fl::detail::quote_identifier(vtab->table_name_),
    fl::detail::quote_identifier(vtab->schema_name_),
    fl::detail::quote_identifier(vtab->table_name_),
    fl::detail::quote_identifier(vtab->table_name_ + "_vertex"),
    fl::detail::quote_identifier(vtab->table_name_),
    fl::detail::quote_identifier(vtab->table_name_),
    vtab->kwarg("contract_vertices").value()

  ));

  auto vertex_history_stmt = tmpdb_.prepare(R"SQL(

    insert into history(src, mid_src, mid_dst, dst)
    select
      p.src,
      :vertex,
      case
      when exists (select 1 from edge where src = dst and src = :vertex) then :vertex
      else null
      end,
      s.dst
    from
      edge p
        inner join edge s
          on (p.dst = :vertex and s.src = :vertex)

  )SQL");

  auto vertex_edge_stmt = tmpdb_.prepare(R"SQL(

    insert or ignore into edge(src, dst)
    select
      p.src, s.dst
    from
      edge p
        inner join edge s
          on (p.dst = :vertex and s.src = :vertex)

  )SQL");

  auto vertex_delete_stmt = tmpdb_.prepare(R"SQL(

    -- https://sqlite.org/forum/forumpost/2ed79a01ae
    delete from edge where :vertex = src or :vertex = dst

  )SQL");

  struct vertex {
    fl::value::variant vertex;
  };

  // TODO: ought to be repeated until nothing changes anymore

  contract_vertices_stmt.execute();
  for (auto&& row : contract_vertices_stmt | fl::as<vertex>()) {
      vertex_history_stmt.reset().execute(row.vertex);
      vertex_edge_stmt.reset().execute(row.vertex);
      vertex_delete_stmt.reset().execute(row.vertex);
  }

}

void
vt_contraction::cursor::contract_edges(vt_contraction* vtab)
{

  if (!vtab->kwarg("contract_edges").has_value()) {
    return;
  }

  auto contract_edges_stmt = vtab->db().prepare(fl::detail::format(R"SQL(

      with
      {} as materialized (
        select * from {}.{} where contracted is false and cursor_ptr is :ptr
      ),
      {}(vertex) as materialized (
        select src from {}
        union
        select dst from {}
      )
      select * from {}

    )SQL",
    fl::detail::quote_identifier(vtab->table_name_),
    fl::detail::quote_identifier(vtab->schema_name_),
    fl::detail::quote_identifier(vtab->table_name_),
    fl::detail::quote_identifier(vtab->table_name_ + "_vertex"),
    fl::detail::quote_identifier(vtab->table_name_),
    fl::detail::quote_identifier(vtab->table_name_),
    vtab->kwarg("contract_edges").value()

  ));

  contract_edges_stmt.bind_pointer(
    ":ptr", "contraction:ptr", static_cast<void*>(this));

  auto edge_delete_stmt =
    tmpdb_.prepare("delete from edge where src = :src and dst = :dst");

  auto edge_history_stmt =
    tmpdb_.prepare("insert into history(src, mid_src, mid_dst, dst) "
                   "values(:src, :mid_src, :mid_dst, :dst)");

  auto edge_edge_stmt = tmpdb_.prepare("insert or ignore into edge(src, dst) "
                                       "values(:src, :dst)");

  auto edge_edges_stmt = tmpdb_.prepare(R"SQL(

    select
      p.src, :src, :dst, s.dst
    from 
      edge p
      inner join
      edge s
      on p.dst = :src and s.src = :dst

  )SQL");

  struct edge
  {
    fl::value::variant src;
    fl::value::variant dst;
    auto operator<=>(const edge&) const = default;
  };

  struct quad
  {
    fl::value::variant src;
    fl::value::variant mid_src;
    fl::value::variant mid_dst;
    fl::value::variant dst;
  };

  std::set<edge> seen;

  while (true) {

    std::stack<edge> todo;

    contract_edges_stmt.reset().execute();

    for (auto&& e : contract_edges_stmt | fl::as<edge>()) {
      if (!seen.contains(e)) {
        todo.push(e);
      }
    }

    if (todo.empty()) {
      break;
    }

    while (!todo.empty()) {

      if (vtab->db().is_interrupted()) {
        throw fl::error::interrupted();
      }

      auto current = todo.top();
      todo.pop();
      seen.insert(current);

      // std::cerr << "edge_edges_stmt..." << std::endl;
      edge_edges_stmt.execute(current.src, current.dst);

      for (auto&& q : edge_edges_stmt | fl::as<quad>()) {
        // std::cerr << "edge_history_stmt..." << std::endl;
        edge_history_stmt.execute(q.src, q.mid_src, q.mid_dst, q.dst);
        auto e = edge{ q.src, q.dst };
        if (!seen.contains(e)) {
          // todo.push(e);
          // std::cerr << "!! edge_edge_stmt..." << e.src << " " << e.dst << std::endl;
          edge_edge_stmt.execute(e.src, e.dst);
        }
      }

      // NOTE: previously deletion happened earlier.
      edge_delete_stmt.execute(current.src, current.dst);
    }

  }

}

vt_contraction::cursor::cursor(vt_contraction* vtab)
  : tmpdb_(":memory:")
{
}

void
vt_contraction::xConnect(bool create)
{

  declare(R"SQL(
    CREATE TABLE fl_iterated_contraction(
      src ANY [BLOB] NOT NULL,
      mid_src ANY [BLOB],
      mid_dst ANY [BLOB],
      dst ANY [BLOB] NOT NULL,
      contracted INT NOT NULL,
      cursor_ptr ANY [BLOB] HIDDEN
    )
  )SQL");
}

bool
vt_contraction::xBestIndex(fl::vtab::index_info& info)
{
  info.mark_wanted("cursor_ptr", SQLITE_INDEX_CONSTRAINT_IS);

  auto cursor_ptr_is = info.get("cursor_ptr", SQLITE_INDEX_CONSTRAINT_IS);

  if (cursor_ptr_is != nullptr) {
    // internal subquery
  }

  return true;
}


fl::stmt
vt_contraction::subquery(const fl::vtab::index_info& info,
                          vt_contraction::cursor* cursor)
{
  auto sub_stmt = cursor->tmpdb_.prepare("select *, null from history where contracted is false");
  return sub_stmt.execute();
}


fl::stmt
vt_contraction::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto cursor_ptr_is = info.get("cursor_ptr", SQLITE_INDEX_CONSTRAINT_IS);

  if (nullptr != cursor_ptr_is) {

    auto ptr = fl::api(sqlite3_value_pointer,
                       db_.get(),
                       cursor_ptr_is->current_raw,
                       "contraction:ptr");

    fl::error::raise_if(nullptr == ptr, "invalid pointer");

    return subquery(info, static_cast<vt_contraction::cursor*>(ptr));
  }

  cursor->import_edges(this);
  cursor->contract_vertices(this);
  cursor->contract_edges(this);

  auto result = cursor->tmpdb_.prepare("SELECT *, null FROM history");
  result.execute();
  return result;
}

/*

create virtual table t using fl_iterated_contraction(
  edges=(...),
  vertices=(...),
  vertex_if=(
    select vertex, true from t_vertex 
  ),
  edge_if=(
    select src, dst, true from t_edge
  )
)

*/