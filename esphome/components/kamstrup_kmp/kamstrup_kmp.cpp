#include "kamstrup_kmp.h"

#include "esphome/core/log.h"

namespace esphome {
namespace kamstrup_kmp {

static const char *const TAG = "kamstrup_kmp";

void KamstrupKMPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "kamstrup_kmp:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Heat Energy", this->heat_energy_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Temperature 1", this->temp1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temp2_sensor_);
  LOG_SENSOR("  ", "Temperature Difference", this->temp_diff_sensor_);
  LOG_SENSOR("  ", "Flow", this->flow_sensor_);
  LOG_SENSOR("  ", "Volume", this->volume_sensor_);

  for (size_t i = 0; i < this->custom_sensors_.size(); i++) {
    LOG_SENSOR("  ", "Custom Sensor", this->custom_sensors_[i]);
    ESP_LOGCONFIG(TAG, "    Command: 0x%04X", this->custom_commands_[i]);
  }

  this->check_uart_settings(1200, 2, uart::UART_CONFIG_PARITY_NONE, 8);
}

void KamstrupKMPComponent::update() {
  if (this->heat_energy_sensor_ != nullptr) {
    this->command_queue_.push(CMD_HEAT_ENERGY);
  }

  if (this->power_sensor_ != nullptr) {
    this->command_queue_.push(CMD_POWER);
  }

  if (this->temp1_sensor_ != nullptr) {
    this->command_queue_.push(CMD_TEMP1);
  }

  if (this->temp2_sensor_ != nullptr) {
    this->command_queue_.push(CMD_TEMP2);
  }

  if (this->temp_diff_sensor_ != nullptr) {
    this->command_queue_.push(CMD_TEMP_DIFF);
  }

  if (this->flow_sensor_ != nullptr) {
    this->command_queue_.push(CMD_FLOW);
  }

  if (this->volume_sensor_ != nullptr) {
    this->command_queue_.push(CMD_VOLUME);
  }

  for (uint16_t custom_command : this->custom_commands_) {
    this->command_queue_.push(custom_command);
  }
}

void KamstrupKMPComponent::loop() {
  switch (this->state_) {
    case STATE_IDLE:
      if (!this->command_queue_.empty()) {
        this->current_command_ = this->command_queue_.front();
        this->clear_uart_rx_buffer_();
        this->send_command_(current_command_);
        this->command_queue_.pop();
        this->state_ = STATE_READING;
        this->last_read_time_ = millis();
        this->rx_buffer_.clear();
      }
      break;

    case STATE_SEARCHING:
    case STATE_READING:
    case STATE_READ_ESCAPE:
      // Handle RX non-blockingly
      while (this->available()) {
        uint8_t data = this->read();
        this->last_read_time_ = millis();  // Reset timeout on new data

        // 0x40 always restarts the current frame.
        if (data == 0x40) {
          this->rx_buffer_.clear();
          this->state_ = STATE_READING;
          continue;
        }
        if (this->state_ == STATE_SEARCHING) {
          continue;
        }

        if (this->state_ == STATE_READ_ESCAPE) {
          this->rx_buffer_.push_back(data ^ 0xFF);
          this->state_ = STATE_READING;
        } else if (data == 0x1B) {  // ESCape
          this->state_ = STATE_READ_ESCAPE;
        } else if (data == 0x0D) {  // End of Message
          this->parse_command_message_(current_command_, rx_buffer_.data(), rx_buffer_.size());
          this->state_ = STATE_IDLE;
          return;
        } else if (this->rx_buffer_.size() > 32) {
          ESP_LOGW(TAG, "Buffer overflow, resetting.");
          this->rx_buffer_.clear();
          this->state_ = STATE_SEARCHING;
        } else {
          this->rx_buffer_.push_back(data);
        }
      }

      // Handle Timeout
      if (millis() - this->last_read_time_ > 250) {
        ESP_LOGE(TAG, "Timeout waiting for Kamstrup response");
        this->state_ = STATE_IDLE;
      }
      break;
  }
}

void KamstrupKMPComponent::send_command_(uint16_t command) {
  // 1. Prepare the raw frame for CRC calculation (5 bytes msg + 2 bytes for CRC padding)
  // Total 7 bytes. We initialize the last two to 0 for the CCITT augmentation.
  uint8_t frame[7];

  frame[0] = 0x3F;
  frame[1] = 0x10;
  frame[2] = 0x01;
  frame[3] = command >> 8;
  frame[4] = command & 0xFF;
  frame[5] = 0x00;
  frame[6] = 0x00;

  // 2. Calculate CRC over the frame
  uint16_t crc = crc16_ccitt(frame, sizeof(frame));
  frame[5] = crc >> 8;
  frame[6] = crc & 0xFF;

  // 3. Prepare actual TX message
  // Max size: 1 (PREFIX) + 7 (payload+crc) * 2 (worst-case stuffing) + 1 (EOM) = 16 bytes.
  uint8_t tx_msg[16];
  int tx_msg_len = 1;

  tx_msg[0] = 0x80;  // PREFIX

  // 4. Single-pass stuffing: move from 'frame' to 'tx_msg'
  for (unsigned char ch : frame) {
    if (ch == 0x06 || ch == 0x0d || ch == 0x1b || ch == 0x40 || ch == 0x80) {
      tx_msg[tx_msg_len++] = 0x1b;
      tx_msg[tx_msg_len++] = ch ^ 0xff;
    } else {
      tx_msg[tx_msg_len++] = ch;
    }
  }

  tx_msg[tx_msg_len++] = 0x0D;  // EOM

  this->write_array(tx_msg, tx_msg_len);
}

void KamstrupKMPComponent::clear_uart_rx_buffer_() {
  uint8_t tmp;
  while (this->available()) {
    this->read_byte(&tmp);
  }
}

void KamstrupKMPComponent::parse_command_message_(uint16_t command, const uint8_t *msg, int msg_len) {
  // Validate CRC
  if (crc16_ccitt(msg, msg_len)) {
    ESP_LOGE(TAG, "Received invalid message (CRC mismatch)");
    return;
  }

  // Validate the message
  if (msg_len < 8) {
    ESP_LOGE(TAG, "Received invalid message (message too small)");
    return;
  }

  if (msg[0] != 0x3F || msg[1] != 0x10) {
    ESP_LOGE(TAG, "Received invalid message (invalid header received 0x%02X%02X, expected 0x3F10)", msg[0], msg[1]);
    return;
  }

  uint16_t recv_command = msg[2] << 8 | msg[3];
  if (recv_command != command) {
    ESP_LOGE(TAG, "Received invalid message (invalid unexpected command received 0x%04X, expected 0x%04X)",
             recv_command, command);
    return;
  }

  uint8_t unit_idx = msg[4];
  uint8_t mantissa_range = msg[5];

  if (mantissa_range > 4) {
    ESP_LOGE(TAG, "Received invalid message (mantissa size too large %d, expected 4)", mantissa_range);
    return;
  }

  // Calculate exponent
  float exponent = msg[6] & 0x3F;
  if (msg[6] & 0x40) {
    exponent = -exponent;
  }
  exponent = powf(10, exponent);
  if (msg[6] & 0x80) {
    exponent = -exponent;
  }

  // Calculate mantissa
  uint32_t mantissa = 0;
  for (int i = 0; i < mantissa_range; i++) {
    mantissa <<= 8;
    mantissa |= msg[i + 7];
  }

  // Calculate the actual value
  float value = mantissa * exponent;

  // Set sensor value
  this->set_sensor_value_(command, value, unit_idx);
}

void KamstrupKMPComponent::set_sensor_value_(uint16_t command, float value, uint8_t unit_idx) {
  const char *unit = UNITS[unit_idx];

  // Standard sensors
  if (command == CMD_HEAT_ENERGY && this->heat_energy_sensor_ != nullptr) {
    this->heat_energy_sensor_->publish_state(value);
  } else if (command == CMD_POWER && this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(value);
  } else if (command == CMD_TEMP1 && this->temp1_sensor_ != nullptr) {
    this->temp1_sensor_->publish_state(value);
  } else if (command == CMD_TEMP2 && this->temp2_sensor_ != nullptr) {
    this->temp2_sensor_->publish_state(value);
  } else if (command == CMD_TEMP_DIFF && this->temp_diff_sensor_ != nullptr) {
    this->temp_diff_sensor_->publish_state(value);
  } else if (command == CMD_FLOW && this->flow_sensor_ != nullptr) {
    this->flow_sensor_->publish_state(value);
  } else if (command == CMD_VOLUME && this->volume_sensor_ != nullptr) {
    this->volume_sensor_->publish_state(value);
  }

  // Custom sensors
  for (size_t i = 0; i < this->custom_commands_.size(); i++) {
    if (command == this->custom_commands_[i]) {
      this->custom_sensors_[i]->publish_state(value);
    }
  }

  ESP_LOGD(TAG, "Received value for command 0x%04X: %.3f [%s]", command, value, unit);
}

uint16_t crc16_ccitt(const uint8_t *buffer, int len) {
  uint32_t poly = 0x1021;
  uint32_t reg = 0x00;
  for (int i = 0; i < len; i++) {
    int mask = 0x80;
    while (mask > 0) {
      reg <<= 1;
      if (buffer[i] & mask) {
        reg |= 1;
      }
      mask >>= 1;
      if (reg & 0x10000) {
        reg &= 0xffff;
        reg ^= poly;
      }
    }
  }
  return (uint16_t) reg;
}

}  // namespace kamstrup_kmp
}  // namespace esphome
