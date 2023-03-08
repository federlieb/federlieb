#include "federlieb/federlieb.hxx"

#include "fx_statistics.hxx"

namespace fl = ::federlieb;

void
fx_median::xStep(double value)
{
  data_.push_back(value);
}

std::optional<double>
fx_median::xFinal()
{

  if (data_.size() < 1) {
    return std::nullopt;
  }

  if (data_.size() % 2 == 0) {
    const auto median_it1 = data_.begin() + data_.size() / 2 - 1;
    const auto median_it2 = data_.begin() + data_.size() / 2;
    std::nth_element(data_.begin(), median_it1 , data_.end());
    std::nth_element(data_.begin(), median_it2 , data_.end());
    return (*median_it1 + *median_it2) / 2;

  } else {
    const auto median_it = data_.begin() + data_.size() / 2;
    std::nth_element(data_.begin(), median_it , data_.end());
    return *median_it;
  }

}

void
fx_median_p2q::xStep(double value)
{
  acc_(value);
}

std::optional<double>
fx_median_p2q::xFinal()
{

  if (boost::accumulators::count(acc_) == 0) {
    return std::nullopt;
  }

  return boost::accumulators::median(acc_);

}

void
fx_variance::xStep(double value)
{
  acc_(value);
}

double
fx_variance::xFinal()
{

  return boost::accumulators::variance(acc_);

}

boost::json::array
fx_utf8_parts::xFunc(uint32_t value)
{
  auto array = boost::json::array();

  if (value < 0x80) {
    array.push_back(value);
    return array;
  }
  
  size_t len = 4;

  if (value < 0x800) {
    len = 2;
  } else if (value < 0x10000) {
    len = 3;
  }

  while (len) {
    array.push_back(value & 63);
    value >>= 6;
    len--;
  }

  return array;
}

