#include "binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {

namespace binary_sensor {

static const char *const TAG = "binary_sensor";

void BinarySensor::publish_state(bool new_state) {
  if (this->filter_list_ == nullptr) {
    this->send_state_internal(new_state);
  } else {
    this->filter_list_->input(new_state);
  }
}
void BinarySensor::publish_initial_state(bool new_state) {
  this->invalidate_state();
  this->publish_state(new_state);
}
void BinarySensor::send_state_internal(bool new_state) {
  // copy the new state to the visible property for backwards compatibility, before any callbacks
  this->state = new_state;
  // Note that set_state_ de-dups and will only trigger callbacks if the state has actually changed
  if (this->set_state_(new_state)) {
    ESP_LOGD(TAG, "'%s': New state is %s", this->get_name().c_str(), ONOFF(new_state));
  }
}

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
bool BinarySensor::is_status_binary_sensor() const { return false; }

}  // namespace binary_sensor

}  // namespace esphome
