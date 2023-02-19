#include <set>
#include <stack>

#include "federlieb/federlieb.hxx"
#include "vt_contraction.hxx"

namespace fl = ::federlieb;

// TODO: Possibly use the pointer passing interface to make edge_if work on a
// whole set of edges instead of executing the query repeatedly?

void
vt_contraction::cursor::import_edges(vt_contraction* vtab)
{
  tmpdb_.execute_script(R"SQL(

    CREATE TABLE edge(
      src ANY [BLOB] NOT NULL,
      dst ANY [BLOB] NOT NULL,
      UNIQUE(src, dst),
      UNIQUE(dst, src)
    );

    CREATE TABLE history(
      src ANY [BLOB] NOT NULL,
      mid_src ANY [BLOB],
      mid_dst ANY [BLOB],
      dst ANY [BLOB] NOT NULL,
      contracted INT NOT NULL DEFAULT FALSE
    );

    CREATE INDEX idx_history_src_dst ON history(src,dst);

    CREATE TRIGGER trigger_delete_edge
    AFTER DELETE ON edge
    BEGIN
      UPDATE
        history
      SET
        contracted = TRUE
      WHERE 
        src = OLD.src
        AND
        dst = OLD.dst
      ;
    END;
    ;

  )SQL");

  auto insert_edge_stmt =
    tmpdb_.prepare("INSERT INTO edge(src, dst) VALUES(:src, :dst)");

  auto edges_stmt = vtab->db().prepare(
    fl::detail::format("SELECT * FROM {}", vtab->kwarg_or_throw("edges")));

  edges_stmt.execute();

  insert_edge_stmt.executemany(edges_stmt);

  tmpdb_.execute_script(R"SQL(

    INSERT INTO history(src, dst) SELECT src, dst FROM edge

  )SQL");
}

void
vt_contraction::cursor::contract_vertices(vt_contraction* vtab)
{

  auto vertex_if_sql = vtab->kwarg("vertex_if");

  if (!vertex_if_sql) {
    return;
  }

  auto vertices_stmt =
    tmpdb_.prepare("SELECT src FROM edge UNION SELECT dst FROM edge");

  vertices_stmt.execute();

  auto vertex_if_stmt = vtab->db().prepare(
    fl::detail::format("SELECT * FROM {}", *vertex_if_sql));

  auto vertex_history_stmt = tmpdb_.prepare(R"SQL(

    INSERT INTO history(src, mid_src, mid_dst, dst)
    SELECT
      p.src, :vertex, NULL, s.dst
    FROM
      edge p
        INNER JOIN edge s
          ON (p.dst = :vertex AND s.src = :vertex)

  )SQL");

  auto vertex_edge_stmt = tmpdb_.prepare(R"SQL(

    INSERT OR IGNORE INTO edge(src, dst)
    SELECT
      p.src, s.dst
    FROM
      edge p
        INNER JOIN edge s
          ON (p.dst = :vertex AND s.src = :vertex)

  )SQL");

  auto vertex_delete_stmt = tmpdb_.prepare(R"SQL(

    -- https://sqlite.org/forum/forumpost/2ed79a01ae
    DELETE FROM edge WHERE :vertex = src OR :vertex = dst

  )SQL");

  for (auto&& row : vertices_stmt) {
    auto&& vertex = row.at(0);

    vertex_if_stmt
      .reset()
      .bind(":vertex", vertex)
      .execute();

    if (vertex_if_stmt.empty()) {
      continue;
    }

    int can_remove = vertex_if_stmt.current_row().at(0);

    if (can_remove) {
      vertex_history_stmt.execute(vertex);
      vertex_edge_stmt.execute(vertex);
      vertex_delete_stmt.execute(vertex);
    }
  }
}

void
vt_contraction::cursor::contract_edges(vt_contraction* vtab)
{
  auto edge_if_sql = vtab->kwarg("edge_if");

  if (!edge_if_sql) {
    return;
  }

  auto edge_if_stmt = vtab->db().prepare(
    fl::detail::format("SELECT * FROM {}", *edge_if_sql));

  auto edge_delete_stmt =
    tmpdb_.prepare("DELETE FROM edge WHERE src = :src AND dst = :dst");

  auto edge_history_stmt =
    tmpdb_.prepare("INSERT INTO history(src, mid_src, mid_dst, dst) "
                   "VALUES(:src, :mid_src, :mid_dst, :dst)");

  auto edge_edge_stmt = tmpdb_.prepare("INSERT OR IGNORE INTO edge(src, dst) "
                                       "VALUES(:src, :dst)");

  auto edge_edges_stmt = tmpdb_.prepare(R"SQL(

    SELECT
      p.src, :src, :dst, s.dst
    FROM 
      edge p
      INNER JOIN
      edge s
      ON p.dst = :src AND s.src = :dst

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

  std::stack<edge> todo;

  auto init = tmpdb_.prepare("SELECT src, dst FROM edge");

  for (auto&& e : init.execute()) {
    todo.push(edge{ e.at(0), e.at(1) });
  }

  std::set<edge> seen;

  while (!todo.empty()) {
    auto current = todo.top();
    todo.pop();
    seen.insert(current);

    edge_if_stmt
      .reset()
      .bind(":src", current.src)
      .bind(":dst", current.dst)
      .execute();
    
    auto contract =
      (!edge_if_stmt.empty() && int(edge_if_stmt.current_row().at(0)));

    if (!contract) {
      continue;
    }

    edge_edges_stmt.execute(current.src, current.dst);

    for (auto&& q : edge_edges_stmt | fl::as<quad>()) {
      edge_history_stmt.execute(q.src, q.mid_src, q.mid_dst, q.dst);
      auto e = edge{ q.src, q.dst };
      if (!seen.contains(e)) {
        todo.push(e);
        edge_edge_stmt.execute(e.src, e.dst);
      }
    }

    // NOTE: previously deletion happened earlier.
    edge_delete_stmt.execute(current.src, current.dst);
  }
}

vt_contraction::cursor::cursor(vt_contraction* vtab)
  : tmpdb_(":memory:")
{
  import_edges(vtab);
  contract_vertices(vtab);
  contract_edges(vtab);
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
      contracted INT NOT NULL
    )
  )SQL");
}

fl::stmt
vt_contraction::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{
  auto result = cursor->tmpdb_.prepare("SELECT * FROM history");
  result.execute();
  return result;
}
