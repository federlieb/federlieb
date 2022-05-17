#ifndef FEDERLIEB_CONCEPTS_HXX
#define FEDERLIEB_CONCEPTS_HXX

#include <concepts>

namespace federlieb::concepts {

namespace fl = ::federlieb;

template<typename Vtab>
concept VirtualTable = requires(Vtab vtab,
                                typename Vtab::cursor c,
                                Vtab::cursor* cursor)
{
  { vtab.xConnect(true) };
  typename Vtab::cursor;
  {
    vtab.xFilter({}, cursor)
    } -> std::ranges::input_range;
  {
    *vtab.xFilter({}, cursor).begin()
    } -> std::ranges::random_access_range;
};

}

#endif
