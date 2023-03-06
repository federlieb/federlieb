#include "federlieb/federlieb.hxx"

#include "fx_only_value.hxx"

namespace fl = ::federlieb;

void
fx_only_value::xStep(const fl::value::variant value)
{
  if (failure_) {
    return;
  }

  if (data_) {
    if (data_ != value) {
      data_.reset();
      failure_ = true;
    }
  } else {
    data_ = value;
  }

}

fl::value::variant
fx_only_value::xFinal()
{
  if (failure_) {
    return fl::value::null{};
  }

  return *data_;
}
