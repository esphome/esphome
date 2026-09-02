#include "hub.h"
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
}

Frame OpenTherm42Hub::build_next_request_() {
  Frame frame{};

  if (!this->boiler_config_read_) {
    this->pending_request_kind_ = RequestKind::BOILER_CONFIG;
    frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
    frame.id = 3;
    return frame;
  }

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
    case RequestKind::BOILER_CONFIG:
      break;  // handled above, unreachable here
  }
  return frame;
}

void OpenTherm42Hub::handle_response_(const Frame &frame) {
  auto const type = static_cast<MessageType>(frame.type);
  switch (this->pending_request_kind_) {
    case RequestKind::BOILER_CONFIG:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Boiler configuration flags (id=3) read was rejected (message type %u)", frame.type);
        return;
      }
      this->boiler_config_flags_ = frame.value_hb;
      this->boiler_member_id_code_ = frame.value_lb;
      this->boiler_config_read_ = true;
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
  }
}

void OpenTherm42Hub::invalidate_response_(RequestKind kind) {
  switch (kind) {
    case RequestKind::BOILER_CONFIG:
    case RequestKind::CONTROL_SETPOINT:
    case RequestKind::CONTROL_SETPOINT_2:
    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      return;  // write-only or startup-retry kinds have no read-only entity to invalidate

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
  }
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
