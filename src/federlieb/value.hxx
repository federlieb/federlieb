#ifndef FEDERLIEB_VALUE_HXX
#define FEDERLIEB_VALUE_HXX

#include <variant>

#include "api.hxx"
#include "federlieb/detail.hxx"
#include "federlieb/error.hxx"

namespace federlieb::value {

namespace fl = ::federlieb;

struct integer
{
  sqlite3_int64 value;
  std::strong_ordering operator<=>(const integer&) const = default;
};

struct real
{
  double value;
  auto operator<=>(const real&) const = default;
};

struct null
{
  nullptr_t value = nullptr;
  // all nulls are equal
  std::strong_ordering operator<=>(const null&) const = default;
};

struct blob
{
  fl::blob_type value;
  std::strong_ordering operator<=>(const blob&) const = default;
};

struct text
{
  std::string value;
  std::strong_ordering operator<=>(const text&) const = default;
};

struct json
{
  std::string value;
  std::strong_ordering operator<=>(const json&) const = default;
  operator fl::value::text();
};

std::ostream&
operator<<(std::ostream& os, const real& v);

std::ostream&
operator<<(std::ostream& os, const integer& v);

std::ostream&
operator<<(std::ostream& os, const text& v);

std::ostream&
operator<<(std::ostream& os, const null& v);

std::ostream&
operator<<(std::ostream& os, const blob& v);

using variant = std::variant<null, integer, real, text, blob, json>;

template<std::integral T>
inline variant
from(const T& value)
{
  return integer{ fl::detail::safe_to<decltype(integer::value)>(value) };
}

variant
from(const double& value);

variant
from(const fl::blob_type& value);

variant
from(const std::string& value);

variant
from(const nullptr_t& value);

variant
from(sqlite3_value* value);

template<typename T>
inline variant
from(const std::optional<T>& value)
{
  return value.has_value() ? from(*value) : null{};
}

std::ostream&
operator<<(std::ostream& os, const variant& variant);

struct coercion
{
  template<std::integral T>
  inline void operator()(sqlite3_value* const value, T& sink)
  {
    sink = fl::detail::safe_to<T>(sqlite3_value_int64(value));
  }

  void operator()(sqlite3_value* const value, double& sink);
  void operator()(sqlite3_value* const value, std::string& sink);
  void operator()(sqlite3_value* const value, blob_type& sink);

  // NOTE: must not be used on unprotected value
  void operator()(sqlite3_value* const value, fl::value::blob& sink);
  // NOTE: must not be used on unprotected value
  void operator()(sqlite3_value* const value, fl::value::variant& sink);

  template<typename T>
  inline void operator()(sqlite3_value* const value, std::optional<T>& sink)
  {
    if (sqlite3_value_type(value) == SQLITE_NULL) {
      sink = std::nullopt;
    } else {
      sink = T{};
      operator()(value, *sink);
    }
  }
};

template<typename T>
T
as(sqlite3_value* possibly_unprotected)
{

  auto shared = std::shared_ptr<sqlite3_value>(
    fl::api(
      sqlite3_value_dup, static_cast<sqlite3*>(nullptr), possibly_unprotected),
    sqlite3_value_free);

  fl::error::raise_if(!shared, "allocation problem");

  fl::value::coercion coerce;

  T result{};

  coerce(shared.get(), result);

  return result;
}

}

#endif
