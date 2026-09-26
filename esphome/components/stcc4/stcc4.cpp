#include "stcc4.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::stcc4 {

static const char *const TAG = "stcc4";

// I2C Commands
static const uint16_t STCC4_CMD_START_CONTINUOUS_MEASUREMENT = 0x218b;
static const uint16_t STCC4_CMD_STOP_CONTINUOUS_MEASUREMENT = 0x3f86;
static const uint16_t STCC4_CMD_MEASURE_SINGLE_SHOT = 0x219d;
static const uint16_t STCC4_CMD_READ_MEASUREMENT = 0xec05;
static const uint16_t STCC4_CMD_GET_PRODUCT_ID = 0x365b;
static const uint16_t STCC4_CMD_SET_RHT_COMPENSATION = 0xe000;
static const uint16_t STCC4_CMD_SET_PRESSURE_COMPENSATION = 0xe016;

// Exit sleep is an 8-bit command (single byte 0x00), not 16-bit
static const uint8_t STCC4_CMD_EXIT_SLEEP_MODE = 0x00;

static const uint32_t STCC4_PRODUCT_ID = 0x0901018a;

void STCC4Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STCC4...");
  this->stop_poller();  // not ready yet

  // Wait 100 ms after power up before attempting to communicate with the sensor
  this->set_timeout(100, [this]() {
    // Send exit sleep mode command (8-bit, NACK expected), wait 5 ms to exit sleep
    this->write_command(STCC4_CMD_EXIT_SLEEP_MODE);
    this->set_timeout(5, [this]() {
      // Stop continuous measurement in case the device is not idle, wait 1200 ms for completion
      if (!this->write_command(STCC4_CMD_STOP_CONTINUOUS_MEASUREMENT)) {
        ESP_LOGE(TAG, "Failed to stop continuous measurements");
        this->mark_failed();
        return;
      }
      this->set_timeout(1200, [this]() {
        // Read product ID to verify communication (6 words: 2 for product_id + 4 for serial)
        uint16_t raw_product_id[6];
        if (!this->get_register(STCC4_CMD_GET_PRODUCT_ID, raw_product_id, 6, 1)) {
          ESP_LOGE(TAG, "Failed to read product ID");
          this->mark_failed();
          return;
        }
        uint32_t product_id = (uint32_t(raw_product_id[0]) << 16) | raw_product_id[1];
        uint64_t serial_number = (uint64_t(raw_product_id[2]) << 48) | (uint64_t(raw_product_id[3]) << 32) |
                                 (uint64_t(raw_product_id[4]) << 16) | raw_product_id[5];
        ESP_LOGD(TAG, "Product ID: 0x%08" PRIX32 ", Serial: 0x%016" PRIX64, product_id, serial_number);
        if (product_id != STCC4_PRODUCT_ID) {
          ESP_LOGE(TAG, "Unsupported product ID");
          this->mark_failed();
          return;
        }

        // Set static ambient pressure compensation if configured
        if (this->ambient_pressure_in_pa_2_ != 0) {
          if (!this->write_ambient_pressure_compensation_()) {
            this->mark_failed();
            return;
          }
        }

        // Setup dynamic compensation sources if configured
        if (this->temperature_source_ != nullptr && this->humidity_source_ != nullptr) {
          this->temperature_source_->add_on_state_callback(
              [this](float) { this->update_rht_compensation_from_source_(); });
          this->humidity_source_->add_on_state_callback(
              [this](float) { this->update_rht_compensation_from_source_(); });
          this->update_rht_compensation_from_source_();
        }
        if (this->ambient_pressure_source_ != nullptr) {
          this->ambient_pressure_source_->add_on_state_callback(
              [this](float) { this->update_ambient_pressure_compensation_from_source_(); });
          this->update_ambient_pressure_compensation_from_source_();
        }

        if (this->measurement_mode_ == MeasurementMode::SINGLE_SHOT) {
          this->finish_setup_();
          return;
        }

        // Start continuous measurement, wait 1200 ms for first measurement to be available
        if (!this->write_command(STCC4_CMD_START_CONTINUOUS_MEASUREMENT)) {
          ESP_LOGE(TAG, "Failed to start continuous measurement");
          this->mark_failed();
          return;
        }
        this->set_timeout(1200, [this]() { this->finish_setup_(); });
      });
    });
  });
}

void STCC4Component::finish_setup_() {
  this->ready_ = true;
  this->start_poller();
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
    ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %f hPa", this->ambient_pressure_in_pa_2_ / 50.f);
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
  if (!this->ready_)
    return;

  if (this->measurement_mode_ == MeasurementMode::CONTINUOUS) {
    this->read_measurement_();
    return;
  }

  // Perform single-shot measurement, wait 500 ms for the measurement to be ready
  if (!this->write_command(STCC4_CMD_MEASURE_SINGLE_SHOT)) {
    ESP_LOGW(TAG, "Failed to start single shot measurement");
    this->status_set_warning();
    return;
  }
  this->set_timeout(500, [this]() { this->read_measurement_(); });
}

void STCC4Component::read_measurement_() {
  // Read measurement data: 4 words (CO2, temperature, humidity, status)
  uint16_t raw_data[4];
  if (!this->get_register(STCC4_CMD_READ_MEASUREMENT, raw_data, 4, 1)) {
    ESP_LOGW(TAG, "Failed to read measurement data");
    this->status_set_warning();
    return;
  }

  // CO2 value is in ppm as int16 (ignore negative values during warm-up)
  int16_t co2_raw = int16_t(raw_data[0]);
  if (this->co2_sensor_ != nullptr && co2_raw >= 0) {
    this->co2_sensor_->publish_state(co2_raw);
  }

  if (this->temperature_sensor_ != nullptr) {
    float temperature = (175.0f * raw_data[1]) / 65535.0f - 45.0f;
    this->temperature_sensor_->publish_state(temperature);
  }

  if (this->humidity_sensor_ != nullptr) {
    float humidity = (125.0f * raw_data[2]) / 65535.0f - 6.0f;
    this->humidity_sensor_->publish_state(humidity);
  }

  this->status_clear_warning();
}

void STCC4Component::update_rht_compensation_from_source_() {
  const float temperature = this->temperature_source_->state;
  const float humidity = this->humidity_source_->state;
  if (std::isnan(temperature) || std::isnan(humidity))
    return;

  const uint16_t temperature_ticks = uint16_t(((temperature + 45.0f) / 175.0f) * 65535.0f);
  const uint16_t humidity_ticks = uint16_t(((humidity + 6.0f) / 125.0f) * 65535.0f);
  if (this->temperature_ticks_ != temperature_ticks || this->humidity_ticks_ != humidity_ticks) {
    this->temperature_ticks_ = temperature_ticks;
    this->humidity_ticks_ = humidity_ticks;
    ESP_LOGVV(TAG, "Set RHT compensation: %f °C, %f %%RH", temperature, humidity);
    const uint16_t data[2] = {temperature_ticks, humidity_ticks};
    if (!this->write_command(STCC4_CMD_SET_RHT_COMPENSATION, data, 2)) {
      ESP_LOGE(TAG, "Failed to set RHT compensation");
    }
  }
}

void STCC4Component::set_ambient_pressure_compensation(float pressure_in_hpa) {
  this->ambient_pressure_in_pa_2_ = uint16_t(std::clamp(pressure_in_hpa, 700.f, 1100.f) * 50);
}

void STCC4Component::update_ambient_pressure_compensation_from_source_() {
  const float pressure = this->ambient_pressure_source_->state;
  if (std::isnan(pressure))
    return;

  if (pressure < 100.f || pressure > 10000.f) {
    // Some pressure sensors report values in Pa instead of hPa and there's no way to check at compile time.
    // Warn if the value seems far outside of the expected range.
    ESP_LOGW(TAG, "Ambient pressure compensation sensor might have incompatible units: got %f hPa", pressure);
  }

  uint16_t old_ambient_pressure_in_pa_2 = this->ambient_pressure_in_pa_2_;
  this->set_ambient_pressure_compensation(pressure);
  if (this->ambient_pressure_in_pa_2_ != old_ambient_pressure_in_pa_2) {
    write_ambient_pressure_compensation_();
  }
}

bool STCC4Component::write_ambient_pressure_compensation_() {
  ESP_LOGVV(TAG, "Set pressure compensation: %f", this->ambient_pressure_in_pa_2_ / 50.f);
  if (!this->write_command(STCC4_CMD_SET_PRESSURE_COMPENSATION, this->ambient_pressure_in_pa_2_)) {
    ESP_LOGE(TAG, "Failed to set ambient pressure compensation");
    return false;
  }
  return true;
}

}  // namespace esphome::stcc4
