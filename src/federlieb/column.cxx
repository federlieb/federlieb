#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

fl::column
fl::column_iterator::operator*() const
{
  return fl::column(*this->stmt_, index_);
}

fl::column_iterator&
fl::column_iterator::operator+=(std::ptrdiff_t i) noexcept
{
  index_ += i;
  return *this;
}

ptrdiff_t
fl::column_iterator::operator-(fl::column_iterator other) const noexcept
{
  return index_ - other.index_;
}

bool
fl::column_iterator::operator==(const fl::column_iterator& other) const noexcept
{
  return index_ == other.index_ && stmt_ == other.stmt_;
}

sqlite3*
fl::column::db() const
{
  return stmt_.db();
}

std::string
fl::column::name() const
{
  auto data = fl::api(sqlite3_column_name, db(), stmt_.stmt_.get(), index_);
  fl::error::raise_if(nullptr == data, "memory failure");
  return std::string(data);
}

std::string
fl::column::declared_type() const
{
  auto data = fl::api(sqlite3_column_decltype, db(), stmt_.stmt_.get(), index_);

  if (data == nullptr) {
    return std::string();
  }

  return std::string(data);
}

std::string
fl::column::declared_affinity() const
{
  return fl::detail::type_string_to_affinity(declared_type());
}

fl::column_iterator
fl::column_view::begin()
{
  return column_iterator(stmt_, 0);
}

fl::column_iterator
fl::column_view::end()
{
  return column_iterator(stmt_, stmt_->column_count());
}

static_assert(std::ranges::range<fl::column_view>);
static_assert(std::ranges::view<fl::column_view>);
