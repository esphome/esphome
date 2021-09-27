#include <iostream>
#include <sstream>
#include "sml.h"
#include "esphome/core/log.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

static const char *const TAG = "sml";

const char START_BYTES_DETECTED = 1;
const char END_BYTES_DETECTED = 2;

char Sml::check_start_end_bytes_(char c) {
  // fill sml_file with incoming bytes
  for (int k = 1; k < 8; k++)
    this->incoming_buffer_[k - 1] = this->incoming_buffer_[k];
  this->incoming_buffer_[7] = c;

  if (memcmp(this->incoming_buffer_, START_BYTES, sizeof(START_BYTES)) == 0)
    return START_BYTES_DETECTED;
  if (memcmp(this->incoming_buffer_, END_BYTES, sizeof(END_BYTES)) == 0)
    return END_BYTES_DETECTED;
  return 0;
}

void Sml::loop() {
  while (available()) {
    const char c = read();

    if (this->record_)
      this->sml_data_.emplace_back(c);

    switch (this->check_start_end_bytes_(c)) {
      case START_BYTES_DETECTED: {
        this->record_ = true;
        this->sml_data_.assign(START_BYTES, START_BYTES + 8);
        break;
      };
      case END_BYTES_DETECTED: {
        if (this->record_) {
          this->record_ = false;
          this->process_sml_file_(this->sml_data_);
        }
        break;
      };
    };
  }
}

void Sml::process_sml_file_(const bytes &sml_data) {
  // check integrity of received sml data
  switch (check_sml_data(sml_data)) {
    case CHECK_CRC16_X25_SUCCESS: {
      ESP_LOGV(TAG, "Checksum verification successful with CRC16/X25.");
      break;
    };
    case CHECK_CRC16_KERMIT_SUCCESS: {
      ESP_LOGV(TAG, "Checksum verification successful with CRC16/KERMIT.");
      break;
    };
    default: {
      ESP_LOGW(TAG, "Checksum error in received SML data.");
      return;
    };
  };

  SmlFile sml_file = SmlFile(sml_data);
  std::vector<ObisInfo> obis_info = sml_file.get_obis_info();
  this->publish_obis_info_(obis_info);

  this->log_obis_info_(obis_info);
}

void Sml::log_obis_info_(const std::vector<ObisInfo> &obis_info_vec) {
  int i = 0;
  ESP_LOGD(TAG, "OBIS info:");
  for (auto const &obis_info : obis_info_vec) {
    std::ostringstream info_stream;
    info_stream << "  (" << bytes_repr(obis_info.server_id) << ") ";
    info_stream << obis_info.code_repr();
    info_stream << "  [0x" << bytes_repr(obis_info.value) << "]";
    std::string info = info_stream.str();
    ESP_LOGD(TAG, "%s", info.c_str());
  }
}

void Sml::publish_obis_info_(const std::vector<ObisInfo> &obis_info_vec) {
  for (auto const &obis_info : obis_info_vec) {
    this->publish_value_(obis_info);
  }
}

void Sml::publish_value_(const ObisInfo &obis_info) {
  for (auto const &sml_listener : sml_listeners_) {
    if ((!sml_listener->server_id.empty()) && (bytes_repr(obis_info.server_id) != sml_listener->server_id))
      continue;
    if (obis_info.code_repr() != sml_listener->obis)
      continue;
    sml_listener->publish_val(obis_info);
  }
}

void Sml::dump_config() { ESP_LOGCONFIG(TAG, "SML:"); }

void Sml::register_sml_listener(SmlListener *listener) { sml_listeners_.emplace_back(listener); }

}  // namespace sml
}  // namespace esphome
