#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace iwr6843aop {

class IWR6843AOPComponent : public Component {
 public:
  IWR6843AOPComponent(uart::UARTComponent *uart1, uart::UARTComponent *uart2) : uart1_dev_(uart1), uart2_dev_(uart2) {}
  void setup() override;
  void loop() override;

  void set_float_output_1(float value) { float_output_1_ = value; }
  void set_float_output_2(float value) { float_output_2_ = value; }
  void set_int_output(int value) { int_output_ = value; }

  float get_float_output_1() const { return float_output_1_; }
  float get_float_output_2() const { return float_output_2_; }
  int get_int_output() const { return int_output_; }

  void set_float_input(const std::string &key, esphome::number::Number *number);
  esphome::sensor::Sensor *get_float_output_1_sensor() const { return float_output_1_sensor_; }
  esphome::sensor::Sensor *get_float_output_2_sensor() const { return float_output_2_sensor_; }
  esphome::sensor::Sensor *get_int_output_sensor() const { return int_output_sensor_; }

  void set_float_output_1_sensor(sensor::Sensor *s) { float_output_1_sensor_ = s; }
  void set_float_output_2_sensor(sensor::Sensor *s) { float_output_2_sensor_ = s; }
  void set_int_output_sensor(sensor::Sensor *s) { int_output_sensor_ = s; }

  // UART API
  void cfg_iwr6843aop();
  void read_uart2();
  void parse_target_list_tlv(const std::vector<uint8_t> &tlv_payload);

 protected:
  uart::UARTComponent *uart1_dev_;
  uart::UARTComponent *uart2_dev_;
  float float_output_1_{0.0f};
  float float_output_2_{0.0f};
  int int_output_{0};
  uint32_t last_update_{0};

  sensor::Sensor *float_output_1_sensor_{nullptr};
  sensor::Sensor *float_output_2_sensor_{nullptr};
  sensor::Sensor *int_output_sensor_{nullptr};

  esphome::number::Number *corner_1_x_{nullptr};
  esphome::number::Number *corner_1_y_{nullptr};
  esphome::number::Number *corner_2_x_{nullptr};
  esphome::number::Number *corner_2_y_{nullptr};
};

}  // namespace iwr6843aop
}  // namespace esphome