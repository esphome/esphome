#include "filter.h"
#include "prometheus_handler.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace prometheus {

static const char *const TAG = "prometheus.filter";

// Filter
std::string Filter::input(const std::string &value) {
  ESP_LOGVV(TAG, "Filter(%p)::input(%s)", this, value.c_str());
  optional<std::string> out = this->new_value(value);
  ESP_LOGVV(TAG, "Filter(%p)::output(%s) -> SENSOR", this, value.c_str());
  if (out.has_value()) {
    return *out;
  } else {
    return value;
  }
}
void Filter::initialize(PrometheusHandler *parent) {
  ESP_LOGVV(TAG, "Filter(%p)::initialize(parent=%p)", this, parent);
  this->parent_ = parent;
}

// Map
optional<std::string> MapFilter::new_value(std::string value) {
  auto item = mappings_.find(value);
  return item == mappings_.end() ? value : item->second;
}

}  // namespace prometheus
}  // namespace esphome
