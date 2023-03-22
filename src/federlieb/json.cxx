#include <ranges>

#include "federlieb/json.hxx"

namespace fl = ::federlieb;

void
fl::value::tag_invoke(const boost::json::value_from_tag&,
                      boost::json::value& theirs,
                      const fl::value::null& ours)
{
  theirs = nullptr;
}

void
fl::value::tag_invoke(const boost::json::value_from_tag&,
                      boost::json::value& theirs,
                      const fl::value::integer& ours)
{
  theirs = ours.value;
}

void
fl::value::tag_invoke(const boost::json::value_from_tag&,
                      boost::json::value& theirs,
                      const fl::value::real& ours)
{
  theirs = ours.value;
}

void
fl::value::tag_invoke(const boost::json::value_from_tag&,
                      boost::json::value& theirs,
                      const fl::value::text& ours)
{
  theirs = ours.value;
}

void
fl::value::tag_invoke(const boost::json::value_from_tag&,
                      boost::json::value& theirs,
                      const fl::value::blob& ours)
{
  fl::error::raise("BLOBs cannot be converted to JSON");
}

void
fl::value::tag_invoke(const boost::json::value_from_tag&,
                      boost::json::value& theirs,
                      const fl::value::variant& ours)
{
  std::visit([&theirs](auto&& e) { theirs = boost::json::value_from(e); },
             ours);
}

void
fl::vtab::tag_invoke(const boost::json::value_from_tag&,
                     boost::json::value& theirs,
                     const fl::vtab::constraint_info& ours)
{

  boost::json::object tmp;

  tmp["id"] = ours.id;
  tmp["op"] = ours.op;
  tmp["usable"] = ours.usable;
  tmp["omit_check"] = ours.omit_check;
  tmp["collation"] = ours.collation;

  detail::tag_invoke_optional(tmp["argv_index"], ours.argv_index);
  detail::tag_invoke_optional(tmp["many_at_once"], ours.many_at_once);
  detail::tag_invoke_optional(tmp["rhs"], ours.rhs);

  theirs = tmp;
}

void
fl::vtab::tag_invoke(const boost::json::value_from_tag&,
                     boost::json::value& theirs,
                     const fl::vtab::column_info& ours)
{

  boost::json::object tmp;

  tmp["column_index"] = ours.column_index;
  tmp["column_name"] = ours.column_name;
  tmp["constraints"] = boost::json::value_from(ours.constraints);
  tmp["used"] = ours.used;

  detail::tag_invoke_optional(tmp["order_by_pos"], ours.order_by_pos);
  detail::tag_invoke_optional(tmp["order_by_desc"], ours.order_by_desc);

  theirs = tmp;
}

void
fl::vtab::tag_invoke(const boost::json::value_from_tag&,
                     boost::json::value& theirs,
                     const fl::vtab::index_info& ours)
{

  boost::json::object tmp;

  tmp["columns"] = boost::json::value_from(ours.columns);
  detail::tag_invoke_optional(tmp["offset"], ours.offset);
  detail::tag_invoke_optional(tmp["limit"], ours.limit);
  tmp["distinct_mode"] = ours.distinct_mode;
  tmp["unique"] = ours.unique;
  tmp["estimated_rows"] = ours.estimated_rows;
  tmp["estimated_cost"] = ours.estimated_cost;
  tmp["order_by_consumed"] = ours.order_by_consumed;
  tmp["next_argv_index"] = ours.next_argv_index;

  theirs = tmp;
}

void
fl::tag_invoke(const boost::json::value_from_tag&,
               boost::json::value& theirs,
               const fl::row& ours)
{
  boost::json::object tmp;

  for (auto&& field : ours) {
    tmp[field.name()] = boost::json::value_from(field.to_variant());
  }

  theirs = tmp;
}

fl::vtab::index_info
fl::vtab::tag_invoke(const boost::json::value_to_tag<fl::vtab::index_info>&,
                     const boost::json::value& theirs)
{

  auto alias = theirs.as_object();

  fl::vtab::index_info ours = {};

  fl::json::to(alias, "distinct_mode", ours.distinct_mode);
  fl::json::to(alias, "unique", ours.unique);
  fl::json::to(alias, "estimated_rows", ours.estimated_rows);
  fl::json::to(alias, "estimated_cost", ours.estimated_cost);
  fl::json::to(alias, "order_by_consumed", ours.order_by_consumed);
  fl::json::to(alias, "next_argv_index", ours.next_argv_index);
  fl::json::to(alias, "offset", ours.offset);
  fl::json::to(alias, "limit", ours.limit);
  fl::json::to(alias, "columns", ours.columns);

  return ours;
}

fl::vtab::column_info
fl::vtab::tag_invoke(const boost::json::value_to_tag<fl::vtab::column_info>&,
                     const boost::json::value& theirs)
{

  auto alias = theirs.as_object();

  fl::vtab::column_info ours = {};

  fl::json::to(alias, "column_index", ours.column_index);
  fl::json::to(alias, "column_name", ours.column_name);
  fl::json::to(alias, "constraints", ours.constraints);
  fl::json::to(alias, "order_by_desc", ours.order_by_desc);
  fl::json::to(alias, "order_by_pos", ours.order_by_pos);
  fl::json::to(alias, "used", ours.used);

  return ours;
}

fl::vtab::constraint_info
fl::vtab::tag_invoke(
  const boost::json::value_to_tag<fl::vtab::constraint_info>&,
  const boost::json::value& theirs)
{

  auto alias = theirs.as_object();

  fl::vtab::constraint_info ours = {};

  fl::json::to(alias, "argv_index", ours.argv_index);
  fl::json::to(alias, "collation", ours.collation);
  fl::json::to(alias, "id", ours.id);
  fl::json::to(alias, "many_at_once", ours.many_at_once);
  fl::json::to(alias, "omit_check", ours.omit_check);
  fl::json::to(alias, "op", ours.op);
  fl::json::to(alias, "rhs", ours.rhs);
  fl::json::to(alias, "usable", ours.usable);

  return ours;
}

fl::value::variant
fl::value::tag_invoke(const boost::json::value_to_tag<fl::value::variant>&,
                      const boost::json::value& theirs)
{

  switch (theirs.kind()) {
    case boost::json::kind::null:
      return fl::value::null{};
    case boost::json::kind::int64:
      return fl::value::integer{
        fl::detail::safe_to<decltype(fl::value::integer::value)>(
          theirs.get_int64())
      };
    case boost::json::kind::uint64:
      return fl::value::integer{
        fl::detail::safe_to<decltype(fl::value::integer::value)>(
          theirs.get_uint64())
      };
    case boost::json::kind::double_:
      return fl::value::real{ theirs.get_double() };
    case boost::json::kind::string:
      return fl::value::text{ std::string(theirs.get_string()) };
    case boost::json::kind::array:
      return fl::value::text{ boost::json::serialize(theirs) };
    case boost::json::kind::object:
      return fl::value::text{ boost::json::serialize(theirs) };
    default:
      fl::error::raise("bad json type");
  }
}

bool
fl::json::less::operator()(const boost::json::value& lhs,
                           const boost::json::value& rhs)
{
  if (lhs.kind() != rhs.kind()) {
    return lhs.kind() < rhs.kind();
  }

  switch (lhs.kind()) {
    case boost::json::kind::null:
      return false;
    case boost::json::kind::bool_:
      return lhs.get_bool() < rhs.get_bool();
    case boost::json::kind::int64:
      return lhs.get_int64() < rhs.get_int64();
    case boost::json::kind::uint64:
      return lhs.get_uint64() < rhs.get_uint64();
    case boost::json::kind::double_:
      return lhs.get_double() < rhs.get_double();
    case boost::json::kind::string:
      return lhs.get_string() < rhs.get_string();
    case boost::json::kind::array:
      return operator()(lhs.get_array(), rhs.get_array());
    case boost::json::kind::object:
      return operator()(lhs.get_object(), rhs.get_object());
  }

  fl::error::raise("impossible");
}

bool
fl::json::less::operator()(const boost::json::key_value_pair& lhs,
                           const boost::json::key_value_pair& rhs)
{
  if (lhs.key() != rhs.key()) {
    return lhs.key() < rhs.key();
  }

  return operator()(lhs.value(), rhs.value());
}

bool
fl::json::less::operator()(const boost::json::object& lhs,
                           const boost::json::object& rhs)
{
  return std::ranges::lexicographical_compare(lhs, rhs, *this);
}

bool
fl::json::less::operator()(const boost::json::array& lhs,
                           const boost::json::array& rhs)
{
  return std::ranges::lexicographical_compare(lhs, rhs, *this);
}

void
fl::json::toset(boost::json::array& array)
{
  std::ranges::sort(array, fl::json::less{});
  auto last = std::unique(array.begin(), array.end());
  array.erase(last, array.end());
}
