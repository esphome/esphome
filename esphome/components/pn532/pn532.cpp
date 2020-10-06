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

  // Get version data
  if (!this->write_command_({0x02})) {
    ESP_LOGE(TAG, "Error sending version command");
    this->mark_failed();
    return;
  }

  auto version_data = this->read_response_();

  if (version_data.empty()) {
    ESP_LOGE(TAG, "Error getting version");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Found chip PN%02X", version_data[0]);
  ESP_LOGD(TAG, "Firmware ver. %d.%d", version_data[1], version_data[2]);

  if (!this->write_command_({
          0x14,  // SAM config command
          0x01,  // normal mode
          0x14,  // zero timeout (not in virtual card mode)
          0x01,
      })) {
    ESP_LOGE(TAG, "No wakeup ack");
    this->mark_failed();
    return;
  }

  // read data packet for wakeup result
  auto wakeup_result = this->read_response_();
  if (wakeup_result.size() != 1) {
    this->error_code_ = WAKEUP_FAILED;
    this->mark_failed();
    return;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min(255u, this->update_interval_ / 50);
  if (!this->write_command_({
          0x14,         // SAM config command
          0x01,         // normal mode
          sam_timeout,  // timeout as multiple of 50ms (actually only for virtual card mode, but shouldn't matter)
          0x01,         // Enable IRQ
      })) {
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  auto sam_result = this->read_response_();
  if (sam_result.size() != 1) {
    ESP_LOGV(TAG, "Invalid SAM result: (%u)", sam_result.size());  // NOLINT
    for (uint8_t dat : sam_result) {
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

  if (!this->write_command_({
          0x4A,  // INLISTPASSIVETARGET
          0x01,  // max 1 card
          0x00,  // baud rate ISO14443A (106 kbit/s)
      })) {
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

  auto read = this->read_response_();
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
  this->write_command_({
      0x32,  // RFConfiguration
      0x1,   // RF Field
      0x0    // Off
  });
}

bool PN532::write_command_(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> write_data;
  // Preamble
  write_data.push_back(0x00);

  // Start code
  write_data.push_back(0x00);
  write_data.push_back(0xFF);

  // Length of message, TFI + data bytes
  const uint8_t real_length = data.size() + 1;
  // LEN
  write_data.push_back(real_length);
  // LCS (Length checksum)
  write_data.push_back(~real_length + 1);

  // TFI (Frame Identifier, 0xD4 means to PN532, 0xD5 means from PN532)
  write_data.push_back(0xD4);
  // calculate checksum, TFI is part of checksum
  uint8_t checksum = 0xD4;

  // DATA
  for (uint8_t dat : data) {
    write_data.push_back(dat);
    checksum += dat;
  }

  // DCS (Data checksum)
  write_data.push_back(~checksum + 1);
  // Postamble
  write_data.push_back(0x00);

  this->write_data(write_data);

  return this->read_ack();
}

std::vector<uint8_t> PN532::read_response_() {
  uint8_t len = this->read_response_length_();
  if (len < 0) {
    return {};
  }

  std::vector<uint8_t> data = this->read_data(6 + len + 2);
  if (data.empty()) {
    return {};
  }

  if (data[1] != 0x00 && data[2] != 0x00 && data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return {};
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5);                                // TFI - frame from PN532 to system controller

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return {};
  }

  data.erase(data.begin(), data.begin() + 7);

  uint8_t checksum = 0xD5;
  for (int i = 0; i < len; i++) {
    uint8_t dat = data[i];
    checksum += dat;
  }
  checksum = ~checksum + 1;

  if (data[len] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", data[len], checksum);
    return {};
  }

  if (data[len + 1] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return {};
  }

  data.erase(data.end() - 2, data.end());

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGD(TAG, "PN532 Data Frame: (%u)", data.size());  // NOLINT
  for (uint8_t dat : data) {
    ESP_LOGD(TAG, "  0x%02X", dat);
  }
#endif

  return data;
}

uint8_t PN532::read_response_length_() {
  std::vector<uint8_t> data = this->read_data(6);
  if (data.empty()) {
    return -1;
  }

  if (data[1] != 0x00 && data[2] != 0x00 && data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return -1;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5);                                // TFI - frame from PN532 to system controller

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return -1;
  }

  this->write_data({0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00});  // NACK - Retransmit last message

  // full length of message, including TFI
  uint8_t full_len = data[4];
  // length of data, excluding TFI
  uint8_t len = full_len - 1;
  if (full_len == 0)
    len = 0;
  return len;
}

bool PN532::read_ack_() {
  ESP_LOGVV(TAG, "Reading ACK...");

  std::vector<uint8_t> data = this->read_data(6);
  if (data.empty()) {
    return false;
  }

  bool matches = (data[1] == 0x00 &&                     // preamble
                  data[2] == 0x00 &&                     // start of packet
                  data[3] == 0xFF && data[4] == 0x00 &&  // ACK packet code
                  data[5] == 0xFF && data[6] == 0x00);   // postamble
  ESP_LOGVV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

float PN532::get_setup_priority() const { return setup_priority::LATE; }

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
