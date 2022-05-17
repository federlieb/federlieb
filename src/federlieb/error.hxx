#ifndef FEDERLIEB_ERROR_HXX
#define FEDERLIEB_ERROR_HXX

#include <boost/stacktrace.hpp>
#include <iostream>

namespace federlieb::error {

namespace fl = ::federlieb;

#if 0
struct missing_constraint : virtual std::exception
{};

std::ostream&
operator<<(std::ostream& os, const fl::error::missing_constraint& e);
#endif

template<typename T>
[[noreturn]] void inline raise(T e)
{
  std::cerr << boost::stacktrace::stacktrace() << std::endl;
  std::cerr << e << std::endl;
  throw e;
}

template<typename T>
inline void
raise_if(bool condition, T e)
{
  if (!condition) {
    return;
  }
  raise(e);
}

}

#endif
