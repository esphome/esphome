#pragma once
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "config.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome {
namespace ebyte_lora {
static const char *const TAG = "ebyte_lora";
static const int MAX_SIZE_TX_PACKET = 200;

// the mode the receiver is in
enum ModeType { NORMAL = 0, WOR_SEND = 1, WOR_RECEIVER = 2, CONFIGURATION = 3, MODE_INIT = 0xFF };
// 1 byte, 8 bits in total
// note that the data sheets shows the order in reverse

// has to be defined first, will be implemented later
class EbyteLoraSwitch;
class EbyteLoraComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void dump_config() override;

  void send_switch_info();
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }
  void set_pin_aux(InternalGPIOPin *pin_aux) { pin_aux_ = pin_aux; }
  void set_switch(EbyteLoraSwitch *obj) { this->sensors_.push_back(obj); }
  void set_pin_m0(InternalGPIOPin *pin_m0) { pin_m0_ = pin_m0; }
  void set_pin_m1(InternalGPIOPin *pin_m1) { pin_m1_ = pin_m1; }
  void set_addh(uint8_t addh) { expected_config_.addh = addh; }
  void set_addl(uint8_t addl) { expected_config_.addl = addl; }
  void set_air_data_rate(AirDataRate air_data_rate) { expected_config_.air_data_rate = air_data_rate; }
  void set_uart_parity(UartParitySetting parity) { expected_config_.parity = parity; }
  void set_uart_bps(UartBpsSpeed bps_speed) { expected_config_.uart_baud = bps_speed; }
  void set_transmission_power(TransmissionPower power) { expected_config_.transmission_power = power; }
  void set_rssi_noise(EnableByte enable) { expected_config_.rssi_noise = enable; }
  void set_sub_packet(SubPacketSetting sub_packet) { expected_config_.sub_packet = sub_packet; }
  void set_channel(uint8_t channel) { expected_config_.channel = channel; }
  void set_wor(WorPeriod wor) { expected_config_.wor_period = wor; }
  void set_enable_lbt(EnableByte enable) { expected_config_.enable_lbt = enable; }
  void set_transmission_mode(TransmissionMode mode) { expected_config_.transmission_mode = mode; }
  void set_enable_rssi(EnableByte enable) { expected_config_.enable_rssi = enable; }
  void set_sent_switch_state(bool enable) { sent_switch_state_ = enable; }
  void set_repeater(bool enable) { repeater_ = enable; }
  void set_network_id(int id) { network_id_ = id; }

 private:
  std::vector<EbyteLoraSwitch *> sensors_;
  ModeType mode_ = MODE_INIT;
  // set WOR mode
  void set_mode_(ModeType mode);
  ModeType get_mode_();
  // checks the aux port to see if it is done setting
  void setup_wait_response_(uint32_t timeout = 1000);
  bool can_send_message_();
  bool check_config_();
  void set_config_();
  void get_current_config_();
  void setup_conf_(std::vector<uint8_t> data);
  void request_repeater_info_();
  void send_repeater_info_();
  void repeat_message_(std::vector<uint8_t> data);

 protected:
  bool update_needed_ = false;
  // if enabled will sent information about itself
  bool sent_switch_state_ = false;
  // if set it will function as a repeater
  bool repeater_ = false;
  // used to tell one lora device apart from another
  int network_id_ = 0;
  int rssi_ = 0;
  uint32_t starting_to_check_;
  uint32_t time_out_after_;
  std::string raw_message_;
  RegisterConfig current_config_;
  RegisterConfig expected_config_;
  sensor::Sensor *rssi_sensor_{nullptr};
  InternalGPIOPin *pin_aux_{nullptr};
  InternalGPIOPin *pin_m0_{nullptr};
  InternalGPIOPin *pin_m1_{nullptr};
};
#ifdef USE_SWITCH
class EbyteLoraSwitch : public switch_::Switch, public Parented<EbyteLoraComponent> {
 public:
  void set_pin(uint8_t pin) { pin_ = pin; }
  uint8_t get_pin() { return pin_; }

 protected:
  void write_state(bool state) override {
    // set it first
    this->publish_state(state);
    // then tell the world about it
    this->parent_->send_switch_info();
  }
  uint8_t pin_;
};
#endif
}  // namespace ebyte_lora
}  // namespace esphome
