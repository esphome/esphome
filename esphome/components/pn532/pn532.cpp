#include "pn532.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532 {

static const char *TAG = "pn532";

void format_uid(char *buf, const uint8_t *uid, uint8_t uid_length) {
  int offset = 0;
  for (uint8_t i = 0; i < uid_length; i++) {
    const char *format = "%02X";
    if (i + 1 < uid_length)
      format = "%02X-";
    offset += sprintf(buf + offset, format, uid[i]);
  }
}

void PN532::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PN532...");

  // send dummy firmware version command to get synced up
  this->pn532_write_commandcheck_ack_({0x02});  // get firmware version command
  // do not actually read any data, this should be OK according to datasheet

  this->pn532_write_command_check_ack_({
      PN532_COMMAND_SAMCONFIGURATION,
      0x01,  // normal mode
      0x14,  // zero timeout (not in virtual card mode)
      0x01,
  });

  // read data packet for wakeup result
  auto wakeup_result = this->pn532_read_data();
  if (wakeup_result.size() != 1) {
    this->error_code_ = WAKEUP_FAILED;
    this->mark_failed();
    return;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min(255u, this->update_interval_ / 50);
  bool ret = this->pn532_write_commandcheck_ack_({
      0x14,         // SAM config command
      0x01,         // normal mode
      sam_timeout,  // timeout as multiple of 50ms (actually only for virtual card mode, but shouldn't matter)
      0x01,         // Enable IRQ
  });

  if (!ret) {
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  auto sam_result = this->pn532_read_data();
  if (sam_result.size() != 1) {
    ESP_LOGV(TAG, "Invalid SAM result: (%u)", sam_result.size());  // NOLINT
    for (auto dat : sam_result) {
      ESP_LOGV(TAG, " 0x%02X", dat);
    }
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  this->turn_off_rf_();
}

void PN532::update() {
  for (auto *obj : this->binary_sensors_)
    obj->on_scan_end();

  bool success = this->pn532_write_commandcheck_ack_({
      0x4A,  // INLISTPASSIVETARGET
      0x01,  // max 1 card
      0x00,  // baud rate ISO14443A (106 kbit/s)
  });
  if (!success) {
    ESP_LOGW(TAG, "Requesting tag read failed!");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->requested_read_ = true;
}

void PN532::loop() {
  if (!this->requested_read_)
    return;

  auto read = this->pn532_read_data();
  this->requested_read_ = false;

  if (read.size() <= 2 || read[0] != 0x4B) {
    // Something failed
    this->turn_off_rf_();
    return;
  }

  uint8_t num_targets = read[1];
  if (num_targets != 1) {
    // no tags found or too many
    this->turn_off_rf_();
    return;
  }

  // const uint8_t target_number = read[2];
  // const uint16_t sens_res = uint16_t(read[3] << 8) | read[4];
  // const uint8_t sel_res = read[5];
  const uint8_t nfcid_length = read[6];
  const uint8_t *nfcid = &read[7];
  if (read.size() < 7U + nfcid_length) {
    // oops, pn532 returned invalid data
    return;
  }

  bool report = true;
  // 1. Go through all triggers
  for (auto *trigger : this->triggers_)
    trigger->process(nfcid, nfcid_length);

  // 2. Find a binary sensor
  for (auto *tag : this->binary_sensors_) {
    if (tag->process(nfcid, nfcid_length)) {
      // 2.1 if found, do not dump
      report = false;
    }
  }

  if (report) {
    char buf[32];
    format_uid(buf, nfcid, nfcid_length);
    ESP_LOGD(TAG, "Found new tag '%s'", buf);
  }

  this->turn_off_rf_();
}

void PN532::turn_off_rf_() {
  ESP_LOGVV(TAG, "Turning RF field OFF");
  this->pn532_write_commandcheck_ack_({
      0x32,  // RFConfiguration
      0x1,   // RF Field
      0x0    // Off
  });
}

float PN532::get_setup_priority() const { return setup_priority::DATA; }

bool PN532::pn532_write_commandcheck_ack_(const std::vector<uint8_t> &data) {
  // 1. write command
  this->pn532_write_command(data);

  // 2. wait for readiness
  if (!this->wait_ready_())
    return false;

  // 3. read ack
  if (!this->read_ack()) {
    ESP_LOGV(TAG, "Invalid ACK frame received from PN532!");
    return false;
  }

  return true;
}

bool PN532::wait_ready_() {
  uint32_t start_time = millis();
  while (!this->is_ready()) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Timed out waiting for readiness from PN532!");
      return false;
    }
    yield();
  }
  return true;
}

void PN532::dump_config() {
  ESP_LOGCONFIG(TAG, "PN532:");
  switch (this->error_code_) {
    case NONE:
      break;
    case WAKEUP_FAILED:
      ESP_LOGE(TAG, "Wake Up command failed!");
      break;
    case SAM_COMMAND_FAILED:
      ESP_LOGE(TAG, "SAM command failed!");
      break;
  }

  LOG_UPDATE_INTERVAL(this);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

bool PN532BinarySensor::process(const uint8_t *data, uint8_t len) {
  if (len != this->uid_.size())
    return false;

  for (uint8_t i = 0; i < len; i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}
void PN532Trigger::process(const uint8_t *uid, uint8_t uid_length) {
  char buf[32];
  format_uid(buf, uid, uid_length);
  this->trigger(std::string(buf));
}

}  // namespace pn532
}  // namespace esphome
