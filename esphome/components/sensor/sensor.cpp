#include "sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sensor {

static const char *TAG = "sensor";

void Sensor::publish_state(float state) {
  this->raw_state = state;
  this->raw_callback_.call(state);

  ESP_LOGV(TAG, "'%s': Received new state %f", this->name_.c_str(), state);

  if (this->filter_list_ == nullptr) {
    this->internal_send_state_to_frontend(state);
  } else {
    this->filter_list_->input(state);
  }
}
void Sensor::push_new_value(float state) { this->publish_state(state); }
std::string Sensor::unit_of_measurement() { return ""; }
std::string Sensor::icon() { return ""; }
uint32_t Sensor::update_interval() { return 0; }
int8_t Sensor::accuracy_decimals() { return 0; }
Sensor::Sensor(const std::string &name) : Nameable(name), state(NAN), raw_state(NAN) {}
Sensor::Sensor() : Sensor("") {}

void Sensor::set_unit_of_measurement(const std::string &unit_of_measurement) {
  this->unit_of_measurement_ = unit_of_measurement;
}
void Sensor::set_icon(const std::string &icon) { this->icon_ = icon; }
void Sensor::set_accuracy_decimals(int8_t accuracy_decimals) { this->accuracy_decimals_ = accuracy_decimals; }
void Sensor::add_on_state_callback(std::function<void(float)> &&callback) { this->callback_.add(std::move(callback)); }
void Sensor::add_on_raw_state_callback(std::function<void(float)> &&callback) {
  this->raw_callback_.add(std::move(callback));
}
std::string Sensor::get_icon() {
  if (this->icon_.has_value())
    return *this->icon_;
  return this->icon();
}
void Sensor::set_device_class(const std::string &device_class) { this->device_class_ = device_class; }
std::string Sensor::get_device_class() {
  if (this->device_class_.has_value())
    return *this->device_class_;
  return this->device_class();
}
std::string Sensor::device_class() { return ""; }
std::string Sensor::get_unit_of_measurement() {
  if (this->unit_of_measurement_.has_value())
    return *this->unit_of_measurement_;
  return this->unit_of_measurement();
}
int8_t Sensor::get_accuracy_decimals() {
  if (this->accuracy_decimals_.has_value())
    return *this->accuracy_decimals_;
  return this->accuracy_decimals();
}
void Sensor::add_filter(Filter *filter) {
  // inefficient, but only happens once on every sensor setup and nobody's going to have massive amounts of
  // filters
  ESP_LOGVV(TAG, "Sensor(%p)::add_filter(%p)", this, filter);
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
void Sensor::add_filters(const std::vector<Filter *> &filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
void Sensor::set_filters(const std::vector<Filter *> &filters) {
  this->clear_filters();
  this->add_filters(filters);
}
void Sensor::clear_filters() {
  if (this->filter_list_ != nullptr) {
    ESP_LOGVV(TAG, "Sensor(%p)::clear_filters()", this);
  }
  this->filter_list_ = nullptr;
}
float Sensor::get_value() const { return this->state; }
float Sensor::get_state() const { return this->state; }
float Sensor::get_raw_value() const { return this->raw_state; }
float Sensor::get_raw_state() const { return this->raw_state; }
std::string Sensor::unique_id() { return ""; }

void Sensor::internal_send_state_to_frontend(float state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %.5f %s with %d decimals of accuracy", this->get_name().c_str(), state,
           this->get_unit_of_measurement().c_str(), this->get_accuracy_decimals());
  this->callback_.call(state);
}
bool Sensor::has_state() const { return this->has_state_; }
uint32_t Sensor::calculate_expected_filter_update_interval() {
  uint32_t interval = this->update_interval();
  if (interval == 4294967295UL)
    // update_interval: never
    return 0;

  if (this->filter_list_ == nullptr) {
    return interval;
  }

  return this->filter_list_->calculate_remaining_interval(interval);
}
uint32_t Sensor::hash_base() { return 2455723294UL; }

PollingSensorComponent::PollingSensorComponent(const std::string &name, uint32_t update_interval)
    : PollingComponent(update_interval), Sensor(name) {}

uint32_t PollingSensorComponent::update_interval() { return this->get_update_interval(); }

}  // namespace sensor
}  // namespace esphome
