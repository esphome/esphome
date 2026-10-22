#include "cw2017.h"

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_system.h>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
extern "C" {
#include <user_interface.h>
}
#endif

namespace esphome::cw2017 {

ESPHOME_LOG_TAG(TAG, "cw2017");

static constexpr uint8_t REG_VERSION = 0x00;
static constexpr uint8_t REG_VCELL = 0x02;
static constexpr uint8_t REG_SOC = 0x04;
static constexpr uint8_t REG_MODE = 0x08;
static constexpr uint8_t REG_SOC_ALERT = 0x0B;
static constexpr uint8_t REG_BATINFO = 0x10;

static constexpr uint8_t MODE_NORMAL = 0x00;
static constexpr uint8_t MODE_SLEEP = 0x30;
static constexpr uint8_t UPDATE_FLAG = 0x80;
static constexpr uint8_t RESTART_MODES[] PROGMEM = {0xF0, MODE_SLEEP, MODE_NORMAL};
static constexpr uint8_t RESTART_STEPS = sizeof(RESTART_MODES);

static constexpr uint16_t VCELL_MASK = 0x3FFF;
static constexpr float VCELL_VOLTS_PER_LSB = 312.5e-6f;

static constexpr uint32_t STEP_DELAY_MS = 20;
static constexpr uint32_t READY_POLL_MS = 100;
static constexpr uint8_t READY_ATTEMPTS = 40;
static constexpr uint32_t INIT_RETRY_MS = 1000;
static constexpr uint8_t INIT_ATTEMPTS = 3;

static bool version_is_running(uint8_t version) { return (version & 0xFD) == 0x0D; }

// The configured profile only changes with new firmware, and flashing never ends in a deep-sleep wake.
static bool woke_from_deep_sleep() {
#if defined(USE_ESP32)
  return esp_reset_reason() == ESP_RST_DEEPSLEEP;
#elif defined(USE_ESP8266)
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  return ESP.getResetInfoPtr()->reason == REASON_DEEP_SLEEP_AWAKE;
#else
  return false;
#endif
}

bool CW2017::read_profile_matches_(bool &matches) {
  uint8_t stored[PROFILE_SIZE];
  uint8_t expected[PROFILE_SIZE];
  if (!this->read_bytes(REG_BATINFO, stored, PROFILE_SIZE))
    return false;
  progmem_memcpy(expected, this->profile_, PROFILE_SIZE);
  matches = std::memcmp(stored, expected, PROFILE_SIZE) == 0;
  return true;
}

bool CW2017::upload_profile_() {
  // Built here with the register first, so the bus layer does not heap-allocate a buffer this size.
  uint8_t buffer[1 + PROFILE_SIZE] = {REG_BATINFO};
  progmem_memcpy(buffer + 1, this->profile_, PROFILE_SIZE);
  return this->write(buffer, sizeof(buffer)) == i2c::ERROR_OK && this->write_byte(REG_SOC_ALERT, UPDATE_FLAG);
}

void CW2017::setup() { this->init_(); }

// Retried because another device resetting on the same bus can disturb an early transfer.
void CW2017::init_failed_(const LogString *reason) {
  if (++this->init_attempts_ >= INIT_ATTEMPTS) {
    this->mark_failed(reason);
    return;
  }
  this->set_timeout(INIT_RETRY_MS, [this]() { this->init_(); });
}

void CW2017::init_() {
  // A profile upload only takes effect after a restart, so a failed restart is retried, not skipped.
  if (this->restart_pending_) {
    this->restart_();
    return;
  }

  uint8_t mode = 0;
  uint8_t version = 0;
  uint8_t config = 0;
  if (!this->read_byte(REG_MODE, &mode) || !this->read_byte(REG_VERSION, &version) ||
      !this->read_byte(REG_SOC_ALERT, &config)) {
    this->init_failed_(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  const bool has_profile = (config & UPDATE_FLAG) != 0;
  bool matches = has_profile && woke_from_deep_sleep();
  if (has_profile && !matches && !this->read_profile_matches_(matches)) {
    this->init_failed_(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  if (!matches) {
    ESP_LOGI(TAG, "Uploading battery profile");
    if (!this->upload_profile_()) {
      this->init_failed_(LOG_STR("Battery profile upload failed"));
      return;
    }
    this->restart_();
    return;
  }

  if (mode == MODE_SLEEP) {
    // The gauge keeps its profile and state through its own sleep, so a wake is a mode write, not a restart.
    if (!this->write_byte(REG_MODE, MODE_NORMAL)) {
      this->init_failed_(LOG_STR("Gauge wake failed"));
      return;
    }
    this->set_timeout(STEP_DELAY_MS, [this]() { this->wait_ready_(0); });
    return;
  }

  if (mode != MODE_NORMAL || !version_is_running(version)) {
    this->restart_();
    return;
  }

  this->wait_ready_(0);
}

void CW2017::restart_() {
  this->restart_pending_ = true;
  this->set_timeout(STEP_DELAY_MS, [this]() { this->restart_step_(0); });
}

void CW2017::restart_step_(uint8_t step) {
  if (!this->write_byte(REG_MODE, progmem_read_byte(&RESTART_MODES[step]))) {
    this->init_failed_(LOG_STR("Restart failed"));
    return;
  }
  if (step + 1 < RESTART_STEPS) {
    this->set_timeout(STEP_DELAY_MS, [this, step]() { this->restart_step_(step + 1); });
  } else {
    this->restart_pending_ = false;
    this->set_timeout(STEP_DELAY_MS, [this]() { this->wait_ready_(0); });
  }
}

void CW2017::wait_ready_(uint8_t attempt) {
  uint8_t version = 0;
  uint8_t soc = 0;
  if (this->read_byte(REG_VERSION, &version) && version_is_running(version) && this->read_byte(REG_SOC, &soc) &&
      soc <= 100) {
    ESP_LOGD(TAG, "Gauge ready %u ms after boot", (unsigned) millis());
    this->ready_ = true;
    this->update();
    return;
  }
  if (attempt + 1 >= READY_ATTEMPTS) {
    this->init_failed_(LOG_STR("Gauge did not become ready"));
    return;
  }
  this->set_timeout(READY_POLL_MS, [this, attempt]() { this->wait_ready_(attempt + 1); });
}

void CW2017::update() {
  if (!this->ready_)
    return;

  if (this->battery_level_sensor_ != nullptr) {
    uint8_t soc = 0;
    if (this->read_byte(REG_SOC, &soc) && soc <= 100) {
      this->battery_level_sensor_->publish_state(soc);
    } else {
      this->status_set_warning();
      return;
    }
  }

  if (this->battery_voltage_sensor_ != nullptr) {
    uint16_t raw = 0;
    if (this->read_byte_16(REG_VCELL, &raw)) {
      this->battery_voltage_sensor_->publish_state((raw & VCELL_MASK) * VCELL_VOLTS_PER_LSB);
    } else {
      this->status_set_warning();
      return;
    }
  }
  this->status_clear_warning();
}

void CW2017::dump_config() {
  ESP_LOGCONFIG(TAG, "CW2017:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
}

}  // namespace esphome::cw2017
