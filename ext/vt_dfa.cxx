#include "federlieb/federlieb.hxx"
#include "vt_dfa.hxx"
#include "fx_toset.hxx"
#include "vt_json_each.hxx"

namespace fl = ::federlieb;

/*

TODO:

put nfatrans trans into a vector of (via, dst) pairs
record start and end position of nfa transitions in that vector in a map
compute dfa state by copying from the transition vector (based on nfa states)
sort that vector
remove duplicates

... at least for reasonably small transition tables that should be much faster


Also need to figure out how to get dfa state -> nfa states mappings out of
the virtual table. An idea would be to cache stuff and allow the table to be
queried multiple times, like

  select * from dfa -> dfa transitions 
  select nfastate from dfa where dfastate = :id -> mapping

that would also allow, in principle, to repeatedly query the same structure
for multiple start states (for instance).

  select * from dfa where start = json_array(1,2,3,4,5,...)

albeit this would make a bit of a weird interface, maybe. Would also require
filtering results by reachability, but that should be easy. Might then want to
allow a set of stop vertices as input?


*/

struct transition {
  uint32_t src;
  uint32_t via;
  uint32_t dst;
};

struct dfa {
  std::vector<transition> transitions;
  std::vector<std::pair<size_t, size_t>> index;

  dfa(fl::db& db) {

    auto stmt = db.prepare("SELECT src, via, dst FROM nfatrans ORDER BY 1,2,3");
    stmt.execute();

    if (stmt.empty()) {
      return;
    }

    for (auto&& e : stmt | fl::as<transition>()) {
      transitions.push_back(e);
    }

    auto max_src = std::ranges::max(transitions, {}, [](auto&& e) { return e.src; });

    index.resize(max_src.src + 1);

    auto prev = transitions.front().src;

    index[prev].first = 0;

    for (size_t ix = 0; ix < transitions.size(); ++ix) {
      auto&& e = transitions[ix];
      if (e.src != prev) {
        index[prev].second = ix;
        index[e.src].first = ix;
        prev = e.src;
      }
    }

    index[prev].second = transitions.size();

  }

  std::map<uint32_t, std::vector<uint32_t>>
  compute(fl::db& db, sqlite3_int64 id) {

    auto stmt = db.prepare(
      "SELECT value FROM dfastate, JSON_EACH(state) WHERE dfastate.id = :id");

    stmt.execute(id);

    std::vector<transition> result;

    for (auto&& row : stmt) {
      auto&& key = row.at(0).to_integer();
      // std::cout << key << ' ' << index[key].first << ' ' << index[key].second << std::endl;
      std::copy(transitions.begin() + index[key].first, transitions.begin() + index[key].second, std::back_inserter(result));
    }

    auto to_via_dst = [](auto&& e) {
      return std::make_pair(e.via, e.dst);
    };

    std::ranges::sort(result, {}, to_via_dst);
    std::ranges::unique(result, {}, to_via_dst);

    // std::vector<std::pair<uint32_t,uint32_t>> r2;
    // std::ranges::transform(result, std::back_inserter(r2), to_via_dst);

    std::map<uint32_t, std::vector<uint32_t>> r2;

    for (auto&& e : result) {
      auto&& via = r2[e.via];
      via.push_back(e.dst);
    }

    return r2;
  }


};


vt_dfa::cursor::cursor(vt_dfa* vtab)
  : tmpdb_("debug.sqlite")
{

  fx_toset_agg::register_function(tmpdb_);
  vt_json_each::register_module(tmpdb_);

  tmpdb_.execute_script(R"SQL(

  pragma synchronous = off;
  pragma journal_mode = memory;

  create table nfastate(
    id integer primary key,
    state any [blob],
    unique(state)
  );

  create table via(
    id integer primary key,
    via any [blob],
    unique(via)
  );

  create table instart(
    state any [blob]
  );

  create table nfatrans(
    id integer primary key,
    src int,
    via int,
    dst int,
    unique(src,via,dst),
    unique(via,src,dst)
  );

  create view nfapipeline as select null as src, null as via, null as dst where false;
  create trigger nfapipeline instead of insert on nfapipeline
  begin
    insert or ignore into nfastate(state) values(NEW.src);
    insert or ignore into via(via) values(NEW.via);
    insert or ignore into nfastate(state) values(NEW.dst);
    insert into nfatrans(src, via, dst)
    select
      (select id from nfastate where state = NEW.src),
      (select id from via where via is NEW.via),
      (select id from nfastate where state = NEW.dst);
  end;

  create table dfatrans(
    id integer primary key,
    src int,
    via int,
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

  create view dfapipeline as select null as src, null as via, null as dst where false;
  create trigger dfapipeline instead of insert on dfapipeline
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
  cursor->tmpdb_.prepare("BEGIN TRANSACTION").execute();

  auto transitions_stmt = db().prepare(
    fl::detail::format("SELECT * FROM {}", kwarg_or_throw("transitions")));

  transitions_stmt.execute();

  cursor
    ->tmpdb_ //
    .prepare("INSERT INTO nfapipeline(src, via, dst) VALUES(?1, ?2, ?3)")
    .executemany(transitions_stmt);

  auto start_stmt = db().prepare(
    fl::detail::format("SELECT * FROM {}", kwarg_or_throw("start")));

  start_stmt.execute();

  cursor
    ->tmpdb_ //
    .prepare("INSERT INTO instart(state) VALUES(?1)")
    .executemany(start_stmt);

  cursor->tmpdb_.execute_script(R"SQL(
  
    insert or ignore into nfatrans(src, via, dst)
    with recursive base as (
      select src, via, dst from nfatrans
      union
      select base.src, nfatrans.via, nfatrans.dst
      from base
        inner join via on via.via is null and base.via = via.id
        inner join nfatrans on base.dst = nfatrans.src 
    )
    select src, via, dst from base;

    delete from nfatrans where via = (select id from via where via.via is null);

    insert into dfastate(state)
    select fl_toset_agg(nfastate.id)
    from instart inner join nfastate on nfastate.state = instart.state
    ;

    analyze;

  )SQL");

  auto update_done_stmt = cursor->tmpdb_.prepare(R"SQL(
    update dfastate set done = true where id = ?1
  )SQL");

  auto todo_stmt = cursor->tmpdb_.prepare(R"SQL(
    select id from dfastate where done is false order by id limit 1
  )SQL");

  auto insert_dfa_trans_stmt = cursor->tmpdb_.prepare(
  R"SQL(
    insert into dfapipeline(src, via, dst) values(?1, ?2, ?3)
  )SQL");

  auto compute_stmt = cursor->tmpdb_.prepare(
  R"SQL(
    insert into dfapipeline(src, via, dst)
    select
        s.id as src,
        nfatrans.via as via,
        fl_toset_agg(nfatrans.dst) as dst
    from
        dfastate s,
        fl_json_each(s.state) each
        inner join nfatrans on nfatrans.src = each.value
    where
        s.id = :id
    group by
        s.id, nfatrans.via

  )SQL");

  cursor->tmpdb_.prepare("COMMIT").execute();

  dfa automaton(cursor->tmpdb_);

  cursor->tmpdb_.prepare("BEGIN TRANSACTION").execute();
  int i = 0;
  while (true) {

    std::cout << i++ << '\n';

    todo_stmt.reset().execute();

    if (todo_stmt.empty()) {
      break;
    }

    auto id = todo_stmt.current_row().at(0).to_integer();

    // compute_stmt.reset().execute(id);

    auto result = automaton.compute(cursor->tmpdb_, id);

    for (auto&& e : result) {
      insert_dfa_trans_stmt.reset().execute(
        id,
        e.first,
        boost::json::serialize(boost::json::value_from(e.second))
      );
    }

    // std::cout << boost::json::serialize(boost::json::value_from(result)) << std::endl;

    update_done_stmt.reset().execute(id);

  }
  cursor->tmpdb_.prepare("COMMIT").execute();

  return cursor->tmpdb_.prepare(R"SQL(
    select src, via, dst from dfatrans
  )SQL").execute();

  return cursor->tmpdb_.prepare(R"SQL(

    select
        fl_toset_agg(nfa_src.state) as src,
        dfatrans.via,
        fl_toset_agg(nfa_dst.state) as dst
    from
        dfatrans
            cross join dfastate src_state on dfatrans.src = src_state.id
            cross join dfastate dst_state on dfatrans.dst = dst_state.id
            cross join fl_json_each(src_state.state) dfa_src
            cross join nfastate nfa_src on dfa_src.value = nfa_src.id
            cross join fl_json_each(dst_state.state) dfa_dst
            cross join nfastate nfa_dst on dfa_dst.value = nfa_dst.id
    group by
        dfatrans.src, dfatrans.via, dfatrans.dst

  )SQL").execute();

}
