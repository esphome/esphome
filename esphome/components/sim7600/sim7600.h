#pragma once

#include <utility>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace sim7600 {

const uint16_t SIM7600_READ_BUFFER_LENGTH = 1024;



enum State {
  STATE_IDLE = 0,
  STATE_INIT,
  STATE_SETUP_CMGF,
  STATE_SETUP_COPS,
  STATE_SETUP_CLIP,
  STATE_CxREG,
  STATE_CxREG_WAIT,
  STATE_CSQ,
  STATE_CSQ_RESPONSE,
  STATE_SENDING_SMS_1,
  STATE_SENDING_SMS_2,
  STATE_SENDING_SMS_3,
  STATE_CHECK_SMS,
  STATE_PARSE_SMS_RESPONSE,
  STATE_RECEIVE_SMS,
  STATE_RECEIVED_SMS,
  STATE_DISABLE_ECHO,
  STATE_DIALING1,
  STATE_DIALING2,
  STATE_PARSE_CLIP,
  STATE_ATA_SENT,
  STATE_CHECK_CALL,
  STATE_SETUP_USSD,
  STATE_SEND_USSD1,
  STATE_SEND_USSD2,
  STATE_CHECK_USSD,
  STATE_RECEIVED_USSD
};

class Sim7600Component : public uart::UARTDevice, public PollingComponent {
 public:
  /// Retrieve the latest sensor values. This operation takes approximately 16ms.
  void update() override;
  void loop() override;
  void dump_config() override;
  
#ifdef USE_BINARY_SENSOR
  void set_registered_binary_sensor(binary_sensor::BinarySensor *registered_binary_sensor) {
    registered_binary_sensor_ = registered_binary_sensor;
  }
#endif

#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }
  void set_network_sensor(sensor::Sensor *network_sensor) { network_sensor_ = network_sensor; }   // DCO
#endif
  void add_on_sms_received_callback(std::function<void(std::string, std::string)> callback) {
    this->sms_received_callback_.add(std::move(callback));
  }
  void add_on_incoming_call_callback(std::function<void(std::string)> callback) {
    this->incoming_call_callback_.add(std::move(callback));
  }
  void add_on_call_connected_callback(std::function<void()> callback) {
    this->call_connected_callback_.add(std::move(callback));
  }
  void add_on_call_disconnected_callback(std::function<void()> callback) {
    this->call_disconnected_callback_.add(std::move(callback));
  }
  void add_on_ussd_received_callback(std::function<void(std::string)> callback) {
    this->ussd_received_callback_.add(std::move(callback));
  }
  void send_sms(const std::string &recipient, const std::string &message);
  void send_ussd(const std::string &ussd_code);
  void dial(const std::string &recipient);
  void connect();
  void disconnect();

 protected:
  void send_cmd_(const std::string &message);
  void parse_cmd_(std::string message);
  void set_registered_(bool registered);

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *registered_binary_sensor_{nullptr};
#endif

#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *network_sensor_{nullptr};
#endif

  std::string sender_;
  std::string message_;
  char read_buffer_[SIM7600_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  uint8_t parse_index_{0};
  uint8_t watch_dog_{0};
  bool expect_ack_{false};
  sim7600::State state_{STATE_IDLE};
  bool registered_{false};

  std::string recipient_;
  std::string outgoing_message_;
  std::string ussd_;
  bool send_pending_;
  bool dial_pending_;
  bool connect_pending_;
  bool disconnect_pending_;
  bool send_ussd_pending_;
  uint8_t call_state_{6};

  CallbackManager<void(std::string, std::string)> sms_received_callback_;
  CallbackManager<void(std::string)> incoming_call_callback_;
  CallbackManager<void()> call_connected_callback_;
  CallbackManager<void()> call_disconnected_callback_;
  CallbackManager<void(std::string)> ussd_received_callback_;
};

class Sim7600ReceivedMessageTrigger : public Trigger<std::string, std::string> {
 public:
  explicit Sim7600ReceivedMessageTrigger(Sim7600Component *parent) {
    parent->add_on_sms_received_callback(
        [this](const std::string &message, const std::string &sender) { this->trigger(message, sender); });
  }
};

class Sim7600IncomingCallTrigger : public Trigger<std::string> {
 public:
  explicit Sim7600IncomingCallTrigger(Sim7600Component *parent) {
    parent->add_on_incoming_call_callback([this](const std::string &caller_id) { this->trigger(caller_id); });
  }
};

class Sim7600CallConnectedTrigger : public Trigger<> {
 public:
  explicit Sim7600CallConnectedTrigger(Sim7600Component *parent) {
    parent->add_on_call_connected_callback([this]() { this->trigger(); });
  }
};

class Sim7600CallDisconnectedTrigger : public Trigger<> {
 public:
  explicit Sim7600CallDisconnectedTrigger(Sim7600Component *parent) {
    parent->add_on_call_disconnected_callback([this]() { this->trigger(); });
  }
};
class Sim7600ReceivedUssdTrigger : public Trigger<std::string> {
 public:
  explicit Sim7600ReceivedUssdTrigger(Sim7600Component *parent) {
    parent->add_on_ussd_received_callback([this](const std::string &ussd) { this->trigger(ussd); });
  }
};

template<typename... Ts> class Sim7600SendSmsAction : public Action<Ts...>, public Component {
 public:
  void set_parent(Sim7600Component *parent) { this->parent_ = parent; }
  TEMPLATABLE_VALUE(std::string, recipient)
  TEMPLATABLE_VALUE(std::string, message)

  void play(const Ts &...x) override {
    auto recipient = this->recipient_.value(x...);
    auto message = this->message_.value(x...);
    this->parent_->send_sms(recipient, message);
  }

 protected:
  Sim7600Component *parent_{nullptr};
};

template<typename... Ts> class Sim7600SendUssdAction : public Action<Ts...>, public Component {
 public:
  void set_parent(Sim7600Component *parent) { this->parent_ = parent; }
  TEMPLATABLE_VALUE(std::string, ussd)

  void play(const Ts &...x) override {
    auto ussd_code = this->ussd_.value(x...);
    this->parent_->send_ussd(ussd_code);
  }

 protected:
  Sim7600Component *parent_{nullptr};
};

template<typename... Ts> class Sim7600DialAction : public Action<Ts...>, public Component {
 public:
  void set_parent(Sim7600Component *parent) { this->parent_ = parent; }
  TEMPLATABLE_VALUE(std::string, recipient)

  void play(const Ts &...x) override {
    auto recipient = this->recipient_.value(x...);
    this->parent_->dial(recipient);
  }

 protected:
  Sim7600Component *parent_{nullptr};
};
template<typename... Ts> class Sim7600ConnectAction : public Action<Ts...>, public Component {
 public:
  void set_parent(Sim7600Component *parent) { this->parent_ = parent; }

  void play(const Ts &...x) override { this->parent_->connect(); }

 protected:
  Sim7600Component *parent_{nullptr};
};

template<typename... Ts> class Sim7600DisconnectAction : public Action<Ts...>, public Component {
 public:
  void set_parent(Sim7600Component *parent) { this->parent_ = parent; }

  void play(const Ts &...x) override { this->parent_->disconnect(); }

 protected:
  Sim7600Component *parent_{nullptr};
};

}  // namespace sim7600
}  // namespace esphome
