#include "text_sensor.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace text_sensor {

static const char *const TAG = "text_sensor";

void log_text_sensor(const char *tag, const char *prefix, const char *type, TextSensor *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());

  if (!obj->get_device_class_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Device Class: '%s'", prefix, obj->get_device_class_ref().c_str());
  }

  if (!obj->get_icon_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", prefix, obj->get_icon_ref().c_str());
  }
}

void TextSensor::publish_state(const std::string &state) { this->publish_state(state.data(), state.size()); }

void TextSensor::publish_state(const char *state) { this->publish_state(state, strlen(state)); }

void TextSensor::publish_state(const char *state, size_t len) {
  if (this->filter_list_ == nullptr) {
    // No filters: raw_state == state, store once and use for both callbacks
    // Only assign if changed to avoid heap allocation
    if (len != this->state.size() || memcmp(state, this->state.data(), len) != 0) {
      this->state.assign(state, len);
    }
    this->raw_callback_.call(this->state);
    ESP_LOGV(TAG, "'%s': Received new state %s", this->name_.c_str(), this->state.c_str());
    this->notify_frontend_();
  } else {
    // Has filters: need separate raw storage
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // Only assign if changed to avoid heap allocation
    if (len != this->raw_state.size() || memcmp(state, this->raw_state.data(), len) != 0) {
      this->raw_state.assign(state, len);
    }
    this->raw_callback_.call(this->raw_state);
    ESP_LOGV(TAG, "'%s': Received new state %s", this->name_.c_str(), this->raw_state.c_str());
    this->filter_list_->input(this->raw_state);
#pragma GCC diagnostic pop
  }
}

void TextSensor::add_filter(Filter *filter) {
  // inefficient, but only happens once on every sensor setup and nobody's going to have massive amounts of
  // filters
  ESP_LOGVV(TAG, "TextSensor(%p)::add_filter(%p)", this, filter);
  if (this->filter_list_ == nullptr) {
    this->filter_list_ = filter;
  } else {
    Filter *last_filter = this->filter_list_;
    while (last_filter->next_ != nullptr)
      last_filter = last_filter->next_;
    last_filter->initialize(this, filter);
  }
  filter->initialize(this, nullptr);
}
void TextSensor::add_filters(std::initializer_list<Filter *> filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
void TextSensor::set_filters(std::initializer_list<Filter *> filters) {
  this->clear_filters();
  this->add_filters(filters);
}
void TextSensor::clear_filters() {
  if (this->filter_list_ != nullptr) {
    ESP_LOGVV(TAG, "TextSensor(%p)::clear_filters()", this);
  }
  this->filter_list_ = nullptr;
}

void TextSensor::add_on_state_callback(std::function<void(const std::string &)> callback) {
  this->callback_.add(std::move(callback));
}
void TextSensor::add_on_raw_state_callback(std::function<void(const std::string &)> callback) {
  this->raw_callback_.add(std::move(callback));
}

const std::string &TextSensor::get_state() const { return this->state; }
const std::string &TextSensor::get_raw_state() const {
  if (this->filter_list_ == nullptr) {
    return this->state;  // No filters, raw == filtered
  }
// Suppress deprecation warning - get_raw_state() is the replacement API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return this->raw_state;
#pragma GCC diagnostic pop
}
void TextSensor::internal_send_state_to_frontend(const std::string &state) {
  this->internal_send_state_to_frontend(state.data(), state.size());
}

void TextSensor::internal_send_state_to_frontend(const char *state, size_t len) {
  // Only assign if changed to avoid heap allocation
  if (len != this->state.size() || memcmp(state, this->state.data(), len) != 0) {
    this->state.assign(state, len);
  }
  this->notify_frontend_();
}

void TextSensor::notify_frontend_() {
  this->set_has_state(true);
  ESP_LOGD(TAG, "'%s' >> '%s'", this->name_.c_str(), this->state.c_str());
  this->callback_.call(this->state);
#if defined(USE_TEXT_SENSOR) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_text_sensor_update(this);
#endif
}

}  // namespace text_sensor
}  // namespace esphome
