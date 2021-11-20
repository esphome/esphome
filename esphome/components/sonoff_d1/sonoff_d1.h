#pragma once

/*
  sonoff_d1.h - Sonoff D1 Dimmer support for ESPHome

  Copyright © 2021 Anatoly Savchenkov
  Copyright © 2020 Jeff Rescignano

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  and associated documentation files (the “Software”), to deal in the Software without
  restriction, including without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  -----

  If modifying this file, in addition to the license above, please ensure to include links back to the original code:
  https://jeffresc.dev/blog/2020-10-10
  https://github.com/JeffResc/Sonoff-D1-Dimmer
  https://github.com/arendst/Tasmota/blob/2d4a6a29ebc7153dbe2717e3615574ac1c84ba1d/tasmota/xdrv_37_sonoff_d1.ino#L119-L131

  -----
*/

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"

namespace esphome {
namespace sonoff_d1 {

class SonoffD1Output : public light::LightOutput, public uart::UARTDevice, public Component {
 public:
  // LightOutput methods
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { this->light_state_ = state; }
  void write_state(light::LightState *state) override;

  // Component methods
  void setup() override{};
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  // Custom method
  void set_use_rm433_remote(const bool use_rm433_remote) { this->use_rm433_remote_ = use_rm433_remote; }

 protected:
  bool use_rm433_remote_;
  bool last_binary_{false};
  int last_brightness_{0};
  int write_count_{0};
  int read_count_{0};
  light::LightState *light_state_{nullptr};

  uint8_t calc_checksum_(const uint8_t *cmd, size_t len);
  void populate_checksum_(uint8_t *cmd, size_t len);
  char *dump_cmd_(const uint8_t *cmd, size_t len);
  void skip_command_();
  bool read_command_(uint8_t *cmd, size_t &len);
  bool read_ack_(const uint8_t *cmd, size_t len);
  bool write_command_(uint8_t *cmd, size_t len, bool needs_ack = true);
  bool control_dimmer_(bool binary, int brightness);
  void process_command_(const uint8_t *cmd, size_t len);
  void publish_state_(float brightness);
};

}  // namespace sonoff_d1
}  // namespace esphome
