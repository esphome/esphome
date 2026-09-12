#pragma once

// =====================================================================
// Midea PortaSplit climate component.
//
// Native, dependency-free implementation of the PortaSplit UART protocol
// variant. Unlike the `midea` component (which wraps dudanov/MideaUART and
// uses the XOR sync byte `LEN ^ 0xAC`), the PortaSplit uses sync byte 0x00
// for climate frames and silently drops XOR-sync climate frames. That is
// why the stock `midea` platform cannot drive this unit.
//
// Frame:  AA | LEN | AC | SYNC | 00 00 00 00 | PROTO | TYPE | PAYLOAD | CRC8 | CS
// =====================================================================

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"

#include <vector>

namespace esphome::midea_portasplit {

// ---- B0/B1 property IDs (little endian on the wire) ----
static const uint16_t PROP_SILENT_MODE = 0x00CD;  // Out Silent Mode 0..3
static const uint16_t PROP_POWER_LIMIT = 0x0048;  // 50 / 75 / 100 %
static const uint16_t PROP_BEEPER = 0x022C;       // 0/1
static const uint16_t PROP_SELF_CLEAN = 0x0039;   // 0/1 (AC powers OFF when deactivated!)
static const uint16_t PROP_TEMP_RANGE = 0x0051;   // 5 bytes: [enable, min*2, max*2, FF, FF]
static const uint16_t PROP_SERIAL = 0x00AB;       // 28 bytes ASCII
static const uint16_t PROP_TELEM_TEMPS = 0x00E0;  // B5 notify: [?, 6x LE16 degC*100, ?]

class PortaSplitClimate;

enum PortaSplitSwitchType : uint8_t {
  SW_ION = 0,
  SW_BEEPER,
  SW_SELF_CLEAN,
  SW_LED,
  SW_TEMP_RANGE,
  SW_SILENT,
};

enum PortaSplitSelectType : uint8_t {
  SEL_POWER_LIMIT = 0,
};

enum PortaSplitNumberType : uint8_t {
  NUM_FAN_SPEED = 0,
  NUM_RANGE_MIN,
  NUM_RANGE_MAX,
};

class PortaSplitSwitch : public switch_::Switch {
 public:
  void set_parent(PortaSplitClimate *parent, PortaSplitSwitchType type) {
    this->parent_ = parent;
    this->type_ = type;
  }

 protected:
  void write_state(bool state) override;
  PortaSplitClimate *parent_{nullptr};
  PortaSplitSwitchType type_{SW_ION};
};

class PortaSplitSelect : public select::Select {
 public:
  void set_parent(PortaSplitClimate *parent, PortaSplitSelectType type) {
    this->parent_ = parent;
    this->type_ = type;
  }

 protected:
  void control(const std::string &value) override;
  PortaSplitClimate *parent_{nullptr};
  PortaSplitSelectType type_{SEL_POWER_LIMIT};
};

class PortaSplitNumber : public number::Number {
 public:
  void set_parent(PortaSplitClimate *parent, PortaSplitNumberType type) {
    this->parent_ = parent;
    this->type_ = type;
  }

 protected:
  void control(float value) override;
  PortaSplitClimate *parent_{nullptr};
  PortaSplitNumberType type_{NUM_FAN_SPEED};
};

class PortaSplitClimate : public climate::Climate, public uart::UARTDevice, public Component {
 public:
  // ---------- setters used by codegen ----------
  void set_outdoor_temperature_sensor(sensor::Sensor *s) { this->outdoor_temperature_sensor_ = s; }
  void set_power_sensor(sensor::Sensor *s) { this->power_sensor_ = s; }
  void set_energy_sensor(sensor::Sensor *s) { this->energy_sensor_ = s; }
  void set_serial_number_sensor(text_sensor::TextSensor *s) { this->serial_number_sensor_ = s; }
  void set_firmware_sensor(text_sensor::TextSensor *s) { this->firmware_sensor_ = s; }
  void set_outdoor_fan_sensor(sensor::Sensor *s) { this->outdoor_fan_sensor_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_sensor_ = s; }
  void set_compressor_freq_sensor(sensor::Sensor *s) { this->compressor_freq_sensor_ = s; }
  void set_defrost_sensor(binary_sensor::BinarySensor *s) { this->defrost_sensor_ = s; }
  void set_evap_temp_sensor(sensor::Sensor *s) { this->evap_temp_sensor_ = s; }
  void set_cond_temp_sensor(sensor::Sensor *s) { this->cond_temp_sensor_ = s; }
  void set_discharge_temp_sensor(sensor::Sensor *s) { this->discharge_temp_sensor_ = s; }
  void set_compressor_runtime_sensor(sensor::Sensor *s) { this->compressor_runtime_sensor_ = s; }
  void set_suction_temp_sensor(sensor::Sensor *s) { this->suction_temp_sensor_ = s; }
  void set_indoor_fan_rpm_sensor(sensor::Sensor *s) { this->indoor_fan_rpm_sensor_ = s; }
  void set_comp_frequency_sensor(sensor::Sensor *s) { this->comp_frequency_sensor_ = s; }
  void set_comp_current_sensor(sensor::Sensor *s) { this->comp_current_sensor_ = s; }
  void set_ion_switch(PortaSplitSwitch *s) { this->ion_switch_ = s; }
  void set_beeper_switch(PortaSplitSwitch *s) { this->beeper_switch_ = s; }
  void set_self_clean_switch(PortaSplitSwitch *s) { this->self_clean_switch_ = s; }
  void set_led_switch(PortaSplitSwitch *s) { this->led_switch_ = s; }
  void set_silent_switch(PortaSplitSwitch *s) { this->silent_switch_ = s; }
  void set_power_limit_select(PortaSplitSelect *s) { this->power_limit_select_ = s; }
  void set_temp_range_switch(PortaSplitSwitch *s) { this->temp_range_switch_ = s; }
  void set_bcd_energy(bool bcd) { this->bcd_energy_ = bcd; }
  void set_log_frames(bool log) { this->log_frames_ = log; }
  void set_fan_number(PortaSplitNumber *n) { this->fan_number_ = n; }
  void set_range_min_number(PortaSplitNumber *n) { this->range_min_number_ = n; }
  void set_range_max_number(PortaSplitNumber *n) { this->range_max_number_ = n; }

  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // ---------- climate ----------
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  // ---------- called by sub-entities ----------
  void switch_command(PortaSplitSwitchType type, bool state);
  void select_command(PortaSplitSelectType type, const std::string &value);
  void number_command(PortaSplitNumberType type, float value);

 protected:
  static const uint32_t STATUS_INTERVAL = 30000;  // 30 s
  static const uint32_t ENERGY_INTERVAL = 60000;  // 60 s
  static const uint32_t PROPS_INTERVAL = 60000;   // 60 s

  // ================= frame helpers =================
  static uint8_t crc8_(const uint8_t *data, size_t len);
  void send_frame_(uint8_t proto, uint8_t type, const uint8_t *payload, size_t len, bool add_crc = true);

  // ================= outgoing commands =================
  void send_set_command_();
  void send_status_query_();
  void send_energy_query_();
  void send_group_query_(uint8_t group);
  void send_group5_query_();
  void send_network_status_();
  void send_keepalive_response_();
  void send_device_info_query_();
  void send_led_toggle_();
  void send_b1_read_(const std::vector<uint16_t> &ids);
  void send_b0_write_(uint16_t id, uint8_t value) { this->send_b0_write_bytes_(id, &value, 1); }
  void send_b0_write_bytes_(uint16_t id, const uint8_t *data, uint8_t len);
  void send_temp_range_();

  // ================= RX handling =================
  void rx_feed_(uint8_t b);
  void handle_frame_(const std::vector<uint8_t> &f);
  void handle_c0_(const std::vector<uint8_t> &f);
  void handle_c1_(const std::vector<uint8_t> &f);
  void handle_b1_b0_(const std::vector<uint8_t> &f);
  void apply_property_(uint16_t id, const uint8_t *value, uint8_t len);
  void handle_b5_(const std::vector<uint8_t> &f);
  void handle_a0_(const std::vector<uint8_t> &f);

  static uint32_t bcd_(uint8_t b) { return (uint32_t) (b >> 4) * 10 + (b & 0x0F); }

  // ================= mode mapping =================
  static uint8_t climate_mode_to_midea_(climate::ClimateMode m);
  static climate::ClimateMode midea_mode_to_climate_(uint8_t m);

  // ================= state =================

  // desired / last known device state (mirrored from C0)
  bool st_power_{false};
  uint8_t st_mode_{1};
  float st_target_{24.0f};
  uint8_t st_fan_{102};
  bool st_swing_{false};
  bool st_turbo_{false};
  bool st_ion_{false};
  bool st_sleep_{false};
  bool st_eco_{false};
  bool st_frost_{false};
  bool led_state_{true};
  bool range_enable_{false};
  bool bcd_energy_{false};
  bool log_frames_{false};
  float range_min_{16.0f};
  float range_max_{30.0f};

  uint8_t msg_id_{1};
  std::vector<uint8_t> rx_;
  uint32_t last_status_poll_{0};
  uint32_t last_energy_poll_{0};
  uint32_t last_props_poll_{0};
  uint32_t followup_at_{0};
  bool serial_requested_{false};
  bool fw_requested_{false};
  bool caps_requested_{false};
  bool caps1_requested_{false};
  uint32_t last_net_status_{0};
  uint8_t net_counter_{0};
  uint32_t last_group5_poll_{0};
  uint32_t last_diag_poll_{0};

  // sub-entities
  sensor::Sensor *outdoor_temperature_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  text_sensor::TextSensor *firmware_sensor_{nullptr};
  sensor::Sensor *outdoor_fan_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *compressor_freq_sensor_{nullptr};
  binary_sensor::BinarySensor *defrost_sensor_{nullptr};
  sensor::Sensor *evap_temp_sensor_{nullptr};
  sensor::Sensor *cond_temp_sensor_{nullptr};
  sensor::Sensor *discharge_temp_sensor_{nullptr};
  sensor::Sensor *compressor_runtime_sensor_{nullptr};
  sensor::Sensor *suction_temp_sensor_{nullptr};
  sensor::Sensor *indoor_fan_rpm_sensor_{nullptr};
  sensor::Sensor *comp_frequency_sensor_{nullptr};
  sensor::Sensor *comp_current_sensor_{nullptr};
  PortaSplitSwitch *ion_switch_{nullptr};
  PortaSplitSwitch *beeper_switch_{nullptr};
  PortaSplitSwitch *self_clean_switch_{nullptr};
  PortaSplitSwitch *led_switch_{nullptr};
  PortaSplitSwitch *silent_switch_{nullptr};
  PortaSplitSelect *power_limit_select_{nullptr};
  PortaSplitSwitch *temp_range_switch_{nullptr};
  PortaSplitNumber *fan_number_{nullptr};
  PortaSplitNumber *range_min_number_{nullptr};
  PortaSplitNumber *range_max_number_{nullptr};
};

}  // namespace esphome::midea_portasplit
