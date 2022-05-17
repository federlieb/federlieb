#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

constexpr fl::row_iterator::row_iterator()
{
  fl::error::raise(
    "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2325r3.html");
}

sqlite3*
fl::row_iterator::db() const
{
  return row_->db();
}

sqlite3*
fl::row::db() const
{
  return stmt_->db();
}

fl::row::row(fl::stmt const* const stmt)
  : stmt_(stmt)
{
  size_ = fl::api(sqlite3_column_count, db(), stmt_->stmt_.get());
}

fl::row_iterator
fl::row::begin() const
{
  return row_iterator(this, 0);
}

fl::row_iterator
fl::row::end() const
{
  return row_iterator(this, size());
}

fl::field
fl::row_iterator::operator*() const
{
  return fl::field(*this->row_, index_);
}

fl::field
fl::row::at(int index) const
{
  if (index >= size()) {
    fl::error::raise("Out of bounds.");
  }
  return fl::field(*this, index);
}

static_assert(std::ranges::range<fl::row>);
static_assert(std::ranges::forward_range<fl::row>);
static_assert(std::ranges::bidirectional_range<fl::row>);
static_assert(std::ranges::random_access_range<fl::row>);
static_assert(std::ranges::sized_range<fl::row>);
