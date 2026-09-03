#include "hub.h"
#include <algorithm>
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42";

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
    default:
      break;  // every startup-only kind is handled by build_startup_request_(), unreachable here
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
  }
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
