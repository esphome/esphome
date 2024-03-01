#include "air_technic_hru.h"

#include "esphome/core/log.h"

namespace esphome {
namespace air_technic_hru {

static const char *const TAG = "air_technic_hru";

static constexpr uint8_t MODBUS_CMD_READ_REGISTER = 3;
static constexpr uint8_t MODBUS_CMD_WRITE_REGISTER = 6;

/****************** SWITCH *****************/
void AirTechnicHRUSwitch::dump_config() { LOG_SWITCH(TAG, " Switch", this); }

void AirTechnicHRUSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->toggle_switch(this->id_, state);
}

/****************** NUMBER *****************/
void AirTechnicHRUNumber::dump_config() { LOG_NUMBER(TAG, " Number", this); }

void AirTechnicHRUNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_number(this->id_, value);
}

/****************** BUTTON *****************/
void AirTechnicHRUButton::dump_config() { LOG_BUTTON(TAG, " Button", this); }

void AirTechnicHRUButton::press_action() { this->parent_->press_button(this->id_); }

/****************** SELECT ****************/
void AirTechnicHRUSelect::dump_config() { LOG_SELECT(TAG, " Select", this); }

void AirTechnicHRUSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_select(this->id_, value);
}

/****************** HRU *****************/
uint16_t AirTechnicHru::data_to_uint16(const std::vector<uint8_t> &data) {
  if (data.size() != 2) {
    ESP_LOGW(TAG, "Tried to convert invalid data to unt16");
    return 0;
  }

  return ((static_cast<int16_t>(data[0])) << 8) | data[1];
}

float AirTechnicHru::data_to_temperature(const std::vector<uint8_t> &data) { return this->data_to_uint16(data) - 40; }

bool AirTechnicHru::data_to_bool(const std::vector<uint8_t> &data) { return this->data_to_uint16(data) == 1; }

uint16_t AirTechnicHru::temperature_to_uint16(float temperature) { return temperature + 40; }

void AirTechnicHru::calculate_supply_exhaust_ratio() {
  if (this->exhaust_fan_speed_ == 0) {
    supply_exhaust_ratio_ = -1;
    return;
  }

  if (this->supply_fan_speed_ == 0) {
    supply_exhaust_ratio_ = 1;
    return;
  }

  if (this->supply_fan_speed_ < this->exhaust_fan_speed_) {
    supply_exhaust_ratio_ = 1 - (static_cast<float>(this->supply_fan_speed_) / this->exhaust_fan_speed_);
    return;
  }

  supply_exhaust_ratio_ = (static_cast<float>(this->exhaust_fan_speed_) / this->supply_fan_speed_) - 1;
}

void AirTechnicHru::calculate_target_supply_exhaust_speeds() {
  if (this->supply_exhaust_ratio_ == 0) {
    this->supply_fan_speed_ = this->speed;
    this->exhaust_fan_speed_ = this->speed;
    return;
  }

  if (this->supply_exhaust_ratio_ > 0) {
    this->supply_fan_speed_ = 0.9 + this->speed * (1 - this->supply_exhaust_ratio_);
    this->exhaust_fan_speed_ = this->speed;
    return;
  }

  this->supply_fan_speed_ = this->speed;
  this->exhaust_fan_speed_ = 0.9 + this->speed * (this->supply_exhaust_ratio_ + 1);
}

void AirTechnicHru::process_flag_data(const std::vector<uint8_t> &data) {
  auto dataInt = this->data_to_uint16(data);

  if (this->fire_alarm_sensor_ != nullptr) {
    this->fire_alarm_sensor_->publish_state(dataInt & 1U);
  }

  if (this->bypass_open_sensor_ != nullptr) {
    this->bypass_open_sensor_->publish_state((dataInt >> 1) & 1U);
  }

  if (this->defrosting_sensor_ != nullptr) {
    this->defrosting_sensor_->publish_state((dataInt >> 3) & 1U);
  }
}

void AirTechnicHru::on_modbus_data(const std::vector<uint8_t> &data) {
  if (this->modbus_send_queue.empty()) {
    if (data.size() == 4) {
      auto reg = static_cast<Register>(this->data_to_uint16({data[0], data[1]}));
      auto val = this->data_to_uint16({data[2], data[3]});
      ESP_LOGD(TAG, "Received confirmation register %d value %u (0x%02X%02X)", static_cast<int>(reg), val, data[2],
               data[3]);

      this->modbus_send_queue.push(reg);
      this->process_register_queue({data[2], data[3]});

      if (reg == Register::OnOff) {
        this->modbus_write_uint16(Register::ExhaustFanSpeed, this->exhaust_fan_speed_);
      } else if (reg == Register::ExhaustFanSpeed) {
        this->modbus_write_uint16(Register::SupplyFanSpeed, this->supply_fan_speed_);
      }
    }

  } else {
    this->process_register_queue(data);
  }
}

void AirTechnicHru::process_register_queue(const std::vector<uint8_t> &data) {
  switch (this->modbus_send_queue.front()) {
    case Register::RoomTemperature:
      if (this->room_temperature_sensor_ != nullptr) {
        this->room_temperature_sensor_->publish_state(this->data_to_temperature(data));
      }
      break;

    case Register::OutdoorTemperature:
      if (this->outdoor_temperature_sensor_ != nullptr) {
        this->outdoor_temperature_sensor_->publish_state(this->data_to_temperature(data));
      }
      break;

    case Register::SupplyAirTemperature:
      if (this->supply_air_temperature_sensor_ != nullptr) {
        this->supply_air_temperature_sensor_->publish_state(this->data_to_temperature(data));
      }
      break;

    case Register::OnOff:
      this->state = data_to_bool(data);
      this->publish_state();
      break;

    case Register::PowerRestore:
      if (this->power_restore_switch_ != nullptr) {
        this->power_restore_switch_->publish_state(this->data_to_bool(data));
      }
      break;

    case Register::HeaterValidOrInvalid:
      if (this->heater_installed_switch != nullptr) {
        this->heater_installed_switch->publish_state(this->data_to_bool(data));
      }
      break;

    case Register::SupplyFanSpeed:
      this->supply_fan_speed_ = this->data_to_uint16(data);
      if (this->supply_fan_speed_sensor_ != nullptr) {
        this->supply_fan_speed_sensor_->publish_state(this->supply_fan_speed_);
      }
      break;

    case Register::ExhaustFanSpeed:
      this->exhaust_fan_speed_ = this->data_to_uint16(data);
      if (this->exhaust_fan_speed_sensor_ != nullptr) {
        this->exhaust_fan_speed_sensor_->publish_state(this->exhaust_fan_speed_);
      }
      break;

    case Register::BypassOpenTemperature:
      if (this->bypass_open_temperature_number_ != nullptr) {
        this->bypass_open_temperature_number_->publish_state(this->data_to_uint16(data));
      }
      break;

    case Register::BypassOpenTemperatureRange:
      if (this->bypass_temperature_range_number_ != nullptr) {
        this->bypass_temperature_range_number_->publish_state(this->data_to_uint16(data));
      }
      break;

    case Register::DefrostingIntervalTime:
      if (this->defrost_interval_time_number_ != nullptr) {
        this->defrost_interval_time_number_->publish_state(this->data_to_uint16(data));
      }
      break;

    case Register::DefrostingEnteringTemperature:
      if (this->defrost_start_temperature_number_ != nullptr) {
        this->defrost_start_temperature_number_->publish_state(this->data_to_temperature(data));
      }
      break;

    case Register::DefrostingDurationTime:
      if (this->defrost_duration_number_ != nullptr) {
        this->defrost_duration_number_->publish_state(this->data_to_uint16(data));
      }
      break;

    case Register::DefrostingTemperature:
      if (this->defrost_temperature_sensor_ != nullptr) {
        this->defrost_temperature_sensor_->publish_state(this->data_to_temperature(data));
      }
      break;

    case Register::ExternalOnOffSignal:
      if (this->external_control_switch_ != nullptr) {
        this->external_control_switch_->publish_state(this->data_to_bool(data));
      }
      break;

    case Register::CO2SensorOnOffSignal:
      if (this->co2_sensor_installed_ != nullptr) {
        this->co2_sensor_installed_->publish_state(this->data_to_bool(data));
      }
      break;

    case Register::CO2Value:
      if (this->co2_sensor_ != nullptr) {
        this->co2_sensor_->publish_state(this->data_to_uint16(data));
      }
      break;

    case Register::FanHourCount:
      if (this->filter_hour_count_sensor_ != nullptr) {
        this->filter_hour_count_sensor_->publish_state(this->data_to_uint16(data) * 0.1);
      }
      break;

    case Register::Flags:
      this->process_flag_data(data);
      break;

    case Register::FilterAlarmTimer:
      if (this->filter_alarm_interval_select_ != nullptr) {
        auto state = this->filter_alarm_interval_select_->at(this->data_to_uint16(data));
        this->filter_alarm_interval_select_->publish_state(state.value_or(""));
      }
      break;

    default:
      break;
  }

  this->modbus_send_queue.pop();
  this->read_next_register();
}

void AirTechnicHru::control(const fan::FanCall &call) {
  auto state = call.get_state();
  if (state.has_value()) {
    this->state = state.value();
    this->publish_state();
  }

  auto speed = call.get_speed();
  if (speed.has_value()) {
    this->speed = speed.value();
    this->publish_state();

    this->calculate_target_supply_exhaust_speeds();
  }

  this->modbus_write_bool(Register::OnOff, this->state);
}

void AirTechnicHru::update() {
  this->clear_modbus_send_queue();

  if (this->supply_exhaust_ratio_number_ != nullptr) {
    this->calculate_supply_exhaust_ratio();
    this->supply_exhaust_ratio_number_->publish_state(this->supply_exhaust_ratio_);
  }

  this->speed =
      (this->exhaust_fan_speed_ > this->supply_fan_speed_) ? this->exhaust_fan_speed_ : this->supply_fan_speed_;
  this->publish_state();

  this->modbus_send_queue.push(Register::PowerRestore);
  this->modbus_send_queue.push(Register::HeaterValidOrInvalid);
  this->modbus_send_queue.push(Register::BypassOpenTemperature);
  this->modbus_send_queue.push(Register::BypassOpenTemperatureRange);
  this->modbus_send_queue.push(Register::DefrostingIntervalTime);
  this->modbus_send_queue.push(Register::DefrostingEnteringTemperature);
  this->modbus_send_queue.push(Register::DefrostingDurationTime);
  this->modbus_send_queue.push(Register::OnOff);
  this->modbus_send_queue.push(Register::SupplyFanSpeed);
  this->modbus_send_queue.push(Register::ExhaustFanSpeed);
  this->modbus_send_queue.push(Register::RoomTemperature);
  this->modbus_send_queue.push(Register::OutdoorTemperature);
  this->modbus_send_queue.push(Register::SupplyAirTemperature);
  this->modbus_send_queue.push(Register::DefrostingTemperature);
  this->modbus_send_queue.push(Register::ExternalOnOffSignal);
  this->modbus_send_queue.push(Register::CO2SensorOnOffSignal);
  this->modbus_send_queue.push(Register::CO2Value);
  this->modbus_send_queue.push(Register::Flags);
  this->modbus_send_queue.push(Register::FanHourCount);
  this->modbus_send_queue.push(Register::FilterAlarmTimer);

  this->read_next_register();
}

void AirTechnicHru::read_next_register() {
  if (this->modbus_send_queue.empty()) {
    return;
  }

  this->send(MODBUS_CMD_READ_REGISTER, static_cast<uint16_t>(this->modbus_send_queue.front()), 1);
}

void AirTechnicHru::modbus_write_bool(AirTechnicHru::Register reg, bool val) {
  uint8_t data[2] = {0};
  data[1] = val & 1;
  this->send(MODBUS_CMD_WRITE_REGISTER, static_cast<uint16_t>(reg), 1, 2, data);

  this->clear_modbus_send_queue();
}

void AirTechnicHru::modbus_write_uint16(AirTechnicHru::Register reg, uint16_t val) {
  ESP_LOGW(TAG, "Writing register %u %u", static_cast<uint16_t>(reg), val);
  uint8_t data[2] = {0};
  data[0] = val >> 8;
  data[1] = val & 0xFF;
  this->send(MODBUS_CMD_WRITE_REGISTER, static_cast<uint16_t>(reg), 1, 2, data);

  this->clear_modbus_send_queue();
}

void AirTechnicHru::toggle_switch(const std::string &id, bool state) {
  if (id == "heater_installed") {
    this->modbus_write_bool(Register::HeaterValidOrInvalid, state);
  } else if (id == "start_after_power_loss") {
    this->modbus_write_bool(Register::PowerRestore, state);
  } else if (id == "co2_sensor_installed") {
    this->modbus_write_bool(Register::CO2SensorOnOffSignal, state);
  } else if (id == "external_control") {
    this->modbus_write_bool(Register::ExternalOnOffSignal, state);
  }
}

void AirTechnicHru::set_number(const std::string &id, float value) {
  if (id == "bypass_open_temperature") {
    this->modbus_write_uint16(Register::BypassOpenTemperature, value);
  } else if (id == "bypass_temperature_range") {
    this->modbus_write_uint16(Register::BypassOpenTemperatureRange, value);
  } else if (id == "defrost_interval_time") {
    this->modbus_write_uint16(Register::DefrostingIntervalTime, value);
  } else if (id == "defrost_start_temperature") {
    this->modbus_write_uint16(Register::DefrostingEnteringTemperature, this->temperature_to_uint16(value));
  } else if (id == "defrost_duration") {
    this->modbus_write_uint16(Register::DefrostingDurationTime, value);
  } else if (id == "supply_exhaust_ratio") {
    this->supply_exhaust_ratio_ = value;
    this->calculate_target_supply_exhaust_speeds();
    this->modbus_write_uint16(Register::ExhaustFanSpeed, this->exhaust_fan_speed_);
  }
}

void AirTechnicHru::press_button(const std::string &id) {
  if (id == "reset_filter_hours") {
    this->modbus_write_uint16(Register::MultifunctionSettings, 1);
  }
}

void AirTechnicHru::set_select(const std::string &id, const std::string &option) {
  if (id == "filter_alarm_interval") {
    auto index = this->filter_alarm_interval_select_->index_of(option);
    if (index.has_value()) {
      this->modbus_write_uint16(Register::FilterAlarmTimer, index.value());
    }
  }
}

void AirTechnicHru::clear_modbus_send_queue() {
  while (!this->modbus_send_queue.empty()) {
    this->modbus_send_queue.pop();
  }
}

void AirTechnicHru::dump_config() {
  ESP_LOGCONFIG(TAG, "AirTechnicHru:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_FAN(TAG, " Fan", this);
}

}  // namespace air_technic_hru
}  // namespace esphome
