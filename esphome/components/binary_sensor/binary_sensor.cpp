#include "binary_sensor.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome::binary_sensor {

static const char *const TAG = "binary_sensor";

// Function implementation of LOG_BINARY_SENSOR macro to reduce code size
void log_binary_sensor(const char *tag, const char *prefix, const char *type, BinarySensor *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());
  LOG_ENTITY_DEVICE_CLASS(tag, prefix, *obj);
}

void BinarySensor::publish_state(bool new_state) {
#ifdef USE_BINARY_SENSOR_FILTER
  if (this->filter_list_ == nullptr) {
#endif
    this->send_state_internal(new_state);
#ifdef USE_BINARY_SENSOR_FILTER
  } else {
    this->filter_list_->input(new_state);
  }
#endif
}
void BinarySensor::publish_initial_state(bool new_state) {
  this->invalidate_state();
  this->publish_state(new_state);
}
bool BinarySensor::set_new_state(const optional<bool> &new_state) {
  if (StatefulEntityBase::set_new_state(new_state)) {
    // weirdly, this file could be compiled even without USE_BINARY_SENSOR defined
#if defined(USE_BINARY_SENSOR) && defined(USE_CONTROLLER_REGISTRY)
    ControllerRegistry::notify_binary_sensor_update(this);
#endif
    ESP_LOGV(TAG, "'%s' >> %s", this->get_name().c_str(), ONOFFMAYBE(new_state));
    return true;
  }
  return false;
}

#ifdef USE_BINARY_SENSOR_FILTER
void BinarySensor::add_filter(Filter *filter) {
  filter->parent_ = this;
  if (this->filter_list_ == nullptr) {
    this->filter_list_ = filter;
  } else {
    Filter *last_filter = this->filter_list_;
    while (last_filter->next_ != nullptr)
      last_filter = last_filter->next_;
    last_filter->next_ = filter;
  }
}
void BinarySensor::add_filters(std::initializer_list<Filter *> filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
#endif  // USE_BINARY_SENSOR_FILTER
bool BinarySensor::is_status_binary_sensor() const { return false; }

}  // namespace esphome::binary_sensor
