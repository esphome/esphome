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
    this->sendCommand_(CMD_HEAT_ENERGY);
  }

  if (this->power_sensor_ != nullptr) {
    this->sendCommand_(CMD_POWER);
  }

  if (this->temp1_sensor_ != nullptr) {
    this->sendCommand_(CMD_TEMP1);
  }

  if (this->temp2_sensor_ != nullptr) {
    this->sendCommand_(CMD_TEMP2);
  }

  if (this->temp_diff_sensor_ != nullptr) {
    this->sendCommand_(CMD_TEMP_DIFF);
  }

  if (this->flow_sensor_ != nullptr) {
    this->sendCommand_(CMD_FLOW);
  }

  if (this->volume_sensor_ != nullptr) {
    this->sendCommand_(CMD_VOLUME);
  }
}

void KamstrupMC40xComponent::sendCommand_(uint16_t command) {
  uint32_t msgLen = 5;
  uint8_t msg[msgLen];

  msg[0] = 0x3F;
  msg[1] = 0x10;
  msg[2] = 0x01;
  msg[3] = command >> 8;
  msg[4] = command & 0xFF;

  this->clearUartRxBuffer_();
  this->sendMessage_(msg, msgLen);
  this->readCommand_(command);
}

void KamstrupMC40xComponent::sendMessage_(const uint8_t *msg, int msgLen) {
  int bufferLen = msgLen + 2;
  uint8_t buffer[bufferLen];

  // Prepare the basic message and appand CRC
  for (int i = 0; i < msgLen; i++) {
    buffer[i] = msg[i];
  }

  buffer[bufferLen - 2] = 0;
  buffer[bufferLen - 1] = 0;

  uint16_t crc = crc16_(buffer, bufferLen);
  buffer[bufferLen - 2] = crc >> 8;
  buffer[bufferLen - 1] = crc & 0xFF;

  // Prepare actual TX message
  uint8_t txMsg[20];
  int txMsgLen = 1;
  txMsg[0] = 0x80;  // prefix

  for (int i = 0; i < bufferLen; i++) {
    if (buffer[i] == 0x06 || buffer[i] == 0x0d || buffer[i] == 0x1b || buffer[i] == 0x40 || buffer[i] == 0x80) {
      txMsg[txMsgLen++] = 0x1b;
      txMsg[txMsgLen++] = buffer[i] ^ 0xff;
    } else {
      txMsg[txMsgLen++] = buffer[i];
    }
  }

  txMsg[txMsgLen++] = 0x0D;  // EOM

  this->write_array(txMsg, txMsgLen);
}

void KamstrupMC40xComponent::clearUartRxBuffer_() {
  uint8_t tmp;
  while (this->available()) {
    this->read_byte(&tmp);
  }
}

void KamstrupMC40xComponent::readCommand_(uint16_t command) {
  uint8_t buffer[20] = {0};
  int bufferLen = 0;
  uint8_t data;
  int timeout = 250;  // ms

  // Read the data from the UART
  while (timeout > 0) {
    if (this->available()) {
      data = this->read();
      if (data > -1) {
        if (data == 0x40) {  // start of message
          bufferLen = 0;
        }
        buffer[bufferLen++] = (uint8_t) data;
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

  if (timeout == 0 || bufferLen == 0) {
    ESP_LOGE(TAG, "Request timed out");
    return;
  }

  // Validate message (prefix and suffix)
  if (buffer[0] != 0x40) {
    ESP_LOGE(TAG, "Received invalid message (prefix mismatch received 0x%02X, expected 0x40)", buffer[0]);
    return;
  }

  if (buffer[bufferLen - 1] != 0x0D) {
    ESP_LOGE(TAG, "Received invalid message (EOM mismatch received 0x%02X, expected 0x0D)", buffer[bufferLen - 1]);
    return;
  }

  // Decode
  uint8_t msg[20] = {0};
  int msgLen = 0;
  for (int i = 1; i < bufferLen - 1; i++) {
    if (buffer[i] == 0x1B) {
      msg[msgLen++] = buffer[i + 1] ^ 0xFF;
      i++;
    } else {
      msg[msgLen++] = buffer[i];
    }
  }

  // Validate CRC
  if (crc16_(msg, msgLen)) {
    ESP_LOGE(TAG, "Received invalid message (CRC mismatch)");
    return;
  }

  // All seems good. Now parse the message
  this->parseCommandMessage_(command, msg, msgLen);
}

void KamstrupMC40xComponent::parseCommandMessage_(uint16_t command, const uint8_t *msg, int msgLen) {
  // Validate the message
  if (msgLen < 8) {
    ESP_LOGE(TAG, "Received invalid message (message too small)");
    return;
  }

  if (msg[0] != 0x3F || msg[1] != 0x10) {
    ESP_LOGE(TAG, "Received invalid message (invalid header received 0x%02X%02X, expected 0x3F10)", msg[0], msg[1]);
    return;
  }

  uint16_t recvCommand = msg[2] << 8 | msg[3];
  if (recvCommand != command) {
    ESP_LOGE(TAG, "Received invalid message (invalid unexpected command received 0x%04X, expected 0x%04X)", recvCommand,
             command);
    return;
  }

  uint8_t unitIdx = msg[4];
  uint8_t mantissaRange = msg[5];

  if (mantissaRange > 4) {
    ESP_LOGE(TAG, "Received invalid message (mantissa size too large %d, expected 4)", mantissaRange);
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
  for (int i = 0; i < mantissaRange; i++) {
    mantissa <<= 8;
    mantissa |= msg[i + 7];
  }

  // Calculate the actual value
  float value = mantissa * exponent;

  // Set sensor value
  this->setSensorValue_(command, value, unitIdx);
}

void KamstrupMC40xComponent::setSensorValue_(uint16_t command, float value, uint8_t unitIdx) {
  const char *unit = UNITS[unitIdx];

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

uint16_t crc16_(const uint8_t *buffer, int len) {
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
