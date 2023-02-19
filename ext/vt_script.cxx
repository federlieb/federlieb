#include <set>
#include <stack>

#include "federlieb/federlieb.hxx"
#include "vt_script.hxx"

namespace fl = ::federlieb;


vt_script::cursor::cursor(vt_script* vtab)
  
{
}

void
vt_script::xConnect(bool create)
{

  declare(R"SQL(
    CREATE TABLE fl_script(
      execute ANY [BLOB]
    )
  )SQL");
}

void unparenthesize(std::string& s) {
  auto lp = s.find_first_of("(");

  if (std::string::npos != lp) {
      s.erase(0, lp + 1);
  }

  auto rp = s.find_last_of(")");

  if (std::string::npos != rp) {
      s.erase(rp);
  }
}

fl::stmt
vt_script::xFilter(const fl::vtab::index_info& info, cursor* cursor)
{
  auto script = arguments().front();
  unparenthesize(script);
  db().execute_script(script);
  return db().prepare("SELECT 1").execute();
}
