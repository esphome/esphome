#include "muart_packet.h"
#include "muart_utils.h"
#include "mitsubishi_uart.h"
#include "esphome/core/datatypes.h"

namespace esphome {
namespace mitsubishi_itp {

// Packet to_strings()

std::string ConnectRequestPacket::to_string() const { return ("Connect Request: " + Packet::to_string()); }
std::string ConnectResponsePacket::to_string() const { return ("Connect Response: " + Packet::to_string()); }
std::string CapabilitiesResponsePacket::to_string() const {
  return (
      "Identify Base Capabilities Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
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
std::string IdentifyCDResponsePacket::to_string() const { return "Identify CD Response: " + Packet::to_string(); }
std::string CurrentTempGetResponsePacket::to_string() const {
  return ("Current Temp Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Temp:" + std::to_string(get_current_temp()) +
          " Outdoor:" + (std::isnan(get_outdoor_temp()) ? "Unsupported" : std::to_string(get_outdoor_temp())));
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
std::string RunStateGetResponsePacket::to_string() const {
  return ("RunState Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n ServiceFilter:" + (service_filter() ? "Yes" : "No") + " Defrost:" + (in_defrost() ? "Yes" : "No") +
          " Preheat:" + (in_preheat() ? "Yes" : "No") + " Standby:" + (in_standby() ? "Yes" : "No") +
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

std::string ThermostatSensorStatusPacket::to_string() const {
  return ("Thermostat Sensor Status: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Indoor RH: " + std::to_string(get_indoor_humidity_percent()) + "%" +
          "  MHK Battery: " + THERMOSTAT_BATTERY_STATE_NAMES[get_thermostat_battery_state()] + "(" +
          std::to_string(get_thermostat_battery_state()) + ")" +
          "  Sensor Flags: " + std::to_string(get_sensor_flags()));
}

std::string ThermostatHelloPacket::to_string() const {
  return ("Thermostat Hello: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n Model: " + get_thermostat_model() +
          " Serial: " + get_thermostat_serial() + " Version: " + get_thermostat_version_string());
}

std::string ThermostatStateUploadPacket::to_string() const {
  uint8_t flags = get_flags();

  std::string result =
      "Thermostat Sync " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n Flags: " + format_hex(flags) + " =>";

  if (flags & TSSF_TIMESTAMP) {
    ESPTime timestamp{};
    get_thermostat_timestamp(&timestamp);

    result += " TS Time: " + timestamp.strftime("%Y-%m-%d %H:%M:%S");
  }

  if (flags & TSSF_AUTO_MODE)
    result += " AutoMode: " + std::to_string(get_auto_mode());
  if (flags & TSSF_HEAT_SETPOINT)
    result += " HeatSetpoint: " + std::to_string(get_heat_setpoint());
  if (flags & TSSF_COOL_SETPOINT)
    result += " CoolSetpoint: " + std::to_string(get_cool_setpoint());

  return result;
}

std::string GetRequestPacket::to_string() const {
  return ("Get Request: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n CommandID: " + format_hex((uint8_t) get_requested_command()));
}

std::string SettingsSetRequestPacket::to_string() const {
  uint8_t flags = get_flags();
  uint8_t flags2 = get_flags_2();

  std::string result = "Settings Set Request: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
                       "\n Flags: " + format_hex(flags2) + format_hex(flags) + " =>";

  if (flags & SettingFlag::SF_POWER)
    result += " Power: " + std::to_string(get_power());
  if (flags & SettingFlag::SF_MODE)
    result += " Mode: " + std::to_string(get_mode());
  if (flags & SettingFlag::SF_TARGET_TEMPERATURE)
    result += " TargetTemp: " + std::to_string(get_target_temp());
  if (flags & SettingFlag::SF_FAN)
    result += " Fan: " + std::to_string(get_fan());
  if (flags & SettingFlag::SF_VANE)
    result += " Vane: " + std::to_string(get_vane());

  if (flags2 & SettingFlag2::SF2_HORIZONTAL_VANE)
    result += " HVane: " + std::to_string(get_horizontal_vane()) + (get_horizontal_vane_msb() ? " (MSB Set)" : "");

  return result;
}

// TODO: Are there function implementations for packets in the .h file? (Yes)  Should they be here?

// SettingsSetRequestPacket functions

void SettingsSetRequestPacket::add_settings_flag_(const SettingFlag flag_to_add) { add_flag(flag_to_add); }

void SettingsSetRequestPacket::add_settings_flag2_(const SettingFlag2 flag2_to_add) { add_flag2(flag2_to_add); }

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

SettingsSetRequestPacket &SettingsSetRequestPacket::set_target_temperature(const float temperature_degrees_c) {
  if (temperature_degrees_c < 63.5 && temperature_degrees_c > -64.0) {
    pkt_.set_payload_byte(PLINDEX_TARGET_TEMPERATURE, MUARTUtils::deg_c_to_temp_scale_a(temperature_degrees_c));
    pkt_.set_payload_byte(PLINDEX_TARGET_TEMPERATURE_CODE,
                          MUARTUtils::deg_c_to_legacy_target_temp(temperature_degrees_c));

    // TODO: while spawning a warning here is fine, we should (a) only actually send that warning if the system can't
    //       support this setpoint, and (b) clamp the setpoint to the known-acceptable values.
    // The utility class will already clamp this for us, so we only need to worry about the warning.
    if (temperature_degrees_c < 16 || temperature_degrees_c > 31.5) {
      ESP_LOGW(PTAG, "Target temp %f is out of range for the legacy temp scale. This may be a problem on older units.",
               temperature_degrees_c);
    }

    add_settings_flag_(SF_TARGET_TEMPERATURE);
  } else {
    ESP_LOGW(PTAG, "Target temp %f is outside valid range - target temperature not set!", temperature_degrees_c);
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

float SettingsSetRequestPacket::get_target_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_TARGET_TEMPERATURE);

  if (enhanced_raw_temp == 0x00) {
    uint8_t legacy_raw_temp = pkt_.get_payload_byte(PLINDEX_TARGET_TEMPERATURE_CODE);
    return MUARTUtils::legacy_target_temp_to_deg_c(legacy_raw_temp);
  }

  return MUARTUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
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

bool SettingsGetResponsePacket::is_i_see_enabled() const {
  uint8_t mode = pkt_.get_payload_byte(PLINDEX_MODE);

  // so far only modes 0x09 to 0x11 are known to be i-see.
  // Mode 0x08 technically *can* be, but it's not a guarantee by itself.
  return (mode >= 0x09 && mode <= 0x11);
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
    float temperature_degrees_c) {
  if (temperature_degrees_c < 63.5 && temperature_degrees_c > -64.0) {
    pkt_.set_payload_byte(PLINDEX_REMOTE_TEMPERATURE, MUARTUtils::deg_c_to_temp_scale_a(temperature_degrees_c));
    pkt_.set_payload_byte(PLINDEX_LEGACY_REMOTE_TEMPERATURE,
                          MUARTUtils::deg_c_to_legacy_room_temp(temperature_degrees_c));
    set_flags(0x01);  // Set flags to say we're providing the temperature
  } else {
    ESP_LOGW(PTAG, "Remote temp %f is outside valid range.", temperature_degrees_c);
  }
  return *this;
}
RemoteTemperatureSetRequestPacket &RemoteTemperatureSetRequestPacket::use_internal_temperature() {
  set_flags(0x00);  // Set flags to say to use internal temperature
  return *this;
}

// SettingsSetRunStatusPacket functions
SetRunStatePacket &SetRunStatePacket::set_filter_reset(bool do_reset) {
  pkt_.set_payload_byte(PLINDEX_FILTER_RESET, do_reset ? 1 : 0);
  set_flags(0x01);
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

float CurrentTempGetResponsePacket::get_outdoor_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_OUTDOORTEMP);

  // Return NAN if unsupported
  return enhanced_raw_temp == 0 ? NAN : MUARTUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

// ThermostatHelloPacket functions
std::string ThermostatHelloPacket::get_thermostat_model() const {
  return MUARTUtils::decode_n_bit_string((pkt_.get_payload_bytes(1)), 4, 6);
}

std::string ThermostatHelloPacket::get_thermostat_serial() const {
  return MUARTUtils::decode_n_bit_string((pkt_.get_payload_bytes(4)), 12, 6);
}

std::string ThermostatHelloPacket::get_thermostat_version_string() const {
  char buf[16];
  sprintf(buf, "%02d.%02d.%02d", pkt_.get_payload_byte(13), pkt_.get_payload_byte(14), pkt_.get_payload_byte(15));

  return buf;
}

// ThermostatStateUploadPacket functions
int32_t ThermostatStateUploadPacket::get_thermostat_timestamp(esphome::ESPTime *outTimestamp) const {
  int32_be_t magic;
  std::memcpy(&magic, pkt_.get_payload_bytes(PLINDEX_THERMOSTAT_TIMESTAMP), 4);

  outTimestamp->second = magic & 63;
  outTimestamp->minute = (magic >> 6) & 63;
  outTimestamp->hour = (magic >> 12) & 31;
  outTimestamp->day_of_month = (magic >> 17) & 31;
  outTimestamp->month = (magic >> 22) & 15;
  outTimestamp->year = (magic >> 26) + 2017;

  outTimestamp->recalc_timestamp_local();
  return outTimestamp->timestamp;
}

uint8_t ThermostatStateUploadPacket::get_auto_mode() const { return pkt_.get_payload_byte(PLINDEX_AUTO_MODE); }

float ThermostatStateUploadPacket::get_heat_setpoint() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_HEAT_SETPOINT);
  return MUARTUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

float ThermostatStateUploadPacket::get_cool_setpoint() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_COOL_SETPOINT);
  return MUARTUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

// ThermostatStateDownloadResponsePacket functions
ThermostatStateDownloadResponsePacket &ThermostatStateDownloadResponsePacket::set_timestamp(esphome::ESPTime ts) {
  int32_t encoded_timestamp = ((ts.year - 2017) << 26) | (ts.month << 22) | (ts.day_of_month << 17) | (ts.hour << 12) |
                              (ts.minute << 6) | (ts.second);

  int32_t swapped_timestamp = byteswap(encoded_timestamp);

  pkt_.set_payload_bytes(PLINDEX_ADAPTER_TIMESTAMP, &swapped_timestamp, 4);
  pkt_.set_payload_byte(10, 0x07);  // ???

  return *this;
}

ThermostatStateDownloadResponsePacket &ThermostatStateDownloadResponsePacket::set_auto_mode(bool is_auto) {
  pkt_.set_payload_byte(PLINDEX_AUTO_MODE, is_auto ? 0x01 : 0x00);
  return *this;
}

ThermostatStateDownloadResponsePacket &ThermostatStateDownloadResponsePacket::set_heat_setpoint(float high_temp) {
  uint8_t temp_a = high_temp != NAN ? MUARTUtils::deg_c_to_temp_scale_a(high_temp) : 0x00;

  pkt_.set_payload_byte(PLINDEX_HEAT_SETPOINT, temp_a);
  return *this;
}

ThermostatStateDownloadResponsePacket &ThermostatStateDownloadResponsePacket::set_cool_setpoint(float low_temp) {
  uint8_t temp_a = low_temp != NAN ? MUARTUtils::deg_c_to_temp_scale_a(low_temp) : 0x00;

  pkt_.set_payload_byte(PLINDEX_COOL_SETPOINT, temp_a);
  return *this;
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

// CapabilitiesResponsePacket functions
uint8_t CapabilitiesResponsePacket::get_supported_fan_speeds() const {
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

climate::ClimateTraits CapabilitiesResponsePacket::as_traits() const {
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

}  // namespace mitsubishi_itp
}  // namespace esphome
