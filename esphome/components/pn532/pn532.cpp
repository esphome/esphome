#include "pn532.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532 {

static const char *TAG = "pn532";

std::string format_uid(std::vector<uint8_t> &uid) {
  char buf[32];
  int offset = 0;
  for (uint8_t i = 0; i < uid.size(); i++) {
    const char *format = "%02X";
    if (i + 1 < uid.size())
      format = "%02X-";
    offset += sprintf(buf + offset, format, uid[i]);
  }
  return std::string(buf);
}

void PN532::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PN532...");

  // Get version data
  if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
    ESP_LOGE(TAG, "Error sending version command");
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> version_data;
  if (!this->read_response_(PN532_COMMAND_VERSION_DATA, version_data)) {
    ESP_LOGE(TAG, "Error getting version");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Found chip PN5%02X", version_data[0]);
  ESP_LOGD(TAG, "Firmware ver. %d.%d", version_data[1], version_data[2]);

  if (!this->write_command_({
          PN532_COMMAND_SAMCONFIGURATION,
          0x01,  // normal mode
          0x14,  // zero timeout (not in virtual card mode)
          0x01,
      })) {
    ESP_LOGE(TAG, "No wakeup ack");
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> wakeup_result;
  if (!this->read_response_(PN532_COMMAND_SAMCONFIGURATION, wakeup_result)) {
    this->error_code_ = WAKEUP_FAILED;
    this->mark_failed();
    return;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min(255u, this->update_interval_ / 50);
  if (!this->write_command_({
          PN532_COMMAND_SAMCONFIGURATION,
          0x01,         // normal mode
          sam_timeout,  // timeout as multiple of 50ms (actually only for virtual card mode, but shouldn't matter)
          0x01,         // Enable IRQ
      })) {
    this->error_code_ = SAM_COMMAND_FAILED;
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> sam_result;
  if (!this->read_response_(PN532_COMMAND_SAMCONFIGURATION, sam_result)) {
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
          PN532_COMMAND_INLISTPASSIVETARGET,
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

  std::vector<uint8_t> read;
  bool success = this->read_response_(PN532_COMMAND_INLISTPASSIVETARGET, read);

  this->requested_read_ = false;

  if (!success) {
    // Something failed
    this->current_uid_ = {};
    this->turn_off_rf_();
    return;
  }

  uint8_t num_targets = read[0];
  if (num_targets != 1) {
    // no tags found or too many
    this->current_uid_ = {};
    this->turn_off_rf_();
    return;
  }

  uint8_t nfcid_length = read[5];
  std::vector<uint8_t> nfcid(read.begin() + 6, read.begin() + 6 + nfcid_length);
  if (read.size() < 6U + nfcid_length) {
    // oops, pn532 returned invalid data
    return;
  }

  bool report = true;
  for (auto *bin_sens : this->binary_sensors_) {
    if (bin_sens->process(nfcid)) {
      report = false;
    }
  }

  if (nfcid.size() == this->current_uid_.size()) {
    bool same_uid = false;
    for (uint8_t i = 0; i < nfcid.size(); i++)
      same_uid |= nfcid[i] == this->current_uid_[i];
    if (same_uid)
      return;
  }

  this->current_uid_ = nfcid;

  for (auto *trigger : this->triggers_)
    trigger->process(nfcid);

  if (report) {
    ESP_LOGD(TAG, "Found new tag '%s'", format_uid(nfcid).c_str());
  }

  this->turn_off_rf_();
}

void PN532::turn_off_rf_() {
  ESP_LOGVV(TAG, "Turning RF field OFF");
  this->write_command_({
      PN532_COMMAND_RFCONFIGURATION,
      0x1,  // RF Field
      0x0   // Off
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

  return this->read_ack_();
}

bool PN532::read_response_(uint8_t command, std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Reading response");
  uint8_t len = this->read_response_length_();
  if (len == 0) {
    return false;
  }

  ESP_LOGV(TAG, "Reading response of length %d", len);
  if (!this->read_data(data, 6 + len + 2)) {
    ESP_LOGD(TAG, "No response data");
    return false;
  }

  if (data[1] != 0x00 && data[2] != 0x00 && data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return false;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5 &&                               // TFI - frame from PN532 to system controller
                       data[7] == command + 1);                         // Correct command response

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return false;
  }

  data.erase(data.begin(), data.begin() + 6);  // Remove headers

  uint8_t checksum = 0;
  for (int i = 0; i < len + 1; i++) {
    uint8_t dat = data[i];
    checksum += dat;
  }
  checksum = ~checksum + 1;

  if (data[len + 1] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", data[len], checksum);
    return false;
  }

  if (data[len + 2] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return false;
  }

  data.erase(data.begin(), data.begin() + 2);  // Remove TFI and command code
  data.erase(data.end() - 2, data.end());      // Remove checksum and postamble

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGD(TAG, "PN532 Data Frame: (%u)", data.size());  // NOLINT
  for (uint8_t dat : data) {
    ESP_LOGD(TAG, "  0x%02X", dat);
  }
#endif

  return true;
}

uint8_t PN532::read_response_length_() {
  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return 0;
  }

  if (data[1] != 0x00 && data[2] != 0x00 && data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return 0;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5);                                // TFI - frame from PN532 to system controller

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return 0;
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

  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return false;
  }

  bool matches = (data[1] == 0x00 &&                     // preamble
                  data[2] == 0x00 &&                     // start of packet
                  data[3] == 0xFF && data[4] == 0x00 &&  // ACK packet code
                  data[5] == 0xFF && data[6] == 0x00);   // postamble
  ESP_LOGVV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

float PN532::get_setup_priority() const { return setup_priority::DATA; }

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

bool PN532BinarySensor::process(std::vector<uint8_t> &data) {
  if (data.size() != this->uid_.size())
    return false;

  for (uint8_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}
void PN532Trigger::process(std::vector<uint8_t> &data) { this->trigger(format_uid(data)); }

}  // namespace pn532
}  // namespace esphome
