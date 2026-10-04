#include "max31888.h"
#include "esphome/core/log.h"

namespace esphome::max31888 {

static const char *const TAG = "max31888.sensor";

static const uint8_t MAX31888_MODEL = 0x54;
static const uint8_t MAX31888_COMMAND_START_CONVERSION = 0x44;
static const uint8_t MAX31888_COMMAND_READ = 0x33;
static const uint8_t MAX31888_COMMAND_SOFT_RESET = 0x82;
static const uint16_t MAX31888_MILIS_TO_WAIT = 20;
static const uint8_t MAX31888_COMMAND_WRITE_SCRATCH_PAD = 0x4E;
static const uint8_t MAX31888_COMMAND_COPY_SCRATCH_PAD = 0x48;

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

  this->status_clear_warning();

  this->send_command_(MAX31888_COMMAND_START_CONVERSION);
  uint16_t crc = this->bus_->read8() | (this->bus_->read8() << 8);  // this must be read to start conversion
  ESP_LOGV(TAG, "CRC: %04X", crc);

  this->set_timeout(this->get_address_name().c_str(), MAX31888_MILIS_TO_WAIT, [this] {
    if (!this->read_fifo_()) {
      this->publish_state(NAN);
      return;
    }

    float tempc = (int16_t) (this->fifo_[0] << 8 | this->fifo_[1]) * 0.005;
    ESP_LOGD(TAG, "'%s': Got Temperature=%.3f°C", this->get_name().c_str(), tempc);
    this->publish_state(tempc);
  });
}

bool MAX31888Sensor::read_fifo_() {
  uint8_t data[7] = {MAX31888_COMMAND_READ, 0x08, 0x01, 0xff, 0xff, 0, 0};
  {
    InterruptLock lock;
    if (this->send_command_(data[0])) {
      this->bus_->write8(data[1]);  // Starting Adddress -> FIFO Data Register
      this->bus_->write8(data[2]);  // Length (Bytes -1) -> 2 Bytes

      for (uint32_t i = 3; i < sizeof(data); i++)
        data[i] = this->bus_->read8();
    }
  }

  // ESP_LOGI(TAG, "FIFO: %02X.%02X.%02X.%02X", this->fifo_[0], this->fifo_[1], this->crc_[0], this->crc_[1]);
  if (crc16(data, sizeof(data) - 2, 0, 0xa001, false, true) == (data[5] | (data[6] << 8))) {
    this->fifo_[0] = data[3];
    this->fifo_[1] = data[4];
    return true;
  } else {
    ESP_LOGW(TAG, "'%s' - CRC failed: %02x.%02x.%02x.%02x", this->get_name().c_str(), data[3], data[4], data[5],
             data[6]);
    // this->status_set_warning ("bus reset failed");
    return false;
  }
}

void MAX31888Sensor::setup() {
  ESP_LOGCONFIG(TAG, "setting up MAX31888 temperature sensor...");
  if (!this->check_address_or_index_())
    return;

  {
    InterruptLock lock;
    if (this->send_command_(MAX31888_COMMAND_SOFT_RESET)) {
      uint16_t crc = this->bus_->read8() | (this->bus_->read8() << 8);  // this must be read to perform reset
      ESP_LOGV(TAG, "CRC: %04X", crc);
    }
  }
}

}  // namespace esphome::max31888
