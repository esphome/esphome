#include "airton_serial_climate.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::airton_serial {

ESPHOME_LOG_TAG(TAG, "airton_serial.climate");

static constexpr uint8_t SYNC_BYTE = 0x7A;
static constexpr uint8_t LENGTH_POS = 4;
static constexpr uint8_t OPCODE_POS = 7;
static constexpr uint8_t MIN_FRAME_LEN = 6;

static constexpr uint8_t ADDR_MODULE = 0x21;
static constexpr uint8_t ADDR_DISPLAY = 0xD1;
static constexpr uint8_t ADDR_BOARD = 0xD5;

static constexpr uint8_t OP_A3_STATE = 0xA3;
static constexpr uint8_t OP_A4_ACK = 0xA4;
static constexpr uint8_t OP_AC_POLL = 0xAC;

// Announcement sent to the display every 10 s, followed by the ESP's MAC address and the CRC.
static constexpr uint8_t A5_HEADER[] PROGMEM = {
    0x7A, 0x7A, 0x21, 0xD1, 0x1B, 0x00, 0x00, 0xA5, 0x0A, 0x0A, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
};

// Poll sent to the climate board every 60 s.
static constexpr uint8_t AB_FRAME[] PROGMEM = {
    0x7A, 0x7A, 0x21, 0xD5, 0x0C, 0x00, 0x00, 0xAB, 0x0A, 0x0A, 0xFC, 0xF9,
};

static constexpr uint8_t TEMP_MIN = 16;
static constexpr uint8_t TEMP_MAX = 31;

static constexpr uint8_t MODE_OFF = 0x4;
static constexpr uint8_t FAN_AUTO = 0x0;
static constexpr uint8_t SWING_OFF = 0x0;
static constexpr uint8_t SWING_ON = 0x1;
static constexpr uint8_t FLAG_QUIET = 0x40;  // Setpoint byte, not the flags byte.

struct CodeMap {
  uint8_t value;  // ESPHome enum value
  uint8_t code;   // Protocol nibble
};

static constexpr CodeMap MODE_CODES[] PROGMEM = {
    {climate::CLIMATE_MODE_OFF, MODE_OFF}, {climate::CLIMATE_MODE_HEAT_COOL, 0x8}, {climate::CLIMATE_MODE_COOL, 0x9},
    {climate::CLIMATE_MODE_DRY, 0xA},      {climate::CLIMATE_MODE_FAN_ONLY, 0xB},  {climate::CLIMATE_MODE_HEAT, 0xC},
};

static constexpr CodeMap FAN_CODES[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, 0x1},
    {climate::CLIMATE_FAN_MEDIUM, 0x3},
    {climate::CLIMATE_FAN_HIGH, 0x5},
};

// Speeds with no standard ESPHome fan mode, and their codes at the same index.
static constexpr const char *const CUSTOM_FAN_MODES[] = {"medlow", "medhigh", "strong"};
static constexpr uint8_t CUSTOM_FAN_CODES[] PROGMEM = {0x2, 0x4, 0xD};

template<size_t N> static bool code_of(const CodeMap (&table)[N], uint8_t value, uint8_t &code) {
  for (const CodeMap &entry : table) {
    if (progmem_read_byte(&entry.value) == value) {
      code = progmem_read_byte(&entry.code);
      return true;
    }
  }
  return false;
}

template<size_t N> static bool value_of(const CodeMap (&table)[N], uint8_t code, uint8_t &value) {
  for (const CodeMap &entry : table) {
    if (progmem_read_byte(&entry.code) == code) {
      value = progmem_read_byte(&entry.value);
      return true;
    }
  }
  return false;
}

AirtonSerialClimate::AirtonSerialClimate() {
  this->set_supported_custom_fan_modes_static(CUSTOM_FAN_MODES, std::size(CUSTOM_FAN_MODES));
  this->target_temperature = 21.0f;
}

void AirtonSerialClimate::setup() {
  // Announce and poll immediately, then keep the module's cadence.
  this->send_a5_();
  this->send_ab_();
  this->set_interval(10000, [this]() { this->send_a5_(); });
  this->set_interval(60000, [this]() { this->send_ab_(); });
}

void AirtonSerialClimate::loop() {
  this->read_available([this](uint8_t byte) { this->handle_byte_(byte); });
}

void AirtonSerialClimate::handle_byte_(uint8_t byte) {
  // Bytes are checked as they arrive, so rx_pos_ stays below the longest frame length.
  uint8_t *buf = this->rx_buffer_.data();
  buf[this->rx_pos_++] = byte;
  while (this->rx_pos_ > 0) {
    uint8_t drop = 1;
    if (buf[0] == SYNC_BYTE && (this->rx_pos_ < 2 || buf[1] == SYNC_BYTE)) {
      if (this->rx_pos_ <= LENGTH_POS)
        return;
      const uint8_t len = buf[LENGTH_POS];
      if (len >= MIN_FRAME_LEN && len <= MAX_FRAME_LEN) {
        if (this->rx_pos_ < len)
          return;
        if (crc16(buf, len - 2) == encode_uint16(buf[len - 2], buf[len - 1])) {
          this->handle_frame_(buf, len);
          drop = len;
        } else {
          ESP_LOGW(TAG, "Bad CRC on %u-byte frame", len);
        }
      }
    }
    // Not a frame start, or a bad frame: look for the next start after its first byte.
    this->rx_pos_ -= drop;
    std::memmove(buf, buf + drop, this->rx_pos_);
  }
}

void AirtonSerialClimate::handle_frame_(const uint8_t *data, size_t len) {
  const uint8_t opcode = data[OPCODE_POS];
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  if (opcode == OP_A3_STATE || opcode == OP_AC_POLL) {
    char hex[format_hex_pretty_size(MAX_FRAME_LEN)];
    ESP_LOGV(TAG, "%02X raw: %s", opcode, format_hex_pretty_to(hex, data, len));
  }
#endif
  if (data[3] != ADDR_MODULE)
    return;
  if (data[2] == ADDR_BOARD) {
    if (len == 34 && opcode == OP_A3_STATE) {
      this->handle_a3_(data);
    } else if (len == 18 && opcode == OP_AC_POLL) {
      this->handle_ac_(data);
    }
  } else if (data[2] == ADDR_DISPLAY && len == 13 && opcode == OP_A4_ACK) {
    ESP_LOGD(TAG, "A4 acknowledgement from display");
  }
}

void AirtonSerialClimate::handle_a3_(const uint8_t *data) {
  const uint8_t mode_fan = data[13];
  const uint8_t setpoint_raw = data[14];
  const uint8_t fan = (mode_fan >> 4) & 0x0F;
  const uint8_t mode = mode_fan & 0x0F;
  const uint8_t setpoint = (setpoint_raw & 0x0F) + TEMP_MIN;

  this->current_temperature = static_cast<float>(data[10]);
  this->target_temperature = static_cast<float>(setpoint);

  uint8_t value;
  if (value_of(MODE_CODES, mode, value))
    this->mode = static_cast<climate::ClimateMode>(value);

  if ((setpoint_raw & FLAG_QUIET) != 0) {
    this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
  } else if (value_of(FAN_CODES, fan, value)) {
    this->set_fan_mode_(static_cast<climate::ClimateFanMode>(value));
  } else {
    for (size_t i = 0; i < std::size(CUSTOM_FAN_MODES); i++) {
      if (progmem_read_byte(&CUSTOM_FAN_CODES[i]) == fan) {
        this->set_custom_fan_mode_(CUSTOM_FAN_MODES[i]);
        break;
      }
    }
  }

  this->swing_mode = (data[15] == 0) ? climate::CLIMATE_SWING_OFF : climate::CLIMATE_SWING_VERTICAL;
  this->flags_ = data[16];
  this->state_received_ = true;
#ifdef USE_SENSOR
  this->revision_sensor_.publish_state_if_not_dup(encode_uint16(data[23], data[24]));
#endif
  ESP_LOGD(TAG, "A3: mode=0x%02X fan=0x%02X setpoint=%u ambient=%u swing=%u flags=0x%02X", mode, fan, setpoint,
           data[10], data[15], data[16]);
  this->publish_settings_();
  this->publish_state();
}

void AirtonSerialClimate::handle_ac_(const uint8_t *data) {
  const uint16_t total_usage_time = encode_uint16(data[10], data[11]);
  const uint16_t revision = encode_uint16(data[13], data[14]);
#ifdef USE_SENSOR
  this->total_usage_time_sensor_.publish_state_if_not_dup(total_usage_time);
  this->revision_sensor_.publish_state_if_not_dup(revision);
#endif
  ESP_LOGD(TAG, "AC: total_usage_time=%u h revision=%u", total_usage_time, revision);
}

void AirtonSerialClimate::publish_settings_() {
#ifdef USE_SWITCH
  if (this->display_switch_ != nullptr)
    this->display_switch_->publish_state((this->flags_ & FLAG_DISPLAY) != 0);
  if (this->ionizer_switch_ != nullptr)
    this->ionizer_switch_->publish_state((this->flags_ & FLAG_IONIZER) != 0);
  if (this->sleep_switch_ != nullptr)
    this->sleep_switch_->publish_state((this->flags_ & FLAG_SLEEP) != 0);
  if (this->eco_switch_ != nullptr)
    this->eco_switch_->publish_state((this->flags_ & FLAG_ECO) != 0);
  if (this->health_switch_ != nullptr)
    this->health_switch_->publish_state((this->flags_ & FLAG_HEALTH) != 0);
#endif
}

bool AirtonSerialClimate::can_send_command_() {
  if (!this->state_received_) {
    ESP_LOGW(TAG, "No state from the unit yet, change ignored");
  }
  return this->state_received_;
}

bool AirtonSerialClimate::set_flag(uint8_t flag, bool state) {
  if (!this->can_send_command_())
    return false;
  assign_bits(this->flags_, flag, state);
  this->send_command_();
  return true;
}

void AirtonSerialClimate::control(const climate::ClimateCall &call) {
  // A1 carries the whole state, so sending it before the unit reported its own would apply the defaults.
  if (!this->can_send_command_())
    return;
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.has_custom_fan_mode()) {
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  } else if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
  }
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  this->send_command_();
  this->publish_state();
}

void AirtonSerialClimate::send_command_() {
  uint8_t mode_nibble = MODE_OFF;
  code_of(MODE_CODES, this->mode, mode_nibble);

  uint8_t fan_nibble = FAN_AUTO;
  bool quiet = false;
  if (this->has_custom_fan_mode()) {
    const StringRef custom = this->get_custom_fan_mode();
    for (size_t i = 0; i < std::size(CUSTOM_FAN_MODES); i++) {
      if (custom == CUSTOM_FAN_MODES[i]) {
        fan_nibble = progmem_read_byte(&CUSTOM_FAN_CODES[i]);
        break;
      }
    }
  } else if (this->fan_mode.has_value()) {
    quiet = *this->fan_mode == climate::CLIMATE_FAN_QUIET;
    code_of(FAN_CODES, *this->fan_mode, fan_nibble);
  }

  const int32_t rounded =
      std::clamp<int32_t>(static_cast<int32_t>(std::lround(this->target_temperature)), TEMP_MIN, TEMP_MAX);
  uint8_t setpoint = static_cast<uint8_t>(rounded - TEMP_MIN);
  if (quiet)
    setpoint |= FLAG_QUIET;

  const uint8_t swing = (this->swing_mode == climate::CLIMATE_SWING_OFF) ? SWING_OFF : SWING_ON;
  const uint8_t state = static_cast<uint8_t>((fan_nibble << 4) | mode_nibble);

  uint8_t frame[24]{};
  frame[0] = SYNC_BYTE;
  frame[1] = SYNC_BYTE;
  frame[2] = ADDR_MODULE;
  frame[3] = ADDR_BOARD;
  frame[LENGTH_POS] = sizeof(frame);
  frame[OPCODE_POS] = 0xA1;
  frame[12] = state;
  frame[13] = setpoint;
  frame[14] = swing;
  frame[15] = this->flags_;
  this->write_frame_(frame, sizeof(frame));
  ESP_LOGD(TAG, "A1: state=0x%02X setpoint=0x%02X swing=0x%02X flags=0x%02X", state, setpoint, swing, this->flags_);
}

void AirtonSerialClimate::send_a5_() {
  uint8_t frame[sizeof(A5_HEADER) + MAC_ADDRESS_SIZE + 2]{};  // MAC stays zero without networking
  progmem_memcpy(frame, A5_HEADER, sizeof(A5_HEADER));
  get_mac_address_raw(frame + sizeof(A5_HEADER));
  this->write_frame_(frame, sizeof(frame));
}

void AirtonSerialClimate::send_ab_() {
  uint8_t frame[sizeof(AB_FRAME)];
  progmem_memcpy(frame, AB_FRAME, sizeof(AB_FRAME));
  this->write_array(frame, sizeof(frame));
}

void AirtonSerialClimate::write_frame_(uint8_t *frame, size_t len) {
  const uint16_t crc = crc16(frame, len - 2);
  frame[len - 2] = static_cast<uint8_t>(crc >> 8);
  frame[len - 1] = static_cast<uint8_t>(crc);
  this->write_array(frame, len);
}

climate::ClimateTraits AirtonSerialClimate::traits() {
  climate::ClimateTraits traits;
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
                              climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                                  climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET});
  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  traits.set_visual_min_temperature(TEMP_MIN);
  traits.set_visual_max_temperature(TEMP_MAX);
  traits.set_visual_temperature_step(1);
  return traits;
}

void AirtonSerialClimate::dump_config() { LOG_CLIMATE("", "Airton Serial Climate", this); }

}  // namespace esphome::airton_serial
