#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace rf_bridge {

static const uint8_t RF_MESSAGE_SIZE = 9;
static const uint8_t RF_CODE_START = 0xAA;
static const uint8_t RF_CODE_ACK = 0xA0;
static const uint8_t RF_CODE_LEARN = 0xA1;
static const uint8_t RF_CODE_LEARN_KO = 0xA2;
static const uint8_t RF_CODE_LEARN_OK = 0xA3;
static const uint8_t RF_CODE_RFIN = 0xA4;
static const uint8_t RF_CODE_RFOUT = 0xA5;
static const uint8_t RF_CODE_SNIFFING_ON = 0xA6;
static const uint8_t RF_CODE_SNIFFING_OFF = 0xA7;
static const uint8_t RF_CODE_RFOUT_NEW = 0xA8;
static const uint8_t RF_CODE_LEARN_NEW = 0xA9;
static const uint8_t RF_CODE_LEARN_KO_NEW = 0xAA;
static const uint8_t RF_CODE_LEARN_OK_NEW = 0xAB;
static const uint8_t RF_CODE_RFOUT_BUCKET = 0xB0;
static const uint8_t RF_CODE_STOP = 0x55;
static const uint8_t RF_DEBOUNCE = 200;

struct RFBridgeData {
  uint16_t sync;
  uint16_t low;
  uint16_t high;
  uint32_t code;
};

class RFBridgeComponent : public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;
  void add_on_code_received_callback(std::function<void(RFBridgeData)> callback) {
    this->callback_.add(std::move(callback));
  }
  void send_code(RFBridgeData data);
  void learn();

 protected:
  void ack_();
  void decode_();

  unsigned long last_ = 0;
  unsigned char uartbuf_[RF_MESSAGE_SIZE + 3] = {0};
  unsigned char uartpos_ = 0;

  CallbackManager<void(RFBridgeData)> callback_;
};

class RFBridgeReceivedCodeTrigger : public Trigger<RFBridgeData> {
 public:
  explicit RFBridgeReceivedCodeTrigger(RFBridgeComponent *parent) {
    parent->add_on_code_received_callback([this](RFBridgeData data) { this->trigger(data); });
  }
};

template<typename... Ts> class RFBridgeSendCodeAction : public Action<Ts...> {
 public:
  RFBridgeSendCodeAction(RFBridgeComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint16_t, sync)
  TEMPLATABLE_VALUE(uint16_t, low)
  TEMPLATABLE_VALUE(uint16_t, high)
  TEMPLATABLE_VALUE(uint32_t, code)

  void play(Ts... x) {
    RFBridgeData data{};
    data.sync = this->sync_.value(x...);
    data.low = this->low_.value(x...);
    data.high = this->high_.value(x...);
    data.code = this->code_.value(x...);
    this->parent_->send_code(data);
  }

 protected:
  RFBridgeComponent *parent_;
};

template<typename... Ts> class RFBridgeLearnAction : public Action<Ts...> {
 public:
  RFBridgeLearnAction(RFBridgeComponent *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->learn(); }

 protected:
  RFBridgeComponent *parent_;
};

}  // namespace rf_bridge
}  // namespace esphome
