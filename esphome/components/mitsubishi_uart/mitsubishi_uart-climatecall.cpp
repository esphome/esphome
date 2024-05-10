#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

// Called to instruct a change of the climate controls
void MitsubishiUART::control(const climate::ClimateCall &call) {
  if (!active_mode)
    return;  // If we're not in active mode, ignore control requests

  SettingsSetRequestPacket setRequestPacket = SettingsSetRequestPacket();

  // Fan

  if (call.get_custom_fan_mode().has_value()) {
    if (call.get_custom_fan_mode().value() == FAN_MODE_VERYHIGH) {
      set_custom_fan_mode_(FAN_MODE_VERYHIGH);
      setRequestPacket.setFan(SettingsSetRequestPacket::FAN_4);
    }
  }

  switch (call.get_fan_mode().value()) {
    case climate::CLIMATE_FAN_QUIET:
      set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      setRequestPacket.setFan(SettingsSetRequestPacket::FAN_QUIET);
      break;
    case climate::CLIMATE_FAN_LOW:
      set_fan_mode_(climate::CLIMATE_FAN_LOW);
      setRequestPacket.setFan(SettingsSetRequestPacket::FAN_1);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      setRequestPacket.setFan(SettingsSetRequestPacket::FAN_2);
      break;
    case climate::CLIMATE_FAN_HIGH:
      set_fan_mode_(climate::CLIMATE_FAN_HIGH);
      setRequestPacket.setFan(SettingsSetRequestPacket::FAN_3);
      break;
    case climate::CLIMATE_FAN_AUTO:
      set_fan_mode_(climate::CLIMATE_FAN_AUTO);
      setRequestPacket.setFan(SettingsSetRequestPacket::FAN_AUTO);
      break;
    default:
      ESP_LOGW(TAG, "Unhandled fan mode %i!", call.get_fan_mode().value());
      break;
  }

  // Mode

  if (call.get_mode().has_value()) {
    mode = call.get_mode().value();

    switch (call.get_mode().value()) {
      case climate::CLIMATE_MODE_HEAT_COOL:
        setRequestPacket.setPower(true).setMode(SettingsSetRequestPacket::MODE_BYTE_AUTO);
        break;
      case climate::CLIMATE_MODE_COOL:
        setRequestPacket.setPower(true).setMode(SettingsSetRequestPacket::MODE_BYTE_COOL);
        break;
      case climate::CLIMATE_MODE_HEAT:
        setRequestPacket.setPower(true).setMode(SettingsSetRequestPacket::MODE_BYTE_HEAT);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        setRequestPacket.setPower(true).setMode(SettingsSetRequestPacket::MODE_BYTE_FAN);
        break;
      case climate::CLIMATE_MODE_DRY:
        setRequestPacket.setPower(true).setMode(SettingsSetRequestPacket::MODE_BYTE_DRY);
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        setRequestPacket.setPower(false);
        break;
    }
  }

  // Target Temperature

  if (call.get_target_temperature().has_value()) {
    target_temperature = call.get_target_temperature().value();
    setRequestPacket.setTargetTemperature(call.get_target_temperature().value());
  }

  // TODO:
  // Vane
  // HVane?
  // Swing?

  // We're assuming that every climate call *does* make some change worth sending to the heat pump
  // Queue the packet to be sent first (so any subsequent update packets come *after* our changes)
  hp_bridge.sendPacket(setRequestPacket);

  // Publish state and any sensor changes (shouldn't be any a a result of this function, but
  // since they lazy-publish, no harm in trying)
  doPublish();
};

}  // namespace mitsubishi_uart
}  // namespace esphome
