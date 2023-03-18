#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

fl::field::operator fl::value::variant()
{
  return to_variant();
}

int
fl::field::type() const
{
  return fl::api(sqlite3_column_type, db(), row_.stmt_->stmt_.get(), index_);
}

std::string
fl::field::name() const
{
  auto name =
    fl::api(sqlite3_column_name, db(), row_.stmt_->stmt_.get(), index_);

  return name;
}

sqlite3*
fl::field::db() const
{
  return row_.stmt_->db();
}

sqlite3_int64
fl::field::to_integer() const
{
  fl::error::raise_if(type() != SQLITE_INTEGER, "not an INTEGER field (type " + std::to_string(type()) + ", index " + std::to_string(index_) + ")");
  return fl::api(sqlite3_column_int64, db(), row_.stmt_->stmt_.get(), index_);
}

double
fl::field::to_float() const
{
  fl::error::raise_if(type() != SQLITE_FLOAT, "not a REAL field");
  return fl::api(sqlite3_column_double, db(), row_.stmt_->stmt_.get(), index_);
}

std::string
fl::field::to_text() const
{
  fl::error::raise_if(type() != SQLITE_TEXT, "not a TEXT field");

  auto&& data = (const char*)fl::api(
    sqlite3_column_text, db(), row_.stmt_->stmt_.get(), index_);
  auto&& length =
    fl::api(sqlite3_column_bytes, db(), row_.stmt_->stmt_.get(), index_);

  return std::string(data, length);
}

fl::blob_type
fl::field::to_blob() const
{
  fl::error::raise_if(type() != SQLITE_BLOB, "not a BLOB field");

  auto data = static_cast<blob_type::value_type const*>(
    fl::api(sqlite3_column_blob, db(), row_.stmt_->stmt_.get(), index_));
  auto length =
    fl::api(sqlite3_column_bytes, db(), row_.stmt_->stmt_.get(), index_);

  return fl::blob_type(data, data + length);
}

fl::value::variant
fl::field::to_variant() const
{
  switch (type()) {
    case SQLITE_INTEGER:
      return fl::value::integer{ to_integer() };
    case SQLITE_FLOAT:
      return fl::value::real{ to_float() };
    case SQLITE_TEXT:
      return fl::value::text{ to_text() };
    case SQLITE_BLOB:
      return fl::value::blob{ to_blob() };
    case SQLITE_NULL:
      return fl::value::null{};
    default:
      fl::error::raise("unknown type");
  }
}

void
fl::field::to(fl::value::variant& sink) const
{
  sink = to_variant();
}

void
fl::field::to(double& sink) const
{
  sink = to_float();
}

void
fl::field::to(std::string& sink) const
{
  sink = to_text();
}

void
fl::field::to(fl::blob_type& sink) const
{
  sink = to_blob();
}

sqlite3_value*
fl::field::unprotected_value() const
{
  return fl::api(sqlite3_column_value, db(), row_.stmt_->stmt_.get(), index_);
}
