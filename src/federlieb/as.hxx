#ifndef FEDERLIEB_AS_HXX
#define FEDERLIEB_AS_HXX

#include <boost/pfr.hpp>
#include <ranges>

#include "federlieb/row.hxx"

namespace federlieb {

namespace fl = ::federlieb;

template<typename T>
requires std::is_default_constructible_v<T>
auto
as()
{
  return std::ranges::views::transform([](fl::row&& row) {
    int sink_size = boost::pfr::tuple_size<T>();

    if (sink_size > row.size()) {
      fl::error::raise("as() needs more columns than those returned");
    }

    T sink;

    boost::pfr::for_each_field(
      sink, [&row](auto&& value, int index) { row.at(index).to(value); });

    return sink;
  });
}

}

#endif
