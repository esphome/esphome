#pragma once

#include <span>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/frame_reader.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

namespace esphome::sun_gtil2 {

class SunGTIL2 final : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(ac_voltage)
  SUB_SENSOR(dc_voltage)
  SUB_SENSOR(ac_power)
  SUB_SENSOR(dc_power)
  SUB_SENSOR(limiter_power)
  SUB_SENSOR(temperature)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(state)
  SUB_TEXT_SENSOR(serial_number)
#endif

 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void loop() override;
  void dump_config() override;

  static constexpr size_t STATE_BUFFER_SIZE = 32;
  static constexpr uint16_t MESSAGE_SIZE = 350;

 protected:
  const char *state_to_string_(uint8_t state, std::span<char, STATE_BUFFER_SIZE> buffer);

  float calculate_temperature_(uint16_t adc_value);
  void handle_char_(uint8_t c);
  FrameReader<MESSAGE_SIZE, 0x07, 0x07> reader_;
};

}  // namespace esphome::sun_gtil2
