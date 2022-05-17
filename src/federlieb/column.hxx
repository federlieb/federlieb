#ifndef FEDERLIEB_COLUMN_HXX
#define FEDERLIEB_COLUMN_HXX

#include "api.hxx"

#include "federlieb/field.hxx"
#include "federlieb/value.hxx"

namespace federlieb {

namespace fl = ::federlieb;

class column_view;
class column_iterator;

class column
{
public:
  int index() const { return index_; }
  std::string name() const;
  std::string declared_type() const;
  std::string declared_affinity() const;

protected:
  friend class fl::column_iterator;
  column(const fl::stmt& stmt, int const index)
    : stmt_(stmt)
    , index_(index)
  {}

  sqlite3* db() const;
  const fl::stmt& stmt_;
  const int index_;
};

class column_iterator
  : public boost::stl_interfaces::iterator_interface<
      fl::column_iterator,
      std::random_access_iterator_tag,
      fl::column>

{
public:
  constexpr column_iterator(){};

protected:
  constexpr explicit column_iterator(fl::stmt const* const stmt, int index)
    : stmt_(stmt)
    , index_(index)
  {}

public:
  fl::column operator*() const;

  column_iterator& operator+=(std::ptrdiff_t i) noexcept;
  std::ptrdiff_t operator-(column_iterator other) const noexcept;
  bool operator==(const column_iterator& other) const noexcept;

protected:
  friend class fl::column;
  friend class fl::column_view;
  sqlite3* db() const;
  fl::stmt const* stmt_ = nullptr;
  std::ptrdiff_t index_ = 0;
};

class column_view : public std::ranges::view_interface<column_view>
{
public:
  column_view() {}
  column_view(fl::stmt const* stmt)
    : stmt_(stmt)
  {}
  column_iterator begin();
  column_iterator end();

protected:
  fl::stmt const* stmt_ = nullptr;
};

}

#endif
