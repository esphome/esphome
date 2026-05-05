#include "ze15_co.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::ze15_co {

static const char *const TAG = "ze15_co";
static const uint8_t ZE15_CO_REQUEST_LENGTH = 8;
static const uint8_t ZE15_CO_RESPONSE_LENGTH = 9;
static const uint8_t ZE15_CO_COMMAND_QA_MODE_REQUEST_DATA[] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00};

void ZE15COComponent::dump_config() {
  LOG_SENSOR("", "ZE15-CO Sensor", this);
  ESP_LOGCONFIG(TAG,
                "  Mode: %s\n"
                "  Warmup time: %" PRIu32 " s",
                this->mode_ == Mode::QA ? "qa" : "stream", this->warmup_seconds_);
}

void ZE15COComponent::setup() {
  if (this->mode_ != Mode::QA) {
    this->stop_poller();
  }
}

void ZE15COComponent::update() {
  // Send Question command
  uint8_t response[ZE15_CO_RESPONSE_LENGTH];
  if (!this->ze15_co_write_command_(ZE15_CO_COMMAND_QA_MODE_REQUEST_DATA, response)) {
    ESP_LOGW(TAG, "Reading data from ZE15-CO failed!");
    this->status_set_warning();
    return;
  }

  // Check the response preamble
  if (response[0] != 0xFF || response[1] != 0x86) {
    ESP_LOGW(TAG, "Invalid preamble from ZE15-CO: Received: 0x%02X 0x%02X, but expected 0xFF 0x86", response[0],
             response[1]);
    this->status_set_warning();
    return;
  }

  // Validate and publish (fault bit at index 2, concentration at indices 2 and 3)
  this->validate_and_publish_(response, 2, 2, 3);
}

void ZE15COComponent::loop() {
  // This event is exclusive for the STREAM mode
  if (this->mode_ != Mode::STREAM)
    return;

  // Read data from UART
  while (this->available()) {
    uint8_t byte = this->read();
    this->process_stream_byte_(byte);
  }
}

uint8_t ze15_co_checksum(const uint8_t *command) {
  uint8_t sum = 0;
  for (uint8_t i = 1; i < ZE15_CO_REQUEST_LENGTH; i++) {
    sum += command[i];
  }
  return 0xFF - sum + 0x01;
}

void ZE15COComponent::validate_and_publish_(const uint8_t *data, uint8_t fault_idx, uint8_t conc_high_idx,
                                            uint8_t conc_low_idx) {
  // Check if we are in the warming period
  if (!this->warmup_complete_) {
    uint32_t now_ms = App.get_loop_component_start_time();
    uint32_t warmup_ms = this->warmup_seconds_ * 1000;
    if (now_ms < warmup_ms) {
      ESP_LOGW(TAG, "ZE15-CO warming up, %" PRIu32 " s left", (warmup_ms - now_ms) / 1000);
      this->status_set_warning();
      return;
    }
    this->warmup_complete_ = true;
  }

  // Check the response checksum
  uint8_t checksum = ze15_co_checksum(data);
  if (data[8] != checksum) {
    ESP_LOGVV(TAG, "Received values from ZE15-CO: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
              data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
    ESP_LOGW(TAG, "Checksum doesn't match: Received 0x%02X but expected 0x%02X", data[8], checksum);
    this->status_set_warning();
    return;
  }

  // Check the sensor fault judgement bit
  if ((data[fault_idx] & 0x80) != 0) {
    ESP_LOGW(TAG, "Received a faulty bit notification from the sensor");
    this->status_set_warning();
    return;
  }

  // Extract and publish the value
  this->status_clear_warning();
  uint16_t raw = ((data[conc_high_idx] & 0x1F) << 8) | data[conc_low_idx];
  float ppm = raw * 0.1f;
  this->publish_state(ppm);
}

void ZE15COComponent::process_stream_byte_(uint8_t byte) {
  // Wait for the preamble
  if (buffer_pos_ == 0 && byte != 0xFF)
    return;

  buffer_[buffer_pos_++] = byte;

  if (buffer_pos_ < ZE15_CO_RESPONSE_LENGTH)
    return;

  buffer_pos_ = 0;

  // Check gas type
  if (buffer_[1] != 0x04) {
    ESP_LOGW(TAG, "Invalid stream frame");
    this->status_set_warning();
    return;
  }

  // Validate and publish (fault bit at index 4, concentration at indices 4 and 5)
  this->validate_and_publish_(buffer_, 4, 4, 5);
}

bool ZE15COComponent::ze15_co_write_command_(const uint8_t *command, uint8_t *response) {
  // Clear RX Buffer
  while (this->available())
    this->read();
  this->write_array(command, ZE15_CO_REQUEST_LENGTH);
  this->write_byte(ze15_co_checksum(command));
  this->flush();

  if (response == nullptr)
    return true;

  return this->read_array(response, ZE15_CO_RESPONSE_LENGTH);
}

}  // namespace esphome::ze15_co
