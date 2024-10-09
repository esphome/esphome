#include "hydreon_rgxx.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hydreon_rgxx {

static const char *const TAG = "hydreon_rgxx.sensor";
static const int MAX_DATA_LENGTH_BYTES = 80;
static const uint8_t ASCII_LF = 0x0A;
#define HYDREON_RGXX_COMMA ,
static const char *const PROTOCOL_NAMES[] = {HYDREON_RGXX_PROTOCOL_LIST(, HYDREON_RGXX_COMMA)};
static const char *const IGNORE_STRINGS[] = {HYDREON_RGXX_IGNORE_LIST(, HYDREON_RGXX_COMMA)};

void HydreonRGxxComponent::dump_config() {
  this->check_uart_settings(9600, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
  ESP_LOGCONFIG(TAG, "hydreon_rgxx:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with hydreon_rgxx failed!");
  }
  if (model_ == RG9) {
    ESP_LOGCONFIG(TAG, "  Model: RG9");
    ESP_LOGCONFIG(TAG, "  Disable Led: %s", TRUEFALSE(this->disable_led_));
  } else {
    ESP_LOGCONFIG(TAG, "  Model: RG15");
    if (this->resolution_ == FORCE_HIGH) {
      ESP_LOGCONFIG(TAG, "  Resolution: high");
    } else {
      ESP_LOGCONFIG(TAG, "  Resolution: low");
    }
  }
  LOG_UPDATE_INTERVAL(this);

  int i = 0;
#define HYDREON_RGXX_LOG_SENSOR(s) \
  if (this->sensors_[i++] != nullptr) { \
    LOG_SENSOR("  ", #s, this->sensors_[i - 1]); \
  }
  HYDREON_RGXX_PROTOCOL_LIST(HYDREON_RGXX_LOG_SENSOR, );
}

void HydreonRGxxComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up hydreon_rgxx...");
  while (this->available() != 0) {
    this->read();
  }
  this->schedule_reboot_();
}

int HydreonRGxxComponent::num_sensors_missing_() {
  if (this->sensors_received_ == -1) {
    return -1;
  }
  int ret = NUM_SENSORS;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (this->sensors_[i] == nullptr) {
      ret -= 1;
      continue;
    }
    if ((this->sensors_received_ >> i & 1) != 0) {
      ret -= 1;
    }
  }
  return ret;
}

void HydreonRGxxComponent::update() {
  if (this->boot_count_ > 0) {
    if (this->num_sensors_missing_() > 0) {
      for (int i = 0; i < NUM_SENSORS; i++) {
        if (this->sensors_[i] == nullptr) {
          continue;
        }
        if ((this->sensors_received_ >> i & 1) == 0) {
          ESP_LOGW(TAG, "Missing %s", PROTOCOL_NAMES[i]);
        }
      }

      this->no_response_count_++;
      ESP_LOGE(TAG, "missing %d sensors; %d times in a row", this->num_sensors_missing_(), this->no_response_count_);
      if (this->no_response_count_ > 15) {
        ESP_LOGE(TAG, "asking sensor to reboot");
        for (auto &sensor : this->sensors_) {
          if (sensor != nullptr) {
            sensor->publish_state(NAN);
          }
        }
        this->schedule_reboot_();
        return;
      }
    } else {
      this->no_response_count_ = 0;
    }
    this->write_str("R\n");
#ifdef USE_BINARY_SENSOR
    if (this->too_cold_sensor_ != nullptr) {
      this->too_cold_sensor_->publish_state(this->too_cold_);
    }
    if (this->lens_bad_sensor_ != nullptr) {
      this->lens_bad_sensor_->publish_state(this->lens_bad_);
    }
    if (this->em_sat_sensor_ != nullptr) {
      this->em_sat_sensor_->publish_state(this->em_sat_);
    }
#endif
    this->too_cold_ = false;
    this->lens_bad_ = false;
    this->em_sat_ = false;
    this->sensors_received_ = 0;
  }
}

void HydreonRGxxComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_ += (char) data;
      if (this->buffer_.back() == static_cast<char>(ASCII_LF) || this->buffer_.length() >= MAX_DATA_LENGTH_BYTES) {
        // complete line received
        this->process_line_();
        this->buffer_.clear();
      }
    }
  }
}

/**
 * Communication with the sensor is asynchronous.
 * We send requests and let esphome continue doing its thing.
 * Once we have received a complete line, we process it.
 *
 * Catching communication failures is done in two layers:
 *
 * 1. We check if all requested data has been received
 *    before we send out the next request. If data keeps
 *    missing, we escalate.
 * 2. Request the sensor to reboot. We retry based on
 *    a timeout. If the sensor does not respond after
 *    several boot attempts, we give up.
 */
void HydreonRGxxComponent::schedule_reboot_() {
  this->boot_count_ = 0;
  this->set_interval("reboot", 5000, [this]() {
    if (this->boot_count_ < 0) {
      ESP_LOGW(TAG, "hydreon_rgxx failed to boot %d times", -this->boot_count_);
    }
    this->boot_count_--;
    this->write_str("K\n");
    if (this->boot_count_ < -5) {
      ESP_LOGE(TAG, "hydreon_rgxx can't boot, giving up");
      for (auto &sensor : this->sensors_) {
        if (sensor != nullptr) {
          sensor->publish_state(NAN);
        }
      }
      this->mark_failed();
    }
  });
}

bool HydreonRGxxComponent::buffer_starts_with_(const std::string &prefix) {
  return this->buffer_starts_with_(prefix.c_str());
}

bool HydreonRGxxComponent::buffer_starts_with_(const char *prefix) { return buffer_.rfind(prefix, 0) == 0; }

void HydreonRGxxComponent::process_line_() {
  ESP_LOGV(TAG, "Read from serial: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());

  if (buffer_[0] == ';') {
    ESP_LOGI(TAG, "Comment: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    return;
  }
  std::string::size_type newlineposn = this->buffer_.find('\n');
  if (newlineposn <= 1) {
    // allow both \r\n and \n
    ESP_LOGD(TAG, "Received empty line");
    return;
  }
  if (newlineposn <= 2) {
    // single character lines, such as acknowledgements
    ESP_LOGD(TAG, "Received ack: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    return;
  }
  if (this->buffer_.find("LensBad") != std::string::npos) {
    ESP_LOGW(TAG, "Received LensBad!");
    this->lens_bad_ = true;
  }
  if (this->buffer_.find("EmSat") != std::string::npos) {
    ESP_LOGW(TAG, "Received EmSat!");
    this->em_sat_ = true;
  }
  if (this->buffer_starts_with_("PwrDays")) {
    if (this->boot_count_ <= 0) {
      this->boot_count_ = 1;
    } else {
      this->boot_count_++;
    }
    this->cancel_interval("reboot");
    this->no_response_count_ = 0;
    ESP_LOGI(TAG, "Boot detected: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());

    if (this->model_ == RG15) {
      if (this->resolution_ == FORCE_HIGH) {
        this->write_str("P\nH\nM\n");  // set sensor to (P)polling mode, (H)high res mode, (M)metric mode
      } else {
        this->write_str("P\nL\nM\n");  // set sensor to (P)polling mode, (L)low res mode, (M)metric mode
      }
    }

    if (this->model_ == RG9) {
      this->write_str("P\n");  // set sensor to (P)polling mode

      if (this->disable_led_) {
        this->write_str("D 1\n");  // set sensor (D 1)rain detection LED disabled
      } else {
        this->write_str("D 0\n");  // set sensor (D 0)rain detection LED enabled
      }
    }
    return;
  }
  if (this->buffer_starts_with_("SW")) {
    std::string::size_type majend = this->buffer_.find('.');
    std::string::size_type endversion = this->buffer_.find(' ', 3);
    if (majend == std::string::npos || endversion == std::string::npos || majend > endversion) {
      ESP_LOGW(TAG, "invalid version string: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    }
    int major = strtol(this->buffer_.substr(3, majend - 3).c_str(), nullptr, 10);
    int minor = strtol(this->buffer_.substr(majend + 1, endversion - (majend + 1)).c_str(), nullptr, 10);

    if (major > 10 || minor >= 1000 || minor < 0 || major < 0) {
      ESP_LOGW(TAG, "invalid version: %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
    }
    this->sw_version_ = major * 1000 + minor;
    ESP_LOGI(TAG, "detected sw version %i", this->sw_version_);
    return;
  }
  bool is_data_line = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (this->sensors_[i] != nullptr && this->buffer_.find(PROTOCOL_NAMES[i]) != std::string::npos) {
      is_data_line = true;
      break;
    }
  }
  if (is_data_line) {
    std::string::size_type tc = this->buffer_.find("TooCold");
    this->too_cold_ |= tc != std::string::npos;
    if (this->too_cold_) {
      ESP_LOGD(TAG, "Received TooCold");
    }
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (this->sensors_[i] == nullptr) {
        continue;
      }
      std::string::size_type n = this->buffer_.find(PROTOCOL_NAMES[i]);
      if (n == std::string::npos) {
        continue;
      }

      if (n == this->buffer_.find('t', n)) {
        // The device temperature ('t') response contains both °C and °F values:
        // "t 72F 22C".
        // ESPHome uses only °C, only parse °C value (move past 'F').
        n = this->buffer_.find('F', n);
        if (n == std::string::npos) {
          continue;
        }
        n += 1;  // move past 'F'
      } else {
        n += strlen(PROTOCOL_NAMES[i]);  // move past protocol name
      }

      // parse value, starting at str position n
      float data = strtof(this->buffer_.substr(n).c_str(), nullptr);
      this->sensors_[i]->publish_state(data);
      ESP_LOGD(TAG, "Received %s: %f", PROTOCOL_NAMES[i], this->sensors_[i]->get_raw_state());
      this->sensors_received_ |= (1 << i);
    }
    if (this->request_temperature_ && this->num_sensors_missing_() == 1) {
      this->write_str("T\n");
    }
  } else {
    for (const auto *ignore : IGNORE_STRINGS) {
      if (this->buffer_starts_with_(ignore)) {
        ESP_LOGI(TAG, "Ignoring %s", this->buffer_.substr(0, this->buffer_.size() - 2).c_str());
        return;
      }
    }
    ESP_LOGI(TAG, "Got unknown line: %s", this->buffer_.c_str());
  }
}

float HydreonRGxxComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace hydreon_rgxx
}  // namespace esphome
