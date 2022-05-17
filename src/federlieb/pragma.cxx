#include "federlieb/federlieb.hxx"

namespace fl = ::federlieb;

std::vector<fl::pragma::table_list_data>
fl::pragma::table_list(fl::db& db)
{
  auto stmt = db.prepare("PRAGMA table_list");
  stmt.execute();
  return fl::detail::to_vector(stmt | fl::as<table_list_data>());
}

std::vector<fl::pragma::table_xinfo_data>
fl::pragma::table_xinfo(fl::db& db,
                        const std::string& schema,
                        const std::string& name)
{

  auto info_stmt = db.prepare(fl::detail::sprintf(
    R"(PRAGMA "%w".table_xinfo("%w"))", schema.c_str(), name.c_str()));

  info_stmt.execute();

  return fl::detail::to_vector(info_stmt | fl::as<table_xinfo_data>());
}

std::vector<fl::pragma::table_xinfo_data>
fl::pragma::table_xinfo(fl::db& db, const std::string& name)
{
  return table_xinfo(db, "main", name);
}
