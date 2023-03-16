#include "federlieb/federlieb.hxx"
#include "vt_dfa.hxx"
#include "fx_toset.hxx"
#include "vt_json_each.hxx"

namespace fl = ::federlieb;

// TODO: would be nice to have a couple of VIEWs that simplify the most common
// JOINs 

// TODO: use NULL as via in nfatrans when NULL is passed, instead of mapping
// it through the via table.


vt_dfa::cursor::cursor(vt_dfa* vtab) : tmpdb_(":memory:") {

  fx_toset_agg::register_function(tmpdb_);
  vt_json_each::register_module(tmpdb_);

  tmpdb_.execute_script(R"SQL(

  pragma synchronous = off;
  pragma journal_mode = off;

  create table if not exists nfastate(
    id integer primary key,
    state any [blob],
    unique(state)
  );

  create table if not exists via(
    id integer primary key,
    via any [blob],
    unique(via)
  );

  create table if not exists nfatrans(
    id integer primary key,
    src int,
    via int,
    dst int,
    unique(src,via,dst),
    unique(via,src,dst)
  );

  create index if not exists idx_nfatrans_dst on nfatrans(dst);
  create unique index if not exists idx_nfatrans_id on nfatrans(id);

  create view if not exists nfapipeline as select null as src, null as via, null as dst where false;
  create trigger if not exists nfapipeline instead of insert on nfapipeline
  begin
    insert or ignore into nfastate(state) values(NEW.src);
    insert or ignore into via(via)
      select NEW.via where NEW.via is not null;
    insert or ignore into nfastate(state) values(NEW.dst);
    insert into nfatrans(src, via, dst)
    select
      (select id from nfastate where state is NEW.src),
      case
      when NEW.via is null then null
      else (select id from via where via is NEW.via)
      end,
      (select id from nfastate where state is NEW.dst);
  end;

  create table if not exists dfatrans(
    id integer primary key,
    src int,
    via int,
    dst int,
    unique(src,via,dst),
    unique(via,src,dst)
  );

  create table if not exists dfastate(
    id integer primary key,
    state any [blob],
    round int,
    unique(state)
  );

  create index if not exists idx_dfastate_round on dfastate(round);

  create view if not exists dfapipeline as select null as src, null as via, null as dst, null as round where false;
  create trigger if not exists dfapipeline instead of insert on dfapipeline
  begin
    insert or ignore into dfastate(state, round) values(NEW.dst, NEW.round);
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

  cursor->tmpdb_.prepare("begin transaction").execute();

  cursor->tmpdb_.execute_script(R"SQL(
    delete from nfastate;
    delete from dfastate;
    delete from nfatrans;
    delete from dfatrans;
    delete from via;
  )SQL");

  auto id_stmt = cursor->tmpdb_.prepare(R"SQL(

    -- with m as (select ifnull(max(id), -1) as m from dfastate)
    -- select m.m + 1 as dead_id, m.m + 2 as start_id
    -- from m

    select 0, 1

  )SQL").execute();

  sqlite3_int64 dead_id = id_stmt.current_row().at(0);
  sqlite3_int64 start_id = id_stmt.current_row().at(1);

  id_stmt.reset();

  cursor->tmpdb_.prepare(R"SQL(

    insert into nfapipeline(src, via, dst)
    select
      each.value->>'$[0]' as src,
      each.value->>'$[1]' as via,
      each.value->>'$[2]' as dst
    from
      json_each(?1) each
    union
    select
      null, null, each.value
    from
      json_each(?2) each
    union
    select
      each.value, null, null
    from
      json_each(?3) each

  )SQL").execute(nfa_transitions, no_incoming, no_outgoing);

  cursor->tmpdb_.execute_script(R"SQL(

    analyze /* vt_dfa xFilter (1) */;

  )SQL");

  cursor->tmpdb_.prepare(R"SQL(

    with rem as materialized (
      select nfatrans.id
      from json_each(?1) each
      cross join nfastate on nfastate.state = each.value
      cross join nfatrans on nfatrans.dst = nfastate.id

      union
 
      select nfatrans.id
      from json_each(?2) each
      cross join nfastate on nfastate.state = each.value
      cross join nfatrans on nfatrans.src = nfastate.id

      union

      select nfatrans.id from nfatrans where dst is null
    )
    delete from nfatrans indexed by idx_nfatrans_id where nfatrans.id in (select id from rem)

  )SQL").execute(no_incoming, no_outgoing);

  cursor->tmpdb_.prepare(R"SQL(

    -- when via is null, consider that an epsilon-transition and replace it.
    insert or ignore into nfatrans(src, via, dst)
    with recursive
    base as (
      select src, via, dst from nfatrans where via is null
      union
      select base.src, nfatrans.via, nfatrans.dst
      from base
        inner join nfatrans on base.dst = nfatrans.src 
      where
        base.via is null
    )
    select src, via, dst from base;

  )SQL").execute();

  cursor->tmpdb_.prepare(R"SQL(

    delete from nfatrans where via is null

  )SQL").execute();

  // dead state
  cursor->tmpdb_.prepare(R"SQL(

    insert into dfastate(id, state, round)
    select ?1, json_array(), 0

  )SQL").execute(dead_id);

  // start state
  cursor->tmpdb_.prepare(R"SQL(

    insert into dfastate(id, state, round)
    select ?1, fl_toset_agg(nfastate.id), 0
    from json_each(?2) each cross join nfastate on nfastate.state = each.value
    group by null

  )SQL").execute(start_id, no_incoming);

  cursor->tmpdb_.execute_script(R"SQL(

    analyze /* vt_dfa xFilter (2) */;

  )SQL");

  auto done_stmt = cursor->tmpdb_.prepare(R"SQL(

    select 1 where exists (select 1 from dfastate where round = ?1)

  )SQL");

  auto count_stmt = cursor->tmpdb_.prepare(R"SQL(

    select count(*) from dfastate

  )SQL");

  auto compute_stmt = cursor->tmpdb_.prepare(
  R"SQL(

    insert into dfapipeline(src, via, dst, round)
    select
        s.id as src,
        nfatrans.via as via,
        fl_toset_agg(nfatrans.dst) as dst,
        ?1
    from
        dfastate s
        cross join json_each(s.state) each
        cross join nfatrans on nfatrans.src = each.value
    where
        s.round = ( (?1) - 1 )
    group by
        s.id, nfatrans.via
    order by
        1, 2

  )SQL");

  cursor->tmpdb_.prepare("commit").execute();

  bool incomplete = false;

  for (sqlite3_int64 round = 1; true; ++round) {

    if (cursor->tmpdb_.is_interrupted()) {
      throw fl::error::interrupted();
    }

    // std::cout << round << "... " << '\n';

    cursor->tmpdb_.prepare("begin transaction").execute();
    compute_stmt.reset().execute(round);

    done_stmt.reset().execute(round);
    cursor->tmpdb_.prepare("commit").execute();

    if (done_stmt.empty()) {
      break;
    }

    done_stmt.reset();

    count_stmt.reset().execute();
    
    auto count = count_stmt.current_row().at(0).to_integer();

    count_stmt.reset();

    if (state_limit_int >= 0 && count >= state_limit_int) {
      incomplete = true;
      break;
    }

  }

  if (fill_int) {

    cursor->tmpdb_.prepare(R"SQL(

      with base as (
        select
          s.id, via.id, 0
        from
          dfastate s join via left join dfatrans t on t.src = s.id and t.via = via.id
        where
          t.id is null
          and
          via.via is not null
      )
      insert into dfatrans(src, via, dst) select * from base

    )SQL").execute();
    
  }

  compute_stmt.reset();

  // std::remove("debug.sqlite");
  // cursor->tmpdb_.execute_script("VACUUM INTO 'debug.sqlite'");

  return cursor->tmpdb_.prepare(R"SQL(

    with s as (
      select
        dfastate.id,
        json_group_array(nfastate.state) as nfa_states
      from
        dfastate
          cross join json_each(dfastate.state) each
          cross join nfastate on nfastate.id = each.value
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
        group by null
      ) as dfa_states,
      (select
        json_group_array(json_array(src, via.via, dst))
       from dfatrans inner join via on via.id = dfatrans.via 
       group by null
      ) as dfa_transitions,
      ?1

  )SQL").execute(nullptr, no_incoming, no_outgoing, nfa_transitions, state_limit_int, fill_int, incomplete);

}
