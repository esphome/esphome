#pragma once

#include <utility>
#include <vector>

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
static const uint8_t RF_CODE_ADVANCED_RFIN = 0xA6;
static const uint8_t RF_CODE_SNIFFING_ON = 0xA6;
static const uint8_t RF_CODE_SNIFFING_OFF = 0xA7;
static const uint8_t RF_CODE_RFOUT_NEW = 0xA8;
static const uint8_t RF_CODE_LEARN_NEW = 0xA9;
static const uint8_t RF_CODE_LEARN_KO_NEW = 0xAA;
static const uint8_t RF_CODE_LEARN_OK_NEW = 0xAB;
static const uint8_t RF_CODE_RFOUT_BUCKET = 0xB0;
static const uint8_t RF_CODE_RFIN_BUCKET = 0xB1;
static const uint8_t RF_CODE_BEEP = 0xC0;
static const uint8_t RF_CODE_STOP = 0x55;
static const uint8_t RF_DEBOUNCE = 200;

struct RFBridgeData {
  uint16_t sync;
  uint16_t low;
  uint16_t high;
  uint32_t code;
};

struct RFBridgeAdvancedData {
  uint8_t length;
  uint8_t protocol;
  std::string code;
};

class RFBridgeComponent : public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;
  void add_on_code_received_callback(std::function<void(RFBridgeData)> callback) {
    this->data_callback_.add(std::move(callback));
  }
  void add_on_advanced_code_received_callback(std::function<void(RFBridgeAdvancedData)> callback) {
    this->advanced_data_callback_.add(std::move(callback));
  }
  void send_code(RFBridgeData data);
  void send_advanced_code(const RFBridgeAdvancedData &data);
  void learn();
  void start_advanced_sniffing();
  void stop_advanced_sniffing();
  void start_bucket_sniffing();
  void send_raw(const std::string &code);
  void beep(uint16_t ms);

 protected:
  void ack_();
  void decode_();
  bool parse_bridge_byte_(uint8_t byte);
  void write_byte_str_(const std::string &codes);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_bridge_byte_{0};

  CallbackManager<void(RFBridgeData)> data_callback_;
  CallbackManager<void(RFBridgeAdvancedData)> advanced_data_callback_;
};

class RFBridgeReceivedCodeTrigger : public Trigger<RFBridgeData> {
 public:
  explicit RFBridgeReceivedCodeTrigger(RFBridgeComponent *parent) {
    parent->add_on_code_received_callback([this](RFBridgeData data) { this->trigger(data); });
  }
};

class RFBridgeReceivedAdvancedCodeTrigger : public Trigger<RFBridgeAdvancedData> {
 public:
  explicit RFBridgeReceivedAdvancedCodeTrigger(RFBridgeComponent *parent) {
    parent->add_on_advanced_code_received_callback([this](const RFBridgeAdvancedData &data) { this->trigger(data); });
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

template<typename... Ts> class RFBridgeSendAdvancedCodeAction : public Action<Ts...> {
 public:
  RFBridgeSendAdvancedCodeAction(RFBridgeComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, length)
  TEMPLATABLE_VALUE(uint8_t, protocol)
  TEMPLATABLE_VALUE(std::string, code)

  void play(Ts... x) {
    RFBridgeAdvancedData data{};
    data.length = this->length_.value(x...);
    data.protocol = this->protocol_.value(x...);
    data.code = this->code_.value(x...);
    this->parent_->send_advanced_code(data);
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

template<typename... Ts> class RFBridgeStartAdvancedSniffingAction : public Action<Ts...> {
 public:
  RFBridgeStartAdvancedSniffingAction(RFBridgeComponent *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->start_advanced_sniffing(); }

 protected:
  RFBridgeComponent *parent_;
};

template<typename... Ts> class RFBridgeStopAdvancedSniffingAction : public Action<Ts...> {
 public:
  RFBridgeStopAdvancedSniffingAction(RFBridgeComponent *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->stop_advanced_sniffing(); }

 protected:
  RFBridgeComponent *parent_;
};

template<typename... Ts> class RFBridgeStartBucketSniffingAction : public Action<Ts...> {
 public:
  RFBridgeStartBucketSniffingAction(RFBridgeComponent *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->start_bucket_sniffing(); }

 protected:
  RFBridgeComponent *parent_;
};

template<typename... Ts> class RFBridgeSendRawAction : public Action<Ts...> {
 public:
  RFBridgeSendRawAction(RFBridgeComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, raw)

  void play(Ts... x) { this->parent_->send_raw(this->raw_.value(x...)); }

 protected:
  RFBridgeComponent *parent_;
};

template<typename... Ts> class RFBridgeBeepAction : public Action<Ts...> {
 public:
  RFBridgeBeepAction(RFBridgeComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint16_t, duration)

  void play(Ts... x) { this->parent_->beep(this->duration_.value(x...)); }

 protected:
  RFBridgeComponent *parent_;
};

}  // namespace rf_bridge
}  // namespace esphome
