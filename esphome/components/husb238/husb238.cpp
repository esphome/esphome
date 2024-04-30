#include "husb238.h"
#include "esphome/core/log.h"

namespace esphome {
namespace husb238 {

static const char *const TAG = "husb238";

static float current_to_float(SrcCurrent current) {
  switch (current) {
    case SrcCurrent::I_0_5_A:
      return 0.5f;
    case SrcCurrent::I_0_7_A:
      return 0.7f;
    case SrcCurrent::I_1_0_A:
      return 1.0f;
    case SrcCurrent::I_1_25_A:
      return 1.25f;
    case SrcCurrent::I_1_5_A:
      return 1.5f;
    case SrcCurrent::I_1_75_A:
      return 1.75f;
    case SrcCurrent::I_2_0_A:
      return 2.0f;
    case SrcCurrent::I_2_25_A:
      return 2.25f;
    case SrcCurrent::I_2_5_A:
      return 2.5f;
    case SrcCurrent::I_2_75_A:
      return 2.75f;
    case SrcCurrent::I_3_0_A:
      return 3.0f;
    case SrcCurrent::I_3_25_A:
      return 3.25f;
    case SrcCurrent::I_3_5_A:
      return 3.5f;
    case SrcCurrent::I_4_0_A:
      return 4.0f;
    case SrcCurrent::I_4_5_A:
      return 4.5f;
    case SrcCurrent::I_5_0_A:
      return 5.0f;
    default:
      return 0.0f;
  }
}

static float current5v_to_float(SrcCurrent5V current) {
  switch (current) {
    case SrcCurrent5V::I_0_5_A:
      return 0.5f;
    case SrcCurrent5V::I_1_5_A:
      return 1.5f;
    case SrcCurrent5V::I_2_4_A:
      return 2.4f;
    case SrcCurrent5V::I_3_0_A:
      return 3.0f;
    default:
      return 0.0f;
  }
}

static float voltage_to_float(SrcVoltage voltage) {
  switch (voltage) {
    case SrcVoltage::PD_5V:
      return 5.0f;
    case SrcVoltage::PD_9V:
      return 9.0f;
    case SrcVoltage::PD_12V:
      return 12.0f;
    case SrcVoltage::PD_15V:
      return 15.0f;
    case SrcVoltage::PD_18V:
      return 18.0f;
    case SrcVoltage::PD_20V:
      return 20.0f;
    default:
      return 0.0f;
  }
}

static float selected_voltage_to_float(SrcVoltageSelection voltage) {
  switch (voltage) {
    case SrcVoltageSelection::SRC_PDO_5V:
      return 5.0f;
    case SrcVoltageSelection::SRC_PDO_9V:
      return 9.0f;
    case SrcVoltageSelection::SRC_PDO_12V:
      return 12.0f;
    case SrcVoltageSelection::SRC_PDO_15V:
      return 15.0f;
    case SrcVoltageSelection::SRC_PDO_18V:
      return 18.0f;
    case SrcVoltageSelection::SRC_PDO_20V:
      return 20.0f;
    default:
      return 0.0f;
  }
}

static const char *status_to_string(PdResponse status) {
  switch (status) {
    case PdResponse::NO_RESPONSE:
      return "No response";
    case PdResponse::SUCCESS:
      return "Success";
    case PdResponse::INVALID_COMMAND:
      return "Invalid command";
    case PdResponse::COMMAND_NOT_SUPPORTED:
      return "Command not supported";
    case PdResponse::TRANSACTION_FAIL:
      return "Transaction fail";
    default:
      return "Unknown";
  }
}

void Husb238Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HUSB238...");
  this->command_get_src_cap();
  this->read_all_();
}

void Husb238Component::loop() {
  if (!this->is_ready()) {
    return;
  }

  this->read_all_();
}

void Husb238Component::update() {
  if (!this->is_ready()) {
    return;
  }
  this->read_all_();

#ifdef BINARY_SENSOR
  if (this->attached_binary_sensor_ != nullptr) {
    this->attached_binary_sensor_->publish_state(this->registers_.pd_status1.attached);
  }
  if (this->cc_direction_binary_sensor_ != nullptr) {
    this->cc_direction_binary_sensor_->publish_state(this->registers_.pd_status1.cc_dir);
  }
#endif
#ifdef USE_SENSOR
  if (this->current_sensor_ != nullptr) {
    this->current_sensor_->publish_state(current_to_float(this->registers_.pd_status0.current));
  }
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(voltage_to_float(this->registers_.pd_status0.voltage));
  }
  if (this->selected_voltage_sensor_ != nullptr) {
    this->selected_voltage_sensor_->publish_state(selected_voltage_to_float(this->registers_.src_pdo_sel.voltage));
  }
  if (this->voltage5v_sensor_ != nullptr) {
    this->voltage5v_sensor_->publish_state(this->registers_.pd_status1.voltage_5v ? 5.0f : 0.0f);
  }
  if (this->current5v_sensor_ != nullptr) {
    this->current5v_sensor_->publish_state(current5v_to_float(this->registers_.pd_status1.current_5v));
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (this->status_text_sensor_ != nullptr) {
    this->status_text_sensor_->publish_state(status_to_string(this->registers_.pd_status1.response));
  }
#endif
}

void Husb238Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HUSB238:");

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "PD Attached", this->attached_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "CC Direction", this->cc_direction_binary_sensor_);
#endif

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Source Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Source Current", this->current_sensor_);
  LOG_SENSOR("  ", "Selected Voltage", this->selected_voltage_sensor_);
  LOG_SENSOR("  ", "5V Voltage", this->voltage5v_sensor_);
  LOG_SENSOR("  ", "5V Current", this->current5v_sensor_);
#endif

#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Last Request Status", this->status_text_sensor_);
#endif
}

bool Husb238Component::command_request_pdo(SrcVoltageSelection voltage) {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Component not ready");
    return false;
  }

  if (!this->select_pdo_voltage_(voltage)) {
    return false;
  }

  return this->send_command_(CommandFunction::REQUEST_PDO);
}

bool Husb238Component::is_attached() {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Component not ready");
    return false;
  }
  return this->registers_.pd_status1.attached;
}

bool Husb238Component::read_all_() {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Component not ready");
    return false;
  }
  auto err = this->read_bytes(static_cast<uint8_t>(CommandRegister::PD_STATUS0), &this->registers_.raw[0], REG_NUM);
  if (err) {
    ESP_LOGE(TAG, "Error reading HUSB238");
  }
  return err;
}

bool Husb238Component::send_command_(CommandFunction function) {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Component not ready");
    return false;
  }
  RegGoCommand go_command;
  auto err = this->read_byte(static_cast<uint8_t>(CommandRegister::GO_COMMAND), &go_command.raw);
  if (err) {
    ESP_LOGE(TAG, "Error reading HUSB238");
    return false;
  }
  go_command.function = function;
  err = this->write_byte(static_cast<uint8_t>(CommandRegister::GO_COMMAND), go_command.raw);
  if (err) {
    ESP_LOGE(TAG, "Error sending command to HUSB238");
  }
  return err;
}

RegSrcPdo Husb238Component::get_detected_current_() {
  RegSrcPdo pdo_default_data;
  pdo_default_data.current = SrcCurrent::I_0_5_A;
  pdo_default_data.detected = false;
  pdo_default_data.reserved = 0;

  if (!this->is_ready() || this->registers_.src_pdo_sel.voltage == SrcVoltageSelection::NOT_SELECTED) {
    ESP_LOGE(TAG, "No voltage selected or component not ready");
    return pdo_default_data;
  };

  if (!this->read_all_()) {
    return pdo_default_data;
  };

  switch (this->registers_.src_pdo_sel.voltage) {
    case SrcVoltageSelection::SRC_PDO_5V:
      return this->registers_.src_pdo_5v;
    case SrcVoltageSelection::SRC_PDO_9V:
      return this->registers_.src_pdo_9v;
    case SrcVoltageSelection::SRC_PDO_12V:
      return this->registers_.src_pdo_12v;
    case SrcVoltageSelection::SRC_PDO_15V:
      return this->registers_.src_pdo_15v;
    case SrcVoltageSelection::SRC_PDO_18V:
      return this->registers_.src_pdo_18v;
    case SrcVoltageSelection::SRC_PDO_20V:
      return this->registers_.src_pdo_20v;
    default:
      ESP_LOGW(TAG, "Invalid or no voltage selected");
      return pdo_default_data;
  }
}

bool Husb238Component::select_pdo_voltage_(SrcVoltageSelection voltage) {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Component not ready");
    return false;
  }

  this->registers_.src_pdo_sel.voltage = voltage;
  auto err = this->write_byte(static_cast<uint8_t>(CommandRegister::SRC_PDO), this->registers_.src_pdo_sel.raw);
  if (err) {
    ESP_LOGE(TAG, "Error setting PDO voltage");
  }
  return err;
}

}  // namespace husb238
}  // namespace esphome
