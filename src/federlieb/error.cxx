#include "federlieb/federlieb.hxx"
#include <boost/stacktrace.hpp>
#include <iostream>

namespace fl = ::federlieb;

#if 0
std::ostream&
fl::error::operator<<(std::ostream& os, const fl::error::missing_constraint& e)
{
  return os << "::missing_constraint";
}
#endif
