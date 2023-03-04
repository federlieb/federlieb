#include "federlieb/federlieb.hxx"

#include "fx_median.hxx"

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
