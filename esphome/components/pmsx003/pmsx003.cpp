#include <array>
#include "pmsx003.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pmsx003 {

static const char *const TAG = "pmsx003";

void PMSX003Component::set_pm_1_0_std_sensor(sensor::Sensor *pm_1_0_std_sensor) {
  pm_1_0_std_sensor_ = pm_1_0_std_sensor;
}
void PMSX003Component::set_pm_2_5_std_sensor(sensor::Sensor *pm_2_5_std_sensor) {
  pm_2_5_std_sensor_ = pm_2_5_std_sensor;
}
void PMSX003Component::set_pm_10_0_std_sensor(sensor::Sensor *pm_10_0_std_sensor) {
  pm_10_0_std_sensor_ = pm_10_0_std_sensor;
}

void PMSX003Component::set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor) { pm_1_0_sensor_ = pm_1_0_sensor; }
void PMSX003Component::set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { pm_2_5_sensor_ = pm_2_5_sensor; }
void PMSX003Component::set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { pm_10_0_sensor_ = pm_10_0_sensor; }

void PMSX003Component::set_pm_particles_03um_sensor(sensor::Sensor *pm_particles_03um_sensor) {
  pm_particles_03um_sensor_ = pm_particles_03um_sensor;
}
void PMSX003Component::set_pm_particles_05um_sensor(sensor::Sensor *pm_particles_05um_sensor) {
  pm_particles_05um_sensor_ = pm_particles_05um_sensor;
}
void PMSX003Component::set_pm_particles_10um_sensor(sensor::Sensor *pm_particles_10um_sensor) {
  pm_particles_10um_sensor_ = pm_particles_10um_sensor;
}
void PMSX003Component::set_pm_particles_25um_sensor(sensor::Sensor *pm_particles_25um_sensor) {
  pm_particles_25um_sensor_ = pm_particles_25um_sensor;
}
void PMSX003Component::set_pm_particles_50um_sensor(sensor::Sensor *pm_particles_50um_sensor) {
  pm_particles_50um_sensor_ = pm_particles_50um_sensor;
}
void PMSX003Component::set_pm_particles_100um_sensor(sensor::Sensor *pm_particles_100um_sensor) {
  pm_particles_100um_sensor_ = pm_particles_100um_sensor;
}

void PMSX003Component::set_temperature_sensor(sensor::Sensor *temperature_sensor) {
  temperature_sensor_ = temperature_sensor;
}
void PMSX003Component::set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
void PMSX003Component::set_formaldehyde_sensor(sensor::Sensor *formaldehyde_sensor) {
  formaldehyde_sensor_ = formaldehyde_sensor;
}

void PMSX003Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PMSX003:");
  LOG_SENSOR("  ", "PM1.0STD", this->pm_1_0_std_sensor_);
  LOG_SENSOR("  ", "PM2.5STD", this->pm_2_5_std_sensor_);
  LOG_SENSOR("  ", "PM10.0STD", this->pm_10_0_std_sensor_);

  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);

  LOG_SENSOR("  ", "PM0.3um", this->pm_particles_03um_sensor_);
  LOG_SENSOR("  ", "PM0.5um", this->pm_particles_05um_sensor_);
  LOG_SENSOR("  ", "PM1.0um", this->pm_particles_10um_sensor_);
  LOG_SENSOR("  ", "PM2.5um", this->pm_particles_25um_sensor_);
  LOG_SENSOR("  ", "PM5.0um", this->pm_particles_50um_sensor_);
  LOG_SENSOR("  ", "PM10.0um", this->pm_particles_100um_sensor_);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  this->check_uart_settings(9600);
}

float PMSX003Component::get_setup_priority() const { return setup_priority::DATA; }

void PMSX003Component::setup() {
  this->is_laser_save_mode_ = this->update_interval_ > this->warmup_interval_;
  ESP_LOGD(TAG, "Laser save mode: %s", this->is_laser_save_mode_ ? "ON" : "OFF");
  if (!this->is_laser_save_mode_) {
    this->set_timeout(1000, [this]() {
      ESP_LOGD(TAG, "Wake up & warm up");
      this->send_command_(PMS_CMD_SLEEP_WAKEUP, 1);
      this->started_at_ = millis();
      this->set_timeout(2000, [this]() {
        ESP_LOGD(TAG, "Passive mode ON");
        this->send_command_(PMS_CMD_PASSIVE_ACTIVE, 0);
      });
    });
  }
}

void PMSX003Component::update() {
  if (this->is_laser_save_mode_) {
    // wakeup
    ESP_LOGD(TAG, "Wake up & warm up");
    this->send_command_(PMS_CMD_SLEEP_WAKEUP, 1);
    // and set mode to passive, as it resets to active on each wakeup
    this->set_timeout(2000, [this]() {
      ESP_LOGD(TAG, "Passive mode ON");
      this->send_command_(PMS_CMD_PASSIVE_ACTIVE, 0);
    });
    this->set_timeout(this->warmup_interval_, [this]() { this->take_measurement_(); });
  } else {
    // ensure the first read is after sensor has warmed up
    if (!this->warmed_up_ && this->started_at_.has_value() && millis() - *this->started_at_ > this->warmup_interval_)
      this->warmed_up_ = true;
    if (this->warmed_up_) {
      this->take_measurement_();
    } else {
      if (this->started_at_.has_value()) {
        ESP_LOGD(TAG, "Remaining warm up time: %u s",
                 int((this->warmup_interval_ - (millis() - *this->started_at_)) / 1000));
      }
    }
  }
}

void PMSX003Component::take_measurement_(uint32_t timeout) {
  while (this->available())
    this->read_byte(this->data_);

  this->send_command_(PMS_CMD_REQUEST_READ, 0);
  this->data_index_ = 0;
  auto start = millis();

  while (true) {
    if (millis() - start > timeout) {
      ESP_LOGE(TAG, "Reading data from PMSx003 timed out");
      break;
    }

    if (this->available() != 0) {
      this->read_byte(&this->data_[this->data_index_]);
      auto check = this->check_byte_();
      if (!check.has_value()) {
        // finished
        this->parse_data_();

        // Spin down the sensor again if we aren't going to need it until more time has
        // passed than it takes to stabilise
        if (this->is_laser_save_mode_) {
          this->set_timeout(1000, [this]() {
            ESP_LOGD(TAG, "Sleep");
            this->send_command_(PMS_CMD_SLEEP_WAKEUP, 0);
          });
        }

        this->status_clear_warning();
        break;
      } else if (*check) {
        // next byte
        this->data_index_++;
      } else {
        // corrupted data
        break;
      }
    }
  }
}

optional<bool> PMSX003Component::check_byte_() {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  if (index == 0)
    return byte == 0x42;

  if (index == 1)
    return byte == 0x4D;

  if (index == 2)
    return true;

  uint16_t payload_length = this->get_16_bit_uint_(2);
  if (index == 3) {
    bool length_matches = false;
    switch (this->type_) {
      case PMSX003_TYPE_X003:
        length_matches = payload_length == 28 || payload_length == 20;
        break;
      case PMSX003_TYPE_5003T:
      case PMSX003_TYPE_5003S:
        length_matches = payload_length == 28;
        break;
      case PMSX003_TYPE_5003ST:
        length_matches = payload_length == 36;
        break;
    }

    if (!length_matches) {
      ESP_LOGW(TAG, "PMSX003 length %u doesn't match. Are you using the correct PMSX003 type?", payload_length);
      return false;
    }
    return true;
  }

  // start (16bit) + length (16bit) + DATA (payload_length-2 bytes) + checksum (16bit)
  uint8_t total_size = 4 + payload_length;

  if (index < total_size - 1)
    return true;

  // checksum is without checksum bytes
  uint16_t checksum = 0;
  for (uint8_t i = 0; i < total_size - 2; i++)
    checksum += this->data_[i];

  uint16_t check = this->get_16_bit_uint_(total_size - 2);
  if (checksum != check) {
    ESP_LOGW(TAG, "PMSX003 checksum mismatch! 0x%02X!=0x%02X", checksum, check);
    return false;
  }

  return {};
}

void PMSX003Component::send_command_(uint8_t cmd, uint16_t data) {
  std::array<uint8_t, 7> buf;
  uint8_t idx = 0;
  buf[idx++] = 0x42;
  buf[idx++] = 0x4D;
  buf[idx++] = cmd;
  buf[idx++] = (data >> 8) & 0xFF;
  buf[idx++] = (data >> 0) & 0xFF;
  int sum = 0;
  for (int i = 0; i < idx; i++)
    sum += buf[i];
  buf[idx++] = (sum >> 8) & 0xFF;
  buf[idx++] = (sum >> 0) & 0xFF;
  this->write_array(buf);
}

void PMSX003Component::parse_data_() {
  switch (this->type_) {
    case PMSX003_TYPE_5003ST: {
      float temperature = this->get_16_bit_uint_(30) / 10.0f;
      float humidity = this->get_16_bit_uint_(32) / 10.0f;

      ESP_LOGD(TAG, "Got Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);

      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(temperature);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(humidity);
      // The rest of the PMS5003ST matches the PMS5003S, continue on
    }
    case PMSX003_TYPE_5003S: {
      uint16_t formaldehyde = this->get_16_bit_uint_(28);

      ESP_LOGD(TAG, "Got Formaldehyde: %u µg/m^3", formaldehyde);

      if (this->formaldehyde_sensor_ != nullptr)
        this->formaldehyde_sensor_->publish_state(formaldehyde);
      // The rest of the PMS5003S matches the PMS5003, continue on
    }
    case PMSX003_TYPE_X003: {
      uint16_t pm_1_0_std_concentration = this->get_16_bit_uint_(4);
      uint16_t pm_2_5_std_concentration = this->get_16_bit_uint_(6);
      uint16_t pm_10_0_std_concentration = this->get_16_bit_uint_(8);

      uint16_t pm_1_0_concentration = this->get_16_bit_uint_(10);
      uint16_t pm_2_5_concentration = this->get_16_bit_uint_(12);
      uint16_t pm_10_0_concentration = this->get_16_bit_uint_(14);

      uint16_t pm_particles_03um = this->get_16_bit_uint_(16);
      uint16_t pm_particles_05um = this->get_16_bit_uint_(18);
      uint16_t pm_particles_10um = this->get_16_bit_uint_(20);
      uint16_t pm_particles_25um = this->get_16_bit_uint_(22);
      uint16_t pm_particles_50um = this->get_16_bit_uint_(24);
      uint16_t pm_particles_100um = this->get_16_bit_uint_(26);

      ESP_LOGD(TAG,
               "Got PM1.0 Concentration: %u µg/m^3, PM2.5 Concentration %u µg/m^3, PM10.0 Concentration: %u µg/m^3",
               pm_1_0_concentration, pm_2_5_concentration, pm_10_0_concentration);

      if (this->pm_1_0_std_sensor_ != nullptr)
        this->pm_1_0_std_sensor_->publish_state(pm_1_0_std_concentration);
      if (this->pm_2_5_std_sensor_ != nullptr)
        this->pm_2_5_std_sensor_->publish_state(pm_2_5_std_concentration);
      if (this->pm_10_0_std_sensor_ != nullptr)
        this->pm_10_0_std_sensor_->publish_state(pm_10_0_std_concentration);

      if (this->pm_1_0_sensor_ != nullptr)
        this->pm_1_0_sensor_->publish_state(pm_1_0_concentration);
      if (this->pm_2_5_sensor_ != nullptr)
        this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
      if (this->pm_10_0_sensor_ != nullptr)
        this->pm_10_0_sensor_->publish_state(pm_10_0_concentration);

      if (this->pm_particles_03um_sensor_ != nullptr)
        this->pm_particles_03um_sensor_->publish_state(pm_particles_03um);
      if (this->pm_particles_05um_sensor_ != nullptr)
        this->pm_particles_05um_sensor_->publish_state(pm_particles_05um);
      if (this->pm_particles_10um_sensor_ != nullptr)
        this->pm_particles_10um_sensor_->publish_state(pm_particles_10um);
      if (this->pm_particles_25um_sensor_ != nullptr)
        this->pm_particles_25um_sensor_->publish_state(pm_particles_25um);
      if (this->pm_particles_50um_sensor_ != nullptr)
        this->pm_particles_50um_sensor_->publish_state(pm_particles_50um);
      if (this->pm_particles_100um_sensor_ != nullptr)
        this->pm_particles_100um_sensor_->publish_state(pm_particles_100um);
      break;
    }
    case PMSX003_TYPE_5003T: {
      uint16_t pm_2_5_concentration = this->get_16_bit_uint_(12);
      float temperature = this->get_16_bit_uint_(24) / 10.0f;
      float humidity = this->get_16_bit_uint_(26) / 10.0f;
      ESP_LOGD(TAG, "Got PM2.5 Concentration: %u µg/m^3, Temperature: %.1f°C, Humidity: %.1f%%", pm_2_5_concentration,
               temperature, humidity);
      if (this->pm_2_5_sensor_ != nullptr)
        this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(temperature);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(humidity);
      break;
    }
  }
}

uint16_t PMSX003Component::get_16_bit_uint_(uint8_t start_index) {
  return (uint16_t(this->data_[start_index]) << 8) | uint16_t(this->data_[start_index + 1]);
}

}  // namespace pmsx003
}  // namespace esphome
