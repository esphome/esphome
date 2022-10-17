#include "q3da.h"
#include "esphome/core/log.h"
#include "q3da_parser.h"

namespace esphome {
namespace q3da {

static const char *const TAG = "q3da";

const char START_BYTES_DETECTED = 1;
const char END_BYTES_DETECTED = 2;

Q3DAListener::Q3DAListener(std::string server_id, std::string obis_code)
    : server_id(std::move(server_id)), obis_code(std::move(obis_code)) {}

char q3da::check_start_end_bytes_(uint8_t byte) {
  if (byte == START_MASK)
    return START_BYTES_DETECTED;
  if (byte == END_MASK)
    return END_BYTES_DETECTED;
  return 0;
}

void q3da::loop() {
  while (available()) {
    const char c = read();

    if (this->record_)
      this->q3da_data_.emplace_back(c);

    switch (this->check_start_end_bytes_(c)) {
      case START_BYTES_DETECTED: {
        this->record_ = true;
        this->q3da_data_.clear();
        break;
      };
      case END_BYTES_DETECTED: {
        if (this->record_) {
          this->record_ = false;

          if (!check_q3da_data(this->q3da_data_))
            break;

          // remove footer bytes
          // this->q3da_data_.resize(this->q3da_data_.size() - 2);
          this->process_q3da_telegram_(this->q3da_data_);
        }
        break;
      };
    };
  }
}

void q3da::process_q3da_telegram_(const bytes &q3da_data) {
  // ESP_LOGV(TAG, "Received a telegram: %d", q3da_data.size());
  Q3DATelegram q3da_file = Q3DATelegram(q3da_data);
  std::vector<ObisInfo> obis_info = q3da_file.get_obis_info();
  this->publish_obis_info_(obis_info);

  this->log_obis_info_(obis_info);
}

void q3da::log_obis_info_(const std::vector<ObisInfo> &obis_info_vec) {
  ESP_LOGD(TAG, "OBIS info:");
  for (auto const &obis_info : obis_info_vec) {
    std::string info;
    info += "  (" + bytes_repr(obis_info.server_id) + ") ";
    info += obis_info.code_repr();
    // info += " [0x" + bytes_repr(obis_info.value) + "]";
    ESP_LOGD(TAG, "%s", info.c_str());
  }
}

void q3da::publish_obis_info_(const std::vector<ObisInfo> &obis_info_vec) {
  for (auto const &obis_info : obis_info_vec) {
    this->publish_value_(obis_info);
  }
}

void q3da::publish_value_(const ObisInfo &obis_info) {
  for (auto const &q3da_listener : q3da_listeners_) {
    // if ((!q3da_listener->server_id.empty()) && (bytes_repr(obis_info.server_id) != q3da_listener->server_id))
    //  continue;
    if (obis_info.code_repr() != q3da_listener->obis_code)
      continue;
    q3da_listener->publish_val(obis_info);
  }
}

void q3da::dump_config() { ESP_LOGCONFIG(TAG, "Q3DA:"); }

void q3da::register_q3da_listener(Q3DAListener *listener) { q3da_listeners_.emplace_back(listener); }

bool check_q3da_data(const bytes &buffer) {
  if (buffer.size() < 2) {
    ESP_LOGW(TAG, "Too less Q3DA data received.");
    return false;
  }

  // Todo: do more fancy checking.

  return true;
}

}  // namespace q3da
}  // namespace esphome
