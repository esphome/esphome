/*
 * Notice:
 *   The code for actual communication with OpenTherm is heavily based on Ihor Melnyk's
 *   OpenTherm Library at https://github.com/ihormelnyk/opentherm_library, which is MIT licensed.
 */

#include "opentherm.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace opentherm {

// All public method implementations

void OpenThermComponent::set_pins(InternalGPIOPin *read_pin, InternalGPIOPin *write_pin) {
  this->read_pin_ = read_pin;
  this->write_pin_ = write_pin;
}

void OpenThermComponent::setup() {
  this->read_pin_->setup();
  this->isr_read_pin_ = this->read_pin_->to_isr();
  this->read_pin_->attach_interrupt(&OpenThermComponent::handle_interrupt, this, gpio::INTERRUPT_ANY_EDGE);
  this->write_pin_->setup();

  this->write_pin_->digital_write(true);
  delay(25);
  this->status_ = OpenThermStatus::READY;

  this->start_millis_ = millis();
  this->start_interval_ = this->get_update_interval() / 24;

#ifdef USE_SWITCH
  if (this->ch_enabled_switch_) {
    this->ch_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_ch_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s CH", (enabled ? "Enabled" : "Disabled"));
        this->wanted_ch_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->ch_2_enabled_switch_) {
    this->ch_2_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_ch_2_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s CH 2", (enabled ? "Enabled" : "Disabled"));
        this->wanted_ch_2_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->dhw_enabled_switch_) {
    this->dhw_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_dhw_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s DHW", (enabled ? "Enabled" : "Disabled"));
        this->wanted_dhw_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->cooling_enabled_switch_) {
    this->cooling_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_cooling_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s cooling", (enabled ? "Enabled" : "Disabled"));
        this->wanted_cooling_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->otc_active_switch_) {
    this->otc_active_switch_->add_on_state_callback([this](bool active) {
      if (this->wanted_otc_active_ != active) {
        ESP_LOGI(TAG, "OTC %s", (active ? "activated" : "deactivated"));
        this->wanted_otc_active_ = active;
        this->set_boiler_status_();
      }
    });
  }
#endif
#ifdef USE_NUMBER
  if (this->ch_setpoint_temperature_number_) {
    this->ch_setpoint_temperature_number_->setup();
    this->ch_setpoint_temperature_number_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
  if (this->ch_2_setpoint_temperature_number_) {
    this->ch_2_setpoint_temperature_number_->setup();
    this->ch_2_setpoint_temperature_number_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH 2 setpoint to %f", temperature); });
  }
  if (this->dhw_setpoint_temperature_number_) {
    this->dhw_setpoint_temperature_number_->setup();
    this->dhw_setpoint_temperature_number_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
#endif
}

void OpenThermComponent::loop() {
  if (this->status_ == OpenThermStatus::READY && !buffer_.empty()) {
    uint32_t request = buffer_.front();
    buffer_.pop();
    this->log_message_(0, "Sending request", request);
    this->send_request_async_(request);
  }

  this->update_spread_();
#ifdef USE_NUMBER
  if (is_elapsed_(last_millis_, 2000)) {
    // The CH setpoint must be written at a fast interval or the boiler
    // might revert to a build-in default as a safety measure.
    if (this->ch_setpoint_temperature_number_) {
      this->request_(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::CH_SETPOINT,
                     this->temperature_to_data_(this->ch_setpoint_temperature_number_->state));
    }
    if (this->ch_2_setpoint_temperature_number_) {
      this->request_(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::CH_SETPOINT_2,
                     this->temperature_to_data_(this->ch_2_setpoint_temperature_number_->state));
    }
    if (this->dhw_setpoint_temperature_number_) {
      if (this->confirmed_dhw_setpoint_ != this->dhw_setpoint_temperature_number_->state) {
        this->request_(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::DHW_SETPOINT,
                       this->temperature_to_data_(this->dhw_setpoint_temperature_number_->state));
      }
    }
  }
#endif

  this->process_();
  yield();
}

void OpenThermComponent::update() { this->set_boiler_status_(); }

void OpenThermComponent::update_spread_() {
#ifdef USE_SENSOR
  if (this->return_temperature_sensor_ && this->should_request_(this->last_millis_return_water_temp_, 0)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::RETURN_WATER_TEMP, 0);
  }
  if (this->boiler_temperature_sensor_ && this->should_request_(this->last_millis_boiler_water_temp_, 1)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::BOILER_WATER_TEMP, 0);
  }
  if (this->boiler_2_temperature_sensor_ && this->should_request_(this->last_millis_boiler_2_water_temp_, 2)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::BOILER_WATER_TEMP_2, 0);
  }
  if (this->dhw_flow_rate_sensor_ && this->should_request_(this->last_millis_flow_rate_, 3)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_FLOW_RATE, 0);
  }
  if (this->pressure_sensor_ && this->should_request_(this->last_millis_ch_pressure_, 4)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::CH_PRESSURE, 0);
  }
  if (this->modulation_sensor_ && this->should_request_(this->last_millis_rel_mod_level_, 5)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::REL_MOD_LEVEL, 0);
  }
  if (this->dhw_temperature_sensor_ && this->should_request_(this->last_millis_dhw_temp_, 6)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_TEMP, 0);
  }
  if (this->dhw_2_temperature_sensor_ && this->should_request_(this->last_millis_dhw_2_temp_, 7)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_TEMP_2, 0);
  }
  if (this->outside_temperature_sensor_ && this->should_request_(this->last_millis_outside_temp_, 8)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::OUTSIDE_TEMP, 0);
  }
  if (this->exhaust_temperature_sensor_ && this->should_request_(this->last_millis_exhaust_temp_, 9)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::BOILER_EXHAUST_TEMP, 0);
  }
  if ((this->dhw_max_temperature_sensor_ || this->dhw_min_temperature_sensor_) &&
      this->should_request_(this->last_millis_dhw_max_min_temp_, 10)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_TEMP_MAX_MIN, 0);
  }
  if ((this->ch_max_temperature_sensor_ || this->ch_min_temperature_sensor_) &&
      this->should_request_(this->last_millis_ch_max_min_temp_, 11)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::CH_TEMP_MAX_MIN, 0);
  }
  if (this->oem_diagnostic_code_sensor_ && this->should_request_(this->last_millis_oem_diagnostic_code_, 12)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::OEM_DIAGNOSTIC_CODE, 0);
  }
  if (this->burner_starts_sensor_ && this->should_request_(this->last_millis_burner_starts_, 13)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::BURNER_STARTS, 0);
  }
  if (this->burner_ops_hours_sensor_ && this->should_request_(this->last_millis_burner_ops_hours_, 14)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::BURNER_OPS_HOURS, 0);
  }
  if (this->ch_pump_starts_sensor_ && this->should_request_(this->last_millis_ch_pump_starts_, 15)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::CH_PUMP_STARTS, 0);
  }
  if (this->ch_pump_ops_hours_sensor_ && this->should_request_(this->last_millis_ch_pump_ops_hours_, 16)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::CH_PUMP_OPS_HOURS, 0);
  }
  if (this->dhw_pump_valve_starts_sensor_ && this->should_request_(this->last_millis_dhw_pump_valve_starts_, 17)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_PUMP_VALVE_STARTS, 0);
  }
  if (this->dhw_pump_valve_ops_hours_sensor_ &&
      this->should_request_(this->last_millis_dhw_pump_valve_ops_hours_, 18)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_PUMP_VALVE_OPS_HOURS, 0);
  }
  if (this->dhw_burner_starts_sensor_ && this->should_request_(this->last_millis_dhw_burner_starts_, 19)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_BURNER_STARTS, 0);
  }
  if (this->dhw_burner_ops_hours_sensor_ && this->should_request_(this->last_millis_dhw_burner_ops_hours_, 20)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::DHW_BURNER_OPS_HOURS, 0);
  }
#endif
#if defined USE_BINARY_SENSOR || defined USE_SENSOR
  if ((
#endif
#ifdef USE_BINARY_SENSOR
          this->service_request_binary_sensor_ || this->lockout_reset_binary_sensor_ ||
          this->water_pressure_fault_binary_sensor_ || this->gas_flame_fault_binary_sensor_ ||
          this->air_pressure_fault_binary_sensor_ || this->water_over_temperature_fault_binary_sensor_
#endif
#if defined USE_BINARY_SENSOR && defined USE_SENSOR
          ||
#endif
#ifdef USE_SENSOR
          this->oem_error_code_sensor_
#endif
#if defined USE_BINARY_SENSOR || defined USE_SENSOR
          ) &&
      this->should_request_(this->last_millis_fault_flags_, 21)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::APP_SPEC_FAULT_FLAGS, 0);
  }
#endif
#ifdef USE_BINARY_SENSOR
  if ((this->dhw_present_binary_sensor_ || this->modulating_binary_sensor_ || this->cooling_supported_binary_sensor_ ||
       this->dhw_storage_tank_binary_sensor_ || this->device_lowoff_pump_control_binary_sensor_ ||
       this->ch_2_present_binary_sensor_) &&
      this->should_request_(this->last_millis_boiler_configuration_, 22)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::BOILER_CONFIGURATION, 0);
  }
#endif
  if (should_request_(this->last_millis_param_flags_, 23)) {
    this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::REMOTE_PARAM_FLAGS, 0);
  }
}

void OpenThermComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm:");
  LOG_PIN("  Write Pin: ", this->write_pin_);
  LOG_PIN("  Read Pin: ", this->read_pin_);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "CH min temperature:", this->ch_min_temperature_sensor_);
  LOG_SENSOR("  ", "CH max temperature:", this->ch_max_temperature_sensor_);
  LOG_SENSOR("  ", "DHW min temperature:", this->dhw_min_temperature_sensor_);
  LOG_SENSOR("  ", "DHW max temperature:", this->dhw_max_temperature_sensor_);
  LOG_SENSOR("  ", "DHW flow rate:", this->dhw_flow_rate_sensor_);
  LOG_SENSOR("  ", "Pressure:", this->pressure_sensor_);
  LOG_SENSOR("  ", "Modulation:", this->modulation_sensor_);
  LOG_SENSOR("  ", "DHW temperature:", this->dhw_temperature_sensor_);
  LOG_SENSOR("  ", "DHW 2 temperature:", this->dhw_2_temperature_sensor_);
  LOG_SENSOR("  ", "Boiler temperature:", this->boiler_temperature_sensor_);
  LOG_SENSOR("  ", "Boiler 2 temperature:", this->boiler_2_temperature_sensor_);
  LOG_SENSOR("  ", "Return temperature:", this->return_temperature_sensor_);
  LOG_SENSOR("  ", "Outside temperature:", this->outside_temperature_sensor_);
  LOG_SENSOR("  ", "Exhaust temperature:", this->exhaust_temperature_sensor_);
  LOG_SENSOR("  ", "OEM error code:", this->oem_error_code_sensor_);
  LOG_SENSOR("  ", "OEM diagnostic code:", this->oem_diagnostic_code_sensor_);
  LOG_SENSOR("  ", "Burner starts:", this->burner_starts_sensor_);
  LOG_SENSOR("  ", "Burner operation hours:", this->burner_ops_hours_sensor_);
  LOG_SENSOR("  ", "CH pump starts:", this->ch_pump_starts_sensor_);
  LOG_SENSOR("  ", "CP pump operation hours:", this->ch_pump_ops_hours_sensor_);
  LOG_SENSOR("  ", "DHW pump/valve starts:", this->dhw_pump_valve_starts_sensor_);
  LOG_SENSOR("  ", "DHW pump/valve operation hours:", this->dhw_pump_valve_ops_hours_sensor_);
  LOG_SENSOR("  ", "DHW burner starts:", this->dhw_burner_starts_sensor_);
  LOG_SENSOR("  ", "DHW burner operation hours:", this->dhw_burner_ops_hours_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "CH active:", this->ch_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "CH 2 active:", this->ch_2_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "DHW active:", this->dhw_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Cooling active:", this->cooling_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Flame active:", this->flame_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Fault:", fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Diagnostic:", this->diagnostic_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Service request:", this->service_request_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Lockout reset:", this->lockout_reset_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Water pressure fault:", this->water_pressure_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Gas/flame fault:", this->gas_flame_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Air pressure fault:", this->air_pressure_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Water over temperature fault:", this->water_over_temperature_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "DHW present:", this->dhw_present_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Modulating :", this->modulating_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Cooling supported:", this->cooling_supported_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "DHW storage tank:", this->dhw_storage_tank_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Device low-off/pump control allowed:", this->device_lowoff_pump_control_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "CH 2 present:", this->ch_2_present_binary_sensor_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "Boiler lock-out reset:", this->boiler_lo_reset_button_);
  LOG_BUTTON("  ", "CH water filling:", this->ch_water_filling_button_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "CH enabled:", this->ch_enabled_switch_);
  LOG_SWITCH("  ", "CH 2 enabled:", this->ch_2_enabled_switch_);
  LOG_SWITCH("  ", "DHW enabled:", this->dhw_enabled_switch_);
  LOG_SWITCH("  ", "Cooling enabled:", this->cooling_enabled_switch_);
  LOG_SWITCH("  ", "OTC active:", this->otc_active_switch_);
#endif
#ifdef USE_NUMBER
  if (this->ch_setpoint_temperature_number_) {
    LOG_NUMBER("  ", "CH setpoint temperature:", this->ch_setpoint_temperature_number_);
    this->ch_setpoint_temperature_number_->dump_custom_config("  ");
  }
  if (this->ch_2_setpoint_temperature_number_) {
    LOG_NUMBER("  ", "CH 2 setpoint temperature:", this->ch_2_setpoint_temperature_number_);
    this->ch_2_setpoint_temperature_number_->dump_custom_config("  ");
  }
  if (this->dhw_setpoint_temperature_number_) {
    LOG_NUMBER("  ", "DHW setpoint temperature:", this->dhw_setpoint_temperature_number_);
    this->dhw_setpoint_temperature_number_->dump_custom_config("  ");
  }
#endif
}

void IRAM_ATTR OpenThermComponent::handle_interrupt(OpenThermComponent *component) {
  if (component->status_ == OpenThermStatus::READY) {
    return;
  }

  uint32_t new_ts = micros();
  bool pin_state = component->isr_read_pin_.digital_read();
  if (component->status_ == OpenThermStatus::RESPONSE_WAITING) {
    if (pin_state) {
      component->status_ = OpenThermStatus::RESPONSE_START_BIT;
      component->response_timestamp_ = new_ts;
    } else {
      component->status_ = OpenThermStatus::RESPONSE_INVALID;
      component->response_timestamp_ = new_ts;
    }
  } else if (component->status_ == OpenThermStatus::RESPONSE_START_BIT) {
    if ((new_ts - component->response_timestamp_ < 750) && !pin_state) {
      component->status_ = OpenThermStatus::RESPONSE_RECEIVING;
      component->response_timestamp_ = new_ts;
      component->response_bit_index_ = 0;
    } else {
      component->status_ = OpenThermStatus::RESPONSE_INVALID;
      component->response_timestamp_ = new_ts;
    }
  } else if (component->status_ == OpenThermStatus::RESPONSE_RECEIVING) {
    if ((new_ts - component->response_timestamp_) > 750) {
      if (component->response_bit_index_ < 32) {
        component->response_ = (component->response_ << 1) | !pin_state;
        component->response_timestamp_ = new_ts;
        component->response_bit_index_++;
      } else {  // stop bit
        component->status_ = OpenThermStatus::RESPONSE_READY;
        component->response_timestamp_ = new_ts;
      }
    }
  }
}

void OpenThermComponent::boiler_lo_reset() {
  ESP_LOGI(TAG, "Execute: Boiler lock-out reset");
  this->request_(opentherm::OpenThermMessageType::WRITE_DATA, opentherm::OpenThermMessageID::COMMAND, 0x100);
}

void OpenThermComponent::ch_water_filling() {
  ESP_LOGI(TAG, "Execute: CH water filling");
  this->request_(opentherm::OpenThermMessageType::WRITE_DATA, opentherm::OpenThermMessageID::COMMAND, 0x200);
}

// Private

void OpenThermComponent::log_message_(uint8_t level, const char *pre_message, uint32_t message) {
  switch (level) {
    case 0:
      ESP_LOGD(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type_(message),
               this->get_data_id_(message), this->get_uint16_(message));
      break;
    default:
      ESP_LOGW(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type_(message),
               this->get_data_id_(message), this->get_uint16_(message));
  }
}

void OpenThermComponent::send_bit_(bool high) {
  if (high) {
    this->write_pin_->digital_write(false);
  } else {
    this->write_pin_->digital_write(true);
  }
  delayMicroseconds(500);
  if (high) {
    this->write_pin_->digital_write(true);
  } else {
    this->write_pin_->digital_write(false);
  }
  delayMicroseconds(500);
}

bool OpenThermComponent::send_request_async_(uint32_t request) {
  bool ready = false;
  {
    InterruptLock lock;
    ready = this->status_ == OpenThermStatus::READY;
  }

  if (!ready) {
    return false;
  }

  this->status_ = OpenThermStatus::REQUEST_SENDING;
  this->response_ = 0;
  this->response_status_ = OpenThermResponseStatus::NONE;

  this->send_bit_(true);  // Start bit
  for (int i = 31; i >= 0; i--) {
    this->send_bit_(request >> i & 1);
  }
  this->send_bit_(true);  // Stop bit
  this->write_pin_->digital_write(true);

  this->status_ = OpenThermStatus::RESPONSE_WAITING;
  this->response_timestamp_ = micros();
  return true;
}

void OpenThermComponent::process_() {
  OpenThermStatus st = OpenThermStatus::NOT_INITIALIZED;
  uint32_t ts = 0;
  {
    InterruptLock lock;
    st = this->status_;
    ts = this->response_timestamp_;
  }

  if (st == OpenThermStatus::READY) {
    return;
  }

  uint32_t new_ts = micros();
  if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (new_ts - ts) > 1000000) {
    this->status_ = OpenThermStatus::READY;
    this->response_status_ = OpenThermResponseStatus::TIMEOUT;
    this->process_response_(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::RESPONSE_INVALID) {
    this->status_ = OpenThermStatus::DELAY;
    this->response_status_ = OpenThermResponseStatus::INVALID;
    this->process_response_(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::RESPONSE_READY) {
    this->status_ = OpenThermStatus::DELAY;
    this->response_status_ =
        this->is_valid_response_(this->response_) ? OpenThermResponseStatus::SUCCESS : OpenThermResponseStatus::INVALID;
    this->process_response_(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::DELAY) {
    if ((new_ts - ts) > 100000) {
      this->status_ = OpenThermStatus::READY;
    }
  }
}

bool OpenThermComponent::parity_(uint32_t frame)  // odd parity
{
  uint8_t p = 0;
  while (frame > 0) {
    if (frame & 1)
      p++;
    frame = frame >> 1;
  }
  return (p & 1);
}

OpenThermMessageType OpenThermComponent::get_message_type_(uint32_t message) {
  OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
  return msg_type;
}

OpenThermMessageID OpenThermComponent::get_data_id_(uint32_t frame) {
  return (OpenThermMessageID) ((frame >> 16) & 0xFF);
}

uint32_t OpenThermComponent::build_request_(OpenThermMessageType type, OpenThermMessageID id, uint32_t data) {
  uint32_t request = data;
  if (type == OpenThermMessageType::WRITE_DATA) {
    request |= 1ul << 28;
  }
  request |= ((uint32_t) id) << 16;
  if (this->parity_(request))
    request |= (1ul << 31);
  return request;
}

uint32_t OpenThermComponent::build_response_(OpenThermMessageType type, OpenThermMessageID id, uint32_t data) {
  uint32_t response = data;
  response |= type << 28;
  response |= ((uint32_t) id) << 16;
  if (this->parity_(response))
    response |= (1ul << 31);
  return response;
}

bool OpenThermComponent::is_valid_response_(uint32_t response) {
  if (this->parity_(response))
    return false;
  uint8_t msg_type = (response << 1) >> 29;
  return msg_type == READ_ACK || msg_type == WRITE_ACK;
}

const char *OpenThermComponent::message_type_to_string_(OpenThermMessageType message_type) {
  switch (message_type) {
    case READ_DATA:
      return "READ_DATA";
    case WRITE_DATA:
      return "WRITE_DATA";
    case INVALID_DATA:
      return "INVALID_DATA";
    case RESERVED:
      return "RESERVED";
    case READ_ACK:
      return "READ_ACK";
    case WRITE_ACK:
      return "WRITE_ACK";
    case DATA_INVALID:
      return "DATA_INVALID";
    case UNKNOWN_DATA_ID:
      return "UNKNOWN_DATA_ID";
    default:
      return "UNKNOWN";
  }
}

void OpenThermComponent::process_response_(uint32_t response, OpenThermResponseStatus response_status) {
  if (response_status == OpenThermResponseStatus::SUCCESS) {
    this->log_message_(0, "Received response", response);
    switch (this->get_data_id_(response)) {
#ifdef USE_BINARY_SENSOR
      case OpenThermMessageID::STATUS:
        this->publish_binary_sensor_state_(this->fault_binary_sensor_, response & 0x01);
        this->publish_binary_sensor_state_(this->ch_active_binary_sensor_, response & 0x02);
        this->publish_binary_sensor_state_(this->dhw_active_binary_sensor_, response & 0x04);
        this->publish_binary_sensor_state_(this->flame_active_binary_sensor_, response & 0x08);
        this->publish_binary_sensor_state_(this->cooling_active_binary_sensor_, response & 0x10);
        this->publish_binary_sensor_state_(this->ch_2_active_binary_sensor_, response & 0x20);
        this->publish_binary_sensor_state_(this->diagnostic_binary_sensor_, response & 0x40);
        break;
      case OpenThermMessageID::BOILER_CONFIGURATION:
        this->publish_binary_sensor_state_(this->dhw_present_binary_sensor_, response & 0x01);
        this->publish_binary_sensor_state_(this->modulating_binary_sensor_, !(response & 0x02));
        this->publish_binary_sensor_state_(this->cooling_supported_binary_sensor_, response & 0x04);
        this->publish_binary_sensor_state_(this->dhw_storage_tank_binary_sensor_, response & 0x08);
        this->publish_binary_sensor_state_(this->device_lowoff_pump_control_binary_sensor_, !(response & 0x10));
        this->publish_binary_sensor_state_(this->ch_2_present_binary_sensor_, response & 0x20);
        break;
#endif
#if defined USE_BINARY_SENSOR || defined USE_SENSOR
      case OpenThermMessageID::APP_SPEC_FAULT_FLAGS:
#endif
#ifdef USE_BINARY_SENSOR
        this->publish_binary_sensor_state_(this->service_request_binary_sensor_, response & 0x0100);
        this->publish_binary_sensor_state_(this->lockout_reset_binary_sensor_, response & 0x0200);
        this->publish_binary_sensor_state_(this->water_pressure_fault_binary_sensor_, response & 0x0400);
        this->publish_binary_sensor_state_(this->gas_flame_fault_binary_sensor_, response & 0x0800);
        this->publish_binary_sensor_state_(this->air_pressure_fault_binary_sensor_, response & 0x1000);
        this->publish_binary_sensor_state_(this->water_over_temperature_fault_binary_sensor_, response & 0x2000);
#endif
#ifdef USE_SENSOR
        this->publish_sensor_state_(this->oem_error_code_sensor_, response & 0xff);
#endif
#if defined USE_BINARY_SENSOR || defined USE_SENSOR
        break;
#endif
#ifdef USE_SENSOR
      case OpenThermMessageID::RETURN_WATER_TEMP:
        this->publish_sensor_state_(this->return_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::BOILER_WATER_TEMP:
        this->publish_sensor_state_(this->boiler_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::BOILER_WATER_TEMP_2:
        this->publish_sensor_state_(this->boiler_2_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::OUTSIDE_TEMP:
        this->publish_sensor_state_(this->outside_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::BOILER_EXHAUST_TEMP:
        this->publish_sensor_state_(this->exhaust_temperature_sensor_, this->get_int16_(response));
        break;
      case OpenThermMessageID::DHW_FLOW_RATE:
        this->publish_sensor_state_(this->dhw_flow_rate_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::CH_PRESSURE:
        this->publish_sensor_state_(this->pressure_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::REL_MOD_LEVEL:
        this->publish_sensor_state_(this->modulation_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::DHW_TEMP:
        this->publish_sensor_state_(this->dhw_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::DHW_TEMP_2:
        this->publish_sensor_state_(this->dhw_2_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::OEM_DIAGNOSTIC_CODE:
        this->publish_sensor_state_(this->oem_diagnostic_code_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::BURNER_STARTS:
        this->publish_sensor_state_(this->burner_starts_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::BURNER_OPS_HOURS:
        this->publish_sensor_state_(this->burner_ops_hours_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::CH_PUMP_STARTS:
        this->publish_sensor_state_(this->ch_pump_starts_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::CH_PUMP_OPS_HOURS:
        this->publish_sensor_state_(this->ch_pump_ops_hours_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::DHW_PUMP_VALVE_STARTS:
        this->publish_sensor_state_(this->dhw_pump_valve_starts_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::DHW_PUMP_VALVE_OPS_HOURS:
        this->publish_sensor_state_(this->dhw_pump_valve_ops_hours_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::DHW_BURNER_STARTS:
        this->publish_sensor_state_(this->dhw_burner_starts_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::DHW_BURNER_OPS_HOURS:
        this->publish_sensor_state_(this->dhw_burner_ops_hours_sensor_, this->get_uint16_(response));
        break;
      case OpenThermMessageID::DHW_TEMP_MAX_MIN:
        this->publish_sensor_state_(this->dhw_max_temperature_sensor_, response >> 8 & 0xFF);
        this->publish_sensor_state_(this->dhw_min_temperature_sensor_, response & 0xFF);
        break;
      case OpenThermMessageID::CH_TEMP_MAX_MIN:
        this->publish_sensor_state_(this->ch_max_temperature_sensor_, response >> 8 & 0xFF);
        this->publish_sensor_state_(this->ch_min_temperature_sensor_, response & 0xFF);
        break;
#endif
      case OpenThermMessageID::DHW_SETPOINT:
        if (this->get_message_type_(response) == OpenThermMessageType::WRITE_ACK) {
          this->confirmed_dhw_setpoint_ = this->get_float_(response);
        }
        break;
      default:
        break;
    }
  } else if (response_status == OpenThermResponseStatus::NONE) {
    ESP_LOGW(TAG, "OpenTherm is not initialized");
  } else if (response_status == OpenThermResponseStatus::TIMEOUT) {
    ESP_LOGW(TAG, "Request timeout");
  } else if (response_status == OpenThermResponseStatus::INVALID) {
    this->log_message_(2, "Received invalid response", response);
  }
}

#ifdef USE_SENSOR
void OpenThermComponent::publish_sensor_state_(sensor::Sensor *sensor, float state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}
#endif

#ifdef USE_BINARY_SENSOR
void OpenThermComponent::publish_binary_sensor_state_(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}
#endif

void OpenThermComponent::request_(OpenThermMessageType type, OpenThermMessageID id, uint32_t data) {
  this->enqueue_request_(this->build_request_(type, id, data));
}

void OpenThermComponent::set_boiler_status_() {
  uint32_t data = this->wanted_ch_enabled_ | (this->wanted_dhw_enabled_ << 1) | (this->wanted_cooling_enabled_ << 2) |
                  (this->wanted_otc_active_ << 3) | (this->wanted_ch_2_enabled_ << 4);
  data <<= 8;
  this->request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::STATUS, data);
}

void OpenThermComponent::enqueue_request_(uint32_t request) {
  if (this->buffer_.size() > 25) {
    this->log_message_(2, "Queue full. Discarded request", request);
  } else {
    this->buffer_.push(request);
    this->log_message_(0, "Enqueued request", request);
  }
}

bool OpenThermComponent::can_start_(uint32_t last_millis, uint8_t index) {
  if (last_millis > 0) {
    return true;
  }
  uint32_t now = millis();
  if (this->start_millis_ > now) {
    this->start_millis_ = now;
    return false;
  }
  bool can_start = now > this->start_millis_ + (this->start_interval_ * index);
  return can_start;
}

bool OpenThermComponent::should_request_(uint32_t &last_millis, uint8_t index) {
  return can_start_(last_millis, index) && is_elapsed_(last_millis, this->get_update_interval());
}

const char *OpenThermComponent::format_message_type_(uint32_t message) {
  return this->message_type_to_string_(this->get_message_type_(message));
}

}  // namespace opentherm
}  // namespace esphome
