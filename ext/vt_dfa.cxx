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
    insert or ignore into via(via)
      select NEW.via
      where NEW.via is not null
      or
      not exists (select 1 from via where via is null);
    insert or ignore into nfastate(state) values(NEW.dst);
    insert into nfatrans(src, via, dst)
    select
      (select id from nfastate where state is NEW.src),
      (select id from via where via is NEW.via order by id limit 1),
      (select id from nfastate where state is NEW.dst);
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
    round int,
    unique(state)
  );

  create index idx_dfastate_round on dfastate(round);

  -- not used at the moment
  create view dfapipeline as select null as src, null as via, null as dst, null as round where false;
  create trigger dfapipeline instead of insert on dfapipeline
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
        incomplete        INT,
        dfa_states        JSON [BLOB],
        dfa_transitions   JSON [BLOB]
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

  sqlite3_int64 state_limit_int =
    nullptr != state_limit ? std::get<fl::value::integer>(state_limit->current_value.value()).value : -1;

  cursor->tmpdb_.prepare("BEGIN TRANSACTION").execute();

  // FIXME: Not sure this is needed...
  cursor->tmpdb_.execute_script(R"SQL(
    delete from nfastate;
    delete from dfastate;
    delete from nfatrans;
    delete from dfatrans;
    delete from instart;
    delete from via;
  )SQL");


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

  cursor->tmpdb_.prepare(R"SQL(

    delete from nfatrans where dst in (
      select
        nfastate.id
      from
        fl_json_each(?1) each inner join nfastate on nfastate.state = each.value
    )

  )SQL").execute(no_incoming);

  cursor->tmpdb_.prepare(R"SQL(

    delete from nfatrans where src in (
      select
        nfastate.id
      from
        fl_json_each(?1) each inner join nfastate on nfastate.state = each.value
    )

  )SQL").execute(no_outgoing);

  cursor->tmpdb_.prepare(R"SQL(

    insert into instart(state)
    select
      each.value
    from
      json_each(?1) each

  )SQL").execute(no_incoming);

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

    insert into dfastate(state, round)
    select fl_toset_agg(nfastate.id), 0
    from instart inner join nfastate on nfastate.state = instart.state
    ;

    analyze;

  )SQL");

  auto done_stmt = cursor->tmpdb_.prepare(R"SQL(

    select 1 where exists (select 1 from dfastate where round = :round)

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
        :round
    from
        dfastate s
        cross join json_each(s.state) each
        cross join nfatrans on nfatrans.src = each.value
    where
        s.round = :round - 1
    group by
        s.id, nfatrans.via
    order by
        1, 2

  )SQL");

  cursor->tmpdb_.prepare("commit").execute();

  bool incomplete = false;

  for (sqlite3_int64 round = 1; true; ++round) {

    // std::cout << round << "... " << '\n';

    cursor->tmpdb_.prepare("begin transaction").execute();
    compute_stmt.reset().execute(round);

    done_stmt.reset().execute(round);
    cursor->tmpdb_.prepare("commit").execute();

    if (done_stmt.empty()) {
      break;
    }

    auto count = count_stmt.reset().execute().current_row().at(0).to_integer();

    if (state_limit_int >= 0 && count >= state_limit_int) {
      incomplete = true;
      break;
    }

  }

  count_stmt.reset();
  compute_stmt.reset();
  done_stmt.reset();

  std::remove("debug.sqlite");
  cursor->tmpdb_.execute_script("VACUUM INTO 'debug.sqlite'");

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
      ?1 as no_incoming,
      ?2 as no_outgoing,
      ?3 as nfa_transitions,
      ?4 as state_limit,
      ?5 as incomplete,
      (select
        json_group_array(json_array(id, nfa_states))
        from s
        group by null
      ) as dfa_states,
      (select
        json_group_array(json_array(src, via.via, dst))
       from dfatrans inner join via on via.id = dfatrans.via
       group by null
      ) as dfa_transitions

  )SQL").execute(no_incoming, no_outgoing, nfa_transitions, state_limit_int, incomplete);

}
