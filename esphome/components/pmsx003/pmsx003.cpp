#include "pmsx003.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace pmsx003 {

static const char *const TAG = "pmsx003";

static const uint8_t START_CHARACTER_1 = 0x42;
static const uint8_t START_CHARACTER_2 = 0x4D;

static const uint16_t PMS_STABILISING_MS = 30000;  // time taken for the sensor to become stable after power on in ms

static const uint16_t PMS_CMD_MEASUREMENT_MODE_PASSIVE =
    0x0000;  // use `PMS_CMD_MANUAL_MEASUREMENT` to trigger a measurement
static const uint16_t PMS_CMD_MEASUREMENT_MODE_ACTIVE = 0x0001;  // automatically perform measurements
static const uint16_t PMS_CMD_SLEEP_MODE_SLEEP = 0x0000;         // go to sleep mode
static const uint16_t PMS_CMD_SLEEP_MODE_WAKEUP = 0x0001;        // wake up from sleep mode

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

  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  this->check_uart_settings(9600);
}

void PMSX003Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // If we update less often than it takes the device to stabilise, spin the fan down
  // rather than running it constantly. It does take some time to stabilise, so we
  // need to keep track of what state we're in.
  if (this->update_interval_ > PMS_STABILISING_MS) {
    if (this->initialised_ == 0) {
      this->send_command_(PMS_CMD_MEASUREMENT_MODE, PMS_CMD_MEASUREMENT_MODE_PASSIVE);
      this->send_command_(PMS_CMD_SLEEP_MODE, PMS_CMD_SLEEP_MODE_WAKEUP);
      this->initialised_ = 1;
    }
    switch (this->state_) {
      case PMSX003_STATE_IDLE:
        // Power on the sensor now so it'll be ready when we hit the update time
        if (now - this->last_update_ < (this->update_interval_ - PMS_STABILISING_MS))
          return;

        this->state_ = PMSX003_STATE_STABILISING;
        this->send_command_(PMS_CMD_SLEEP_MODE, PMS_CMD_SLEEP_MODE_WAKEUP);
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
        this->send_command_(PMS_CMD_MANUAL_MEASUREMENT, 0);
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

optional<bool> PMSX003Component::check_byte_() {
  const uint8_t index = this->data_index_;
  const uint8_t byte = this->data_[index];

  if (index == 0 || index == 1) {
    const uint8_t start_char = index == 0 ? START_CHARACTER_1 : START_CHARACTER_2;
    if (byte == start_char) {
      return true;
    }

    ESP_LOGW(TAG, "Start character %u mismatch: 0x%02X != 0x%02X", index + 1, byte, START_CHARACTER_1);
    return false;
  }

  if (index == 2) {
    return true;
  }

  const uint16_t payload_length = this->get_16_bit_uint_(2);
  if (index == 3) {
    if (this->check_payload_length_(payload_length)) {
      return true;
    } else {
      ESP_LOGW(TAG, "Payload length %u doesn't match. Are you using the correct PMSX003 type?", payload_length);
      return false;
    }
  }

  // start (16bit) + length (16bit) + DATA (payload_length - 16bit) + checksum (16bit)
  const uint16_t total_size = 4 + payload_length;

  if (index < total_size - 1) {
    return true;
  }

  // checksum is without checksum bytes
  uint16_t checksum = 0;
  for (uint16_t i = 0; i < total_size - 2; i++) {
    checksum += this->data_[i];
  }

  const uint16_t check = this->get_16_bit_uint_(total_size - 2);
  if (checksum != check) {
    ESP_LOGW(TAG, "PMSX003 checksum mismatch! 0x%02X != 0x%02X", checksum, check);
    return false;
  }

  return {};
}

bool PMSX003Component::check_payload_length_(uint16_t payload_length) {
  switch (this->type_) {
    case PMSX003_TYPE_X003:
      // The expected payload length is typically 28 bytes.
      // However, a 20-byte payload check was already present in the code.
      // No official documentation was found confirming this.
      // Retaining this check to avoid breaking existing behavior.
      return payload_length == 28 || payload_length == 20;  // 2*13+2
    case PMSX003_TYPE_5003T:
    case PMSX003_TYPE_5003S:
      return payload_length == 28;  // 2*13+2 (Data 13 not set/reserved)
    case PMSX003_TYPE_5003ST:
      return payload_length == 36;  // 2*17+2 (Data 16 not set/reserved)
  }
  return false;
}

void PMSX003Component::send_command_(PMSX0003Command cmd, uint16_t data) {
  uint8_t send_data[7] = {
      START_CHARACTER_1,            // Start Byte 1
      START_CHARACTER_2,            // Start Byte 2
      cmd,                          // Command
      uint8_t((data >> 8) & 0xFF),  // Data 1
      uint8_t((data >> 0) & 0xFF),  // Data 2
      0,                            // Verify Byte 1
      0,                            // Verify Byte 2
  };

  // Calculate checksum
  uint16_t checksum = 0;
  for (uint8_t i = 0; i < 5; i++) {
    checksum += send_data[i];
  }
  send_data[5] = (checksum >> 8) & 0xFF;  // Verify Byte 1
  send_data[6] = (checksum >> 0) & 0xFF;  // Verify Byte 2

  for (auto send_byte : send_data) {
    this->write_byte(send_byte);
  }
}

void PMSX003Component::parse_data_() {
  // Particle Matter
  const uint16_t pm_1_0_std_concentration = this->get_16_bit_uint_(4);
  const uint16_t pm_2_5_std_concentration = this->get_16_bit_uint_(6);
  const uint16_t pm_10_0_std_concentration = this->get_16_bit_uint_(8);

  const uint16_t pm_1_0_concentration = this->get_16_bit_uint_(10);
  const uint16_t pm_2_5_concentration = this->get_16_bit_uint_(12);
  const uint16_t pm_10_0_concentration = this->get_16_bit_uint_(14);

  const uint16_t pm_particles_03um = this->get_16_bit_uint_(16);
  const uint16_t pm_particles_05um = this->get_16_bit_uint_(18);
  const uint16_t pm_particles_10um = this->get_16_bit_uint_(20);
  const uint16_t pm_particles_25um = this->get_16_bit_uint_(22);

  ESP_LOGD(TAG,
           "Got PM1.0 Standard Concentration: %u µg/m³, PM2.5 Standard Concentration %u µg/m³, PM10.0 Standard "
           "Concentration: %u µg/m³, PM1.0 Concentration: %u µg/m³, PM2.5 Concentration %u µg/m³, PM10.0 "
           "Concentration: %u µg/m³",
           pm_1_0_std_concentration, pm_2_5_std_concentration, pm_10_0_std_concentration, pm_1_0_concentration,
           pm_2_5_concentration, pm_10_0_concentration);

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

  if (this->type_ == PMSX003_TYPE_5003T) {
    ESP_LOGD(TAG,
             "Got PM0.3 Particles: %u Count/0.1L, PM0.5 Particles: %u Count/0.1L, PM1.0 Particles: %u Count/0.1L, "
             "PM2.5 Particles %u Count/0.1L",
             pm_particles_03um, pm_particles_05um, pm_particles_10um, pm_particles_25um);
  } else {
    // Note the pm particles 50um & 100um are not returned,
    // as PMS5003T uses those data values for temperature and humidity.
    const uint16_t pm_particles_50um = this->get_16_bit_uint_(24);
    const uint16_t pm_particles_100um = this->get_16_bit_uint_(26);

    ESP_LOGD(TAG,
             "Got PM0.3 Particles: %u Count/0.1L, PM0.5 Particles: %u Count/0.1L, PM1.0 Particles: %u Count/0.1L, "
             "PM2.5 Particles %u Count/0.1L, PM5.0 Particles: %u Count/0.1L, PM10.0 Particles %u Count/0.1L",
             pm_particles_03um, pm_particles_05um, pm_particles_10um, pm_particles_25um, pm_particles_50um,
             pm_particles_100um);

    if (this->pm_particles_50um_sensor_ != nullptr)
      this->pm_particles_50um_sensor_->publish_state(pm_particles_50um);
    if (this->pm_particles_100um_sensor_ != nullptr)
      this->pm_particles_100um_sensor_->publish_state(pm_particles_100um);
  }

  // Formaldehyde
  if (this->type_ == PMSX003_TYPE_5003ST || this->type_ == PMSX003_TYPE_5003S) {
    const uint16_t formaldehyde = this->get_16_bit_uint_(28);

    ESP_LOGD(TAG, "Got Formaldehyde: %u µg/m^3", formaldehyde);

    if (this->formaldehyde_sensor_ != nullptr)
      this->formaldehyde_sensor_->publish_state(formaldehyde);
  }

  // Temperature and Humidity
  if (this->type_ == PMSX003_TYPE_5003ST || this->type_ == PMSX003_TYPE_5003T) {
    const uint8_t temperature_offset = (this->type_ == PMSX003_TYPE_5003T) ? 24 : 30;

    const float temperature = static_cast<int16_t>(this->get_16_bit_uint_(temperature_offset)) / 10.0f;
    const float humidity = this->get_16_bit_uint_(temperature_offset + 2) / 10.0f;

    ESP_LOGD(TAG, "Got Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);

    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
  }

  // Firmware Version and Error Code
  if (this->type_ == PMSX003_TYPE_5003ST) {
    const uint8_t firmware_version = this->data_[36];
    const uint8_t error_code = this->data_[37];

    ESP_LOGD(TAG, "Got Firmware Version: 0x%02X, Error Code: 0x%02X", firmware_version, error_code);
  }

  // Spin down the sensor again if we aren't going to need it until more time has
  // passed than it takes to stabilise
  if (this->update_interval_ > PMS_STABILISING_MS) {
    this->send_command_(PMS_CMD_SLEEP_MODE, PMS_CMD_SLEEP_MODE_SLEEP);
    this->state_ = PMSX003_STATE_IDLE;
  }

  this->status_clear_warning();
}

uint16_t PMSX003Component::get_16_bit_uint_(uint8_t start_index) {
  return (uint16_t(this->data_[start_index]) << 8) | uint16_t(this->data_[start_index + 1]);
}

}  // namespace pmsx003
}  // namespace esphome
