#include <cstdint>

#include "stcc4.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::stcc4 {

static const char *const TAG = "stcc4";

// I2C Commands
static constexpr uint16_t STCC4_CMD_START_CONTINUOUS_MEASUREMENT = 0x218b;
static constexpr uint16_t STCC4_CMD_STOP_CONTINUOUS_MEASUREMENT = 0x3f86;
static constexpr uint16_t STCC4_CMD_MEASURE_SINGLE_SHOT = 0x219d;
static constexpr uint16_t STCC4_CMD_READ_MEASUREMENT = 0xec05;
static constexpr uint16_t STCC4_CMD_GET_PRODUCT_ID = 0x365b;
static constexpr uint16_t STCC4_CMD_SET_RHT_COMPENSATION = 0xe000;
static constexpr uint16_t STCC4_CMD_SET_PRESSURE_COMPENSATION = 0xe016;

// Exit sleep is an 8-bit command (single byte 0x00), not 16-bit
static constexpr uint8_t STCC4_CMD_EXIT_SLEEP_MODE = 0x00;

static constexpr uint32_t STCC4_PRODUCT_ID = 0x0901018a;

// Timeout for determining when the device is ready for use, in milliseconds.
// While waiting for the previous measurement to finish, the device will NACK all I2C requests
// and it can take up to 1200 ms for the operation to complete according to the datasheet.
static constexpr uint32_t READY_TIMEOUT_MS = 1200;

// Poll interval for determining when the device is ready for use, in milliseconds.
static constexpr uint32_t READY_POLL_INTERVAL_MS = 100;

// Convert units the the device's representation according to the datasheet.
constexpr uint16_t temperature_in_c_to_ticks(float temperature_in_c) {
  return uint16_t((std::clamp(temperature_in_c, -45.f, 130.f) + 45.f) * 65535.f / 175.f);
}

constexpr float temperature_in_ticks_to_c(uint16_t temperature_in_ticks) {
  return temperature_in_ticks * 175.f / 65535.f - 45.f;
}

constexpr uint16_t humidity_in_percent_to_ticks(float humidity_in_percent) {
  return uint16_t((std::clamp(humidity_in_percent, 0.f, 100.f) + 6.f) * 65535.f / 125.f);
}

constexpr float humidity_in_ticks_to_percent(uint16_t humidity_in_ticks) {
  return humidity_in_ticks * 125.f / 65535.f - 6.f;
}

constexpr uint16_t pressure_in_hpa_to_pa_2(float pressure_in_hpa) {
  return uint16_t(std::clamp(pressure_in_hpa, 400.f, 1100.f) * 50.f);
}

constexpr float pressure_in_pa_2_to_hpa(uint16_t pressure_in_pa_2) { return pressure_in_pa_2 / 50.f; }

void STCC4Component::setup() {
  this->stop_poller();  // not ready yet

  // Wait 100 ms after power up before attempting to communicate with the sensor
  this->set_timeout(100, [this]() {
    // Send exit sleep mode command (8-bit, NACK expected), wait 5 ms to exit sleep
    this->write_command(STCC4_CMD_EXIT_SLEEP_MODE);
    this->set_timeout(5, [this]() { this->poll_until_ready_for_setup_or_timeout_(millis()); });
  });
}

void STCC4Component::poll_until_ready_for_setup_or_timeout_(uint32_t start_time) {
  // Stop continuous measurements in case they were previously running
  // The device may NACK this request if it is not ready to communicate yet
  if (this->write_command(STCC4_CMD_STOP_CONTINUOUS_MEASUREMENT)) {
    // Read product ID to verify communication (6 words: 2 for product_id + 4 for serial)
    // The device may NACK this request if it is not ready to communicate yet
    uint16_t raw_product_id[6];
    if (this->get_register(STCC4_CMD_GET_PRODUCT_ID, raw_product_id, 6, 1)) {
      uint32_t product_id = (uint32_t(raw_product_id[0]) << 16) | raw_product_id[1];
      uint64_t serial_number = (uint64_t(raw_product_id[2]) << 48) | (uint64_t(raw_product_id[3]) << 32) |
                               (uint64_t(raw_product_id[4]) << 16) | raw_product_id[5];
      ESP_LOGD(TAG, "Product ID: 0x%08" PRIX32 ", Serial: 0x%016" PRIX64, product_id, serial_number);
      if (product_id != STCC4_PRODUCT_ID) {
        ESP_LOGE(TAG, "Unsupported product ID");
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }

      // Set static ambient pressure compensation if configured
      if (this->ambient_pressure_in_pa_2_ != 0) {
        if (!this->write_ambient_pressure_compensation_(this->ambient_pressure_in_pa_2_)) {
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
      }

      // Apply the dynamic compensation sources' current values, if configured
      if (this->temperature_source_ != nullptr && this->humidity_source_ != nullptr) {
        this->update_rht_compensation_from_source_();
      }
      if (this->ambient_pressure_source_ != nullptr) {
        this->update_ambient_pressure_compensation_from_source_();
      }

      if (this->measurement_mode_ == MeasurementMode::SINGLE_SHOT) {
        this->start_poller();
        this->finish_setup_();
        return;
      }

      // Start continuous measurement
      if (!this->write_command(STCC4_CMD_START_CONTINUOUS_MEASUREMENT)) {
        ESP_LOGE(TAG, "Failed to start continuous measurement");
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
      this->schedule_continuous_update_(false);
      this->finish_setup_();
      return;
    }
  }

  if (millis() - start_time < READY_TIMEOUT_MS) {
    ESP_LOGVV(TAG, "Retry sync");
    this->set_timeout(READY_POLL_INTERVAL_MS,
                      [this, start_time]() { this->poll_until_ready_for_setup_or_timeout_(start_time); });
    return;
  }

  ESP_LOGE(TAG, "Failed to stop continuous measurements and read product ID");
  this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
}

void STCC4Component::finish_setup_() {
  this->ready_ = true;
  // Follow the sources only once measuring started, so a failed setup stops writing to the device
  if (this->temperature_source_ != nullptr && this->humidity_source_ != nullptr) {
    this->temperature_source_->add_on_state_callback([this](float) { this->update_rht_compensation_from_source_(); });
    this->humidity_source_->add_on_state_callback([this](float) { this->update_rht_compensation_from_source_(); });
  }
  if (this->ambient_pressure_source_ != nullptr) {
    this->ambient_pressure_source_->add_on_state_callback(
        [this](float) { this->update_ambient_pressure_compensation_from_source_(); });
  }
}

void STCC4Component::dump_config() {
  ESP_LOGCONFIG(TAG, "STCC4:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Measurement mode: %s",
                this->measurement_mode_ == MeasurementMode::CONTINUOUS ? LOG_STR_LITERAL("Continuous (1s)")
                                                                       : LOG_STR_LITERAL("Single shot"));
  if (this->ambient_pressure_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dynamic ambient pressure compensation using '%s'",
                  this->ambient_pressure_source_->get_name().c_str());
  } else if (this->ambient_pressure_in_pa_2_ != 0) {
    ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %f hPa",
                  pressure_in_pa_2_to_hpa(this->ambient_pressure_in_pa_2_));
  }
  if (this->temperature_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Temperature compensation using '%s'", this->temperature_source_->get_name().c_str());
  }
  if (this->humidity_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Humidity compensation using '%s'", this->humidity_source_->get_name().c_str());
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void STCC4Component::update() {
  if (!this->ready_ || this->measurement_mode_ != MeasurementMode::SINGLE_SHOT)
    return;

  // Perform single-shot measurement, wait 500 ms for the measurement to be ready
  if (!this->write_command(STCC4_CMD_MEASURE_SINGLE_SHOT)) {
    ESP_LOGW(TAG, "Failed to start single shot measurement");
    this->status_set_warning();
    return;
  }
  this->set_timeout(500, [this]() {
    if (this->read_measurement_(0)) {
      this->status_clear_warning();
    } else {
      ESP_LOGW(TAG, "Failed to read measurement data");
      this->status_set_warning();
    }
  });
}

void STCC4Component::schedule_continuous_update_(bool retry_for_clock_drift) {
  // In continuous measurement mode, the STCC4 produces a sample every 1000 ms according to its
  // internal clock.  The datasheet recommends retrying 150 ms after a failed read to compensate
  // for clock drift between the host and the device.
  this->set_timeout(retry_for_clock_drift ? 150 : 1000, [this, retry_for_clock_drift]() {
    if (this->read_measurement_(retry_for_clock_drift ? 0 : sensirion_common::SENSIRION_OPTION_READ_MAY_NACK)) {
      this->status_clear_warning();
      this->schedule_continuous_update_(false);
    } else if (!retry_for_clock_drift) {
      this->schedule_continuous_update_(true);
    } else {
      this->status_set_warning();
      this->schedule_continuous_update_(false);
    }
  });
}

bool STCC4Component::read_measurement_(uint8_t sensirion_options) {
  // Read measurement data: 4 words (CO2, temperature, humidity, status)
  uint16_t raw_data[4];
  if (!this->get_register_(STCC4_CMD_READ_MEASUREMENT, ADDR_16_BIT, raw_data, 4, 1, sensirion_options)) {
    return false;
  }

  // CO2 value is in ppm as int16 (ignore negative values during warm-up)
  const int16_t co2_raw = int16_t(raw_data[0]);
  if (this->co2_sensor_ != nullptr && co2_raw >= 0) {
    this->co2_sensor_->publish_state(co2_raw);
  }

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature_in_ticks_to_c(raw_data[1]));
  }

  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(humidity_in_ticks_to_percent(raw_data[2]));
  }
  return true;
}

void STCC4Component::update_rht_compensation_from_source_() {
  const float temperature_in_c = this->temperature_source_->state;
  const float humidity_in_percent = this->humidity_source_->state;
  if (std::isnan(temperature_in_c) || std::isnan(humidity_in_percent))
    return;

  const uint16_t temperature_in_ticks = temperature_in_c_to_ticks(temperature_in_c);
  const uint16_t humidity_in_ticks = humidity_in_percent_to_ticks(humidity_in_percent);
  if ((this->temperature_in_ticks_ != temperature_in_ticks || this->humidity_in_ticks_ != humidity_in_ticks) &&
      this->write_rht_compensation_(temperature_in_ticks, humidity_in_ticks)) {
    this->temperature_in_ticks_ = temperature_in_ticks;
    this->humidity_in_ticks_ = humidity_in_ticks;
  }
}

void STCC4Component::set_ambient_pressure_compensation(float pressure_in_hpa) {
  this->ambient_pressure_in_pa_2_ = pressure_in_hpa_to_pa_2(pressure_in_hpa);
}

void STCC4Component::update_ambient_pressure_compensation_from_source_() {
  const float pressure_in_hpa = this->ambient_pressure_source_->state;
  if (std::isnan(pressure_in_hpa))
    return;

  if (pressure_in_hpa < 100.f || pressure_in_hpa > 10000.f) {
    // Some pressure sensors report values in Pa instead of hPa and there's no way to check at compile time.
    // Warn if the value seems far outside of the expected range.
    if (!this->ambient_pressure_unit_warning_logged_) {
      this->ambient_pressure_unit_warning_logged_ = true;
      ESP_LOGW(TAG, "Ambient pressure compensation sensor might have incompatible units: got %f hPa", pressure_in_hpa);
    }
    return;  // skip this update
  } else {
    this->ambient_pressure_unit_warning_logged_ = false;
  }

  const uint16_t ambient_pressure_in_pa_2 = pressure_in_hpa_to_pa_2(pressure_in_hpa);
  if (this->ambient_pressure_in_pa_2_ != ambient_pressure_in_pa_2 &&
      this->write_ambient_pressure_compensation_(ambient_pressure_in_pa_2)) {
    this->ambient_pressure_in_pa_2_ = ambient_pressure_in_pa_2;
  }
}

bool STCC4Component::write_rht_compensation_(uint16_t temperature_in_ticks, uint16_t humidity_in_ticks) {
  ESP_LOGVV(TAG, "Set RHT compensation: %f °C, %f %%RH", temperature_in_ticks_to_c(temperature_in_ticks),
            humidity_in_ticks_to_percent(humidity_in_ticks));
  const uint16_t data[2] = {temperature_in_ticks, humidity_in_ticks};
  if (!this->write_command(STCC4_CMD_SET_RHT_COMPENSATION, data, 2)) {
    ESP_LOGE(TAG, "Failed to set RHT compensation");
    return false;
  }
  return true;
}

bool STCC4Component::write_ambient_pressure_compensation_(uint16_t pressure_in_pa_2) {
  ESP_LOGVV(TAG, "Set pressure compensation: %f hPa", pressure_in_pa_2_to_hpa(pressure_in_pa_2));
  if (!this->write_command(STCC4_CMD_SET_PRESSURE_COMPENSATION, pressure_in_pa_2)) {
    ESP_LOGE(TAG, "Failed to set ambient pressure compensation");
    return false;
  }
  return true;
}

}  // namespace esphome::stcc4
