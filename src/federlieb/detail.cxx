#include <iostream>

#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

std::string
fl::detail::quote_identifier(const std::string& s)
{
  std::stringstream ss;
  ss << std::quoted(s, '"', '"');
  return ss.str();
}

std::vector<std::string>
fl::detail::regex_split(const std::string& s,
                        const std::regex& re,
                        int const submatch)
{
  std::vector<std::string> vec;
  std::copy(std::sregex_token_iterator(s.begin(), s.end(), re, submatch),
            std::sregex_token_iterator(),
            std::back_inserter(vec));
  return vec;
}

std::string
fl::detail::type_string_to_affinity(const std::string& s)
{

  // NOTE: This returns the wrong affinity for type string "ANY" used
  // on STRICT tables, as there has been an undocumented rule change.
  
  auto re = std::regex("[^]*?(INT)|[^]*?(CHAR|CLOB|TEXT)|[^]*?(BLOB)|[^]*?("
                       "REAL|FLOA|DOUB)|(^\\s*$)|[^]*?($)",
                       std::regex::icase | std::regex::optimize);

  std::smatch m;

  std::array<const char*, 6> affinity{ "INT",  "TEXT", "BLOB",
                                       "REAL", "BLOB", "NUMERIC" };

  if (std::regex_search(s, m, re)) {

    auto sub_matches = m | std::views::drop(1);

    auto first_match =
      std::ranges::find_if(sub_matches, [](auto&& e) { return e.matched; });

    auto match_index = std::ranges::distance(sub_matches.begin(), first_match);

    return affinity[match_index];

  } else {
    fl::error::raise("impossible");
  }
}
