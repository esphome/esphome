#include "mitp_packet.h"
#include "mitp_utils.h"
#include "mitsubishi_itp.h"
#include "esphome/core/datatypes.h"

namespace esphome {
namespace mitsubishi_itp {

// Packet to_strings()

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
std::string Functions1GetResponsePacket::to_string() const {
  std::stringstream funcstream;
  funcstream << "Functions1 Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n";
  for (uint8_t i = 1; i < pkt_.get_length() - 6; i++) {
    uint8_t b = pkt_.get_payload_byte(i);
    funcstream << std::to_string(((b >> 2) & 0xff) + 100) + ":" + std::to_string(b & 3) + " ";
  }
  return funcstream.str();
}
std::string Functions2GetResponsePacket::to_string() const {
  std::stringstream funcstream;
  funcstream << "Functions2 Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n";
  for (uint8_t i = 1; i < pkt_.get_length() - 6; i++) {
    uint8_t b = pkt_.get_payload_byte(i);
    funcstream << std::to_string(((b >> 2) & 0xff) + 100) + ":" + std::to_string(b & 3) + " ";
  }
  return funcstream.str();
}

// SettingsGetResponsePacket functions
float SettingsGetResponsePacket::get_target_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_TARGETTEMP);

  if (enhanced_raw_temp == 0x00) {
    uint8_t legacy_raw_temp = pkt_.get_payload_byte(PLINDEX_TARGETTEMP_LEGACY);
    return MITPUtils::legacy_target_temp_to_deg_c(legacy_raw_temp);
  }

  return MITPUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

bool SettingsGetResponsePacket::is_i_see_enabled() const {
  uint8_t mode = pkt_.get_payload_byte(PLINDEX_MODE);

  // so far only modes 0x09 to 0x11 are known to be i-see.
  // Mode 0x08 technically *can* be, but it's not a guarantee by itself.
  return (mode >= 0x09 && mode <= 0x11);
}

// CurrentTempGetResponsePacket functions
float CurrentTempGetResponsePacket::get_current_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_CURRENTTEMP);

  // TODO: Figure out how to handle "out of range" issues here.
  if (enhanced_raw_temp == 0) {
    uint8_t legacy_raw_temp = pkt_.get_payload_byte(PLINDEX_CURRENTTEMP_LEGACY);
    return MITPUtils::legacy_room_temp_to_deg_c(legacy_raw_temp);
  }

  return MITPUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
}

float CurrentTempGetResponsePacket::get_outdoor_temp() const {
  uint8_t enhanced_raw_temp = pkt_.get_payload_byte(PLINDEX_OUTDOORTEMP);

  // Return NAN if unsupported
  return enhanced_raw_temp == 0 ? NAN : MITPUtils::temp_scale_a_to_deg_c(enhanced_raw_temp);
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

}  // namespace mitsubishi_itp
}  // namespace esphome
