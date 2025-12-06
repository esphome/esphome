#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace ufm01 {

class UFM01Component : public uart::UARTDevice, public sensor::Sensor, public Component {

SUB_SENSOR(volume)
SUB_SENSOR(flow)
SUB_SENSOR(temperature)

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(ufp_chip_error)
  SUB_BINARY_SENSOR(flow_direction_wrong)
  SUB_BINARY_SENSOR(empty_tube)
  SUB_BINARY_SENSOR(flow_rate_out_of_range)
#endif
  
 public:

  void setup() override;
  
  void loop() override;

  float get_setup_priority() const override;

 protected:

  bool clear_accumulated_flow();
  bool set_active_mode();
  bool reset_device();

 private:
  bool send_command(std::vector<uint8_t> command);
  
  int read_index = 0;
  uint8_t data[32];
  void on_data(uint8_t data[32]);
};

}  // namespace ufm01
}  // namespace esphome