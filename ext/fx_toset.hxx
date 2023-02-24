#include <map>
#include <set>

#include "federlieb/fx.hxx"
#include "federlieb/json.hxx"

namespace fl = ::federlieb;

class fx_toset : public fl::fx::base<fx_toset>
{
public:
  static inline auto const name = "fl_toset";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  boost::json::array xFunc(const fl::value::variant& value);
};

class fx_toset_agg : public fl::fx::base<fx_toset_agg>
{
public:
  static inline auto const name = "fl_toset_agg";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  void xStep(const fl::value::variant value);
  boost::json::array xFinal();

protected:
  boost::json::array data_;
};

class fx_toset_union : public fl::fx::base<fx_toset_union>
{
public:
  static inline auto const name = "fl_toset_union";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  std::optional<std::string> xFunc(const std::optional<std::string>& lhs,
                                   const std::optional<std::string>& rhs);
};

class fx_toset_intersection : public fl::fx::base<fx_toset_intersection>
{
public:
  static inline auto const name = "fl_toset_intersection";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  std::optional<std::string> xFunc(const std::optional<std::string>& lhs,
                                   const std::optional<std::string>& rhs);
};

class fx_toset_except : public fl::fx::base<fx_toset_except>
{
public:
  static inline auto const name = "fl_toset_except";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  std::optional<std::string> xFunc(const std::optional<std::string>& lhs,
                                   const std::optional<std::string>& rhs);
};

class fx_toset_contains : public fl::fx::base<fx_toset_contains>
{
public:
  static inline auto const name = "fl_toset_contains";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  int xFunc(const std::string haystack, const fl::value::variant needle);
};

class fx_object_set_agg : public fl::fx::base<fx_object_set_agg>
{
public:
  static inline auto const name = "fl_object_set_agg";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  void xStep(const std::string key, const fl::value::variant value);
  boost::json::object xFinal();

protected:
  std::map<std::string, std::set<fl::value::variant>> data_;
};

class fx_stack_push : public fl::fx::base<fx_stack_push>
{
public:
  static inline auto const name = "fl_stack_push";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  std::optional<std::string> xFunc(const std::string&, fl::value::variant value);

};

class fx_stack_pop : public fl::fx::base<fx_stack_pop>
{
public:
  static inline auto const name = "fl_stack_pop";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  std::optional<std::string> xFunc(const std::string& array);

};

class fx_stack_top : public fl::fx::base<fx_stack_top>
{
public:
  static inline auto const name = "fl_stack_top";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  std::optional<std::string> xFunc(const std::string& array);

};
