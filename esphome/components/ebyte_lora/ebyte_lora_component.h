#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "config.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include <utility>
#include <vector>
#include <map>

namespace esphome {
namespace ebyte_lora {

#ifdef USE_SENSOR
struct Sensor {
  sensor::Sensor *sensor;
  const char *id;
  bool updated;
};
#endif

#ifdef USE_BINARY_SENSOR
struct BinarySensor {
  binary_sensor::BinarySensor *sensor;
  const char *id;
  bool updated;
};
#endif

static const char *const TAG = "ebyte_lora";
// the mode the receiver is in
enum ModeType { NORMAL = 0, WOR_SEND = 1, WOR_RECEIVER = 2, CONFIGURATION = 3, MODE_INIT = 0xFF };
// 1 byte, 8 bits in total
// note that the data sheets shows the order in reverse

class EbyteLoraComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void dump_config() override;

#ifdef USE_SENSOR
  void add_sensor(const char *id, sensor::Sensor *sensor) {
    Sensor st{sensor, id, true};
    this->sensors_.push_back(st);
  }
  void add_remote_sensor(int network_id, const char *remote_id, sensor::Sensor *sensor) {
    this->remote_sensors_[network_id][remote_id] = sensor;
  }
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(const char *id, binary_sensor::BinarySensor *sensor) {
    BinarySensor st{sensor, id, true};
    this->binary_sensors_.push_back(st);
  }

  void add_remote_binary_sensor(int network_id, const char *remote_id, binary_sensor::BinarySensor *sensor) {
    this->remote_binary_sensors_[network_id][remote_id] = sensor;
  }
#endif

#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }
#endif
  void set_pin_aux(InternalGPIOPin *pin_aux) { pin_aux_ = pin_aux; }
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
  void set_recycle_time(uint32_t recycle_time) { this->recyle_time_ = recycle_time; }
  // if enabled, will repeat any message it received as long as it isn't its own network id
  void set_repeater(bool enable) { repeater_enabled_ = enable; }
  // software network id, this will make sure that only items from that network id are being found
  void set_network_id(int id) { network_id_ = id; }

 private:
  ModeType current_mode_ = MODE_INIT;

  // set WOR mode
  void set_mode_(ModeType mode);
  ModeType get_mode_();
  // check if you can sent a message
  bool is_config_right_();
  void set_config_();
  void request_current_config_();
  void send_data_(bool all);
  void request_repeater_info_();
  void send_repeater_info_();
  bool can_send_message_(const char *info);

 protected:
  bool need_send_info_{};
  bool request_repeater_info_update_needed_{};
  // for now do it every 600s
  uint32_t recyle_time_ = 600;
  uint32_t last_key_time_{};
  // auto delay doing anything
  uint32_t busy_till_ = 0;
  void setup_conf_(std::vector<uint8_t> conf);
  void process_(std::vector<uint8_t> data);
  void repeat_message_(std::vector<uint8_t> data);
  // set if there is some sensor info available or it is in repeater mode
  bool should_send_ = false;
  // if set it will function as a repeater only
  bool repeater_enabled_ = false;
  // used to tell one lora device apart from another
  uint8_t network_id_ = 0;
  int rssi_ = 0;
  RegisterConfig current_config_{};
  RegisterConfig expected_config_{};
#ifdef USE_SENSOR
  std::vector<Sensor> sensors_{};
  std::map<int, std::map<std::string, sensor::Sensor *>> remote_sensors_{};
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<BinarySensor> binary_sensors_{};
  std::map<int, std::map<std::string, binary_sensor::BinarySensor *>> remote_binary_sensors_{};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
#endif
  InternalGPIOPin *pin_aux_{nullptr};
  InternalGPIOPin *pin_m0_{nullptr};
  InternalGPIOPin *pin_m1_{nullptr};
};
}  // namespace ebyte_lora
}  // namespace esphome
