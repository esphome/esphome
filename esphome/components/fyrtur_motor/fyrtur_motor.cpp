#include "fyrtur_motor.hpp"
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace fyrtur_motor {

static const char *const TAG = "fyrtur_motor";

static const size_t DEFAULT_ATTEMPTS_COUNT = 3;
static const size_t DEFAULT_LOOP_TIMEOUT_MS = 1000;

static const size_t CHECKSUM_SIZE = 1;
static const size_t HEADER_SIZE = 3;

static const size_t STATUS_RESPONSE_SIZE = 4;

void FyrturMotorComponent::setup(void) { get_status(); }

void FyrturMotorComponent::dump_config(void) {
  ESP_LOGCONFIG(TAG, "Fyrtur motor:");
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "MoveUpButton", this->move_up_button_);
  LOG_BUTTON("  ", "StopButton", this->stop_button_);
  LOG_BUTTON("  ", "MoveDownButton", this->move_down_button_);
  LOG_BUTTON("  ", "GetStatusButton", this->get_status_button_);
#endif
}

void FyrturMotorComponent::update(void) { get_status(); }

// void FyrturMotorComponent::loop(void) {
//   // probably not needed
// }

float FyrturMotorComponent::get_setup_priority(void) const { return setup_priority::DATA; }

// Moves motor to position 0-100
void FyrturMotorComponent::set_position(uint8_t position) {}

// Moves the blinds up until stop command is received or until motor stalls
void FyrturMotorComponent::move_up(void) {
  const std::vector<uint8_t> data = {0x0a, 0xdd};
  send_command(data);
}

// Moves the blinds down until stop command is received or until max length is reached
void FyrturMotorComponent::move_down(void) {
  const std::vector<uint8_t> data = {0x0a, 0xee};
  send_command(data);
}

// Moves the blinds up 6mm
void FyrturMotorComponent::move_up_6(void) {
  const std::vector<uint8_t> data = {0x0a, 0x0d};
  send_command(data);
}

// Moves the blinds down 6mm
void FyrturMotorComponent::move_down_6(void) {
  const std::vector<uint8_t> data = {0x0a, 0x0e};
  send_command(data);
}

// Moves the blinds up 30mm
void FyrturMotorComponent::move_up_30(void) {
  const std::vector<uint8_t> data = {0xfa, 0xd1};
  send_command(data);
}

// Moves the blinds down 30mm
void FyrturMotorComponent::move_down_30(void) {
  const std::vector<uint8_t> data = {0xfa, 0xd2};
  send_command(data);
}

// Moves the blinds up 2mm
void FyrturMotorComponent::move_up_2(void) {
  const std::vector<uint8_t> data = {0xfa, 0xd3};
  send_command(data);
}

// Moves the blinds down 2mm
void FyrturMotorComponent::move_down_2(void) {
  const std::vector<uint8_t> data = {0xfa, 0xd4};
  send_command(data);
}

// Sets max length
void FyrturMotorComponent::set_max_length(void) {
  const std::vector<uint8_t> data = {0xfa, 0xee};
  send_command(data);
}

// Sets full length
void FyrturMotorComponent::set_full_length(void) {
  const std::vector<uint8_t> data = {0xfa, 0xcc};
  send_command(data);
}

// Resets max length
void FyrturMotorComponent::reset_max_length(void) {
  const std::vector<uint8_t> data = {0xfa, 0x00};
  send_command(data);
}

// Sets rolling direction
void FyrturMotorComponent::set_rolling_direction(RollingDirection_t direction) {
  const std::vector<uint8_t> data = {0xd5, 0x00};
  send_command(data);
  if (direction == REVERSE) {
    const std::vector<uint8_t> data = {0xd6, 0x00};
    send_command(data);
  }
}

// Stops the movement
void FyrturMotorComponent::stop(void) {
  const std::vector<uint8_t> data = {0x0a, 0xcc};
  send_command(data);
}

// Gets battery level, battery voltage, speed and position
void FyrturMotorComponent::get_status(void) {
  const std::vector<uint8_t> data = {0xcc, 0xcc};
  const std::vector<uint8_t> response =
      send_command_and_get_response(data, STATUS_RESPONSE_SIZE, DEFAULT_ATTEMPTS_COUNT);

  if (response.size() == 0) {
    ESP_LOGE(TAG, "Failed to get response");
  }

  float battery_level = response[0];
  float battery_voltage = response[1] / 30.0;
  uint8_t speed = response[2];
  uint8_t position = response[3];

  ESP_LOGI(TAG, "Got status response:");
  ESP_LOGI("  ", "Battery level: %.0f /%", battery_level);
  ESP_LOGI("  ", "Battery voltage: %.1f V", battery_voltage);
  ESP_LOGI("  ", "Speed: %u RPM", speed);
  ESP_LOGI("  ", "Position: %u /%", position);

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
  this->write_byte(get_checksum(data));
  this->flush();
}

const std::vector<uint8_t> FyrturMotorComponent::get_response(size_t amount_of_data_bytes_to_get) {
  std::vector<uint8_t> response;
  size_t loop_timeout = millis() + DEFAULT_LOOP_TIMEOUT_MS;
  while ((loop_timeout <= millis()) &&
         (response.size() != (amount_of_data_bytes_to_get + HEADER_SIZE + CHECKSUM_SIZE))) {
    if (this->available() > 0) {
      uint8_t byte;
      this->read_byte(&byte);
      response.push_back(byte);
    }
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

const std::vector<uint8_t> FyrturMotorComponent::send_command_and_get_response(const std::vector<uint8_t> &data,
                                                                               size_t amount_of_data_bytes_to_get,
                                                                               size_t attempts) {
  for (size_t attempt = attempts; attempt > 0; attempt--) {
    // Clear the RX buffer before sending anything
    while (this->available() > 0) {
      this->read();
    }

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
