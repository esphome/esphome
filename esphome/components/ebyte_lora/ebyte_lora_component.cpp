#include "ebyte_lora_component.h"
namespace esphome {
namespace ebyte_lora {

// when this is called it is asking peers to say something about repeater
static const uint8_t REQUEST_REPEATER_INFO = 0x88;
static const uint8_t REPEATER_INFO = 0x99;
static const uint8_t REPEATER_KEY = 0x99;
static const uint8_t PROGRAM_CONF = 0xC1;
static const uint8_t BINARY_SENSOR_KEY = 0x66;
static const uint8_t SENSOR_KEY = 0x77;
bool EbyteLoraComponent::is_config_right() {
  if (this->current_config_.addh != this->expected_config_.addh) {
    ESP_LOGD(TAG, "addh is not set right, it should be:");
    ESP_LOGD(TAG, "%u", this->expected_config_.addl);
    return false;
  }
  if (this->current_config_.addl != this->expected_config_.addl) {
    ESP_LOGD(TAG, "addl is not set right, it should be:");
    ESP_LOGD(TAG, "%u", this->expected_config_.addl);
    return false;
  }
  if (this->current_config_.air_data_rate != this->expected_config_.air_data_rate) {
    ESP_LOGD(TAG, "air_data_rate is not set right, it should be:");
    switch (this->expected_config_.air_data_rate) {
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
    return false;
  }
  if (this->current_config_.parity != this->expected_config_.parity) {
    ESP_LOGD(TAG, "parity is not set right, it should be:");
    switch (this->expected_config_.parity) {
      case EBYTE_UART_8N1:
        ESP_LOGD(TAG, "uart_parity: 8N1");
        break;
      case EBYTE_UART_8O1:
        ESP_LOGD(TAG, "uart_parity: 8O1");
        break;
      case EBYTE_UART_8E1:
        ESP_LOGD(TAG, "uart_parity: 8E1");
        break;
    }
    return false;
  }
  if (this->current_config_.uart_baud != this->expected_config_.uart_baud) {
    ESP_LOGD(TAG, "uart_baud is not set right, it should be:");
    switch (this->expected_config_.uart_baud) {
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
    return false;
  }
  if (this->current_config_.transmission_power != this->expected_config_.transmission_power) {
    ESP_LOGD(TAG, "transmission_power is not set right, it should be:");
    switch (this->expected_config_.transmission_power) {
      case TX_DEFAULT_MAX:
        ESP_LOGD(TAG, "transmission_power: default or max");
        break;
      case TX_LOWER:
        ESP_LOGD(TAG, "transmission_power: lower");
        break;
      case TX_EVEN_LOWER:
        ESP_LOGD(TAG, "transmission_power: even lower");
        break;
      case TX_LOWEST:
        ESP_LOGD(TAG, "transmission_power: Lowest");
        break;
    }
    return false;
  }
  if (this->current_config_.rssi_noise != this->expected_config_.rssi_noise) {
    ESP_LOGD(TAG, "rssi_noise is not set right, it should be:");
    switch (this->expected_config_.rssi_noise) {
      case EBYTE_ENABLED:
        ESP_LOGD(TAG, "rssi_noise: ENABLED");
        break;
      case EBYTE_DISABLED:
        ESP_LOGD(TAG, "rssi_noise: DISABLED");
        break;
    }
    return false;
  }
  if (this->current_config_.sub_packet != this->expected_config_.sub_packet) {
    ESP_LOGD(TAG, "sub_packet is not set right, it should be:");
    switch (this->expected_config_.sub_packet) {
      case SUB_200B:
        ESP_LOGD(TAG, "sub_packet: 200 bytes");
        break;
      case SUB_128B:
        ESP_LOGD(TAG, "sub_packet: 128 bytes");
        break;
      case SUB_64B:
        ESP_LOGD(TAG, "sub_packet: 64 bytes");
        break;
      case SUB_32B:
        ESP_LOGD(TAG, "sub_packet: 32 bytes");
        break;
    }
    return false;
  }
  if (this->current_config_.channel != this->expected_config_.channel) {
    ESP_LOGD(TAG, "channel is not set right is %u, should be %u", this->current_config_.channel,
             this->expected_config_.channel);
    return false;
  }
  if (this->current_config_.wor_period != this->expected_config_.wor_period) {
    ESP_LOGD(TAG, "wor_period is not set right, it should be:");
    switch (this->expected_config_.wor_period) {
      case WOR_500:
        ESP_LOGD(TAG, "wor_period: 500");
        break;
      case WOR_1000:
        ESP_LOGD(TAG, "wor_period: 1000");
        break;
      case WOR_1500:
        ESP_LOGD(TAG, "wor_period: 1500");
        break;
      case WOR_2000:
        ESP_LOGD(TAG, "wor_period: 2000");
        break;
      case WOR_2500:
        ESP_LOGD(TAG, "wor_period: 2500");
        break;
      case WOR_3000:
        ESP_LOGD(TAG, "wor_period: 3000");
        break;
      case WOR_3500:
        ESP_LOGD(TAG, "wor_period: 3500");
        break;
      case WOR_4000:
        ESP_LOGD(TAG, "wor_period: 4000");
        break;
    }
    return false;
  }
  if (this->current_config_.enable_lbt != this->expected_config_.enable_lbt) {
    ESP_LOGD(TAG, "enable_lbt is not set right, it should be:");
    switch (this->current_config_.enable_lbt) {
      case EBYTE_ENABLED:
        ESP_LOGD(TAG, "enable_lbt: ENABLED");
        break;
      case EBYTE_DISABLED:
        ESP_LOGD(TAG, "enable_lbt: DISABLED");
        break;
    }
    return false;
  }
  if (this->current_config_.transmission_mode != this->expected_config_.transmission_mode) {
    ESP_LOGD(TAG, "transmission_mode is not set right, it should be:");
    switch (this->expected_config_.transmission_mode) {
      case TRANSPARENT:
        ESP_LOGD(TAG, "transmission_type: TRANSPARENT");
        break;
      case FIXED:
        ESP_LOGD(TAG, "transmission_type: FIXED");
        break;
    }
    return false;
  }

  if (this->current_config_.enable_rssi != this->expected_config_.enable_rssi) {
    ESP_LOGD(TAG, "enable_rssi is not set right, it should be:");
    switch (this->expected_config_.enable_rssi) {
      case EBYTE_ENABLED:
        ESP_LOGD(TAG, "enable_rssi: ENABLED");
        break;
      case EBYTE_DISABLED:
        ESP_LOGD(TAG, "enable_rssi: DISABLED");
        break;
    }
    return false;
  }
  return true;
}
void EbyteLoraComponent::setup_conf_(std::vector<uint8_t> conf) {
  // 3 is addh
  this->current_config_.addh = conf[3];
  // 4 is addl
  this->current_config_.addl = conf[4];
  // 5 is reg0, which is air_data for first 3 bits, then parity for 2, uart_baud for 3
  ESP_LOGD(TAG, "reg0: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(conf[5]));
  this->current_config_.air_data_rate = (conf[5] >> 0) & 0b111;
  this->current_config_.parity = (conf[5] >> 3) & 0b11;
  this->current_config_.uart_baud = (conf[5] >> 5) & 0b111;
  // 6 is reg1; transmission_power : 2, reserve : 3, rssi_noise : 1, sub_packet : 2
  ESP_LOGD(TAG, "reg1: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(conf[6]));
  this->current_config_.transmission_power = (conf[6] >> 0) & 0b11;
  this->current_config_.rssi_noise = (conf[6] >> 5) & 0b1;
  this->current_config_.sub_packet = (conf[6] >> 6) & 0b11;
  // 7 is reg2; channel
  this->current_config_.channel = conf[7];
  // 8 is reg3; wor_period:3, reserve:1, enable_lbt:1, reserve:1, transmission_mode:1, enable_rssi:1
  ESP_LOGD(TAG, "reg3: %c%c%c%c%c%c%c%c", BYTE_TO_BINARY(conf[8]));
  this->current_config_.wor_period = (conf[8] >> 0) & 0b111;
  this->current_config_.enable_lbt = (conf[8] >> 4) & 0b1;
  this->current_config_.transmission_mode = (conf[8] >> 6) & 0b1;
  this->current_config_.enable_rssi = (conf[8] >> 7) & 0b1;
  ESP_LOGD(TAG, "Config set");
  this->current_config_.config_set = true;
}
void EbyteLoraComponent::set_config_() {
  uint8_t data[11];
  // set register
  data[0] = 0xC0;
  // where to start, 0 always
  data[1] = 0;
  // length, 8 bytes (3 start bytes don't count)
  data[2] = 8;
  // 3 is addh
  data[3] = this->expected_config_.addh;
  // 4 is addl
  data[4] = this->expected_config_.addl;
  // 5 is reg0, which is air_data for first 3 bits, then parity for 2, uart_baud for 3
  data[5] = (this->expected_config_.uart_baud << 5) | (this->expected_config_.parity << 3) |
            (this->expected_config_.air_data_rate << 0);
  // 6 is reg1; transmission_power : 2, reserve : 3, rssi_noise : 1, sub_packet : 2
  data[6] = (this->expected_config_.sub_packet << 6) | (this->expected_config_.rssi_noise << 5) |
            (this->expected_config_.transmission_power << 0);
  // 7 is reg2; channel
  data[7] = this->expected_config_.channel;
  // 8 is reg3; wor_period:3, reserve:1, enable_lbt:1, reserve:1, transmission_mode:1, enable_rssi:1
  data[8] = (this->expected_config_.enable_rssi << 7) | (this->expected_config_.transmission_mode << 6) |
            (this->expected_config_.enable_lbt << 4) | (this->expected_config_.wor_period << 0);
  // crypt stuff make 0 for now
  data[9] = 0;
  data[10] = 0;
  this->set_mode_(CONFIGURATION);
  this->write_array(data, sizeof(data));
}
void EbyteLoraComponent::setup() {
#ifdef USE_SENSOR
  for (auto &sensor : this->sensors_) {
    sensor.sensor->add_on_state_callback([this, &sensor](float x) {
      this->updated_ = true;
      sensor.updated = true;
    });
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &sensor : this->binary_sensors_) {
    sensor.sensor->add_on_state_callback([this, &sensor](bool value) {
      this->updated_ = true;
      sensor.updated = true;
    });
  }
#endif
  this->should_send_ = this->repeater_enabled_;
#ifdef USE_SENSOR
  this->should_send_ |= !this->sensors_.empty();
#endif
#ifdef USE_BINARY_SENSOR
  this->should_send_ |= !this->binary_sensors_.empty();
#endif
  this->pin_aux_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->pin_aux_->setup();
  this->pin_m0_->setup();
  this->pin_m1_->setup();
  ESP_LOGD(TAG, "Setup success");
}
void EbyteLoraComponent::request_current_config_() {
  if (this->get_mode_() != CONFIGURATION) {
    ESP_LOGD(TAG, "Mode not set right requesting that and returning %u", this->get_mode_());
    this->set_mode_(CONFIGURATION);
  }
  if (this->can_send_message_("get_current_config_")) {
    uint8_t data[3] = {PROGRAM_CONF, 0x00, 0x08};
    this->write_array(data, sizeof(data));
    ESP_LOGD(TAG, "Config info requested");
  } else {
    ESP_LOGD(TAG, "Config info can't be requested right now, since device is busy");
  }
}
ModeType EbyteLoraComponent::get_mode_() {
  if (!this->can_send_message_("get_mode_"))
    return MODE_INIT;
  bool pin1 = this->pin_m0_->digital_read();
  bool pin2 = this->pin_m1_->digital_read();
  if (!pin1 && !pin2) {
    // ESP_LOGD(TAG, "MODE NORMAL!");
    return NORMAL;
  }
  if (pin1 && !pin2) {
    // ESP_LOGD(TAG, "MODE WOR!");
    return WOR_SEND;
  }
  if (!pin1 && pin2) {
    // ESP_LOGD(TAG, "MODE WOR!");
    return WOR_RECEIVER;
  }
  if (pin1 && pin2) {
    // ESP_LOGD(TAG, "MODE Conf!");
    return CONFIGURATION;
  }
  return MODE_INIT;
}
void EbyteLoraComponent::set_mode_(ModeType mode) {
  if (this->pin_m0_ == nullptr || this->pin_m1_ == nullptr) {
    ESP_LOGD(TAG, "The M0 and M1 pins is not set, this mean that you are connect directly the pins as you need!");
    return;
  }
  // no need to do anything if the mode is correct
  if (this->get_mode_() == mode) {
    ESP_LOGD(TAG, "Mode is already correct");
    this->current_mode_ = mode;
    return;
  }
  // when the system starts, aux will stay low until you set the first mode
  // so make sure mode init isn't set AND we can't sent because aux is low
  if (!this->can_send_message_("set_mode")) {
    if (this->current_mode_ == MODE_INIT) {
      ESP_LOGD(TAG, "Very first time setting the mode, going to ignore device busy state");
    } else {
      ESP_LOGD(TAG, "Device busy lets wait, current mode is %u", this->current_mode_);
      return;
    }
  }
  switch (mode) {
    case NORMAL:
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
      this->pin_m0_->digital_write(false);
      this->pin_m1_->digital_write(true);
      ESP_LOGD(TAG, "MODE RECEIVING!");
      break;
    case CONFIGURATION:
      this->pin_m0_->digital_write(true);
      this->pin_m1_->digital_write(true);
      ESP_LOGD(TAG, "MODE SLEEP and CONFIG!");
      break;
    case MODE_INIT:
      ESP_LOGD(TAG, "Don't call this!");
      break;
  }
  this->current_mode_ = mode;
}
bool EbyteLoraComponent::can_send_message_(const char *info) {
  // High means no more information is needed
  if (this->pin_aux_->digital_read()) {
    this->flush();
    ESP_LOGD(TAG, "Aux pin is High! Can send again! for: %s", info);
    return true;
  } else {
    ESP_LOGD(TAG, "Can't sent it right now for %s", info);
    return false;
  }
}
void EbyteLoraComponent::update() {
  ESP_LOGD(TAG, "Update loop");
  if (!this->current_config_.config_set) {
    ESP_LOGD(TAG, "Config not set yet! Requesting");
    this->request_current_config_();
    return;
  }
  if (!this->is_config_right()) {
    ESP_LOGD(TAG, "Config is not right, changing it now");
    this->set_config_();
    return;
  }
  // only make it normal when config is set
  if (this->current_mode_ != NORMAL) {
    ESP_LOGD(TAG, "Current mode is not normal");
    this->current_mode_ = this->get_mode_();

    if (this->current_mode_ != NORMAL) {
      ESP_LOGD(TAG, "Mode is not set right");
      this->set_mode_(NORMAL);
    }
  }
  this->updated_ = true;
  auto now = millis() / 1000;
  if (this->last_key_time_ + this->repeater_request_recyle_time_ < now) {
    this->resend_repeater_request_ = true;
    this->last_key_time_ = now;
  }
}
void EbyteLoraComponent::loop() {
  std::vector<uint8_t> data;
  if (!this->available())
    return;
  ESP_LOGD(TAG, "Reading serial");
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    data.push_back(c);
  }

  if (this->repeater_enabled_) {
    this->repeat_message_(data);
  }
  this->process_(data);

  if (this->resend_repeater_request_)
    this->request_repeater_info_();
  if (this->updated_) {
    this->send_data_(true);
  }
}
void EbyteLoraComponent::process_(std::vector<uint8_t> data) {
#ifdef USE_SENSOR
  auto &sensors = this->remote_sensors_[network_id_];
#endif
#ifdef USE_BINARY_SENSOR
  auto &binary_sensors = this->remote_binary_sensors_[network_id_];
#endif
  ESP_LOGD(TAG, "GOT new data to process");
  uint8_t first_byte = data[0];
  // rssi is always the last one, except for when it is a program conf
  if (first_byte == REQUEST_REPEATER_INFO) {
    ESP_LOGD(TAG, "Got request for repeater info from network id %u", data[1]);
    this->send_repeater_info_();
  }
  if (first_byte == REPEATER_INFO) {
    ESP_LOGD(TAG, "Got some repeater info from network %u setting rssi next", data[2]);
  }
  if (first_byte == PROGRAM_CONF) {
    ESP_LOGD(TAG, "GOT PROGRAM_CONF");
    this->setup_conf_(data);
    this->set_mode_(NORMAL);
  }
  // Do all the stuff if they are sensors
  if (first_byte == BINARY_SENSOR_KEY || first_byte == SENSOR_KEY) {
    for (size_t i = 0; i < data.size() - 1; i++) {
      uint8_t key = data[i];
      uint32_t u32 = 0;
      if (key == BINARY_SENSOR_KEY) {
        // 1 byte for the length of the sensor name, one for the name, 1 for the data
        if (data.size() - i < 3) {
          return ESP_LOGV(TAG, "Binary sensor key requires at least 3 more bytes");
        }
        // grab the first bite, that is the state, there will be 2 at least two more
        i++;
        u32 = data[i];
      } else if (key == SENSOR_KEY) {
        // same as before but we need 4 for sensor data
        if (data.size() - i < 6) {
          return ESP_LOGV(TAG, "Sensor key requires at least 6 more bytes");
        }
        i++;
        u32 = data[i];
        i++;
        u32 += data[i] << 8;
        i++;
        u32 += data[i] << 16;
        i++;
        u32 += data[i] << 24;
      }

      // max length of sensors name
      char sensor_name[256]{};
      // key length for the sensor data
      i++;
      uint8_t sensor_name_length = 0;
      sensor_name_length = data[i];
      if (data.size() - i < sensor_name_length) {
        return ESP_LOGV(TAG, "Name length of %u not available", sensor_name_length);
      }
      // get the memory cleared and set
      memset(sensor_name, 0, sizeof sensor_name);
      for (size_t s = 0; s < sensor_name_length; s++) {
        // set each
        sensor_name[s] = data[s];
      }

      ESP_LOGV(TAG, "Found sensor key %d, id %s, data %lX", key, sensor_name, (unsigned long) u32);
      // move the buffer to after sensor name length
      i += sensor_name_length;

#ifdef USE_SENSOR
      if (key == SENSOR_KEY && sensors.count(sensor_name) != 0)
        sensors[sensor_name]->publish_state(u32);
#endif
#ifdef USE_BINARY_SENSOR
      if (key == BINARY_SENSOR_KEY && binary_sensors.count(sensor_name) != 0)
        binary_sensors[sensor_name]->publish_state(u32 != 0);
#endif
    }

  } else {
    return ESP_LOGW(TAG, "Unknown key byte %X", first_byte);
  }

  // RSSI is always found whenever it is not program info
  if (first_byte != PROGRAM_CONF) {
    float rssi = (data[data.size() - 1] / 255.0) * 100;
#ifdef USE_SENSOR
    this->rssi_sensor_->publish_state(rssi);
#endif
    ESP_LOGD(TAG, "RSSI: %f", rssi);
  }
};
void EbyteLoraComponent::send_data_(bool all) {
  if (!this->can_send_message_("send_data_"))
    return;
  std::vector<uint8_t> data;
  data.push_back(network_id_);
#ifdef USE_SENSOR
  for (auto &sensor : this->sensors_) {
    if (all || sensor.updated) {
      sensor.updated = false;
      uint32_t u32 = sensor.sensor->get_state();
      data.push_back(SENSOR_KEY);
      data.push_back(u32 & 0xFF);
      data.push_back((u32 >> 8) & 0xFF);
      data.push_back((u32 >> 16) & 0xFF);
      data.push_back((u32 >> 24) & 0xFF);
      // add all the sensor date info
      auto len = strlen(sensor.id);
      data.push_back(len);
      for (size_t i = 0; i != len; i++) {
        data.push_back(*sensor.id++);
      }
    }
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &sensor : this->binary_sensors_) {
    if (all || sensor.updated) {
      sensor.updated = false;
      data.push_back(BINARY_SENSOR_KEY);
      data.push_back((uint8_t) sensor.sensor->state);
      auto len = strlen(sensor.id);
      data.push_back(len);
      for (size_t i = 0; i != len; i++) {
        data.push_back(*sensor.id++);
      }
    }
  }
#endif
  this->write_array(data);
}
void EbyteLoraComponent::send_repeater_info_() {
  if (!this->can_send_message_("send_repeater_info_"))
    return;
  uint8_t data[3];
  data[0] = REPEATER_INFO;  // response
  data[1] = this->repeater_enabled_;
  data[2] = network_id_;
  ESP_LOGD(TAG, "Telling system if i am a repeater and what my network_id is");
  this->write_array(data, sizeof(data));
}
void EbyteLoraComponent::request_repeater_info_() {
  if (!this->can_send_message_("request_repeater_info_"))
    return;
  uint8_t data[2];
  data[0] = REQUEST_REPEATER_INFO;  // Request
  data[1] = this->network_id_;      // for unique id
  ESP_LOGD(TAG, "Asking for repeater info");
  this->write_array(data, sizeof(data));
}
void EbyteLoraComponent::repeat_message_(std::vector<uint8_t> data) {
  ESP_LOGD(TAG, "Got some info that i need to repeat for network %u", data[1]);
  if (!this->can_send_message_("repeat_message_"))
    return;
  this->write_array(data.data(), data.size());
}
void EbyteLoraComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Ebyte Lora E220:");
  ESP_LOGCONFIG(TAG, "  Network id: %u", this->network_id_);
  if (this->repeater_enabled_) {
    ESP_LOGCONFIG(TAG, "  Mode: Repeater mode");
  } else {
    ESP_LOGCONFIG(TAG, "  Mode: Normal mode");
  }
#ifdef USE_SENSOR
  for (auto sensor : this->sensors_)
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor.id);
#endif
#ifdef USE_BINARY_SENSOR
  for (auto sensor : this->binary_sensors_)
    ESP_LOGCONFIG(TAG, "  Binary Sensor: %s", sensor.id);
#endif
  ESP_LOGCONFIG(TAG, "  Remote network: %u", this->network_id_);
#ifdef USE_SENSOR
  for (const auto &sensor : this->remote_sensors_[this->network_id_])
    ESP_LOGCONFIG(TAG, "    Sensor: %s", sensor.first.c_str());
#endif
#ifdef USE_BINARY_SENSOR
  for (const auto &sensor : this->remote_binary_sensors_[this->network_id_])
    ESP_LOGCONFIG(TAG, "    Binary Sensor: %s", sensor.first.c_str());
#endif
};
}  // namespace ebyte_lora
}  // namespace esphome
