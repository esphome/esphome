#include "pd.h"

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::fusb302b {

static const char *const TAG = "fusb302b.pd";

static constexpr uint32_t AMS_TIMEOUT_MS = 2000;

static PdContract parse_pdo(uint32_t pdo) {
  PdContract info{};
  info.type = static_cast<PdPdoType>(pdo >> 30);
  switch (info.type) {
    case PD_PDO_TYPE_FIXED_SUPPLY:
      // USB PD 6.4.1.2.3 Source Fixed Supply Power Data Object
      info.max_v = (pdo >> 10) & 0x3FF;
      info.max_i = pdo & 0x3FF;
      break;
    case PD_PDO_TYPE_BATTERY:
      // USB PD 6.4.1.2.5 Battery Supply Power Data Object
      info.min_v = (pdo >> 10) & 0x3FF;
      info.max_v = (pdo >> 20) & 0x3FF;
      info.max_p = pdo & 0x3FF;
      break;
    case PD_PDO_TYPE_VARIABLE_SUPPLY:
      // USB PD 6.4.1.2.4 Variable Supply (non-Battery) Power Data Object
      info.min_v = (pdo >> 10) & 0x3FF;
      info.max_v = (pdo >> 20) & 0x3FF;
      info.max_i = pdo & 0x3FF;
      break;
    case PD_PDO_TYPE_AUGMENTED:
      // USB PD 6.4.1.3.4 Programmable Power Supply APDO (100 mV and 50 mA units)
      info.max_v = ((pdo >> 17) & 0xFF) * 2;
      info.min_v = ((pdo >> 8) & 0xFF) * 2;
      info.max_i = (pdo & 0x7F) * 5;
      break;
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

bool PowerDelivery::is_connected() const {
  PdState state = this->state_;
  return state == PdState::PD_STATE_DEFAULT_CONTRACT || state == PdState::PD_STATE_TRANSITION ||
         state == PdState::PD_STATE_EXPLICIT_CONTRACT || state == PdState::PD_STATE_PD_TIMEOUT;
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

void PowerDelivery::handle_message_(const PdMsg &msg) {
  if (msg.num_of_obj == 0 && msg.type == PD_CNTRL_GOODCRC) {
    // Our last message was received, so the next one gets a new MessageID
    this->msg_counter_++;
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
    case PD_CNTRL_WAIT:
      ESP_LOGW(TAG, "Source refused the request (%s)",
               msg.type == PD_CNTRL_REJECT ? LOG_STR_LITERAL("reject") : LOG_STR_LITERAL("wait"));
      this->set_ams_(false);
      break;
    case PD_CNTRL_PING:
      break;
    case PD_CNTRL_SOFT_RESET: {
      PdMsg accept = this->make_control_msg_(PD_CNTRL_ACCEPT);
      accept.id = 0;
      this->send_message(accept);
      this->msg_counter_ = 0;
      this->set_state_(PdState::PD_STATE_DEFAULT_CONTRACT);
      break;
    }
    case PD_CNTRL_GET_SINK_CAP: {
      // USB PD 6.4.1.2.3 Sink Fixed Supply PDO: 5 V, 5 A operational current (500 x 10 mA), USB comms capable
      static constexpr uint32_t SINK_PDO =
          (500u << 0) | (100u << 10) | (1u << 26) | (static_cast<uint32_t>(PD_PDO_TYPE_FIXED_SUPPLY) << 30);
      this->send_message(this->make_data_msg_(PD_DATA_SINK_CAP, &SINK_PDO, 1));
      break;
    }
    default:
      this->send_message(this->make_control_msg_(PD_CNTRL_NOT_SUPPORTED));
      break;
  }
}

void PowerDelivery::respond_to_source_caps_(const PdMsg &msg) {
  // Pick the highest fixed, variable or battery PDO that does not exceed the requested voltage.
  // The first PDO is always the 5 V fixed supply.
  const uint16_t request_v = this->request_voltage_ * 20;  // 50 mV units
  PdContract selected{};
  uint8_t position = 0;  // 1-based object position, 0 = none
  for (uint8_t idx = 0; idx < msg.num_of_obj; idx++) {
    PdContract info = parse_pdo(msg.data_objects[idx]);
    if (info.type == PD_PDO_TYPE_AUGMENTED)
      continue;
    if (info.max_v <= request_v || position == 0) {
      selected = info;
      position = idx + 1;
    }
  }

  // USB PD 6.4.2 Request Data Object: no USB suspend (bit 24), USB communications capable (bit 25)
  uint32_t rdo = (1u << 24) | (1u << 25);
  if (position == 0) {
    ESP_LOGW(TAG, "No usable PDO in source capabilities, requesting 5 V");
    // Object 1 (5 V fixed supply), 300 mA maximum, 100 mA operating current
    selected = PdContract{PD_PDO_TYPE_FIXED_SUPPLY, 0, 100, 30, 0};
    rdo |= (30u << 0) | (10u << 10) | (1u << 28);
  } else {
    // Request the full current (or power, for battery PDOs) the source offers
    uint32_t amount = selected.max_i != 0 ? selected.max_i : selected.max_p;
    rdo |= (amount << 0) | (amount << 10) | (static_cast<uint32_t>(position) << 28);
  }
  ESP_LOGD(TAG, "Requesting PDO %u: %.2f V", position, selected.max_v * 0.05f);
  this->requested_contract_ = selected;
  this->send_message(this->make_data_msg_(PD_DATA_REQUEST, &rdo, 1));
}

}  // namespace esphome::fusb302b
