#include "bme680.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bme680 {

static const char *const TAG = "bme680";

void BME680Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BME680...");

  // Initialize BME680
  int8_t rslt = BME680_OK;
  
  dev.intf_ptr = &wire;
  dev.intf = BME680_I2C_INTF;
  dev.read = [](uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) -> int8_t {
    TwoWire *wire = static_cast<TwoWire *>(intf_ptr);
    wire->beginTransmission(BME680_I2C_ADDR);
    wire->write(reg_addr);
    if (wire->endTransmission() != 0) {
      return BME680_E_COM_FAIL;
    }
    wire->requestFrom(BME680_I2C_ADDR, len);
    for (uint32_t i = 0; i < len; i++) {
      reg_data[i] = wire->read();
    }
    return BME680_OK;
  };
  
  dev.write = [](const uint8_t *reg_data, uint32_t len, void *intf_ptr) -> int8_t {
    TwoWire *wire = static_cast<TwoWire *>(intf_ptr);
    wire->beginTransmission(BME680_I2C_ADDR);
    wire->write(reg_data, len);
    return wire->endTransmission() == 0 ? BME680_OK : BME680_E_COM_FAIL;
  };
  
  dev.delay_ms = [](uint32_t ms, void *intf_ptr) -> void {
    delay(ms);
  };

  rslt = bme680_init(&dev);
  if (rslt != BME680_OK) {
    ESP_LOGCONFIG(TAG, "BME680 init failed: %d", rslt);
    this->mark_failed();
    return;
  }

  // Configure sensor
  uint8_t set_required_settings;
  
  // Set the temperature, pressure and humidity settings
  dev.conf.tph_sett.os_hum = BME680_OS_2X;
  dev.conf.tph_sett.os_pres = BME680_OS_4X;
  dev.conf.tph_sett.os_temp = BME680_OS_8X;
  dev.conf.tph_sett.filter = BME680_FILTER_SIZE_3;

  // Set the remaining settings
  dev.conf.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS;
  dev.conf.gas_sett.heatr_temp = 300;  // Temperature in degrees Celsius
  dev.conf.gas_sett.heatr_dur = 100;   // Duration in milliseconds

  dev.power.mode = BME680_FORCED_MODE;

  /* Set the required sensor settings and then check if they were applied */
  set_required_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL | BME680_GAS_MEAS_SEL;

  rslt = bme680_set_sensor_settings(set_required_settings, &dev);
  if (rslt != BME680_OK) {
    ESP_LOGCONFIG(TAG, "BME680 settings failed: %d", rslt);
    this->mark_failed();
    return;
  }

  // Set the power mode
  rslt = bme680_set_power_mode(&dev, BME680_FORCED_MODE);
  if (rslt != BME680_OK) {
    ESP_LOGCONFIG(TAG, "BME680 power mode failed: %d", rslt);
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "BME680 setup complete!");
}

void BME680Component::update() {
  int8_t rslt = BME680_OK;
  struct bme680_field_data data;

  // Trigger the measurement
  rslt = bme680_set_sensor_mode(&dev, BME680_FORCED_MODE);
  if (rslt != BME680_OK) {
    ESP_LOGW(TAG, "BME680 measurement trigger failed: %d", rslt);
    return;
  }

  // Wait for the measurement to complete
  uint16_t delay_ms = bme680_get_profile_duration(&dev);
  delay(delay_ms + 10);

  // Get the results
  rslt = bme680_get_sensor_data(BME680_ALL, &data, &dev);
  if (rslt != BME680_OK) {
    ESP_LOGW(TAG, "BME680 data read failed: %d", rslt);
    return;
  }

  // Publish temperature
  if (this->temperature_sensor_ != nullptr && data.status & BME680_NEW_DATA_MSK) {
    float temperature = data.temperature / 100.0f;  // Convert to Celsius
    this->temperature_sensor_->publish_state(temperature);
    ESP_LOGD(TAG, "Temperature: %.2f °C", temperature);
  }

  // Publish pressure
  if (this->pressure_sensor_ != nullptr && data.status & BME680_NEW_DATA_MSK) {
    float pressure = data.pressure / 100.0f;  // Convert to hPa
    this->pressure_sensor_->publish_state(pressure);
    ESP_LOGD(TAG, "Pressure: %.2f hPa", pressure);
  }

  // Publish humidity
  if (this->humidity_sensor_ != nullptr && data.status & BME680_NEW_DATA_MSK) {
    float humidity = data.humidity / 1000.0f;  // Convert to %
    this->humidity_sensor_->publish_state(humidity);
    ESP_LOGD(TAG, "Humidity: %.2f %%", humidity);
  }

  // Publish gas resistance
  if (this->gas_resistance_sensor_ != nullptr && data.gas_resistance > 0) {
    float gas_resistance = data.gas_resistance / 1000.0f;  // Convert to kOhm
    this->gas_resistance_sensor_->publish_state(gas_resistance);
    ESP_LOGD(TAG, "Gas Resistance: %.2f kOhm", gas_resistance);
  }
}

void BME680Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BME680:");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Gas Resistance", this->gas_resistance_sensor_);
}

}  // namespace bme680
}  // namespace esphome
