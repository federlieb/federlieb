#ifndef FEDERLIEB_JSON_HXX
#define FEDERLIEB_JSON_HXX

#include <boost/json.hpp>

#include "federlieb/value.hxx"
#include "federlieb/vtab.hxx"

namespace federlieb {

namespace fl = ::federlieb;

namespace detail {
template<typename T>
void
tag_invoke_optional(boost::json::value& theirs, const std::optional<T>& ours)
{
  if (ours.has_value()) {
    theirs = boost::json::value_from(*ours);
  } else {
    theirs = nullptr;
  }
}

}
}

namespace federlieb::value {

namespace fl = ::federlieb;

void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::null& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::integer& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::real& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::text& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::json& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::blob& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::value::variant& ours);
}

namespace federlieb::vtab {

namespace fl = ::federlieb;

void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::vtab::constraint_info& ours);
void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::vtab::column_info& ours);

void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::vtab::index_info& ours);

}

namespace federlieb {

namespace fl = ::federlieb;

void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const fl::row& ours);
}

namespace federlieb::json {

namespace fl = ::federlieb;

template<typename T>
void
to(const boost::json::object& source, const std::string& key, T& sink)
{
  sink = boost::json::value_to<T>(source.at(key));
}

template<typename T>
void
to(const boost::json::object& source,
   const std::string& key,
   std::optional<T>& sink)
{
  if (source.contains(key) && !source.at(key).is_null()) {
    sink = boost::json::value_to<T>(source.at(key));
  } else {
    sink = std::nullopt;
  }
}

}

namespace federlieb::vtab {

namespace fl = ::federlieb;

fl::vtab::index_info
tag_invoke(const boost::json::value_to_tag<fl::vtab::index_info>&,
           const boost::json::value& theirs);

fl::vtab::column_info
tag_invoke(const boost::json::value_to_tag<fl::vtab::column_info>&,
           const boost::json::value& theirs);

fl::vtab::constraint_info
tag_invoke(const boost::json::value_to_tag<fl::vtab::constraint_info>&,
           const boost::json::value& theirs);

}

namespace federlieb::value {

namespace fl = ::federlieb;

fl::value::variant
tag_invoke(const boost::json::value_to_tag<fl::value::variant>&,
           const boost::json::value& theirs);

}

namespace federlieb::json {

namespace fl = ::federlieb;

struct less
{
  bool operator()(const boost::json::value& lhs, const boost::json::value& rhs);
  bool operator()(const boost::json::key_value_pair& lhs,
                  const boost::json::key_value_pair& rhs);
  bool operator()(const boost::json::object& lhs,
                  const boost::json::object& rhs);
  bool operator()(const boost::json::array& lhs, const boost::json::array& rhs);
};

void
toset(boost::json::array& array);

}

#endif
