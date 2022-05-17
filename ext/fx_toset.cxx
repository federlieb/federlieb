#include <boost/json.hpp>

#include "fx_toset.hxx"

namespace fl = ::federlieb;

boost::json::array
fx_toset::xFunc(const fl::value::variant& value)
{
  auto array = boost::json::value_from(value).as_array();
  fl::json::toset(array);
  return array;
}

void
fx_toset_agg::xStep(const fl::value::variant value)
{
  if (std::holds_alternative<fl::value::blob>(value)) {
    fl::error::raise("Cannot turn BLOB into JSON");
  }
  data_.insert(data_.end(), boost::json::value_from(value));
}

boost::json::array
fx_toset_agg::xFinal()
{
  fl::json::toset(data_);
  return data_;
}

std::optional<std::string>
fx_toset_union::xFunc(const std::optional<std::string>& lhs,
                      const std::optional<std::string>& rhs)
{
  if (!lhs.has_value() || !rhs.has_value()) {
    return std::nullopt;
  }

  auto a1 = boost::json::parse(lhs.value()).as_array();
  auto a2 = boost::json::parse(rhs.value()).as_array();

  a1.insert(a1.end(), a2.begin(), a2.end());

  fl::json::toset(a1);

  return boost::json::serialize(a1);
}

std::optional<std::string>
fx_toset_intersection::xFunc(const std::optional<std::string>& lhs,
                             const std::optional<std::string>& rhs)
{
  if (!lhs.has_value() || !rhs.has_value()) {
    return std::nullopt;
  }

  auto a1 = boost::json::parse(lhs.value()).as_array();
  auto a2 = boost::json::parse(rhs.value()).as_array();

  fl::json::toset(a1);
  fl::json::toset(a2);

  auto result = boost::json::array();

  std::ranges::set_intersection(
    a1, a2, std::back_inserter(result), fl::json::less{});

  return boost::json::serialize(result);
}

std::optional<std::string>
fx_toset_except::xFunc(const std::optional<std::string>& lhs,
                       const std::optional<std::string>& rhs)
{
  if (!lhs.has_value() || !rhs.has_value()) {
    return std::nullopt;
  }

  auto a1 = boost::json::parse(lhs.value()).as_array();
  auto a2 = boost::json::parse(rhs.value()).as_array();

  fl::json::toset(a1);
  fl::json::toset(a2);

  auto result = boost::json::array();

  std::ranges::set_difference(
    a1, a2, std::back_inserter(result), fl::json::less{});

  return boost::json::serialize(result);
}

int
fx_toset_contains::xFunc(const std::string haystack,
                         const fl::value::variant needle)
{
  if (std::holds_alternative<fl::value::blob>(needle)) {
    return 0;
  }

  auto array = boost::json::parse(haystack).as_array();

  return (array.end() !=
          std::ranges::find(array, boost::json::value_from(needle)));
}
