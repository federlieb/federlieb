#ifndef FEDERLIEB_STMT_HXX
#define FEDERLIEB_STMT_HXX

#include "federlieb/column.hxx"
#include "federlieb/detail.hxx"
#include "federlieb/error.hxx"
#include "federlieb/field.hxx"
#include "federlieb/row.hxx"

namespace federlieb {

namespace fl = ::federlieb;

class stmt_iterator
  : public boost::stl_interfaces::
      iterator_interface<fl::stmt_iterator, std::input_iterator_tag, fl::row>

{
public:
  using boost::stl_interfaces::iterator_interface<fl::stmt_iterator,
                                                  std::input_iterator_tag,
                                                  fl::row>::operator++;

  stmt_iterator()
  {
#if 0
    fl::error::raise(
      "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2325r3.html");
#endif
  }

  explicit stmt_iterator(fl::stmt* stmt);

  struct sentinel
  {};

  bool operator==(const fl::stmt_iterator::sentinel&) const;

  fl::row operator*() const;

  stmt_iterator& operator++() noexcept;

protected:
  fl::stmt* stmt_ = nullptr;
  sqlite3* db() const;
};

class stmt
{
public:
  stmt(){
    // creates unusable object
  };

  stmt(std::shared_ptr<sqlite3> db, std::shared_ptr<sqlite3_stmt> stmt)
    : db_(db)
    , stmt_(stmt)
  {
    if (is_busy()) {
      state_ = state::running;
    }
  }

  fl::stmt& bind(int const col, const nullptr_t& v);
  fl::stmt& bind(int const col, const fl::value::blob& v);
  fl::stmt& bind(int const col, const fl::value::text& v);
  fl::stmt& bind(int const col, const fl::value::json& v);
  fl::stmt& bind(int const col, const fl::value::integer& v);
  fl::stmt& bind(int const col, const fl::value::real& v);
  fl::stmt& bind(int const col, const fl::value::null& v);
  fl::stmt& bind(int const col, const fl::value::variant& variant);
  fl::stmt& bind(int const col, const fl::field& field);

  fl::stmt& bind_pointer(int const col, char const* const id, void* ptr);
  fl::stmt& bind_pointer(const std::string& name,
                         char const* const id,
                         void* ptr);

  template<typename Value>
  fl::stmt& bind(const std::string& name, const Value& value)
  {
    int col =
      fl::api(sqlite3_bind_parameter_index, db(), stmt_.get(), name.c_str());

    fl::error::raise_if(0 == col, "Name has no index");

    return bind(col, value);
  }

  template<fl::compatible_type T>
  inline fl::stmt& bind(int const col, const T& value)
  {
    return bind(col, fl::value::from(value));
  }

  fl::stmt& execute();

  template<std::ranges::range T>
  inline fl::stmt& execute(T&& params)
  {
    reset();
    for (auto ix = 1; auto&& param : params) {
      bind(ix++, param);
    }
    execute();
    return *this;
  }

  template<typename... T>
  inline fl::stmt& execute(T&&... param)
  {
    reset();
    int ix = 1;
    (bind(ix++, param), ...);
    execute();
    return *this;
  }

  template<std::ranges::range T>
  inline void executemany(T&& many_params)
  {
    for (auto&& row : many_params) {
      reset();
      for (auto index = 1; auto&& param : row) {
        bind(index++, param);
      }
      execute();
    }
  }

  stmt_iterator begin() { return stmt_iterator(this); }
  stmt_iterator::sentinel end() { return stmt_iterator::sentinel(); }
  bool empty() { return begin() == end(); }
  fl::row current_row() { return *begin(); }

  int bind_parameter_count() const;
  std::optional<std::string> bind_parameter_name(int index) const;
  int bind_parameter_index(const std::string_view& name) const;
  fl::stmt& clear_bindings();
  fl::stmt& reset();

  int column_count() const;
  std::string column_decltype(int const index) const;

  fl::column_view columns();

  std::string expanded_sql() const;
  std::string sql() const;

  bool is_busy() const;
  bool is_explain() const;
  bool is_readonly() const;

  std::shared_ptr<sqlite3_stmt> ptr() { return stmt_; }

protected:
  friend class fl::column_iterator;
  friend class fl::row_iterator;
  friend class fl::stmt_iterator;
  friend class fl::row;
  friend class fl::field;
  friend class fl::column;

  enum class state
  {
    prepared,
    running,
    done
  } state_ = state::prepared;

  sqlite3* db() const;
  std::shared_ptr<sqlite3> db_ = nullptr;
  std::shared_ptr<sqlite3_stmt> stmt_ = nullptr;
};

}

#endif
