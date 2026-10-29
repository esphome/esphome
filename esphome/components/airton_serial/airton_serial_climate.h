#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome {
namespace airton_serial {

/// ESPHome climate for the Airton serial bus.
///
/// The ESP takes the role of the original Wi-Fi module (address 0x21):
///  - announces itself to the display with A5 every 10 s;
///  - polls the climate board with AB every 60 s;
///  - parses the A3/A6 state frames and the AC poll response;
///  - sends A1 commands when Home Assistant changes the climate.
class AirtonSerialClimate final : public uart::UARTDevice, public climate::Climate, public Component {
 public:
  AirtonSerialClimate();

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  // Settings exposed as switches. The board shares one bit between anti-mould and auxiliary heat.
  void set_display_state(bool state);
  void set_ionizer_state(bool state);
  void set_sleep_state(bool state);
  void set_eco_state(bool state);
  void set_health_state(bool state);

#ifdef USE_SWITCH
  SUB_SWITCH(display)
  SUB_SWITCH(ionizer)
  SUB_SWITCH(sleep)
  SUB_SWITCH(eco)
  SUB_SWITCH(health)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(total_usage_time)
  SUB_SENSOR(revision)
#endif

 protected:
  void send_a5_();
  void send_ab_();
  void send_command_();
  void parse_buffer_();
  void handle_frame_(const uint8_t *data, size_t len);
  void handle_a3_(const uint8_t *data, size_t len);
  void handle_ac_(const uint8_t *data, size_t len);
  void publish_settings_();
  void set_flag_(uint8_t flag, bool state);

  static uint16_t crc16_(const uint8_t *data, size_t len);

  std::array<uint8_t, 128> rx_buffer_;
  size_t rx_len_{0};
  uint8_t flags_{0x80};
};

}  // namespace airton_serial
}  // namespace esphome
