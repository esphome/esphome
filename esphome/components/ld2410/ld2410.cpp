#include "ld2410.h"
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#define highbyte(val) (uint8_t)((val) >> 8)
#define lowbyte(val) (uint8_t)((val) &0xff)

namespace esphome {
namespace ld2410 {

static const char *const TAG = "ld2410";

LD2410Component::LD2410Component()
    : gate_still_threshold_numbers_(9),
      gate_move_threshold_numbers_(9),
      gate_still_sensors_(9),
      gate_move_sensors_(9) {}

void LD2410Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2410:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "HasTargetSensor", this->target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "MovingSensor", this->moving_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "StillSensor", this->still_target_binary_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "EngineeringModeSwitch", this->engineering_mode_switch_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "ResetButton", this->reset_button_);
  LOG_BUTTON("  ", "RestartButton", this->restart_button_);
  LOG_BUTTON("  ", "QueryButton", this->query_button_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Moving Distance", this->moving_target_distance_sensor_);
  LOG_SENSOR("  ", "Still Distance", this->still_target_distance_sensor_);
  LOG_SENSOR("  ", "Moving Energy", this->moving_target_energy_sensor_);
  LOG_SENSOR("  ", "Still Energy", this->still_target_energy_sensor_);
  LOG_SENSOR("  ", "Detection Distance", this->detection_distance_sensor_);
  for (sensor::Sensor *s : this->gate_still_sensors_)
    LOG_SENSOR("  ", "Still Sesnsor", s);
  for (sensor::Sensor *s : this->gate_move_sensors_)
    LOG_SENSOR("  ", "Move Sesnsor", s);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "VersionTextSensor", this->version_text_sensor_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "MaxStillDistanceNumber", this->max_still_distance_number_);
  LOG_NUMBER("  ", "MaxMoveDistanceNumber", this->max_move_distance_number_);
  LOG_NUMBER("  ", "TimeoutNumber", this->timeout_number_);
  for (number::Number *n : this->gate_still_threshold_numbers_)
    LOG_NUMBER("  ", "Still Thresholds Number", n);
  for (number::Number *n : this->gate_move_threshold_numbers_)
    LOG_NUMBER("  ", "Move Thresholds Number", n);
#endif
  this->set_config_mode_(true);
  this->get_version_();
  delay(50);  // NOLINT
  this->query_parameters_();
  this->set_config_mode_(false);
  ESP_LOGCONFIG(TAG, "  Throttle_ : %ums", this->throttle_);
  ESP_LOGCONFIG(TAG, "  Firmware Version : %s", const_cast<char *>(this->version_.c_str()));
}

void LD2410Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2410...");
  this->set_config_mode_(true);
  this->get_version_();
  this->query_parameters_();
  this->set_config_mode_(false);
  ESP_LOGCONFIG(TAG, "Firmware Version : %s", const_cast<char *>(this->version_.c_str()));
  ESP_LOGCONFIG(TAG, "LD2410 setup complete.");
}

void LD2410Component::loop() {
  const int max_line_length = 80;
  static uint8_t buffer[max_line_length];

  while (available()) {
    this->readline_(read(), buffer, max_line_length);
  }
}

void LD2410Component::send_command_(uint8_t command, uint8_t *command_value, int command_value_len) {
  // frame start bytes
  this->write_array(CMD_FRAME_HEADER, 4);
  // length bytes
  int len = 2;
  if (command_value != nullptr)
    len += command_value_len;
  this->write_byte(lowbyte(len));
  this->write_byte(highbyte(len));

  // command
  this->write_byte(lowbyte(command));
  this->write_byte(highbyte(command));

  // command value bytes
  if (command_value != nullptr) {
    for (int i = 0; i < command_value_len; i++) {
      this->write_byte(command_value[i]);
    }
  }
  // frame end bytes
  this->write_array(CMD_FRAME_END, 4);
  // FIXME to remove
  delay(50);  // NOLINT
}

void LD2410Component::handle_periodic_data_(uint8_t *buffer, int len) {
  if (len < 12)
    return;  // 4 frame start bytes + 2 length bytes + 1 data end byte + 1 crc byte + 4 frame end bytes
  if (buffer[0] != 0xF4 || buffer[1] != 0xF3 || buffer[2] != 0xF2 || buffer[3] != 0xF1)  // check 4 frame start bytes
    return;
  if (buffer[7] != HEAD || buffer[len - 6] != END || buffer[len - 5] != CHECK)  // Check constant values
    return;  // data head=0xAA, data end=0x55, crc=0x00

  /*
    Reduce data update rate to prevent home assistant database size grow fast
  */
  int32_t current_millis = millis();
  if (current_millis - last_periodic_millis_ < this->throttle_)
    return;
  last_periodic_millis_ = current_millis;

  /*
    Data Type: 7th
    0x01: Engineering mode
    0x02: Normal mode
  */
  bool engineering_mode = buffer[DATA_TYPES] == 0x01;
#ifdef USE_SWITCH
  if (this->engineering_mode_switch_ != nullptr &&
      current_millis - last_engineering_mode_change_millis_ > this->throttle_) {
    if (engineering_mode && !this->engineering_mode_switch_->state) {
      this->engineering_mode_switch_->turn_on();
    } else if (!engineering_mode && this->engineering_mode_switch_->state) {
      this->engineering_mode_switch_->turn_off();
    }
  }
#endif
#ifdef USE_BINARY_SENSOR
  /*
    Target states: 9th
    0x00 = No target
    0x01 = Moving targets
    0x02 = Still targets
    0x03 = Moving+Still targets
  */
  char target_state = buffer[TARGET_STATES];
  if (this->target_binary_sensor_ != nullptr) {
    this->target_binary_sensor_->publish_state(target_state != 0x00);
  }
  if (this->moving_target_binary_sensor_ != nullptr) {
    this->moving_target_binary_sensor_->publish_state(CHECK_BIT(target_state, 0));
  }
  if (this->still_target_binary_sensor_ != nullptr) {
    this->still_target_binary_sensor_->publish_state(CHECK_BIT(target_state, 1));
  }
#endif
  /*
    Moving target distance: 10~11th bytes
    Moving target energy: 12th byte
    Still target distance: 13~14th bytes
    Still target energy: 15th byte
    Detect distance: 16~17th bytes
  */
#ifdef USE_SENSOR
  if (this->moving_target_distance_sensor_ != nullptr) {
    int new_moving_target_distance = this->two_byte_to_int_(buffer[MOVING_TARGET_LOW], buffer[MOVING_TARGET_HIGH]);
    if (this->moving_target_distance_sensor_->get_state() != new_moving_target_distance)
      this->moving_target_distance_sensor_->publish_state(new_moving_target_distance);
  }
  if (this->moving_target_energy_sensor_ != nullptr) {
    int new_moving_target_energy = buffer[MOVING_ENERGY];
    if (this->moving_target_energy_sensor_->get_state() != new_moving_target_energy)
      this->moving_target_energy_sensor_->publish_state(new_moving_target_energy);
  }
  if (this->still_target_distance_sensor_ != nullptr) {
    int new_still_target_distance = this->two_byte_to_int_(buffer[STILL_TARGET_LOW], buffer[STILL_TARGET_HIGH]);
    if (this->still_target_distance_sensor_->get_state() != new_still_target_distance)
      this->still_target_distance_sensor_->publish_state(new_still_target_distance);
  }
  if (this->still_target_energy_sensor_ != nullptr) {
    int new_still_target_energy = buffer[STILL_ENERGY];
    if (this->still_target_energy_sensor_->get_state() != new_still_target_energy)
      this->still_target_energy_sensor_->publish_state(new_still_target_energy);
  }
  if (this->detection_distance_sensor_ != nullptr) {
    int new_detect_distance = this->two_byte_to_int_(buffer[DETECT_DISTANCE_LOW], buffer[DETECT_DISTANCE_HIGH]);
    if (this->detection_distance_sensor_->get_state() != new_detect_distance)
      this->detection_distance_sensor_->publish_state(new_detect_distance);
  }
  if (engineering_mode) {
    /*
      Moving distance range: 18th byte
      Still distance range: 19th byte
      Moving enery: 20~28th bytes
    */
    for (std::vector<sensor::Sensor *>::size_type i = 0; i != this->gate_move_sensors_.size(); i++) {
      sensor::Sensor *s = this->gate_move_sensors_[i];
      if (s != nullptr) {
        s->publish_state(buffer[MOVING_SENSOR_START + i]);
      }
    }
    /*
      Still energy: 29~37th bytes
    */
    for (std::vector<sensor::Sensor *>::size_type i = 0; i != this->gate_still_sensors_.size(); i++) {
      sensor::Sensor *s = this->gate_still_sensors_[i];
      if (s != nullptr) {
        s->publish_state(buffer[STILL_SENSOR_START + i]);
      }
    }
  } else {
    for (std::vector<sensor::Sensor *>::size_type i = 0; i != this->gate_move_sensors_.size(); i++) {
      sensor::Sensor *s = this->gate_move_sensors_[i];
      if (s != nullptr && !std::isnan(s->get_state())) {
        s->publish_state(NAN);
      }
    }
    for (std::vector<sensor::Sensor *>::size_type i = 0; i != this->gate_still_sensors_.size(); i++) {
      sensor::Sensor *s = this->gate_still_sensors_[i];
      if (s != nullptr && !std::isnan(s->get_state())) {
        s->publish_state(NAN);
      }
    }
  }
#endif
}

const char VERSION_FMT[] = "%u.%u.%u%u%u%u";

std::string format_version(uint8_t *buffer) {
  std::string::size_type version_size = 256;
  std::string version;
  do {
    version.resize(version_size + 1);
    version_size = std::snprintf(&version[0], version.size(), VERSION_FMT, buffer[13], buffer[12], buffer[17],
                                 buffer[16], buffer[15], buffer[14]);
  } while (version_size + 1 > version.size());
  version.resize(version_size);
  return version;
}

#ifdef USE_NUMBER
std::function<void(void)> set_number_value(number::Number *n, float value) {
  float normalized_value = value * 1.0;
  if (n != nullptr && (!n->has_state() || n->state != normalized_value)) {
    n->state = normalized_value;
    return [n, normalized_value]() { n->publish_state(normalized_value); };
  }
  return []() {};
}
#endif

void LD2410Component::handle_ack_data_(uint8_t *buffer, int len) {
  ESP_LOGV(TAG, "Handling ACK DATA for COMMAND");
  if (len < 10) {
    ESP_LOGE(TAG, "Error with last command : incorrect length");
    return;
  }
  if (buffer[0] != 0xFD || buffer[1] != 0xFC || buffer[2] != 0xFB || buffer[3] != 0xFA) {  // check 4 frame start bytes
    ESP_LOGE(TAG, "Error with last command : incorrect Header");
    return;
  }
  if (buffer[COMMAND_STATUS] != 0x01) {
    ESP_LOGE(TAG, "Error with last command : status != 0x01");
    return;
  }
  if (this->two_byte_to_int_(buffer[8], buffer[9]) != 0x00) {
    ESP_LOGE(TAG, "Error with last command , last buffer was: %u , %u", buffer[8], buffer[9]);
    return;
  }

  switch (buffer[COMMAND]) {
    case lowbyte(CMD_ENABLE_CONF):
      ESP_LOGV(TAG, "Handled Enable conf command");
      break;
    case lowbyte(CMD_DISABLE_CONF):
      ESP_LOGV(TAG, "Handled Disabled conf command");
      break;
    case lowbyte(CMD_VERSION):
      this->version_ = format_version(buffer);
      ESP_LOGV(TAG, "FW Version is: %s", const_cast<char *>(this->version_.c_str()));
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr) {
        this->version_text_sensor_->publish_state(this->version_);
      }
#endif
      break;
    case lowbyte(CMD_GATE_SENS):
      ESP_LOGV(TAG, "Handled sensitivity command");
      break;
    case lowbyte(CMD_QUERY):  // Query parameters response
    {
      if (buffer[10] != 0xAA)
        return;  // value head=0xAA
#ifdef USE_NUMBER
      /*
        Moving distance range: 13th byte
        Still distance range: 14th byte
      */
      std::vector<std::function<void(void)>> updates;
      updates.push_back(set_number_value(this->max_move_distance_number_, buffer[12] * 0.75));
      updates.push_back(set_number_value(this->max_still_distance_number_, buffer[13] * 0.75));
      /*
        Moving Sensitivities: 15~23th bytes
      */
      for (std::vector<number::Number *>::size_type i = 0; i != this->gate_move_threshold_numbers_.size(); i++) {
        updates.push_back(set_number_value(this->gate_move_threshold_numbers_[i], buffer[14 + i]));
      }
      /*
        Still Sensitivities: 24~32th bytes
      */
      for (std::vector<number::Number *>::size_type i = 0; i != this->gate_still_threshold_numbers_.size(); i++) {
        updates.push_back(set_number_value(this->gate_still_threshold_numbers_[i], buffer[23 + i]));
      }
      /*
        None Duration: 33~34th bytes
      */
      updates.push_back(set_number_value(this->timeout_number_, this->two_byte_to_int_(buffer[32], buffer[33])));
      for (std::vector<std::function<void(void)>>::size_type i = 0; i != updates.size(); i++) {
        updates[i]();
      }
#endif
    } break;
    default:
      break;
  }
}

void LD2410Component::readline_(int readch, uint8_t *buffer, int len) {
  static int pos = 0;

  if (readch >= 0) {
    if (pos < len - 1) {
      buffer[pos++] = readch;
      buffer[pos] = 0;
    } else {
      pos = 0;
    }
    if (pos >= 4) {
      if (buffer[pos - 4] == 0xF8 && buffer[pos - 3] == 0xF7 && buffer[pos - 2] == 0xF6 && buffer[pos - 1] == 0xF5) {
        ESP_LOGV(TAG, "Will handle Periodic Data");
        this->handle_periodic_data_(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      } else if (buffer[pos - 4] == 0x04 && buffer[pos - 3] == 0x03 && buffer[pos - 2] == 0x02 &&
                 buffer[pos - 1] == 0x01) {
        ESP_LOGV(TAG, "Will handle ACK Data");
        this->handle_ack_data_(buffer, pos);
        pos = 0;  // Reset position index ready for next time
      }
    }
  }
}

void LD2410Component::set_config_mode_(bool enable) {
  uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(cmd, enable ? cmd_value : nullptr, 2);
}

void LD2410Component::set_engineering_mode_(bool enable) {
  last_engineering_mode_change_millis_ = millis();
  uint8_t cmd = enable ? CMD_ENABLE_ENG : CMD_DISABLE_ENG;
  this->send_command_(cmd, nullptr, 0);
}

void LD2410Component::factory_reset_() { this->send_command_(CMD_RESET, nullptr, 0); }

void LD2410Component::restart_() { this->send_command_(CMD_RESTART, nullptr, 0); }

void LD2410Component::query_parameters_() { this->send_command_(CMD_QUERY, nullptr, 0); }
void LD2410Component::get_version_() { this->send_command_(CMD_VERSION, nullptr, 0); }

void LD2410Component::set_max_distances_timeout_() {
  if (!this->max_move_distance_number_->has_state() || !this->max_still_distance_number_->has_state() ||
      !this->timeout_number_->has_state()) {
    return;
  }
  int max_moving_distance_range = static_cast<int>(this->max_move_distance_number_->state / 0.75);
  int max_still_distance_range = static_cast<int>(this->max_still_distance_number_->state / 0.75);
  int timeout = static_cast<int>(this->timeout_number_->state);
  uint8_t value[18] = {0x00,
                       0x00,
                       lowbyte(max_moving_distance_range),
                       highbyte(max_moving_distance_range),
                       0x00,
                       0x00,
                       0x01,
                       0x00,
                       lowbyte(max_still_distance_range),
                       highbyte(max_still_distance_range),
                       0x00,
                       0x00,
                       0x02,
                       0x00,
                       lowbyte(timeout),
                       highbyte(timeout),
                       0x00,
                       0x00};
  this->send_command_(CMD_MAXDIST_DURATION, value, 18);
  delay(50);  // NOLINT
  this->query_parameters_();
}

void LD2410Component::set_gate_threshold_(uint8_t gate) {
  number::Number *motionsens = this->gate_move_threshold_numbers_[gate];
  number::Number *stillsens = this->gate_still_threshold_numbers_[gate];

  if (!motionsens->has_state() || !stillsens->has_state()) {
    return;
  }
  int motion = static_cast<int>(motionsens->state);
  int still = static_cast<int>(stillsens->state);

  // reference
  // https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-?spm=a2g0o.detail.1000023.17.93465697yFwVxH
  //   Send data: configure the motion sensitivity of distance gate 3 to 40, and the static sensitivity of 40
  // 00 00 (gate)
  // 03 00 00 00 (gate number)
  // 01 00 (motion sensitivity)
  // 28 00 00 00 (value)
  // 02 00 (still sensitivtiy)
  // 28 00 00 00 (value)
  uint8_t value[18] = {0x00, 0x00, lowbyte(gate),   highbyte(gate),   0x00, 0x00,
                       0x01, 0x00, lowbyte(motion), highbyte(motion), 0x00, 0x00,
                       0x02, 0x00, lowbyte(still),  highbyte(still),  0x00, 0x00};
  this->send_command_(CMD_GATE_SENS, value, 18);
  delay(50);  // NOLINT
  this->query_parameters_();
}

void LD2410Component::set_gate_still_threshold_number(int gate, number::Number *n) {
  this->gate_still_threshold_numbers_[gate] = n;
  this->gate_still_threshold_numbers_[gate]->add_on_state_callback([this, gate](float state) {
    this->set_config_mode_(true);
    this->set_gate_threshold_(gate);
    this->set_config_mode_(false);
  });
}

void LD2410Component::set_gate_move_threshold_number(int gate, number::Number *n) {
  this->gate_move_threshold_numbers_[gate] = n;
  this->gate_move_threshold_numbers_[gate]->add_on_state_callback([this, gate](float state) {
    this->set_config_mode_(true);
    this->set_gate_threshold_(gate);
    this->set_config_mode_(false);
  });
}

void LD2410Component::set_gate_move_sensor(int gate, sensor::Sensor *s) { this->gate_move_sensors_[gate] = s; }
void LD2410Component::set_gate_still_sensor(int gate, sensor::Sensor *s) { this->gate_still_sensors_[gate] = s; }

}  // namespace ld2410
}  // namespace esphome
