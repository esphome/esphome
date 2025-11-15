#include "toshiba.h"
#include "esphome/components/remote_base/toshiba_ac_protocol.h"

#include <vector>

namespace esphome {
namespace toshiba {

struct RacPt1411hwruFanSpeed {
  uint8_t code1;
  uint8_t code2;
};

static const char *const TAG = "toshiba.climate";
// Timings for IR bits/data
const uint16_t TOSHIBA_HEADER_MARK = 4380;
const uint16_t TOSHIBA_HEADER_SPACE = 4370;
const uint16_t TOSHIBA_GAP_SPACE = 5480;
const uint16_t TOSHIBA_PACKET_SPACE = 10500;
const uint16_t TOSHIBA_BIT_MARK = 540;
const uint16_t TOSHIBA_ZERO_SPACE = 540;
const uint16_t TOSHIBA_ONE_SPACE = 1620;
const uint16_t TOSHIBA_CARRIER_FREQUENCY = 38000;
const uint8_t TOSHIBA_HEADER_LENGTH = 4;
// Generic Toshiba commands/flags
const uint8_t TOSHIBA_COMMAND_DEFAULT = 0x01;
const uint8_t TOSHIBA_COMMAND_TIMER = 0x02;
const uint8_t TOSHIBA_COMMAND_POWER = 0x08;
const uint8_t TOSHIBA_COMMAND_MOTION = 0x02;

const uint8_t TOSHIBA_MODE_AUTO = 0x00;
const uint8_t TOSHIBA_MODE_COOL = 0x01;
const uint8_t TOSHIBA_MODE_DRY = 0x02;
const uint8_t TOSHIBA_MODE_HEAT = 0x03;
const uint8_t TOSHIBA_MODE_FAN_ONLY = 0x04;
const uint8_t TOSHIBA_MODE_OFF = 0x07;

const uint8_t TOSHIBA_FAN_SPEED_AUTO = 0x00;
const uint8_t TOSHIBA_FAN_SPEED_QUIET = 0x20;
const uint8_t TOSHIBA_FAN_SPEED_1 = 0x40;
const uint8_t TOSHIBA_FAN_SPEED_2 = 0x60;
const uint8_t TOSHIBA_FAN_SPEED_3 = 0x80;
const uint8_t TOSHIBA_FAN_SPEED_4 = 0xa0;
const uint8_t TOSHIBA_FAN_SPEED_5 = 0xc0;

const uint8_t TOSHIBA_POWER_HIGH = 0x01;
const uint8_t TOSHIBA_POWER_ECO = 0x03;

const uint8_t TOSHIBA_MOTION_SWING = 0x04;
const uint8_t TOSHIBA_MOTION_FIX = 0x00;

// RAC-PT1411HWRU temperature code flag bits
const uint8_t RAC_PT1411HWRU_FLAG_FAH = 0x01;
const uint8_t RAC_PT1411HWRU_FLAG_FRAC = 0x20;
const uint8_t RAC_PT1411HWRU_FLAG_NEG = 0x10;
// RAC-PT1411HWRU temperature short code flags mask
const uint8_t RAC_PT1411HWRU_FLAG_MASK = 0x0F;
// RAC-PT1411HWRU Headers, Footers and such
const uint8_t RAC_PT1411HWRU_MESSAGE_HEADER0 = 0xB2;
const uint8_t RAC_PT1411HWRU_MESSAGE_HEADER1 = 0xD5;
const uint8_t RAC_PT1411HWRU_MESSAGE_LENGTH = 6;
// RAC-PT1411HWRU "Comfort Sense" feature bits
const uint8_t RAC_PT1411HWRU_CS_ENABLED = 0x40;
const uint8_t RAC_PT1411HWRU_CS_DATA = 0x80;
const uint8_t RAC_PT1411HWRU_CS_HEADER = 0xBA;
const uint8_t RAC_PT1411HWRU_CS_FOOTER_AUTO = 0x7A;
const uint8_t RAC_PT1411HWRU_CS_FOOTER_COOL = 0x72;
const uint8_t RAC_PT1411HWRU_CS_FOOTER_HEAT = 0x7E;
// RAC-PT1411HWRU Swing
const uint8_t RAC_PT1411HWRU_SWING_HEADER = 0xB9;
const std::vector<uint8_t> RAC_PT1411HWRU_SWING_VERTICAL{0xB9, 0x46, 0xF5, 0x0A, 0x04, 0xFB};
const std::vector<uint8_t> RAC_PT1411HWRU_SWING_OFF{0xB9, 0x46, 0xF5, 0x0A, 0x05, 0xFA};
// RAC-PT1411HWRU Fan speeds
const uint8_t RAC_PT1411HWRU_FAN_OFF = 0x7B;
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_AUTO{0xBF, 0x66};
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_LOW{0x9F, 0x28};
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_MED{0x5F, 0x3C};
constexpr RacPt1411hwruFanSpeed RAC_PT1411HWRU_FAN_HIGH{0x3F, 0x64};
// RAC-PT1411HWRU Fan speed for Auto and Dry climate modes
const RacPt1411hwruFanSpeed RAC_PT1411HWRU_NO_FAN{0x1F, 0x65};
// RAC-PT1411HWRU Modes
const uint8_t RAC_PT1411HWRU_MODE_AUTO = 0x08;
const uint8_t RAC_PT1411HWRU_MODE_COOL = 0x00;
const uint8_t RAC_PT1411HWRU_MODE_DRY = 0x04;
const uint8_t RAC_PT1411HWRU_MODE_FAN = 0x04;
const uint8_t RAC_PT1411HWRU_MODE_HEAT = 0x0C;
const uint8_t RAC_PT1411HWRU_MODE_OFF = 0x00;
// RAC-PT1411HWRU Fan-only "temperature"/system off
const uint8_t RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY = 0x0E;
// RAC-PT1411HWRU temperature codes are not sequential; they instead follow a modified Gray code.
//  Hence these look-up tables. In addition, the upper nibble is used here for additional
//  "negative" and "fractional value" flags as required for some temperatures.
// RAC-PT1411HWRU °C Temperatures (short codes)
const std::vector<uint8_t> RAC_PT1411HWRU_TEMPERATURE_C{0x10, 0x00, 0x01, 0x03, 0x02, 0x06, 0x07, 0x05,
                                                        0x04, 0x0C, 0x0D, 0x09, 0x08, 0x0A, 0x0B};
// RAC-PT1411HWRU °F Temperatures (short codes)
const std::vector<uint8_t> RAC_PT1411HWRU_TEMPERATURE_F{0x10, 0x30, 0x00, 0x20, 0x01, 0x21, 0x03, 0x23, 0x02,
                                                        0x22, 0x06, 0x26, 0x07, 0x05, 0x25, 0x04, 0x24, 0x0C,
                                                        0x2C, 0x0D, 0x2D, 0x09, 0x08, 0x28, 0x0A, 0x2A, 0x0B};

// RAS-2819T protocol constants
const uint16_t RAS_2819T_HEADER1 = 0xC23D;
const uint8_t RAS_2819T_HEADER2 = 0xD5;
const uint8_t RAS_2819T_MESSAGE_LENGTH = 6;

// RAS-2819T fan speed codes for rc_code_1 (bytes 2-3)
const uint16_t RAS_2819T_FAN_AUTO = 0xBF40;
const uint16_t RAS_2819T_FAN_QUIET = 0xFF00;
const uint16_t RAS_2819T_FAN_LOW = 0x9F60;
const uint16_t RAS_2819T_FAN_MEDIUM = 0x5FA0;
const uint16_t RAS_2819T_FAN_HIGH = 0x3FC0;

// RAS-2819T fan speed codes for rc_code_2 (byte 1)
const uint8_t RAS_2819T_FAN2_AUTO = 0x66;
const uint8_t RAS_2819T_FAN2_QUIET = 0x01;
const uint8_t RAS_2819T_FAN2_LOW = 0x28;
const uint8_t RAS_2819T_FAN2_MEDIUM = 0x3C;
const uint8_t RAS_2819T_FAN2_HIGH = 0x50;

// RAS-2819T second packet suffix bytes for rc_code_2 (bytes 3-5)
// These are fixed patterns, not actual checksums
struct Ras2819tPacketSuffix {
  uint8_t byte3;
  uint8_t byte4;
  uint8_t byte5;
};
const Ras2819tPacketSuffix RAS_2819T_SUFFIX_AUTO{0x00, 0x02, 0x3D};
const Ras2819tPacketSuffix RAS_2819T_SUFFIX_QUIET{0x00, 0x02, 0xD8};
const Ras2819tPacketSuffix RAS_2819T_SUFFIX_LOW{0x00, 0x02, 0xFF};
const Ras2819tPacketSuffix RAS_2819T_SUFFIX_MEDIUM{0x00, 0x02, 0x13};
const Ras2819tPacketSuffix RAS_2819T_SUFFIX_HIGH{0x00, 0x02, 0x27};

// RAS-2819T swing toggle command
const uint64_t RAS_2819T_SWING_TOGGLE = 0xC23D6B94E01F;

// RAS-2819T single-packet commands
const uint64_t RAS_2819T_POWER_OFF_COMMAND = 0xC23D7B84E01F;

// RAS-2819T known valid command patterns for validation
const std::array<uint64_t, 2> RAS_2819T_VALID_SINGLE_COMMANDS = {
    RAS_2819T_POWER_OFF_COMMAND,  // Power off
    RAS_2819T_SWING_TOGGLE,       // Swing toggle
};

const uint16_t RAS_2819T_VALID_HEADER1 = 0xC23D;
const uint8_t RAS_2819T_VALID_HEADER2 = 0xD5;

const uint8_t RAS_2819T_DRY_BYTE2 = 0x1F;
const uint8_t RAS_2819T_DRY_BYTE3 = 0xE0;
const uint8_t RAS_2819T_DRY_TEMP_OFFSET = 0x24;

const uint8_t RAS_2819T_AUTO_BYTE2 = 0x1F;
const uint8_t RAS_2819T_AUTO_BYTE3 = 0xE0;
const uint8_t RAS_2819T_AUTO_TEMP_OFFSET = 0x08;

const uint8_t RAS_2819T_FAN_ONLY_TEMP = 0xE4;
const uint8_t RAS_2819T_FAN_ONLY_TEMP_INV = 0x1B;

const uint8_t RAS_2819T_HEAT_TEMP_OFFSET = 0x0C;

// RAS-2819T second packet fixed values
const uint8_t RAS_2819T_AUTO_DRY_FAN_BYTE = 0x65;
const uint8_t RAS_2819T_AUTO_DRY_SUFFIX = 0x3A;
const uint8_t RAS_2819T_HEAT_SUFFIX = 0x3B;

// RAS-2819T temperature codes for 18-30°C
static const uint8_t RAS_2819T_TEMP_CODES[] = {
    0x10,  // 18°C
    0x30,  // 19°C
    0x20,  // 20°C
    0x60,  // 21°C
    0x70,  // 22°C
    0x50,  // 23°C
    0x40,  // 24°C
    0xC0,  // 25°C
    0xD0,  // 26°C
    0x90,  // 27°C
    0x80,  // 28°C
    0xA0,  // 29°C
    0xB0   // 30°C
};

// Helper functions for RAS-2819T protocol
//
// ===== RAS-2819T PROTOCOL DOCUMENTATION =====
//
// The RAS-2819T uses a two-packet IR protocol with some exceptions for simple commands.
//
// PACKET STRUCTURE:
// All packets are 6 bytes (48 bits) transmitted with standard Toshiba timing.
//
// TWO-PACKET COMMANDS (Mode/Temperature/Fan changes):
//
// First Packet (rc_code_1):  [C2 3D] [FAN_HI FAN_LO] [TEMP] [~TEMP]
//   Byte 0-1: Header (always 0xC23D)
//   Byte 2-3: Fan speed encoding (varies by mode, see fan tables below)
//   Byte 4:   Temperature + mode encoding
//   Byte 5:   Bitwise complement of temperature byte
//
// Second Packet (rc_code_2): [D5] [FAN2] [00] [SUF1] [SUF2] [SUF3]
//   Byte 0:   Header (always 0xD5)
//   Byte 1:   Fan speed secondary encoding
//   Byte 2:   Always 0x00
//   Byte 3-5: Fixed suffix pattern (depends on fan speed and mode)
//
// TEMPERATURE ENCODING:
// Base temp codes: 18°C=0x10, 19°C=0x30, 20°C=0x20, 21°C=0x60, 22°C=0x70,
//                  23°C=0x50, 24°C=0x40, 25°C=0xC0, 26°C=0xD0, 27°C=0x90,
//                  28°C=0x80, 29°C=0xA0, 30°C=0xB0
// Mode offsets added to base temp:
//   COOL: No offset
//   HEAT: +0x0C (e.g., 24°C heat = 0x40 | 0x0C = 0x4C)
//   AUTO: +0x08 (e.g., 24°C auto = 0x40 | 0x08 = 0x48)
//   DRY:  +0x24 (e.g., 24°C dry = 0x40 | 0x24 = 0x64)
//
// FAN SPEED ENCODING (First packet bytes 2-3):
//   AUTO: 0xBF40, QUIET: 0xFF00, LOW: 0x9F60, MEDIUM: 0x5FA0, HIGH: 0x3FC0
//   Special cases: AUTO/DRY modes use 0x1FE0 instead
//
// SINGLE-PACKET COMMANDS:
// Power Off: 0xC23D7B84E01F (6 bytes, no second packet)
// Swing Toggle: 0xC23D6B94E01F (6 bytes, no second packet)
//
// MODE DETECTION (from first packet):
// - Check bytes 2-3: if 0x7B84 → OFF mode
// - Check bytes 2-3: if 0x1FE0 → AUTO/DRY/low-temp-COOL (distinguish by temp code)
// - Otherwise: COOL/HEAT/FAN_ONLY (distinguish by temp code and byte 5)

/**
 * Get fan speed encoding for RAS-2819T first packet (rc_code_1, bytes 2-3)
 */
static uint16_t get_ras_2819t_fan_code(climate::ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_QUIET:
      return RAS_2819T_FAN_QUIET;
    case climate::CLIMATE_FAN_LOW:
      return RAS_2819T_FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM:
      return RAS_2819T_FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:
      return RAS_2819T_FAN_HIGH;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return RAS_2819T_FAN_AUTO;
  }
}

/**
 * Get fan speed encoding for RAS-2819T rc_code_2 packet (second packet)
 */
struct Ras2819tSecondPacketCodes {
  uint8_t fan_byte;
  Ras2819tPacketSuffix suffix;
};

static Ras2819tSecondPacketCodes get_ras_2819t_second_packet_codes(climate::ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_QUIET:
      return {RAS_2819T_FAN2_QUIET, RAS_2819T_SUFFIX_QUIET};
    case climate::CLIMATE_FAN_LOW:
      return {RAS_2819T_FAN2_LOW, RAS_2819T_SUFFIX_LOW};
    case climate::CLIMATE_FAN_MEDIUM:
      return {RAS_2819T_FAN2_MEDIUM, RAS_2819T_SUFFIX_MEDIUM};
    case climate::CLIMATE_FAN_HIGH:
      return {RAS_2819T_FAN2_HIGH, RAS_2819T_SUFFIX_HIGH};
    case climate::CLIMATE_FAN_AUTO:
    default:
      return {RAS_2819T_FAN2_AUTO, RAS_2819T_SUFFIX_AUTO};
  }
}

/**
 * Get temperature code for RAS-2819T protocol
 */
static uint8_t get_ras_2819t_temp_code(float temperature) {
  int temp_index = static_cast<int>(temperature) - 18;
  if (temp_index < 0 || temp_index >= static_cast<int>(sizeof(RAS_2819T_TEMP_CODES))) {
    ESP_LOGW(TAG, "Temperature %.1f°C out of range [18-30°C], defaulting to 24°C", temperature);
    return 0x40;  // Default to 24°C
  }

  return RAS_2819T_TEMP_CODES[temp_index];
}

/**
 * Decode temperature from RAS-2819T temp code
 */
static float decode_ras_2819t_temperature(uint8_t temp_code) {
  uint8_t base_temp_code = temp_code & 0xF0;

  // Find the code in the temperature array
  for (size_t temp_index = 0; temp_index < sizeof(RAS_2819T_TEMP_CODES); temp_index++) {
    if (RAS_2819T_TEMP_CODES[temp_index] == base_temp_code) {
      return static_cast<float>(temp_index + 18);  // 18°C is the minimum
    }
  }

  ESP_LOGW(TAG, "Unknown temp code: 0x%02X, defaulting to 24°C", base_temp_code);
  return 24.0f;  // Default to 24°C
}

/**
 * Decode fan speed from RAS-2819T IR codes
 */
static climate::ClimateFanMode decode_ras_2819t_fan_mode(uint16_t fan_code) {
  switch (fan_code) {
    case RAS_2819T_FAN_QUIET:
      return climate::CLIMATE_FAN_QUIET;
    case RAS_2819T_FAN_LOW:
      return climate::CLIMATE_FAN_LOW;
    case RAS_2819T_FAN_MEDIUM:
      return climate::CLIMATE_FAN_MEDIUM;
    case RAS_2819T_FAN_HIGH:
      return climate::CLIMATE_FAN_HIGH;
    case RAS_2819T_FAN_AUTO:
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

/**
 * Validate RAS-2819T IR command structure and content
 */
static bool is_valid_ras_2819t_command(uint64_t rc_code_1, uint64_t rc_code_2 = 0) {
  // Check header of first packet
  uint16_t header1 = (rc_code_1 >> 32) & 0xFFFF;
  if (header1 != RAS_2819T_VALID_HEADER1) {
    return false;
  }

  // Single packet commands
  if (rc_code_2 == 0) {
    for (uint64_t valid_cmd : RAS_2819T_VALID_SINGLE_COMMANDS) {
      if (rc_code_1 == valid_cmd) {
        return true;
      }
    }
    // Additional validation for unknown single packets
    return false;
  }

  // Two-packet commands - validate second packet header
  uint8_t header2 = (rc_code_2 >> 40) & 0xFF;
  if (header2 != RAS_2819T_VALID_HEADER2) {
    return false;
  }

  // Validate temperature complement in first packet (byte 4 should be ~byte 5)
  uint8_t temp_byte = (rc_code_1 >> 8) & 0xFF;
  uint8_t temp_complement = rc_code_1 & 0xFF;
  if (temp_byte != static_cast<uint8_t>(~temp_complement)) {
    return false;
  }

  // Validate fan speed combinations make sense
  uint16_t fan_code = (rc_code_1 >> 16) & 0xFFFF;
  uint8_t fan2_byte = (rc_code_2 >> 32) & 0xFF;

  // Check if fan codes are from known valid patterns
  bool valid_fan_combo = false;
  if (fan_code == RAS_2819T_FAN_AUTO && fan2_byte == RAS_2819T_FAN2_AUTO)
    valid_fan_combo = true;
  if (fan_code == RAS_2819T_FAN_QUIET && fan2_byte == RAS_2819T_FAN2_QUIET)
    valid_fan_combo = true;
  if (fan_code == RAS_2819T_FAN_LOW && fan2_byte == RAS_2819T_FAN2_LOW)
    valid_fan_combo = true;
  if (fan_code == RAS_2819T_FAN_MEDIUM && fan2_byte == RAS_2819T_FAN2_MEDIUM)
    valid_fan_combo = true;
  if (fan_code == RAS_2819T_FAN_HIGH && fan2_byte == RAS_2819T_FAN2_HIGH)
    valid_fan_combo = true;
  if (fan_code == 0x1FE0 && fan2_byte == RAS_2819T_AUTO_DRY_FAN_BYTE)
    valid_fan_combo = true;  // AUTO/DRY

  return valid_fan_combo;
}

void ToshibaClimate::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      this->transmit_rac_pt1411hwru_temp_();
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else {
    this->current_temperature = NAN;
  }
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature =
        roundf(clamp<float>(this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  // Set supported modes & temperatures based on model
  this->minimum_temperature_ = this->temperature_min_();
  this->maximum_temperature_ = this->temperature_max_();
  this->swing_modes_ = this->toshiba_swing_modes_();

  // Ensure swing mode is always initialized to a valid value
  if (this->swing_modes_.empty() || !this->swing_modes_.count(this->swing_mode)) {
    // No swing support for this model or current swing mode not supported, reset to OFF
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  // Ensure mode is valid - ESPHome should only use standard climate modes
  if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_HEAT &&
      this->mode != climate::CLIMATE_MODE_COOL && this->mode != climate::CLIMATE_MODE_HEAT_COOL &&
      this->mode != climate::CLIMATE_MODE_DRY && this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
    ESP_LOGW(TAG, "Invalid mode detected during setup, resetting to OFF");
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  // Ensure fan mode is valid
  if (!this->fan_mode.has_value()) {
    ESP_LOGW(TAG, "Fan mode not set during setup, defaulting to AUTO");
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  }

  // Never send nan to HA
  if (std::isnan(this->target_temperature))
    this->target_temperature = 24;
  // Log final state for debugging HA errors
  ESP_LOGV(TAG, "Setup complete - Mode: %d, Fan: %s, Swing: %d, Temp: %.1f", static_cast<int>(this->mode),
           this->fan_mode.has_value() ? std::to_string(static_cast<int>(this->fan_mode.value())).c_str() : "NONE",
           static_cast<int>(this->swing_mode), this->target_temperature);
}

void ToshibaClimate::transmit_state() {
  if (this->model_ == MODEL_RAC_PT1411HWRU_C || this->model_ == MODEL_RAC_PT1411HWRU_F) {
    this->transmit_rac_pt1411hwru_();
  } else if (this->model_ == MODEL_RAS_2819T) {
    this->transmit_ras_2819t_();
  } else {
    this->transmit_generic_();
  }
}

void ToshibaClimate::transmit_generic_() {
  uint8_t message[16] = {0};
  uint8_t message_length = 9;

  // Header
  message[0] = 0xf2;
  message[1] = 0x0d;

  // Message length
  message[2] = message_length - 6;

  // First checksum
  message[3] = message[0] ^ message[1] ^ message[2];

  // Command
  message[4] = TOSHIBA_COMMAND_DEFAULT;

  // Temperature
  uint8_t temperature = static_cast<uint8_t>(
      clamp<float>(this->target_temperature, TOSHIBA_GENERIC_TEMP_C_MIN, TOSHIBA_GENERIC_TEMP_C_MAX));
  message[5] = (temperature - static_cast<uint8_t>(TOSHIBA_GENERIC_TEMP_C_MIN)) << 4;

  // Mode and fan
  uint8_t mode;
  switch (this->mode) {
    case climate::CLIMATE_MODE_OFF:
      mode = TOSHIBA_MODE_OFF;
      break;

    case climate::CLIMATE_MODE_HEAT:
      mode = TOSHIBA_MODE_HEAT;
      break;

    case climate::CLIMATE_MODE_COOL:
      mode = TOSHIBA_MODE_COOL;
      break;

    case climate::CLIMATE_MODE_DRY:
      mode = TOSHIBA_MODE_DRY;
      break;

    case climate::CLIMATE_MODE_FAN_ONLY:
      mode = TOSHIBA_MODE_FAN_ONLY;
      break;

    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      mode = TOSHIBA_MODE_AUTO;
  }

  uint8_t fan;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_QUIET:
      fan = TOSHIBA_FAN_SPEED_QUIET;
      break;

    case climate::CLIMATE_FAN_LOW:
      fan = TOSHIBA_FAN_SPEED_1;
      break;

    case climate::CLIMATE_FAN_MEDIUM:
      fan = TOSHIBA_FAN_SPEED_3;
      break;

    case climate::CLIMATE_FAN_HIGH:
      fan = TOSHIBA_FAN_SPEED_5;
      break;

    case climate::CLIMATE_FAN_AUTO:
    default:
      fan = TOSHIBA_FAN_SPEED_AUTO;
      break;
  }
  message[6] = fan | mode;

  // Zero
  message[7] = 0x00;

  // If timers bit in the command is set, two extra bytes are added here

  // If power bit is set in the command, one extra byte is added here

  // The last byte is the xor of all bytes from [4]
  for (uint8_t i = 4; i < 8; i++) {
    message[8] ^= message[i];
  }

  // Transmit
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  this->encode_(data, message, message_length, 1);

  transmit.perform();
}

void ToshibaClimate::transmit_rac_pt1411hwru_() {
  uint8_t code = 0, index = 0, message[RAC_PT1411HWRU_MESSAGE_LENGTH * 2] = {0};
  float temperature =
      clamp<float>(this->target_temperature, TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN, TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX);
  float temp_adjd = temperature - TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  // Byte 0:  Header upper (0xB2)
  message[0] = RAC_PT1411HWRU_MESSAGE_HEADER0;
  // Byte 1:  Header lower (0x4D)
  message[1] = ~message[0];
  // Byte 2u: Fan speed
  // Byte 2l: 1111 (on) or 1011 (off)
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    message[2] = RAC_PT1411HWRU_FAN_OFF;
  } else if ((this->mode == climate::CLIMATE_MODE_HEAT_COOL) || (this->mode == climate::CLIMATE_MODE_DRY)) {
    message[2] = RAC_PT1411HWRU_NO_FAN.code1;
    message[7] = RAC_PT1411HWRU_NO_FAN.code2;
  } else {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_LOW:
        message[2] = RAC_PT1411HWRU_FAN_LOW.code1;
        message[7] = RAC_PT1411HWRU_FAN_LOW.code2;
        break;

      case climate::CLIMATE_FAN_MEDIUM:
        message[2] = RAC_PT1411HWRU_FAN_MED.code1;
        message[7] = RAC_PT1411HWRU_FAN_MED.code2;
        break;

      case climate::CLIMATE_FAN_HIGH:
        message[2] = RAC_PT1411HWRU_FAN_HIGH.code1;
        message[7] = RAC_PT1411HWRU_FAN_HIGH.code2;
        break;

      case climate::CLIMATE_FAN_AUTO:
      default:
        message[2] = RAC_PT1411HWRU_FAN_AUTO.code1;
        message[7] = RAC_PT1411HWRU_FAN_AUTO.code2;
    }
  }
  // Byte 3u: ~Fan speed
  // Byte 3l: 0000 (on) or 0100 (off)
  message[3] = ~message[2];
  // Byte 4u: Temp
  if (this->model_ == MODEL_RAC_PT1411HWRU_F) {
    temperature = (temperature * 1.8) + 32;
    temp_adjd = temperature - TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN;
  }

  index = static_cast<uint8_t>(roundf(temp_adjd));

  if (this->model_ == MODEL_RAC_PT1411HWRU_F) {
    code = RAC_PT1411HWRU_TEMPERATURE_F[index];
    message[9] |= RAC_PT1411HWRU_FLAG_FAH;
  } else {
    code = RAC_PT1411HWRU_TEMPERATURE_C[index];
  }
  if ((this->mode == climate::CLIMATE_MODE_FAN_ONLY) || (this->mode == climate::CLIMATE_MODE_OFF)) {
    code = RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY;
  }

  if (code & RAC_PT1411HWRU_FLAG_FRAC) {
    message[8] |= RAC_PT1411HWRU_FLAG_FRAC;
  }
  if (code & RAC_PT1411HWRU_FLAG_NEG) {
    message[9] |= RAC_PT1411HWRU_FLAG_NEG;
  }
  message[4] = (code & RAC_PT1411HWRU_FLAG_MASK) << 4;
  // Byte 4l: Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_OFF:
      // zerooooo
      break;

    case climate::CLIMATE_MODE_HEAT:
      message[4] |= RAC_PT1411HWRU_MODE_HEAT;
      break;

    case climate::CLIMATE_MODE_COOL:
      message[4] |= RAC_PT1411HWRU_MODE_COOL;
      break;

    case climate::CLIMATE_MODE_DRY:
      message[4] |= RAC_PT1411HWRU_MODE_DRY;
      break;

    case climate::CLIMATE_MODE_FAN_ONLY:
      message[4] |= RAC_PT1411HWRU_MODE_FAN;
      break;

    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      message[4] |= RAC_PT1411HWRU_MODE_AUTO;
  }

  // Byte 5u: ~Temp
  // Byte 5l: ~Mode
  message[5] = ~message[4];

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    // Byte 6:  Header (0xD5)
    message[6] = RAC_PT1411HWRU_MESSAGE_HEADER1;
    // Byte 7:  Fan speed part 2 (done above)
    // Byte 8: 0x20 for °F frac, else 0 (done above)
    // Byte 9: 0x10=NEG, 0x01=°F (done above)
    // Byte 10: 0
    // Byte 11: Checksum (bytes 6 through 10)
    for (index = 6; index <= 10; index++) {
      message[11] += message[index];
    }
  }

  // load first block of IR code and repeat it once
  this->encode_(data, &message[0], RAC_PT1411HWRU_MESSAGE_LENGTH, 1);
  // load second block of IR code, if present
  if (message[6] != 0) {
    this->encode_(data, &message[6], RAC_PT1411HWRU_MESSAGE_LENGTH, 0);
  }

  transmit.perform();

  // Swing Mode
  data->reset();
  data->space(TOSHIBA_PACKET_SPACE);
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      this->encode_(data, &RAC_PT1411HWRU_SWING_VERTICAL[0], RAC_PT1411HWRU_MESSAGE_LENGTH, 1);
      break;

    case climate::CLIMATE_SWING_OFF:
    default:
      this->encode_(data, &RAC_PT1411HWRU_SWING_OFF[0], RAC_PT1411HWRU_MESSAGE_LENGTH, 1);
  }

  data->space(TOSHIBA_PACKET_SPACE);
  transmit.perform();

  if (this->sensor_) {
    this->transmit_rac_pt1411hwru_temp_(true, false);
  }
}

void ToshibaClimate::transmit_rac_pt1411hwru_temp_(const bool cs_state, const bool cs_send_update) {
  if ((this->mode == climate::CLIMATE_MODE_HEAT) || (this->mode == climate::CLIMATE_MODE_COOL) ||
      (this->mode == climate::CLIMATE_MODE_HEAT_COOL)) {
    uint8_t message[RAC_PT1411HWRU_MESSAGE_LENGTH] = {0};
    float temperature = clamp<float>(this->current_temperature, 0.0, TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX + 1);
    auto transmit = this->transmitter_->transmit();
    auto *data = transmit.get_data();
    // "Comfort Sense" feature notes
    // IR Code: 0xBA45 xxXX yyYY
    // xx: Temperature in °C
    //     Bit 6: feature state (on/off)
    //     Bit 7: message contains temperature data for feature (bit 6 must also be set)
    // XX: Bitwise complement of xx
    // yy: Mode: Auto=0x7A, Cool=0x72, Heat=0x7E
    // YY: Bitwise complement of yy
    //
    // Byte 0:  Header upper (0xBA)
    message[0] = RAC_PT1411HWRU_CS_HEADER;
    // Byte 1:  Header lower (0x45)
    message[1] = ~message[0];
    // Byte 2: Temperature in °C
    message[2] = static_cast<uint8_t>(roundf(temperature));
    if (cs_send_update) {
      message[2] |= RAC_PT1411HWRU_CS_ENABLED | RAC_PT1411HWRU_CS_DATA;
    } else if (cs_state) {
      message[2] |= RAC_PT1411HWRU_CS_ENABLED;
    }
    // Byte 3: Bitwise complement of byte 2
    message[3] = ~message[2];
    // Byte 4: Footer upper
    switch (this->mode) {
      case climate::CLIMATE_MODE_HEAT:
        message[4] = RAC_PT1411HWRU_CS_FOOTER_HEAT;
        break;

      case climate::CLIMATE_MODE_COOL:
        message[4] = RAC_PT1411HWRU_CS_FOOTER_COOL;
        break;

      case climate::CLIMATE_MODE_HEAT_COOL:
        message[4] = RAC_PT1411HWRU_CS_FOOTER_AUTO;

      default:
        break;
    }
    // Byte 5: Footer lower/bitwise complement of byte 4
    message[5] = ~message[4];

    // load IR code and repeat it once
    this->encode_(data, message, RAC_PT1411HWRU_MESSAGE_LENGTH, 1);

    transmit.perform();
  }
}

void ToshibaClimate::transmit_ras_2819t_() {
  // Handle swing mode transmission for RAS-2819T
  // Note: RAS-2819T uses a toggle command, so we need to track state changes

  // Check if ONLY swing mode changed (and no other climate parameters)
  bool swing_changed = (this->swing_mode != this->last_swing_mode_);
  bool mode_changed = (this->mode != this->last_mode_);
  bool fan_changed = (this->fan_mode != this->last_fan_mode_);
  bool temp_changed = (abs(this->target_temperature - this->last_target_temperature_) > 0.1f);

  bool only_swing_changed = swing_changed && !mode_changed && !fan_changed && !temp_changed;

  if (only_swing_changed) {
    // Send ONLY swing toggle command (like the physical remote does)
    auto swing_transmit = this->transmitter_->transmit();
    auto *swing_data = swing_transmit.get_data();

    // Convert toggle command to bytes for transmission
    uint8_t swing_message[RAS_2819T_MESSAGE_LENGTH];
    swing_message[0] = (RAS_2819T_SWING_TOGGLE >> 40) & 0xFF;
    swing_message[1] = (RAS_2819T_SWING_TOGGLE >> 32) & 0xFF;
    swing_message[2] = (RAS_2819T_SWING_TOGGLE >> 24) & 0xFF;
    swing_message[3] = (RAS_2819T_SWING_TOGGLE >> 16) & 0xFF;
    swing_message[4] = (RAS_2819T_SWING_TOGGLE >> 8) & 0xFF;
    swing_message[5] = RAS_2819T_SWING_TOGGLE & 0xFF;

    // Use single packet transmission WITH repeat (like regular commands)
    this->encode_(swing_data, swing_message, RAS_2819T_MESSAGE_LENGTH, 1);
    swing_transmit.perform();

    // Update all state tracking
    this->last_swing_mode_ = this->swing_mode;
    this->last_mode_ = this->mode;
    this->last_fan_mode_ = this->fan_mode;
    this->last_target_temperature_ = this->target_temperature;

    // Immediately publish the state change to Home Assistant
    this->publish_state();

    return;  // Exit early - don't send climate command
  }

  // If we get here, send the regular climate command (temperature/mode/fan)
  uint8_t message1[RAS_2819T_MESSAGE_LENGTH] = {0};
  uint8_t message2[RAS_2819T_MESSAGE_LENGTH] = {0};
  float temperature =
      clamp<float>(this->target_temperature, TOSHIBA_RAS_2819T_TEMP_C_MIN, TOSHIBA_RAS_2819T_TEMP_C_MAX);

  // Build first packet (RAS_2819T_HEADER1 + 4 bytes)
  message1[0] = (RAS_2819T_HEADER1 >> 8) & 0xFF;
  message1[1] = RAS_2819T_HEADER1 & 0xFF;

  // Handle OFF mode
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    // Extract bytes from power off command constant
    message1[2] = (RAS_2819T_POWER_OFF_COMMAND >> 24) & 0xFF;
    message1[3] = (RAS_2819T_POWER_OFF_COMMAND >> 16) & 0xFF;
    message1[4] = (RAS_2819T_POWER_OFF_COMMAND >> 8) & 0xFF;
    message1[5] = RAS_2819T_POWER_OFF_COMMAND & 0xFF;
    // No second packet for OFF
  } else {
    // Get temperature and fan encoding
    uint8_t temp_code = get_ras_2819t_temp_code(temperature);

    // Get fan speed encoding for rc_code_1
    climate::ClimateFanMode effective_fan_mode = this->fan_mode.value();

    // Dry mode only supports AUTO fan speed
    if (this->mode == climate::CLIMATE_MODE_DRY) {
      effective_fan_mode = climate::CLIMATE_FAN_AUTO;
      if (this->fan_mode.value() != climate::CLIMATE_FAN_AUTO) {
        ESP_LOGW(TAG, "Dry mode only supports AUTO fan speed, forcing AUTO");
      }
    }

    uint16_t fan_code = get_ras_2819t_fan_code(effective_fan_mode);

    // Mode and temperature encoding
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:
        // All cooling temperatures support fan speed control
        message1[2] = (fan_code >> 8) & 0xFF;
        message1[3] = fan_code & 0xFF;
        message1[4] = temp_code;
        message1[5] = ~temp_code;
        break;

      case climate::CLIMATE_MODE_HEAT:
        // Heating supports fan speed control
        message1[2] = (fan_code >> 8) & 0xFF;
        message1[3] = fan_code & 0xFF;
        // Heat mode adds offset to temperature code
        message1[4] = temp_code | RAS_2819T_HEAT_TEMP_OFFSET;
        message1[5] = ~(temp_code | RAS_2819T_HEAT_TEMP_OFFSET);
        break;

      case climate::CLIMATE_MODE_HEAT_COOL:
        // Auto mode uses fixed encoding
        message1[2] = RAS_2819T_AUTO_BYTE2;
        message1[3] = RAS_2819T_AUTO_BYTE3;
        message1[4] = temp_code | RAS_2819T_AUTO_TEMP_OFFSET;
        message1[5] = ~(temp_code | RAS_2819T_AUTO_TEMP_OFFSET);
        break;

      case climate::CLIMATE_MODE_DRY:
        // Dry mode uses fixed encoding and forces AUTO fan
        message1[2] = RAS_2819T_DRY_BYTE2;
        message1[3] = RAS_2819T_DRY_BYTE3;
        message1[4] = temp_code | RAS_2819T_DRY_TEMP_OFFSET;
        message1[5] = ~message1[4];
        break;

      case climate::CLIMATE_MODE_FAN_ONLY:
        // Fan only mode supports fan speed control
        message1[2] = (fan_code >> 8) & 0xFF;
        message1[3] = fan_code & 0xFF;
        message1[4] = RAS_2819T_FAN_ONLY_TEMP;
        message1[5] = RAS_2819T_FAN_ONLY_TEMP_INV;
        break;

      default:
        // Default case supports fan speed control
        message1[2] = (fan_code >> 8) & 0xFF;
        message1[3] = fan_code & 0xFF;
        message1[4] = temp_code;
        message1[5] = ~temp_code;
        break;
    }

    // Build second packet (RAS_2819T_HEADER2 + 4 bytes)
    message2[0] = RAS_2819T_HEADER2;

    // Get fan speed encoding for rc_code_2
    Ras2819tSecondPacketCodes second_packet_codes = get_ras_2819t_second_packet_codes(effective_fan_mode);

    // Determine header byte 2 and fan encoding based on mode
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:
        message2[1] = second_packet_codes.fan_byte;
        message2[2] = 0x00;
        message2[3] = second_packet_codes.suffix.byte3;
        message2[4] = second_packet_codes.suffix.byte4;
        message2[5] = second_packet_codes.suffix.byte5;
        break;

      case climate::CLIMATE_MODE_HEAT:
        message2[1] = second_packet_codes.fan_byte;
        message2[2] = 0x00;
        message2[3] = second_packet_codes.suffix.byte3;
        message2[4] = 0x00;
        message2[5] = RAS_2819T_HEAT_SUFFIX;
        break;

      case climate::CLIMATE_MODE_HEAT_COOL:
      case climate::CLIMATE_MODE_DRY:
        // Auto/Dry modes use fixed values regardless of fan setting
        message2[1] = RAS_2819T_AUTO_DRY_FAN_BYTE;
        message2[2] = 0x00;
        message2[3] = 0x00;
        message2[4] = 0x00;
        message2[5] = RAS_2819T_AUTO_DRY_SUFFIX;
        break;

      case climate::CLIMATE_MODE_FAN_ONLY:
        message2[1] = second_packet_codes.fan_byte;
        message2[2] = 0x00;
        message2[3] = second_packet_codes.suffix.byte3;
        message2[4] = 0x00;
        message2[5] = RAS_2819T_HEAT_SUFFIX;
        break;

      default:
        message2[1] = second_packet_codes.fan_byte;
        message2[2] = 0x00;
        message2[3] = second_packet_codes.suffix.byte3;
        message2[4] = second_packet_codes.suffix.byte4;
        message2[5] = second_packet_codes.suffix.byte5;
        break;
    }
  }

  // Log final messages being transmitted

  // Transmit using proper Toshiba protocol timing
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  // Use existing Toshiba encode function for proper timing
  this->encode_(data, message1, RAS_2819T_MESSAGE_LENGTH, 1);

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    // Send second packet with gap
    this->encode_(data, message2, RAS_2819T_MESSAGE_LENGTH, 0);
  }

  transmit.perform();

  // Update all state tracking after successful transmission
  this->last_swing_mode_ = this->swing_mode;
  this->last_mode_ = this->mode;
  this->last_fan_mode_ = this->fan_mode;
  this->last_target_temperature_ = this->target_temperature;
}

uint8_t ToshibaClimate::is_valid_rac_pt1411hwru_header_(const uint8_t *message) {
  const std::vector<uint8_t> header{RAC_PT1411HWRU_MESSAGE_HEADER0, RAC_PT1411HWRU_CS_HEADER,
                                    RAC_PT1411HWRU_SWING_HEADER};

  for (auto i : header) {
    if ((message[0] == i) && (message[1] == static_cast<uint8_t>(~i)))
      return i;
  }
  if (message[0] == RAC_PT1411HWRU_MESSAGE_HEADER1)
    return RAC_PT1411HWRU_MESSAGE_HEADER1;

  return 0;
}

bool ToshibaClimate::compare_rac_pt1411hwru_packets_(const uint8_t *message1, const uint8_t *message2) {
  for (uint8_t i = 0; i < RAC_PT1411HWRU_MESSAGE_LENGTH; i++) {
    if (message1[i] != message2[i])
      return false;
  }
  return true;
}

bool ToshibaClimate::is_valid_rac_pt1411hwru_message_(const uint8_t *message) {
  uint8_t checksum = 0;

  switch (this->is_valid_rac_pt1411hwru_header_(message)) {
    case RAC_PT1411HWRU_MESSAGE_HEADER0:
    case RAC_PT1411HWRU_CS_HEADER:
    case RAC_PT1411HWRU_SWING_HEADER:
      if (this->is_valid_rac_pt1411hwru_header_(message) && (message[2] == static_cast<uint8_t>(~message[3])) &&
          (message[4] == static_cast<uint8_t>(~message[5]))) {
        return true;
      }
      break;

    case RAC_PT1411HWRU_MESSAGE_HEADER1:
      for (uint8_t i = 0; i < RAC_PT1411HWRU_MESSAGE_LENGTH - 1; i++) {
        checksum += message[i];
      }
      if (checksum == message[RAC_PT1411HWRU_MESSAGE_LENGTH - 1]) {
        return true;
      }
      break;

    default:
      return false;
  }

  return false;
}

bool ToshibaClimate::process_ras_2819t_command_(const remote_base::ToshibaAcData &toshiba_data) {
  // Check for power-off command (single packet)
  if (toshiba_data.rc_code_2 == 0 && toshiba_data.rc_code_1 == RAS_2819T_POWER_OFF_COMMAND) {
    this->mode = climate::CLIMATE_MODE_OFF;
    ESP_LOGI(TAG, "Mode: OFF");
    this->publish_state();
    return true;
  }

  // Check for swing toggle command (single packet)
  if (toshiba_data.rc_code_2 == 0 && toshiba_data.rc_code_1 == RAS_2819T_SWING_TOGGLE) {
    // Toggle swing mode
    if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
      ESP_LOGI(TAG, "Swing: OFF");
    } else {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      ESP_LOGI(TAG, "Swing: VERTICAL");
    }
    this->publish_state();
    return true;
  }

  // Handle regular two-packet commands (mode/temperature/fan changes)
  if (toshiba_data.rc_code_2 != 0) {
    // Convert to byte array for easier processing
    uint8_t message1[6], message2[6];
    for (uint8_t i = 0; i < 6; i++) {
      message1[i] = (toshiba_data.rc_code_1 >> (40 - i * 8)) & 0xFF;
      message2[i] = (toshiba_data.rc_code_2 >> (40 - i * 8)) & 0xFF;
    }

    // Decode the protocol using message1 (rc_code_1)
    uint8_t temp_code = message1[4];

    // Decode mode - check bytes 2-3 pattern and temperature code
    if ((message1[2] == 0x7B) && (message1[3] == 0x84)) {
      // OFF mode has specific pattern
      this->mode = climate::CLIMATE_MODE_OFF;
      ESP_LOGI(TAG, "Mode: OFF");
    } else if ((message1[2] == 0x1F) && (message1[3] == 0xE0)) {
      // 0x1FE0 pattern is used for AUTO, DRY, and low-temp COOL
      if ((temp_code & 0x0F) == 0x08) {
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        ESP_LOGI(TAG, "Mode: AUTO");
      } else if ((temp_code & 0x0F) == 0x04) {
        this->mode = climate::CLIMATE_MODE_DRY;
        ESP_LOGI(TAG, "Mode: DRY");
      } else {
        this->mode = climate::CLIMATE_MODE_COOL;
        ESP_LOGI(TAG, "Mode: COOL (low temp)");
      }
    } else {
      // Variable fan speed patterns - decode by temperature code
      if ((temp_code & 0x0F) == 0x0C) {
        this->mode = climate::CLIMATE_MODE_HEAT;
        ESP_LOGI(TAG, "Mode: HEAT");
      } else if (message1[5] == 0x1B) {
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        ESP_LOGI(TAG, "Mode: FAN_ONLY");
      } else {
        this->mode = climate::CLIMATE_MODE_COOL;
        ESP_LOGI(TAG, "Mode: COOL");
      }
    }

    // Decode fan speed from rc_code_1
    uint16_t fan_code = (message1[2] << 8) | message1[3];
    this->fan_mode = decode_ras_2819t_fan_mode(fan_code);

    // Decode temperature
    if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
      this->target_temperature = decode_ras_2819t_temperature(temp_code);
    }

    this->publish_state();
    return true;
  } else {
    ESP_LOGD(TAG, "Unknown single-packet RAS-2819T command: 0x%" PRIX64, toshiba_data.rc_code_1);
    return false;
  }
}

bool ToshibaClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Try modern ToshibaAcProtocol decoder first (handles RAS-2819T and potentially others)
  remote_base::ToshibaAcProtocol toshiba_protocol;
  auto decode_result = toshiba_protocol.decode(data);

  if (decode_result.has_value()) {
    auto toshiba_data = decode_result.value();
    // Validate and process RAS-2819T commands
    if (is_valid_ras_2819t_command(toshiba_data.rc_code_1, toshiba_data.rc_code_2)) {
      return this->process_ras_2819t_command_(toshiba_data);
    }
  }

  // Fall back to generic processing for older protocols
  uint8_t message[18] = {0};
  uint8_t message_length = TOSHIBA_HEADER_LENGTH, temperature_code = 0;

  // Validate header
  if (!data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE)) {
    return false;
  }
  // Read incoming bits into buffer
  if (!this->decode_(&data, message, message_length)) {
    return false;
  }
  // Determine incoming message protocol version and/or length
  if (this->is_valid_rac_pt1411hwru_header_(message)) {
    // We already received four bytes
    message_length = RAC_PT1411HWRU_MESSAGE_LENGTH - 4;
  } else if ((message[0] ^ message[1] ^ message[2]) != message[3]) {
    // Return false if first checksum was not valid
    return false;
  } else {
    // First checksum was valid so continue receiving the remaining bits
    message_length = message[2] + 2;
  }
  // Decode the remaining bytes
  if (!this->decode_(&data, &message[4], message_length)) {
    return false;
  }
  // If this is a RAC-PT1411HWRU message, we expect the first packet a second time and also possibly a third packet
  if (this->is_valid_rac_pt1411hwru_header_(message)) {
    // There is always a space between packets
    if (!data.expect_item(TOSHIBA_BIT_MARK, TOSHIBA_GAP_SPACE)) {
      return false;
    }
    // Validate header 2
    if (!data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE)) {
      return false;
    }
    if (!this->decode_(&data, &message[6], RAC_PT1411HWRU_MESSAGE_LENGTH)) {
      return false;
    }
    // If this is a RAC-PT1411HWRU message, there may also be a third packet.
    // We do not fail the receive if we don't get this; it isn't always present
    if (data.expect_item(TOSHIBA_BIT_MARK, TOSHIBA_GAP_SPACE)) {
      // Validate header 3
      data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE);
      if (this->decode_(&data, &message[12], RAC_PT1411HWRU_MESSAGE_LENGTH)) {
        if (!this->is_valid_rac_pt1411hwru_message_(&message[12])) {
          // If a third packet was received but the checksum is not valid, fail
          return false;
        }
      }
    }
    if (!this->compare_rac_pt1411hwru_packets_(&message[0], &message[6])) {
      // If the first two packets don't match each other, fail
      return false;
    }
    if (!this->is_valid_rac_pt1411hwru_message_(&message[0])) {
      // If the first packet isn't valid, fail
      return false;
    }
  }

  // Header has been verified, now determine protocol version and set the climate component properties
  switch (this->is_valid_rac_pt1411hwru_header_(message)) {
    // Power, temperature, mode, fan speed
    case RAC_PT1411HWRU_MESSAGE_HEADER0:
      // Get the mode
      switch (message[4] & 0x0F) {
        case RAC_PT1411HWRU_MODE_AUTO:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
          break;

        // case RAC_PT1411HWRU_MODE_OFF:
        case RAC_PT1411HWRU_MODE_COOL:
          if (((message[4] >> 4) == RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY) && (message[2] == RAC_PT1411HWRU_FAN_OFF)) {
            this->mode = climate::CLIMATE_MODE_OFF;
          } else {
            this->mode = climate::CLIMATE_MODE_COOL;
          }
          break;

        // case RAC_PT1411HWRU_MODE_DRY:
        case RAC_PT1411HWRU_MODE_FAN:
          if ((message[4] >> 4) == RAC_PT1411HWRU_TEMPERATURE_FAN_ONLY) {
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          } else {
            this->mode = climate::CLIMATE_MODE_DRY;
          }
          break;

        case RAC_PT1411HWRU_MODE_HEAT:
          this->mode = climate::CLIMATE_MODE_HEAT;
          break;

        default:
          this->mode = climate::CLIMATE_MODE_OFF;
          break;
      }
      // Get the fan speed/mode
      switch (message[2]) {
        case RAC_PT1411HWRU_FAN_LOW.code1:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;

        case RAC_PT1411HWRU_FAN_MED.code1:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;

        case RAC_PT1411HWRU_FAN_HIGH.code1:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          break;

        case RAC_PT1411HWRU_FAN_AUTO.code1:
        default:
          this->fan_mode = climate::CLIMATE_FAN_AUTO;
          break;
      }
      // Get the target temperature
      if (this->is_valid_rac_pt1411hwru_message_(&message[12])) {
        temperature_code =
            (message[4] >> 4) | (message[14] & RAC_PT1411HWRU_FLAG_FRAC) | (message[15] & RAC_PT1411HWRU_FLAG_NEG);
        if (message[15] & RAC_PT1411HWRU_FLAG_FAH) {
          for (size_t i = 0; i < RAC_PT1411HWRU_TEMPERATURE_F.size(); i++) {
            if (RAC_PT1411HWRU_TEMPERATURE_F[i] == temperature_code) {
              this->target_temperature = static_cast<float>((i + TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN - 32) * 5) / 9;
            }
          }
        } else {
          for (size_t i = 0; i < RAC_PT1411HWRU_TEMPERATURE_C.size(); i++) {
            if (RAC_PT1411HWRU_TEMPERATURE_C[i] == temperature_code) {
              this->target_temperature = i + TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
            }
          }
        }
      }
      break;
    // "Comfort Sense" temperature packet
    case RAC_PT1411HWRU_CS_HEADER:
      // "Comfort Sense" feature notes
      // IR Code: 0xBA45 xxXX yyYY
      // xx: Temperature in °C
      //     Bit 6: feature state (on/off)
      //     Bit 7: message contains temperature data for feature (bit 6 must also be set)
      // XX: Bitwise complement of xx
      // yy: Mode: Auto: 7A
      //           Cool: 72
      //           Heat: 7E
      // YY: Bitwise complement of yy
      if ((message[2] & RAC_PT1411HWRU_CS_ENABLED) && (message[2] & RAC_PT1411HWRU_CS_DATA)) {
        // Setting current_temperature this way allows the unit's remote to provide the temperature to HA
        this->current_temperature = message[2] & ~(RAC_PT1411HWRU_CS_ENABLED | RAC_PT1411HWRU_CS_DATA);
      }
      break;
    // Swing mode
    case RAC_PT1411HWRU_SWING_HEADER:
      if (message[4] == RAC_PT1411HWRU_SWING_VERTICAL[4]) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }
      break;
    // Generic (old) Toshiba packet
    default:
      uint8_t checksum = 0;
      // Add back the length of the header (we pruned it above)
      message_length += TOSHIBA_HEADER_LENGTH;
      // Validate the second checksum before trusting any more of the message
      for (uint8_t i = TOSHIBA_HEADER_LENGTH; i < message_length - 1; i++) {
        checksum ^= message[i];
      }
      // Did our computed checksum and the provided checksum match?
      if (checksum != message[message_length - 1]) {
        return false;
      }
      // Check if this is a short swing/fix message
      if (message[4] & TOSHIBA_COMMAND_MOTION) {
        // Not supported yet
        return false;
      }

      // Get the mode
      switch (message[6] & 0x0F) {
        case TOSHIBA_MODE_OFF:
          this->mode = climate::CLIMATE_MODE_OFF;
          break;

        case TOSHIBA_MODE_COOL:
          this->mode = climate::CLIMATE_MODE_COOL;
          break;

        case TOSHIBA_MODE_DRY:
          this->mode = climate::CLIMATE_MODE_DRY;
          break;

        case TOSHIBA_MODE_FAN_ONLY:
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          break;

        case TOSHIBA_MODE_HEAT:
          this->mode = climate::CLIMATE_MODE_HEAT;
          break;

        case TOSHIBA_MODE_AUTO:
        default:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      }

      // Get the fan mode
      switch (message[6] & 0xF0) {
        case TOSHIBA_FAN_SPEED_QUIET:
          this->fan_mode = climate::CLIMATE_FAN_QUIET;
          break;

        case TOSHIBA_FAN_SPEED_1:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;

        case TOSHIBA_FAN_SPEED_3:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;

        case TOSHIBA_FAN_SPEED_5:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          break;

        case TOSHIBA_FAN_SPEED_AUTO:
        default:
          this->fan_mode = climate::CLIMATE_FAN_AUTO;
          break;
      }

      // Get the target temperature
      this->target_temperature = (message[5] >> 4) + TOSHIBA_GENERIC_TEMP_C_MIN;
  }

  this->publish_state();
  return true;
}

void ToshibaClimate::encode_(remote_base::RemoteTransmitData *data, const uint8_t *message, const uint8_t nbytes,
                             const uint8_t repeat) {
  data->set_carrier_frequency(TOSHIBA_CARRIER_FREQUENCY);

  for (uint8_t copy = 0; copy <= repeat; copy++) {
    data->item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE);

    for (uint8_t byte = 0; byte < nbytes; byte++) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        data->mark(TOSHIBA_BIT_MARK);
        if (message[byte] & (1 << (7 - bit))) {
          data->space(TOSHIBA_ONE_SPACE);
        } else {
          data->space(TOSHIBA_ZERO_SPACE);
        }
      }
    }
    data->item(TOSHIBA_BIT_MARK, TOSHIBA_GAP_SPACE);
  }
}

bool ToshibaClimate::decode_(remote_base::RemoteReceiveData *data, uint8_t *message, const uint8_t nbytes) {
  for (uint8_t byte = 0; byte < nbytes; byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data->expect_item(TOSHIBA_BIT_MARK, TOSHIBA_ONE_SPACE)) {
        message[byte] |= 1 << (7 - bit);
      } else if (data->expect_item(TOSHIBA_BIT_MARK, TOSHIBA_ZERO_SPACE)) {
        message[byte] &= static_cast<uint8_t>(~(1 << (7 - bit)));
      } else {
        return false;
      }
    }
  }
  return true;
}

}  // namespace toshiba
}  // namespace esphome
