#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/lg_uart/lg_hub.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace lg_uart {

class LGUartSensor : public sensor::Sensor, public LGUartClient, public PollingComponent {
 public:
  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::LATE; }

  std::string describe() override;

  /** Called when uart packet for us inbound */
  void on_reply_packet(std::vector<uint8_t> *pkt) override;

  /** Command specific */
  void set_cmd(const std::string &cmd_str);

 protected:
};

}  // namespace lg_uart
}  // namespace esphome
