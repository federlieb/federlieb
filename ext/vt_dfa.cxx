#include "federlieb/federlieb.hxx"
#include "vt_dfa.hxx"
#include "fx_toset.hxx"
#include "vt_json_each.hxx"

namespace fl = ::federlieb;

vt_dfa::cursor::cursor(vt_dfa* vtab)
  : tmpdb_(":memory:")
{

  fx_toset_agg::register_function(tmpdb_);
  vt_json_each::register_module(tmpdb_);

#if 0
  auto s = tmpdb_.prepare("select * from pragma_function_list").execute();

  for (auto&& row : s) {
    for (auto&& cell : row) {
        std::cout << cell.to_variant() << ' ';
    }
    std::cout << std::endl;
  }
#endif

  tmpdb_.execute_script(R"SQL(

  create table nfastate(
    id integer primary key,
    state any [blob],
    unique(state)
  );

  create table intrans(
    src any [blob],
    via any [blob],
    dst any [blob]
  );

  create table instart(
    state any [blob]
  );

  create table nfatrans(
    id integer primary key,
    src int,
    via any [blob],
    dst int,
    unique(src,via,dst),
    unique(via,src,dst)
  );

  create table dfatrans(
    id integer primary key,
    src int,
    via any [blob],
    dst int,
    unique(src,via,dst),
    unique(via,src,dst)
  );

  create table dfastate(
    id integer primary key,
    state any [blob],
    done bool default false,
    unique(state)
  );

  create view map as select null as src, null as via, null as dst where false;

  create trigger new_dfa_states instead of insert on map
  begin
    insert or ignore into dfastate(state) values(NEW.dst);
    insert into dfatrans(src, via, dst)
    select NEW.src, NEW.via, (select id from dfastate where state = NEW.dst);
  end;


  )SQL");
}

void
vt_dfa::xConnect(bool create)
{

  declare(R"(
      CREATE TABLE fl_dfa(
        src           ANY [BLOB],
        via           ANY [BLOB],
        dst           ANY [BLOB]
      )
    )");
}

vt_dfa::result_type
vt_dfa::xFilter(const fl::vtab::index_info& info,
                                    cursor* cursor)
{

  auto transitions_stmt = db().prepare(
    fl::detail::format("SELECT * FROM {}", kwarg_or_throw("transitions")));

  transitions_stmt.execute();

  cursor
    ->tmpdb_ //
    .prepare("INSERT INTO intrans(src, via, dst) VALUES(?1, ?2, ?3)")
    .executemany(transitions_stmt);

  auto start_stmt = db().prepare(
    fl::detail::format("SELECT * FROM {}", kwarg_or_throw("start")));

  start_stmt.execute();

  cursor
    ->tmpdb_ //
    .prepare("INSERT INTO instart(state) VALUES(?1)")
    .executemany(start_stmt);

  cursor->tmpdb_.execute_script(R"SQL(
  
    insert into nfastate(state)
    select src from intrans
    union
    select dst from intrans
    ;

    with base as (
        select
            src_state.id,
            intrans.via,
            dst_state.id
        from
            intrans
                inner join nfastate src_state on intrans.src = src_state.state
                inner join nfastate dst_state on intrans.dst = dst_state.state
    )
    insert into nfatrans(src, via, dst)
    select * from base
    ;

    insert into dfastate(state)
    select fl_toset_agg(nfastate.id) from instart inner join nfastate on nfastate.state = instart.state
    ;

  )SQL");

  auto update_done_stmt = cursor->tmpdb_.prepare(R"SQL(
    update dfastate set done = true where id = ?1
  )SQL");

  auto todo_stmt = cursor->tmpdb_.prepare(R"SQL(
    select id from dfastate where done is false
  )SQL");

  auto compute_stmt = cursor->tmpdb_.prepare(
  R"SQL(

    with s as (
        select id, state from dfastate where done is false order by id limit 1
    )
    insert into map(src, via, dst)
    select
        s.id as src,
        nfatrans.via as via,
        fl_toset_agg(nfatrans.dst) as dst
    from
        s,
        fl_json_each(s.state) each
        inner join nfatrans on nfatrans.src = each.value
    group by
        s.id, nfatrans.via
    returning
        src

  )SQL");

  for (int i = 0; i < 10; ++i) {
    std::set<sqlite3_int64> done;
    compute_stmt.reset().execute();

    for (auto&& row : compute_stmt) {
        done.insert(row.at(0).to_integer());
    }

    for (auto&& elem : done) {
        update_done_stmt.reset().execute(elem);
    }

    if (todo_stmt.reset().execute().empty()) {
        break;
    }

  }

  return cursor->tmpdb_.prepare(R"SQL(

    select
        fl_toset_agg(nfa_src.state) as src,
        dfatrans.via,
        fl_toset_agg(nfa_dst.state) as dst
    from
        dfatrans
            inner join dfastate src_state on dfatrans.src = src_state.id
            inner join dfastate dst_state on dfatrans.dst = dst_state.id
            inner join fl_json_each(src_state.state) dfa_src
            inner join nfastate nfa_src on dfa_src.value = nfa_src.id
            inner join fl_json_each(dst_state.state) dfa_dst
            inner join nfastate nfa_dst on dfa_dst.value = nfa_dst.id
    group by
        dfatrans.src, dfatrans.via, dfatrans.dst

  )SQL").execute();

}
