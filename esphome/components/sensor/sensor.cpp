#include "sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sensor {

static const char *const TAG = "sensor";

std::string state_class_to_string(StateClass state_class) {
  switch (state_class) {
    case STATE_CLASS_MEASUREMENT:
      return "measurement";
    case STATE_CLASS_TOTAL_INCREASING:
      return "total_increasing";
    case STATE_CLASS_NONE:
    default:
      return "";
  }
}

Sensor::Sensor(const std::string &name) : EntityBase(name), state(NAN), raw_state(NAN) {}
Sensor::Sensor() : Sensor("") {}

std::string Sensor::get_unit_of_measurement() {
  if (this->unit_of_measurement_.has_value())
    return *this->unit_of_measurement_;
  return this->unit_of_measurement();
}
void Sensor::set_unit_of_measurement(const std::string &unit_of_measurement) {
  this->unit_of_measurement_ = unit_of_measurement;
}
std::string Sensor::unit_of_measurement() { return ""; }

int8_t Sensor::get_accuracy_decimals() {
  if (this->accuracy_decimals_.has_value())
    return *this->accuracy_decimals_;
  return this->accuracy_decimals();
}
void Sensor::set_accuracy_decimals(int8_t accuracy_decimals) { this->accuracy_decimals_ = accuracy_decimals; }
int8_t Sensor::accuracy_decimals() { return 0; }

std::string Sensor::get_device_class() {
  if (this->device_class_.has_value())
    return *this->device_class_;
  return this->device_class();
}
void Sensor::set_device_class(const std::string &device_class) { this->device_class_ = device_class; }
std::string Sensor::device_class() { return ""; }

void Sensor::set_state_class(StateClass state_class) { this->state_class_ = state_class; }
StateClass Sensor::get_state_class() {
  if (this->state_class_.has_value())
    return *this->state_class_;
  return this->state_class();
}
StateClass Sensor::state_class() { return StateClass::STATE_CLASS_NONE; }

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

void Sensor::add_on_state_callback(std::function<void(float)> &&callback) { this->callback_.add(std::move(callback)); }
void Sensor::add_on_raw_state_callback(std::function<void(float)> &&callback) {
  this->raw_callback_.add(std::move(callback));
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
float Sensor::get_state() const { return this->state; }
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
uint32_t Sensor::hash_base() { return 2455723294UL; }

}  // namespace sensor
}  // namespace esphome
