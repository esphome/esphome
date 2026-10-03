#include "pd.h"

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::fusb302b {

static const char *const TAG = "fusb302b.pd";

static constexpr uint32_t AMS_TIMEOUT_MS = 2000;
// tSinkRequest: delay before repeating a request that the source answered with Wait
static constexpr uint32_t SINK_REQUEST_MS = 100;
static constexpr uint8_t MAX_WAIT_RETRIES = 10;

// USB PD 6.4.1.2.3 Source Fixed Supply Power Data Object; other PDO types only report their type
static PdContract parse_pdo(uint32_t pdo) {
  PdContract info{};
  info.type = static_cast<PdPdoType>(pdo >> 30);
  if (info.type == PD_PDO_TYPE_FIXED_SUPPLY) {
    info.max_v = (pdo >> 10) & 0x3FF;
    info.max_i = pdo & 0x3FF;
  }
  return info;
}

void PdMsg::set_header(uint16_t header) {
  this->type = header & 0x1F;
  this->spec_rev = static_cast<PdSpecRevision>((header >> 6) & 0x3);
  this->id = (header >> 9) & 0x7;
  this->num_of_obj = (header >> 12) & 0x7;
  this->extended = (header >> 15) & 0x1;
}

uint16_t PdMsg::get_coded_header() const {
  // Data role UFP (bit 5 = 0), power role sink (bit 8 = 0)
  return static_cast<uint16_t>(this->type & 0x1F) | (static_cast<uint16_t>(this->spec_rev) << 6) |
         (static_cast<uint16_t>(this->id & 0x7) << 9) | (static_cast<uint16_t>(this->num_of_obj & 0x7) << 12) |
         (static_cast<uint16_t>(this->extended) << 15);
}

PdMsg PowerDelivery::make_control_msg_(PdControlMsgType type) const {
  PdMsg msg;
  msg.type = type;
  msg.id = this->msg_counter_ % 8;
  return msg;
}

PdMsg PowerDelivery::make_data_msg_(PdDataMsgType type, const uint32_t *objects, uint8_t len) const {
  PdMsg msg;
  msg.type = type;
  msg.id = this->msg_counter_ % 8;
  msg.num_of_obj = len;
  memcpy(msg.data_objects, objects, len * sizeof(uint32_t));
  return msg;
}

void PowerDelivery::reset_protocol_() {
  this->last_received_msg_id_ = 255;
  this->msg_counter_ = 0;
  this->active_ams_ = false;
  this->retry_request_pending_ = false;
  this->wait_retries_ = 0;
}

void PowerDelivery::set_state_(PdState state) {
  if (this->state_.exchange(state) != state)
    this->on_state_changed();
}

void PowerDelivery::set_contract_(const PdContract &contract) {
  this->accepted_contract_ = contract;
  const uint32_t packed = PackedContract::from(contract).value;
  if (this->contract_.exchange(packed) != packed)
    this->on_state_changed();
}

void PowerDelivery::clear_contract_() {
  this->accepted_contract_ = {};
  if (this->contract_.exchange(0) != 0)
    this->on_state_changed();
}

void PowerDelivery::set_ams_(bool active) {
  this->active_ams_ = active;
  if (active)
    this->ams_start_ = millis();
}

bool PowerDelivery::check_ams_() {
  if (this->active_ams_ && millis() - this->ams_start_ > AMS_TIMEOUT_MS)
    this->active_ams_ = false;
  return this->active_ams_;
}

void PowerDelivery::check_request_retry_() {
  if (!this->retry_request_pending_ || millis() - this->wait_received_ms_ < SINK_REQUEST_MS)
    return;
  this->retry_request_pending_ = false;
  this->set_ams_(true);
  this->send_message(this->make_data_msg_(PD_DATA_REQUEST, &this->last_rdo_, 1));
}

bool PowerDelivery::transition_timed_out_() {
  // The AMS that sent the request only ends on PS_RDY, so an ended AMS while in transition means a timeout
  return this->state_ == PdState::PD_STATE_TRANSITION && !this->check_ams_();
}

void PowerDelivery::handle_message_(const PdMsg &msg) {
  if (msg.num_of_obj == 0 && msg.type == PD_CNTRL_GOODCRC) {
    // Our last message was received, so the next one gets a new MessageID
    this->msg_counter_++;
    return;
  }
  // Soft_Reset always has MessageID 0, so it must not be dropped as a retransmission of the last message
  if (msg.num_of_obj == 0 && msg.type == PD_CNTRL_SOFT_RESET) {
    this->handle_soft_reset_(msg);
    return;
  }
  // Retransmissions repeat the MessageID of a message that was already handled
  if (msg.id == this->last_received_msg_id_)
    return;
  this->last_received_msg_id_ = msg.id;

  if (msg.num_of_obj == 0) {
    this->handle_control_message_(msg);
  } else {
    this->handle_data_message_(msg);
  }
}

void PowerDelivery::handle_data_message_(const PdMsg &msg) {
  if (msg.extended || msg.type != PD_DATA_SOURCE_CAP)
    return;
  this->set_ams_(true);
  this->waiting_for_source_caps_ = false;
  this->respond_to_source_caps_(msg);
}

void PowerDelivery::handle_control_message_(const PdMsg &msg) {
  switch (msg.type) {
    case PD_CNTRL_ACCEPT:
      if (this->active_ams_) {
        if (this->requested_contract_ != this->accepted_contract_)
          this->set_state_(PdState::PD_STATE_TRANSITION);
        this->set_contract_(this->requested_contract_);
      }
      break;
    case PD_CNTRL_PS_RDY:
      this->set_ams_(false);
      this->set_state_(PdState::PD_STATE_EXPLICIT_CONTRACT);
      break;
    case PD_CNTRL_REJECT:
      ESP_LOGW(TAG, "Source rejected the request");
      this->set_ams_(false);
      break;
    case PD_CNTRL_WAIT:
      if (this->active_ams_ && this->wait_retries_ < MAX_WAIT_RETRIES) {
        ESP_LOGD(TAG, "Source asked to wait, repeating the request");
        this->wait_retries_++;
        this->retry_request_pending_ = true;
        this->wait_received_ms_ = millis();
      } else {
        ESP_LOGW(TAG, "Source kept answering Wait, giving up");
      }
      this->set_ams_(false);
      break;
    case PD_CNTRL_PING:
    case PD_CNTRL_NOT_SUPPORTED:
      break;
    case PD_CNTRL_GET_SINK_CAP: {
      // USB PD 6.4.1.2.3 Sink Fixed Supply PDO: 5 V, 3 A operational current (300 x 10 mA), USB comms capable
      static constexpr uint32_t SINK_PDO =
          (300u << 0) | (100u << 10) | (1u << 26) | (static_cast<uint32_t>(PD_PDO_TYPE_FIXED_SUPPLY) << 30);
      this->send_message(this->make_data_msg_(PD_DATA_SINK_CAP, &SINK_PDO, 1));
      break;
    }
    default:
      // Messages are sent as PD 2.0, which has no Not_Supported message
      this->send_message(this->make_control_msg_(PD_CNTRL_REJECT));
      break;
  }
}

void PowerDelivery::handle_soft_reset_(const PdMsg &msg) {
  this->reset_protocol_();
  this->last_received_msg_id_ = msg.id;
  this->send_message(this->make_control_msg_(PD_CNTRL_ACCEPT));
  // A soft reset keeps the power level, but the outcome of a transition in progress is unknown
  if (this->state_ == PdState::PD_STATE_TRANSITION) {
    this->set_contract_(DEFAULT_CONTRACT);
    this->set_state_(PdState::PD_STATE_DEFAULT_CONTRACT);
  }
  this->on_soft_reset_received();
}

void PowerDelivery::respond_to_source_caps_(const PdMsg &msg) {
  // Pick the highest fixed supply PDO that does not exceed the requested voltage.
  // The first PDO is always the 5 V fixed supply.
  const uint16_t request_v = this->request_voltage_ * 20;  // 50 mV units
  PdContract selected{};
  uint8_t position = 0;  // 1-based object position, 0 = none
  for (uint8_t idx = 0; idx < msg.num_of_obj; idx++) {
    PdContract info = parse_pdo(msg.data_objects[idx]);
    if (info.type != PD_PDO_TYPE_FIXED_SUPPLY)
      continue;
    if (position == 0 || (info.max_v <= request_v && info.max_v > selected.max_v)) {
      selected = info;
      position = idx + 1;
    }
  }

  // USB PD 6.4.2 Request Data Object: no USB suspend (bit 24), USB communications capable (bit 25)
  uint32_t rdo = (1u << 24) | (1u << 25);
  if (position == 0) {
    ESP_LOGW(TAG, "No usable PDO in source capabilities, requesting 5 V");
    // Object 1 (5 V fixed supply), 300 mA maximum, 100 mA operating current
    selected = PdContract{PD_PDO_TYPE_FIXED_SUPPLY, 100, 30};
    rdo |= (30u << 0) | (10u << 10) | (1u << 28);
  } else {
    // Request the full current the source offers
    const uint32_t current = selected.max_i;
    rdo |= (current << 0) | (current << 10) | (static_cast<uint32_t>(position) << 28);
  }
  ESP_LOGD(TAG, "Requesting PDO %u: %.2f V", position, selected.max_v * 0.05f);
  this->requested_contract_ = selected;
  this->last_rdo_ = rdo;
  this->retry_request_pending_ = false;
  this->wait_retries_ = 0;
  this->send_message(this->make_data_msg_(PD_DATA_REQUEST, &rdo, 1));
}

}  // namespace esphome::fusb302b
