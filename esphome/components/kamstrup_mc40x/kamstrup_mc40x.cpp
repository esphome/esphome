#include "kamstrup_mc40x.h"

#include "esphome/core/log.h"

namespace esphome {
namespace kamstrup_mc40x {

static const char *const TAG = "KamstrupMC40x";

void KamstrupMC40xComponent::setup() {}

void KamstrupMC40xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "kamstrup_mc40x:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CSE7761 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(1200, 2, uart::UART_CONFIG_PARITY_NONE, 8);
}

float KamstrupMC40xComponent::get_setup_priority() const { return setup_priority::DATA; }

void KamstrupMC40xComponent::update() {
  if (this->heat_energy_sensor_ != nullptr) {
    this->send_command(CMD_HEAT_ENERGY);
  }

  if (this->power_sensor_ != nullptr) {
    this->send_command(CMD_POWER);
  }

  if (this->temp1_sensor_ != nullptr) {
    this->send_command(CMD_TEMP1);
  }

  if (this->temp2_sensor_ != nullptr) {
    this->send_command(CMD_TEMP2);
  }

  if (this->temp_diff_sensor_ != nullptr) {
    this->send_command(CMD_TEMP_DIFF);
  }

  if (this->flow_sensor_ != nullptr) {
    this->send_command(CMD_FLOW);
  }

  if (this->volume_sensor_ != nullptr) {
    this->send_command(CMD_VOLUME);
  }
}

void KamstrupMC40xComponent::send_command(uint16_t command) {
  uint32_t msg_len = 5;
  uint8_t msg[msg_len];

  msg[0] = 0x3F;
  msg[1] = 0x10;
  msg[2] = 0x01;
  msg[3] = command >> 8;
  msg[4] = command & 0xFF;

  this->clear_uart_rx_buffer();
  this->send_message(msg, msg_len);
  this->read_command(command);
}

void KamstrupMC40xComponent::send_message(const uint8_t *msg, int msg_len) {
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

void KamstrupMC40xComponent::clear_uart_rx_buffer() {
  uint8_t tmp;
  while (this->available()) {
    this->read_byte(&tmp);
  }
}

void KamstrupMC40xComponent::read_command(uint16_t command) {
  uint8_t buffer[20] = {0};
  int buffer_len = 0;
  uint8_t data;
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
  this->parse_command_message(command, msg, msg_len);
}

void KamstrupMC40xComponent::parse_command_message(uint16_t command, const uint8_t *msg, int msg_len) {
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
  this->set_sensor_value(command, value, unit_idx);
}

void KamstrupMC40xComponent::set_sensor_value(uint16_t command, float value, uint8_t unit_idx) {
  const char *unit = UNITS[unit_idx];

  switch (command) {
    case CMD_HEAT_ENERGY:
      this->heat_energy_sensor_->set_unit_of_measurement(unit);
      this->heat_energy_sensor_->publish_state(value);
      break;

    case CMD_POWER:
      this->power_sensor_->set_unit_of_measurement(unit);
      this->power_sensor_->publish_state(value);
      break;

    case CMD_TEMP1:
      this->temp1_sensor_->set_unit_of_measurement(unit);
      this->temp1_sensor_->publish_state(value);
      break;

    case CMD_TEMP2:
      this->temp2_sensor_->set_unit_of_measurement(unit);
      this->temp2_sensor_->publish_state(value);
      break;

    case CMD_TEMP_DIFF:
      this->temp_diff_sensor_->set_unit_of_measurement(unit);
      this->temp_diff_sensor_->publish_state(value);
      break;

    case CMD_FLOW:
      this->flow_sensor_->set_unit_of_measurement(unit);
      this->flow_sensor_->publish_state(value);
      break;

    case CMD_VOLUME:
      this->volume_sensor_->set_unit_of_measurement(unit);
      this->volume_sensor_->publish_state(value);
      break;

    default:
      ESP_LOGE(TAG, "Unsupported command %d", command);
      break;
  }
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

}  // namespace kamstrup_mc40x
}  // namespace esphome
