#include "federlieb/federlieb.hxx"

#include "fx_ordered_concat_agg.hxx"

namespace fl = ::federlieb;

void
fx_ordered_concat_agg::xStep(const std::string& value,
                             const fl::value::variant& sort_key,
                             const std::string& separator)
{
  data_.push_back(std::make_tuple(value, sort_key, separator));
}

std::string
fx_ordered_concat_agg::xFinal()
{

  fl::error::raise_if(data_.size() < 1, "xFinal on empty aggregate");

  std::ranges::stable_sort(data_, {}, [](auto& e) { return std::get<1>(e); });

  auto joined =
    data_ | std::views::drop(1) | std::views::transform([](auto& e) {
      return std::get<2>(e) + std::get<0>(e);
    });

  return std::get<0>(data_[0]) + fl::detail::str(joined);
}
