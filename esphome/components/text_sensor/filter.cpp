#include "filter.h"
#include "text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace text_sensor {

static const char *const TAG = "text_sensor.filter";

// Filter
void Filter::input(std::string value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%s)", this, value.c_str());
  if (this->new_value(value))
    this->output(value);
}
void Filter::output(std::string &value) {
  if (this->next_ == nullptr) {
    ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> SENSOR", this, value.c_str());
    this->parent_->internal_send_state_to_frontend(value);
  } else {
    ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> %p", this, value.c_str(), this->next_);
    this->next_->input(std::move(value));
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

bool LambdaFilter::new_value(std::string &value) {
  auto result = this->lambda_filter_(value);
  if (result.has_value()) {
    ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%s) -> %s (continue)", this, value.c_str(), result->c_str());
    value = std::move(*result);
    return true;
  }
  ESP_LOGVV(TAG, "LambdaFilter(%p)::new_value(%s) -> (stop)", this, value.c_str());
  return false;
}

// ToUpperFilter
bool ToUpperFilter::new_value(std::string &value) {
  for (char &c : value)
    c = ::toupper(c);
  return true;
}

// ToLowerFilter
bool ToLowerFilter::new_value(std::string &value) {
  for (char &c : value)
    c = ::tolower(c);
  return true;
}

// Append
bool AppendFilter::new_value(std::string &value) {
  value.append(this->suffix_);
  return true;
}

// Prepend
bool PrependFilter::new_value(std::string &value) {
  value.insert(0, this->prefix_);
  return true;
}

// Substitute
SubstituteFilter::SubstituteFilter(const std::initializer_list<Substitution> &substitutions)
    : substitutions_(substitutions) {}

bool SubstituteFilter::new_value(std::string &value) {
  for (const auto &sub : this->substitutions_) {
    // Compute lengths once per substitution (strlen is fast, called infrequently)
    const size_t from_len = strlen(sub.from);
    const size_t to_len = strlen(sub.to);
    std::size_t pos = 0;
    while ((pos = value.find(sub.from, pos, from_len)) != std::string::npos) {
      value.replace(pos, from_len, sub.to, to_len);
      // Advance past the replacement to avoid infinite loop when
      // the replacement contains the search pattern (e.g., f -> foo)
      pos += to_len;
    }
  }
  return true;
}

// Map
MapFilter::MapFilter(const std::initializer_list<Substitution> &mappings) : mappings_(mappings) {}

bool MapFilter::new_value(std::string &value) {
  for (const auto &mapping : this->mappings_) {
    if (value == mapping.from) {
      value.assign(mapping.to);
      return true;
    }
  }
  return true;  // Pass through if no match
}

}  // namespace text_sensor
}  // namespace esphome
