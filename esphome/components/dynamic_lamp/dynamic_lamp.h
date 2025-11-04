#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/gpio/switch/gpio_switch.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/fram/FRAM.h"

namespace esphome {
namespace dynamic_lamp {

enum SupportedSaveModes : uint8_t {
  SAVE_MODE_NONE = 0,
  SAVE_MODE_LOCAL = 1,
  SAVE_MODE_FRAM = 2
};

enum LinkedOutputModeIdx : uint8_t {
  MODE_EQUAL = 0,
  MODE_STATIC = 1,
  MODE_PERCENTAGE = 2,
  MODE_FUNCTION = 3
};

struct LinkedOutput {
  unsigned char validation_bytes[2] = {'F', 'F'};
  bool in_use = false;
  uint8_t output_index = 255;
  uint8_t lamp_index = 255;
  float state = 0.0;
  uint8_t mode = 0;
  float mode_value = 0.0;
  unsigned char linked_relays_[4] = {0, 0, 0, 0};
  optional<float> min_value;
  optional<float> max_value;
  bool update_level = false;
};

struct AvailableOutput {
  output::FloatOutput* output = nullptr;
  std::string output_id = "";
  bool available = false;
};
struct AvailableRelay {
  esphome::gpio::GPIOSwitch* relay = nullptr;
  std::string relay_id = "";
  bool available = false;
  bool in_use = false;
  uint8_t relay_index = 255;
  uint8_t output_index = 255;
};

enum DynamicLampIdx : uint8_t {
  LAMP_1 = 0,
  LAMP_2 = 1,
  LAMP_3 = 2,
  LAMP_4 = 3,
  LAMP_5 = 4,
  LAMP_6 = 5,
  LAMP_7 = 6,
  LAMP_8 = 7,
  LAMP_9 = 8,
  LAMP_10 = 9,
  LAMP_11 = 10,
  LAMP_12 = 11,
  LAMP_13 = 12,
  LAMP_14 = 13,
  LAMP_15 = 14,
  LAMP_16 = 15,
};

struct CombinedLamp {
  unsigned char validation_byte;
  uint8_t lamp_index : 4;
  bool active : 1;
  bool update_ : 1;
  bool available : 1;
  bool on_ : 1;
  unsigned char name[16];
  float state_;
  unsigned char used_outputs[2];
};

struct AvailableLamp {
  light::LightState* lamp = nullptr;
  std::string lamp_id = "";
  bool available = false;
};

struct DynamicLampTimer {
  unsigned char validation_byte;
  unsigned char timer_desc[32];
  unsigned char lamp_list[2];
  uint16_t begin_date_year : 11;
  uint8_t begin_date_month : 4;
  uint8_t begin_date_day : 5;
  uint16_t end_date_year : 11;
  uint8_t end_date_month : 4;
  uint8_t end_date_day : 5;
  bool in_use : 1;
  uint8_t action : 2;
  uint8_t hour : 5;
  uint8_t minute : 6;
  bool active : 1;
  bool monday : 1;
  bool tuesday : 1;
  bool wednesday : 1;
  bool thursday : 1;
  bool friday : 1;
  bool saturday : 1;
  bool sunday : 1;
  bool is_dst : 1;
  bool respect_dst : 1;
  uint16_t action_value;
};

class DynamicLamp;

class DynamicLampComponent : public Component {
 public:
  explicit DynamicLampComponent(time::RealTimeClock *rtc, fram::FRAM *fram) : rtc_(rtc), fram_(fram) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  void begin();
  void add_available_output(output::FloatOutput* output, std::string output_id);
  void add_available_lamp(light::LightState* lamp, std::string output_id);
  void add_available_relay(esphome::gpio::GPIOSwitch* relay, std::string relay_id);
  void set_save_mode(uint8_t save_mode);
  void set_lamp_level(uint8_t lamp_number, float state);
  void set_lamp_level_by_name(std::string lamp_name, float state);
  bool add_lamp(std::string name);
  void remove_lamp(std::string name);
  uint8_t get_lamp_count();
  std::string get_lamp_name(uint8_t lamp_number);
  std::array<bool, 16> get_active_lamps();
  void rename_lamp(std::string old_lamp_name, std::string new_lamp_name);
  void add_output_to_lamp(std::string lamp_name, LinkedOutput *output, uint8_t mode, float mode_value, std::string relay_ids);
  std::array<bool, 16> get_available_outputs();
  std::string get_output_id_string(uint8_t output_index);
  LinkedOutput get_linked_output(uint8_t output_index);
  void remove_output_from_lamp(uint8_t lamp_index, LinkedOutput *output);
  void remove_output_from_lamp_by_name(std::string lamp_name, LinkedOutput *output);
  void attach_output_to_lamp(std::string lamp_name, std::string output_id, uint8_t mode, float mode_value, std::string relay_ids);
  void add_relay_to_output(std::string output_id, std::string relay_id);
  void add_relays_to_output_by_pointer(LinkedOutput *output, std::string relay_ids);
  void remove_relay_from_output(std::string output_id, std::string relay_id);
  void remove_relay_from_output_by_index(uint8_t output_index, uint8_t relay_index);
  void add_relays_to_output(std::string output_id, std::string relay_ids);
  void remove_relays_from_output(std::string output_id, std::string relay_ids);
  std::string get_relay_list_for_output(uint8_t output_index);
  std::array<bool, 16> get_lamp_outputs(uint8_t lamp_number);
  bool add_timer(std::string timer_desc, std::string lamp_name, bool timer_active, uint8_t action, uint16_t action_value,
    uint8_t hour, uint8_t minute, bool monday, bool tuesday, bool wednesday, bool thursday, bool friday,
    bool saturday, bool sunday, bool respect_dst, ESPTime valid_from_date, ESPTime valid_until_date);
  bool save_timer(std::string old_timer_desc, std::string new_timer_desc,std::string lamp_list_str, bool timer_active, uint8_t action,
      uint16_t action_value, uint8_t hour, uint8_t minute, bool monday, bool tuesday, bool wednesday, bool thursday, bool friday,
      bool saturday, bool sunday, bool respect_dst, ESPTime valid_from_date, ESPTime valid_until_date);
  DynamicLampTimer get_timer(uint8_t timer_index);
  DynamicLampTimer get_timer_by_name(std::string timer_desc);
  bool remove_timer(std::string timer_desc);
  std::array<bool, 64> get_in_use_timers();
  std::string get_timer_desc(uint8_t lamp_number);
  void read_initialized_timers_to_log();
  void read_fram_timers_to_log();
  void read_initialized_settings_to_log();
  void read_fram_settings_to_log();
  void read_output_config_to_log();
  void clear_fram();

 protected:
  friend class DynamicLamp;
  time::RealTimeClock *rtc_;
  fram::FRAM *fram_;
  void restore_lamp_settings_();
  void restore_timers_();
  uint8_t get_timer_index_by_name_(std::string timer_desc);
  bool write_state_(uint8_t lamp_number, float state);
  uint8_t get_lamp_index_by_name_(std::string lamp_name);
  std::array<bool, 16> get_lamp_outputs_by_name_(std::string lamp_name);
  std::vector<uint8_t> split_to_int_vector_(std::string lamp_list_str);
  std::vector<bool> build_lamp_list_from_list_str_(std::string lamp_list_str);
  std::vector<uint8_t> get_timers_to_execute_(ESPTime& now);
  time_t get_next_active_timer_timestamp_(ESPTime& now);
  time_t get_next_timer_timestamp_by_day_(ESPTime now, bool ignore_current_time = false);
 
  CombinedLamp active_lamps_[16];
  AvailableLamp available_lamps_[16];
  AvailableRelay available_relays_[32];
  LinkedOutput linked_outputs_[16];
  AvailableOutput available_outputs_[16];
  DynamicLampTimer timers_[64];
  uint8_t save_mode_;
  uint8_t lamp_count_ = 0;
  time_t next_timer_exec_time_{0};
};


}  // namespace dynamic_lamp
}  // namespace esphome
