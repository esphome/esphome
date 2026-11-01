#include "airton_serial_climate.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::airton_serial {

ESPHOME_LOG_TAG(TAG, "airton_serial.climate");

static constexpr uint8_t SYNC_BYTE = 0x7A;
static constexpr uint8_t LENGTH_POS = 4;
static constexpr uint8_t OPCODE_POS = 7;
static constexpr uint8_t MIN_FRAME_LEN = OPCODE_POS + 1 + 2;  // Up to the opcode, then the CRC
// A partial frame left this long without a new byte is not a frame start.
static constexpr uint32_t FRAME_GAP_MS = 50;

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

// State byte: bit 7 super, bits 4-6 fan speed, bit 3 power, bits 0-2 mode.
static constexpr uint8_t POWER_BIT = 0x08;
static constexpr uint8_t MODE_MASK = 0x07;
static constexpr uint8_t FAN_AUTO = 0x0;
static constexpr uint8_t FAN_SUPER = 0x8;  // Bit 7 of the state byte, the top bit of the fan nibble
static constexpr uint8_t FAN_STRONG = FAN_SUPER | 0x5;
// Setpoint byte: bit 6 mute, bit 5 Fahrenheit display, bits 0-4 setpoint above TEMP_MIN.
static constexpr uint8_t FLAG_QUIET = 0x40;
static constexpr uint8_t FLAG_FAHRENHEIT = 0x20;
static constexpr uint8_t SETPOINT_MASK = 0x1F;
// Swing byte: bit 0 vertical, bit 4 horizontal.
static constexpr uint8_t SWING_VERTICAL = 0x01;
static constexpr uint8_t SWING_HORIZONTAL = 0x10;
static constexpr uint8_t SWING_MASK = SWING_VERTICAL | SWING_HORIZONTAL;

struct CodeMap {
  uint8_t value;  // ESPHome enum value
  uint8_t code;   // Protocol nibble
};

// Mode bits; off is the power bit, which keeps the mode bits.
static constexpr CodeMap MODE_CODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT_COOL, 0x0}, {climate::CLIMATE_MODE_COOL, 0x1}, {climate::CLIMATE_MODE_DRY, 0x2},
    {climate::CLIMATE_MODE_FAN_ONLY, 0x3},  {climate::CLIMATE_MODE_HEAT, 0x4},
};

static constexpr CodeMap SWING_CODES[] PROGMEM = {
    {climate::CLIMATE_SWING_OFF, 0x00},
    {climate::CLIMATE_SWING_VERTICAL, SWING_VERTICAL},
    {climate::CLIMATE_SWING_HORIZONTAL, SWING_HORIZONTAL},
    {climate::CLIMATE_SWING_BOTH, SWING_MASK},
};

static constexpr CodeMap FAN_CODES[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, 0x1},
    {climate::CLIMATE_FAN_MEDIUM, 0x3},
    {climate::CLIMATE_FAN_HIGH, 0x5},
};

// Speeds with no standard ESPHome fan mode, and their codes at the same index.
static constexpr const char *const CUSTOM_FAN_MODES[] = {"medlow", "medhigh", "strong"};
static constexpr uint8_t CUSTOM_FAN_CODES[] PROGMEM = {0x2, 0x4, FAN_STRONG};

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
  const uint32_t now = App.get_loop_component_start_time();
  bool received = false;
  this->read_available([this, &received](uint8_t byte) {
    received = true;
    this->handle_byte_(byte);
  });
  if (received) {
    this->last_rx_ms_ = now;
  } else if (this->rx_pos_ > 0 && now - this->last_rx_ms_ >= FRAME_GAP_MS) {
    // A corrupt length can hold back complete frames behind it; look for them in what is buffered.
    while (this->rx_pos_ > 0)
      this->drop_first_byte_();
  }
}

void AirtonSerialClimate::handle_byte_(uint8_t byte) {
  // Bytes are checked as they arrive, so rx_pos_ stays below the longest frame length.
  this->rx_buffer_[this->rx_pos_++] = byte;
  this->scan_buffer_();
}

void AirtonSerialClimate::drop_first_byte_() {
  this->rx_pos_--;
  std::memmove(this->rx_buffer_.data(), this->rx_buffer_.data() + 1, this->rx_pos_);
  this->scan_buffer_();
}

void AirtonSerialClimate::scan_buffer_() {
  const uint8_t *buf = this->rx_buffer_.data();
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
    std::memmove(this->rx_buffer_.data(), buf + drop, this->rx_pos_);
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
  // Kept so that quiet, which mutes whatever speed is set, and speeds with no fan mode are sent back unchanged.
  this->fan_raw_ = mode_fan >> 4;
  this->fahrenheit_ = (setpoint_raw & FLAG_FAHRENHEIT) != 0;
  // Super is reported with the speed it runs at; it is the "strong" custom mode.
  const uint8_t fan = (this->fan_raw_ & FAN_SUPER) != 0 ? FAN_STRONG : this->fan_raw_;
  const uint8_t mode = mode_fan & (POWER_BIT | MODE_MASK);
  const uint8_t setpoint = (setpoint_raw & SETPOINT_MASK) + TEMP_MIN;

  this->current_temperature = static_cast<float>(data[10]) + static_cast<float>(data[11]) / 10.0f;
  this->target_temperature = static_cast<float>(setpoint);

  uint8_t value;
  this->mode_bits_ = mode & MODE_MASK;
  if ((mode & POWER_BIT) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if (value_of(MODE_CODES, this->mode_bits_, value)) {
    this->mode = static_cast<climate::ClimateMode>(value);
  }

  if ((setpoint_raw & FLAG_QUIET) != 0) {
    this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
  } else if (value_of(FAN_CODES, fan, value)) {
    this->set_fan_mode_(static_cast<climate::ClimateFanMode>(value));
  } else {
    bool found = false;
    for (size_t i = 0; i < std::size(CUSTOM_FAN_MODES) && !found; i++) {
      found = progmem_read_byte(&CUSTOM_FAN_CODES[i]) == fan;
      if (found)
        this->set_custom_fan_mode_(CUSTOM_FAN_MODES[i]);
    }
    if (!found) {
      // Speed 6 has no fan mode; with none set, commands send fan_raw_ back.
      this->fan_mode.reset();
      this->clear_custom_fan_mode_();
    }
  }

  this->swing_ = data[15];
  if (value_of(SWING_CODES, this->swing_ & SWING_MASK, value))
    this->swing_mode = static_cast<climate::ClimateSwingMode>(value);
  this->flags_ = data[16];
  this->state_received_ = true;
  ESP_LOGD(TAG, "A3: mode=0x%02X fan=0x%02X setpoint=%u ambient=%.1f swing=%u flags=0x%02X", mode, fan, setpoint,
           this->current_temperature, data[15], data[16]);
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
  PUBLISH_STATE(this->display_switch_, (this->flags_ & FLAG_DISPLAY) != 0);
  PUBLISH_STATE(this->ionizer_switch_, (this->flags_ & FLAG_IONIZER) != 0);
  PUBLISH_STATE(this->sleep_switch_, (this->flags_ & FLAG_SLEEP) != 0);
  PUBLISH_STATE(this->eco_switch_, (this->flags_ & FLAG_ECO) != 0);
  PUBLISH_STATE(this->health_switch_, (this->flags_ & FLAG_HEALTH) != 0);
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
  if (call.get_target_temperature().has_value()) {
    // The unit takes whole degrees.
    this->target_temperature = static_cast<float>(
        std::clamp<int32_t>(static_cast<int32_t>(std::lround(*call.get_target_temperature())), TEMP_MIN, TEMP_MAX));
  }
  if (call.has_custom_fan_mode()) {
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  } else if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
  }
  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();
    uint8_t code = 0;
    code_of(SWING_CODES, this->swing_mode, code);
    // Other bits of the swing byte are kept as the unit reported them.
    this->swing_ = static_cast<uint8_t>((this->swing_ & ~SWING_MASK) | code);
  }

  this->send_command_();
  this->publish_state();
}

void AirtonSerialClimate::send_command_() {
  // Off clears the power bit and leaves the last mode bits, as the unit reports them.
  uint8_t mode_nibble = this->mode_bits_;
  if (code_of(MODE_CODES, this->mode, this->mode_bits_))
    mode_nibble = this->mode_bits_ | POWER_BIT;

  uint8_t fan_nibble = this->fan_raw_;
  bool quiet = false;
  if (this->has_custom_fan_mode()) {
    const StringRef custom = this->get_custom_fan_mode();
    for (size_t i = 0; i < std::size(CUSTOM_FAN_MODES); i++) {
      if (custom == CUSTOM_FAN_MODES[i]) {
        fan_nibble = progmem_read_byte(&CUSTOM_FAN_CODES[i]);
        break;
      }
    }
    // Super runs on top of a speed; keep the one the unit reported instead of forcing speed 5
    if (fan_nibble == FAN_STRONG && (this->fan_raw_ & FAN_SUPER) != 0)
      fan_nibble = this->fan_raw_;
  } else if (this->fan_mode.has_value()) {
    quiet = *this->fan_mode == climate::CLIMATE_FAN_QUIET;
    if (!quiet)
      code_of(FAN_CODES, *this->fan_mode, fan_nibble);
  }
  this->fan_raw_ = fan_nibble;

  const int32_t rounded =
      std::clamp<int32_t>(static_cast<int32_t>(std::lround(this->target_temperature)), TEMP_MIN, TEMP_MAX);
  uint8_t setpoint = static_cast<uint8_t>(rounded - TEMP_MIN);
  if (quiet)
    setpoint |= FLAG_QUIET;
  if (this->fahrenheit_)
    setpoint |= FLAG_FAHRENHEIT;

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
  frame[14] = this->swing_;
  frame[15] = this->flags_;
  this->write_frame_(frame, sizeof(frame));
  ESP_LOGD(TAG, "A1: state=0x%02X setpoint=0x%02X swing=0x%02X flags=0x%02X", state, setpoint, this->swing_,
           this->flags_);
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
  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                    climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
  traits.set_visual_min_temperature(TEMP_MIN);
  traits.set_visual_max_temperature(TEMP_MAX);
  traits.set_visual_target_temperature_step(1);
  traits.set_visual_current_temperature_step(0.1f);
  return traits;
}

void AirtonSerialClimate::dump_config() {
  LOG_CLIMATE("", "Airton Serial Climate", this);
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Display", this->display_switch_);
  LOG_SWITCH("  ", "Ionizer", this->ionizer_switch_);
  LOG_SWITCH("  ", "Sleep", this->sleep_switch_);
  LOG_SWITCH("  ", "Eco", this->eco_switch_);
  LOG_SWITCH("  ", "Health", this->health_switch_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP("  ", "Total Usage Time", this->total_usage_time_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "Revision", this->revision_sensor_);
#endif
}

}  // namespace esphome::airton_serial
