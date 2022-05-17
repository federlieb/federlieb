#ifndef FEDERLIEB_FIELD_HXX
#define FEDERLIEB_FIELD_HXX

#include "api.hxx"
#include "federlieb/row.hxx"
#include "federlieb/value.hxx"

namespace federlieb {

namespace fl = ::federlieb;

class context;

class field
{
public:
  int index() const { return index_; }
  std::string name() const;
  int type() const;
  bool is_null() const { return type() == SQLITE_NULL; }
  bool is_text() const { return type() == SQLITE_TEXT; }
  bool is_blob() const { return type() == SQLITE_BLOB; }
  bool is_integer() const { return type() == SQLITE_INTEGER; }
  bool is_float() const { return type() == SQLITE_FLOAT; }

  template<fl::compatible_type T>
  inline operator T()
  {
    T sink;
    to(sink);
    return sink;
  }

  operator fl::value::variant();

  void to(fl::value::variant& sink) const;

  template<std::integral T>
  inline void to(T& sink) const
  {
    auto value = to_integer();
    sink = fl::detail::safe_to<T>(value);
  }

  void to(double& sink) const;
  void to(std::string& sink) const;
  void to(fl::blob_type& sink) const;

  template<fl::compatible_type T>
  inline void to(std::optional<T>& sink) const
  {
    if (is_null()) {
      sink = std::nullopt;
      return;
    }
    to(*sink);
  }

  fl::value::variant to_variant() const;
  sqlite3_int64 to_integer() const;
  std::string to_text() const;
  double to_float() const;
  fl::blob_type to_blob() const;

protected:
  constexpr field(const fl::row& row, int const index)
    : row_(row)
    , index_(index)
  {}

  sqlite3_value* unprotected_value() const;

  const fl::row& row_;
  int const index_;
  sqlite3* db() const;
  friend class fl::row;
  friend class fl::row_iterator;
  friend class fl::stmt;

  friend class fl::context;
};

}

#endif
