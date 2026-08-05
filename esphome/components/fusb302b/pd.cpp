#include "pd.h"

#include <cstdio>
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fusb302b {

static const char *const TAG = "PowerDelivery";

static PdContract pd_parse_power_info(const PdPdo &pdo) {
  PdContract power_info{};
  power_info.type = static_cast<PdPowerDataObjType>(pdo >> 30);
  switch (power_info.type) {
    case PD_PDO_TYPE_FIXED_SUPPLY:
      /* Reference: 6.4.1.2.3 Source Fixed Supply Power Data Object */
      power_info.min_v = 0;
      power_info.max_v = (pdo >> 10) & 0x3FF; /* B19...10 Voltage in 50mV units */
      power_info.max_i = (pdo >> 0) & 0x3FF;  /* B9...0   Max Current in 10mA units */
      power_info.max_p = 0;
      break;
    case PD_PDO_TYPE_BATTERY:
      /* Reference: 6.4.1.2.5 Battery Supply Power Data Object */
      power_info.min_v = (pdo >> 10) & 0x3FF; /* B19...10 Min Voltage in 50mV units */
      power_info.max_v = (pdo >> 20) & 0x3FF; /* B29...20 Max Voltage in 50mV units */
      power_info.max_i = 0;
      power_info.max_p = (pdo >> 0) & 0x3FF; /* B9...0   Max Allowable Power in 250mW units */
      break;
    case PD_PDO_TYPE_VARIABLE_SUPPLY:
      /* Reference: 6.4.1.2.4 Variable Supply (non-Battery) Power Data Object */
      power_info.min_v = (pdo >> 10) & 0x3FF; /* B19...10 Min Voltage in 50mV units */
      power_info.max_v = (pdo >> 20) & 0x3FF; /* B29...20 Max Voltage in 50mV units */
      power_info.max_i = (pdo >> 0) & 0x3FF;  /* B9...0   Max Current in 10mA units */
      power_info.max_p = 0;
      break;
    case PD_PDO_TYPE_AUGMENTED_PDO:
      /* Reference: 6.4.1.3.4 Programmable Power Supply Augmented Power Data Object */
      power_info.max_v = ((pdo >> 17) & 0xFF) * 2; /* B24...17 Max Voltage in 100mV units */
      power_info.min_v = ((pdo >> 8) & 0xFF) * 2;  /* B15...8  Min Voltage in 100mV units */
      power_info.max_i = ((pdo >> 0) & 0x7F) * 5;  /* B6...0   Max Current in 50mA units */
      power_info.max_p = 0;
      break;
  }
  return power_info;
}

static PDMsg build_source_cap_response(const PowerDelivery *pd, PdContract pwr_info, uint8_t pos) {
  /* Reference: 6.4.2 Request Message */
  constexpr uint32_t templ = (((uint32_t) 1 << 24) | /* B24 No USB Suspend */
                              ((uint32_t) 1 << 25)   /* B25 USB Communication Capable */
  );
  uint32_t data = templ;
  if (pwr_info.type != PD_PDO_TYPE_AUGMENTED_PDO) {
    uint32_t req = pwr_info.max_i ? pwr_info.max_i : pwr_info.max_p;
    data |= ((uint32_t) req << 0) |  /* B9...0   Max Operating Current 10mA / Max Operating Power 250mW units */
            ((uint32_t) req << 10) | /* B19...10 Operating Current 10mA / Operating Power 250mW units */
            ((uint32_t) pos << 28);  /* B30...28 Object position (000b is Reserved) */
  } else {
    ESP_LOGE(TAG, "Augmented PDO is not supported yet");
  }
  return PDMsg(pd, PD_DATA_REQUEST, &data, 1);
}

PDMsg build_get_sink_cap_response(const PowerDelivery *pd) {
  /* Reference: 6.4.1.2.3 Sink Fixed Supply Power Data Object */
  constexpr uint32_t data = (((uint32_t) 500 << 0) |  /* B9...0   Operational Current in 10mA units */
                             ((uint32_t) 100 << 10) | /* B19...10 Voltage in 50mV units (5V) */
                             ((uint32_t) 1 << 26) |   /* B26 USB Communications Capable */
                             ((uint32_t) PD_PDO_TYPE_FIXED_SUPPLY << 30) /* B31...30 Fixed supply */
  );
  return PDMsg(pd, PD_DATA_SINK_CAP, &data, 1);
}

bool PowerDelivery::handle_message(const PDMsg &msg) {
  if (msg.num_of_obj == 0) {
    if (msg.type == PD_CNTRL_GOODCRC) {
      this->msg_counter_++;
      return true;
    }
    return this->handle_cntrl_message_(msg);
  }
  return this->handle_data_message_(msg);
}

bool PowerDelivery::handle_data_message_(const PDMsg &msg) {
  if (msg.id == this->last_received_msg_id_) {
    return false;
  }
  this->last_received_msg_id_ = msg.id;
  switch (msg.type) {
    case PD_DATA_SOURCE_CAP:
      this->set_ams(true);
      this->wait_src_cap_ = false;
      this->respond_to_src_cap_msg_(msg);
      break;
    default:
      break;
  }
  return true;
}

bool PowerDelivery::handle_cntrl_message_(const PDMsg &msg) {
  if (msg.id == this->last_received_msg_id_) {
    return false;
  }
  this->last_received_msg_id_ = msg.id;
  switch (msg.type) {
    case PD_CNTRL_GOODCRC:
      break;
    case PD_CNTRL_ACCEPT:
      if (this->active_ams_) {
        if (this->requested_contract_ != this->accepted_contract_) {
          this->set_state_(PD_STATE_TRANSITION);
        }
        this->set_contract_(this->requested_contract_);
      }
      break;
    case PD_CNTRL_PS_RDY:
      this->set_ams(false);
      this->set_state_(PD_STATE_EXPLICIT_SPR_CONTRACT);
      break;
    case PD_CNTRL_SOFT_RESET:
      this->send_message(PDMsg(this, PD_CNTRL_ACCEPT, 0));
      this->set_state_(PD_STATE_DEFAULT_CONTRACT);
      this->msg_counter_ = 0;
      break;
    case PD_CNTRL_GET_SINK_CAP:
      this->send_message(build_get_sink_cap_response(this));
      break;
    default:
      this->send_message(PDMsg(this, PD_CNTRL_NOT_SUPPORTED));
      break;
  }
  return true;
}

PDMsg PowerDelivery::create_fallback_request_message() const {
  /* Request first PDO (always the 5V Fixed Supply), max current 500mA */
  constexpr uint32_t data = ((uint32_t) 30 << 0) |  /* B9...0   Max Operating Current 10mA units (300mA) */
                            ((uint32_t) 10 << 10) | /* B19...10 Operating Current 10mA units (100mA) */
                            ((uint32_t) 1 << 24) |  /* B24 No USB Suspend */
                            ((uint32_t) 1 << 25) |  /* B25 USB Communication Capable */
                            ((uint32_t) 1 << 28);   /* B31...28 Object position 1 */
  return PDMsg(this, PD_DATA_REQUEST, &data, 1);
}

bool PowerDelivery::respond_to_src_cap_msg_(const PDMsg &msg) {
  PdContract selected_info{};
  uint8_t selected = 255;
  for (int idx = 0; idx < msg.num_of_obj; idx++) {
    PdContract pwr_info = pd_parse_power_info(msg.data_objects[idx]);
    if (pwr_info.type == PD_PDO_TYPE_AUGMENTED_PDO) {
      continue;
    }
    if (pwr_info.max_v * 50 / 1000 <= this->request_voltage_ || selected == 255) {
      selected_info = pwr_info;
      selected = idx;
    }
  }
  this->requested_contract_ = selected_info;

  PDMsg response = build_source_cap_response(this, selected_info, selected + 1);
  this->send_message(response);

  return true;
}

void PowerDelivery::set_ams(bool ams) {
  this->active_ams_ = ams;
  if (ams) {
    this->active_ams_timer_ = millis();
  }
}

bool PowerDelivery::check_ams() {
  if (this->active_ams_ && ((uint32_t) (millis() - this->active_ams_timer_) > 2000)) {
    this->active_ams_ = false;
  }
  return this->active_ams_;
}

std::string PowerDelivery::get_contract_string(PdContract contract) const {
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1fA @ %.0fV", contract.max_i / 100.0f, contract.max_v * 5 / 100.0f);
  return buf;
}

void PowerDelivery::set_contract_(PdContract contract) {
  this->accepted_contract_ = contract;
  this->contract_ = this->get_contract_string(contract);
  this->contract_voltage_ = contract.max_v * 5 / 100;
  this->contract_timer_ = millis();
}

bool PowerDelivery::request_voltage(int voltage) {
  if (!this->active_ams_) {
    this->set_request_voltage(voltage);
    this->wait_src_cap_ = true;
    this->get_src_cap_retry_count_ = 0;
    return true;
  }
  return false;
}

PDMsg::PDMsg(uint16_t header) { this->set_header(header); }

bool PDMsg::set_header(uint16_t header) {
  this->type = static_cast<PdDataMsgType>((header >> 0) & 0x1F);     /* 4...0  Message Type */
  this->spec_rev = static_cast<PdSpecRevision>((header >> 6) & 0x3); /* 7...6  Specification Revision */
  this->id = (header >> 9) & 0x7;                                    /* 11...9  MessageID */
  this->num_of_obj = (header >> 12) & 0x7;                           /* 14...12 Number of Data Objects */
  this->extended = (header >> 15) & 0x1;
  return true;
}

PDMsg::PDMsg(const PowerDelivery *pd, PdControlMsgType cntrl_msg_type) {
  this->type = cntrl_msg_type;
  this->spec_rev = pd->msg_spec_rev_;
  this->id = (pd->msg_counter_) % 8;
  this->num_of_obj = 0;
  this->extended = false;
}

PDMsg::PDMsg(const PowerDelivery *pd, PdControlMsgType cntrl_msg_type, uint8_t msg_id) {
  this->type = cntrl_msg_type;
  this->spec_rev = pd->msg_spec_rev_;
  this->id = msg_id;
  this->num_of_obj = 0;
  this->extended = false;
}

PDMsg::PDMsg(const PowerDelivery *pd, PdDataMsgType msg_type, const uint32_t *objects, uint8_t len) {
  if (len == 0 || len > PD_MAX_NUM_DATA_OBJECTS) {
    ESP_LOGE(TAG, "Invalid data object count: %d", len);
    this->num_of_obj = 0;
    return;
  }
  this->type = msg_type;
  this->spec_rev = pd->msg_spec_rev_;
  this->id = (pd->msg_counter_) % 8;
  this->num_of_obj = len;
  this->extended = false;
  memcpy(this->data_objects, objects, len * sizeof(uint32_t));
}

uint16_t PDMsg::get_coded_header() const {
  return ((uint16_t) this->type << 0) | ((uint16_t) 0x00 << 5) |     /* DataRole 0: UFP */
         ((uint16_t) this->spec_rev << 6) | ((uint16_t) 0x00 << 8) | /* PowerRole 0: sink */
         ((uint16_t) this->id << 9) | ((uint16_t) this->num_of_obj << 12) | ((uint16_t) !!(this->extended) << 15);
}

void PDMsg::debug_log() const {
  ESP_LOGD(TAG, "PD Message type=%d rev=%d id=%d obj=%d ext=%d coded=%d", this->type, this->spec_rev, this->id,
           this->num_of_obj, !!(this->extended), this->get_coded_header());
}

}  // namespace fusb302b
}  // namespace esphome
