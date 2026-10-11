#include "max31888.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::max31888 {

ESPHOME_LOG_TAG(TAG, "max31888");

static constexpr uint32_t CONVERSION_TIMEOUT_ID = 0;

static constexpr uint8_t MAX31888_COMMAND_START_CONVERSION = 0x44;
static constexpr uint8_t MAX31888_COMMAND_READ = 0x33;
static constexpr uint8_t MAX31888_COMMAND_SOFT_RESET = 0x82;
static constexpr uint8_t MAX31888_REGISTER_FIFO_DATA = 0x08;
static constexpr uint8_t MAX31888_FIFO_READ_LENGTH = 0x01;  // number of bytes to read, minus one
static constexpr uint16_t MAX31888_CONVERSION_TIME_MS = 20;
static constexpr float MAX31888_DEGREES_PER_LSB = 0.005f;

// Sends a command and checks the checksum the device appends to it (Maxim CRC-16 over the command byte,
// sent inverted and low byte first)
bool MAX31888Sensor::send_checked_command_(uint8_t command) {
  if (!this->send_command_(command)) {
    this->status_set_warning(LOG_STR("bus reset failed"));
    return false;
  }
  const uint8_t low = this->bus_->read8();
  const uint8_t high = this->bus_->read8();
  if (crc16(&command, 1, 0, 0xa001, false, true) != encode_uint16(high, low)) {
    this->status_set_warning(LOG_STR("command checksum invalid"));
    return false;
  }
  return true;
}

void MAX31888Sensor::setup() {
  if (!this->check_address_or_index_())
    return;
  if (!this->send_checked_command_(MAX31888_COMMAND_SOFT_RESET)) {
    this->mark_failed();
  }
}

void MAX31888Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX31888 Sensor:");
  if (this->address_ == 0) {
    ESP_LOGW(TAG, "  Unable to select an address");
    return;
  }
  LOG_ONE_WIRE_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

void MAX31888Sensor::update() {
  if (this->address_ == 0)
    return;

  if (!this->send_checked_command_(MAX31888_COMMAND_START_CONVERSION)) {
    return;
  }

  this->set_timeout(CONVERSION_TIMEOUT_ID, MAX31888_CONVERSION_TIME_MS, [this] {
    int16_t raw;
    if (!this->read_temperature_(raw)) {
      this->publish_state(NAN);
      return;
    }
    const float temperature = raw * MAX31888_DEGREES_PER_LSB;
    ESP_LOGD(TAG, "'%s': Got Temperature=%.3f°C", this->get_name().c_str(), temperature);
    this->status_clear_warning();
    this->publish_state(temperature);
  });
}

bool MAX31888Sensor::read_temperature_(int16_t &raw) {
  // The checksum covers the command and its two arguments as well as the data
  uint8_t frame[7] = {MAX31888_COMMAND_READ, MAX31888_REGISTER_FIFO_DATA, MAX31888_FIFO_READ_LENGTH};
  if (!this->send_command_(frame[0])) {
    this->status_set_warning(LOG_STR("bus reset failed"));
    return false;
  }
  this->bus_->write8(frame[1]);
  this->bus_->write8(frame[2]);
  for (size_t i = 3; i < sizeof(frame); i++) {
    frame[i] = this->bus_->read8();
  }
  if (crc16(frame, 5, 0, 0xa001, false, true) != encode_uint16(frame[6], frame[5])) {
    this->status_set_warning(LOG_STR("checksum invalid"));
    return false;
  }
  raw = static_cast<int16_t>(encode_uint16(frame[3], frame[4]));
  return true;
}

}  // namespace esphome::max31888
