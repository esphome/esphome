#include "filter.h"
#include "text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace text_sensor {

static const char *const TAG = "text_sensor.filter";

// Filter
void Filter::input(const std::string &value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%s)", this, value.c_str());
  optional<std::string> out = this->new_value(value);
  if (out.has_value())
    this->output(*out);
}
void Filter::output(const std::string &value) {
  if (this->next_ == nullptr) {
    ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> SENSOR", this, value.c_str());
    this->parent_->internal_send_state_to_frontend(value);
  } else {
    ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> %p", this, value.c_str(), this->next_);
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
  ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%s) -> %s", this, value.c_str(), it.value_or("").c_str());
  return it;
}

// ToUpperFilter
optional<std::string> ToUpperFilter::new_value(std::string value) {
  for (char &c : value)
    c = ::toupper(c);
  return value;
}

// ToLowerFilter
optional<std::string> ToLowerFilter::new_value(std::string value) {
  for (char &c : value)
    c = ::tolower(c);
  return value;
}

// Append
optional<std::string> AppendFilter::new_value(std::string value) { return value + this->suffix_; }

// Prepend
optional<std::string> PrependFilter::new_value(std::string value) { return this->prefix_ + value; }

// Substitute
optional<std::string> SubstituteFilter::new_value(std::string value) {
  std::size_t pos;
  for (size_t i = 0; i < this->from_strings_.size(); i++) {
    while ((pos = value.find(this->from_strings_[i])) != std::string::npos)
      value.replace(pos, this->from_strings_[i].size(), this->to_strings_[i]);
  }
  return value;
}

// Map
optional<std::string> MapFilter::new_value(std::string value) {
  auto item = mappings_.find(value);
  return item == mappings_.end() ? value : item->second;
}

}  // namespace text_sensor
}  // namespace esphome
