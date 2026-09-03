#include "hub.h"
#include <algorithm>
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42";

// clang-format off
const SimpleSensorInfo OpenTherm42Hub::SIMPLE_SENSORS[] = {
    {RequestKind::RELATIVE_MODULATION_LEVEL, 17, SimpleValueKind::F88, &OpenTherm42Hub::relative_modulation_level_sensor_, "Relative Modulation Level (id=17)"},
    {RequestKind::CH_WATER_PRESSURE, 18, SimpleValueKind::F88, &OpenTherm42Hub::ch_water_pressure_sensor_, "CH water pressure (id=18)"},
    {RequestKind::DHW_FLOW_RATE, 19, SimpleValueKind::F88, &OpenTherm42Hub::dhw_flow_rate_sensor_, "DHW flow rate (id=19)"},
    {RequestKind::BOILER_WATER_TEMPERATURE, 25, SimpleValueKind::F88, &OpenTherm42Hub::boiler_water_temperature_sensor_, "Boiler water temp. (id=25)"},
    {RequestKind::DHW_TEMPERATURE, 26, SimpleValueKind::F88, &OpenTherm42Hub::dhw_temperature_sensor_, "DHW temperature (id=26)"},
    {RequestKind::RETURN_WATER_TEMPERATURE, 28, SimpleValueKind::F88, &OpenTherm42Hub::return_water_temperature_sensor_, "Return water temperature (id=28)"},
    {RequestKind::SOLAR_STORAGE_TEMPERATURE, 29, SimpleValueKind::F88, &OpenTherm42Hub::solar_storage_temperature_sensor_, "Solar storage temperature (id=29)"},
    {RequestKind::SOLAR_COLLECTOR_TEMPERATURE, 30, SimpleValueKind::S16, &OpenTherm42Hub::solar_collector_temperature_sensor_, "Solar collector temperature (id=30)"},
    {RequestKind::FLOW_TEMPERATURE_CH2, 31, SimpleValueKind::F88, &OpenTherm42Hub::flow_temperature_ch2_sensor_, "Flow temperature CH2 (id=31)"},
    {RequestKind::DHW2_TEMPERATURE, 32, SimpleValueKind::F88, &OpenTherm42Hub::dhw2_temperature_sensor_, "DHW2 temperature (id=32)"},
    {RequestKind::EXHAUST_TEMPERATURE, 33, SimpleValueKind::S16, &OpenTherm42Hub::exhaust_temperature_sensor_, "Exhaust temperature (id=33)"},
    {RequestKind::BOILER_HEAT_EXCHANGER_TEMPERATURE, 34, SimpleValueKind::F88, &OpenTherm42Hub::boiler_heat_exchanger_temperature_sensor_, "Boiler heat exchanger temperature (id=34)"},
    {RequestKind::FLAME_CURRENT, 36, SimpleValueKind::F88, &OpenTherm42Hub::flame_current_sensor_, "Flame current (id=36)"},
    {RequestKind::RELATIVE_VENTILATION, 77, SimpleValueKind::U8_LB, &OpenTherm42Hub::relative_ventilation_sensor_, "Relative ventilation (id=77)"},
    {RequestKind::SUPPLY_INLET_TEMPERATURE, 80, SimpleValueKind::F88, &OpenTherm42Hub::supply_inlet_temperature_sensor_, "Supply inlet temperature (id=80)"},
    {RequestKind::SUPPLY_OUTLET_TEMPERATURE, 81, SimpleValueKind::F88, &OpenTherm42Hub::supply_outlet_temperature_sensor_, "Supply outlet temperature (id=81)"},
    {RequestKind::EXHAUST_INLET_TEMPERATURE, 82, SimpleValueKind::F88, &OpenTherm42Hub::exhaust_inlet_temperature_sensor_, "Exhaust inlet temperature (id=82)"},
    {RequestKind::EXHAUST_OUTLET_TEMPERATURE, 83, SimpleValueKind::F88, &OpenTherm42Hub::exhaust_outlet_temperature_sensor_, "Exhaust outlet temperature (id=83)"},
    {RequestKind::ACTUAL_EXHAUST_FAN_SPEED, 84, SimpleValueKind::U16, &OpenTherm42Hub::actual_exhaust_fan_speed_sensor_, "Actual exhaust fan speed (id=84)"},
    {RequestKind::ACTUAL_INLET_FAN_SPEED, 85, SimpleValueKind::U16, &OpenTherm42Hub::actual_inlet_fan_speed_sensor_, "Actual inlet fan speed (id=85)"},
    {RequestKind::COOLING_OPERATION_HOURS, 96, SimpleValueKind::U16, &OpenTherm42Hub::cooling_operation_hours_sensor_, "Cooling Operation hours (id=96)"},
    {RequestKind::POWER_CYCLES, 97, SimpleValueKind::U16, &OpenTherm42Hub::power_cycles_sensor_, "Power Cycles (id=97)"},
    {RequestKind::ELECTRICITY_PRODUCER_STARTS, 109, SimpleValueKind::U16, &OpenTherm42Hub::electricity_producer_starts_sensor_, "Electricity producer starts (id=109)"},
    {RequestKind::ELECTRICITY_PRODUCER_HOURS, 110, SimpleValueKind::U16, &OpenTherm42Hub::electricity_producer_hours_sensor_, "Electricity producer hours (id=110)"},
    {RequestKind::ELECTRICITY_PRODUCTION, 111, SimpleValueKind::U16, &OpenTherm42Hub::electricity_production_sensor_, "Electricity production (id=111)"},
    {RequestKind::CUMULATIVE_ELECTRICITY_PRODUCTION, 112, SimpleValueKind::U16, &OpenTherm42Hub::cumulative_electricity_production_sensor_, "Cumulative Electricity production (id=112)"},
    {RequestKind::NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS, 113, SimpleValueKind::U16, &OpenTherm42Hub::number_of_unsuccessful_burner_starts_sensor_, "Number of un-successful burner starts (id=113)"},
    {RequestKind::NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW, 114, SimpleValueKind::U16, &OpenTherm42Hub::number_of_times_flame_signal_too_low_sensor_, "Number of times flame signal was too low (id=114)"},
    {RequestKind::SUCCESSFUL_BURNER_STARTS, 116, SimpleValueKind::U16, &OpenTherm42Hub::successful_burner_starts_sensor_, "Successful Burner starts (id=116)"},
    {RequestKind::CH_PUMP_STARTS, 117, SimpleValueKind::U16, &OpenTherm42Hub::ch_pump_starts_sensor_, "CH pump starts (id=117)"},
    {RequestKind::DHW_PUMP_VALVE_STARTS, 118, SimpleValueKind::U16, &OpenTherm42Hub::dhw_pump_valve_starts_sensor_, "DHW pump/valve starts (id=118)"},
    {RequestKind::DHW_BURNER_STARTS, 119, SimpleValueKind::U16, &OpenTherm42Hub::dhw_burner_starts_sensor_, "DHW burner starts (id=119)"},
    {RequestKind::BURNER_OPERATION_HOURS, 120, SimpleValueKind::U16, &OpenTherm42Hub::burner_operation_hours_sensor_, "Burner operation hours (id=120)"},
    {RequestKind::CH_PUMP_OPERATION_HOURS, 121, SimpleValueKind::U16, &OpenTherm42Hub::ch_pump_operation_hours_sensor_, "CH pump operation hours (id=121)"},
    {RequestKind::DHW_PUMP_VALVE_OPERATION_HOURS, 122, SimpleValueKind::U16, &OpenTherm42Hub::dhw_pump_valve_operation_hours_sensor_, "DHW pump/valve operation hours (id=122)"},
    {RequestKind::DHW_BURNER_OPERATION_HOURS, 123, SimpleValueKind::U16, &OpenTherm42Hub::dhw_burner_operation_hours_sensor_, "DHW burner operation hours (id=123)"},
    {RequestKind::NUMBER_OF_TSPS, 10, SimpleValueKind::U8_HB, &OpenTherm42Hub::number_of_tsps_sensor_, "Number of TSP's (id=10)"},
    {RequestKind::NUMBER_OF_TSPS_VENTILATION, 88, SimpleValueKind::U8_HB, &OpenTherm42Hub::number_of_tsps_ventilation_sensor_, "Number of TSP's ventilation/heat-recovery (id=88)"},
    {RequestKind::NUMBER_OF_TSPS_SOLAR_STORAGE, 105, SimpleValueKind::U8_HB, &OpenTherm42Hub::number_of_tsps_solar_storage_sensor_, "Number of TSP's Solar Storage (id=105)"},
    {RequestKind::FAULT_HISTORY_BUFFER_SIZE, 12, SimpleValueKind::U8_HB, &OpenTherm42Hub::fault_history_buffer_size_sensor_, "Size of Fault Buffer (id=12)"},
    {RequestKind::FAULT_HISTORY_BUFFER_SIZE_VENTILATION, 90, SimpleValueKind::U8_HB, &OpenTherm42Hub::fault_history_buffer_size_ventilation_sensor_, "Size of Fault Buffer ventilation/heat-recovery (id=90)"},
    {RequestKind::FAULT_HISTORY_BUFFER_SIZE_SOLAR_STORAGE, 107, SimpleValueKind::U8_HB, &OpenTherm42Hub::fault_history_buffer_size_solar_storage_sensor_, "Size of Fault Buffer Solar Storage (id=107)"},
};
// clang-format on

const SimpleSensorInfo *OpenTherm42Hub::find_simple_sensor_(RequestKind kind) const {
  for (auto const &info : SIMPLE_SENSORS) {
    if (info.kind == kind) {
      return &info;
    }
  }
  return nullptr;
}

void OpenTherm42Hub::setup() {
  this->datalink_ = make_unique<OpenThermDataLink>(this->in_pin_, this->out_pin_);
  if (!this->datalink_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize the OpenTherm datalink (%s); see previous log messages for details",
             timer_error_to_string(this->datalink_->get_timer_error()));
    this->mark_failed();
    return;
  }
  this->build_schedule_();
}

void OpenTherm42Hub::loop() {
  switch (this->datalink_->get_state()) {
    case DataLinkState::IDLE: {
      if (millis() - this->last_conversation_end_ms_ < MASTER_WAIT_TIME_MS) {
        return;  // §4.3.1 MWT: wait at least 100 ms since the end of the previous conversation.
      }
      this->datalink_->send(this->build_next_request_());
      return;
    }
    case DataLinkState::SENT:
      this->datalink_->listen(RESPONSE_TIMEOUT_MS);
      return;
    case DataLinkState::RECEIVED:
      this->handle_response_(this->datalink_->get_frame());
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    case DataLinkState::ERROR:
      ESP_LOGW(TAG, "Conversation failed: %s", data_link_error_to_string(this->datalink_->get_error()));
      this->invalidate_response_(this->pending_request_kind_);
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    default:
      return;  // SENDING/LISTENING/RECEIVING: bit-level progress driven by the datalink's timer ISR.
  }
}

void OpenTherm42Hub::build_schedule_() {
  // STATUS and CONTROL_SETPOINT are always essential -- §5.2 requires sending them regardless of
  // whether any entity is configured for their bits.
  this->essential_requests_.push_back(RequestKind::STATUS);
  this->essential_requests_.push_back(RequestKind::CONTROL_SETPOINT);
  if (this->control_setpoint_2_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::CONTROL_SETPOINT_2);
  }
  if (this->ventilation_status_write_.any_configured() || this->ventilation_status_read_.any_configured()) {
    this->essential_requests_.push_back(RequestKind::VENTILATION_STATUS);
  }
  if (this->control_setpoint_ventilation_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::CONTROL_SETPOINT_VENTILATION);
  }

  if (this->fault_flags_read_.any_configured() || this->oem_fault_code_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::FAULT_FLAGS);
  }
  if (this->ventilation_fault_flags_read_.any_configured() || this->oem_fault_code_ventilation_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::VENTILATION_FAULT_FLAGS);
  }
  if (this->solar_storage_fault_indication_binary_sensor_ != nullptr ||
      this->master_solar_storage_status_solar_mode_sensor_ != nullptr ||
      this->solar_storage_mode_and_status_solar_mode_sensor_ != nullptr ||
      this->solar_storage_mode_and_status_solar_status_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::SOLAR_STORAGE_STATUS);
  }
  if (this->oem_fault_code_solar_storage_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::SOLAR_STORAGE_FAULT_FLAGS);
  }
  if (this->oem_diagnostic_code_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::OEM_DIAGNOSTIC_CODE);
  }
  if (this->oem_diagnostic_code_ventilation_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION);
  }

  if (this->ventilation_configuration_read_.any_configured() || this->member_id_code_ventilation_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::VENTILATION_CONFIGURATION);
  }
  if (this->solar_storage_configuration_system_type_binary_sensor_ != nullptr ||
      this->solar_storage_member_id_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::SOLAR_STORAGE_CONFIGURATION);
  }
  if (this->opentherm_version_boiler_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::OPENTHERM_VERSION_BOILER);
  }
  if (this->boiler_product_type_sensor_ != nullptr || this->boiler_product_version_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::PRODUCT_VERSION_BOILER);
  }
  if (this->opentherm_version_ventilation_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::OPENTHERM_VERSION_VENTILATION);
  }
  if (this->ventilation_product_type_sensor_ != nullptr || this->ventilation_product_version_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::PRODUCT_VERSION_VENTILATION);
  }
  if (this->solar_storage_product_type_sensor_ != nullptr || this->solar_storage_product_version_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::PRODUCT_VERSION_SOLAR_STORAGE);
  }

  // §5.3.4 Class 4: write-only numbers -- essential, like the Class 1 setpoints, since they represent
  // this master's active control input.
  if (this->room_setpoint_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::ROOM_SETPOINT);
  }
  if (this->room_setpoint_ch2_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::ROOM_SETPOINT_CH2);
  }
  if (this->room_temperature_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::ROOM_TEMPERATURE);
  }
  if (this->trch2_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::TRCH2);
  }
  if (this->time_id_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::DAY_TIME);
    this->essential_requests_.push_back(RequestKind::DATE);
    this->essential_requests_.push_back(RequestKind::YEAR);
  }
  // IDs 27/38/78/79: essential (write) if the "_set" number is configured, else informational (read)
  // if the plain sensor is configured -- never both, see build_next_request_()/handle_response_().
  if (this->outside_temperature_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::OUTSIDE_TEMPERATURE);
  } else if (this->outside_temperature_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::OUTSIDE_TEMPERATURE);
  }
  if (this->relative_humidity_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::RELATIVE_HUMIDITY);
  } else if (this->relative_humidity_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::RELATIVE_HUMIDITY);
  }
  if (this->relative_humidity_exhaust_air_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::RELATIVE_HUMIDITY_EXHAUST_AIR);
  } else if (this->relative_humidity_exhaust_air_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::RELATIVE_HUMIDITY_EXHAUST_AIR);
  }
  if (this->co2_level_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::CO2_LEVEL);
  } else if (this->co2_level_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::CO2_LEVEL);
  }
  if (this->boiler_fan_speed_setpoint_sensor_ != nullptr || this->boiler_fan_speed_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::BOILER_FAN_SPEED);
  }
  // Every plain read-only sensor: informational if its entity is configured.
  for (auto const &info : SIMPLE_SENSORS) {
    if (this->*(info.member) != nullptr) {
      this->informational_requests_.push_back(info.kind);
    }
  }

  // §5.3.5 Class 5.
  if (this->remote_parameter_transfer_enable_flags_read_.any_configured() ||
      this->remote_parameter_read_write_flags_read_.any_configured()) {
    this->informational_requests_.push_back(RequestKind::REMOTE_PARAMETER_FLAGS);
  }
  if (this->remote_parameter_transfer_enable_flags_ventilation_read_.any_configured() ||
      this->remote_parameter_read_write_flags_ventilation_read_.any_configured()) {
    this->informational_requests_.push_back(RequestKind::REMOTE_PARAMETER_FLAGS_VENTILATION);
  }
  if (this->dhwsetp_upper_bound_sensor_ != nullptr || this->dhwsetp_lower_bound_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::DHWSETP_BOUNDS);
  }
  if (this->max_chsetp_upper_bound_sensor_ != nullptr || this->max_chsetp_lower_bound_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::MAX_CHSETP_BOUNDS);
  }
  if (this->dhw_setpoint_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::DHW_SETPOINT);
  } else if (this->dhw_setpoint_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::DHW_SETPOINT);
  }
  if (this->max_ch_water_setpoint_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::MAX_CH_WATER_SETPOINT);
  } else if (this->max_ch_water_setpoint_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::MAX_CH_WATER_SETPOINT);
  }
  if (this->nominal_ventilation_value_number_ != nullptr) {
    this->essential_requests_.push_back(RequestKind::NOMINAL_VENTILATION_VALUE);
  } else if (this->nominal_ventilation_value_sensor_ != nullptr) {
    this->informational_requests_.push_back(RequestKind::NOMINAL_VENTILATION_VALUE);
  }

  // §5.3.6 Class 6: one informational slot round-robins through every configured TSP for periodic
  // reads; on-demand writes (see write_tsp()) are serviced ahead of this rotation.
  if (!this->tsp_slots_.empty()) {
    this->informational_requests_.push_back(RequestKind::TSP);
  }

  // §5.3.7 Class 7: same round-robin, for fault-history-buffer entries (purely read-only).
  if (!this->fhb_slots_.empty()) {
    this->informational_requests_.push_back(RequestKind::FHB);
  }
}

Frame OpenTherm42Hub::build_next_request_() {
  if (this->startup_phase_ != StartupPhase::DONE) {
    return this->build_startup_request_();
  }
  if (this->remote_request_pending_) {
    this->remote_request_pending_ = false;
    this->pending_request_kind_ = RequestKind::REMOTE_REQUEST;
    Frame frame{};
    frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
    frame.id = 4;
    frame.value_hb = this->remote_request_code_;
    return frame;
  }
  if (this->tsp_write_pending_) {
    this->tsp_write_pending_ = false;
    this->pending_request_kind_ = RequestKind::TSP;
    this->pending_tsp_slot_index_ = this->tsp_write_slot_index_;
    this->pending_tsp_is_write_ = true;
    Frame frame{};
    auto const &slot = this->tsp_slots_[this->tsp_write_slot_index_];
    frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
    frame.id = slot.data_id;
    frame.value_hb = slot.index;
    frame.value_lb = this->tsp_write_value_;
    return frame;
  }

  Frame frame{};
  RequestKind kind;
  if (this->next_is_informational_ && !this->informational_requests_.empty()) {
    kind = this->informational_requests_[this->informational_index_];
    this->informational_index_ = (this->informational_index_ + 1) % this->informational_requests_.size();
  } else {
    kind = this->essential_requests_[this->essential_index_];
    this->essential_index_ = (this->essential_index_ + 1) % this->essential_requests_.size();
  }
  if (!this->informational_requests_.empty()) {
    // Alternate essential/informational so a long informational list can never starve the essentials
    // (which include the §5.2 mandatory heartbeat) beyond §4.3.1's 1.15 s MCI.
    this->next_is_informational_ = !this->next_is_informational_;
  }
  this->pending_request_kind_ = kind;

  switch (kind) {
    case RequestKind::STATUS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 0;
      frame.value_hb = this->master_status_write_.pack();
      break;
    case RequestKind::CONTROL_SETPOINT:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 1;
      frame.set_value_f88(this->control_setpoint_number_ != nullptr ? this->control_setpoint_number_->state : 0.0f);
      break;
    case RequestKind::CONTROL_SETPOINT_2:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 8;
      frame.set_value_f88(this->control_setpoint_2_number_ != nullptr ? this->control_setpoint_2_number_->state : 0.0f);
      break;
    case RequestKind::VENTILATION_STATUS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 70;
      frame.value_hb = this->ventilation_status_write_.pack();
      break;
    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 71;
      frame.value_lb = this->control_setpoint_ventilation_number_ != nullptr
                           ? static_cast<uint8_t>(this->control_setpoint_ventilation_number_->state)
                           : 0;
      break;
    case RequestKind::FAULT_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 5;
      break;
    case RequestKind::VENTILATION_FAULT_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 72;
      break;
    case RequestKind::SOLAR_STORAGE_STATUS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 101;
      break;
    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 102;
      break;
    case RequestKind::OEM_DIAGNOSTIC_CODE:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 115;
      break;
    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 73;
      break;
    case RequestKind::REMOTE_REQUEST:
      break;  // built directly in build_next_request_() before this switch, unreachable here

    case RequestKind::ROOM_SETPOINT:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 16;
      frame.set_value_f88(this->room_setpoint_number_ != nullptr ? this->room_setpoint_number_->state : 0.0f);
      break;
    case RequestKind::ROOM_SETPOINT_CH2:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 23;
      frame.set_value_f88(this->room_setpoint_ch2_number_ != nullptr ? this->room_setpoint_ch2_number_->state : 0.0f);
      break;
    case RequestKind::ROOM_TEMPERATURE:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 24;
      frame.set_value_f88(this->room_temperature_number_ != nullptr ? this->room_temperature_number_->state : 0.0f);
      break;
    case RequestKind::TRCH2:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 37;
      frame.set_value_f88(this->trch2_number_ != nullptr ? this->trch2_number_->state : 0.0f);
      break;

    case RequestKind::DAY_TIME: {
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 20;
      if (this->time_id_ != nullptr) {
        ESPTime const now = this->time_id_->now();
        // §5.3.4 ID 20: day of week is Monday=1..Sunday=7; ESPTime's is Sunday=1..Saturday=7.
        uint8_t const day_of_week = now.day_of_week == 1 ? 7 : now.day_of_week - 1;
        frame.value_hb = (day_of_week << 5) | (now.hour & 0x1F);
        frame.value_lb = now.minute;
      }
      break;
    }
    case RequestKind::DATE:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 21;
      if (this->time_id_ != nullptr) {
        ESPTime const now = this->time_id_->now();
        frame.value_hb = now.month;
        frame.value_lb = now.day_of_month;
      }
      break;
    case RequestKind::YEAR:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 22;
      if (this->time_id_ != nullptr) {
        frame.set_value_u16(this->time_id_->now().year);
      }
      break;

    case RequestKind::OUTSIDE_TEMPERATURE:
      frame.id = 27;
      if (this->outside_temperature_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.set_value_f88(this->outside_temperature_number_->state);
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;
    case RequestKind::RELATIVE_HUMIDITY:
      frame.id = 38;
      if (this->relative_humidity_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.set_value_f88(this->relative_humidity_number_->state);
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;
    case RequestKind::RELATIVE_HUMIDITY_EXHAUST_AIR:
      frame.id = 78;
      if (this->relative_humidity_exhaust_air_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.value_lb = static_cast<uint8_t>(this->relative_humidity_exhaust_air_number_->state);
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;
    case RequestKind::CO2_LEVEL:
      frame.id = 79;
      if (this->co2_level_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.set_value_u16(static_cast<uint16_t>(this->co2_level_number_->state));
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;

    case RequestKind::BOILER_FAN_SPEED:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 35;
      break;

    case RequestKind::REMOTE_PARAMETER_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 6;
      break;
    case RequestKind::REMOTE_PARAMETER_FLAGS_VENTILATION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 86;
      break;
    case RequestKind::DHWSETP_BOUNDS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 48;
      break;
    case RequestKind::MAX_CHSETP_BOUNDS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 49;
      break;

    case RequestKind::DHW_SETPOINT:
      frame.id = 56;
      if (this->dhw_setpoint_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.set_value_f88(this->dhw_setpoint_number_->state);
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;
    case RequestKind::MAX_CH_WATER_SETPOINT:
      frame.id = 57;
      if (this->max_ch_water_setpoint_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.set_value_f88(this->max_ch_water_setpoint_number_->state);
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;
    case RequestKind::NOMINAL_VENTILATION_VALUE:
      frame.id = 87;
      if (this->nominal_ventilation_value_number_ != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
        frame.value_hb = static_cast<uint8_t>(this->nominal_ventilation_value_number_->state);
      } else {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      }
      break;

    case RequestKind::TSP:
      // Only reached for the periodic-read rotation -- on-demand writes are intercepted by the
      // tsp_write_pending_ check above build_next_request_()'s switch.
      if (!this->tsp_slots_.empty()) {
        this->pending_tsp_slot_index_ = this->tsp_read_index_;
        this->pending_tsp_is_write_ = false;
        auto const &slot = this->tsp_slots_[this->tsp_read_index_];
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
        frame.id = slot.data_id;
        frame.value_hb = slot.index;
        this->tsp_read_index_ = (this->tsp_read_index_ + 1) % this->tsp_slots_.size();
      }
      break;

    case RequestKind::FHB:
      if (!this->fhb_slots_.empty()) {
        this->pending_fhb_slot_index_ = this->fhb_read_index_;
        auto const &slot = this->fhb_slots_[this->fhb_read_index_];
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
        frame.id = slot.data_id;
        frame.value_hb = slot.index;
        this->fhb_read_index_ = (this->fhb_read_index_ + 1) % this->fhb_slots_.size();
      }
      break;

    default: {
      // Every plain read-only sensor (see the SIMPLE_SENSORS table) shares this one case.
      const SimpleSensorInfo *info = this->find_simple_sensor_(kind);
      if (info != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
        frame.id = info->id;
      }
      break;  // info == nullptr only for startup-only kinds, unreachable here
    }
  }
  return frame;
}

Frame OpenTherm42Hub::build_startup_request_() {
  Frame frame{};
  switch (this->startup_phase_) {
    case StartupPhase::BOILER_CONFIG:
      this->pending_request_kind_ = RequestKind::BOILER_CONFIG;
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 3;
      return frame;
    case StartupPhase::MASTER_CONFIG:
      this->pending_request_kind_ = RequestKind::MASTER_CONFIG;
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 2;
      frame.value_hb = 0;  // bit 0 Smart Power: not implemented -- see §3.4, out of scope for this component
      frame.value_lb = this->controller_member_id_code_;
      return frame;
    case StartupPhase::MASTER_OPENTHERM_VERSION:
      this->pending_request_kind_ = RequestKind::MASTER_OPENTHERM_VERSION;
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 124;
      frame.set_value_f88(this->controller_opentherm_version_);
      return frame;
    case StartupPhase::MASTER_PRODUCT_VERSION:
      this->pending_request_kind_ = RequestKind::MASTER_PRODUCT_VERSION;
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 126;
      frame.value_hb = this->controller_product_type_;
      frame.value_lb = this->controller_product_version_;
      return frame;
    case StartupPhase::BRAND:
      this->pending_request_kind_ = RequestKind::BRAND;
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 93;
      frame.value_hb = this->brand_.next_index;
      return frame;
    case StartupPhase::BRAND_VERSION:
      this->pending_request_kind_ = RequestKind::BRAND_VERSION;
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 94;
      frame.value_hb = this->brand_version_.next_index;
      return frame;
    case StartupPhase::BRAND_SERIAL_NUMBER:
      this->pending_request_kind_ = RequestKind::BRAND_SERIAL_NUMBER;
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 95;
      frame.value_hb = this->brand_serial_number_.next_index;
      return frame;
    case StartupPhase::DONE:
      break;  // guarded by the caller, unreachable here
  }
  return frame;
}

bool OpenTherm42Hub::startup_phase_actionable_(StartupPhase phase) const {
  switch (phase) {
    case StartupPhase::BRAND:
      return this->brand_.sensor != nullptr;
    case StartupPhase::BRAND_VERSION:
      return this->brand_version_.sensor != nullptr;
    case StartupPhase::BRAND_SERIAL_NUMBER:
      return this->brand_serial_number_.sensor != nullptr;
    default:
      return true;
  }
}

void OpenTherm42Hub::advance_startup_phase_() {
  do {
    switch (this->startup_phase_) {
      case StartupPhase::BOILER_CONFIG:
        this->startup_phase_ = StartupPhase::MASTER_CONFIG;
        break;
      case StartupPhase::MASTER_CONFIG:
        this->startup_phase_ = StartupPhase::MASTER_OPENTHERM_VERSION;
        break;
      case StartupPhase::MASTER_OPENTHERM_VERSION:
        this->startup_phase_ = StartupPhase::MASTER_PRODUCT_VERSION;
        break;
      case StartupPhase::MASTER_PRODUCT_VERSION:
        this->startup_phase_ = StartupPhase::BRAND;
        break;
      case StartupPhase::BRAND:
        this->startup_phase_ = StartupPhase::BRAND_VERSION;
        break;
      case StartupPhase::BRAND_VERSION:
        this->startup_phase_ = StartupPhase::BRAND_SERIAL_NUMBER;
        break;
      case StartupPhase::BRAND_SERIAL_NUMBER:
        this->startup_phase_ = StartupPhase::DONE;
        break;
      case StartupPhase::DONE:
        return;
    }
  } while (!this->startup_phase_actionable_(this->startup_phase_));
}

void OpenTherm42Hub::handle_response_(const Frame &frame) {
  auto const type = static_cast<MessageType>(frame.type);
  switch (this->pending_request_kind_) {
    case RequestKind::BOILER_CONFIG:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Boiler configuration flags (id=3) read was rejected (message type %u)", frame.type);
        return;  // keep retrying -- see build_startup_request_()/StartupPhase
      }
      this->boiler_config_flags_ = frame.value_hb;
      this->boiler_member_id_code_ = frame.value_lb;
      this->boiler_configuration_read_.publish(frame.value_hb);
      if (this->boiler_member_id_code_sensor_ != nullptr) {
        this->boiler_member_id_code_sensor_->publish_state(frame.value_lb);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::STATUS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Status exchange (id=0) was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::STATUS);
        return;
      }
      this->boiler_status_ = frame.value_lb;
      this->boiler_status_read_.publish(frame.value_lb);
      return;

    case RequestKind::CONTROL_SETPOINT:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Control setpoint (id=1) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::CONTROL_SETPOINT_2:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Control setpoint 2 (id=8) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::VENTILATION_STATUS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Ventilation/heat-recovery status exchange (id=70) was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::VENTILATION_STATUS);
        return;
      }
      this->ventilation_status_read_.publish(frame.value_lb);
      return;

    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Control setpoint ventilation/heat-recovery (id=71) write was rejected (message type %u)",
                 frame.type);
      }
      return;

    case RequestKind::FAULT_FLAGS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Application-specific fault flags (id=5) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::FAULT_FLAGS);
        return;
      }
      this->fault_flags_read_.publish(frame.value_hb);
      if (this->oem_fault_code_sensor_ != nullptr) {
        this->oem_fault_code_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::VENTILATION_FAULT_FLAGS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG,
                 "Application-specific fault flags ventilation/heat-recovery (id=72) read was rejected "
                 "(message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::VENTILATION_FAULT_FLAGS);
        return;
      }
      this->ventilation_fault_flags_read_.publish(frame.value_hb);
      if (this->oem_fault_code_ventilation_sensor_ != nullptr) {
        this->oem_fault_code_ventilation_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::SOLAR_STORAGE_STATUS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Solar storage status (id=101) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::SOLAR_STORAGE_STATUS);
        return;
      }
      // HB bits 2,1,0 and LB bits 3,2,1 both encode "Solar mode" (same 5-value enum, different byte);
      // LB bit 0 is a fault flag and LB bits 5,4 are "Solar status" -- see the spec's ID 101 table.
      if (this->master_solar_storage_status_solar_mode_sensor_ != nullptr) {
        this->master_solar_storage_status_solar_mode_sensor_->publish_state(frame.value_hb & 0x7);
      }
      if (this->solar_storage_fault_indication_binary_sensor_ != nullptr) {
        this->solar_storage_fault_indication_binary_sensor_->publish_state(frame.value_lb & 0x1);
      }
      if (this->solar_storage_mode_and_status_solar_mode_sensor_ != nullptr) {
        this->solar_storage_mode_and_status_solar_mode_sensor_->publish_state((frame.value_lb >> 1) & 0x7);
      }
      if (this->solar_storage_mode_and_status_solar_status_sensor_ != nullptr) {
        this->solar_storage_mode_and_status_solar_status_sensor_->publish_state((frame.value_lb >> 4) & 0x3);
      }
      return;

    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Solar storage specific fault flags (id=102) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::SOLAR_STORAGE_FAULT_FLAGS);
        return;
      }
      if (this->oem_fault_code_solar_storage_sensor_ != nullptr) {
        this->oem_fault_code_solar_storage_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::OEM_DIAGNOSTIC_CODE:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "OEM diagnostic code (id=115) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::OEM_DIAGNOSTIC_CODE);
        return;
      }
      if (this->oem_diagnostic_code_sensor_ != nullptr) {
        this->oem_diagnostic_code_sensor_->publish_state(frame.value_u16());
      }
      return;

    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "OEM diagnostic code ventilation/heat-recovery (id=73) read was rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION);
        return;
      }
      if (this->oem_diagnostic_code_ventilation_sensor_ != nullptr) {
        this->oem_diagnostic_code_ventilation_sensor_->publish_state(frame.value_u16());
      }
      return;

    case RequestKind::MASTER_CONFIG:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Master configuration (id=2) write was rejected (message type %u)", frame.type);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::MASTER_OPENTHERM_VERSION:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "OpenTherm version Master (id=124) write was rejected (message type %u)", frame.type);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::MASTER_PRODUCT_VERSION:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Master product version number and type (id=126) write was rejected (message type %u)",
                 frame.type);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::VENTILATION_CONFIGURATION:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Configuration ventilation/heat-recovery (id=74) read was rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::VENTILATION_CONFIGURATION);
        return;
      }
      this->ventilation_configuration_read_.publish(frame.value_hb);
      if (this->member_id_code_ventilation_sensor_ != nullptr) {
        this->member_id_code_ventilation_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::SOLAR_STORAGE_CONFIGURATION:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Solar Storage configuration (id=103) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::SOLAR_STORAGE_CONFIGURATION);
        return;
      }
      if (this->solar_storage_configuration_system_type_binary_sensor_ != nullptr) {
        this->solar_storage_configuration_system_type_binary_sensor_->publish_state(frame.value_hb & 0x1);
      }
      if (this->solar_storage_member_id_sensor_ != nullptr) {
        this->solar_storage_member_id_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::OPENTHERM_VERSION_BOILER:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "OpenTherm version Boiler (id=125) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::OPENTHERM_VERSION_BOILER);
        return;
      }
      if (this->opentherm_version_boiler_sensor_ != nullptr) {
        this->opentherm_version_boiler_sensor_->publish_state(frame.value_f88());
      }
      return;

    case RequestKind::PRODUCT_VERSION_BOILER:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Boiler product version number and type (id=127) read was rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::PRODUCT_VERSION_BOILER);
        return;
      }
      if (this->boiler_product_type_sensor_ != nullptr) {
        this->boiler_product_type_sensor_->publish_state(frame.value_hb);
      }
      if (this->boiler_product_version_sensor_ != nullptr) {
        this->boiler_product_version_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::OPENTHERM_VERSION_VENTILATION:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "OpenTherm version ventilation/heat-recovery (id=75) read was rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::OPENTHERM_VERSION_VENTILATION);
        return;
      }
      if (this->opentherm_version_ventilation_sensor_ != nullptr) {
        this->opentherm_version_ventilation_sensor_->publish_state(frame.value_f88());
      }
      return;

    case RequestKind::PRODUCT_VERSION_VENTILATION:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG,
                 "Ventilation/heat-recovery product version number and type (id=76) read was rejected "
                 "(message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::PRODUCT_VERSION_VENTILATION);
        return;
      }
      if (this->ventilation_product_type_sensor_ != nullptr) {
        this->ventilation_product_type_sensor_->publish_state(frame.value_hb);
      }
      if (this->ventilation_product_version_sensor_ != nullptr) {
        this->ventilation_product_version_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::PRODUCT_VERSION_SOLAR_STORAGE:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Solar Storage product version number and type (id=104) read was rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::PRODUCT_VERSION_SOLAR_STORAGE);
        return;
      }
      if (this->solar_storage_product_type_sensor_ != nullptr) {
        this->solar_storage_product_type_sensor_->publish_state(frame.value_hb);
      }
      if (this->solar_storage_product_version_sensor_ != nullptr) {
        this->solar_storage_product_version_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::BRAND:
      this->handle_brand_response_(frame, this->brand_, "Brand (id=93)");
      return;

    case RequestKind::BRAND_VERSION:
      this->handle_brand_response_(frame, this->brand_version_, "Brand version (id=94)");
      return;

    case RequestKind::BRAND_SERIAL_NUMBER:
      this->handle_brand_response_(frame, this->brand_serial_number_, "Brand serial number (id=95)");
      return;

    case RequestKind::REMOTE_REQUEST:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Remote request (id=4, code=%u) was rejected (message type %u)", this->remote_request_code_,
                 frame.type);
        this->invalidate_response_(RequestKind::REMOTE_REQUEST);
        return;
      }
      if (this->remote_request_last_response_code_sensor_ != nullptr) {
        this->remote_request_last_response_code_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::ROOM_SETPOINT:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Room Setpoint (id=16) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::ROOM_SETPOINT_CH2:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Room Setpoint CH2 (id=23) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::ROOM_TEMPERATURE:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Room temperature (id=24) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::TRCH2:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "TrCH2 (id=37) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::DAY_TIME:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Day of Week & Time of Day (id=20) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::DATE:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Date (id=21) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::YEAR:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Year (id=22) write was rejected (message type %u)", frame.type);
      }
      return;

    case RequestKind::OUTSIDE_TEMPERATURE:
      if (this->outside_temperature_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "Outside temperature (id=27) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Outside temperature (id=27) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::OUTSIDE_TEMPERATURE);
        return;
      }
      if (this->outside_temperature_sensor_ != nullptr) {
        this->outside_temperature_sensor_->publish_state(frame.value_f88());
      }
      return;

    case RequestKind::RELATIVE_HUMIDITY:
      if (this->relative_humidity_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "Relative Humidity (id=38) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Relative Humidity (id=38) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::RELATIVE_HUMIDITY);
        return;
      }
      if (this->relative_humidity_sensor_ != nullptr) {
        this->relative_humidity_sensor_->publish_state(frame.value_f88());
      }
      return;

    case RequestKind::RELATIVE_HUMIDITY_EXHAUST_AIR:
      if (this->relative_humidity_exhaust_air_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "Relative humidity exhaust air (id=78) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Relative humidity exhaust air (id=78) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::RELATIVE_HUMIDITY_EXHAUST_AIR);
        return;
      }
      if (this->relative_humidity_exhaust_air_sensor_ != nullptr) {
        this->relative_humidity_exhaust_air_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::CO2_LEVEL:
      if (this->co2_level_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "CO2 level (id=79) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "CO2 level (id=79) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::CO2_LEVEL);
        return;
      }
      if (this->co2_level_sensor_ != nullptr) {
        this->co2_level_sensor_->publish_state(frame.value_u16());
      }
      return;

    case RequestKind::BOILER_FAN_SPEED:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Boiler fan speed (id=35) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::BOILER_FAN_SPEED);
        return;
      }
      if (this->boiler_fan_speed_setpoint_sensor_ != nullptr) {
        this->boiler_fan_speed_setpoint_sensor_->publish_state(frame.value_hb);
      }
      if (this->boiler_fan_speed_sensor_ != nullptr) {
        this->boiler_fan_speed_sensor_->publish_state(frame.value_lb);
      }
      return;

    case RequestKind::REMOTE_PARAMETER_FLAGS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Remote-parameter transfer-enable/read-write flags (id=6) read was rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::REMOTE_PARAMETER_FLAGS);
        return;
      }
      this->remote_parameter_transfer_enable_flags_read_.publish(frame.value_hb);
      this->remote_parameter_read_write_flags_read_.publish(frame.value_lb);
      return;

    case RequestKind::REMOTE_PARAMETER_FLAGS_VENTILATION:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG,
                 "Remote-parameter transfer-enable/read-write flags ventilation/heat-recovery (id=86) read was "
                 "rejected (message type %u)",
                 frame.type);
        this->invalidate_response_(RequestKind::REMOTE_PARAMETER_FLAGS_VENTILATION);
        return;
      }
      this->remote_parameter_transfer_enable_flags_ventilation_read_.publish(frame.value_hb);
      this->remote_parameter_read_write_flags_ventilation_read_.publish(frame.value_lb);
      return;

    case RequestKind::DHWSETP_BOUNDS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "DHWsetp upp-/low-bound (id=48) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::DHWSETP_BOUNDS);
        return;
      }
      if (this->dhwsetp_upper_bound_sensor_ != nullptr) {
        this->dhwsetp_upper_bound_sensor_->publish_state(static_cast<int8_t>(frame.value_hb));
      }
      if (this->dhwsetp_lower_bound_sensor_ != nullptr) {
        this->dhwsetp_lower_bound_sensor_->publish_state(static_cast<int8_t>(frame.value_lb));
      }
      return;

    case RequestKind::MAX_CHSETP_BOUNDS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "max CHsetp upp-/low-bnd (id=49) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::MAX_CHSETP_BOUNDS);
        return;
      }
      if (this->max_chsetp_upper_bound_sensor_ != nullptr) {
        this->max_chsetp_upper_bound_sensor_->publish_state(static_cast<int8_t>(frame.value_hb));
      }
      if (this->max_chsetp_lower_bound_sensor_ != nullptr) {
        this->max_chsetp_lower_bound_sensor_->publish_state(static_cast<int8_t>(frame.value_lb));
      }
      return;

    case RequestKind::DHW_SETPOINT:
      if (this->dhw_setpoint_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "DHW Setpoint (id=56) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "DHW Setpoint (id=56) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::DHW_SETPOINT);
        return;
      }
      if (this->dhw_setpoint_sensor_ != nullptr) {
        this->dhw_setpoint_sensor_->publish_state(frame.value_f88());
      }
      return;

    case RequestKind::MAX_CH_WATER_SETPOINT:
      if (this->max_ch_water_setpoint_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "max CH water Setpoint (id=57) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "max CH water Setpoint (id=57) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::MAX_CH_WATER_SETPOINT);
        return;
      }
      if (this->max_ch_water_setpoint_sensor_ != nullptr) {
        this->max_ch_water_setpoint_sensor_->publish_state(frame.value_f88());
      }
      return;

    case RequestKind::NOMINAL_VENTILATION_VALUE:
      if (this->nominal_ventilation_value_number_ != nullptr) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "Nominal ventilation value (id=87) write was rejected (message type %u)", frame.type);
        }
        return;
      }
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Nominal ventilation value (id=87) read was rejected (message type %u)", frame.type);
        this->invalidate_response_(RequestKind::NOMINAL_VENTILATION_VALUE);
        return;
      }
      if (this->nominal_ventilation_value_sensor_ != nullptr) {
        this->nominal_ventilation_value_sensor_->publish_state(frame.value_hb);
      }
      return;

    case RequestKind::TSP: {
      auto const &slot = this->tsp_slots_[this->pending_tsp_slot_index_];
      if (this->pending_tsp_is_write_) {
        if (type != MessageType::WRITE_ACK) {
          ESP_LOGW(TAG, "TSP write (id=%u, index=%u) was rejected (message type %u)", slot.data_id, slot.index,
                   frame.type);
          return;
        }
      } else if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "TSP read (id=%u, index=%u) was rejected (message type %u)", slot.data_id, slot.index,
                 frame.type);
        if (slot.number != nullptr) {
          slot.number->set_has_state(false);
        }
        return;
      }
      // §5.3.6: both a READ-ACK and a WRITE-ACK echo the (possibly boiler-clamped) TSP-value in LB --
      // always trust that over whatever was requested.
      if (slot.number != nullptr) {
        slot.number->publish_state(frame.value_lb);
      }
      return;
    }

    case RequestKind::FHB: {
      auto const &slot = this->fhb_slots_[this->pending_fhb_slot_index_];
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "FHB read (id=%u, index=%u) was rejected (message type %u)", slot.data_id, slot.index,
                 frame.type);
        if (slot.sensor != nullptr) {
          slot.sensor->set_has_state(false);
        }
        return;
      }
      if (slot.sensor != nullptr) {
        slot.sensor->publish_state(frame.value_lb);
      }
      return;
    }

    default: {
      // Every plain read-only sensor (see the SIMPLE_SENSORS table) shares this one case.
      const SimpleSensorInfo *info = this->find_simple_sensor_(this->pending_request_kind_);
      if (info == nullptr) {
        return;  // startup-only kinds are handled by handle_response_()'s dedicated cases, unreachable here
      }
      sensor::Sensor *sensor_ptr = this->*(info->member);
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "%s read was rejected (message type %u)", info->log_name, frame.type);
        if (sensor_ptr != nullptr) {
          sensor_ptr->set_has_state(false);
        }
        return;
      }
      if (sensor_ptr == nullptr) {
        return;
      }
      switch (info->value_kind) {
        case SimpleValueKind::F88:
          sensor_ptr->publish_state(frame.value_f88());
          return;
        case SimpleValueKind::S16:
          sensor_ptr->publish_state(frame.value_s16());
          return;
        case SimpleValueKind::U16:
          sensor_ptr->publish_state(frame.value_u16());
          return;
        case SimpleValueKind::U8_LB:
          sensor_ptr->publish_state(frame.value_lb);
          return;
        case SimpleValueKind::U8_HB:
          sensor_ptr->publish_state(frame.value_hb);
          return;
      }
    }
  }
}

void OpenTherm42Hub::handle_brand_response_(const Frame &frame, BrandRead &brand, const char *log_name) {
  if (brand.sensor == nullptr) {
    return;  // only scheduled when configured; defensive in case that invariant is ever broken
  }
  auto const type = static_cast<MessageType>(frame.type);
  if (type != MessageType::READ_ACK) {
    ESP_LOGW(TAG, "%s read was rejected (message type %u)", log_name, frame.type);
    brand.sensor->set_has_state(false);
    this->advance_startup_phase_();
    return;
  }
  // §5.3.2: the response's HB is the total character count (not an index) -- e.g. HB=0x06 means "6
  // characters can be read" -- and LB is the character at the index this request's HB asked for.
  uint8_t const total_len = std::min<uint8_t>(frame.value_hb, brand.buffer.size() - 1);
  if (brand.next_index < total_len) {
    brand.buffer[brand.next_index] = static_cast<char>(frame.value_lb);
    brand.next_index++;
  }
  if (brand.next_index >= total_len) {
    brand.buffer[brand.next_index] = '\0';
    brand.sensor->publish_state(brand.buffer.data(), brand.next_index);
    this->advance_startup_phase_();
  }
}

void OpenTherm42Hub::invalidate_response_(RequestKind kind) {
  switch (kind) {
    case RequestKind::BOILER_CONFIG:
      return;  // retried indefinitely on failure -- see StartupPhase, do not advance past it here

    case RequestKind::CONTROL_SETPOINT:
    case RequestKind::CONTROL_SETPOINT_2:
    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      return;  // write-only kinds have no read-only entity to invalidate

    case RequestKind::MASTER_CONFIG:
    case RequestKind::MASTER_OPENTHERM_VERSION:
    case RequestKind::MASTER_PRODUCT_VERSION:
      // Write-only startup kinds, attempted once -- a raw datalink error (as opposed to a rejected
      // ack, handled in handle_response_()) must still advance past them so startup can finish.
      this->advance_startup_phase_();
      return;

    case RequestKind::BRAND:
      if (this->brand_.sensor != nullptr) {
        this->brand_.sensor->set_has_state(false);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::BRAND_VERSION:
      if (this->brand_version_.sensor != nullptr) {
        this->brand_version_.sensor->set_has_state(false);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::BRAND_SERIAL_NUMBER:
      if (this->brand_serial_number_.sensor != nullptr) {
        this->brand_serial_number_.sensor->set_has_state(false);
      }
      this->advance_startup_phase_();
      return;

    case RequestKind::STATUS:
      this->boiler_status_read_.invalidate();
      return;

    case RequestKind::VENTILATION_STATUS:
      this->ventilation_status_read_.invalidate();
      return;

    case RequestKind::FAULT_FLAGS:
      this->fault_flags_read_.invalidate();
      if (this->oem_fault_code_sensor_ != nullptr) {
        this->oem_fault_code_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::VENTILATION_FAULT_FLAGS:
      this->ventilation_fault_flags_read_.invalidate();
      if (this->oem_fault_code_ventilation_sensor_ != nullptr) {
        this->oem_fault_code_ventilation_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::SOLAR_STORAGE_STATUS:
      if (this->solar_storage_fault_indication_binary_sensor_ != nullptr) {
        this->solar_storage_fault_indication_binary_sensor_->set_has_state(false);
      }
      if (this->master_solar_storage_status_solar_mode_sensor_ != nullptr) {
        this->master_solar_storage_status_solar_mode_sensor_->set_has_state(false);
      }
      if (this->solar_storage_mode_and_status_solar_mode_sensor_ != nullptr) {
        this->solar_storage_mode_and_status_solar_mode_sensor_->set_has_state(false);
      }
      if (this->solar_storage_mode_and_status_solar_status_sensor_ != nullptr) {
        this->solar_storage_mode_and_status_solar_status_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      if (this->oem_fault_code_solar_storage_sensor_ != nullptr) {
        this->oem_fault_code_solar_storage_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::OEM_DIAGNOSTIC_CODE:
      if (this->oem_diagnostic_code_sensor_ != nullptr) {
        this->oem_diagnostic_code_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      if (this->oem_diagnostic_code_ventilation_sensor_ != nullptr) {
        this->oem_diagnostic_code_ventilation_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::VENTILATION_CONFIGURATION:
      this->ventilation_configuration_read_.invalidate();
      if (this->member_id_code_ventilation_sensor_ != nullptr) {
        this->member_id_code_ventilation_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::SOLAR_STORAGE_CONFIGURATION:
      if (this->solar_storage_configuration_system_type_binary_sensor_ != nullptr) {
        this->solar_storage_configuration_system_type_binary_sensor_->set_has_state(false);
      }
      if (this->solar_storage_member_id_sensor_ != nullptr) {
        this->solar_storage_member_id_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::OPENTHERM_VERSION_BOILER:
      if (this->opentherm_version_boiler_sensor_ != nullptr) {
        this->opentherm_version_boiler_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::PRODUCT_VERSION_BOILER:
      if (this->boiler_product_type_sensor_ != nullptr) {
        this->boiler_product_type_sensor_->set_has_state(false);
      }
      if (this->boiler_product_version_sensor_ != nullptr) {
        this->boiler_product_version_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::OPENTHERM_VERSION_VENTILATION:
      if (this->opentherm_version_ventilation_sensor_ != nullptr) {
        this->opentherm_version_ventilation_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::PRODUCT_VERSION_VENTILATION:
      if (this->ventilation_product_type_sensor_ != nullptr) {
        this->ventilation_product_type_sensor_->set_has_state(false);
      }
      if (this->ventilation_product_version_sensor_ != nullptr) {
        this->ventilation_product_version_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::PRODUCT_VERSION_SOLAR_STORAGE:
      if (this->solar_storage_product_type_sensor_ != nullptr) {
        this->solar_storage_product_type_sensor_->set_has_state(false);
      }
      if (this->solar_storage_product_version_sensor_ != nullptr) {
        this->solar_storage_product_version_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::REMOTE_REQUEST:
      if (this->remote_request_last_response_code_sensor_ != nullptr) {
        this->remote_request_last_response_code_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::ROOM_SETPOINT:
    case RequestKind::ROOM_SETPOINT_CH2:
    case RequestKind::ROOM_TEMPERATURE:
    case RequestKind::TRCH2:
    case RequestKind::DAY_TIME:
    case RequestKind::DATE:
    case RequestKind::YEAR:
      return;  // write-only, nothing to invalidate

    case RequestKind::OUTSIDE_TEMPERATURE:
      if (this->outside_temperature_number_ == nullptr && this->outside_temperature_sensor_ != nullptr) {
        this->outside_temperature_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::RELATIVE_HUMIDITY:
      if (this->relative_humidity_number_ == nullptr && this->relative_humidity_sensor_ != nullptr) {
        this->relative_humidity_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::RELATIVE_HUMIDITY_EXHAUST_AIR:
      if (this->relative_humidity_exhaust_air_number_ == nullptr &&
          this->relative_humidity_exhaust_air_sensor_ != nullptr) {
        this->relative_humidity_exhaust_air_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::CO2_LEVEL:
      if (this->co2_level_number_ == nullptr && this->co2_level_sensor_ != nullptr) {
        this->co2_level_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::BOILER_FAN_SPEED:
      if (this->boiler_fan_speed_setpoint_sensor_ != nullptr) {
        this->boiler_fan_speed_setpoint_sensor_->set_has_state(false);
      }
      if (this->boiler_fan_speed_sensor_ != nullptr) {
        this->boiler_fan_speed_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::REMOTE_PARAMETER_FLAGS:
      this->remote_parameter_transfer_enable_flags_read_.invalidate();
      this->remote_parameter_read_write_flags_read_.invalidate();
      return;

    case RequestKind::REMOTE_PARAMETER_FLAGS_VENTILATION:
      this->remote_parameter_transfer_enable_flags_ventilation_read_.invalidate();
      this->remote_parameter_read_write_flags_ventilation_read_.invalidate();
      return;

    case RequestKind::DHWSETP_BOUNDS:
      if (this->dhwsetp_upper_bound_sensor_ != nullptr) {
        this->dhwsetp_upper_bound_sensor_->set_has_state(false);
      }
      if (this->dhwsetp_lower_bound_sensor_ != nullptr) {
        this->dhwsetp_lower_bound_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::MAX_CHSETP_BOUNDS:
      if (this->max_chsetp_upper_bound_sensor_ != nullptr) {
        this->max_chsetp_upper_bound_sensor_->set_has_state(false);
      }
      if (this->max_chsetp_lower_bound_sensor_ != nullptr) {
        this->max_chsetp_lower_bound_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::DHW_SETPOINT:
      if (this->dhw_setpoint_number_ == nullptr && this->dhw_setpoint_sensor_ != nullptr) {
        this->dhw_setpoint_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::MAX_CH_WATER_SETPOINT:
      if (this->max_ch_water_setpoint_number_ == nullptr && this->max_ch_water_setpoint_sensor_ != nullptr) {
        this->max_ch_water_setpoint_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::NOMINAL_VENTILATION_VALUE:
      if (this->nominal_ventilation_value_number_ == nullptr && this->nominal_ventilation_value_sensor_ != nullptr) {
        this->nominal_ventilation_value_sensor_->set_has_state(false);
      }
      return;

    case RequestKind::TSP:
      if (this->pending_tsp_slot_index_ < this->tsp_slots_.size()) {
        number::Number *tsp_number = this->tsp_slots_[this->pending_tsp_slot_index_].number;
        if (tsp_number != nullptr) {
          tsp_number->set_has_state(false);
        }
      }
      return;

    case RequestKind::FHB:
      if (this->pending_fhb_slot_index_ < this->fhb_slots_.size()) {
        sensor::Sensor *fhb_sensor = this->fhb_slots_[this->pending_fhb_slot_index_].sensor;
        if (fhb_sensor != nullptr) {
          fhb_sensor->set_has_state(false);
        }
      }
      return;

    default: {
      const SimpleSensorInfo *info = this->find_simple_sensor_(kind);
      if (info != nullptr) {
        sensor::Sensor *sensor_ptr = this->*(info->member);
        if (sensor_ptr != nullptr) {
          sensor_ptr->set_has_state(false);
        }
      }
      return;
    }
  }
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
