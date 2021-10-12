#pragma once

#include <utility>

#include "esphome/core/component.h"
#include <esphome/components/sensor/sensor.h>
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace sim800l {

const uint16_t SIM800L_READ_BUFFER_LENGTH = 1024;

enum State {
  STATE_IDLE = 0,
  STATE_INIT,
  STATE_CHECK_AT,
  STATE_CREG,
  STATE_CREGWAIT,
  STATE_CSQ,
  STATE_CSQ_RESPONSE,
  STATE_SENDINGSMS1,
  STATE_SENDINGSMS2,
  STATE_SENDINGSMS3,
  STATE_CHECK_SMS,
  STATE_PARSE_SMS_RESPONSE,
  STATE_RECEIVESMS,
  STATE_RECEIVEDSMS,
  STATE_DISABLE_ECHO,
  STATE_DIALING1,
  STATE_DIALING2
};

class Sim800LComponent : public uart::UARTDevice, public PollingComponent {
 public:
  /// Retrieve the latest sensor values. This operation takes approximately 16ms.
  void update() override;
  void loop() override;
  void dump_config() override;
  void add_on_sms_received_callback(std::function<void(std::string, std::string)> callback) {
    this->callback_.add(std::move(callback));
  }
  void add_on_connected_callback(std::function<void()> callback) { this->callback_reg.add(std::move(callback)); }
  void send_sms(const std::string &recipient, const std::string &message);
  void delete_sms();
  void dial(const std::string &recipient);

  void set_rssi_(sensor::Sensor *rssi_);

 protected:
  void send_cmd_(const std::string &);
  void parse_cmd_(std::string);

  std::string sender_;
  std::string message_;
  char read_buffer_[SIM800L_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  uint8_t parse_index_{0};
  uint8_t watch_dog_{0};
  bool expect_ack_{false};
  sim800l::State state_{STATE_IDLE};
  bool registered_{false};

  std::string recipient_;
  std::string outgoing_message_;
  bool send_pending_;
  bool dial_pending_;

  CallbackManager<void(std::string, std::string)> callback_;
  CallbackManager<void()> callback_reg;

  sensor::Sensor *rssi_{nullptr};
};

class Sim800LReceivedMessageTrigger : public Trigger<std::string, std::string> {
 public:
  explicit Sim800LReceivedMessageTrigger(Sim800LComponent *parent) {
    parent->add_on_sms_received_callback(
        [this](const std::string &message, const std::string &sender) { this->trigger(message, sender); });
  }
};

class Sim800LRegistrationSuccededTrigger : public Trigger<> {
 public:
  explicit Sim800LRegistrationSuccededTrigger(Sim800LComponent *parent) {
    parent->add_on_connected_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class Sim800LSendSmsAction : public Action<Ts...> {
 public:
  Sim800LSendSmsAction(Sim800LComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, recipient)
  TEMPLATABLE_VALUE(std::string, message)

  void play(Ts... x) {
    auto recipient = this->recipient_.value(x...);
    auto message = this->message_.value(x...);
    this->parent_->send_sms(recipient, message);
  }

 protected:
  Sim800LComponent *parent_;
};

template<typename... Ts> class Sim800LDeleteSmsAction : public Action<Ts...> {
 public:
  Sim800LDeleteSmsAction(Sim800LComponent *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->delete_sms(); }

 protected:
  Sim800LComponent *parent_;
};

template<typename... Ts> class Sim800LDialAction : public Action<Ts...> {
 public:
  Sim800LDialAction(Sim800LComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, recipient)

  void play(Ts... x) {
    auto recipient = this->recipient_.value(x...);
    this->parent_->dial(recipient);
  }

 protected:
  Sim800LComponent *parent_;
};

}  // namespace sim800l
}  // namespace esphome
