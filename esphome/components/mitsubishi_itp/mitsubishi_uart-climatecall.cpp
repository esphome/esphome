#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_itp {

// Called to instruct a change of the climate controls
void MitsubishiUART::control(const climate::ClimateCall &call) {
  if (!active_mode_)
    return;  // If we're not in active mode, ignore control requests

  SettingsSetRequestPacket set_request_packet = SettingsSetRequestPacket();

  // Apply fan settings
  // Prioritize a custom fan mode if it's set.
  if (call.get_custom_fan_mode().has_value()) {
    if (call.get_custom_fan_mode().value() == FAN_MODE_VERYHIGH) {
      set_custom_fan_mode_(FAN_MODE_VERYHIGH);
      set_request_packet.set_fan(SettingsSetRequestPacket::FAN_4);
    }
  } else if (call.get_fan_mode().has_value()) {
    switch (call.get_fan_mode().value()) {
      case climate::CLIMATE_FAN_QUIET:
        set_fan_mode_(climate::CLIMATE_FAN_QUIET);
        set_request_packet.set_fan(SettingsSetRequestPacket::FAN_QUIET);
        break;
      case climate::CLIMATE_FAN_LOW:
        set_fan_mode_(climate::CLIMATE_FAN_LOW);
        set_request_packet.set_fan(SettingsSetRequestPacket::FAN_1);
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
        set_request_packet.set_fan(SettingsSetRequestPacket::FAN_2);
        break;
      case climate::CLIMATE_FAN_HIGH:
        set_fan_mode_(climate::CLIMATE_FAN_HIGH);
        set_request_packet.set_fan(SettingsSetRequestPacket::FAN_3);
        break;
      case climate::CLIMATE_FAN_AUTO:
        set_fan_mode_(climate::CLIMATE_FAN_AUTO);
        set_request_packet.set_fan(SettingsSetRequestPacket::FAN_AUTO);
        break;
      default:
        ESP_LOGW(TAG, "Unhandled fan mode %i!", call.get_fan_mode().value());
        break;
    }
  }

  // Mode

  if (call.get_mode().has_value()) {
    mode = call.get_mode().value();

    switch (call.get_mode().value()) {
      case climate::CLIMATE_MODE_HEAT_COOL:
        set_request_packet.set_power(true).set_mode(SettingsSetRequestPacket::MODE_BYTE_AUTO);
        break;
      case climate::CLIMATE_MODE_COOL:
        set_request_packet.set_power(true).set_mode(SettingsSetRequestPacket::MODE_BYTE_COOL);
        break;
      case climate::CLIMATE_MODE_HEAT:
        set_request_packet.set_power(true).set_mode(SettingsSetRequestPacket::MODE_BYTE_HEAT);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        set_request_packet.set_power(true).set_mode(SettingsSetRequestPacket::MODE_BYTE_FAN);
        break;
      case climate::CLIMATE_MODE_DRY:
        set_request_packet.set_power(true).set_mode(SettingsSetRequestPacket::MODE_BYTE_DRY);
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        set_request_packet.set_power(false);
        break;
    }
  }

  // Target Temperature

  if (call.get_target_temperature().has_value()) {
    target_temperature = call.get_target_temperature().value();
    set_request_packet.set_target_temperature(call.get_target_temperature().value());

    // update our MHK tracking setpoints accordingly
    switch (mode) {
      case climate::CLIMATE_MODE_COOL:
      case climate::CLIMATE_MODE_DRY:
        this->mhk_state_.cool_setpoint_ = target_temperature;
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->mhk_state_.heat_setpoint_ = target_temperature;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        if (this->get_traits().get_supports_two_point_target_temperature()) {
          this->mhk_state_.cool_setpoint_ = target_temperature_low;
          this->mhk_state_.heat_setpoint_ = target_temperature_high;
        } else {
          // HACK: This is not accurate, but it's good enough for testing.
          this->mhk_state_.cool_setpoint_ = target_temperature + 2;
          this->mhk_state_.heat_setpoint_ = target_temperature - 2;
        }
      default:
        break;
    }
  }

  // TODO:
  // Vane
  // HVane?
  // Swing?

  // We're assuming that every climate call *does* make some change worth sending to the heat pump
  // Queue the packet to be sent first (so any subsequent update packets come *after* our changes)
  hp_bridge_.send_packet(set_request_packet);

  // Publish state and any sensor changes (shouldn't be any a a result of this function, but
  // since they lazy-publish, no harm in trying)
  do_publish_();
};

}  // namespace mitsubishi_itp
}  // namespace esphome
