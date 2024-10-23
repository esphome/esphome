#include "sample_rate_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.sample_rate_select";

void SampleRateSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_sample_rate(state);
}

}  // namespace ld2415h
}  // namespace esphome
