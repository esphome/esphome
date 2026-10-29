#include "airton_serial_climate.h"

#include <cstring>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace airton_serial {

ESPHOME_LOG_TAG(TAG, "airton_serial.climate");

// Announcement sent to the display (D1) every 10 s. It carries the ESP's MAC address.
static constexpr uint8_t A5_HEADER[] = {
    0x7A, 0x7A, 0x21, 0xD1, 0x1B, 0x00, 0x00, 0xA5, 0x0A, 0x0A, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
};

// Poll sent to the climate board (D5) every 60 s.
static constexpr uint8_t AB_FRAME[] = {
    0x7A, 0x7A, 0x21, 0xD5, 0x0C, 0x00, 0x00, 0xAB, 0x0A, 0x0A, 0xFC, 0xF9,
};

// Protocol values (see the component documentation).
static constexpr uint8_t MODE_OFF = 0x4;
static constexpr uint8_t MODE_HEAT_COOL = 0x8;
static constexpr uint8_t MODE_COOL = 0x9;
static constexpr uint8_t MODE_DRY = 0xA;
static constexpr uint8_t MODE_FAN_ONLY = 0xB;
static constexpr uint8_t MODE_HEAT = 0xC;

static constexpr uint8_t FAN_AUTO = 0x0;
static constexpr uint8_t FAN_LOW = 0x1;
static constexpr uint8_t FAN_MEDLOW = 0x2;
static constexpr uint8_t FAN_MEDIUM = 0x3;
static constexpr uint8_t FAN_MEDHIGH = 0x4;
static constexpr uint8_t FAN_HIGH = 0x5;
static constexpr uint8_t FAN_STRONG = 0xD;

static constexpr uint8_t SWING_OFF = 0x0;
static constexpr uint8_t SWING_ON = 0x1;

static constexpr uint8_t FLAG_DISPLAY = 0x80;
static constexpr uint8_t FLAG_IONIZER = 0x40;
static constexpr uint8_t FLAG_HEALTH = 0x10;
static constexpr uint8_t FLAG_SLEEP = 0x02;
static constexpr uint8_t FLAG_ECO = 0x01;
static constexpr uint8_t FLAG_QUIET = 0x40;  // Setpoint byte, not the flags byte.

AirtonSerialClimate::AirtonSerialClimate() {
  // These three speeds have no standard ESPHome enum value.
  this->set_supported_custom_fan_modes({"medlow", "medhigh", "strong"});
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
  while (this->available() > 0) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    if (this->rx_len_ >= this->rx_buffer_.size())
      this->rx_len_ = 0;
    this->rx_buffer_[this->rx_len_++] = byte;
  }
  this->parse_buffer_();
}

void AirtonSerialClimate::parse_buffer_() {
  while (true) {
    size_t sync = 0;
    while (sync + 1 < this->rx_len_ &&
           !(this->rx_buffer_[sync] == 0x7A && this->rx_buffer_[sync + 1] == 0x7A)) {
      sync++;
    }
    if (sync + 1 >= this->rx_len_) {
      if (this->rx_len_ > 0 && this->rx_buffer_[this->rx_len_ - 1] == 0x7A) {
        this->rx_buffer_[0] = 0x7A;
        this->rx_len_ = 1;
      } else {
        this->rx_len_ = 0;
      }
      return;
    }
    if (sync > 0) {
      std::memmove(this->rx_buffer_.data(), this->rx_buffer_.data() + sync, this->rx_len_ - sync);
      this->rx_len_ -= sync;
    }
    if (this->rx_len_ < 5)
      return;

    const uint8_t frame_len = this->rx_buffer_[4];
    if (frame_len < 6 || frame_len > 64) {
      std::memmove(this->rx_buffer_.data(), this->rx_buffer_.data() + 1, this->rx_len_ - 1);
      this->rx_len_--;
      continue;
    }
    if (this->rx_len_ < frame_len)
      return;

    const uint16_t expected = crc16_(this->rx_buffer_.data(), frame_len - 2);
    const uint16_t received =
        (static_cast<uint16_t>(this->rx_buffer_[frame_len - 2]) << 8) | this->rx_buffer_[frame_len - 1];
    if (expected == received) {
      this->handle_frame_(this->rx_buffer_.data(), frame_len);
    } else {
      ESP_LOGW(TAG, "Bad CRC on %u-byte frame", frame_len);
    }
    std::memmove(this->rx_buffer_.data(), this->rx_buffer_.data() + frame_len, this->rx_len_ - frame_len);
    this->rx_len_ -= frame_len;
  }
}

void AirtonSerialClimate::handle_frame_(const uint8_t *data, size_t len) {
  if (data[7] == 0xA3 || data[7] == 0xA6 || data[7] == 0xAC) {
    char hex[3 * 128 + 1];
    ESP_LOGV(TAG, "%02X raw: %s", data[7], format_hex_pretty_to(hex, data, len));
  }
  if (len == 34 && data[2] == 0xD5 && data[3] == 0x21 && data[7] == 0xA3) {
    this->handle_a3_(data, len);
  } else if (len == 18 && data[2] == 0xD5 && data[3] == 0x21 && data[7] == 0xAC) {
    this->handle_ac_(data, len);
  } else if (len == 13 && data[2] == 0xD1 && data[3] == 0x21 && data[7] == 0xA4) {
    ESP_LOGD(TAG, "A4 acknowledgement from display");
  }
}

void AirtonSerialClimate::handle_a3_(const uint8_t *data, size_t len) {
  const uint8_t mode_fan = data[13];
  const uint8_t setpoint_raw = data[14];
  const uint8_t fan = (mode_fan >> 4) & 0x0F;
  const uint8_t mode = mode_fan & 0x0F;
  const bool quiet = (setpoint_raw & FLAG_QUIET) != 0;
  const uint8_t setpoint = (setpoint_raw & 0x0F) + 16;

  this->current_temperature = static_cast<float>(data[10]);
  this->target_temperature = static_cast<float>(setpoint);

  switch (mode) {
    case MODE_OFF:
      this->mode = climate::CLIMATE_MODE_OFF;
      break;
    case MODE_HEAT_COOL:
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      break;
    case MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case MODE_FAN_ONLY:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;
    default:
      break;
  }

  if (quiet) {
    this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
  } else {
    switch (fan) {
      case FAN_AUTO:
        this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
        break;
      case FAN_LOW:
        this->set_fan_mode_(climate::CLIMATE_FAN_LOW);
        break;
      case FAN_MEDLOW:
        this->set_custom_fan_mode_("medlow");
        break;
      case FAN_MEDIUM:
        this->set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
        break;
      case FAN_MEDHIGH:
        this->set_custom_fan_mode_("medhigh");
        break;
      case FAN_HIGH:
        this->set_fan_mode_(climate::CLIMATE_FAN_HIGH);
        break;
      case FAN_STRONG:
        this->set_custom_fan_mode_("strong");
        break;
      default:
        break;
    }
  }

  this->swing_mode = (data[15] == 0) ? climate::CLIMATE_SWING_OFF : climate::CLIMATE_SWING_VERTICAL;
  this->flags_ = data[16];
  const uint16_t revision = (static_cast<uint16_t>(data[23]) << 8) | data[24];
#ifdef USE_SENSOR
  if (this->revision_sensor_ != nullptr)
    this->revision_sensor_->publish_state(revision);
#endif
  ESP_LOGD(TAG, "A3: mode=0x%02X fan=0x%02X setpoint=%u ambient=%u swing=%u flags=0x%02X", mode, fan, setpoint,
           data[10], data[15], data[16]);
  this->publish_settings_();
  this->publish_state();
}

void AirtonSerialClimate::handle_ac_(const uint8_t *data, size_t len) {
  const uint16_t total_usage_time = (static_cast<uint16_t>(data[10]) << 8) | data[11];
  const uint16_t revision = (static_cast<uint16_t>(data[13]) << 8) | data[14];
#ifdef USE_SENSOR
  if (this->total_usage_time_sensor_ != nullptr)
    this->total_usage_time_sensor_->publish_state(total_usage_time);
  if (this->revision_sensor_ != nullptr)
    this->revision_sensor_->publish_state(revision);
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

void AirtonSerialClimate::set_flag_(uint8_t flag, bool state) {
  if (state) {
    this->flags_ |= flag;
  } else {
    this->flags_ &= ~flag;
  }
  this->send_command_();
  this->publish_state();
  this->publish_settings_();
}

void AirtonSerialClimate::set_display_state(bool state) { this->set_flag_(FLAG_DISPLAY, state); }
void AirtonSerialClimate::set_ionizer_state(bool state) { this->set_flag_(FLAG_IONIZER, state); }
void AirtonSerialClimate::set_sleep_state(bool state) { this->set_flag_(FLAG_SLEEP, state); }
void AirtonSerialClimate::set_eco_state(bool state) { this->set_flag_(FLAG_ECO, state); }
void AirtonSerialClimate::set_health_state(bool state) { this->set_flag_(FLAG_HEALTH, state); }

void AirtonSerialClimate::control(const climate::ClimateCall &call) {
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
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      mode_nibble = MODE_HEAT_COOL;
      break;
    case climate::CLIMATE_MODE_COOL:
      mode_nibble = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_nibble = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_nibble = MODE_FAN_ONLY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      mode_nibble = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      mode_nibble = MODE_OFF;
      break;
  }

  uint8_t fan_nibble = FAN_AUTO;
  bool quiet = false;
  if (this->has_custom_fan_mode()) {
    const char *custom = this->get_custom_fan_mode().c_str();
    if (strcmp(custom, "medlow") == 0) {
      fan_nibble = FAN_MEDLOW;
    } else if (strcmp(custom, "medhigh") == 0) {
      fan_nibble = FAN_MEDHIGH;
    } else if (strcmp(custom, "strong") == 0) {
      fan_nibble = FAN_STRONG;
    }
  } else if (this->fan_mode.has_value()) {
    switch (*this->fan_mode) {
      case climate::CLIMATE_FAN_LOW:
        fan_nibble = FAN_LOW;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        fan_nibble = FAN_MEDIUM;
        break;
      case climate::CLIMATE_FAN_HIGH:
        fan_nibble = FAN_HIGH;
        break;
      case climate::CLIMATE_FAN_QUIET:
        fan_nibble = FAN_AUTO;
        quiet = true;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        fan_nibble = FAN_AUTO;
        break;
    }
  }

  float target = this->target_temperature;
  if (std::isnan(target))
    target = 21.0f;
  int rounded = static_cast<int>(std::lround(target));
  rounded = std::max(16, std::min(31, rounded));
  uint8_t setpoint = static_cast<uint8_t>(rounded - 16);
  if (quiet)
    setpoint |= FLAG_QUIET;

  const uint8_t swing = (this->swing_mode == climate::CLIMATE_SWING_OFF) ? SWING_OFF : SWING_ON;
  const uint8_t flags = this->flags_;
  const uint8_t state = static_cast<uint8_t>((fan_nibble << 4) | mode_nibble);

  uint8_t frame[24] = {
      0x7A,  0x7A,     0x21,  0xD5,  0x18, 0x00, 0x00, 0xA1,  // header
      0x00,  0x00,     0x00,  0x00,                           // reserved
      state, setpoint, swing, flags,                          // useful state
      0x00,  0x00,     0x00,  0x00,  0x00, 0x00,              // reserved
      0x00,  0x00                                             // CRC
  };
  const uint16_t crc = crc16_(frame, 22);
  frame[22] = static_cast<uint8_t>(crc >> 8);
  frame[23] = static_cast<uint8_t>(crc & 0xFF);
  this->write_array(frame, sizeof(frame));
  ESP_LOGD(TAG, "A1: state=0x%02X setpoint=0x%02X swing=0x%02X flags=0x%02X", state, setpoint, swing, flags);
}

void AirtonSerialClimate::send_a5_() {
  uint8_t frame[27];
  std::memcpy(frame, A5_HEADER, sizeof(A5_HEADER));
  get_mac_address_raw(frame + sizeof(A5_HEADER));
  const uint16_t crc = crc16_(frame, sizeof(frame) - 2);
  frame[25] = static_cast<uint8_t>(crc >> 8);
  frame[26] = static_cast<uint8_t>(crc & 0xFF);
  this->write_array(frame, sizeof(frame));
}

void AirtonSerialClimate::send_ab_() { this->write_array(AB_FRAME, sizeof(AB_FRAME)); }

climate::ClimateTraits AirtonSerialClimate::traits() {
  climate::ClimateTraits traits;
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
                              climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                                  climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET});
  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(31);
  traits.set_visual_temperature_step(1);
  return traits;
}

void AirtonSerialClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Airton Serial Climate:");
  ESP_LOGCONFIG(TAG, "  Current temperature: %.1f C", this->current_temperature);
  ESP_LOGCONFIG(TAG, "  Target temperature: %.1f C", this->target_temperature);
  ESP_LOGCONFIG(TAG, "  Flags: 0x%02X", this->flags_);
}

uint16_t AirtonSerialClimate::crc16_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

}  // namespace airton_serial
}  // namespace esphome
