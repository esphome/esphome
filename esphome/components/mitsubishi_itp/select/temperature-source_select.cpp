#include "mitp_select.h"

namespace esphome {
namespace mitsubishi_itp {

void TemperatureSourceSelect::publish(bool force) {
  // Only publish if force, or a change has occurred and we have a real value
  if (force || (mitp_select_value_.has_value() && mitp_select_value_.value() != state)) {
    publish_state(mitp_select_value_.value());
    if (active_index().has_value()) {
      preferences_.save(&active_index().value());
    }
  }
}

void TemperatureSourceSelect::setup(bool thermostat_is_present) {
  if (thermostat_is_present) {
    temp_select_options_.push_back(TEMPERATURE_SOURCE_THERMOSTAT);
  }

  traits.set_options(temp_select_options_);

  // Using App.get_compilation_time() means these will get reset each time the firmware is updated, but this
  // is an easy way to prevent wierd conflicts if e.g. select options change.
  this->preferences_ =
      global_preferences->make_preference<size_t>(this->get_object_id_hash() ^ fnv1_hash(App.get_compilation_time()));

  size_t saved_index;
  if (this->preferences_.load(&saved_index) && has_index(saved_index) && at(saved_index).has_value()) {
    control(at(saved_index).value());
  } else {
    control(TEMPERATURE_SOURCE_INTERNAL);  // Set to internal if no preferences loaded.
  }
}

void TemperatureSourceSelect::temperature_source_change(const std::string temp_source) {
  mitp_select_value_ = temp_source;
}

void TemperatureSourceSelect::register_temperature_source(std::string temperature_source_name) {
  temp_select_options_.push_back(temperature_source_name);
}

void TemperatureSourceSelect::control(const std::string &value) {
  if (parent_->select_temperature_source(value)) {
    mitp_select_value_ = value;
    publish();
  }
}

}  // namespace mitsubishi_itp
}  // namespace esphome
