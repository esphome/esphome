#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"

#include <array>

// component API definition at https://www.sciosense.com/wp-content/uploads/2025/06/UFM-01-Datasheet-1.pdf

namespace esphome::ufm01 {

static constexpr size_t FRAME_SIZE = 32;

class UFM01Component : public uart::UARTDevice, public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(accumulated_flow)
  SUB_SENSOR(flow)
  SUB_SENSOR(temperature)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(ufc_chip_error)
  SUB_BINARY_SENSOR(flow_direction_wrong)
  SUB_BINARY_SENSOR(empty_tube)
  SUB_BINARY_SENSOR(flow_rate_out_of_range)
#endif

 public:
  void setup() override;

  void dump_config() override;

  void loop() override;

  float get_setup_priority() const override;

 protected:
  bool clear_accumulated_flow_();
  bool set_active_mode_();
  bool reset_device_();

 private:
  bool send_command_(const std::array<uint8_t, 7> &command);

  int32_t read_index_ = 0;
  uint8_t data_[FRAME_SIZE];
  void on_data_(uint8_t data[FRAME_SIZE]);
};

}  // namespace esphome::ufm01
