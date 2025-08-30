#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"

#include "commands.h"

namespace esphome {
namespace dfrobot_c4001 {
// Enumeration for RunApp or Sensor Mode
enum DFRobotMode : uint8_t {
  MODE_PRESENCE = 0,
  MODE_SPEED_AND_DISTANCE,
  MODE_UNKNOWN,
};

enum DFRobotModel : uint8_t {
  MODEL_SEN0609 = 0,
  MODEL_SEN0610,
  MODEL_UNKNOWN,
};

const uint8_t MMWAVE_READ_BUFFER_LENGTH = 64;

static const uint8_t COMMAND_QUEUE_SIZE = 32;

class CircularCommandQueue {
 public:
  int8_t enqueue(std::unique_ptr<Command> cmd, bool first = false);
  std::unique_ptr<Command> dequeue();
  bool is_retry_power_stop();
  bool is_empty();
  bool is_full();
  int8_t process(DFRobotC4001Hub *parent);

 protected:
  int front_{-1};
  int rear_{-1};
  std::unique_ptr<Command> commands_[COMMAND_QUEUE_SIZE];
};

class DFRobotC4001Hub : public uart::UARTDevice, public Component {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(occupancy)
  SUB_BINARY_SENSOR(config_changed)
#endif

#ifdef USE_BUTTON
  SUB_BUTTON(config_save)
  SUB_BUTTON(factory_reset)
  SUB_BUTTON(restart)
#endif

#ifdef USE_NUMBER
  SUB_NUMBER(max_range)
  SUB_NUMBER(min_range)
  SUB_NUMBER(trigger_range)
  SUB_NUMBER(hold_sensitivity)
  SUB_NUMBER(trigger_sensitivity)
  SUB_NUMBER(on_latency)
  SUB_NUMBER(off_latency)
  SUB_NUMBER(inhibit_time)
  SUB_NUMBER(threshold_factor)
#endif

#ifdef USE_SENSOR
  SUB_SENSOR(target_distance)
  SUB_SENSOR(target_speed)
  SUB_SENSOR(target_energy)
#endif

#ifdef USE_SWITCH
  SUB_SWITCH(out_led_enable)
  SUB_SWITCH(run_led_enable)
  SUB_SWITCH(micro_motion_enable)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(software_version)
  SUB_TEXT_SENSOR(hardware_version)
#endif

 public:
  void dump_config() override;
  void setup() override;
  void loop() override;

  void set_min_range(float min, bool needs_save = true);
  void set_max_range(float max, bool needs_save = true);
  void set_trigger_range(float trig, bool needs_save = true);
  void set_hold_sensitivity(float value, bool needs_save = true);
  void set_trigger_sensitivity(float value, bool needs_save = true);
  void set_on_latency(float value, bool needs_save = true);
  void set_off_latency(float value, bool needs_save = true);
  void set_inhibit_time(float value, bool needs_save = true);
  void set_threshold_factor(float value, bool needs_save = true);
  void set_out_led_enable(bool value, bool needs_save = true);
  void set_run_led_enable(bool value, bool needs_save = true);
  void set_micro_motion_enable(bool enable, bool needs_save = true);
  void flash_run_led_enable();
  void set_mode(DFRobotMode value);
  void set_model(DFRobotModel value);
  void set_software_version(char *version);
  void set_hardware_version(char *version);
  void set_needs_save(bool needs_save);
  void setup_module();
  void config_load();
  void config_save();
  void factory_reset();
  void restart();
  void set_occupancy(bool occupancy);
  void set_target_distance(float value);
  void set_target_speed(float value);
  void set_target_energy(float value);
  int8_t enqueue(std::unique_ptr<Command> cmd, bool first = false);

 protected:
  bool is_setup_{false};
  float min_range_{0.6};
  float max_range_{25.0};
  float trigger_range_{6.0};
  float hold_sensitivity_{7};
  float trigger_sensitivity_{5};
  float on_latency_{0.05};
  float off_latency_{15.0};
  float inhibit_time_{1.0};
  float threshold_factor_{5};
  bool micro_motion_enable_{false};
  bool out_led_enable_{true};
  bool run_led_enable_{true};
  bool needs_save_{false};
  bool occupancy_{false};
  float target_distance_;
  float target_speed_;
  float target_energy_;
  DFRobotMode mode_{MODE_PRESENCE};
  DFRobotModel model_{MODEL_UNKNOWN};
  DFRobotModel hw_model_{MODEL_UNKNOWN};
  std::string hw_version_;
  std::string sw_version_;

#ifdef USE_SWITCH
  ESPPreferenceObject pref_;
#endif

  char read_buffer_[MMWAVE_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  CircularCommandQueue cmd_queue_;
  uint32_t ts_last_cmd_sent_{0};
  int32_t ts_cmd_error_cnt_{0};

  uint8_t read_message_();
  uint8_t send_cmd_(const char *cmd, uint32_t duration);

  friend class Command;
  friend class ReadStateCommand;
};

}  // namespace dfrobot_c4001
}  // namespace esphome
