#include "muart_packet.h"
#include "muart_utils.h"
#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

// Packet to_strings()

std::string ConnectRequestPacket::to_string() const { return ("Connect Request: " + Packet::to_string()); }
std::string ConnectResponsePacket::to_string() const { return ("Connect Response: " + Packet::to_string()); }
std::string ExtendedConnectResponsePacket::to_string() const {
  return (
      "Extended Connect Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
      "\n HeatDisabled:" + (is_heat_disabled() ? "Yes" : "No") + " SupportsVane:" + (supports_vane() ? "Yes" : "No") +
      " SupportsVaneSwing:" + (supports_vane_swing() ? "Yes" : "No")

      + " DryDisabled:" + (is_dry_disabled() ? "Yes" : "No") + " FanDisabled:" + (is_fan_disabled() ? "Yes" : "No") +
      " ExtTempRange:" + (has_extended_temperature_range() ? "Yes" : "No") +
      " AutoFanDisabled:" + (auto_fan_speed_disabled() ? "Yes" : "No") +
      " InstallerSettings:" + (supports_installer_settings() ? "Yes" : "No") +
      " TestMode:" + (supports_test_mode() ? "Yes" : "No") + " DryTemp:" + (supports_dry_temperature() ? "Yes" : "No")

      + " StatusDisplay:" + (has_status_display() ? "Yes" : "No")

      + "\n CoolDrySetpoint:" + std::to_string(get_min_cool_dry_setpoint()) + "/" +
      std::to_string(get_max_cool_dry_setpoint()) + " HeatSetpoint:" + std::to_string(get_min_heating_setpoint()) +
      "/" + std::to_string(get_max_heating_setpoint()) + " AutoSetpoint:" + std::to_string(get_min_auto_setpoint()) +
      "/" + std::to_string(get_max_auto_setpoint()) + " FanSpeeds:" + std::to_string(get_supported_fan_speeds()));
}
std::string CurrentTempGetResponsePacket::to_string() const {
  return ("Current Temp Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Temp:" + std::to_string(get_current_temp()));
}
std::string SettingsGetResponsePacket::to_string() const {
  return ("Settings Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n Fan:" + format_hex(get_fan()) +
          " Mode:" + format_hex(get_mode()) + " Power:" +
          (get_power() == 3  ? "Test"
           : get_power() > 0 ? "On"
                             : "Off") +
          " TargetTemp:" + std::to_string(get_target_temp()) + " Vane:" + format_hex(get_vane()) +
          " HVane:" + format_hex(get_horizontal_vane()) + (get_horizontal_vane_msb() ? " (MSB Set)" : "") +
          "\n PowerLock:" + (locked_power() ? "Yes" : "No") + " ModeLock:" + (locked_mode() ? "Yes" : "No") +
          " TempLock:" + (locked_temp() ? "Yes" : "No"));
}
std::string StandbyGetResponsePacket::to_string() const {
  return ("Standby Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n ServiceFilter:" + (service_filter() ? "Yes" : "No") + " Defrost:" + (in_defrost() ? "Yes" : "No") +
          " HotAdjust:" + (in_hot_adjust() ? "Yes" : "No") + " Standby:" + (in_standby() ? "Yes" : "No") +
          " ActualFan:" + ACTUAL_FAN_SPEED_NAMES[get_actual_fan_speed()] + " (" +
          std::to_string(get_actual_fan_speed()) + ")" + " AutoMode:" + format_hex(get_auto_mode()));
}
std::string StatusGetResponsePacket::to_string() const {
  return ("Status Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n CompressorFrequency: " +
          std::to_string(get_compressor_frequency()) + " Operating: " + (get_operating() ? "Yes" : "No"));
}
std::string ErrorStateGetResponsePacket::to_string() const {
  return ("Error State Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Error State: " + (error_present() ? "Yes" : "No") + " ErrorCode: " + format_hex(get_error_code()) +
          " ShortCode: " + get_short_code() + "(" + format_hex(get_raw_short_code()) + ")");
}
std::string RemoteTemperatureSetRequestPacket::to_string() const {
  return ("Remote Temp Set Request: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Temp:" + std::to_string(get_remote_temperature()));
}

std::string ThermostatHelloRequestPacket::to_string() const {
  return ("Thermostat Hello: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n Model: " + get_thermostat_model() +
          " Serial: " + get_thermostat_serial() + " Version: " + get_thermostat_version_string());
}

// TODO: Are there function implementations for packets in the .h file? (Yes)  Should they be here?

// SettingsSetRequestPacket functions

void SettingsSetRequestPacket::add_settings_flag_(const SettingFlag flag_to_add) { add_flag(flag_to_add); }

void SettingsSetRequestPacket::add_settings_flag2_(const SettingFlaG2 flag2_to_add) { add_flag2(flag2_to_add); }

SettingsSetRequestPacket &SettingsSetRequestPacket::set_power(const bool is_on) {
  pkt_.set_payload_byte(PLINDEX_POWER, is_on ? 0x01 : 0x00);
  add_settings_flag_(SF_POWER);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::set_mode(const ModeByte mode) {
  pkt_.set_payload_byte(PLINDEX_MODE, mode);
  add_settings_flag_(SF_MODE);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::set_target_temperature(const float temperature_degress_c) {
  if (temperature_degress_c < 63.5 && temperature_degress_c > -64.0) {
    pkt_.set_payload_byte(PLINDEX_TARGET_TEMPERATURE, MUARTUtils::deg_c_to_temp_scale_a(temperature_degress_c));
    pkt_.set_payload_byte(PLINDEX_TARGET_TEMPERATURE_CODE,
                          MUARTUtils::deg_c_to_legacy_target_temp(temperature_degress_c));

    // TODO: while spawning a warning here is fine, we should (a) only actually send that warning if the system can't
    //       support this setpoint, and (b) clamp the setpoint to the known-acceptable values.
    // The utility class will already clamp this for us, so we only need to worry about the warning.
    if (temperature_degress_c < 16 || temperature_degress_c > 31.5) {
      ESP_LOGW(PTAG, "Target temp %f is out of range for the legacy temp scale. This may be a problem on older units.",
               temperature_degress_c);
    }

    add_settings_flag_(SF_TARGET_TEMPERATURE);
  } else {
    ESP_LOGW(PTAG, "Target temp %f is outside valid range - target temperature not set!", temperature_degress_c);
  }

  return *this;
}
SettingsSetRequestPacket &SettingsSetRequestPacket::set_fan(const FanByte fan) {
  pkt_.set_payload_byte(PLINDEX_FAN, fan);
  add_settings_flag_(SF_FAN);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::set_vane(const VaneByte vane) {
  pkt_.set_payload_byte(PLINDEX_VANE, vane);
  add_settings_flag_(SF_VANE);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::set_horizontal_vane(const HorizontalVaneByte horizontal_vane) {
  pkt_.set_payload_byte(PLINDEX_HORIZONTAL_VANE, horizontal_vane);
  add_settings_flag2_(SF2_HORIZONTAL_VANE);
  return *this;
}

// SettingsGetResponsePacket functions
float SettingsGetResponsePacket::get_target_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_TARGETTEMP);

  if (enhanced_raw_temp == 0x00) {
    uint8_t legacy_raw_temp = pkt_.get_payload_byte(PLINDEX_TARGETTEMP_LEGACY);
    return MUARTUtils::legacy_target_temp_to_deg_c(legacy_raw_temp);
  }

  return MUARTUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

// RemoteTemperatureSetRequestPacket functions

float RemoteTemperatureSetRequestPacket::get_remote_temperature() const {
  uint8_t raw_temp_a = pkt_.get_payload_byte(PLINDEX_REMOTE_TEMPERATURE);

  if (raw_temp_a == 0) {
    uint8_t raw_temp_legacy = pkt_.get_payload_byte(PLINDEX_LEGACY_REMOTE_TEMPERATURE);
    return MUARTUtils::legacy_room_temp_to_deg_c(raw_temp_legacy);
  }

  return MUARTUtils::temp_scale_a_to_deg_c(raw_temp_a);
}

RemoteTemperatureSetRequestPacket &RemoteTemperatureSetRequestPacket::set_remote_temperature(
    float temperature_degress_c) {
  if (temperature_degress_c < 63.5 && temperature_degress_c > -64.0) {
    pkt_.set_payload_byte(PLINDEX_REMOTE_TEMPERATURE, MUARTUtils::deg_c_to_temp_scale_a(temperature_degress_c));
    pkt_.set_payload_byte(PLINDEX_LEGACY_REMOTE_TEMPERATURE,
                          MUARTUtils::deg_c_to_legacy_room_temp(temperature_degress_c));
    set_flags(0x01);  // Set flags to say we're providing the temperature
  } else {
    ESP_LOGW(PTAG, "Remote temp %f is outside valid range.", temperature_degress_c);
  }
  return *this;
}
RemoteTemperatureSetRequestPacket &RemoteTemperatureSetRequestPacket::use_internal_temperature() {
  set_flags(0x00);  // Set flags to say to use internal temperature
  return *this;
}

// CurrentTempGetResponsePacket functions
float CurrentTempGetResponsePacket::get_current_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_CURRENTTEMP);

  // TODO: Figure out how to handle "out of range" issues here.
  if (enhanced_raw_temp == 0) {
    uint8_t legacy_raw_temp = pkt_.get_payload_byte(PLINDEX_CURRENTTEMP_LEGACY);
    return MUARTUtils::legacy_room_temp_to_deg_c(legacy_raw_temp);
  }

  return MUARTUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

// ThermostatHelloRequestPacket functions
std::string ThermostatHelloRequestPacket::get_thermostat_model() const {
  return MUARTUtils::decode_n_bit_string((pkt_.get_bytes() + 1), 3, 6);
}

std::string ThermostatHelloRequestPacket::get_thermostat_serial() const {
  return MUARTUtils::decode_n_bit_string((pkt_.get_bytes() + 4), 8, 6);
}

std::string ThermostatHelloRequestPacket::get_thermostat_version_string() const {
  char buf[16];
  sprintf(buf, "%02d.%02d.%02d", pkt_.get_payload_byte(13), pkt_.get_payload_byte(14), pkt_.get_payload_byte(15));

  return buf;
}

// ErrorStateGetResponsePacket functions
std::string ErrorStateGetResponsePacket::get_short_code() const {
  const char *upper_alphabet = "AbEFJLPU";
  const char *lower_alphabet = "0123456789ABCDEFOHJLPU";
  const uint8_t error_code = this->get_raw_short_code();

  uint8_t low_bits = error_code & 0x1F;
  if (low_bits > 0x15) {
    char buf[7];
    sprintf(buf, "ERR_%x", error_code);
    return buf;
  }

  return {upper_alphabet[(error_code & 0xE0) >> 5], lower_alphabet[low_bits]};
}

// ExtendedConnectResponsePacket functions
uint8_t ExtendedConnectResponsePacket::get_supported_fan_speeds() const {
  uint8_t raw_value = ((pkt_.get_payload_byte(7) & 0x10) >> 2) + ((pkt_.get_payload_byte(8) & 0x08) >> 2) +
                      ((pkt_.get_payload_byte(9) & 0x02) >> 1);

  switch (raw_value) {
    case 1:
    case 2:
    case 4:
      return raw_value;
    case 0:
      return 3;
    case 6:
      return 5;

    default:
      ESP_LOGW(PACKETS_TAG, "Unexpected supported fan speeds: %i", raw_value);
      return 0;  // TODO: Depending on how this is used, it might be more useful to just return 3 and hope for the best
  }
}

climate::ClimateTraits ExtendedConnectResponsePacket::as_traits() const {
  auto ct = climate::ClimateTraits();

  // always enabled
  ct.add_supported_mode(climate::CLIMATE_MODE_COOL);
  ct.add_supported_mode(climate::CLIMATE_MODE_OFF);

  if (!this->is_heat_disabled())
    ct.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  if (!this->is_dry_disabled())
    ct.add_supported_mode(climate::CLIMATE_MODE_DRY);
  if (!this->is_fan_disabled())
    ct.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);

  if (this->supports_vane_swing()) {
    ct.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);

    if (this->supports_vane() && this->supports_h_vane())
      ct.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    if (this->supports_vane())
      ct.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (this->supports_h_vane())
      ct.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
  }

  ct.set_visual_min_temperature(std::min(this->get_min_cool_dry_setpoint(), this->get_min_heating_setpoint()));
  ct.set_visual_max_temperature(std::max(this->get_max_cool_dry_setpoint(), this->get_max_heating_setpoint()));

  // TODO: Figure out what these states *actually* map to so we aren't sending bad data.
  // This is probably a dynamic map, so the setter will need to be aware of things.
  switch (this->get_supported_fan_speeds()) {
    case 1:
      ct.set_supported_fan_modes({climate::CLIMATE_FAN_HIGH});
      break;
    case 2:
      ct.set_supported_fan_modes({climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_HIGH});
      break;
    case 3:
      ct.set_supported_fan_modes({climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
      break;
    case 4:
      ct.set_supported_fan_modes({
          climate::CLIMATE_FAN_QUIET,
          climate::CLIMATE_FAN_LOW,
          climate::CLIMATE_FAN_MEDIUM,
          climate::CLIMATE_FAN_HIGH,
      });
      break;
    case 5:
      ct.set_supported_fan_modes({
          climate::CLIMATE_FAN_QUIET,
          climate::CLIMATE_FAN_LOW,
          climate::CLIMATE_FAN_MEDIUM,
          climate::CLIMATE_FAN_HIGH,
      });
      ct.add_supported_custom_fan_mode("Very High");
      break;
    default:
      // no-op, don't set a fan mode.
      break;
  }
  if (!this->auto_fan_speed_disabled())
    ct.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);

  return ct;
}

}  // namespace mitsubishi_uart
}  // namespace esphome
