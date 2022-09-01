#include "dxs238xw.h"

namespace esphome {
namespace dxs238xw {

static const char *const TAG = "dxs238xw";

#ifdef USE_SENSOR
#define UPDATE_SENSOR(name, value) \
  if (this->name##_sensor_ != nullptr) { \
    if (this->name##_sensor_->get_raw_state() != (value) || this->get_component_state() == COMPONENT_STATE_SETUP) { \
      this->name##_sensor_->publish_state(value); \
    } \
  }
#else
#define UPDATE_SENSOR(name, value)
#endif

#ifdef USE_SENSOR
#define UPDATE_SENSOR_MEASUREMENTS(name, value) \
  if (this->name##_sensor_ != nullptr) { \
    float value_float = value; \
\
    if (this->name##_sensor_->get_raw_state() != value_float || \
        this->get_component_state() == COMPONENT_STATE_SETUP) { \
      this->name##_sensor_->publish_state(value_float); \
    } \
  }
#else
#define UPDATE_SENSOR_MEASUREMENTS(name, value)
#endif

#ifdef USE_TEXT_SENSOR
#define UPDATE_TEXT_SENSOR(name, value) \
  if (this->name##_text_sensor_ != nullptr) { \
    if (this->name##_text_sensor_->get_raw_state() != (value) || \
        this->get_component_state() == COMPONENT_STATE_SETUP) { \
      this->name##_text_sensor_->publish_state(value); \
    } \
  }
#else
#define UPDATE_TEXT_SENSOR(name, value)
#endif

#ifdef USE_BINARY_SENSOR
#define UPDATE_BINARY_SENSOR(name, value) \
  if (this->name##_binary_sensor_ != nullptr) { \
    if (this->name##_binary_sensor_->state != (value) || this->get_component_state() == COMPONENT_STATE_SETUP) { \
      this->name##_binary_sensor_->publish_state(value); \
    } \
  }
#else
#define UPDATE_BINARY_SENSOR(name, value)
#endif

#ifdef USE_NUMBER
#define UPDATE_NUMBER(name, value) \
  if (this->name##_number_ != nullptr) { \
    if (this->name##_number_->state != (value) || this->get_component_state() == COMPONENT_STATE_SETUP) { \
      this->name##_number_->publish_state(value); \
    } \
  }
#else
#define UPDATE_NUMBER(name, value)
#endif

#ifdef USE_SWITCH
#define UPDATE_SWITCH(name, value) \
  if (this->name##_switch_ != nullptr) { \
    if (this->name##_switch_->state != (value) || this->get_component_state() == COMPONENT_STATE_SETUP) { \
      this->name##_switch_->publish_state(value); \
    } \
  }
#else
#define UPDATE_SWITCH(name, value)
#endif

//------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------

void Dxs238xwComponent::setup() {
  ESP_LOGI(TAG, "* Load Preferences Values");
  this->load_initial_number_value_(this->preference_delay_value_set_, "delay_value_set",
                                   &this->ms_data_.delay_value_set);
  this->load_initial_number_value_(this->preference_energy_purchase_value_, "energy_purchase_value",
                                   &this->lp_data_.energy_purchase_value_tmp);
  this->load_initial_number_value_(this->preference_energy_purchase_alarm_, "energy_purchase_alarm",
                                   &this->lp_data_.energy_purchase_alarm_tmp);
  UPDATE_NUMBER(delay_value_set, this->ms_data_.delay_value_set)
  UPDATE_NUMBER(energy_purchase_value, this->lp_data_.energy_purchase_value_tmp)
  UPDATE_NUMBER(energy_purchase_alarm, this->lp_data_.energy_purchase_alarm_tmp)

  ESP_LOGI(TAG, "* Load GET_METER_ID");
  this->send_command_(SmCommandSend::GET_METER_ID);

  ESP_LOGI(TAG, "* Load GET_POWER_STATE");
  this->send_command_(SmCommandSend::GET_POWER_STATE);

  ESP_LOGI(TAG, "* Load GET_LIMIT_AND_PURCHASE_DATA");
  this->send_command_(SmCommandSend::GET_LIMIT_AND_PURCHASE_DATA);

  ESP_LOGI(TAG, "* Load GET_MEASUREMENT_DATA");
  this->send_command_(SmCommandSend::GET_MEASUREMENT_DATA);

  this->status_momentary_warning("wait_to_loop_and_update", SM_WAIT_TO_LOOP_AND_UPDATE_STATE);
}

void Dxs238xwComponent::loop() {
  if (this->get_component_state() == COMPONENT_STATE_LOOP) {
    this->incoming_messages_();

    this->send_command_(SmCommandSend::GET_POWER_STATE);
    this->send_command_(SmCommandSend::GET_LIMIT_AND_PURCHASE_DATA);
  }
}

void Dxs238xwComponent::update() {
  if (this->get_component_state() == COMPONENT_STATE_LOOP) {
    this->send_command_(SmCommandSend::GET_MEASUREMENT_DATA);
  }
}

void Dxs238xwComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "* energy_purchase_value: %u", this->lp_data_.energy_purchase_value_tmp);
  ESP_LOGCONFIG(TAG, "* energy_purchase_Alarm: %u", this->lp_data_.energy_purchase_alarm_tmp);
  ESP_LOGCONFIG(TAG, "* delay_value_set: %u", this->ms_data_.delay_value_set);
}

//------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------

void Dxs238xwComponent::meter_state_toggle() {
  bool state = !this->ms_data_.meter_state;

  this->send_command_(SmCommandSend::SET_POWER_STATE, state);
}

void Dxs238xwComponent::meter_state_on() {
  bool state = true;

  this->send_command_(SmCommandSend::SET_POWER_STATE, state);
}

void Dxs238xwComponent::meter_state_off() {
  bool state = false;

  this->send_command_(SmCommandSend::SET_POWER_STATE, state);
}

void Dxs238xwComponent::hex_message(std::string message, bool check_crc) {
  ESP_LOGD(TAG, "In --- send_hex_message");

  ESP_LOGD(TAG, "* message in = %s", message.c_str());

  this->error_type_ = SmErrorType::NO_ERROR;
  this->error_code_ = SmErrorCode::NO_ERROR;

  uint8_t length_message = message.length();

  if (length_message == 0 || length_message > SM_MAX_HEX_MSG_LENGTH) {
    this->error_type_ = SmErrorType::INPUT_DATA;
    this->error_code_ = SmErrorCode::MESSAGE_LENGTH;
  }

  if (this->error_code_ == SmErrorCode::NO_ERROR) {
    char tmp_message[SM_MAX_HEX_MSG_LENGTH_PARSE];

    uint8_t size_message_without_dots = 0;

    uint8_t character_hex_index = 0;

    for (uint8_t i = 0; i < length_message; i++) {
      if (message[i] == '.') {
        if (character_hex_index == 2) {
          character_hex_index = 0;
        } else {
          this->error_type_ = SmErrorType::INPUT_DATA;
          this->error_code_ = SmErrorCode::WRONG_MSG;

          break;
        }
      } else {
        if (character_hex_index == 2) {
          this->error_type_ = SmErrorType::INPUT_DATA;
          this->error_code_ = SmErrorCode::WRONG_MSG;

          break;
        } else {
          tmp_message[size_message_without_dots] = message[i];
          size_message_without_dots++;

          character_hex_index++;
        }
      }
    }

    if (this->error_code_ == SmErrorCode::NO_ERROR) {
      if ((size_message_without_dots % 2) != 0) {
        this->error_type_ = SmErrorType::INPUT_DATA;
        this->error_code_ = SmErrorCode::WRONG_MSG;
      }

      if (this->error_code_ == SmErrorCode::NO_ERROR) {
        const char *hex_message = tmp_message;

        uint8_t length_array = size_message_without_dots / 2;

        uint8_t send_array[length_array];

        parse_hex(hex_message, size_message_without_dots, send_array, length_array);

        if (check_crc) {
          if (this->calculate_crc_(send_array, length_array) != send_array[length_array - 1]) {
            this->error_type_ = SmErrorType::INPUT_DATA;
            this->error_code_ = SmErrorCode::CRC;
          }
        } else {
          send_array[length_array - 1] = this->calculate_crc_(send_array, length_array);
        }

        if (this->error_code_ == SmErrorCode::NO_ERROR) {
          ESP_LOGD(TAG, "* Message send: %s", format_hex_pretty(send_array, length_array).c_str());

          if (this->transmit_serial_data_(send_array, length_array)) {
            ESP_LOGD(TAG, "* Waiting answer:");

            uint32_t response_time = millis() + SM_MAX_MILLIS_TO_RESPONSE;

            while (available() == 0) {
              if (response_time < millis()) {
                this->error_type_ = SmErrorType::COMMUNICATION;
                this->error_code_ = SmErrorCode::TIMEOUT;

                break;
              }

              yield();
            }

            if (this->error_code_ == SmErrorCode::NO_ERROR) {
              if (this->incoming_messages_(false)) {
                ESP_LOGD(TAG, "* Successful answer");

                ESP_LOGD(TAG, "Out --- send_hex_message");

                return;
              }
            }

            ESP_LOGD(TAG, "* Failed answer");
          }
        }
      }
    }
  }

  this->print_error_();

  ESP_LOGD(TAG, "Out -- send_hex_message");
}

//------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------

void Dxs238xwComponent::set_switch_value(SmIdEntity entity, bool value) {
  if (this->get_component_state() == COMPONENT_STATE_LOOP) {
    SmCommandSend tmp_cmd_send;

    switch (entity) {
      case SmIdEntity::SWITCH_ENERGY_PURCHASE_STATE: {
        tmp_cmd_send = SmCommandSend::SET_PURCHASE_DATA;
        break;
      }
      case SmIdEntity::SWITCH_METER_STATE: {
        tmp_cmd_send = SmCommandSend::SET_POWER_STATE;
        break;
      }
      case SmIdEntity::SWITCH_DELAY_STATE: {
        tmp_cmd_send = SmCommandSend::SET_DELAY;
        break;
      }
      default: {
        ESP_LOGE(TAG, "ID %hhu is not a SWITCH or is not included in the case list", entity);
        return;
      }
    }

    this->send_command_(tmp_cmd_send, value);
  }
}

void Dxs238xwComponent::set_button_value(SmIdEntity entity) {
  if (this->get_component_state() == COMPONENT_STATE_LOOP) {
    switch (entity) {
      case SmIdEntity::BUTTON_RESET_DATA: {
        this->send_command_(SmCommandSend::SET_PURCHASE_DATA, false);
        this->send_command_(SmCommandSend::SET_RESET);
        break;
      }
      default: {
        ESP_LOGE(TAG, "ID %hhu is not a BUTTON or is not included in the case list", entity);
        break;
      }
    }
  }
}

void Dxs238xwComponent::set_number_value(SmIdEntity entity, float value) {
  if (this->get_component_state() == COMPONENT_STATE_LOOP) {
    uint32_t tmp_value = std::round(value);

    switch (entity) {
      case SmIdEntity::NUMBER_MAX_CURRENT_LIMIT: {
        this->lp_data_.max_current_limit = tmp_value;

        this->send_command_(SmCommandSend::SET_LIMIT_DATA);

        UPDATE_NUMBER(max_current_limit, this->lp_data_.max_current_limit)
        break;
      }
      case SmIdEntity::NUMBER_MAX_VOLTAGE_LIMIT: {
        if (tmp_value > this->lp_data_.min_voltage_limit) {
          this->lp_data_.max_voltage_limit = tmp_value;

          this->send_command_(SmCommandSend::SET_LIMIT_DATA);
        } else {
          ESP_LOGW(TAG, "max_voltage_limit - Value %u must not be less than min_voltage_limit %u", tmp_value,
                   this->lp_data_.min_voltage_limit);
        }

        UPDATE_NUMBER(max_voltage_limit, this->lp_data_.max_voltage_limit)
        break;
      }
      case SmIdEntity::NUMBER_MIN_VOLTAGE_LIMIT: {
        if (tmp_value < this->lp_data_.max_voltage_limit) {
          this->lp_data_.min_voltage_limit = tmp_value;

          this->send_command_(SmCommandSend::SET_LIMIT_DATA);
        } else {
          ESP_LOGW(TAG, "min_voltage_limit - Value %u must not be greater than max_voltage_limit %u", tmp_value,
                   this->lp_data_.max_voltage_limit);
        }

        UPDATE_NUMBER(min_voltage_limit, this->lp_data_.min_voltage_limit)
        break;
      }
      case SmIdEntity::NUMBER_ENERGY_PURCHASE_VALUE: {
        this->lp_data_.energy_purchase_value_tmp = tmp_value;

        this->save_initial_number_value_(this->preference_energy_purchase_value_,
                                         &this->lp_data_.energy_purchase_value_tmp);

        if (this->lp_data_.energy_purchase_state) {
          if (this->send_command_(SmCommandSend::SET_PURCHASE_DATA, false)) {
            this->send_command_(SmCommandSend::SET_PURCHASE_DATA, true);
          }
        }

        UPDATE_NUMBER(energy_purchase_value, this->lp_data_.energy_purchase_value_tmp)
        break;
      }
      case SmIdEntity::NUMBER_ENERGY_PURCHASE_ALARM: {
        this->lp_data_.energy_purchase_alarm_tmp = tmp_value;

        this->save_initial_number_value_(this->preference_energy_purchase_alarm_,
                                         &this->lp_data_.energy_purchase_alarm_tmp);

        if (this->lp_data_.energy_purchase_state) {
          if (this->send_command_(SmCommandSend::SET_PURCHASE_DATA, false)) {
            this->send_command_(SmCommandSend::SET_PURCHASE_DATA, true);
          }
        }

        UPDATE_NUMBER(energy_purchase_alarm, this->lp_data_.energy_purchase_alarm_tmp)
        break;
      }
      case SmIdEntity::NUMBER_DELAY_VALUE_SET: {
        this->ms_data_.delay_value_set = tmp_value;

        this->save_initial_number_value_(this->preference_delay_value_set_, &this->ms_data_.delay_value_set);

        if (this->ms_data_.delay_state) {
          this->send_command_(SmCommandSend::SET_DELAY, true);
        }

        UPDATE_NUMBER(delay_value_set, this->ms_data_.delay_value_set)
        break;
      }
      default: {
        ESP_LOGE(TAG, "ID %hhu is not a NUMBER or is not included in the case list", entity);
        return;
      }
    }
  }
}

//------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------

bool Dxs238xwComponent::transmit_serial_data_(uint8_t *array, uint8_t size) {
  while (available() > 0) {
    read();

    delay(2);
  }

  write_array(array, size);

  flush();

  ESP_LOGV(TAG, "* Waiting confirmation:");

  if (this->receive_serial_data_(array, size, array[4], HEKR_TYPE_SEND)) {
    ESP_LOGV(TAG, "* Successful Confirmation");

    return true;
  }

  ESP_LOGV(TAG, "* Confirmation Failed");

  return false;
}

bool Dxs238xwComponent::pre_transmit_serial_data_(uint8_t cmd, uint8_t frame_size, const uint8_t *array_data) {
  static uint8_t version = 0;

  uint8_t send_array[frame_size];

  send_array[0] = HEKR_HEADER;
  send_array[1] = frame_size;
  send_array[2] = HEKR_TYPE_SEND;
  send_array[3] = version++;
  send_array[4] = cmd;

  if (array_data != nullptr) {
    uint8_t send_array_index = 5;
    uint8_t data_size = frame_size - 6;

    for (uint8_t i = 0; i < data_size; i++) {
      send_array[send_array_index] = array_data[i];
      send_array_index++;
    }
  }

  send_array[frame_size - 1] = this->calculate_crc_(send_array, frame_size);

  ESP_LOGV(TAG, "* Message send: %s", format_hex_pretty(send_array, frame_size).c_str());

  return this->transmit_serial_data_(send_array, frame_size);
}

bool Dxs238xwComponent::receive_serial_data_(uint8_t *array, uint8_t size, uint8_t cmd, uint8_t type_message) {
  uint32_t response_time;

  SmErrorCode read_error = SmErrorCode::NO_ERROR;

  response_time = millis() + SM_MAX_MILLIS_TO_RESPONSE;

  while (available() < size) {
    if (response_time < millis()) {
      if (available() > 0) {
        read_error = SmErrorCode::NOT_ENOUGHT_BYTES;
      } else {
        read_error = SmErrorCode::TIMEOUT;
      }

      break;
    }

    yield();
  }

  delay(2);

  if (read_error == SmErrorCode::NO_ERROR) {
    if (available() == size) {
      for (uint8_t n = 0; n < size; n++) {
        array[n] = read();
      }

      ESP_LOGV(TAG, "* Message received: %s", format_hex_pretty(array, size).c_str());

      if (array[0] == HEKR_HEADER && array[1] == size && array[2] == type_message && array[4] == cmd) {
        if (this->calculate_crc_(array, size) != array[size - 1]) {
          read_error = SmErrorCode::CRC;
        }
      } else {
        read_error = SmErrorCode::WRONG_BYTES;
      }
    } else {
      read_error = SmErrorCode::EXCEEDS_BYTES;
    }
  }

  if (read_error != SmErrorCode::NO_ERROR) {
    this->error_type_ = SmErrorType::COMMUNICATION;
    this->error_code_ = read_error;
  }

  delay(2);

  while (available() > 0) {
    read();

    delay(2);
  }

  return (this->error_code_ == SmErrorCode::NO_ERROR);
}

bool Dxs238xwComponent::pre_receive_serial_data_(uint8_t cmd, uint8_t frame_size) {
  uint8_t receive_array[frame_size];

  ESP_LOGV(TAG, "* Waiting answer:");

  if (this->receive_serial_data_(receive_array, frame_size, cmd, HEKR_TYPE_RECEIVE)) {
    this->process_and_update_data_(receive_array);

    ESP_LOGV(TAG, "* Successful answer");

    return true;
  }

  ESP_LOGV(TAG, "* Failed answer");

  return false;
}

void Dxs238xwComponent::process_and_update_data_(const uint8_t *receive_array) {
  uint8_t cmd = receive_array[4];

  switch (cmd) {
    case HEKR_CMD_RECEIVE_METER_STATE: {
      this->ms_data_.time = millis();

      this->ms_data_.phase_count = receive_array[5];
      this->ms_data_.meter_state = receive_array[6];
      this->ms_data_.delay_state = receive_array[18];
      this->ms_data_.delay_value_remaining = (receive_array[16] << 8) | receive_array[17];

      if (this->ms_data_.meter_state) {
        this->ms_data_.warning_off_by_over_voltage = false;
        this->ms_data_.warning_off_by_under_voltage = false;
        this->ms_data_.warning_off_by_over_current = false;
        this->ms_data_.warning_off_by_end_purchase = false;
        this->ms_data_.warning_off_by_end_delay = false;
        this->ms_data_.warning_off_by_user = false;
      }

      if (!this->ms_data_.warning_off_by_over_voltage) {
        this->ms_data_.warning_off_by_over_voltage = (receive_array[11] == 1);
      }

      if (!this->ms_data_.warning_off_by_under_voltage) {
        this->ms_data_.warning_off_by_under_voltage = (receive_array[11] == 2);
      }

      if (!this->ms_data_.warning_off_by_over_current) {
        this->ms_data_.warning_off_by_over_current = receive_array[15];
      }

      if (!this->ms_data_.warning_off_by_end_purchase) {
        this->ms_data_.warning_off_by_end_purchase = receive_array[19];
      }

      if (!this->ms_data_.warning_off_by_end_delay) {
        this->ms_data_.warning_off_by_end_delay =
            (this->ms_data_.delay_value_remaining == 0 && this->ms_data_.delay_state);
      }
      this->set_delay_state_();

      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

      if (this->ms_data_.delay_state) {
        std::string delay_value_remaining_string =
            this->get_delay_value_remaining_string_(this->ms_data_.delay_value_remaining);
        UPDATE_TEXT_SENSOR(delay_value_remaining, delay_value_remaining_string)
      }

      UPDATE_BINARY_SENSOR(warning_off_by_over_voltage, this->ms_data_.warning_off_by_over_voltage)
      UPDATE_BINARY_SENSOR(warning_off_by_under_voltage, this->ms_data_.warning_off_by_under_voltage)
      UPDATE_BINARY_SENSOR(warning_off_by_over_current, this->ms_data_.warning_off_by_over_current)
      UPDATE_BINARY_SENSOR(warning_off_by_end_purchase, this->ms_data_.warning_off_by_end_purchase)
      UPDATE_BINARY_SENSOR(warning_off_by_end_delay, this->ms_data_.warning_off_by_end_delay)
      UPDATE_BINARY_SENSOR(warning_off_by_user, this->ms_data_.warning_off_by_user)

      UPDATE_BINARY_SENSOR(meter_state, this->ms_data_.meter_state)

      UPDATE_SENSOR(phase_count, this->ms_data_.phase_count)

      UPDATE_SWITCH(meter_state, this->ms_data_.meter_state)
      UPDATE_SWITCH(delay_state, this->ms_data_.delay_state)

      this->update_meter_state_detail_();

      break;
    }
    case HEKR_CMD_RECEIVE_MEASUREMENT: {
      UPDATE_SENSOR_MEASUREMENTS(current_phase_1,
                                 ((receive_array[5] << 16) | (receive_array[6] << 8) | receive_array[7]) * 0.001)
      UPDATE_SENSOR_MEASUREMENTS(current_phase_2,
                                 ((receive_array[8] << 16) | (receive_array[9] << 8) | receive_array[10]) * 0.001)
      UPDATE_SENSOR_MEASUREMENTS(current_phase_3,
                                 ((receive_array[11] << 16) | (receive_array[12] << 8) | receive_array[13]) * 0.001)

      UPDATE_SENSOR_MEASUREMENTS(voltage_phase_1, ((receive_array[14] << 8) | receive_array[15]) * 0.1)
      UPDATE_SENSOR_MEASUREMENTS(voltage_phase_2, ((receive_array[16] << 8) | receive_array[17]) * 0.1)
      UPDATE_SENSOR_MEASUREMENTS(voltage_phase_3, ((receive_array[18] << 8) | receive_array[19]) * 0.1)

      UPDATE_SENSOR_MEASUREMENTS(reactive_power_total,
                                 receive_array[20] + (((receive_array[21] << 8) | receive_array[22]) * 0.0001))
      UPDATE_SENSOR_MEASUREMENTS(reactive_power_phase_1,
                                 receive_array[23] + (((receive_array[24] << 8) | receive_array[25]) * 0.0001))
      UPDATE_SENSOR_MEASUREMENTS(reactive_power_phase_2,
                                 receive_array[26] + (((receive_array[27] << 8) | receive_array[28]) * 0.0001))
      UPDATE_SENSOR_MEASUREMENTS(reactive_power_phase_3,
                                 receive_array[29] + (((receive_array[30] << 8) | receive_array[31]) * 0.0001))

      UPDATE_SENSOR_MEASUREMENTS(active_power_total,
                                 receive_array[32] + (((receive_array[33] << 8) | receive_array[34]) * 0.0001))
      UPDATE_SENSOR_MEASUREMENTS(active_power_phase_1,
                                 receive_array[35] + (((receive_array[36] << 8) | receive_array[37]) * 0.0001))
      UPDATE_SENSOR_MEASUREMENTS(active_power_phase_2,
                                 receive_array[38] + (((receive_array[39] << 8) | receive_array[40]) * 0.0001))
      UPDATE_SENSOR_MEASUREMENTS(active_power_phase_3,
                                 receive_array[41] + (((receive_array[42] << 8) | receive_array[43]) * 0.0001))

      UPDATE_SENSOR_MEASUREMENTS(power_factor_total, ((receive_array[44] << 8) | receive_array[45]) * 0.001)
      UPDATE_SENSOR_MEASUREMENTS(power_factor_phase_1, ((receive_array[46] << 8) | receive_array[47]) * 0.001)
      UPDATE_SENSOR_MEASUREMENTS(power_factor_phase_2, ((receive_array[48] << 8) | receive_array[49]) * 0.001)
      UPDATE_SENSOR_MEASUREMENTS(power_factor_phase_3, ((receive_array[50] << 8) | receive_array[51]) * 0.001)

      UPDATE_SENSOR_MEASUREMENTS(
          total_energy,
          ((receive_array[54] << 24) | (receive_array[55] << 16) | (receive_array[56] << 8) | receive_array[57]) * 0.01)
      UPDATE_SENSOR_MEASUREMENTS(
          import_active_energy,
          ((receive_array[58] << 24) | (receive_array[59] << 16) | (receive_array[60] << 8) | receive_array[61]) * 0.01)
      UPDATE_SENSOR_MEASUREMENTS(
          export_active_energy,
          ((receive_array[62] << 24) | (receive_array[63] << 16) | (receive_array[64] << 8) | receive_array[65]) * 0.01)

      UPDATE_SENSOR_MEASUREMENTS(frequency, ((receive_array[52] << 8) | receive_array[53]) * 0.01)

      break;
    }
    case HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE: {
      this->lp_data_.time = millis();

      this->lp_data_.max_voltage_limit = (receive_array[5] << 8) | receive_array[6];
      this->lp_data_.min_voltage_limit = (receive_array[7] << 8) | receive_array[8];
      this->lp_data_.max_current_limit = ((receive_array[9] << 8) | receive_array[10]) * 0.01;

      this->lp_data_.energy_purchase_value =
          (((receive_array[11] << 24) | (receive_array[12] << 16) | (receive_array[13] << 8) | receive_array[14]) *
           0.01);
      this->lp_data_.energy_purchase_balance =
          (((receive_array[15] << 24) | (receive_array[16] << 16) | (receive_array[17] << 8) | receive_array[18]) *
           0.01);
      this->lp_data_.energy_purchase_alarm =
          (((receive_array[19] << 24) | (receive_array[20] << 16) | (receive_array[21] << 8) | receive_array[22]) *
           0.01);

      this->lp_data_.energy_purchase_state = receive_array[23];

      this->ms_data_.warning_purchase_alarm =
          (this->lp_data_.energy_purchase_balance <= this->lp_data_.energy_purchase_alarm &&
           this->lp_data_.energy_purchase_state);

      ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

      UPDATE_SENSOR(energy_purchase_balance, this->lp_data_.energy_purchase_balance)

      UPDATE_SWITCH(energy_purchase_state, this->lp_data_.energy_purchase_state)

      UPDATE_BINARY_SENSOR(warning_purchase_alarm, this->ms_data_.warning_purchase_alarm)

      UPDATE_NUMBER(max_current_limit, this->lp_data_.max_current_limit)
      UPDATE_NUMBER(max_voltage_limit, this->lp_data_.max_voltage_limit)
      UPDATE_NUMBER(min_voltage_limit, this->lp_data_.min_voltage_limit)

      break;
    }
    case HEKR_CMD_RECEIVE_METER_ID: {
      char serial_number[20];

      sprintf(serial_number, "%u%u%u %u%u%u", receive_array[5], receive_array[6], receive_array[7], receive_array[8],
              receive_array[9], receive_array[10]);

      std::string string_serial_number(serial_number);

      ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

      UPDATE_TEXT_SENSOR(meter_id, string_serial_number)

      break;
    }
  }
}

bool Dxs238xwComponent::send_command_(SmCommandSend cmd, bool state) {
  bool is_good_result = false;

  switch (cmd) {
    case SmCommandSend::GET_POWER_STATE: {
      if ((millis() - this->ms_data_.time) >= SM_MIN_INTERVAL_TO_GET_DATA) {
        is_good_result = this->put_command_data_(HEKR_CMD_SEND_GET_METER_STATE, HEKR_CMD_RECEIVE_METER_STATE,
                                                 HEKR_SIZE_SEND_GET_DATA, HEKR_SIZE_RECEIVE_METER_STATE, nullptr);

        ESP_LOGV(TAG, "In --- send_command - GET_POWER_STATE - Result = %s", TRUEFALSE(is_good_result));
      }

      break;
    }
    case SmCommandSend::GET_MEASUREMENT_DATA: {
      is_good_result = this->put_command_data_(HEKR_CMD_SEND_GET_MEASUREMENT, HEKR_CMD_RECEIVE_MEASUREMENT,
                                               HEKR_SIZE_SEND_GET_DATA, HEKR_SIZE_RECEIVE_MEASUREMENT, nullptr);

      ESP_LOGV(TAG, "In --- send_command - GET_MEASUREMENT_DATA - Result = %s", TRUEFALSE(is_good_result));

      break;
    }
    case SmCommandSend::GET_LIMIT_AND_PURCHASE_DATA: {
      if ((millis() - this->lp_data_.time) >= SM_MIN_INTERVAL_TO_GET_DATA) {
        is_good_result =
            this->put_command_data_(HEKR_CMD_SEND_GET_LIMIT_AND_PURCHASE, HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE,
                                    HEKR_SIZE_SEND_GET_DATA, HEKR_SIZE_RECEIVE_LIMIT_AND_PURCHASE, nullptr);

        ESP_LOGV(TAG, "In --- send_command - GET_LIMIT_AND_PURCHASE_DATA - Result = %s", TRUEFALSE(is_good_result));
      }

      break;
    }
    case SmCommandSend::GET_METER_ID: {
      is_good_result = this->put_command_data_(HEKR_CMD_SEND_GET_METER_ID, HEKR_CMD_RECEIVE_METER_ID,
                                               HEKR_SIZE_SEND_GET_DATA, HEKR_SIZE_RECEIVE_METER_ID, nullptr);

      ESP_LOGV(TAG, "In --- send_command - GET_METER_ID - Result = %s", TRUEFALSE(is_good_result));

      break;
    }
    case SmCommandSend::SET_LIMIT_DATA: {
      uint16_t tmp_current_limit = this->lp_data_.max_current_limit * 100;

      uint8_t send_array[6];

      send_array[0] = (tmp_current_limit >> 8);
      send_array[1] = (tmp_current_limit & SM_GET_ONE_BYTE);

      send_array[2] = (this->lp_data_.max_voltage_limit >> 8);
      send_array[3] = (this->lp_data_.max_voltage_limit & SM_GET_ONE_BYTE);

      send_array[4] = (this->lp_data_.min_voltage_limit >> 8);
      send_array[5] = (this->lp_data_.min_voltage_limit & SM_GET_ONE_BYTE);

      is_good_result = this->put_command_data_(HEKR_CMD_SEND_SET_LIMIT, HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE,
                                               HEKR_SIZE_SEND_LIMIT, HEKR_SIZE_RECEIVE_LIMIT_AND_PURCHASE, send_array);

      ESP_LOGD(TAG, "In --- send_command - SET_LIMIT_DATA - Result = %s", TRUEFALSE(is_good_result));
      ESP_LOGD(TAG, "* Input Data: max_current_limit = %u, max_voltage_limit = %u, min_voltage_limit = %u",
               this->lp_data_.max_current_limit, this->lp_data_.max_voltage_limit, this->lp_data_.min_voltage_limit);

      break;
    }
    case SmCommandSend::SET_PURCHASE_DATA: {
      uint32_t purchase_value = 0;
      uint32_t purchase_alarm = 0;

      if (state) {
        purchase_value = this->lp_data_.energy_purchase_value_tmp * 100;
        purchase_alarm = this->lp_data_.energy_purchase_alarm_tmp * 100;
      }

      uint8_t send_array[9];

      send_array[0] = (purchase_value >> 24);
      send_array[1] = (purchase_value >> 16);
      send_array[2] = (purchase_value >> 8);
      send_array[3] = (purchase_value & SM_GET_ONE_BYTE);

      send_array[4] = (purchase_alarm >> 24);
      send_array[5] = (purchase_alarm >> 16);
      send_array[6] = (purchase_alarm >> 8);
      send_array[7] = (purchase_alarm & SM_GET_ONE_BYTE);

      send_array[8] = state;

      is_good_result =
          this->put_command_data_(HEKR_CMD_SEND_SET_PURCHASE, HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE,
                                  HEKR_SIZE_SEND_PURCHASE, HEKR_SIZE_RECEIVE_LIMIT_AND_PURCHASE, send_array);

      ESP_LOGD(TAG, "In --- send_command - SET_PURCHASE_DATA - Result = %s", TRUEFALSE(is_good_result));
      ESP_LOGD(TAG, "* Input Data: purchase_value = %u, purchase_alarm = %u, state = %s",
               (state ? this->lp_data_.energy_purchase_value_tmp : 0),
               (state ? this->lp_data_.energy_purchase_alarm_tmp : 0), ONOFF(state));

      break;
    }
    case SmCommandSend::SET_POWER_STATE: {
      this->ms_data_.warning_off_by_user = !state;

      uint8_t send_array[1];

      send_array[0] = state;

      is_good_result = this->put_command_data_(HEKR_CMD_SEND_SET_METER_STATE, HEKR_CMD_RECEIVE_METER_STATE,
                                               HEKR_SIZE_SEND_METER_STATE, HEKR_SIZE_RECEIVE_METER_STATE, send_array);

      ESP_LOGD(TAG, "In --- send_command - SET_POWER_STATE - Result = %s", TRUEFALSE(is_good_result));
      ESP_LOGD(TAG, "* Input Data: state = %s", ONOFF(state));

      break;
    }
    case SmCommandSend::SET_DELAY: {
      uint16_t delay_value_set = 0;

      if (state)
        delay_value_set = this->ms_data_.delay_value_set;

      uint8_t send_array[3];

      send_array[0] = (delay_value_set >> 8);
      send_array[1] = (delay_value_set & SM_GET_ONE_BYTE);

      send_array[2] = state;

      is_good_result = this->put_command_data_(HEKR_CMD_SEND_SET_DELAY, HEKR_CMD_RECEIVE_METER_STATE,
                                               HEKR_SIZE_SEND_DELAY, HEKR_SIZE_RECEIVE_METER_STATE, send_array);

      ESP_LOGD(TAG, "In --- send_command - SET_DELAY - Result = %s", TRUEFALSE(is_good_result));
      ESP_LOGD(TAG, "* Input Data: delay_value_set = %u, state = %s", delay_value_set, ONOFF(state));

      break;
    }
    case SmCommandSend::SET_RESET: {
      is_good_result = this->put_command_data_(HEKR_CMD_SEND_SET_RESET, HEKR_CMD_RECEIVE_MEASUREMENT,
                                               HEKR_SIZE_SEND_RESET, HEKR_SIZE_RECEIVE_MEASUREMENT, nullptr);

      ESP_LOGD(TAG, "In --- send_command - SET_RESET - Result = %s", TRUEFALSE(is_good_result));

      break;
    }
  }

  return is_good_result;
}

//------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------

void Dxs238xwComponent::update_meter_state_detail_() {
  SmErrorMeterStateType tmp_meter_state_detail = SmErrorMeterStateType::UNKNOWN;

  if (this->ms_data_.meter_state) {
    tmp_meter_state_detail = SmErrorMeterStateType::POWER_OK;
  } else {
    if (this->ms_data_.warning_off_by_end_delay) {
      tmp_meter_state_detail = SmErrorMeterStateType::END_DELAY;
    } else if (this->ms_data_.warning_off_by_end_purchase) {
      tmp_meter_state_detail = SmErrorMeterStateType::END_PURCHASE;
    } else if (this->ms_data_.warning_off_by_over_voltage) {
      tmp_meter_state_detail = SmErrorMeterStateType::OVER_VOLTAGE;
    } else if (this->ms_data_.warning_off_by_under_voltage) {
      tmp_meter_state_detail = SmErrorMeterStateType::UNDER_VOLTAGE;
    } else if (this->ms_data_.warning_off_by_over_current) {
      tmp_meter_state_detail = SmErrorMeterStateType::OVER_CURRENT;
    } else if (this->ms_data_.warning_off_by_user) {
      tmp_meter_state_detail = SmErrorMeterStateType::END_BY_USER;
    }
  }

  if (tmp_meter_state_detail != this->ms_data_.meter_state_detail) {
    this->ms_data_.meter_state_detail = tmp_meter_state_detail;

    std::string meter_state_detail_string = this->get_meter_state_string_(tmp_meter_state_detail);
    UPDATE_TEXT_SENSOR(meter_state_detail, meter_state_detail_string)
  }
}

void Dxs238xwComponent::set_delay_state_() {
  if (this->ms_data_.warning_off_by_end_delay) {
    if (this->ms_data_.meter_state) {
      uint8_t send_meter_state_off[HEKR_SIZE_SEND_METER_STATE] = {0x48, 0x07, 0x02, 0x01, 0x09, 0x00, 0x5B};

      this->transmit_serial_data_(send_meter_state_off, HEKR_SIZE_SEND_METER_STATE);
    }

    if (this->ms_data_.delay_state && !this->ms_data_.meter_state) {
      uint8_t send_array_delay_off[HEKR_SIZE_SEND_DELAY] = {0x48, 0x09, 0x02, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x60};

      this->transmit_serial_data_(send_array_delay_off, HEKR_SIZE_SEND_DELAY);
    }
  }
}

bool Dxs238xwComponent::put_command_data_(uint8_t cmd_send, uint8_t cmd_receive, uint8_t frame_size_send,
                                          uint8_t frame_size_receive, uint8_t *array) {
  this->error_type_ = SmErrorType::NO_ERROR;
  this->error_code_ = SmErrorCode::NO_ERROR;

  if (this->pre_transmit_serial_data_(cmd_send, frame_size_send, array)) {
    if (this->pre_receive_serial_data_(cmd_receive, frame_size_receive)) {
      return true;
    }
  }

  this->print_error_();

  return false;
}

bool Dxs238xwComponent::incoming_messages_(bool print_error) {
  uint8_t index = 0;

  uint8_t incoming_byte_message[SM_MAX_BYTE_MSG_BUFFER];

  while (available() > 0) {
    incoming_byte_message[index] = read();

    delay(2);  // ESP is more quickly

    index++;

    if (index == SM_MAX_BYTE_MSG_BUFFER) {
      break;
    }
  }

  if (index == 0) {
    return true;  // 0 is not error
  }

  ESP_LOGI(TAG, "* Incoming message arrived: %s", format_hex_pretty(incoming_byte_message, index).c_str());

  this->error_type_ = SmErrorType::NO_ERROR;
  this->error_code_ = SmErrorCode::NO_ERROR;

  if (incoming_byte_message[0] == HEKR_HEADER) {
    if (this->calculate_crc_(incoming_byte_message, index) != incoming_byte_message[index - 1]) {
      this->error_type_ = SmErrorType::COMMUNICATION;
      this->error_code_ = SmErrorCode::CRC;
    } else {
      this->process_and_update_data_(incoming_byte_message);

      return true;
    }
  } else {
    this->error_type_ = SmErrorType::COMMUNICATION;
    this->error_code_ = SmErrorCode::WRONG_BYTES;
  }

  if (print_error) {
    this->print_error_();
  }

  return false;
}

std::string Dxs238xwComponent::get_delay_value_remaining_string_(uint16_t value) {
  uint32_t seconds = value * 60;
  uint16_t days = seconds / (24 * 3600);
  seconds = seconds % (24 * 3600);
  uint8_t hours = seconds / 3600;
  seconds = seconds % 3600;
  uint8_t minutes = seconds / 60;

  return ((days ? to_string(days) + "d " : "") + (hours ? to_string(hours) + "h " : "") + (to_string(minutes) + "m"));
}

std::string Dxs238xwComponent::get_meter_state_string_(SmErrorMeterStateType error) {
  switch (error) {
    case SmErrorMeterStateType::POWER_OK:
      return SM_STR_POWER_STATE_DETAILS_POWER_OK;
      break;
    case SmErrorMeterStateType::OVER_VOLTAGE:
      return SM_STR_POWER_STATE_DETAILS_OVER_VOLTAGE;
      break;
    case SmErrorMeterStateType::UNDER_VOLTAGE:
      return SM_STR_POWER_STATE_DETAILS_UNDER_VOLTAGE;
      break;
    case SmErrorMeterStateType::OVER_CURRENT:
      return SM_STR_POWER_STATE_DETAILS_OVER_CURRENT;
      break;
    case SmErrorMeterStateType::END_PURCHASE:
      return SM_STR_POWER_STATE_DETAILS_END_PURCHASE;
      break;
    case SmErrorMeterStateType::END_DELAY:
      return SM_STR_POWER_STATE_DETAILS_END_DELAY;
      break;
    case SmErrorMeterStateType::END_BY_USER:
      return SM_STR_POWER_STATE_DETAILS_END_BY_USER;
      break;
    case SmErrorMeterStateType::UNKNOWN:
      return SM_STR_POWER_STATE_DETAILS_UNKNOWN;
      break;
  }

  return SM_STR_POWER_STATE_DETAILS_UNKNOWN;
}

uint8_t Dxs238xwComponent::calculate_crc_(const uint8_t *array, uint8_t size) {
  uint16_t tmp_crc = 0;
  uint8_t tmp_size = size - 1;

  for (uint8_t n = 0; n < tmp_size; n++) {
    tmp_crc = tmp_crc + array[n];
  }

  return tmp_crc & SM_GET_ONE_BYTE;
}

void Dxs238xwComponent::print_error_() {
  std::string string_type;
  std::string string_code;

  switch (this->error_type_) {
    case SmErrorType::NO_ERROR:
      string_type = SM_STR_TYPE_NO_ERROR;
      break;
    case SmErrorType::COMMUNICATION:
      string_type = SM_STR_TYPE_COMUNICATION;
      break;
    case SmErrorType::INPUT_DATA:
      string_type = SM_STR_TYPE_INPUT_DATA;
      break;
  }

  switch (this->error_code_) {
    case SmErrorCode::NO_ERROR:
      string_code = SM_STR_CODE_NO_ERROR;
      break;
    case SmErrorCode::CRC:
      string_code = SM_STR_CODE_CRC;
      break;
    case SmErrorCode::WRONG_BYTES:
      string_code = SM_STR_CODE_WRONG_BYTES;
      break;
    case SmErrorCode::NOT_ENOUGHT_BYTES:
      string_code = SM_STR_CODE_NOT_ENOUGH_BYTES;
      break;
    case SmErrorCode::EXCEEDS_BYTES:
      string_code = SM_STR_CODE_EXCEEDED_BYTES;
      break;
    case SmErrorCode::TIMEOUT:
      string_code = SM_STR_CODE_TIMED_OUT;
      break;
    case SmErrorCode::WRONG_MSG:
      string_code = SM_STR_CODE_WRONG_MSG;
      break;
    case SmErrorCode::MESSAGE_LENGTH:
      string_code = SM_STR_CODE_MESSAGE_LENGTH;
      break;
  }

  ESP_LOGE(TAG, "* Error, Type: %s, Description: %s", string_type.c_str(), string_code.c_str());

  this->error_type_ = SmErrorType::NO_ERROR;
  this->error_code_ = SmErrorCode::NO_ERROR;
}

void Dxs238xwComponent::load_initial_number_value_(ESPPreferenceObject &preference, const std::string &preference_name,
                                                   uint32_t *value_store) {
  preference = global_preferences->make_preference<uint32_t>(fnv1_hash(preference_name));

  uint32_t initial_state;

  if (!preference.load(&initial_state)) {
    ESP_LOGE(TAG, "* Error load store number: %s, return default value: %u", preference_name.c_str(), *value_store);

    this->save_initial_number_value_(preference, value_store);
  }

  ESP_LOGD(TAG, "* Load store number: %s, Return value: %u", preference_name.c_str(), initial_state);

  *value_store = initial_state;
}

void Dxs238xwComponent::save_initial_number_value_(ESPPreferenceObject &preference, const uint32_t *value) {
  if (preference.save(value)) {
    ESP_LOGD(TAG, "* Save number value: %u", *value);
  } else {
    ESP_LOGE(TAG, "* Error save number value: %u", *value);
  }
}

}  // namespace dxs238xw
}  // namespace esphome
