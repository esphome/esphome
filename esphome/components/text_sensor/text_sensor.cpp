#include "text_sensor.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

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

void TextSensor::publish_state(const std::string &state) {
// Suppress deprecation warning - we need to populate raw_state for backwards compatibility
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  this->raw_state = state;
#pragma GCC diagnostic pop
  if (this->raw_callback_) {
    this->raw_callback_->call(state);
  }

  ESP_LOGV(TAG, "'%s': Received new state %s", this->name_.c_str(), state.c_str());

  if (this->filter_list_ == nullptr) {
    this->internal_send_state_to_frontend(state);
  } else {
    this->filter_list_->input(state);
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

void TextSensor::add_on_state_callback(std::function<void(std::string)> callback) {
  this->callback_.add(std::move(callback));
}
void TextSensor::add_on_raw_state_callback(std::function<void(std::string)> callback) {
  if (!this->raw_callback_) {
    this->raw_callback_ = make_unique<CallbackManager<void(std::string)>>();
  }
  this->raw_callback_->add(std::move(callback));
}

std::string TextSensor::get_state() const { return this->state; }
std::string TextSensor::get_raw_state() const {
// Suppress deprecation warning - get_raw_state() is the replacement API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return this->raw_state;
#pragma GCC diagnostic pop
}
void TextSensor::internal_send_state_to_frontend(const std::string &state) {
  this->state = state;
  this->set_has_state(true);
  ESP_LOGD(TAG, "'%s': Sending state '%s'", this->name_.c_str(), state.c_str());
  this->callback_.call(state);
#if defined(USE_TEXT_SENSOR) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_text_sensor_update(this);
#endif
}

}  // namespace text_sensor
}  // namespace esphome
