#ifndef FEDERLIEB_ROW_HXX
#define FEDERLIEB_ROW_HXX

#include <boost/stl_interfaces/iterator_interface.hpp>

#include "api.hxx"
#include "federlieb/field.hxx"

namespace federlieb {

namespace fl = ::federlieb;

class field;
class row;
class stmt;

class row_iterator
  : public boost::stl_interfaces::iterator_interface<
      fl::row_iterator,
      std::random_access_iterator_tag,
      fl::field>

{
public:
  constexpr row_iterator();

protected:
  constexpr explicit row_iterator(fl::row const* const row, int index)
    : row_(row)
    , index_(index)
  {}

public:
  fl::field operator*() const;

  constexpr row_iterator& operator+=(std::ptrdiff_t i) noexcept
  {
    index_ += i;
    return *this;
  }

  constexpr auto operator-(row_iterator other) const noexcept
  {
    return index_ - other.index_;
  }

  constexpr bool operator==(const row_iterator& other) const noexcept
  {
    return index_ == other.index_ && row_ == other.row_;
  }

protected:
  friend class fl::row;
  sqlite3* db() const;
  fl::row const* row_ = nullptr;
  std::ptrdiff_t index_ = 0;
};

class row
{
public:
  using iterator = row_iterator;

  row(stmt const* const stmt);

  int size() const { return size_; }

  fl::field at(int index) const;

  row_iterator begin() const;
  row_iterator end() const;

protected:
  fl::stmt const* stmt_ = nullptr;
  int size_ = 0;
  friend class fl::field;
  friend class fl::row_iterator;
  sqlite3* db() const;
};

}

#endif
