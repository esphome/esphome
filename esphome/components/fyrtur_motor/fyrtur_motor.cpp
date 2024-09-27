#include "fyrtur_motor.h"
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace fyrtur_motor {

static const char *const TAG = "fyrtur_motor";

static const size_t DEFAULT_ATTEMPTS_COUNT = 1;
static const size_t DEFAULT_LOOP_TIMEOUT_MS = 20;

static const size_t CHECKSUM_SIZE = 1;
static const size_t HEADER_SIZE = 3;

static const size_t STATUS_RESPONSE_SIZE = 4;

void FyrturMotorComponent::setup() { get_status(); }

// void FyrturMotorComponent::dump_config() {}

void FyrturMotorComponent::update() { get_status(); }

void FyrturMotorComponent::loop(void) {
  // while (this->available() > 0) {
  //   if (this->read_byte(&data)) {
  //     this->process_rx_(data);
  //   }
  // }
}

float FyrturMotorComponent::get_setup_priority() const { return setup_priority::DATA; }

static uint8_t last_upper_setpoint = 0;
static uint8_t last_lower_setpoint = 0;

void FyrturMotorComponent::update_setpoints() {
  if (!this->upper_setpoint_number_->has_state() || !this->lower_setpoint_number_->has_state()) {
    return;
  }

  uint8_t upper_setpoint = static_cast<uint8_t>(this->upper_setpoint_number_->state);
  uint8_t lower_setpoint = static_cast<uint8_t>(this->lower_setpoint_number_->state);

  if ((last_upper_setpoint != upper_setpoint) && (upper_setpoint >= lower_setpoint)) {
    if (upper_setpoint == 100) {
      upper_setpoint = 99;
      lower_setpoint = 100;
      this->upper_setpoint_number_->publish_state(upper_setpoint);
      this->lower_setpoint_number_->publish_state(lower_setpoint);
    } else {
      lower_setpoint = upper_setpoint + 1;
      this->lower_setpoint_number_->publish_state(lower_setpoint);
    }
  } else if ((last_lower_setpoint != lower_setpoint) && (lower_setpoint <= upper_setpoint)) {
    if (lower_setpoint == 0) {
      upper_setpoint = 0;
      lower_setpoint = 1;
      this->upper_setpoint_number_->publish_state(upper_setpoint);
      this->lower_setpoint_number_->publish_state(lower_setpoint);
    } else {
      lower_setpoint = upper_setpoint - 1;
      this->lower_setpoint_number_->publish_state(lower_setpoint);
    }
  }

  last_upper_setpoint = upper_setpoint;
  last_lower_setpoint = lower_setpoint;
}

void FyrturMotorComponent::open_close(bool state) {
  if (!this->upper_setpoint_number_->has_state() || !this->lower_setpoint_number_->has_state()) {
    return;
  }

  uint8_t position = 0;

  if (state) {
    position = static_cast<uint8_t>(this->upper_setpoint_number_->state);
  } else {
    position = static_cast<uint8_t>(this->lower_setpoint_number_->state);
  }

  set_position(position);
}

// Moves motor to position 0-100
void FyrturMotorComponent::set_position(uint8_t position) {
  const std::vector<uint8_t> data = {0xdd, position};
  send_command(data);
}

// Moves the blinds up until stop command is received or until motor stalls
void FyrturMotorComponent::move_up() {
  const std::vector<uint8_t> data = {0x0a, 0xdd};
  send_command(data);
}

// Moves the blinds down until stop command is received or until max length is reached
void FyrturMotorComponent::move_down() {
  const std::vector<uint8_t> data = {0x0a, 0xee};
  send_command(data);
}

// Moves the blinds up 6mm
void FyrturMotorComponent::move_up_6() {
  const std::vector<uint8_t> data = {0x0a, 0x0d};
  send_command(data);
}

// Moves the blinds down 6mm
void FyrturMotorComponent::move_down_6() {
  const std::vector<uint8_t> data = {0x0a, 0x0e};
  send_command(data);
}

// Moves the blinds up 30mm
void FyrturMotorComponent::move_up_30() {
  const std::vector<uint8_t> data = {0xfa, 0xd1};
  send_command(data);
}

// Moves the blinds down 30mm
void FyrturMotorComponent::move_down_30() {
  const std::vector<uint8_t> data = {0xfa, 0xd2};
  send_command(data);
}

// Moves the blinds up 2mm
void FyrturMotorComponent::move_up_2() {
  const std::vector<uint8_t> data = {0xfa, 0xd3};
  send_command(data);
}

// Moves the blinds down 2mm
void FyrturMotorComponent::move_down_2() {
  const std::vector<uint8_t> data = {0xfa, 0xd4};
  send_command(data);
}

// Sets max length
void FyrturMotorComponent::set_max_length() {
  const std::vector<uint8_t> data = {0xfa, 0xee};
  send_command(data);
}

// Sets full length
void FyrturMotorComponent::set_full_length() {
  const std::vector<uint8_t> data = {0xfa, 0xcc};
  send_command(data);
}

// Resets max length
void FyrturMotorComponent::reset_max_length() {
  const std::vector<uint8_t> data = {0xfa, 0x00};
  send_command(data);
}

// Sets rolling direction
void FyrturMotorComponent::toggle_roll_direction() {
  const std::vector<uint8_t> data = {0xd6, 0x00};
  send_command(data);
}

// Stops the movement
void FyrturMotorComponent::stop() {
  const std::vector<uint8_t> data = {0x0a, 0xcc};
  send_command(data);
}

// Gets battery level, battery voltage, speed and position
void FyrturMotorComponent::get_status() {
  const std::vector<uint8_t> data = {0xcc, 0xcc};
  const std::vector<uint8_t> response =
      send_command_and_get_response(data, STATUS_RESPONSE_SIZE, DEFAULT_ATTEMPTS_COUNT);

  if (response.size() == 0) {
    ESP_LOGE(TAG, "Failed to get response");
    return;
  }

  float battery_level = response[0];
  float battery_voltage = (float) response[1] / 30.65;
  uint8_t speed = response[2];
  uint8_t position = response[3];

#ifdef USE_SENSOR
  if (this->battery_level_sensor_) {
    this->battery_level_sensor_->publish_state(battery_level);
  }
  if (this->voltage_sensor_) {
    this->voltage_sensor_->publish_state(battery_voltage);
  }
  if (this->speed_sensor_) {
    this->speed_sensor_->publish_state(speed);
  }
  if (this->position_sensor_) {
    this->position_sensor_->publish_state((float) position);
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->moving_binary_sensor_) {
    this->moving_binary_sensor_->publish_state(speed > 0);
  }
  if (this->fully_closed_binary_sensor_) {
    this->fully_closed_binary_sensor_->publish_state(position == 100);
  }
  if (this->fully_open_binary_sensor_) {
    this->fully_open_binary_sensor_->publish_state(position == 0);
  }
  if (this->partialy_open_binary_sensor_) {
    this->partialy_open_binary_sensor_->publish_state((position != 0) && (position != 100));
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (this->status_text_sensor_ != nullptr) {
    if (speed == 0) {
      switch (position) {
        case 0:
          this->status_text_sensor_->publish_state("Fully open");
          break;
        case 100:
          this->status_text_sensor_->publish_state("Fully closed");
          break;
        default:
          this->status_text_sensor_->publish_state("Partialy open");
          break;
      }
    } else {
      this->status_text_sensor_->publish_state("Moving");
    }
  }
#endif
}

uint8_t FyrturMotorComponent::get_checksum(const std::vector<uint8_t> &data) {
  uint8_t checksum = 0;
  for (const uint8_t &byte : data) {
    checksum ^= byte;
  }
  return checksum;
}

void FyrturMotorComponent::send_command(const std::vector<uint8_t> &data) {
  const std::vector<uint8_t> command_header = {0x00, 0xff, 0x9a};
  this->write_array(command_header);
  this->write_array(data);
  uint8_t checksum = get_checksum(data);
  this->write_byte(get_checksum(data));
}

std::vector<uint8_t> FyrturMotorComponent::get_response(size_t amount_of_data_bytes_to_get) {
  std::vector<uint8_t> response;
  while ((this->available() > 0) && (response.size() != (amount_of_data_bytes_to_get + HEADER_SIZE + CHECKSUM_SIZE))) {
    uint8_t byte;
    this->read_byte(&byte);
    response.push_back(byte);
  }

  if (response.size() != amount_of_data_bytes_to_get + HEADER_SIZE + CHECKSUM_SIZE) {
    ESP_LOGE(TAG, "Expected to get %lu bytes, got %lu", amount_of_data_bytes_to_get + HEADER_SIZE + CHECKSUM_SIZE,
             response.size());
    const std::vector<uint8_t> empty_response;
    return empty_response;
  }

  std::vector<uint8_t> data = {response.begin() + 3, response.end() - 1};
  if (get_checksum(data) != response.back()) {
    ESP_LOGE(TAG, "Bad checksum");
    const std::vector<uint8_t> empty_response;
    return empty_response;
  }

  return data;
}

std::vector<uint8_t> FyrturMotorComponent::send_command_and_get_response(const std::vector<uint8_t> &data,
                                                                         size_t amount_of_data_bytes_to_get,
                                                                         size_t attempts) {
  for (size_t attempt = attempts; attempt > 0; attempt--) {
    send_command(data);

    const std::vector<uint8_t> response = get_response(amount_of_data_bytes_to_get);
    if (response.size() == amount_of_data_bytes_to_get) {
      return response;
    }
  }

  const std::vector<uint8_t> empty_response;
  return empty_response;
}

}  // namespace fyrtur_motor
}  // namespace esphome
