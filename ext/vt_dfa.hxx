#pragma once

#include "federlieb/vtab.hxx"
#include "vt_bgl.hxx"

namespace fl = ::federlieb;

class vt_deterministic_automaton
  : public fl::vtab::base<vt_deterministic_automaton>
{
public:
  static inline char const* const name = "deterministic_automaton";

  using result_type = std::list<std::array<fl::value::variant, 2>>;

  struct cursor
  {
    cursor(vt_deterministic_automaton* vtab);
  };

  void xConnect(bool create);

  result_type xFilter(const fl::vtab::index_info& info, cursor* cursor);
};
