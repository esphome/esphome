#pragma once

#include "esphome/components/select/select.h"
#include "../mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPSelect : public select::Select, public Parented<MitsubishiUART>, public MITPListener {
 public:
  MITPSelect() = default;
  using Parented<MitsubishiUART>::Parented;
  void publish(bool force = false) {
    // Only publish if force, or a change has occurred and we have a real value
    if (force || (mitp_select_value_.has_value() && mitp_select_value_.value() != state)) {
      publish_state(mitp_select_value_.value());
    }
  }

 protected:
  void control(const std::string &value) override = 0;
  optional<std::string> mitp_select_value_;
};

class TemperatureSourceSelect : public MITPSelect {
 public:
  void publish(bool force = false) override;
  void setup(bool thermostat_is_present) override;
  void temperature_source_change(const std::string temp_source) override;

  // Adds an option to temperature_source_select_
  void register_temperature_source(std::string temperature_source_name);

 protected:
  void control(const std::string &value) override;

 private:
  ESPPreferenceObject preferences_;
  std::vector<std::string> temp_select_options_ = {
      TEMPERATURE_SOURCE_INTERNAL};  // Used to map strings to indexes for preference storage
};

class VanePositionSelect : public MITPSelect {
  void process_packet(const SettingsGetResponsePacket &packet) {
    switch (packet.get_vane()) {
      case SettingsSetRequestPacket::VANE_AUTO:
        mitp_select_value_ = std::string("Auto");
        break;
      case SettingsSetRequestPacket::VANE_1:
        mitp_select_value_ = std::string("1");
        break;
      case SettingsSetRequestPacket::VANE_2:
        mitp_select_value_ = std::string("2");
        break;
      case SettingsSetRequestPacket::VANE_3:
        mitp_select_value_ = std::string("3");
        break;
      case SettingsSetRequestPacket::VANE_4:
        mitp_select_value_ = std::string("4");
        break;
      case SettingsSetRequestPacket::VANE_5:
        mitp_select_value_ = std::string("5");
        break;
      case SettingsSetRequestPacket::VANE_SWING:
        mitp_select_value_ = std::string("Swing");
        break;
      default:
        ESP_LOGW(TAG, "Vane in unknown position %x", packet.get_vane());
    }
  }

 protected:
  void control(const std::string &value) {
    if (parent_->select_vane_position(value)) {
      mitp_select_value_ = value;
      publish();
    }
  }
};

class HorizontalVanePositionSelect : public MITPSelect {
  void process_packet(const SettingsGetResponsePacket &packet) {
    switch (packet.get_horizontal_vane()) {
      case SettingsSetRequestPacket::HV_AUTO:
        mitp_select_value_ = std::string("Auto");
        break;
      case SettingsSetRequestPacket::HV_LEFT_FULL:
        mitp_select_value_ = std::string("<<");
        break;
      case SettingsSetRequestPacket::HV_LEFT:
        mitp_select_value_ = std::string("<");
        break;
      case SettingsSetRequestPacket::HV_CENTER:
        mitp_select_value_ = std::string("");
        break;
      case SettingsSetRequestPacket::HV_RIGHT:
        mitp_select_value_ = std::string(">");
        break;
      case SettingsSetRequestPacket::HV_RIGHT_FULL:
        mitp_select_value_ = std::string(">>");
        break;
      case SettingsSetRequestPacket::HV_SPLIT:
        mitp_select_value_ = std::string("<>");
        break;
      case SettingsSetRequestPacket::HV_SWING:
        mitp_select_value_ = std::string("Swing");
        break;
      default:
        ESP_LOGW(TAG, "Vane in unknown horizontal position %x", packet.get_horizontal_vane());
    }
  }

 protected:
  void control(const std::string &value) {
    if (parent_->select_horizontal_vane_position(value)) {
      mitp_select_value_ = value;
      publish();
    }
  }
};

}  // namespace mitsubishi_itp
}  // namespace esphome
