#include "sun_gtil2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sun_gtil2 {

static const char *const TAG = "sun_gtil2";

static const double NTC_A = 0.0011591051055979914;
static const double NTC_B = 0.00022878183547845582;
static const double NTC_C = 1.0396291358342124e-07;
static const float PULLUP_RESISTANCE = 10000.0f;
static const uint16_t ADC_MAX = 1023;  // ADC of the inverter controller, not the ESP

struct SunGTIL2Message {
  uint16_t sync;
  uint8_t ac_waveform[277];
  uint8_t frequency;
  uint16_t ac_voltage;
  uint16_t ac_power;
  uint16_t dc_voltage;
  uint8_t state;
  uint8_t unknown1;
  uint8_t unknown2;
  uint8_t unknown3;
  uint8_t limiter_mode;
  uint8_t unknown4;
  uint16_t temperature;
  uint32_t limiter_power;
  uint16_t dc_power;
  char serial_number[10];
  uint8_t unknown5;
  uint8_t end[39];
} __attribute__((packed));

static const uint16_t MESSAGE_SIZE = sizeof(SunGTIL2Message);

static_assert(MESSAGE_SIZE == 350, "Expected the message size to be 350 bytes");

void SunGTIL2::setup() { this->rx_message_.reserve(MESSAGE_SIZE); }

void SunGTIL2::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
}

std::string SunGTIL2::state_to_string_(uint8_t state) {
  switch (state) {
    case 0x02:
      return "Starting voltage too low";
    case 0x07:
      return "Working";
    default:
      return str_sprintf("Unknown (0x%02x)", state);
  }
}

float SunGTIL2::calculate_temperature_(uint16_t adc_value) {
  if (adc_value >= ADC_MAX || adc_value == 0) {
    return NAN;
  }

  float ntc_resistance = PULLUP_RESISTANCE / ((static_cast<float>(ADC_MAX) / adc_value) - 1.0f);
  double lr = log(double(ntc_resistance));
  double v = NTC_A + NTC_B * lr + NTC_C * lr * lr * lr;
  return float(1.0 / v - 273.15);
}

void SunGTIL2::handle_char_(uint8_t c) {
  if (this->rx_message_.size() > 1 || c == 0x07) {
    this->rx_message_.push_back(c);
  } else if (!this->rx_message_.empty()) {
    this->rx_message_.clear();
  }
  if (this->rx_message_.size() < MESSAGE_SIZE) {
    return;
  }

  SunGTIL2Message msg;
  memcpy(&msg, this->rx_message_.data(), MESSAGE_SIZE);
  this->rx_message_.clear();

  if (!((msg.end[0] == 0) && (msg.end[38] == 0x08)))
    return;

  ESP_LOGVV(TAG, "Frequency raw value: %02x", msg.frequency);
  ESP_LOGVV(TAG, "Unknown values: %02x %02x %02x %02x %02x", msg.unknown1, msg.unknown2, msg.unknown3, msg.unknown4,
            msg.unknown5);

#ifdef USE_SENSOR
  if (this->ac_voltage_ != nullptr)
    this->ac_voltage_->publish_state(__builtin_bswap16(msg.ac_voltage) / 10.0f);
  if (this->dc_voltage_ != nullptr)
    this->dc_voltage_->publish_state(__builtin_bswap16(msg.dc_voltage) / 8.0f);
  if (this->ac_power_ != nullptr)
    this->ac_power_->publish_state(__builtin_bswap16(msg.ac_power) / 10.0f);
  if (this->dc_power_ != nullptr)
    this->dc_power_->publish_state(__builtin_bswap16(msg.dc_power) / 10.0f);
  if (this->limiter_power_ != nullptr)
    this->limiter_power_->publish_state(static_cast<int32_t>(__builtin_bswap32(msg.limiter_power)) / 10.0f);
  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(calculate_temperature_(__builtin_bswap16(msg.temperature)));
#endif
#ifdef USE_TEXT_SENSOR
  if (this->state_ != nullptr) {
    this->state_->publish_state(this->state_to_string_(msg.state));
  }
  if (this->serial_number_ != nullptr) {
    std::string serial_number;
    serial_number.assign(msg.serial_number, 10);
    this->serial_number_->publish_state(serial_number);
  }
#endif
}

void SunGTIL2::dump_config() {
#ifdef USE_SENSOR
  LOG_SENSOR("", "AC Voltage", this->ac_voltage_);
  LOG_SENSOR("", "DC Voltage", this->dc_voltage_);
  LOG_SENSOR("", "AC Power", this->ac_power_);
  LOG_SENSOR("", "DC Power", this->dc_power_);
  LOG_SENSOR("", "Limiter Power", this->limiter_power_);
  LOG_SENSOR("", "Temperature", this->temperature_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("", "State", this->state_);
  LOG_TEXT_SENSOR("", "Serial Number", this->serial_number_);
#endif
}

}  // namespace sun_gtil2
}  // namespace esphome
