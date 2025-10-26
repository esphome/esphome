#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include "esphome/components/opentherm/opentherm_base.h"

#ifdef USE_ESP32
#include "esphome/components/opentherm/opentherm_rmt.h"
#endif
#ifdef ESP8266
#include "esphome/components/opentherm/opentherm_esp8266.h"
#endif

namespace esphome {
namespace opentherm_boiler {

using opentherm::MessageId;
using opentherm::OpenTherm;
using opentherm::OpenthermData;

class RequestProcessor {
 public:
  virtual bool handle_request(OpenthermData &data) = 0;

  virtual const char *get_type_name() const = 0;
  void set_id(const char *id) { this->id_ = id; }
  const char *get_id() const { return this->id_; }

 protected:
  const char *id_ = nullptr;
};

class Boiler : public Component {
 public:
  Boiler() = default;

  // Setters for the input and output OpenTherm interface pins
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

  // Trigggers
  void add_on_receive_callback(std::function<void(OpenthermData &)> &&callback) {
    this->on_receive_trigger_.add(std::move(callback));
  }
  void add_before_transmit_callback(std::function<void(OpenthermData &)> &&callback) {
    this->before_transmit_trigger_.add(std::move(callback));
  }

  void register_request_processor(MessageId msg, RequestProcessor *item) {
    this->request_processors_.emplace(msg, item);
  }

  // Component methods
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void on_shutdown() override;

 protected:
  // Communication pins for the OpenTherm interface
  InternalGPIOPin *in_pin_, *out_pin_;
  // The OpenTherm interface
  std::unique_ptr<OpenTherm> opentherm_;

  // Triggers
  CallbackManager<void(OpenthermData &)> on_receive_trigger_;
  CallbackManager<void(OpenthermData &)> before_transmit_trigger_;

  std::unordered_multimap<MessageId, RequestProcessor *> request_processors_;

  opentherm::OperationMode last_mode_{opentherm::OperationMode::IDLE};
  uint32_t last_mode_change_ = 0;
  uint32_t last_rx_ = 0;
  OpenthermData response_;
  bool response_enqueued_ = false;

  void read_request_();
  void process_request_(OpenthermData &data);
  void transmit_response_();
};

class OnReceiveTrigger : public Trigger<OpenthermData &> {
 public:
  explicit OnReceiveTrigger(Boiler *boiler) {
    boiler->add_on_receive_callback([this](OpenthermData &data) { this->trigger(data); });
  }
};

class BeforeTransmitTrigger : public Trigger<OpenthermData &> {
 public:
  explicit BeforeTransmitTrigger(Boiler *boiler) {
    boiler->add_before_transmit_callback([this](OpenthermData &data) { this->trigger(data); });
  }
};

}  // namespace opentherm_boiler
}  // namespace esphome
