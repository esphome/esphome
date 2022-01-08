#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/optional.h"
#include "esphome/core/log.h"

#include <CEC_Device.h>

#include <functional>

namespace esphome {
namespace hdmi_cec {

class HdmiCecTrigger;
template<typename... Ts> class HdmiCecSendAction;

static const uint8_t HDMI_CEC_MAX_DATA_LENGTH = 16;

class HdmiCec : public Component, CEC_Device {
 public:
  HdmiCec(){};
  // Component overrides
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  // CEC_Device overrides
  bool LineState() override;                                                    // NOLINT(readability-identifier-naming)
  void SetLineState(bool) override;                                             // NOLINT(readability-identifier-naming)
  void OnReady(int logical_address) override;                                   // NOLINT(readability-identifier-naming)
  void OnReceiveComplete(unsigned char *buffer, int count, bool ack) override;  // NOLINT(readability-identifier-naming)
  void OnTransmitComplete(unsigned char *buffer, int count,
                          bool ack) override;  // NOLINT(readability-identifier-naming)

  void send_data(uint8_t source, uint8_t destination, const std::vector<uint8_t> &data);
  void set_address(uint8_t address) { this->address_ = address; }
  void set_physical_address(uint16_t physical_address) { this->physical_address_ = physical_address; }
  void set_promiscuous_mode(uint16_t promiscuous_mode) { this->promiscuous_mode_ = promiscuous_mode; }
  void set_pin(InternalGPIOPin *pin) {
    this->pin_ = pin;
    this->pin_->pin_mode(gpio::FLAG_INPUT);
  }
  void add_trigger(HdmiCecTrigger *trigger);
  static void pin_interrupt(HdmiCec *arg);

 protected:
  void send_data_internal_(uint8_t source, uint8_t destination, unsigned char *buffer, int count);

  template<typename... Ts> friend class HdmiCecSendAction;

  InternalGPIOPin *pin_;
  std::vector<HdmiCecTrigger *> triggers_{};
  uint8_t address_;
  uint16_t physical_address_;
  bool promiscuous_mode_;
  HighFrequencyLoopRequester high_freq_;
  // Used so that `pin_interrupt` doesn't fire when we're toggling the line
  volatile boolean disable_line_interrupts_ = false;
};

template<typename... Ts> class HdmiCecSendAction : public Action<Ts...>, public Parented<HdmiCec> {
 public:
  void set_data_template(const std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }
  void set_source(uint8_t source) { this->source_ = source; }
  void set_destination(uint8_t destination) { this->destination_ = destination; }

  void play(Ts... x) override {
    auto source = this->source_.has_value() ? *this->source_ : this->parent_->address_;
    if (this->static_) {
      this->parent_->send_data(source, this->destination_, this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->send_data(source, this->destination_, val);
    }
  }

 protected:
  optional<uint8_t> source_{};
  uint8_t destination_;
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

class HdmiCecTrigger : public Trigger<uint8_t, uint8_t, std::vector<uint8_t>>, public Component {
  friend class HdmiCec;

 public:
  explicit HdmiCecTrigger(HdmiCec *parent) : parent_(parent){};
  void setup() override { this->parent_->add_trigger(this); }

  void set_source(uint8_t source) { this->source_ = source; }
  void set_destination(uint8_t destination) { this->destination_ = destination; }
  void set_opcode(uint8_t opcode) { this->opcode_ = opcode; }
  void set_data(const std::vector<uint8_t> &data) { this->data_ = data; }

 protected:
  HdmiCec *parent_;
  optional<uint8_t> source_{};
  optional<uint8_t> destination_{};
  optional<uint8_t> opcode_{};
  optional<std::vector<uint8_t>> data_{};
};

}  // namespace hdmi_cec
}  // namespace esphome

#endif  // USE_ARDUINO
