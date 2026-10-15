#pragma once

// Midea PortaSplit climate component.
//
// The PortaSplit uses sync byte 0x00 for climate frames and silently drops the
// XOR sync byte (LEN ^ 0xAC) frames that the `midea` component sends, which is
// why the stock `midea` platform cannot drive this unit.
//
// Frame:  AA | LEN | AC | SYNC | 00 00 00 00 | PROTO | TYPE | PAYLOAD | CRC8 | CS

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

#include <array>

namespace esphome::midea_portasplit {

static constexpr uint8_t FAN_AUTO = 102;

class PortaSplitClimate : public climate::Climate, public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

 protected:
  static constexpr size_t MAX_FRAME = 128;

  // The payload may live in PROGMEM.
  void send_frame_(uint8_t proto, uint8_t type, const uint8_t *payload, size_t len);
  void send_set_command_();
  void send_status_query_();
  void send_module_frame_(uint8_t type, uint8_t counter, uint8_t wifi_flags);
  void send_capability_queries_();

  void rx_feed_(uint8_t b);
  void handle_frame_(const uint8_t *f, size_t len);
  void handle_c0_(const uint8_t *f, size_t len);

  uint32_t rx_last_byte_{0};
  // Last known device state, mirrored from C0 so a set command does not clobber it.
  uint8_t st_target_half_{48};  // Target temperature in half degrees
  uint8_t st_mode_{1};
  uint8_t st_fan_{FAN_AUTO};
  bool st_power_ : 1 {false};
  bool st_swing_ : 1 {false};
  bool st_turbo_ : 1 {false};
  bool st_ion_ : 1 {false};
  bool st_sleep_ : 1 {false};
  bool st_eco_ : 1 {false};
  bool st_frost_ : 1 {false};

  uint8_t msg_id_{1};
  uint8_t net_counter_{0};
  uint8_t rx_len_{0};
  std::array<uint8_t, MAX_FRAME> rx_{};
};

}  // namespace esphome::midea_portasplit
