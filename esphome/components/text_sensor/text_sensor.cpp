#include "text_sensor.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::text_sensor {

ESPHOME_LOG_TAG(TAG, "text_sensor");

void log_text_sensor(const char *tag, const char *prefix, const char *type, TextSensor *obj) {
  if (!log_entity_header(tag, prefix, type, obj)) {
    return;
  }
  LOG_ENTITY_DEVICE_CLASS(tag, prefix, *obj);
  LOG_ENTITY_ICON(tag, prefix, *obj);
}

void TextSensor::publish_state(const char *state, size_t len) {
#ifdef USE_TEXT_SENSOR_FILTER
  if (this->filter_list_ == nullptr) {
#endif
    // No filters: raw_state == state, store once and use for both callbacks
    // Only assign if changed to avoid heap allocation
    if (len != this->state.size() || memcmp(state, this->state.data(), len) != 0) {
      this->state.assign(state, len);
    }
#ifdef USE_TEXT_SENSOR_FILTER
    this->raw_callback_.call(this->state);
#endif
    ESP_LOGV(TAG, "'%s': Received new state %s", LOG_STR_ARG(this->get_log_name()), this->state.c_str());
    this->notify_frontend_();
#ifdef USE_TEXT_SENSOR_FILTER
  } else {
    // Has filters: need separate raw storage
    this->set_raw_state_(state, len);
    ESP_LOGV(TAG, "'%s': Received new state %s", LOG_STR_ARG(this->get_log_name()), this->raw_state_.c_str());
    this->filter_list_->input(this->raw_state_);
  }
#endif
}

#ifdef USE_ESP8266
void TextSensor::publish_state(ProgmemStr state) {
  char buf[256];
  strncpy_P(buf, reinterpret_cast<PGM_P>(state), sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  this->publish_state(buf);
}
#endif

#ifdef USE_TEXT_SENSOR_FILTER
void TextSensor::add_filter(Filter *filter) {
  ESP_LOGVV(TAG, "TextSensor(%p)::add_filter(%p)", this, filter);
  // Link the new filter before the chain can reach it
  filter->initialize(this, nullptr);
  if (this->filter_list_ == nullptr) {
    this->filter_list_ = filter;
  } else {
    this->filter_list_->chain_last_()->initialize(this, filter);
  }
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
#endif  // USE_TEXT_SENSOR_FILTER

void TextSensor::publish_state_unfiltered(const char *state, size_t len) {
#ifdef USE_TEXT_SENSOR_FILTER
  if (this->filter_list_ != nullptr) {
    this->set_raw_state_(state, len);
    // A raw callback may have published again or freed the caller's buffer
    this->internal_send_state_to_frontend(this->raw_state_.data(), this->raw_state_.size());
    return;
  }
#endif
  this->publish_state(state, len);
}

const std::string &TextSensor::get_state() const { return this->state; }
const std::string &TextSensor::get_raw_state() const {
#ifdef USE_TEXT_SENSOR_FILTER
  if (this->filter_list_ != nullptr) {
    return this->raw_state_;
  }
#endif
  return this->state;  // No filters, raw == filtered
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
  ESP_LOGV(TAG, "'%s' >> '%s'", LOG_STR_ARG(this->get_log_name()), this->state.c_str());
  this->callback_.call(this->state);
#if defined(USE_TEXT_SENSOR) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_text_sensor_update(this);
#endif
}

}  // namespace esphome::text_sensor
