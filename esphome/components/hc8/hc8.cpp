#include "hc8.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <array>

namespace esphome::hc8 {

static const char *const TAG = "hc8";
static const std::array<uint8_t, 5> HC8_COMMAND_GET_PPM{0x64, 0x69, 0x03, 0x5E, 0x4E};
static const std::array<uint8_t, 3> HC8_COMMAND_CALIBRATE_PREAMBLE{0x11, 0x03, 0x03};

void HC8Component::setup() {
  // send an initial query to the device, this will
  // get it out of "active output mode", where it
  // generates data every second
  this->write_array(HC8_COMMAND_GET_PPM);
  this->flush();

  // ensure the buffer is empty
  while (this->available())
    this->read();
}

void HC8Component::update() {
  uint32_t now_ms = App.get_loop_component_start_time();
  uint32_t warmup_ms = this->warmup_seconds_ * 1000;
  if (now_ms < warmup_ms) {
    ESP_LOGW(TAG, "HC8 warming up, %" PRIu32 " s left", (warmup_ms - now_ms) / 1000);
    this->status_set_warning();
    return;
  }

  while (this->available())
    this->read();

  this->write_array(HC8_COMMAND_GET_PPM);
  this->flush();

  // the sensor is a bit slow in responding, so trying to
  // read immediately after sending a query will timeout
  this->set_timeout(50, [this]() {
    std::array<uint8_t, 14> response;
    if (!this->read_array(response.data(), response.size())) {
      ESP_LOGW(TAG, "Reading data from HC8 failed!");
      this->status_set_warning();
      return;
    }

    if (response[0] != 0x64 || response[1] != 0x69) {
      ESP_LOGW(TAG, "Invalid preamble from HC8!");
      this->status_set_warning();
      return;
    }

    if (crc16(response.data(), 12) != encode_uint16(response[13], response[12])) {
      ESP_LOGW(TAG, "HC8 Checksum mismatch");
      this->status_set_warning();
      return;
    }

    this->status_clear_warning();

    const uint16_t ppm = encode_uint16(response[5], response[4]);
    ESP_LOGD(TAG, "HC8 Received CO₂=%uppm", ppm);
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(ppm);
  });
}

void HC8Component::calibrate(uint16_t baseline) {
  ESP_LOGD(TAG, "HC8 Calibrating baseline to %uppm", baseline);

  std::array<uint8_t, 6> command{};
  std::copy(begin(HC8_COMMAND_CALIBRATE_PREAMBLE), end(HC8_COMMAND_CALIBRATE_PREAMBLE), begin(command));
  command[3] = baseline >> 8;
  command[4] = baseline;
  command[5] = 0;

  // the last byte is a checksum over the data
  for (uint8_t i = 0; i < 5; ++i)
    command[5] -= command[i];

  this->write_array(command);
  this->flush();
}

float HC8Component::get_setup_priority() const { return setup_priority::DATA; }

void HC8Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HC8:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  this->check_uart_settings(9600);

  ESP_LOGCONFIG(TAG, "  Warmup time: %" PRIu32 " s", this->warmup_seconds_);
}

}  // namespace esphome::hc8
