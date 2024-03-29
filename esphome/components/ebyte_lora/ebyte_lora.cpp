#include "ebyte_lora.h"
namespace esphome {
namespace ebyte_lora {
static const uint8_t SWITCH_PUSH = 0x55;
static const uint8_t SWITCH_INFO = 0x66;
static const uint8_t PROGRAM_CONF = 0xC1;
void EbyteLoraComponent::update() {
  if (this->config.command == 0) {
    ESP_LOGD(TAG, "Config not set yet!, gonna request it now!");
    get_current_config_();
    return;
  }
  if (get_mode_() != NORMAL) {
    ESP_LOGD(TAG, "Mode was not set right");
    set_mode_(NORMAL);
  }

  send_switch_info_();
}
void EbyteLoraComponent::setup() {
  this->pin_aux_->setup();
  this->pin_m0_->setup();
  this->pin_m1_->setup();
  get_current_config_();
  ESP_LOGD(TAG, "Setup success");
}
void EbyteLoraComponent::get_current_config_() {
  set_mode_(CONFIGURATION);
  uint8_t data[3] = {PROGRAM_CONF, 0x00, 0x08};
  this->write_array(data, sizeof(data));
  if (!this->available()) {
    return;
  }
  if (read_array((uint8_t *) &this->config, sizeof(this->config))) {
    ESP_LOGD(TAG, "Found config");
    ESP_LOGD(TAG, this->config.addh_description_().c_str());
    ESP_LOGD(TAG, this->config.addl_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_0.air_data_rate_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_0.uart_baud_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_0.parity_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_1.rssi_noise_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_1.sub_packet_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_1.transmission_power_description_().c_str());
    ESP_LOGD(TAG, this->config.channel_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_3.enable_lbt_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_3.wor_period_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_3.enable_rssi_description_().c_str());
    ESP_LOGD(TAG, this->config.reg_3.transmission_type_description_().c_str());
    set_mode_(NORMAL);
  } else {
    ESP_LOGW(TAG, "Junk on wire. Throwing away partial message");
  }
}
ModeType EbyteLoraComponent::get_mode_() {
  ModeType internalMode = MODE_INIT;
  if (!EbyteLoraComponent::can_send_message_()) {
    return internalMode;
  }

  bool pin1 = this->pin_m0_->digital_read();
  bool pin2 = this->pin_m1_->digital_read();
  if (!pin1 && !pin2) {
    // ESP_LOGD(TAG, "MODE NORMAL!");
    internalMode = NORMAL;
  }
  if (pin1 && !pin2) {
    // ESP_LOGD(TAG, "MODE WOR!");
    internalMode = WOR_SEND;
  }
  if (!pin1 && pin2) {
    // ESP_LOGD(TAG, "MODE WOR!");
    internalMode = WOR_RECEIVER;
  }
  if (pin1 && pin2) {
    // ESP_LOGD(TAG, "MODE Conf!");
    internalMode = CONFIGURATION;
  }
  if (internalMode != this->mode_) {
    ESP_LOGD(TAG, "Modes are not equal, calling the set function!! , checked: %u, expected: %u", internalMode,
             this->mode_);
    set_mode_(internalMode);
  }
  return internalMode;
}
void EbyteLoraComponent::set_mode_(ModeType mode) {
  if (!can_send_message_()) {
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
  if (!EbyteLoraComponent::can_send_message_()) {
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
  }
}
void EbyteLoraComponent::send_switch_info_() {
  if (!EbyteLoraComponent::can_send_message_()) {
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
  ESP_LOGD(TAG, "Sending info back");
  this->write_array(data);
  this->setup_wait_response_(5000);
  ESP_LOGD(TAG, "Successfully put in queue");
}
}  // namespace ebyte_lora
}  // namespace esphome
