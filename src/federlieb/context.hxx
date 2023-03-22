#ifndef FEDERLIEB_CONTEXT_HXX
#define FEDERLIEB_CONTEXT_HXX

#include <boost/json.hpp>

#include "api.hxx"

#include "federlieb/db.hxx"
#include "federlieb/detail.hxx"
#include "federlieb/value.hxx"

namespace federlieb {

namespace fl = ::federlieb;

class context
{
public:
  context(sqlite3_context* ctx, sqlite3* db = nullptr);

  fl::db db() const;

  template<std::integral T>
  inline void result(const T& value)
  {
    fl::api(sqlite3_result_int64,
            db_,
            ctx_,
            fl::detail::safe_to<sqlite3_int64>(value));
  }

  void result(const double& value);
  void result(const std::string& value);
  void result(const nullptr_t& v);
  void result(const fl::value::blob& v);
  void result(const fl::value::text& v);
  void result(const fl::value::integer& v);
  void result(const fl::value::real& v);
  void result(const fl::value::null& v);
  void result(const fl::value::variant& variant);
  void result(const fl::field& field);

  template<typename T>
  requires requires { boost::json::serialize(std::declval<T>()); }
  inline void result(const T& v)
  {
    auto text = boost::json::serialize(v);
    result(text);
    subtype('J');
  }

  template<typename T>
  inline void result(const std::optional<T>& v)
  {
    if (v.has_value()) {
      result(v.value());
    } else {
      result(nullptr);
    }
  }

  void error(int code) noexcept;
  void error(const std::string& message) noexcept;
  void error_toobig() noexcept;
  void error_nomem() noexcept;

  void subtype(unsigned int subtype);
  void* user_data() const;

  std::shared_ptr<sqlite3_context> ptr();

protected:
  sqlite3_context* ctx_ = nullptr;
  sqlite3* db_ = nullptr;
};

}

#endif
