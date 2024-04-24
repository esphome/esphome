#include "ebyte_lora.h"
namespace esphome {
namespace ebyte_lora {
static const uint8_t SWITCH_PUSH = 0x55;
static const uint8_t SWITCH_INFO = 0x66;
static const uint8_t PROGRAM_CONF = 0xC1;
void EbyteLoraComponent::update() {
  if (this->config.command == 0) {
    ESP_LOGD(TAG, "Config not set yet!, gonna request it now!");
    this->get_current_config_();
    return;
  }
  if (this->get_mode_() != NORMAL) {
    ESP_LOGD(TAG, "Mode was not set right");
    this->set_mode_(NORMAL);
  }

  this->send_switch_info_();
}
void EbyteLoraComponent::setup() {
  this->pin_aux_->setup();
  this->pin_m0_->setup();
  this->pin_m1_->setup();
  this->get_current_config_();
  ESP_LOGD(TAG, "Setup success");
}
void EbyteLoraComponent::get_current_config_() {
  this->set_mode_(CONFIGURATION);
  uint8_t data[3] = {PROGRAM_CONF, 0x00, 0x08};
  this->write_array(data, sizeof(data));
  ESP_LOGD(TAG, "Config info requested");
}
ModeType EbyteLoraComponent::get_mode_() {
  ModeType internal_mode = MODE_INIT;
  if (!this->can_send_message_()) {
    return internal_mode;
  }

  bool pin1 = this->pin_m0_->digital_read();
  bool pin2 = this->pin_m1_->digital_read();
  if (!pin1 && !pin2) {
    // ESP_LOGD(TAG, "MODE NORMAL!");
    internal_mode = NORMAL;
  }
  if (pin1 && !pin2) {
    // ESP_LOGD(TAG, "MODE WOR!");
    internal_mode = WOR_SEND;
  }
  if (!pin1 && pin2) {
    // ESP_LOGD(TAG, "MODE WOR!");
    internal_mode = WOR_RECEIVER;
  }
  if (pin1 && pin2) {
    // ESP_LOGD(TAG, "MODE Conf!");
    internal_mode = CONFIGURATION;
  }
  if (internal_mode != this->mode_) {
    ESP_LOGD(TAG, "Modes are not equal, calling the set function!! , checked: %u, expected: %u", internal_mode,
             this->mode_);
    this->set_mode_(internal_mode);
  }
  return internal_mode;
}
void EbyteLoraComponent::set_mode_(ModeType mode) {
  if (!this->can_send_message_()) {
    return;
  }
  if (this->pin_m0_ == nullptr && this->pin_m1_ == nullptr) {
    ESP_LOGD(TAG, "The M0 and M1 pins is not set, this mean that you are connect directly the pins as you need!");
  } else {
    switch (mode) {
      case NORMAL:
        // Mode 0 | normal operation
        this->pin_m0_->digital_write(false);
        this->pin_m1_->digital_write(false);
        ESP_LOGD(TAG, "MODE NORMAL!");
        break;
      case WOR_SEND:
        this->pin_m0_->digital_write(true);
        this->pin_m1_->digital_write(false);
        ESP_LOGD(TAG, "MODE WOR SEND!");
        break;
      case WOR_RECEIVER:
        // case MODE_2_PROGRAM:
        this->pin_m0_->digital_write(false);
        this->pin_m1_->digital_write(true);
        ESP_LOGD(TAG, "MODE RECEIVING!");
        break;
      case CONFIGURATION:
        // Mode 3 | Setting operation
        this->pin_m0_->digital_write(true);
        this->pin_m1_->digital_write(true);
        ESP_LOGD(TAG, "MODE SLEEP and CONFIG!");
        break;
      case MODE_INIT:
        ESP_LOGD(TAG, "Don't call this!");
        break;
    }
  }
  // wait until aux pin goes back low
  this->setup_wait_response_(1000);
  this->mode_ = mode;
  ESP_LOGD(TAG, "Mode is going to be set");
}
bool EbyteLoraComponent::can_send_message_() {
  // High means no more information is needed
  if (this->pin_aux_->digital_read()) {
    if (!this->starting_to_check_ == 0 && !this->time_out_after_ == 0) {
      this->starting_to_check_ = 0;
      this->time_out_after_ = 0;
      this->flush();
      ESP_LOGD(TAG, "Aux pin is High! Can send again!");
    }
    return true;
  } else {
    // it has taken too long to complete, error out!
    if ((millis() - this->starting_to_check_) > this->time_out_after_) {
      ESP_LOGD(TAG, "Timeout error! Resetting timers");
      this->starting_to_check_ = 0;
      this->time_out_after_ = 0;
    }
    return false;
  }
}

void EbyteLoraComponent::setup_wait_response_(uint32_t timeout) {
  if (this->starting_to_check_ != 0 || this->time_out_after_ != 0) {
    ESP_LOGD(TAG, "Wait response already set!!  %u", timeout);
  }
  ESP_LOGD(TAG, "Setting a timer for %u", timeout);
  this->starting_to_check_ = millis();
  this->time_out_after_ = timeout;
}
void EbyteLoraComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220");
  LOG_PIN("Aux pin:", this->pin_aux_);
  LOG_PIN("M0 Pin:", this->pin_m0_);
  LOG_PIN("M1 Pin:", this->pin_m1_);
};
void EbyteLoraComponent::digital_write(uint8_t pin, bool value) { this->send_switch_push_(pin, value); }
void EbyteLoraComponent::send_switch_push_(uint8_t pin, bool value) {
  if (!this->can_send_message_()) {
    return;
  }
  uint8_t data[3];
  data[0] = SWITCH_PUSH;  // number one to indicate
  data[1] = pin;          // Pin to send
  data[2] = value;        // Inverted for the pcf8574
  ESP_LOGD(TAG, "Sending message to remote lora");
  ESP_LOGD(TAG, "PIN: %u ", data[1]);
  ESP_LOGD(TAG, "VALUE: %u ", data[2]);
  this->write_array(data, sizeof(data));
  this->setup_wait_response_(5000);
  ESP_LOGD(TAG, "Successfully put in queue");
}
void EbyteLoraComponent::loop() {
  std::string buffer;
  std::vector<uint8_t> data;
  if (!this->available())
    return;
  ESP_LOGD(TAG, "Reading serial");
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    data.push_back(c);
  }
  // if it is only push info
  if (data[0] == SWITCH_PUSH) {
    ESP_LOGD(TAG, "GOT SWITCH PUSH ", data.size());
    ESP_LOGD(TAG, "Total: %u ", data.size());
    ESP_LOGD(TAG, "Start bit: ", data[0]);
    ESP_LOGD(TAG, "PIN: %u ", data[1]);
    ESP_LOGD(TAG, "VALUE: %u ", data[2]);
    ESP_LOGD(TAG, "RSSI: %u % ", (data[3] / 255.0) * 100);
    if (this->rssi_sensor_ != nullptr)
      this->rssi_sensor_->publish_state((data[3] / 255.0) * 100);

    for (auto *sensor : this->sensors_) {
      if (sensor->get_pin() == data[1]) {
        ESP_LOGD(TAG, "Updating switch");
        sensor->publish_state(data[2]);
      }
    }
    send_switch_info_();
  }
  // starting info loop
  if (data[0] == SWITCH_INFO) {
    for (int i = 0; i < data.size(); i++) {
      if (data[i] == SWITCH_INFO) {
        ESP_LOGD(TAG, "GOT INFO ", data.size());
        uint8_t pin = data[i + 1];
        bool value = data[i + 2];
        for (auto *sensor : this->sensors_) {
          if (pin == sensor->get_pin()) {
            sensor->publish_state(value);
          }
        }
      }
    }
    this->rssi_sensor_->publish_state((data[data.size() - 1] / 255.0) * 100);
    ESP_LOGD(TAG, "RSSI: %u % ", (data[data.size() - 1] / 255.0) * 100);
  }
  if (data[0] == PROGRAM_CONF) {
    ESP_LOGD(TAG, "GOT PROGRAM_CONF");
    this->setup_conf_(data);
    this->set_mode_(NORMAL);
  }
}
void EbyteLoraComponent::setup_conf_(std::vector<uint8_t> data) {
  ESP_LOGD(TAG, "Current config:");
  for (int i = 0; i < data.size(); i++) {
    // 3 is addh
    if (i == 3) {
      ESP_LOGD(TAG, "addh: %u", data[i]);
    }
    // 4 is addl
    if (i == 4) {
      ESP_LOGD(TAG, "addl: %u", data[i]);
    }
    // 5 is reg0, which is air_data for first 3 bits, then parity for 2, uart_baud for 3
    if (i == 5) {
      ESP_LOGD(TAG, "reg0: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(data[i]));
      uint8_t air_data = (data[i] >> 0) & 0b111;
      uint8_t parity = (data[i] >> 3) & 0b11;
      uint8_t uart_baud = (data[i] >> 5) & 0b111;
      ESP_LOGD(TAG, "air_data: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(air_data));
      switch (air_data) {
        case AIR_2_4KB:
          ESP_LOGD(TAG, "air_data_rate: 2.4kb");
          break;
        case AIR_4_8KB:
          ESP_LOGD(TAG, "air_data_rate: 4.8kb");
          break;
        case AIR_9_6KB:
          ESP_LOGD(TAG, "air_data_rate: 9.6kb");
          break;
        case AIR_19_2KB:
          ESP_LOGD(TAG, "air_data_rate: 19.2kb");
          break;
        case AIR_38_4KB:
          ESP_LOGD(TAG, "air_data_rate: 38.4kb");
          break;
        case AIR_62_5KB:
          ESP_LOGD(TAG, "air_data_rate: 62.5kb");
          break;
      }
      ESP_LOGD(TAG, "parity: %u", parity);
      ESP_LOGD(TAG, "uart_baud: %u", uart_baud);
      switch (uart_baud) {
        case UART_1200:
          ESP_LOGD(TAG, "uart_baud: 1200");
          break;
        case UART_2400:
          ESP_LOGD(TAG, "uart_baud: 2400");
          break;
        case UART_4800:
          ESP_LOGD(TAG, "uart_baud: 4800");
          break;
        case UART_9600:
          ESP_LOGD(TAG, "uart_baud: 9600");
          break;
        case UART_19200:
          ESP_LOGD(TAG, "uart_baud: 19200");
          break;
        case UART_38400:
          ESP_LOGD(TAG, "uart_baud: 38400");
          break;
        case UART_57600:
          ESP_LOGD(TAG, "uart_baud: 57600");
          break;
        case UART_115200:
          ESP_LOGD(TAG, "uart_baud: 115200");
          break;
      }
    }
    // 6 is reg1; transmission_power : 2, reserve : 3, rssi_noise : 1, sub_packet : 2
    if (i == 6) {
      ESP_LOGD(TAG, "reg1: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(data[i]));
      uint8_t transmission_power = (data[i] >> 0) & 0b11;
      uint8_t rssi_noise = (data[i] >> 5) & 0b1;
      uint8_t sub_packet = data[i] & 0b00000011;
      ESP_LOGD(TAG, "transmission_power: %u", transmission_power);
      ESP_LOGD(TAG, "rssi_noise: %u", rssi_noise);
      ESP_LOGD(TAG, "sub_packet: %u", sub_packet);
    }
    // 7 is reg2; channel
    if (i == 7) {
      ESP_LOGD(TAG, "channel: %u", data[i]);
    }
    // 8 is reg3; wor_period:3, reserve:1, enable_lbt:1, reserve:1, transmission_mode:1, enable_rssi:1
    if (i == 7) {
      ESP_LOGD(TAG, "reg3: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(data[i]));
      uint8_t wor_period = data[i] & 0b111;
      uint8_t enable_lbt = data[i] & 0b00001;
      uint8_t transmission_mode = data[i] & 0b0000001;
      uint8_t enable_rssi = data[i] & 0b00000001;
      ESP_LOGD(TAG, "wor_period: %u", wor_period);
      ESP_LOGD(TAG, "enable_lbt: %u", enable_lbt);
      ESP_LOGD(TAG, "transmission_mode: %u", transmission_mode);
      ESP_LOGD(TAG, "enable_rssi: %u", enable_rssi);
    }
  }
}
void EbyteLoraComponent::send_switch_info_() {
  if (!this->can_send_message_()) {
    return;
  }
  std::vector<uint8_t> data;

  for (auto *sensor : this->sensors_) {
    uint8_t pin = sensor->get_pin();
    uint8_t value = sensor->state;
    data.push_back(SWITCH_INFO);  // number one to indicate
    data.push_back(pin);
    data.push_back(value);  // Pin to send
  }
  ESP_LOGD(TAG, "Sending switch info");
  this->write_array(data);
  this->setup_wait_response_(5000);
}
}  // namespace ebyte_lora
}  // namespace esphome
