#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tuya {

class TuyaListener {
 public:
  virtual void dp_update(int dpid, uint32_t value) = 0;
};

struct DpT {
  int id;
  int type;
  uint32_t value;
  TuyaListener *listener;
};

class Tuya : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(int dpid, TuyaListener *listener);
  void set_dp_value(int dpid, uint32_t value);
  uint32_t get_dp_value(int dpid);

 protected:
  void handle_char_(int c);
  void handle_command_(uint8_t command, uint8_t version, int count, uint8_t *buffer);
  void send_command_(uint8_t command, int count = 0, uint8_t *buffer = nullptr);

  int gpio_status_ = -1;
  int gpio_reset_ = -1;
  std::vector<DpT> dp_info_;
  bool in_setup_ = true;
  uint32_t last_hb_ = 0;
};

}  // namespace tuya
}  // namespace esphome
