#pragma once

#include <cstdint>
#include <vector>

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace airton_serial {

/// ESPHome climate for the Airton serial bus.
///
/// The ESP takes the role of the original Wi-Fi module (address 0x21):
///  - announces itself to the display with A5 every 10 s;
///  - polls the climate board with AB every 60 s;
///  - parses the A3/A6 state frames sent by the board;
///  - sends A1 commands when Home Assistant changes the climate.
class AirtonSerialClimate : public uart::UARTDevice, public climate::Climate, public Component {
 public:
  AirtonSerialClimate();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

 protected:
  void send_a5_();
  void send_ab_();
  void send_command_();
  void parse_buffer_();
  void handle_frame_(const uint8_t *data, size_t len);
  void handle_a3_(const uint8_t *data, size_t len);

  static uint16_t crc16_(const uint8_t *data, size_t len);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_a5_{0};
  uint32_t last_ab_{0};
};

}  // namespace airton_serial
}  // namespace esphome
