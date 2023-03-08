#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/variance.hpp>

#include "federlieb/fx.hxx"

namespace fl = ::federlieb;

class fx_median : public fl::fx::base<fx_median>
{
public:
  static inline auto const name = "fl_median";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  void xStep(const double value);
  std::optional<double> xFinal();

protected:
  std::vector<double> data_;
};

class fx_median_p2q : public fl::fx::base<fx_median_p2q>
{
public:
  static inline auto const name = "fl_median_p2q";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  void xStep(const double value);
  std::optional<double> xFinal();

protected:

    boost::accumulators::accumulator_set<
      double,
      boost::accumulators::features<
        boost::accumulators::tag::count,
        boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile)> > acc_;

};

class fx_variance : public fl::fx::base<fx_variance>
{
public:
  static inline auto const name = "fl_variance";
  static inline auto const deterministic = false;
  static inline auto const direct_only = false;

  void xStep(const double value);
  double xFinal();

protected:

    boost::accumulators::accumulator_set<
      double,
      boost::accumulators::features<
        // boost::accumulators::tag::count,
        boost::accumulators::tag::variance> > acc_;

};




// TODO: Move this
class fx_utf8_parts : public fl::fx::base<fx_utf8_parts>
{
public:
  static inline auto const name = "fl_utf8_parts";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  boost::json::array xFunc(uint32_t value);
};

#pragma GCC diagnostic pop
