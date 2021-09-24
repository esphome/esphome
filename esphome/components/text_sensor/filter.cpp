#include "filter.h"
#include "text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace text_sensor {

static const char *const TAG = "sensor.filter";

// Filter
void Filter::input(std::string value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%f)", this, value);
  optional<std::string> out = this->new_value(value);
  if (out.has_value())
    this->output(*out);
}
void Filter::output(std::string value) {
  if (this->next_ == nullptr) {
    ESP_LOGVV(TAG, "Filter(%p)::output(%f) -> SENSOR", this, value);
    this->parent_->internal_send_state_to_frontend(value);
  } else {
    ESP_LOGVV(TAG, "Filter(%p)::output(%f) -> %p", this, value, this->next_);
    this->next_->input(value);
  }
}
void Filter::initialize(TextSensor *parent, Filter *next) {
  ESP_LOGVV(TAG, "Filter(%p)::initialize(parent=%p next=%p)", this, parent, next);
  this->parent_ = parent;
  this->next_ = next;
}

// LambdaFilter
LambdaFilter::LambdaFilter(lambda_filter_t lambda_filter) : lambda_filter_(std::move(lambda_filter)) {}
const lambda_filter_t &LambdaFilter::get_lambda_filter() const { return this->lambda_filter_; }
void LambdaFilter::set_lambda_filter(const lambda_filter_t &lambda_filter) { this->lambda_filter_ = lambda_filter; }

optional<std::string> LambdaFilter::new_value(std::string value) {
  auto it = this->lambda_filter_(value);
  ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%f) -> %f", this, value, it.value_or(INFINITY));
  return it;
}

// ToUpperFilter
ToUpperFilter::ToUpperFilter(std::string dummy){};
optional<std::string> ToUpperFilter::new_value(std::string value) {
  std::for_each(value.begin(), value.end(), [](char &c) { c = ::toupper(c); });
  return value;
}

}  // namespace text_sensor
}  // namespace esphome
