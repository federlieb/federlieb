#ifndef FEDERLIEB_DETAIL_HXX
#define FEDERLIEB_DETAIL_HXX

#include <chrono>
#include <limits>
#include <ranges>
#include <regex>
#include <source_location>
#include <string>

#include "api.hxx"

#include "federlieb/detail.hxx"
#include "federlieb/error.hxx"

namespace federlieb {

namespace fl = ::federlieb;

using blob_type = std::vector<std::byte>;

template<typename T>
concept compatible_type = (std::integral<T> || std::same_as<T, double> ||
                           std::same_as<T, std::string> ||
                           std::same_as<T, fl::blob_type>);

namespace detail {
template<typename... Args>
inline std::string
sprintf(Args... args)
{
  auto result = sqlite3_mprintf(args...);
  std::shared_ptr<char> ptr(result, sqlite3_free);
  fl::error::raise_if(nullptr == ptr, "Unable to format");
  return std::string{ ptr.get() };
}

template<std::integral To, std::integral From>
inline To
safe_to(From value)
{
  fl::error::raise_if(!std::in_range<To>(value), "out of range");
  return To(value);
}

template<std::integral To>
inline To
safe_to(bool value)
{
  return safe_to<To>(int(value));
}

template<std::ranges::range R>
inline std::vector<std::ranges::range_value_t<R>>
to_vector(R&& range)
{
  auto tmp = range | std::views::common;

  std::vector<std::ranges::range_value_t<R>> v;

  if constexpr (requires { std::ranges::size(range); }) {
    v.reserve(std::ranges::size(range));
  }

  std::ranges::copy(tmp, std::back_inserter(v));

  return v;
}

std::string
mangle_for_multiline_comment(const std::string& s);

std::string
quote_string(const std::string& s);

std::string
quote_identifier(const std::string& s);

std::vector<std::string>
regex_split(const std::string& s,
            const std::regex& re,
            int const submatch = -1);

std::string
type_string_to_affinity(const std::string& s);

std::string extract_query(const std::string s);

template<typename T = std::string>
auto
suffix(const T& suffix)
{
  return std::views::transform([suffix](auto&& e) { return e + suffix; });
}

template<typename T = std::string>
auto
prefix(const T& prefix)
{
  return std::views::transform([prefix](auto&& e) { return prefix + e; });
}

struct formatter
{
  template<std::integral T>
  std::string operator()(const T& value)
  {
    return std::to_string(value);
  }

  std::string operator()(const std::string& value) { return value; }
};

template<typename... Ts>
std::string
format(const std::string& format, Ts... args)
{
  if (format.empty()) {
    return "";
  }

  formatter f;
  std::vector<std::string> values{ f(args)... };
  std::stringstream s;

  size_t ix = 0;
  bool open = false;

  for (auto it = format.begin(); it != format.end(); ++it) {
    if (!open && *it == '{') {
      open = true;
    } else if (!open) {
      s << *it;
    } else if (*it == '}') {
      s << values.at(ix++);
      open = false;
    } else {
      fl::error::raise("bad format string");
    }
  }

  return s.str();
}

struct capture_location
{
  char const* const fmt;
  const std::source_location& loc;

  capture_location(
    char const* const fmt,
    const std::source_location& loc = std::source_location::current())
    : fmt(fmt)
    , loc(loc)
  {}
};

template<typename... Ts>
void
log(const capture_location& fmt, Ts&&... args)
{

#if 0
    auto time = std::chrono::zoned_time{ std::chrono::current_zone(),
                                         std::chrono::system_clock::now() };
#else
  // %Y-%m-%dT%H:%M%:S%z
  auto time = std::chrono::system_clock::now();
#endif

  auto formatted = format(fmt.fmt, std::forward<Ts>(args)...);

#if 0
  boost::json::object ecs = { { "@timestamp", nullptr },
                              { "ecs", { "version", "1.0.0" } },
                              { "message", formatted },
                              { "data_stream", { "type", "log" } },
                              { "process", { { "pid", nullptr } } },
                              { "log",
                                { "level", nullptr },
                                { "origin",
                                  { "function", fmt.loc.function_name() },
                                  { "file",
                                    { "line", fmt.loc.line() },
                                    { "name", fmt.loc.file_name() } } } } };
#endif

  std::cerr << fl::detail::format("{} {} {}\n{}\n",
                                  fmt.loc.file_name(),
                                  fmt.loc.function_name(),
                                  fmt.loc.line(),
                                  formatted);
}

template<typename T>
std::string
str(T&& r)
{
  auto flat = r | std::views::join;
  std::string s;
  std::ranges::copy(flat, std::back_inserter(s));
  return s;
}

}

}

#endif
