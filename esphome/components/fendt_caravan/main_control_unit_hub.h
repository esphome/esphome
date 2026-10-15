#pragma once

#ifdef USE_ESP32
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "fendt_caravan.h"
#include "fendt_switch.h"
#include "press_control.h"

namespace esphome::fendt_caravan {

/// The main control unit: mains power, lights, floor heater, temperatures and the fresh water level
class MainControlUnitHub : public Component {
 public:
  explicit MainControlUnitHub(FendtCaravan *link) : link_(link) { link->set_hub(this); }
  void dump_config() override;
  /// Publishes the entity a report belongs to; `value` is writable so a decoder can edit it in place
  void handle_report(const char *key, char *value);
  void on_link_lost() {
    this->hs_key_.reset();
    this->floor_heater_.reset();
  }
  /// Called by the link when a queued command leaves the queue; `sent` is false when it was given up
  void on_command_done(const char *command, bool sent);
  /// Asks for `sw` to reach `state`; presses are sent one at a time, each judged against the last report
  void write_switch(FendtSwitch *sw, bool state);

  SUB_SENSOR(temp_in)
  SUB_SENSOR(temp_out)
  SUB_SENSOR(water_level)
  SUB_BINARY_SENSOR(power_status)
  SUB_TEXT_SENSOR(software_version)
  SUB_SWITCH(main)
  SUB_SWITCH(all_lights)
  SUB_SWITCH(floor_heater)

 protected:
  /// Queues a command PanelControl asked for and tells it whether the link took it
  void send_(PanelControl &control, const char *command);

  FendtCaravan *link_;
  PanelControl hs_key_{nullptr};  // 0 off, 1 on, 2 on with all lights
  PanelControl floor_heater_{CMD_FLOOR_HEATER_TOGGLE};
};

}  // namespace esphome::fendt_caravan
#endif
