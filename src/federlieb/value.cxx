#include <variant>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>

#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

fl::value::json::operator fl::value::text()
{
  return fl::value::text{ value };
}

std::ostream&
fl::value::operator<<(std::ostream& os, const fl::value::real& v)
{
  return os << v.value;
}

std::ostream&
fl::value::operator<<(std::ostream& os, const fl::value::integer& v)
{
  return os << v.value;
}

std::ostream&
fl::value::operator<<(std::ostream& os, const fl::value::text& v)
{
  return os << std::quoted(v.value, '\'', '\'');
}

std::ostream&
fl::value::operator<<(std::ostream& os, const fl::value::null& v)
{
  return os << "NULL";
}

std::ostream&
fl::value::operator<<(std::ostream& os, const fl::value::blob& v)
{

  os << "X'";

  std::string out;

  boost::algorithm::hex(reinterpret_cast<char const*>(&v.value[0]),
                        reinterpret_cast<char const*>(&v.value[v.value.size()]),
                        std::back_inserter(out));

  os << out;
  os << "'";

  return os;
}

fl::value::variant
fl::value::from(const double& value)
{
  return fl::value::real{ value };
}

fl::value::variant
fl::value::from(const fl::blob_type& value)
{
  return fl::value::blob{ value };
}

fl::value::variant
fl::value::from(const std::string& value)
{
  return fl::value::text{ value };
}

fl::value::variant
fl::value::from(const nullptr_t& value)
{
  return fl::value::null{};
}

std::ostream&
fl::value::operator<<(std::ostream& os, const fl::value::variant& variant)
{
  std::visit([&os](auto&& e) { os << e; }, variant);
  return os;
}

fl::value::variant
fl::value::from(sqlite3_value* value)
{
  // NOTE: must not be used on unprotected sqlite3_value
  switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
      return fl::value::integer{ sqlite3_value_int64(value) };
    case SQLITE_FLOAT:
      return fl::value::real{ sqlite3_value_double(value) };
    case SQLITE_TEXT: {
      auto data = sqlite3_value_text(value);
      fl::error::raise_if(nullptr == data, "allocation problem");
      auto length = sqlite3_value_bytes(value);
      auto str = std::string(reinterpret_cast<const char*>(data), length);
      if ('J' == sqlite3_value_subtype(value)) {
        return fl::value::json{ str };
      }
      return fl::value::text{ str };
    }
    case SQLITE_BLOB: {
      auto data = static_cast<fl::blob_type::value_type const*>(
        sqlite3_value_blob(value));
      fl::error::raise_if(nullptr == data, "allocation problem");
      auto length = sqlite3_value_bytes(value);
      return fl::value::blob{ fl::blob_type(data, data + length) };
    }
    case SQLITE_NULL:
      return fl::value::null{};
    default:
      fl::error::raise("unsupported sqlite3_value_type");
  }
}

void
fl::value::coercion::operator()(sqlite3_value* const value, double& sink)
{
  sink = double(sqlite3_value_double(value));
}

void
fl::value::coercion::operator()(sqlite3_value* const value, std::string& sink)
{
  fl::error::raise_if(sqlite3_value_type(value) == SQLITE_NULL,
                      "Cannot convert NULL to std::string");

  auto data = sqlite3_value_text(value);
  auto length = sqlite3_value_bytes(value);

  fl::error::raise_if(nullptr == data, "allocation error");

  sink = std::string(reinterpret_cast<const char*>(data), length);
}

void
fl::value::coercion::operator()(sqlite3_value* const value, blob_type& sink)
{

  fl::error::raise_if(sqlite3_value_type(value) == SQLITE_NULL,
                      "Cannot convert NULL to blob_type");

  auto data =
    static_cast<fl::blob_type::value_type const*>(sqlite3_value_blob(value));
  auto length = sqlite3_value_bytes(value);

  fl::error::raise_if(nullptr == data, "allocation error");

  sink = fl::blob_type(data, data + length);
}

void
fl::value::coercion::operator()(sqlite3_value* const value,
                                fl::value::blob& sink)
{
  operator()(value, sink.value);
}

void
fl::value::coercion::operator()(sqlite3_value* const value,
                                fl::value::variant& sink)
{
  sink = fl::value::from(value);
}
