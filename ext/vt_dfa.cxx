#include <unordered_map>
#include <boost/unordered_map.hpp>

#include "federlieb/federlieb.hxx"
#include "vt_dfa.hxx"
#include "fx_toset.hxx"
#include "vt_json_each.hxx"

namespace fl = ::federlieb;

static std::mutex g_mutex;

// TODO: there is still a bug here where the helper generates transitions for
// via=0 and dst=0.

template<typename T, typename E>
void insert_at(T& vec, size_t pos, E element) {
  if (vec.size() <= pos) {
    vec.resize( (pos + 1) * 2 );
  }
  vec[pos] = element;
}

struct helper {

    using nfastate = uint32_t;
    using dfastate = uint32_t;
    using via = uint32_t;
    using viadst = uint64_t;

    helper(fl::db& db) {

      struct posviadst {
        size_t pos;
        via via_;
        nfastate dst;
      };

      auto trans_stmt = db.prepare(R"SQL(

        select
          row_number() over (order by src, via, dst),
          ifnull(via, 0),
          ifnull(dst, 0)
        from
          nfastate left join nfatrans on nfastate.id = nfatrans.src
        order by
          src, via, dst

      )SQL");

      trans_stmt.execute();

      for (auto&& row : trans_stmt | fl::as<posviadst>()) {
        viadst current_via = row.via_;
        viadst current_dst = row.dst;
        current_via <<= 32;
        insert_at(trans_, row.pos, current_via | current_dst);
      }

      struct minmax {
        nfastate vertex;
        size_t min;
        size_t max;
      };

      auto minmax_stmt = db.prepare(R"SQL(

        with base as (
          select
            nfastate.id, row_number() over (order by src, via, dst) as pos
          from
            nfastate left join nfatrans on nfastate.id = nfatrans.src
        )
        select
          id,
          ifnull(min(pos), 1),
          ifnull(max(pos), 0)
        from
          base
        group by
          id

      )SQL");

      minmax_stmt.execute();

      for (auto&& row : minmax_stmt | fl::as<minmax>()) {
        insert_at(min_, row.vertex, row.min);
        insert_at(max_, row.vertex, row.max);
      }

      insert_state_stmt_ = db.prepare(R"SQL(

        insert or ignore into dfastate(state, round) values(?1, ?2)

      )SQL");

      insert_trans_stmt_ = db.prepare(R"SQL(

        insert into dfatrans(src, via, dst) values(?1, ?2, ?3)

      )SQL");

      state_to_id_stmt_ = db.prepare(R"SQL(

        select id from dfastate where state = ?1

      )SQL");

    }

    void done_via(dfastate src, via v, int32_t round, fl::stmt insert_stmt) {

        if (!destinations_.empty()) {

          std::ranges::sort(destinations_);

          destinations_.erase(
            std::unique(destinations_.begin(), destinations_.end()),
            destinations_.end());

          if (vec2id_.contains(destinations_)) {

            insert_trans_stmt_.reset().execute(src, v, vec2id_[destinations_]);

          } else {

            auto state = boost::json::serialize(
              boost::json::value_from(destinations_)
            );

            insert_state_stmt_.reset().execute(state, round);

            state_to_id_stmt_.reset().bind(1, state).execute();

            fl::error::raise_if(state_to_id_stmt_.empty(), "impossible");

            int64_t id = state_to_id_stmt_.current_row().at(0).to_integer();
            state_to_id_stmt_.reset();

            vec2id_[destinations_] = id;

            insert_trans_stmt_.reset().execute(src, v, id);

          }

        }
        
    }

    void done_state(dfastate src, int32_t round, fl::stmt insert_stmt) {

        std::ranges::sort(transitions_);

        // NOTE: this assumes that via.id is never zero.
        via prev_via = 0;

        for (auto&& val : transitions_) {
            std::pair<via, nfastate> vd = std::make_pair(val >> 32, val & 0xFFFFFFFFLU);
            if (prev_via != vd.first) {
                done_via(src, prev_via, round, insert_stmt);
                destinations_.clear();
                prev_via = vd.first;
            }
            destinations_.push_back(vd.second);
        }

        done_via(src, prev_via, round, insert_stmt);
        destinations_.clear();
    }

    void accumulate(int32_t round, fl::stmt select_stmt, fl::stmt insert_stmt) {

        struct dfanfa {
          dfastate d;
          nfastate n;
        };

        select_stmt.reset().execute();

        // NOTE: this assumes that nfastate.id is never zero.
        dfastate prev_src = 0;
        
        for (auto&& row : select_stmt | fl::as<dfanfa>()) {

            if (prev_src != row.d) {
                done_state(prev_src, round, insert_stmt);
                prev_src = row.d;
                transitions_.clear();
            }

            transitions_.insert(transitions_.end(), &trans_[min_[row.n]], &trans_[1 + max_[row.n]]);
        }

        done_state(prev_src, round, insert_stmt);
        transitions_.clear();
    }

    std::vector<viadst> transitions_;
    std::vector<nfastate> destinations_; 
    std::vector<viadst> trans_;
    std::vector<size_t> min_;
    std::vector<size_t> max_;
    fl::stmt insert_state_stmt_;
    fl::stmt insert_trans_stmt_;
    fl::stmt state_to_id_stmt_;

    // NOTE: This is just a cache, it could be cleared anytime to save memory.
    boost::unordered_map< std::vector<nfastate>, int64_t > vec2id_;
};

vt_dfa::cursor::cursor(vt_dfa* vtab) : tmpdb_(":memory:") {

  fx_toset_agg::register_function(tmpdb_);
  vt_json_each::register_module(tmpdb_);

  tmpdb_.execute_script(R"SQL(

  pragma synchronous = off;
  pragma journal_mode = off;

  create table if not exists dfa(
    complete int not null default false,
    state_limit int default -1,
    fill int not null default false,
    start_state int not null,
    dead_state int not null,
    no_incoming any [blob],
    no_outgoing any [blob]
  );

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

  create table if not exists dfastate(
    id integer primary key,
    state any [blob],
    round int,
    unique(state)
  );

  create table if not exists dfatrans(
    id integer primary key,
    src int,
    via int,
    dst int
    -- This is true, but no need to make SQLite check for it
    -- , unique(src,via,dst)
    -- , unique(via,src,dst)
  );

  create index if not exists idx_dfastate_round on dfastate(round);

  create view if not exists dfapipeline as select null as src, null as via, null as dst, null as round where false;
  create trigger if not exists dfapipeline instead of insert on dfapipeline
  begin
    insert or ignore into dfastate(state, round) values(NEW.dst, NEW.round);
    insert into dfatrans(src, via, dst)
      select NEW.src, NEW.via, (select id from dfastate where state = NEW.dst);
  end;

  create view dfatrans_via as
  select
    dfatrans.src, via.via, dfatrans.dst
  from
    dfatrans inner join via on via.id = dfatrans.via
  ;

  create view nfatrans_via as
  select
    nfatrans.src, via.via, nfatrans.dst
  from
    nfatrans inner join via on via.id = nfatrans.via
  ;

  create view dfastate_nfa as
  select
    dfastate.id as dfa_state_id,
    dfastate.state as dfa_state,
    nfastate.id as nfa_state_id,
    nfastate.state as nfa_state
  from
    dfastate
      inner join json_each(dfastate.state) each
      inner join nfastate on nfastate.id = each.value
  ;

  )SQL");

}

void
vt_dfa::xDisconnect(bool destroy) {

  if (destroy) {

    for (auto&& shadow_table : shadow_tables) {

      auto name = shadow_table.first;

      db().execute_script(fl::detail::format(R"SQL(

        drop table if exists {}.{};

        )SQL",
        fl::detail::quote_identifier(schema_name_),
        fl::detail::quote_identifier(table_name_ + "_" + name)
        )
      );

    }

  }

  const std::lock_guard<std::mutex> lock(g_mutex);

  if (memory_) {
    memory_->rc--;

    if (memory_->rc <= 0) {
      delete memory_;
    }
  }

}

void
vt_dfa::xConnect(bool create)
{

  declare(R"(
      CREATE TABLE fl_dfa(
        no_incoming       JSON [BLOB] HIDDEN VT_REQUIRED,
        no_outgoing       JSON [BLOB] HIDDEN VT_REQUIRED,
        nfa_transitions   JSON [BLOB] HIDDEN VT_REQUIRED,
        alphabet          JSON [BLOB] HIDDEN,
        state_limit       INT HIDDEN,
        fill              INT HIDDEN,
        incomplete        INT,
        start_state       INT,
        dfa_id            INT,
        state_count       INT
      )
    )");

  memory_ = new ref_counted_memory;

  for (auto&& shadow_table : shadow_tables) {

    auto name = shadow_table.first;

    db().execute_script(fl::detail::format(R"SQL(

      create virtual table if not exists {}.{} using {}({});

      )SQL", 
      fl::detail::quote_identifier(schema_name_),
      fl::detail::quote_identifier(table_name_ + "_" + name),
      shadow_table.second,
      name
    ));

    db()
      .prepare(fl::detail::format(R"SQL(

        select 1 from {}.{} where ptr is :ptr and dfa_id = -1

        )SQL",
        fl::detail::quote_identifier(schema_name_),
        fl::detail::quote_identifier(table_name_ + "_" + name)

      ))
      .bind_pointer(":ptr", "vt_dfa_view:ptr", static_cast<void*>(memory_))
      .execute();

  }

}

vt_dfa::result_type
vt_dfa::xFilter(const fl::vtab::index_info& info,
                                    cursor* cursor)
{

  auto& no_incoming = info.columns[1].constraints[0].current_value.value();
  auto& no_outgoing = info.columns[2].constraints[0].current_value.value();
  auto& nfa_transitions = info.columns[3].constraints[0].current_value.value();
  // auto& state_limit = info.columns[4].constraints[0].current_value;

  // TODO: make no_outgoing default to empty json array and make it optional

  auto state_limit = info.get("state_limit", SQLITE_INDEX_CONSTRAINT_EQ);
  auto fill = info.get("fill", SQLITE_INDEX_CONSTRAINT_EQ);
  auto alphabet = info.get("alphabet", SQLITE_INDEX_CONSTRAINT_EQ);

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
    delete from dfa;

  )SQL");

  if (alphabet) {
    cursor->tmpdb_.prepare(R"SQL(
      
      insert or ignore into via(via) select value from json_each(?1)

    )SQL").execute(alphabet->current_value.value());
  }

  sqlite3_int64 dead_id = 2;
  sqlite3_int64 start_id = 1;

  cursor->tmpdb_.prepare(R"SQL(

    insert or ignore into nfastate(state)
    select
      each.value
    from
      json_each(?1) each
    union
    select
      each.value
    from
      json_each(?2) each

  )SQL").execute(no_incoming, no_outgoing);

  cursor->tmpdb_.prepare(R"SQL(

    insert into nfapipeline(src, via, dst)
    select
      each.value->>'$[0]' as src,
      each.value->>'$[1]' as via,
      each.value->>'$[2]' as dst
    from
      json_each(?1) each

  )SQL").execute(nfa_transitions);

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
        inner join nfatrans on base.via is null and base.dst = nfatrans.src
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

    with base as materialized (
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
    )
    -- insert into dfapipeline(src, via, dst, round)
    select * from base order by 1, 2

  )SQL");

  auto helper_stmt = cursor->tmpdb_.prepare(
  R"SQL(

    select
        s.id as src,
        each.value
    from
        dfastate s
        cross join json_each(s.state) each
    where
        s.round = ( (?1) - 1 )
    order by
        s.id

  )SQL");


  auto insert_stmt = cursor->tmpdb_.prepare(R"SQL(

    insert into dfapipeline(src, via, dst, round) values(?1,?2,?3,?4)

  )SQL");

  cursor->tmpdb_.prepare("commit").execute();

  helper h(cursor->tmpdb_);

  bool incomplete = false;

  for (sqlite3_int64 round = 1; true; ++round) {

    if (cursor->tmpdb_.is_interrupted()) {
      throw fl::error::interrupted();
    }

    // std::cout << round << "... " << '\n';

    cursor->tmpdb_.prepare("begin transaction").execute();

    // compute_stmt.reset().execute(round);

    helper_stmt.reset().bind(1, round);

    h.accumulate(round, helper_stmt, insert_stmt);

    // insert_stmt.reset().executemany(compute_stmt);

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
          s.id, via.id, 2 -- dead_state
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

  sqlite3_int64 dfa_id = -1;

  if (true) {

    const std::lock_guard<std::mutex> lock(g_mutex);

    dfa_id = 1 + fl::detail::safe_to<sqlite_int64>( memory_->dfas.size() );

    cursor->tmpdb_.prepare(R"SQL(

      insert into dfa(
        complete, state_limit, fill, start_state, dead_state, no_incoming, no_outgoing
      ) values(
        :complete, :state_limit, :fill, :start_state, :dead_state, :no_incoming, :no_outgoing
      )

    )SQL").execute(
      !incomplete, state_limit_int, fill_int, start_id, dead_id, no_incoming, no_outgoing
    );

    cursor->tmpdb_.execute_script("analyze");
    memory_->dfas.push_back(cursor->tmpdb_.serialize("main"));

  }

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
      ?9 as alphabet,
      ?5 as state_limit,
      ?6 as fill,
      ?7 as incomplete,
      ?8 as start_state,
      ?1,
      (select count(*) from dfastate) as state_count

  )SQL").execute(dfa_id, no_incoming, no_outgoing, nfa_transitions,
    state_limit_int, fill_int, incomplete, start_id,
    alphabet ? alphabet->current_value.value() : fl::value::null{});

}

vt_dfa_view::cursor::cursor(vt_dfa_view* vtab) {
}

bool
vt_dfa_view::xBestIndex(fl::vtab::index_info& info)
{
  info.mark_wanted("ptr", SQLITE_INDEX_CONSTRAINT_IS);
  info.mark_wanted("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);

  auto&& ptr = info.get("ptr", SQLITE_INDEX_CONSTRAINT_IS);
  auto&& dfa_id = info.get("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);

  if (!ptr && !dfa_id) {
    return false;
  }

  // TODO: provide row count estimates

  return true;
}


void
vt_dfa_view::xDisconnect(bool destroy) {

  const std::lock_guard<std::mutex> lock(g_mutex);

  if (memory_) {
    memory_->rc--;

    if (memory_->rc <= 0) {
      delete memory_;
    }
  }

}

void
vt_dfa_view::xConnect(bool create)
{

  vt_dfa::cursor dfa_cursor(nullptr);

  std::stringstream ss;

  fl::error::raise_if(arguments().size() != 1, "fl_dfa_view needs exactly 1 argument");

  auto which = arguments().front();

  ss << "create table fl_dfa_view(\n";
  ss << "dfa_id INT HIDDEN,\n";
  ss << "ptr INT HIDDEN,\n";

  for (auto&& row : fl::pragma::table_xinfo(dfa_cursor.tmpdb_, which)) {
    ss << fl::detail::quote_identifier(row.name);
    ss << ",\n";
  }

  ss.seekp(-2, ss.cur);
  ss << "\n)";

  declare(ss.str());
}

vt_dfa::ref_counted_memory*
recall(std::shared_ptr<sqlite3> db, const fl::vtab::constraint_info* ptr_is) {


    auto ptr = fl::api(sqlite3_value_pointer,
                       db.get(),
                       ptr_is->current_raw,
                       "vt_dfa_view:ptr");

    fl::error::raise_if(nullptr == ptr, "invalid pointer");

    return static_cast<vt_dfa::ref_counted_memory*>(ptr);
}

fl::stmt
vt_dfa_view::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{

  auto ptr_is = info.get("ptr", SQLITE_INDEX_CONSTRAINT_IS);

  if (nullptr != ptr_is) {

    const std::lock_guard<std::mutex> lock(g_mutex);

    memory_ = recall(db_, ptr_is);
    memory_->rc++;

    // NOTE: needs to cover worst case number of columns
    return db().prepare("select null, null, null, null, null, null, null where false").execute();
  }

  // TODO: Could be nice and support unconstrained dfa_id.

  auto dfa_id_eq = info.get("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);
  auto dfa_id = std::get<fl::value::integer>(dfa_id_eq->current_value.value());

  fl::error::raise_if(nullptr == memory_, "not initialized");

  fl::error::raise_if(
    dfa_id.value < 1 || fl::detail::safe_to<size_t>(dfa_id.value - 1) >= memory_->dfas.size(),
    "no such dfa");

  fl::db db(":memory:");
  db.deserialize("main", memory_->dfas[dfa_id.value - 1], SQLITE_DESERIALIZE_READONLY);

  auto query = "select ?1, null, * from " + fl::detail::quote_identifier(arguments().front());

  // TODO: could add constraints

  auto stmt = db
    .prepare(query)
    .execute(dfa_id.value);

  return stmt;
}

vt_dfastate_subset::cursor::cursor(vt_dfastate_subset* vtab) {
}

bool
vt_dfastate_subset::xBestIndex(fl::vtab::index_info& info)
{
  info.mark_wanted("ptr", SQLITE_INDEX_CONSTRAINT_IS);
  info.mark_wanted("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);

  info.mark_wanted("by_ids", SQLITE_INDEX_CONSTRAINT_EQ);
  info.mark_wanted("by_states", SQLITE_INDEX_CONSTRAINT_EQ);

  auto&& ptr = info.get("ptr", SQLITE_INDEX_CONSTRAINT_IS);
  auto&& dfa_id = info.get("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);

  if (!ptr && !dfa_id) {
    return false;
  }

  auto&& by_ids = info.get("by_ids", SQLITE_INDEX_CONSTRAINT_EQ);
  auto&& by_states = info.get("by_states", SQLITE_INDEX_CONSTRAINT_EQ);

//  if (dfa_id && !(by_ids || by_states)) {
//    return false;
//  }

  // TODO: provide row count estimates?

  return true;
}

void
vt_dfastate_subset::xDisconnect(bool destroy) {

  const std::lock_guard<std::mutex> lock(g_mutex);

  if (memory_) {
    memory_->rc--;

    if (memory_->rc <= 0) {
      delete memory_;
    }
  }

}

void
vt_dfastate_subset::xConnect(bool create)
{
  declare(R"SQL(

    create table fl_dfastate_subset(
      dfa_id INT HIDDEN,
      ptr INT HIDDEN,
      by_ids any [blob] HIDDEN,
      by_states any [blob] HIDDEN,
      state any [blob]
    )

  )SQL");
}

vt_dfastate_subset::result_type
vt_dfastate_subset::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{
  auto ptr_is = info.get("ptr", SQLITE_INDEX_CONSTRAINT_IS);

  if (nullptr != ptr_is) {

    const std::lock_guard<std::mutex> lock(g_mutex);
    memory_ = recall(db_, ptr_is);
    memory_->rc++;

    return {};
  }

  // TODO: Could be nice and support unconstrained dfa_id.

  auto dfa_id_eq = info.get("dfa_id", SQLITE_INDEX_CONSTRAINT_EQ);
  auto dfa_id = std::get<fl::value::integer>(dfa_id_eq->current_value.value());

  fl::error::raise_if(nullptr == memory_, "not initialized");

  fl::error::raise_if(
    dfa_id.value < 1 || fl::detail::safe_to<size_t>(dfa_id.value - 1) >= memory_->dfas.size(),
    "no such dfa");

  fl::db tmp(":memory:");
  tmp.deserialize("main", memory_->dfas[dfa_id.value - 1], SQLITE_DESERIALIZE_READONLY);

  struct dfastate {
    std::string state;
  };

  auto by_ids_eq = info.get("by_ids", SQLITE_INDEX_CONSTRAINT_EQ);
  auto by_states_eq = info.get("by_states", SQLITE_INDEX_CONSTRAINT_EQ);

  boost::json::array subset;

  fl::value::variant subset_json;

  if (nullptr != by_ids_eq) {
    subset_json = by_ids_eq->current_value.value();
  }

  if (nullptr != by_states_eq) {

    subset_json = tmp.prepare(R"SQL(

      select
        json_group_array(id)
      from
        nfastate
      where
        state in (select value from json_each(?1))
      group by
        null

    )SQL").execute(by_states_eq->current_value.value()).current_row().at(0).to_variant();
  }

  subset = boost::json::parse(std::get<fl::value::text>(subset_json).value).as_array();

  fl::json::toset(subset);

  boost::unordered_set<std::string> seen;

  result_type result;

  auto state_stmt = tmp.prepare("select state from dfastate").execute();

  for (auto&& row : state_stmt | fl::as<dfastate>()) {
    auto sv = boost::json::parse(row.state).as_array();

    auto intersected = boost::json::array();

    std::ranges::set_intersection(
      sv, subset, std::back_inserter(intersected), fl::json::less{});

    std::string s = boost::json::serialize(intersected);

    if (seen.contains(s)) {
      continue;
    }

    seen.insert(s);

    result.push_back({
      dfa_id,
      fl::value::null{},
      ( by_ids_eq ? by_ids_eq->current_value.value() : fl::value::null{} ),
      ( by_states_eq ? by_states_eq->current_value.value() : fl::value::null{} ),
      fl::value::text{ s }
    });


  }

  return result;

}


