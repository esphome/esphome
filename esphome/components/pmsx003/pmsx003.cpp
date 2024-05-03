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

void PMSX003Component::loop() {
  const uint32_t now = millis();

  // If we update less often than it takes the device to stabilise, spin the fan down
  // rather than running it constantly. It does take some time to stabilise, so we
  // need to keep track of what state we're in.
  if (this->update_interval_ > PMS_STABILISING_MS) {
    if (this->initialised_ == 0) {
      this->send_command_(PMS_CMD_AUTO_MANUAL, 0);
      this->send_command_(PMS_CMD_ON_STANDBY, 1);
      this->initialised_ = 1;
    }
    switch (this->state_) {
      case PMSX003_STATE_IDLE:
        // Power on the sensor now so it'll be ready when we hit the update time
        if (now - this->last_update_ < (this->update_interval_ - PMS_STABILISING_MS))
          return;

        this->state_ = PMSX003_STATE_STABILISING;
        this->send_command_(PMS_CMD_ON_STANDBY, 1);
        this->fan_on_time_ = now;
        return;
      case PMSX003_STATE_STABILISING:
        // wait for the sensor to be stable
        if (now - this->fan_on_time_ < PMS_STABILISING_MS)
          return;
        // consume any command responses that are in the serial buffer
        while (this->available())
          this->read_byte(&this->data_[0]);
        // Trigger a new read
        this->send_command_(PMS_CMD_TRIG_MANUAL, 0);
        this->state_ = PMSX003_STATE_WAITING;
        break;
      case PMSX003_STATE_WAITING:
        // Just go ahead and read stuff
        break;
    }
  } else if (now - this->last_update_ < this->update_interval_) {
    // Otherwise just leave the sensor powered up and come back when we hit the update
    // time
    return;
  }

  if (now - this->last_transmission_ >= 500) {
    // last transmission too long ago. Reset RX index.
    this->data_index_ = 0;
  }

  if (this->available() == 0)
    return;

  this->last_transmission_ = now;
  while (this->available() != 0) {
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
      // finished
      this->parse_data_();
      this->data_index_ = 0;
      this->last_update_ = now;
    } else if (!*check) {
      // wrong data
      this->data_index_ = 0;
    } else {
      // next byte
      this->data_index_++;
    }
  }
}
float PMSX003Component::get_setup_priority() const { return setup_priority::DATA; }
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
  this->data_index_ = 0;
  this->data_[data_index_++] = 0x42;
  this->data_[data_index_++] = 0x4D;
  this->data_[data_index_++] = cmd;
  this->data_[data_index_++] = (data >> 8) & 0xFF;
  this->data_[data_index_++] = (data >> 0) & 0xFF;
  int sum = 0;
  for (int i = 0; i < data_index_; i++) {
    sum += this->data_[i];
  }
  this->data_[data_index_++] = (sum >> 8) & 0xFF;
  this->data_[data_index_++] = (sum >> 0) & 0xFF;
  for (int i = 0; i < data_index_; i++) {
    this->write_byte(this->data_[i]);
  }
  this->data_index_ = 0;
}

void PMSX003Component::parse_data_() {
  switch (this->type_) {
    case PMSX003_TYPE_5003ST: {
      float temperature = (int16_t) this->get_16_bit_uint_(30) / 10.0f;
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
      // Note the pm particles 50um & 100um are not returned,
      // as PMS5003T uses those data values for temperature and humidity.

      float temperature = (int16_t) this->get_16_bit_uint_(24) / 10.0f;
      float humidity = this->get_16_bit_uint_(26) / 10.0f;

      ESP_LOGD(TAG,
               "Got PM1.0 Concentration: %u µg/m^3, PM2.5 Concentration %u µg/m^3, PM10.0 Concentration: %u µg/m^3, "
               "Temperature: %.1f°C, Humidity: %.1f%%",
               pm_1_0_concentration, pm_2_5_concentration, pm_10_0_concentration, temperature, humidity);

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

      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(temperature);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(humidity);
      break;
    }
  }

  // Spin down the sensor again if we aren't going to need it until more time has
  // passed than it takes to stabilise
  if (this->update_interval_ > PMS_STABILISING_MS) {
    this->send_command_(PMS_CMD_ON_STANDBY, 0);
    this->state_ = PMSX003_STATE_IDLE;
  }

  this->status_clear_warning();
}
uint16_t PMSX003Component::get_16_bit_uint_(uint8_t start_index) {
  return (uint16_t(this->data_[start_index]) << 8) | uint16_t(this->data_[start_index + 1]);
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

}  // namespace pmsx003
}  // namespace esphome
