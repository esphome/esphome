#include "hydreon_rgxx.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::hydreon_rgxx {

ESPHOME_LOG_TAG(TAG, "hydreon_rgxx.sensor");

static constexpr uint32_t REBOOT_INTERVAL_ID = 0;

#define HYDREON_RGXX_COMMA ,
static const char *const PROTOCOL_NAMES[] = {HYDREON_RGXX_PROTOCOL_LIST(, HYDREON_RGXX_COMMA)};
static const char *const IGNORE_STRINGS[] = {HYDREON_RGXX_IGNORE_LIST(, HYDREON_RGXX_COMMA)};

void HydreonRGxxComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "hydreon_rgxx:");
  if (model_ == RG9) {
    ESP_LOGCONFIG(TAG,
                  "  Model: RG9\n"
                  "  Disable Led: %s",
                  TRUEFALSE(this->disable_led_));
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
  this->discard_available();
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
    if (!this->read_byte(&data)) {
      continue;
    }
    size_t len;
    const char *line = this->line_reader_.feed(data, len);
    this->line_reader_.log_if_overflowed(TAG);
    if (line != nullptr) {
      this->process_line_(StringRef(line, len));
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
  this->set_interval(REBOOT_INTERVAL_ID, 5000, [this]() {
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
      this->mark_failed(LOG_STR("Sensor did not boot"));
    }
  });
}

void HydreonRGxxComponent::process_line_(StringRef line) {
  ESP_LOGV(TAG, "Read from serial: %s", line.c_str());

  if (line[0] == ';') {
    ESP_LOGI(TAG, "Comment: %s", line.c_str());
    return;
  }
  if (line.empty()) {
    ESP_LOGD(TAG, "Received empty line");
    return;
  }
  if (line.size() == 1) {
    // single character lines, such as acknowledgements
    ESP_LOGD(TAG, "Received ack: %s", line.c_str());
    return;
  }
  if (line.find("LensBad") != std::string::npos) {
    ESP_LOGW(TAG, "Received LensBad!");
    this->lens_bad_ = true;
  }
  if (line.find("EmSat") != std::string::npos) {
    ESP_LOGW(TAG, "Received EmSat!");
    this->em_sat_ = true;
  }
  if (line.starts_with("PwrDays")) {
    if (this->boot_count_ <= 0) {
      this->boot_count_ = 1;
    } else {
      this->boot_count_++;
    }
    this->cancel_interval(REBOOT_INTERVAL_ID);
    this->no_response_count_ = 0;
    ESP_LOGI(TAG, "Boot detected: %s", line.c_str());

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
  if (line.starts_with("SW")) {
    StringRef::size_type majend = line.find('.');
    StringRef::size_type endversion = line.find(' ', 3);
    if (majend == std::string::npos || endversion == std::string::npos || majend > endversion) {
      ESP_LOGW(TAG, "invalid version string: %s", line.c_str());
    }
    // strtol stops at the '.' and the ' '
    int major = line.size() > 3 ? strtol(line.c_str() + 3, nullptr, 10) : 0;
    int minor = strtol(line.c_str() + (majend == std::string::npos ? 0 : majend + 1), nullptr, 10);

    if (major > 10 || minor >= 1000 || minor < 0 || major < 0) {
      ESP_LOGW(TAG, "invalid version: %s", line.c_str());
    }
    this->sw_version_ = major * 1000 + minor;
    ESP_LOGI(TAG, "detected sw version %i", this->sw_version_);
    return;
  }
  bool is_data_line = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (this->sensors_[i] == nullptr) {
      continue;
    }
    StringRef::size_type n = line.find(PROTOCOL_NAMES[i]);
    if (n == std::string::npos) {
      continue;
    }
    is_data_line = true;

    if (n == line.find('t', n)) {
      // The device temperature ('t') response contains both °C and °F values:
      // "t 72F 22C".
      // ESPHome uses only °C, only parse °C value (move past 'F').
      n = line.find('F', n);
      if (n == std::string::npos) {
        continue;
      }
      n += 1;  // move past 'F'
    } else {
      n += strlen(PROTOCOL_NAMES[i]);  // move past protocol name
    }

    // parse value, starting at str position n
    float data = strtof(line.c_str() + n, nullptr);
    this->sensors_[i]->publish_state(data);
    ESP_LOGD(TAG, "Received %s: %f", PROTOCOL_NAMES[i], this->sensors_[i]->get_raw_state());
    this->sensors_received_ |= (1 << i);
  }
  if (is_data_line) {
    StringRef::size_type tc = line.find("TooCold");
    this->too_cold_ |= tc != std::string::npos;
    if (this->too_cold_) {
      ESP_LOGD(TAG, "Received TooCold");
    }
    if (this->request_temperature_ && this->num_sensors_missing_() == 1) {
      this->write_str("T\n");
    }
  } else {
    for (const auto *ignore : IGNORE_STRINGS) {
      if (line.starts_with(ignore)) {
        ESP_LOGI(TAG, "Ignoring %s", line.c_str());
        return;
      }
    }
    ESP_LOGI(TAG, "Got unknown line: %s", line.c_str());
  }
}

}  // namespace esphome::hydreon_rgxx
