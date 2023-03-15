#include "federlieb/federlieb.hxx"
#include "vt_dfa.hxx"
#include "fx_toset.hxx"
#include "vt_json_each.hxx"

namespace fl = ::federlieb;

vt_dfa::cursor::cursor(vt_dfa* vtab) {}

// TODO: would be nice to have a couple of VIEWs that simplify the most common
// JOINs 

bool
vt_dfa::xBestIndex(fl::vtab::index_info& info) {

  auto eq = info.get("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);

  if (nullptr != eq && eq->usable) {
    return false;
  }

  return true;

}

void
vt_dfa::xConnect(bool create)
{

  path_ = kwarg("db").value_or("'file:/dfa?vfs=memdb'");

  fl::error::raise_if(path_.size() < 2, "bad db path");

  // TODO: should verify single-quote use

  path_.pop_back();
  path_.erase(0, 1);

  tmpdb_ = fl::db(path_);

  fx_toset_agg::register_function(tmpdb_);
  vt_json_each::register_module(tmpdb_);

  tmpdb_.execute_script(R"SQL(

  pragma synchronous = off;
  pragma journal_mode = off;

  create table if not exists dfa(
    dfa_id integer primary key,
    start int default 1,
    dead int default 0,
    complete int default false
  );

  create table if not exists nfastate(
    dfa_id integer not null,
    id integer primary key,
    state any [blob],
    unique(dfa_id, state)
  );

  create table if not exists via(
    dfa_id integer not null,
    id integer primary key,
    via any [blob],
    unique(dfa_id, via)
  );

  create table if not exists nfatrans(
    dfa_id integer not null,
    id integer primary key,
    src int,
    via int,
    dst int,
    unique(dfa_id,src,via,dst),
    unique(dfa_id,via,src,dst)
  );

  create index if not exists idx_nfatrans_dfa_id_dst on nfatrans(dfa_id, dst);

  create view if not exists nfapipeline as select null as dfa_id, null as src, null as via, null as dst where false;
  create trigger if not exists nfapipeline instead of insert on nfapipeline
  begin
    insert or ignore into nfastate(dfa_id, state) values(NEW.dfa_id, NEW.src);
    insert or ignore into via(dfa_id, via)
      select NEW.dfa_id, NEW.via
      where NEW.via is not null
      or
      not exists (select 1 from via where via is null and dfa_id = NEW.dfa_id);
    insert or ignore into nfastate(dfa_id, state) values(NEW.dfa_id, NEW.dst);
    insert into nfatrans(dfa_id, src, via, dst)
    select
      (select NEW.dfa_id),
      (select id from nfastate where dfa_id is NEW.dfa_id and state is NEW.src),
      (select id from via      where dfa_id is NEW.dfa_id and   via is NEW.via order by id limit 1), -- TODO: why order and limit?
      (select id from nfastate where dfa_id is NEW.dfa_id and state is NEW.dst);
  end;

  create table if not exists dfatrans(
    dfa_id integer not null,
    id integer primary key,
    src int,
    via int,
    dst int,
    unique(dfa_id, src,via,dst),
    unique(dfa_id, via,src,dst)
  );

  create table if not exists dfastate(
    dfa_id integer not null,
    id integer primary key,
    state any [blob],
    round int,
    unique(dfa_id, state)
  );

  create index if not exists idx_dfastate_dfa_id_round on dfastate(dfa_id, round);

  create view if not exists dfapipeline as select null as dfa_id, null as src, null as via, null as dst, null as round where false;
  create trigger if not exists dfapipeline instead of insert on dfapipeline
  begin
    insert or ignore into dfastate(dfa_id, state, round) values(NEW.dfa_id, NEW.dst, NEW.round);
    insert into dfatrans(dfa_id, src, via, dst)
    select NEW.dfa_id, NEW.src, NEW.via, (select id from dfastate where dfa_id = NEW.dfa_id and state = NEW.dst);
  end;

  )SQL");


  declare(R"(
      CREATE TABLE fl_dfa(
        no_incoming       JSON [BLOB] HIDDEN VT_REQUIRED,
        no_outgoing       JSON [BLOB] HIDDEN VT_REQUIRED,
        nfa_transitions   JSON [BLOB] HIDDEN VT_REQUIRED,
        state_limit       INT HIDDEN,
        fill              INT HIDDEN,
        incomplete        INT,
        dfa_states        JSON [BLOB],
        dfa_transitions   JSON [BLOB],
        dfa_id            INT
      )
    )");
}

vt_dfa::result_type
vt_dfa::xFilter(const fl::vtab::index_info& info,
                                    cursor* cursor)
{

  auto& no_incoming = info.columns[1].constraints[0].current_value.value();
  auto& no_outgoing = info.columns[2].constraints[0].current_value.value();
  auto& nfa_transitions = info.columns[3].constraints[0].current_value.value();
  // auto& state_limit = info.columns[4].constraints[0].current_value;

  auto state_limit = info.get("state_limit", SQLITE_INDEX_CONSTRAINT_EQ);
  auto fill = info.get("fill", SQLITE_INDEX_CONSTRAINT_EQ);

  sqlite3_int64 state_limit_int =
    nullptr != state_limit ? std::get<fl::value::integer>(state_limit->current_value.value()).value : -1;

  sqlite3_int64 fill_int =
    nullptr != fill ? std::get<fl::value::integer>(fill->current_value.value()).value : 0;

  tmpdb_.prepare("begin transaction").execute();

  auto id_stmt = tmpdb_.prepare(R"SQL(

    with m as (select ifnull(max(id), -1) as m from dfastate)
    select m.m + 1 as dead_id, m.m + 2 as start_id
    from m

  )SQL").execute();

  sqlite3_int64 dead_id = id_stmt.current_row().at(0);
  sqlite3_int64 start_id = id_stmt.current_row().at(1);

  id_stmt.reset();

  auto dfa_stmt = tmpdb_.prepare(R"SQL(

    insert into dfa(complete, start, dead)
    select false, ?1, ?2 returning dfa_id

  )SQL");

  sqlite_int64 dfa_id = dfa_stmt.execute(start_id, dead_id).current_row().at(0);
  dfa_stmt.reset();

  tmpdb_.prepare(R"SQL(

    insert into nfapipeline(dfa_id, src, via, dst)
    select
      ?1,
      each.value->>'$[0]' as src,
      each.value->>'$[1]' as via,
      each.value->>'$[2]' as dst
    from
      json_each(?2) each
    union
    select
      ?1, null, null, each.value
    from
      json_each(?3) each
    union
    select
      ?1, each.value, null, null
    from
      json_each(?4) each

  )SQL").execute(dfa_id, nfa_transitions, no_incoming, no_outgoing);

  tmpdb_.prepare(R"SQL(

    delete from nfatrans where nfatrans.id in (
      select nfatrans.id
      from json_each(?2) each
      cross join nfastate on nfastate.state = each.value and nfastate.dfa_id = ?1
      cross join nfatrans on nfatrans.dst = nfastate.id and nfatrans.dfa_id = nfastate.dfa_id

      union

      select nfatrans.id
      from json_each(?3) each
      cross join nfastate on nfastate.state = each.value and nfastate.dfa_id = ?1
      cross join nfatrans on nfatrans.src = nfastate.id and nfatrans.dfa_id = nfastate.dfa_id

      union

      select nfatrans.id from nfatrans where dfa_id = ?1 and dst is null
    )

  )SQL").execute(dfa_id, no_incoming, no_outgoing);

  tmpdb_.prepare(R"SQL(

    -- when via is null, consider that an epsilon-transition and replace it.
    insert or ignore into nfatrans(dfa_id, src, via, dst)
    with recursive
    nullvia as materialized (select id from via where dfa_id = ?1 and via is null),
    base as (
      select dfa_id, src, via, dst from nfatrans where nfatrans.dfa_id = ?1 and via in (select id from nullvia)
      union
      select base.dfa_id, base.src, nfatrans.via, nfatrans.dst
      from base
        inner join nfatrans on base.dst = nfatrans.src and nfatrans.dfa_id = base.dfa_id
      where
        base.via in (select id from nullvia)
    )
    select dfa_id, src, via, dst from base;

  )SQL").execute(dfa_id);

  tmpdb_.prepare(R"SQL(

    delete from nfatrans where via = (select id from via where via.via is null and via.dfa_id = ?1) and nfatrans.dfa_id = ?1;

  )SQL").execute(dfa_id);

  // dead state
  tmpdb_.prepare(R"SQL(

    insert into dfastate(dfa_id, id, state, round)
    select ?1, ?2, json_array(), 0

  )SQL").execute(dfa_id, dead_id);

  // start state
  tmpdb_.prepare(R"SQL(

    insert into dfastate(dfa_id, id, state, round)
    select ?1, ?2, fl_toset_agg(nfastate.id), 0
    from json_each(?3) each cross join nfastate on nfastate.state = each.value and nfastate.dfa_id = ?1

  )SQL").execute(dfa_id, start_id, no_incoming);


  if (start_id < 2) {
    tmpdb_.execute_script(R"SQL(

      analyze /* vt_dfa xFilter */;

    )SQL");
  }

  auto done_stmt = tmpdb_.prepare(R"SQL(

    select 1 where exists (select 1 from dfastate where dfa_id = ?1 and round = ?2)

  )SQL");

  auto count_stmt = tmpdb_.prepare(R"SQL(

    select count(*) from dfastate where dfa_id = ?1

  )SQL");

  auto compute_stmt = tmpdb_.prepare(
  R"SQL(

    insert into dfapipeline(dfa_id, src, via, dst, round)
    select
        s.dfa_id,
        s.id as src,
        nfatrans.via as via,
        fl_toset_agg(nfatrans.dst) as dst,
        ?2
    from
        dfastate s
        cross join json_each(s.state) each
        cross join nfatrans on nfatrans.src = each.value and nfatrans.dfa_id = s.dfa_id
    where
        s.dfa_id = ?1
        and
        s.round = ( (?2) - 1 )
    group by
        s.id, nfatrans.via
    order by
        2, 3

  )SQL");

  tmpdb_.prepare("commit").execute();

  bool incomplete = false;

  for (sqlite3_int64 round = 1; true; ++round) {

    if (tmpdb_.is_interrupted()) {
      throw fl::error::interrupted();
    }

    // std::cout << round << "... " << '\n';

    tmpdb_.prepare("begin transaction").execute();
    compute_stmt.reset().execute(dfa_id, round);

    done_stmt.reset().execute(dfa_id, round);
    tmpdb_.prepare("commit").execute();

    if (done_stmt.empty()) {
      break;
    }

    done_stmt.reset();

    count_stmt.reset().execute(dfa_id);
    
    auto count = count_stmt.current_row().at(0).to_integer();

    count_stmt.reset();

    if (state_limit_int >= 0 && count >= state_limit_int) {
      incomplete = true;
      break;
    }

  }

  if (fill_int) {

    tmpdb_.prepare(R"SQL(

      with base as (
        select
          s.dfa_id, s.id, via.id, 0
        from
          dfastate s join via left join dfatrans t on t.src = s.id and t.via = via.id
        where
          t.id is null
          and
          via.via is not null
          and
          s.dfa_id = ?1
          and
          t.dfa_id = s.dfa_id
          and
          via.dfa_id = s.dfa_id
      )
      insert into dfatrans(dfa_id, src, via, dst) select * from base

    )SQL").execute(dfa_id);
    
  }

  compute_stmt.reset();

  tmpdb_.prepare(R"SQL(

    update dfa set complete = ?2 where dfa_id = ?1

  )SQL").execute(dfa_id, !incomplete);

  std::remove("debug.sqlite");
  tmpdb_.execute_script("VACUUM INTO 'debug.sqlite'");

  return tmpdb_.prepare(R"SQL(

    with s as (
      select
        dfastate.dfa_id,
        dfastate.id,
        json_group_array(nfastate.state) as nfa_states
      from
        dfastate
          cross join json_each(dfastate.state) each
          cross join nfastate on nfastate.id = each.value and nfastate.dfa_id = dfastate.dfa_id
      where
        dfastate.dfa_id = ?1
      group by
        dfastate.id
    )
    select
      ?2 as no_incoming,
      ?3 as no_outgoing,
      ?4 as nfa_transitions,
      ?5 as state_limit,
      ?6 as fill,
      ?7 as incomplete,
      (select
        json_group_array(json_array(id, nfa_states))
        from s
        where dfa_id = ?1
        group by null
      ) as dfa_states,
      (select
        json_group_array(json_array(src, via.via, dst))
       from dfatrans inner join via on via.id = dfatrans.via and via.dfa_id = dfatrans.dfa_id where dfatrans.dfa_id = ?1
       group by null
      ) as dfa_transitions,
      ?1 -- as dfa_id,

  )SQL").execute(dfa_id, no_incoming, no_outgoing, nfa_transitions, state_limit_int, fill_int, incomplete);

}
