#include <boost/algorithm/hex.hpp>

#include "federlieb/federlieb.hxx"
#include "federlieb/vtab.hxx"

namespace fl = ::federlieb;

// TODO: Instead of encoding xBestIndex information as JSON in idxStr, make use
// of the fact that SQLite considers a return value of SQLITE_CONSTRAINT from
// xFilter a sign that xBestIndex needs to be run again (because the scheme may
// have changed since preparing the statement) and does so for a configurable
// number of times (SQLITE_MAX_SCHEMA_RETRY) when using `sqlite3_prepare_v2`.
// We can then simply record the last couple of xBestIndex data sets and report
// a counter value in idxNum as identifier. Return an error when the idxNum has
// become invalid and hope SQLite will re-prepare the statement.

std::string
fl::vtab::to_sql(const fl::value::null& v)
{
  return "NULL";
}

std::string
fl::vtab::to_sql(const fl::value::integer& v)
{
  return std::to_string(v.value);
}

std::string
fl::vtab::to_sql(const fl::value::real& v)
{
  std::stringstream ss;
  ss << std::defaultfloat << std::setprecision(17) << v.value;
  return ss.str();
}

std::string
fl::vtab::to_sql(const fl::value::text& v)
{
  std::stringstream ss;
  ss << std::quoted(v.value, '\'', '\'');
  return ss.str();
}

std::string
fl::vtab::to_sql(const fl::value::blob& v)
{
  std::string out;

  auto encoded = boost::algorithm::hex(
    reinterpret_cast<char const*>(&v.value[0]),
    reinterpret_cast<char const*>(&v.value[v.value.size()]),
    std::back_inserter(out));

  return "X'" + out + "'";
}

std::string
fl::vtab::to_sql(const fl::value::variant& v)
{
  return std::visit([](auto&& v) { return to_sql(v); }, v);
}

constexpr char const*
fl::vtab::constraint_op_to_string(int const op)
{
  switch (op) {
    case SQLITE_INDEX_CONSTRAINT_EQ:
      return "=";
    case SQLITE_INDEX_CONSTRAINT_GT:
      return ">";
    case SQLITE_INDEX_CONSTRAINT_LE:
      return "<=";
    case SQLITE_INDEX_CONSTRAINT_LT:
      return "<";
    case SQLITE_INDEX_CONSTRAINT_GE:
      return ">=";
    case SQLITE_INDEX_CONSTRAINT_MATCH:
      return "MATCH";
    case SQLITE_INDEX_CONSTRAINT_LIKE:
      return "LIKE";
    case SQLITE_INDEX_CONSTRAINT_GLOB:
      return "GLOB";
    case SQLITE_INDEX_CONSTRAINT_REGEXP:
      return "REGEXP";
    case SQLITE_INDEX_CONSTRAINT_NE:
      return "<>";
    case SQLITE_INDEX_CONSTRAINT_ISNOT:
      return "IS NOT";
    case SQLITE_INDEX_CONSTRAINT_ISNOTNULL:
      return "IS NOT"; // Apparently so
    case SQLITE_INDEX_CONSTRAINT_ISNULL:
      return "IS"; // Apparently so
    case SQLITE_INDEX_CONSTRAINT_IS:
      return "IS";
  }

  return nullptr;
}

std::string
fl::vtab::to_sql(const fl::vtab::constraint_info& constraint,
                 bool const include_collation)
{

  fl::error::raise_if(include_collation, "unimplemented");
  fl::error::raise_if(constraint.many_at_once.value_or(false), "unimplemented");

  auto op_str = constraint_op_to_string(constraint.op);

  if ((constraint.current_value.has_value()) && nullptr != op_str) {
    return std::string(op_str) + " " + to_sql(constraint.current_value.value());
  }

  return "";
}

std::string
fl::vtab::usable_constraints_to_where_fragment(const std::string& table_name,
                                               const fl::vtab::index_info& info)
{
  std::stringstream ss;

  auto quoted_table_name = fl::detail::quote_identifier(table_name);

  for (auto column : info.columns) {
    auto quoted_column_name = fl::detail::quote_identifier(column.column_name);

    for (auto constraint : column.constraints) {
      auto constraint_string = to_sql(constraint);

      if (!constraint_string.empty() && column.column_index >= 0) {
        if (ss.tellp() > 0) {
          ss << " AND ";
        }
        ss << quoted_table_name << "." << quoted_column_name << " "
           << constraint_string;
      }
    }
  }

  return ss.str();
}

void
fl::vtab::index_info::mark_wanted(const std::string& name, int const op)
{
  // FIXME: do not trust next_argv_index
  for (auto&& column : columns) {
    if (column.column_name == name) {
      for (auto&& constraint : column.constraints) {
        if (constraint.usable && constraint.op == op) {
          if (!constraint.argv_index) {
            constraint.argv_index = next_argv_index++;
          }
        }
      }
    }
  }
}

void
fl::vtab::index_info::mark_transferables(const std::string& column_name)
{
  // FIXME: do not trust next_argv_index
  for (auto&& column : columns) {
    if (column_name == column.column_name) {
      for (auto&& constraint : column.constraints) {
        if (constraint.usable &&
            nullptr != constraint_op_to_string(constraint.op)) {
          if (!constraint.argv_index) {
            constraint.argv_index = next_argv_index++;
          }
        }
      }
    }
  }
}

fl::vtab::constraint_info const*
fl::vtab::index_info::get(const std::string& name, int const op) const
{

  for (auto&& column : columns) {
    if (column.column_name == name) {
      for (auto&& constraint : column.constraints) {
        if (constraint.op == op && constraint.usable) {
          return &constraint;
        }
      }
    }
  }

  return nullptr;
}

void
fl::vtab::index_info_export(const fl::vtab::index_info& ours,
                            sqlite3_index_info* theirs)
{

  theirs->estimatedCost = ours.estimated_cost;

  if (sqlite3_libversion_number() >= 3008002) {
    theirs->estimatedRows = ours.estimated_rows;
  }

  if (sqlite3_libversion_number() >= 3009000) {
    theirs->idxFlags &= ~SQLITE_INDEX_SCAN_UNIQUE;
    if (ours.unique) {
      theirs->idxFlags |= SQLITE_INDEX_SCAN_UNIQUE;
    }
  }

  theirs->orderByConsumed = ours.order_by_consumed;

  for (auto&& column : ours.columns) {
    for (auto&& con : column.constraints) {

      theirs->aConstraintUsage[con.id].omit = con.omit_check;

      if (con.many_at_once && *con.many_at_once) {
        if (!con.argv_index) {
          fl::error::raise(
            "Must specify argv_index for many_at_once processing");
        }
        fl::api(sqlite3_vtab_in,
                static_cast<sqlite3*>(nullptr),
                theirs,
                fl::detail::safe_to<int>(con.id),
                fl::detail::safe_to<int>(*con.many_at_once));
      }

      if (con.argv_index) {
        theirs->aConstraintUsage[con.id].argvIndex =
          fl::detail::safe_to<int>(*con.argv_index);
      }
    }
  }
}

fl::vtab::index_info
fl::vtab::index_info_import(std::vector<fl::vtab::column> columns_,
                            sqlite3_index_info* theirs)
{

  fl::vtab::index_info ours = {};

  ours.estimated_cost = theirs->estimatedCost;

  ours.columns = std::vector<fl::vtab::column_info>(columns_.size());

  if (sqlite3_libversion_number() >= 3008002) {
    ours.estimated_rows = theirs->estimatedRows;
  } else {
    ours.estimated_rows = 25;
  }

  if (sqlite3_libversion_number() >= 3009000) {
    ours.unique = theirs->idxFlags & SQLITE_INDEX_SCAN_UNIQUE;
  }

  if (sqlite3_libversion_number() >= 3010000) {
    for (size_t ix = 0; ix < std::min(size_t(63), columns_.size() - 1); ++ix) {
      ours.columns[ix + 1].used = theirs->colUsed & (1 << (ix));
    }
  }

  for (int i = 0; i < fl::detail::safe_to<int>(columns_.size()); ++i) {
    auto& column = ours.columns[i];
    column.column_index = columns_[i].index;
    column.column_name = columns_[i].name;
  }

  for (auto ix = 0; ix < theirs->nOrderBy; ++ix) {
    auto&& column = ours.columns[theirs->aOrderBy[ix].iColumn + 1];

    if (!column.order_by_pos) {
      column.order_by_pos = ix + 1;
      column.order_by_desc = !!theirs->aOrderBy[ix].desc;
    }
  }

  for (auto ix = 0; ix < theirs->nConstraint; ++ix) {

    switch (theirs->aConstraint[ix].op) {
      case SQLITE_INDEX_CONSTRAINT_LIMIT:
        continue;
      case SQLITE_INDEX_CONSTRAINT_OFFSET:
        continue;
    }

    auto&& column = ours.columns[theirs->aConstraint[ix].iColumn + 1];
    fl::vtab::constraint_info con;

    con.id = ix;
    con.op = theirs->aConstraint[ix].op;
    con.usable = theirs->aConstraint[ix].usable;
    con.collation = std::string(sqlite3_vtab_collation(theirs, ix));
    con.omit_check = theirs->aConstraintUsage[ix].omit;
    con.argv_index = std::nullopt;

    if (sqlite3_libversion_number() >= 3038000) {

      if (fl::api(
            sqlite3_vtab_in, static_cast<sqlite3*>(nullptr), theirs, ix, -1)) {
        con.many_at_once = false;
      }

      sqlite3_value* rhs = nullptr;
      int rc = fl::api(sqlite3_vtab_rhs_value,
                       static_cast<sqlite3*>(nullptr),
                       theirs,
                       ix,
                       &rhs);

      if (rc == SQLITE_OK) {
        con.rhs = fl::value::from(rhs);
      }
    }

    column.constraints.push_back(con);
    column.used = true;
  }

  for (auto&& column : ours.columns) {
    std::ranges::sort(column.constraints, {}, [](auto&& e) {
      return std::make_tuple(e.op, !e.usable);
    });
  }

  if (sqlite3_libversion_number() >= 3038000) {
    ours.distinct_mode =
      fl::api(sqlite3_vtab_distinct, static_cast<sqlite3*>(nullptr), theirs);
  }

  return ours;
}

bool
fl::vtab::mark_constraints(std::vector<fl::vtab::column> columns_,
                           fl::vtab::index_info& ours)
{
  for (size_t ix = 0; ix < ours.columns.size(); ++ix) {
    // NOTE: this relies on import sorting / xBestIndex not changing the
    // order.
    bool has_eq =
      (!ours.columns[ix].constraints.empty() &&
       ours.columns[ix].constraints[0].op == SQLITE_INDEX_CONSTRAINT_EQ &&
       ours.columns[ix].constraints[0].usable);

    if ((columns_[ix].required || columns_[ix].hidden) && has_eq) {
      ours.columns[ix].constraints[0].argv_index = ours.next_argv_index++;

    } else if (columns_[ix].required) {

      return false;
    }
  }

  return true;
}
