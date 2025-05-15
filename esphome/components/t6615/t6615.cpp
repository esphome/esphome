#include "t6615.h"
#include "esphome/core/log.h"

namespace esphome {
namespace t6615 {

static const char *const TAG = "t6615";

static const uint32_t T6615_TIMEOUT = 1000;
static const uint8_t T6615_MAGIC = 0xFF;
static const uint8_t T6615_ADDR_HOST = 0xFA;
static const uint8_t T6615_ADDR_SENSOR = 0xFE;
static const uint8_t T6615_COMMAND_GET_PPM[] = {0x02, 0x03};
static const uint8_t T6615_COMMAND_GET_SERIAL[] = {0x02, 0x01};
static const uint8_t T6615_COMMAND_GET_VERSION[] = {0x02, 0x0D};
static const uint8_t T6615_COMMAND_GET_ELEVATION[] = {0x02, 0x0F};
static const uint8_t T6615_COMMAND_GET_ABC[] = {0xB7, 0x00};
static const uint8_t T6615_COMMAND_ENABLE_ABC[] = {0xB7, 0x01};
static const uint8_t T6615_COMMAND_DISABLE_ABC[] = {0xB7, 0x02};
static const uint8_t T6615_COMMAND_SET_ELEVATION[] = {0x03, 0x0F};

void T6615Component::send_ppm_command_() {
  this->command_time_ = millis();
  this->command_ = T6615Command::GET_PPM;
  this->write_byte(T6615_MAGIC);
  this->write_byte(T6615_ADDR_SENSOR);
  this->write_byte(sizeof(T6615_COMMAND_GET_PPM));
  this->write_array(T6615_COMMAND_GET_PPM, sizeof(T6615_COMMAND_GET_PPM));
}

void T6615Component::loop() {
  if (this->available() < 5) {
    if (this->command_ == T6615Command::GET_PPM && millis() - this->command_time_ > T6615_TIMEOUT) {
      /* command got eaten, clear the buffer and fire another */
      while (this->available())
        this->read();
      this->send_ppm_command_();
    }
    return;
  }

  uint8_t response_buffer[6];

  /* by the time we get here, we know we have at least five bytes in the buffer */
  this->read_array(response_buffer, 5);

  // Read header
  if (response_buffer[0] != T6615_MAGIC || response_buffer[1] != T6615_ADDR_HOST) {
    ESP_LOGW(TAG, "Got bad data from T6615! Magic was %02X and address was %02X", response_buffer[0],
             response_buffer[1]);
    /* make sure the buffer is empty */
    while (this->available())
      this->read();
    /* try again to read the sensor */
    this->send_ppm_command_();
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  switch (this->command_) {
    case T6615Command::GET_PPM: {
      const uint16_t ppm = encode_uint16(response_buffer[3], response_buffer[4]);
      ESP_LOGD(TAG, "T6615 Received CO₂=%uppm", ppm);
      if (this->co2_sensor_ != nullptr)
        this->co2_sensor_->publish_state(ppm);
      break;
    }
    default:
      break;
  }
  this->command_time_ = 0;
  this->command_ = T6615Command::NONE;
}

void T6615Component::update() { this->query_ppm_(); }

void T6615Component::query_ppm_() {
  if (this->co2_sensor_ == nullptr ||
      (this->command_ != T6615Command::NONE && millis() - this->command_time_ < T6615_TIMEOUT)) {
    return;
  }

  this->send_ppm_command_();
}

float T6615Component::get_setup_priority() const { return setup_priority::DATA; }
void T6615Component::dump_config() {
  ESP_LOGCONFIG(TAG, "T6615:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(19200);
}

}  // namespace t6615
}  // namespace esphome
