#pragma once
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "config.h"

namespace esphome {
namespace ebyte_lora {
static const uint8_t MAX_SIZE_TX_PACKET = 200;

// the mode the receiver is in
enum ModeType { NORMAL = 0, WOR_SEND = 1, WOR_RECEIVER = 2, CONFIGURATION = 3, MODE_INIT = 0xFF };
// 1 byte, 8 bits in total
// note that the data sheets shows the order in reverse

// has to be defined first, will be implemented later
class EbyteLoraSwitch;
class EbyteLoraComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override { send_switch_info_(); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void dump_config() override;
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }
  void set_pin_aux(GPIOPin *pin_aux) { pin_aux_ = pin_aux; }
  void register_sensor(EbyteLoraSwitch *obj) { this->sensors_.push_back(obj); }
  void set_pin_m0(GPIOPin *pin_m0) { pin_m0_ = pin_m0; }
  void set_pin_m1(GPIOPin *pin_m1) { pin_m1_ = pin_m1; }

 private:
  std::vector<EbyteLoraSwitch *> sensors_;
  ModeType mode_ = MODE_INIT;
  // set WOR mode
  void set_mode_(ModeType mode);
  ModeType get_mode_();
  // checks the aux port to see if it is done setting
  void setup_wait_response_(uint32_t timeout = 1000);
  bool can_send_message_();
  void get_current_config_();
  void send_switch_push_(uint8_t pin, bool value);
  void send_switch_info_();

 protected:
  bool update_needed_ = false;
  int rssi_ = 0;
  uint32_t starting_to_check_;
  uint32_t time_out_after_;
  std::string raw_message_;
  sensor::Sensor *rssi_sensor_{nullptr};
  GPIOPin *pin_aux_;
  GPIOPin *pin_m0_;
  GPIOPin *pin_m1_;
};
class EbyteLoraSwitch : public switch_::Switch, public Component {
 public:
  void dump_config() override { LOG_SWITCH("ebyte_lora_switch", "Ebyte Lora Switch", this); }
  void set_parent(EbyteLoraComponent *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  uint8_t get_pin() { return pin_; }

 protected:
  void write_state(bool state) override { this->parent_->digital_write(this->pin_, state); }
  EbyteLoraComponent *parent_;
  uint8_t pin_;
};

}  // namespace ebyte_lora
}  // namespace esphome
