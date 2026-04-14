#pragma once

#include <atomic>
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
#ifdef USE_TEXT_SENSOR
namespace text_sensor {
class TextSensor;
}  // namespace text_sensor
#endif
namespace fusb302b {

static constexpr uint8_t PD_MAX_NUM_DATA_OBJECTS = 7;

// Forward declaration
class PowerDelivery;

enum PdSpecRevision {
  PD_SPEC_REV_1 = 0,
  PD_SPEC_REV_2 = 1,
  PD_SPEC_REV_3 = 2
  /* 3 Reserved */
};

enum PdDataMsgType {
  /* 0 Reserved */
  PD_DATA_SOURCE_CAP = 0x01,
  PD_DATA_REQUEST = 0x02,
  PD_DATA_BIST = 0x03,
  PD_DATA_SINK_CAP = 0x04,
  PD_DATA_BATTERY_STATUS = 0x05,
  PD_DATA_ALERT = 0x06,
  PD_DATA_GET_COUNTRY_INFO = 0x07,
  PD_DATA_ENTER_USB = 0x08,
  PD_DATA_EPR_REQUEST = 0x09,
  PD_DATA_EPR_MODE = 0x0A,
  PD_DATA_SOURCE_INFO = 0x0B,
  PD_DATA_REVISION = 0x0C,
  PD_DATA_VENDOR_DEF = 0x0F
};

enum PdControlMsgType {
  PD_CNTRL_GOODCRC = 0x01,
  PD_CNTRL_GOTOMIN = 0x02,
  PD_CNTRL_ACCEPT = 0x03,
  PD_CNTRL_REJECT = 0x04,
  PD_CNTRL_PING = 0x05,
  PD_CNTRL_PS_RDY = 0x06,
  PD_CNTRL_GET_SOURCE_CAP = 0x07,
  PD_CNTRL_GET_SINK_CAP = 0x08,
  PD_CNTRL_DR_SWAP = 0x09,
  PD_CNTRL_PR_SWAP = 0x0A,
  PD_CNTRL_VCONN_SWAP = 0x0B,
  PD_CNTRL_WAIT = 0x0C,
  PD_CNTRL_SOFT_RESET = 0x0D,
  PD_CNTRL_NOT_SUPPORTED = 0x10,
  PD_CNTRL_GET_SOURCE_CAP_EXTENDED = 0x11,
  PD_CNTRL_GET_STATUS = 0x12,
  PD_CNTRL_FR_SWAP = 0x13,
  PD_CNTRL_GET_PPS_STATUS = 0x14,
  PD_CNTRL_GET_COUNTRY_CODES = 0x15,
  PD_CNTRL_GET_SINK_CAP_EXTENDED = 0x16,
  PD_CNTRL_GET_SOURCE_INFO = 0x17,
  PD_CNTRL_GET_REVISION = 0x18
};

enum PdPowerDataObjType {
  PD_PDO_TYPE_FIXED_SUPPLY = 0,
  PD_PDO_TYPE_BATTERY = 1,
  PD_PDO_TYPE_VARIABLE_SUPPLY = 2,
  PD_PDO_TYPE_AUGMENTED_PDO = 3 /* USB PD 3.0 */
};

enum PowerDeliveryState : uint8_t {
  PD_STATE_DISCONNECTED,
  PD_STATE_PD_TIMEOUT,
  PD_STATE_DEFAULT_CONTRACT,
  PD_STATE_TRANSITION,
  PD_STATE_EXPLICIT_SPR_CONTRACT,
  PD_STATE_EXPLICIT_EPR_CONTRACT,
  PD_STATE_ERROR
};

enum PowerDeliveryEvent : uint8_t {
  PD_EVENT_ATTACHED,
  PD_EVENT_DETACHED,
  PD_EVENT_RECEIVED_MSG,
  PD_EVENT_SENDING_MSG_FAILED,
  PD_EVENT_SOFT_RESET,
  PD_EVENT_HARD_RESET
};

struct PdContract {
  PdPowerDataObjType type;
  uint16_t min_v; /* Voltage in 50mV units */
  uint16_t max_v; /* Voltage in 50mV units */
  uint16_t max_i; /* Current in 10mA units */
  uint16_t max_p; /* Power in 250mW units */

  bool operator==(const PdContract &other) const {
    return max_v == other.max_v && max_i == other.max_i && type == other.type;
  }
  bool operator!=(const PdContract &other) const { return !(*this == other); }
};

using PdPdo = uint32_t;

class PDMsg {
 public:
  PDMsg() = default;
  PDMsg(uint16_t header);
  PDMsg(const PowerDelivery *pd, PdControlMsgType cntrl_msg_type);
  PDMsg(const PowerDelivery *pd, PdControlMsgType cntrl_msg_type, uint8_t msg_id);
  PDMsg(const PowerDelivery *pd, PdDataMsgType data_msg_type, const uint32_t *objects, uint8_t len);

  uint16_t get_coded_header() const;
  bool set_header(uint16_t header);

  uint8_t type;
  PdSpecRevision spec_rev;
  uint8_t id;
  uint8_t num_of_obj;
  bool extended;
  uint32_t data_objects[PD_MAX_NUM_DATA_OBJECTS];

  void debug_log() const;
};

class PDEventInfo {
 public:
  PowerDeliveryEvent event;
  PDMsg msg;
};

class PowerDelivery {
  friend class PDMsg;

 public:
  PowerDeliveryState get_state() const { return this->state_; }
  int get_contract_voltage() const { return this->contract_voltage_; }
  std::string get_contract() const { return this->contract_; }

  PowerDeliveryState get_prev_state() const { return this->prev_state_; }

  bool request_voltage(int voltage);

  virtual bool send_message(const PDMsg &msg) = 0;
  virtual bool read_message(PDMsg &msg) = 0;

  PDMsg create_fallback_request_message() const;
  bool handle_message(const PDMsg &msg);

  void set_request_voltage(int voltage) { this->request_voltage_ = voltage; }
  std::string get_contract_string(PdContract contract) const;
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  void set_ams(bool ams);
  bool check_ams();

#ifdef USE_TEXT_SENSOR
  void set_contract_sensor(text_sensor::TextSensor *sensor) { this->contract_sensor_ = sensor; }
#endif

 protected:
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *contract_sensor_{nullptr};
#endif
  uint32_t active_ams_timer_{0};
  bool active_ams_{false};

  uint8_t last_received_msg_id_{255};
  PdSpecRevision msg_spec_rev_{PD_SPEC_REV_2};
  std::atomic<uint8_t> msg_counter_{0};

  bool handle_data_message_(const PDMsg &msg);
  bool handle_cntrl_message_(const PDMsg &msg);

  bool respond_to_src_cap_msg_(const PDMsg &msg);

  void set_contract_(PdContract contract);
  PdContract requested_contract_{};
  PdContract accepted_contract_{};
  PdContract previous_contract_{};
  uint32_t contract_timer_{0};

  void set_state_(PowerDeliveryState new_state) {
    this->prev_state_ = this->state_;
    this->state_ = new_state;
  }

  virtual void publish() {}

  PdSpecRevision spec_revision_{PD_SPEC_REV_2};

  bool wait_src_cap_{true};
  bool tried_soft_reset_{false};
  int get_src_cap_retry_count_{0};
  uint32_t get_src_cap_time_stamp_{0};

  int request_voltage_{5};

  PowerDeliveryState state_{PD_STATE_DISCONNECTED};
  int contract_voltage_{5};
  std::string contract_{"0.5A @ 5V"};

  CallbackManager<void()> state_callback_{};

 private:
  PowerDeliveryState prev_state_{PD_STATE_DISCONNECTED};
};

PDMsg build_get_sink_cap_response(const PowerDelivery *pd);

}  // namespace fusb302b
}  // namespace esphome
