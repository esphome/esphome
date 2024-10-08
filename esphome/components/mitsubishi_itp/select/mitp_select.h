#pragma once

#include "esphome/components/select/select.h"
#include "../mitsubishi_itp.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPSelect : public select::Select, public Parented<MitsubishiUART>, public MITPListener {
 public:
  MITPSelect() = default;
  using Parented<MitsubishiUART>::Parented;
  void publish() override {
    // Only publish if force, or a change has occurred and we have a real value
    if (mitp_select_value_.has_value() && mitp_select_value_.value() != state) {
      publish_state(mitp_select_value_.value());
    }
  }

 protected:
  void control(const std::string &value) override = 0;
  optional<std::string> mitp_select_value_;
};

class TemperatureSourceSelect : public MITPSelect {
 public:
  void publish() override;
  void setup(bool thermostat_is_present) override;
  void temperature_source_change(const std::string &temp_source) override;

  // Adds an option to temperature_source_select_
  void register_temperature_source(const std::string &temperature_source_name);

 protected:
  void control(const std::string &value) override;

 private:
  ESPPreferenceObject preferences_;
  std::vector<std::string> temp_select_options_ = {
      TEMPERATURE_SOURCE_INTERNAL};  // Used to map strings to indexes for preference storage
};

class VanePositionSelect : public MITPSelect {
  void process_packet(const SettingsGetResponsePacket &packet) override;

 protected:
  void control(const std::string &value) override {
    if (parent_->select_vane_position(value)) {
      mitp_select_value_ = value;
      publish();
    }
  }
};

class HorizontalVanePositionSelect : public MITPSelect {
  void process_packet(const SettingsGetResponsePacket &packet) override;

 protected:
  void control(const std::string &value) override {
    if (parent_->select_horizontal_vane_position(value)) {
      mitp_select_value_ = value;
      publish();
    }
  }
};

}  // namespace mitsubishi_itp
}  // namespace esphome
