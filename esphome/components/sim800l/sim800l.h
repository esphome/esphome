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

namespace esphome::sim800l {

const uint16_t SIM800L_READ_BUFFER_LENGTH = 1024;

enum State {
  STATE_IDLE = 0,
  STATE_INIT,
  STATE_SETUP_CMGF,
  STATE_SETUP_CLIP,
  STATE_CREG,
  STATE_CREG_WAIT,
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

class Sim800LComponent : public uart::UARTDevice, public PollingComponent {
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
#endif
  template<typename F> void add_on_sms_received_callback(F &&callback) {
    this->sms_received_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_incoming_call_callback(F &&callback) {
    this->incoming_call_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_call_connected_callback(F &&callback) {
    this->call_connected_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_call_disconnected_callback(F &&callback) {
    this->call_disconnected_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_ussd_received_callback(F &&callback) {
    this->ussd_received_callback_.add(std::forward<F>(callback));
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
#endif
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
  std::string ussd_;
  bool send_pending_{false};
  bool dial_pending_{false};
  bool connect_pending_{false};
  bool disconnect_pending_{false};
  bool send_ussd_pending_{false};
  uint8_t call_state_{6};

  CallbackManager<void(std::string, std::string)> sms_received_callback_;
  CallbackManager<void(std::string)> incoming_call_callback_;
  CallbackManager<void()> call_connected_callback_;
  CallbackManager<void()> call_disconnected_callback_;
  CallbackManager<void(std::string)> ussd_received_callback_;
};

template<typename... Ts> class Sim800LSendSmsAction : public Action<Ts...> {
 public:
  Sim800LSendSmsAction(Sim800LComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, recipient)
  TEMPLATABLE_VALUE(std::string, message)

  void play(const Ts &...x) {
    auto recipient = this->recipient_.value(x...);
    auto message = this->message_.value(x...);
    this->parent_->send_sms(recipient, message);
  }

 protected:
  Sim800LComponent *parent_;
};

template<typename... Ts> class Sim800LSendUssdAction : public Action<Ts...> {
 public:
  Sim800LSendUssdAction(Sim800LComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, ussd)

  void play(const Ts &...x) {
    auto ussd_code = this->ussd_.value(x...);
    this->parent_->send_ussd(ussd_code);
  }

 protected:
  Sim800LComponent *parent_;
};

template<typename... Ts> class Sim800LDialAction : public Action<Ts...> {
 public:
  Sim800LDialAction(Sim800LComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, recipient)

  void play(const Ts &...x) {
    auto recipient = this->recipient_.value(x...);
    this->parent_->dial(recipient);
  }

 protected:
  Sim800LComponent *parent_;
};
template<typename... Ts> class Sim800LConnectAction : public Action<Ts...> {
 public:
  Sim800LConnectAction(Sim800LComponent *parent) : parent_(parent) {}

  void play(const Ts &...x) { this->parent_->connect(); }

 protected:
  Sim800LComponent *parent_;
};

template<typename... Ts> class Sim800LDisconnectAction : public Action<Ts...> {
 public:
  Sim800LDisconnectAction(Sim800LComponent *parent) : parent_(parent) {}

  void play(const Ts &...x) { this->parent_->disconnect(); }

 protected:
  Sim800LComponent *parent_;
};

}  // namespace esphome::sim800l
