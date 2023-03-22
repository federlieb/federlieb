#include "federlieb/context.hxx"
#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

fl::context::context(sqlite3_context* ctx, sqlite3* db)
{
  ctx_ = ctx;

  if (db == nullptr) {
    db_ = sqlite3_context_db_handle(ctx_);
  } else {
    db_ = db;
  }
}

fl::db
fl::context::db() const
{
  return fl::db(std::shared_ptr<sqlite3>(db_, [](auto&&) {}));
}

void
fl::context::error(int const code) noexcept
{
  fl::api(sqlite3_result_error_code, db_, ctx_, code);
}

void
fl::context::error(const std::string& message) noexcept
{
  fl::api(sqlite3_result_error, db_, ctx_, message.c_str(), -1);
}

void
fl::context::error_toobig() noexcept
{
  fl::api(sqlite3_result_error_toobig, db_, ctx_);
}

void
fl::context::error_nomem() noexcept
{
  fl::api(sqlite3_result_error_nomem, db_, ctx_);
}

void
fl::context::subtype(unsigned int const subtype)
{
  fl::api(sqlite3_result_subtype, db_, ctx_, subtype);
}

void*
fl::context::user_data() const
{
  return fl::api(sqlite3_user_data, db_, ctx_);
}

void
fl::context::result(const double& value)
{
  fl::api(sqlite3_result_double, db_, ctx_, value);
}

void
fl::context::result(const std::string& value)
{
  fl::api(sqlite3_result_text64,
          db_,
          ctx_,
          value.c_str(),
          fl::detail::safe_to<sqlite_int64>(value.size()),
          SQLITE_TRANSIENT,
          SQLITE_UTF8);
}

void
fl::context::result(const nullptr_t& v)
{
  fl::api(sqlite3_result_null, db_, ctx_);
}

void
fl::context::result(const fl::value::blob& v)
{
  fl::api(sqlite3_result_blob64,
          db_,
          ctx_,
          (&v.value[0]),
          fl::detail::safe_to<int>(v.value.size()),
          SQLITE_TRANSIENT);
}

void
fl::context::result(const fl::value::text& v)
{
  fl::api(sqlite3_result_text64,
          db_,
          ctx_,
          v.value.c_str(),
          fl::detail::safe_to<int>(v.value.size()),
          SQLITE_TRANSIENT,
          SQLITE_UTF8);
}

void
fl::context::result(const fl::value::integer& v)
{
  fl::api(sqlite3_result_int64, db_, ctx_, v.value);
}

void
fl::context::result(const fl::value::real& v)
{
  fl::api(sqlite3_result_double, db_, ctx_, v.value);
}

void
fl::context::result(const fl::value::null& v)
{
  fl::api(sqlite3_result_null, db_, ctx_);
}

void
fl::context::result(const fl::value::variant& variant)
{
  std::visit([this](auto&& value) { result(value); }, variant);
}

void
fl::context::result(const fl::field& field)
{
  fl::api(sqlite3_result_value, db_, ctx_, field.unprotected_value());
}

std::shared_ptr<sqlite3_context>
fl::context::ptr()
{
  return std::shared_ptr<sqlite3_context>(ctx_, [](void*) {});
}
