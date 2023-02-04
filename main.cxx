#include <iostream>
#include <numeric>
#include <stddef.h>
#include <string>
#include <vector>

#include "ext/fx_counter.hxx"
#include "ext/fx_kcrypto.hxx"
#include "ext/fx_ordered_concat_agg.hxx"
#include "ext/fx_toset.hxx"

#include "ext/vt_contraction.hxx"
#include "ext/vt_dijkstra_shortest_paths.hxx"
#include "ext/vt_dominator_tree.hxx"
#include "ext/vt_nameless.hxx"
#include "ext/vt_partition_by.hxx"
#include "ext/vt_stmt.hxx"
#include "ext/vt_strong_components.hxx"
#include "ext/vt_transitive_closure.hxx"
#include "ext/vt_weak_components.hxx"

#include "federlieb/federlieb.hxx"
#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

int
main(void)
{

  auto db = fl::db(":memory:");

  vt_dominator_tree::register_module(db);
  vt_stmt::register_module(db);
  vt_partition_by::register_module(db);
  vt_strong_components::register_module(db);
  vt_weak_components::register_module(db);
  vt_transitive_closure::register_module(db);
  vt_nameless::register_module(db);
  vt_contraction::register_module(db);
  vt_dijkstra_shortest_paths::register_module(db);

  fx_toset::register_function(db);
  fx_toset_agg::register_function(db);
  fx_toset_union::register_function(db);
  fx_toset_except::register_function(db);
  fx_toset_contains::register_function(db);
  fx_toset_intersection::register_function(db);

  fx_sha1::register_function(db);

  fx_ordered_concat_agg::register_function(db);

  {
    db.execute_script(R"(

      BEGIN TRANSACTION;

      CREATE TABLE t(a INT);
      INSERT INTO t SELECT 1;
      INSERT INTO t SELECT 2;

    )");

    auto reader = db.prepare("SELECT * FROM t").execute();
    db.prepare("INSERT INTO t SELECT 0").execute();
    auto writer = db.prepare("DELETE FROM t WHERE a < 2").execute();

    for (auto&& row : reader) {
      for (auto&& e : row) {
        std::clog << e.to_variant() << ' ';
      }
      std::clog << '\n';
    }

    db.execute_script("ROLLBACK");
  }

#if 1

  {
    auto db = fl::db("MODERN.sqlite");

    vt_dominator_tree::register_module(db);
    vt_stmt::register_module(db);
    vt_partition_by::register_module(db);
    vt_strong_components::register_module(db);
    vt_weak_components::register_module(db);
    vt_transitive_closure::register_module(db);
    vt_nameless::register_module(db);
    vt_contraction::register_module(db);
    vt_dijkstra_shortest_paths::register_module(db);

    fx_toset_agg::register_function(db);
    fx_toset_union::register_function(db);
    fx_toset_contains::register_function(db);
    fx_toset_intersection::register_function(db);

    fx_sha1::register_function(db);
    fx_counter::register_function(db);

    db.execute_script(R"(
drop table if exists d;
create virtual table d using fl_dijkstra_shortest_paths(vertices=(select vertex from vp), edges=(select src, dst, cast(is_term as real) AS weight from grammar_edge inner join vp on vp.vertex = src))

    )");

    auto s = db.prepare("select * from d(8710)");
    s.execute();
    for (auto&& row : s) {
      for (auto&& e : row) {
        std::clog << e.to_variant() << ' ';
      }
      std::clog << '\n';
    }
  }

#endif

  {
    auto resultx = db.select_scalar("SELECT fl_sha1('Hello')");

    std::cerr << resultx << std::endl;
    return 0;
  }

  {

    db.execute_script(R"(
      CREATE VIRTUAL TABLE d USING fl_stmt(( select * from pragma_function_list where name like :name ),x=1,y=2,z=([]))
  )");

    auto s = db.prepare("SELECT * FROM d('fl_%') ");

    s.execute();

    for (auto row : s) {
      for (auto f : row) {
        std::clog << f.to_variant() << ' ';
      }
      std::clog << std::endl;
    }
  }

  {
    auto resultx =
      db.select_scalar("SELECT fl_toset_union('[1,2,3,4]', '[2,5]')");

    std::cerr << resultx << std::endl;
    return 0;
  }

  {

    auto s = db.prepare(R"SQL(

      WITH
      t(cid, name, type, "notnull", dflt_value, pk, schema) AS (
        SELECT c1, c2, c3, c4, c5, c6, c7 FROM nameless(
          'select * from pragma_table_xinfo(:p1)', 'sqlite_schema'
        )
      )
      SELECT
        *
      FROM
        t
      WHERE
        fl_toset_contains('[1,2,3]', 2)

    )SQL");

    s.execute();

    for (auto row : s) {
      for (auto f : row) {
        std::clog << f.to_variant() << ' ';
      }
      std::clog << std::endl;
    }

    return 0;
  }

  {
    db.execute_script(R"SQL(

      CREATE TABLE e(src, dst);
      INSERT INTO e VALUES(1,2);
      INSERT INTO e VALUES(2,3);
      INSERT INTO e VALUES(3,4);
      INSERT INTO e VALUES(4,3);
      INSERT INTO e VALUES(4,5);
      INSERT INTO e VALUES(5,6);

      CREATE VIRTUAL TABLE sc USING dominator_tree(
        edges=(SELECT src, dst FROM e)
      );

    )SQL");

    auto s = db.prepare("SELECT * FROM sc WHERE root = 1 AND root IS NOT NULL");
    s.execute();

    for (auto row : s) {
      for (auto f : row) {
        std::clog << f.to_variant() << ' ';
      }
      std::clog << std::endl;
    }

    return 0;
  }

  {
    auto stmt = db.prepare(R"SQL(

        CREATE VIRTUAL TABLE vt_xxx USING partition_by(
          elements=(
            WITH RECURSIVE base AS (
              SELECT 1 AS value
              UNION ALL
              SELECT value + 1 FROM base WHERE value < 20
            )
            SELECT value FROM base
          ),
          once_by=(SELECT element, element % 3 FROM vt_xxx),
          then_by=(SELECT element, element % 7 FROM vt_xxx),
        )
      )SQL");

    stmt.execute();

    auto s = db.prepare("SELECT * FROM vt_xxx WHERE history = FALSE");

    s.execute();

    for (auto row : s) {
      for (auto f : row) {
        std::clog << f.to_variant() << ' ';
      }
      std::clog << std::endl;
    }
  }

  {
    db.prepare(
        "CREATE TABLE e AS SELECT * FROM (WITH a(src, dst) AS (SELECT 1, 2 "
        "union all select 1,3 union all select 2,4 union all select 4,5) "
        "select * from a)")
      .execute();

    db.prepare("CREATE VIRTUAL TABLE v USING dominator_tree(vertices=(SELECT "
               "null where 1=0), edges=(select src, dst from e))")
      .execute();

    auto s = db.prepare("SELECT * FROM v JOIN e ON e.src = v.root");

    s.execute();

    for (auto row : s) {
      for (auto f : row) {
        std::clog << f.to_variant() << ' ';
      }
      std::clog << std::endl;
    }
  }

  auto resultx =
    db.select_scalar("SELECT fl_toset_union('[1,2,3,4]', '[2,5]')");

  std::cerr << resultx << std::endl;

  {
    auto stmt =
      db.prepare("SELECT * FROM pragma_function_list WHERE name LIKE 'fl%'");
    stmt.execute();
    for (auto&& row : stmt) {
      for (auto&& field : row) {
        std::cout << field.to_variant() << ' ';
      }
      std::cout << '\n';
    }
  }

  return 0;

  int rc = SQLITE_ERROR;

  auto s2 = db.prepare(R"(
      CREATE VIRTUAL TABLE d USING stmt(( select * from pragma_function_list where name like :name ),x=1,y=2,z=([]))
    )");

  s2.execute();

  auto s4 = db.prepare(R"(
      SELECT * FROM d('s%')
    )");
  s4.execute();

  for (auto&& row : s4) {
    for (auto&& field : row) {
      std::cout << field.to_variant() << ' ';
    }
    std::cout << std::endl;
  }

#if 0
    auto s3 = db.prepare(R"(
      SELECT * FROM pragma_table_list
    )");

    s3.execute();

    std::vector<int> a(3);
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    for (auto&& element : a | std::ranges::views::reverse) {
      std::cout << element << std::endl;
    }

    struct table_list_row
    {
      fl::value::variant schema;
      std::string name;
      std::string type;
      int64_t ncol;
      int64_t wr;
      fl::value::variant strict;
    };

    for (auto&& row : s3 | fl::as<table_list_row>()) {
      // int(*name)(fl::row row) = xx;
      // std::cerr << name(row) << std::endl;
      // std::cerr << row.schema << std::endl;

      boost::pfr::for_each_field(row, [](auto&& value) {
        // std::cout << value << ' ';
      });

      std::cout << std::endl;
    }

    for (auto&& e : fl::pragma::table_xinfo(db, "sqlite_schema")) {
      std::cout << e.cid << std::endl;
    }
}
#endif

  return 0;
}

/*

SQLite allows specifying collations in various places, like

```sql
SELECT ... GROUP BY a COLLATE '...'
SELECT ... ORDER BY a COLLATE '...'
SELECT ... WHERE a = '...' COLLATE '...'
```

As of SQLite 3.38.1 it seems that virtual table implementations will
not see the relevant columns in `sqlite3_index_info::aOrderBy` if the
collation is anything other than `BINARY` (which is good, as there is
no way to find out which collation is in effect). The documentation
unfortunately does not discuss this anywhere.

(Aside: what is the effect of seeting `orderByConsumed` when `aOrderBy`
is empty?)

On the other hand, `sqlite3_index_info::aConstraint` will very much
be populated and implementations can use `sqlite3_vtab_collation` to
retrieve the name of the collation for any constraint, but there is
no way to use the specified collation, other than guessing what the
name corresponds to and having a secondary implementation of it, or
executing some secondary SQL that uses that collation.

Another special case is the `IN` operator. SQLite hides that from the
virtual table implementation, giving it `EQ` constraints instead. So,

```sql
SELECT ... FROM ... WHERE a IN ( 'A' COLLATE 'nocase', 'a' COLLATE 'rtrim' )
```

Here SQLite would claim the collation for the `EQ` constraint on `à`
is `BINARY` (or whatever default collation has been declared), which
is inconsistent with the behavior for

```sql
SELECT ... FROM ... WHERE a = 'A' COLLATE 'nocase' OR a = 'a' COLLATE 'rtrim'
```

The recently added `sqlite3_vtab_in` mechanism also does not allow to
access the collation for the individual terms in the constraint.

One problem here is that unsuspecting virtual table implementers may
set `sqlite3_index_constraint_usage.omit` as an  optimisation, and that
then breaks existing code relying SQLite interpreting the `COLLATE`
semantics, which presumably it ignores when `omit` is set.


*/

#if 0

#endif

// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2325r3.html

// static_assert(std::sentinel_for<fl::stmt_iterator::sentinel,
// fl::stmt_iterator>);

#if 0

TODO:

  * fl::as with blobs
  * debug why bind with name does not work
  *

#endif

constexpr bool
strings_equal(char const* a, char const* b)
{
  return *a == *b && (*a == '\0' || strings_equal(a + 1, b + 1));
}

static_assert(strings_equal(__VERSION__, "11.3.0"));
static_assert(__cplusplus == 202002L);

#if 0
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic pop
#endif
