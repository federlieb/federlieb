#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

int
fl::stmt::bind_parameter_count() const
{
  return fl::api(sqlite3_bind_parameter_count, db(), stmt_.get());
};

std::optional<std::string>
fl::stmt::bind_parameter_name(int index) const
{
  auto name = fl::api(sqlite3_bind_parameter_name, db(), stmt_.get(), index);

  if (nullptr == name) {
    return std::nullopt;
  }

  return name;
};

int
fl::stmt::bind_parameter_index(const std::string_view& name) const
{
  return fl::api(sqlite3_bind_parameter_index, db(), stmt_.get(), name.data());
};

fl::stmt&
fl::stmt::clear_bindings()
{
  fl::api(sqlite3_clear_bindings, { SQLITE_OK }, db(), stmt_.get());
  return *this;
};

fl::stmt&
fl::stmt::bind(int const col, const nullptr_t& v)
{
  fl::api(sqlite3_bind_null, { SQLITE_OK }, db(), stmt_.get(), col);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::blob& v)
{
  fl::api(sqlite3_bind_blob64,
          { SQLITE_OK },
          db(),
          stmt_.get(),
          col,
          (&v.value[0]),
          fl::detail::safe_to<int>(v.value.size()),
          SQLITE_TRANSIENT);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::text& v)
{
  fl::api(sqlite3_bind_text,
          { SQLITE_OK },
          db(),
          stmt_.get(),
          col,
          v.value.c_str(),
          fl::detail::safe_to<int>(v.value.size()),
          SQLITE_TRANSIENT);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::json& v)
{
  return bind(col, fl::value::text{ v.value });
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::integer& v)
{
  fl::api(sqlite3_bind_int64, { SQLITE_OK }, db(), stmt_.get(), col, v.value);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::real& v)
{
  fl::api(sqlite3_bind_double, { SQLITE_OK }, db(), stmt_.get(), col, v.value);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::null& v)
{
  fl::api(sqlite3_bind_null, { SQLITE_OK }, db(), stmt_.get(), col);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::value::variant& variant)
{
  std::visit([this, &col](auto&& value) { bind(col, value); }, variant);
  return *this;
}

fl::stmt&
fl::stmt::bind(int const col, const fl::field& field)
{
  fl::api(sqlite3_bind_value,
          { SQLITE_OK },
          db(),
          stmt_.get(),
          col,
          field.unprotected_value());
  return *this;
}

fl::stmt&
fl::stmt::bind_pointer(int const col, char const* const id, void* ptr)
{
  fl::api(sqlite3_bind_pointer,
          { SQLITE_OK },
          db(),
          stmt_.get(),
          col,
          ptr,
          id,
          nullptr);
  return *this;
}

fl::stmt&
fl::stmt::bind_pointer(const std::string& name, char const* const id, void* ptr)
{
  int col =
    fl::api(sqlite3_bind_parameter_index, db(), stmt_.get(), name.c_str());

  fl::error::raise_if(0 == col, "Name has no index");

  return bind_pointer(col, id, ptr);
}

fl::stmt&
fl::stmt::reset()
{
  fl::api(sqlite3_reset, { SQLITE_OK }, db(), stmt_.get());
  return *this;
};

int
fl::stmt::column_count() const
{
  return fl::api(sqlite3_column_count, db(), stmt_.get());
};

bool
fl::stmt::is_busy() const
{
  return fl::api(sqlite3_stmt_busy, db(), stmt_.get());
};

bool
fl::stmt::is_explain() const
{
  return fl::api(sqlite3_stmt_isexplain, db(), stmt_.get());
};

bool
fl::stmt::is_readonly() const
{
  return fl::api(sqlite3_stmt_readonly, db(), stmt_.get());
};

std::string
fl::stmt::column_decltype(int const index) const
{
  auto data = fl::api(sqlite3_column_decltype, db(), stmt_.get(), index);
  fl::error::raise_if(nullptr == data, "bad index");
  return data;
};

std::string
fl::stmt::expanded_sql() const
{
  auto data = fl::api(sqlite3_expanded_sql, db(), stmt_.get());
  fl::error::raise_if(nullptr == data, "memory error or length limit");
  return data;
};

std::string
fl::stmt::sql() const
{
  auto data = fl::api(sqlite3_sql, db(), stmt_.get());
  fl::error::raise_if(nullptr == data, "memory error or length limit");
  return data;
};

sqlite3*
fl::stmt::db() const
{
  if (db_) {
    return db_.get();
  }

  return fl::api(sqlite3_db_handle, nullptr, stmt_.get());
}

fl::stmt&
fl::stmt::execute()
{
  reset();

  int rc =
    fl::api(sqlite3_step, { SQLITE_DONE, SQLITE_ROW }, db(), stmt_.get());

  if (rc == SQLITE_DONE) {
    state_ = state::done;
  } else if (rc == SQLITE_ROW) {
    state_ = state::running;
  }

  return *this;
}

fl::column_view
fl::stmt::columns()
{
  return fl::column_view(this);
}

fl::stmt_iterator::stmt_iterator(fl::stmt* stmt)
  : stmt_(stmt)
{
  if (nullptr == stmt) {
    fl::error::raise("Neeeds a statement");
  }

  if (stmt_->state_ == fl::stmt::state::prepared) {
    fl::error::raise("Statement not executed");
  }
}

bool
fl::stmt_iterator::operator==(const fl::stmt_iterator::sentinel&) const
{
  return stmt_->state_ == fl::stmt::state::done;
}

fl::row
fl::stmt_iterator::operator*() const
{
  return fl::row(stmt_);
};

fl::stmt_iterator&
fl::stmt_iterator::operator++() noexcept
{
  if (stmt_->state_ == fl::stmt::state::done) {
    fl::error::raise("Statement already finished");
  }

  int rc = fl::api(
    sqlite3_step, { SQLITE_DONE, SQLITE_ROW }, db(), stmt_->stmt_.get());

  if (SQLITE_DONE == rc) {
    stmt_->state_ = fl::stmt::state::done;
  }

  return *this;
}

sqlite3*
fl::stmt_iterator::db() const
{
  return stmt_->db();
}

int fl::scanstat::index() {
  return idx_;
}

fl::scanstat::scanstat(fl::stmt* stmt, int index) {
  stmt_ = stmt;
  idx_ = index;
}

sqlite3_int64 fl::scanstat::nloop() {
  sqlite3_int64 value = -1;
  // int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_NLOOP, static_cast<void*>(&value));
  return value;
}

sqlite3_int64 fl::scanstat::nvisit() {
  sqlite3_int64 value = -1;
  // int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_NVISIT, static_cast<void*>(&value));
  return value;
}

double fl::scanstat::est() {
  double value = -1;
  // int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_EST, static_cast<void*>(&value));
  return value;
}

std::optional<std::string> fl::scanstat::name() {
  const char* value = nullptr;
  // int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_SELECTID, static_cast<void*>(&value));
  return value;
}

std::optional<std::string> fl::scanstat::explain() {
  const char* value = nullptr;
  // int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_SELECTID, static_cast<void*>(&value));
  return value;
}

int fl::scanstat::selectid() {
  int value = -1;
  // int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_SELECTID, static_cast<void*>(&value));
  return value;
}

int fl::scanstat::parentid() {
  int value = -1;
  #if 0
  int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_PARENTID, static_cast<void*>(&value));
  #endif
  return value;
}

sqlite3_int64 fl::scanstat::ncycle() {
  sqlite3_int64 value = -1;
  #if 0
  int rc = fl::api(sqlite3_stmt_scanstatus, { SQLITE_OK }, db(), stmt_->stmt_.get(), index(), SQLITE_SCANSTAT_NCYCLE, static_cast<void*>(&value));
  #endif
  return value;

}

static_assert(std::movable<fl::stmt_iterator>);
static_assert(std::input_iterator<fl::stmt_iterator>);
static_assert(std::ranges::range<fl::stmt>);
