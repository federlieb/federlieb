#ifndef FEDERLIEB_PRAGMA_HXX
#define FEDERLIEB_PRAGMA_HXX

#include <vector>

namespace federlieb::pragma {

namespace fl = ::federlieb;

struct table_list_data
{
  std::string schema;
  std::string name;
  std::string type;
  uint64_t ncol;
  int without_rowid;
  int strict;
};

struct table_xinfo_data
{
  uint64_t cid;
  std::string name;
  std::string type;
  int64_t notnull;
  fl::value::variant dflt_value;
  int64_t pk;
  int64_t hidden;
};

std::vector<table_list_data>
table_list(fl::db& db);

std::vector<table_xinfo_data>
table_xinfo(fl::db& db, const std::string& schema, const std::string& name);

std::vector<table_xinfo_data>
table_xinfo(fl::db& db, const std::string& name);

}

#endif
