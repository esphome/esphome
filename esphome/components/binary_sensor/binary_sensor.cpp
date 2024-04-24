#include "binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {

namespace binary_sensor {

static const char *const TAG = "binary_sensor";

void BinarySensor::add_on_state_callback(std::function<void(bool)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void BinarySensor::publish_state(bool state) {
  if (!this->publish_dedup_.next(state))
    return;
  if (this->filter_list_ == nullptr) {
    this->send_state_internal(state, false);
  } else {
    this->filter_list_->input(state, false);
  }
}
void BinarySensor::publish_initial_state(bool state) {
  if (!this->publish_dedup_.next(state))
    return;
  if (this->filter_list_ == nullptr) {
    this->send_state_internal(state, true);
  } else {
    this->filter_list_->input(state, true);
  }
}
void BinarySensor::send_state_internal(bool state, bool is_initial) {
  if (is_initial) {
    ESP_LOGD(TAG, "'%s': Sending initial state %s", this->get_name().c_str(), ONOFF(state));
  } else {
    ESP_LOGD(TAG, "'%s': Sending state %s", this->get_name().c_str(), ONOFF(state));
  }
  this->has_state_ = true;
  this->state = state;
  if (!is_initial || this->publish_initial_state_) {
    this->state_callback_.call(state);
  }
}

BinarySensor::BinarySensor() : state(false) {}

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
void BinarySensor::add_filters(const std::vector<Filter *> &filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
bool BinarySensor::has_state() const { return this->has_state_; }
bool BinarySensor::is_status_binary_sensor() const { return false; }

}  // namespace binary_sensor

}  // namespace esphome
