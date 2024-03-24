#include "kamstrup_kmp.h"

#include "esphome/core/log.h"

namespace esphome {
namespace kamstrup_kmp {

static const char *const TAG = "kamstrup_kmp";

void KamstrupKMPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "kamstrup_kmp:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with Kamstrup meter failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Heat Energy", this->heat_energy_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Temperature 1", this->temp1_sensor_);
  LOG_SENSOR("  ", "Temperature 2", this->temp2_sensor_);
  LOG_SENSOR("  ", "Temperature Difference", this->temp_diff_sensor_);
  LOG_SENSOR("  ", "Flow", this->flow_sensor_);
  LOG_SENSOR("  ", "Volume", this->volume_sensor_);

  for (int i = 0; i < this->custom_sensors_.size(); i++) {
    LOG_SENSOR("  ", "Custom Sensor", this->custom_sensors_[i]);
    ESP_LOGCONFIG(TAG, "    Command: 0x%04X", this->custom_commands_[i]);
  }

  this->check_uart_settings(1200, 2, uart::UART_CONFIG_PARITY_NONE, 8);
}

float KamstrupKMPComponent::get_setup_priority() const { return setup_priority::DATA; }

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
  if (!this->command_queue_.empty()) {
    uint16_t command = this->command_queue_.front();
    this->send_command_(command);
    this->command_queue_.pop();
  }
}

void KamstrupKMPComponent::send_command_(uint16_t command) {
  uint32_t msg_len = 5;
  uint8_t msg[msg_len];

  msg[0] = 0x3F;
  msg[1] = 0x10;
  msg[2] = 0x01;
  msg[3] = command >> 8;
  msg[4] = command & 0xFF;

  this->clear_uart_rx_buffer_();
  this->send_message_(msg, msg_len);
  this->read_command_(command);
}

void KamstrupKMPComponent::send_message_(const uint8_t *msg, int msg_len) {
  int buffer_len = msg_len + 2;
  uint8_t buffer[buffer_len];

  // Prepare the basic message and appand CRC
  for (int i = 0; i < msg_len; i++) {
    buffer[i] = msg[i];
  }

  buffer[buffer_len - 2] = 0;
  buffer[buffer_len - 1] = 0;

  uint16_t crc = crc16_ccitt(buffer, buffer_len);
  buffer[buffer_len - 2] = crc >> 8;
  buffer[buffer_len - 1] = crc & 0xFF;

  // Prepare actual TX message
  uint8_t tx_msg[20];
  int tx_msg_len = 1;
  tx_msg[0] = 0x80;  // prefix

  for (int i = 0; i < buffer_len; i++) {
    if (buffer[i] == 0x06 || buffer[i] == 0x0d || buffer[i] == 0x1b || buffer[i] == 0x40 || buffer[i] == 0x80) {
      tx_msg[tx_msg_len++] = 0x1b;
      tx_msg[tx_msg_len++] = buffer[i] ^ 0xff;
    } else {
      tx_msg[tx_msg_len++] = buffer[i];
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

void KamstrupKMPComponent::read_command_(uint16_t command) {
  uint8_t buffer[20] = {0};
  int buffer_len = 0;
  int data;
  int timeout = 250;  // ms

  // Read the data from the UART
  while (timeout > 0) {
    if (this->available()) {
      data = this->read();
      if (data > -1) {
        if (data == 0x40) {  // start of message
          buffer_len = 0;
        }
        buffer[buffer_len++] = (uint8_t) data;
        if (data == 0x0D) {
          break;
        }
      } else {
        ESP_LOGE(TAG, "Error while reading from UART");
      }
    } else {
      delay(1);
      timeout--;
    }
  }

  if (timeout == 0 || buffer_len == 0) {
    ESP_LOGE(TAG, "Request timed out");
    return;
  }

  // Validate message (prefix and suffix)
  if (buffer[0] != 0x40) {
    ESP_LOGE(TAG, "Received invalid message (prefix mismatch received 0x%02X, expected 0x40)", buffer[0]);
    return;
  }

  if (buffer[buffer_len - 1] != 0x0D) {
    ESP_LOGE(TAG, "Received invalid message (EOM mismatch received 0x%02X, expected 0x0D)", buffer[buffer_len - 1]);
    return;
  }

  // Decode
  uint8_t msg[20] = {0};
  int msg_len = 0;
  for (int i = 1; i < buffer_len - 1; i++) {
    if (buffer[i] == 0x1B) {
      msg[msg_len++] = buffer[i + 1] ^ 0xFF;
      i++;
    } else {
      msg[msg_len++] = buffer[i];
    }
  }

  // Validate CRC
  if (crc16_ccitt(msg, msg_len)) {
    ESP_LOGE(TAG, "Received invalid message (CRC mismatch)");
    return;
  }

  // All seems good. Now parse the message
  this->parse_command_message_(command, msg, msg_len);
}

void KamstrupKMPComponent::parse_command_message_(uint16_t command, const uint8_t *msg, int msg_len) {
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
  for (int i = 0; i < this->custom_commands_.size(); i++) {
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
