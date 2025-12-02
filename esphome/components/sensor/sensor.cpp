#include "sensor.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sensor {

static const char *const TAG = "sensor";

// Function implementation of LOG_SENSOR macro to reduce code size
void log_sensor(const char *tag, const char *prefix, const char *type, Sensor *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag,
                "%s%s '%s'\n"
                "%s  State Class: '%s'\n"
                "%s  Unit of Measurement: '%s'\n"
                "%s  Accuracy Decimals: %d",
                prefix, type, obj->get_name().c_str(), prefix,
                LOG_STR_ARG(state_class_to_string(obj->get_state_class())), prefix,
                obj->get_unit_of_measurement_ref().c_str(), prefix, obj->get_accuracy_decimals());

  if (!obj->get_device_class_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Device Class: '%s'", prefix, obj->get_device_class_ref().c_str());
  }

  if (!obj->get_icon_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", prefix, obj->get_icon_ref().c_str());
  }

  if (obj->get_force_update()) {
    ESP_LOGV(tag, "%s  Force Update: YES", prefix);
  }
}

const LogString *state_class_to_string(StateClass state_class) {
  switch (state_class) {
    case STATE_CLASS_MEASUREMENT:
      return LOG_STR("measurement");
    case STATE_CLASS_TOTAL_INCREASING:
      return LOG_STR("total_increasing");
    case STATE_CLASS_TOTAL:
      return LOG_STR("total");
    case STATE_CLASS_MEASUREMENT_ANGLE:
      return LOG_STR("measurement_angle");
    case STATE_CLASS_NONE:
    default:
      return LOG_STR("");
  }
}

Sensor::Sensor() : state(NAN), raw_state(NAN) {}

int8_t Sensor::get_accuracy_decimals() {
  if (this->sensor_flags_.has_accuracy_override)
    return this->accuracy_decimals_;
  return 0;
}
void Sensor::set_accuracy_decimals(int8_t accuracy_decimals) {
  this->accuracy_decimals_ = accuracy_decimals;
  this->sensor_flags_.has_accuracy_override = true;
}

void Sensor::set_state_class(StateClass state_class) {
  this->state_class_ = state_class;
  this->sensor_flags_.has_state_class_override = true;
}
StateClass Sensor::get_state_class() {
  if (this->sensor_flags_.has_state_class_override)
    return this->state_class_;
  return StateClass::STATE_CLASS_NONE;
}

void Sensor::publish_state(float state) {
  this->raw_state = state;
  if (this->raw_callback_) {
    this->raw_callback_->call(state);
  }

  ESP_LOGV(TAG, "'%s': Received new state %f", this->name_.c_str(), state);

  if (this->filter_list_ == nullptr) {
    this->internal_send_state_to_frontend(state);
  } else {
    this->filter_list_->input(state);
  }
}

void Sensor::add_on_state_callback(std::function<void(float)> &&callback) { this->callback_.add(std::move(callback)); }
void Sensor::add_on_raw_state_callback(std::function<void(float)> &&callback) {
  if (!this->raw_callback_) {
    this->raw_callback_ = make_unique<CallbackManager<void(float)>>();
  }
  this->raw_callback_->add(std::move(callback));
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
void Sensor::add_filters(std::initializer_list<Filter *> filters) {
  for (Filter *filter : filters) {
    this->add_filter(filter);
  }
}
void Sensor::set_filters(std::initializer_list<Filter *> filters) {
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

void Sensor::internal_send_state_to_frontend(float state) {
  this->set_has_state(true);
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %.5f %s with %d decimals of accuracy", this->get_name().c_str(), state,
           this->get_unit_of_measurement_ref().c_str(), this->get_accuracy_decimals());
  this->callback_.call(state);
#if defined(USE_SENSOR) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_sensor_update(this);
#endif
}

}  // namespace sensor
}  // namespace esphome
