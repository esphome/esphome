#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/gpio.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include <array>
#include <vector>
#include <cmath>

namespace esphome::ld6002b {

static constexpr uint8_t MAX_TARGETS = 3;
static constexpr uint8_t AREA_COUNT = 4;
static constexpr size_t DEFAULT_MAX_DATA_LEN = 1024;
static constexpr size_t DEFAULT_MAX_DATA_LEN_POINT_CLOUD = 4096;
static constexpr size_t CMD_MAX_DATA_LEN = 32;

enum class NumberType : uint8_t {
  HOLD_DELAY,
  Z_MIN,
  Z_MAX,
  LOW_POWER_SLEEP,
  AREA_X_MIN,
  AREA_X_MAX,
  AREA_Y_MIN,
  AREA_Y_MAX,
  AREA_Z_MIN,
  AREA_Z_MAX,
};

enum class SelectType : uint8_t {
  SENSITIVITY,
  TRIGGER_SPEED,
  INSTALLATION_MODE,
  AREA_ID,
};

enum class SwitchType : uint8_t {
  LOW_POWER,
  POINT_CLOUD,
  TARGET_DISPLAY,
};

enum class ButtonType : uint8_t {
  APPLY_AREA,
  AUTO_INTERFERENCE,
  GET_AREAS,
  CLEAR_INTERFERENCE,
  RESET_DETECTION_AREA,
  GET_DELAY,
  GET_SENSITIVITY,
  GET_TRIGGER_SPEED,
  GET_Z_RANGE,
  GET_INSTALLATION,
  GET_LOW_POWER_MODE,
  GET_LOW_POWER_SLEEP_TIME,
  RESET_UNATTENDED,
  WAKE,
};

#ifdef USE_SENSOR
struct TargetSensors {
  sensor::Sensor *x{nullptr};
  sensor::Sensor *y{nullptr};
  sensor::Sensor *z{nullptr};
  sensor::Sensor *dop_idx{nullptr};
  sensor::Sensor *cluster_id{nullptr};
};

struct AreaSensors {
  sensor::Sensor *x_min{nullptr};
  sensor::Sensor *x_max{nullptr};
  sensor::Sensor *y_min{nullptr};
  sensor::Sensor *y_max{nullptr};
  sensor::Sensor *z_min{nullptr};
  sensor::Sensor *z_max{nullptr};
};
#endif

struct AreaConfig {
  float x_min{NAN};
  float x_max{NAN};
  float y_min{NAN};
  float y_max{NAN};
  float z_min{NAN};
  float z_max{NAN};
};

struct VersionPref {
  char value[16];
};

class LD6002BComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_wakeup_pin(GPIOPin *pin) { this->wakeup_pin_ = pin; }
  void set_wakeup_pulse_ms(uint32_t ms) { this->wakeup_pulse_ms_ = ms; }
  void set_auto_wake(bool enable) { this->auto_wake_ = enable; }
  void set_throttle(uint32_t throttle_ms) { this->throttle_ms_ = throttle_ms; }

#ifdef USE_SENSOR
  void set_target_count_sensor(sensor::Sensor *sensor) { this->target_count_sensor_ = sensor; }
  void set_point_count_sensor(sensor::Sensor *sensor) { this->point_count_sensor_ = sensor; }

  void set_target_x_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].x = sensor; }
  void set_target_y_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].y = sensor; }
  void set_target_z_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].z = sensor; }
  void set_target_dop_idx_sensor(uint8_t target, sensor::Sensor *sensor) { this->targets_[target].dop_idx = sensor; }
  void set_target_cluster_id_sensor(uint8_t target, sensor::Sensor *sensor) {
    this->targets_[target].cluster_id = sensor;
  }
  void set_interference_area_x_min_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->interference_areas_[area].x_min = sensor;
  }
  void set_interference_area_x_max_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->interference_areas_[area].x_max = sensor;
  }
  void set_interference_area_y_min_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->interference_areas_[area].y_min = sensor;
  }
  void set_interference_area_y_max_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->interference_areas_[area].y_max = sensor;
  }
  void set_interference_area_z_min_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->interference_areas_[area].z_min = sensor;
  }
  void set_interference_area_z_max_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->interference_areas_[area].z_max = sensor;
  }

  void set_detection_area_x_min_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->detection_areas_[area].x_min = sensor;
  }
  void set_detection_area_x_max_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->detection_areas_[area].x_max = sensor;
  }
  void set_detection_area_y_min_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->detection_areas_[area].y_min = sensor;
  }
  void set_detection_area_y_max_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->detection_areas_[area].y_max = sensor;
  }
  void set_detection_area_z_min_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->detection_areas_[area].z_min = sensor;
  }
  void set_detection_area_z_max_sensor(uint8_t area, sensor::Sensor *sensor) {
    this->detection_areas_[area].z_max = sensor;
  }
#endif

#ifdef USE_BINARY_SENSOR
  void set_presence_binary_sensor(binary_sensor::BinarySensor *sensor) { this->presence_binary_sensor_ = sensor; }
  void set_target_presence_binary_sensor(uint8_t target, binary_sensor::BinarySensor *sensor) {
    this->target_presence_[target] = sensor;
  }
  void set_area_presence_binary_sensor(uint8_t area, binary_sensor::BinarySensor *sensor) {
    this->area_presence_[area] = sensor;
  }
#endif

#ifdef USE_TEXT_SENSOR
  void set_work_mode_text_sensor(text_sensor::TextSensor *sensor) { this->work_mode_text_sensor_ = sensor; }
  void set_ota_version_text_sensor(text_sensor::TextSensor *sensor) { this->ota_version_text_sensor_ = sensor; }
#endif

#ifdef USE_NUMBER
  void set_hold_delay_number(number::Number *number) { this->hold_delay_number_ = number; }
  void set_z_min_number(number::Number *number) { this->z_min_number_ = number; }
  void set_z_max_number(number::Number *number) { this->z_max_number_ = number; }
  void set_low_power_sleep_number(number::Number *number) { this->low_power_sleep_number_ = number; }

  void set_area_x_min_number(number::Number *number) { this->area_x_min_number_ = number; }
  void set_area_x_max_number(number::Number *number) { this->area_x_max_number_ = number; }
  void set_area_y_min_number(number::Number *number) { this->area_y_min_number_ = number; }
  void set_area_y_max_number(number::Number *number) { this->area_y_max_number_ = number; }
  void set_area_z_min_number(number::Number *number) { this->area_z_min_number_ = number; }
  void set_area_z_max_number(number::Number *number) { this->area_z_max_number_ = number; }
#endif

#ifdef USE_SELECT
  void set_sensitivity_select(select::Select *select) { this->sensitivity_select_ = select; }
  void set_trigger_speed_select(select::Select *select) { this->trigger_speed_select_ = select; }
  void set_installation_select(select::Select *select) { this->installation_select_ = select; }
  void set_area_id_select(select::Select *select) { this->area_id_select_ = select; }
#endif

#ifdef USE_SWITCH
  void set_low_power_switch(switch_::Switch *sw) { this->low_power_switch_ = sw; }
  void set_point_cloud_switch(switch_::Switch *sw) { this->point_cloud_switch_ = sw; }
  void set_target_display_switch(switch_::Switch *sw) { this->target_display_switch_ = sw; }
#endif

  void set_number_value(NumberType type, float value);
  void set_select_value(SelectType type, size_t index);
  void set_switch_state(SwitchType type, bool state);
  void press_button(ButtonType type);
  void set_max_data_len(size_t max_data_len);

 protected:
  enum class ParseState : uint8_t { SOF, HEADER, HCK, DATA, DCK, DISCARD };

  struct PendingCommand {
    uint16_t type{0};
    uint8_t len{0};
    std::array<uint8_t, CMD_MAX_DATA_LEN> data{};
  };

  bool should_throttle_stream_(uint32_t &last_publish_ms);
  void init_installation_pref_();
  void save_installation_pref_(uint8_t value);
  void parse_byte_(uint8_t byte);
  void reset_parser_();
  void handle_frame_(uint16_t type, const uint8_t *data, uint16_t len);
  void handle_target_report_(const uint8_t *data, uint16_t len);
  void handle_point_cloud_(const uint8_t *data, uint16_t len);
  void handle_area_presence_(const uint8_t *data, uint16_t len);
  void handle_area_report_(bool interference, const uint8_t *data, uint16_t len);
  void handle_delay_report_(const uint8_t *data, uint16_t len);
  void handle_sensitivity_report_(const uint8_t *data, uint16_t len);
  void handle_trigger_speed_report_(const uint8_t *data, uint16_t len);
  void handle_z_range_report_(const uint8_t *data, uint16_t len);
  void handle_installation_report_(const uint8_t *data, uint16_t len);
  void handle_low_power_report_(const uint8_t *data, uint16_t len);
  void handle_low_power_sleep_report_(const uint8_t *data, uint16_t len);
  void handle_work_mode_report_(const uint8_t *data, uint16_t len);
  void handle_version_report_(const uint8_t *data, uint16_t len);
  void update_work_mode_fallback_();
  void publish_work_mode_(bool low_power);
  void update_area_numbers_(const AreaConfig &area);
  void update_area_numbers_for_id_(uint8_t area_id);
  void queue_area_config_(uint8_t area_id, const AreaConfig &desired);
  void try_apply_pending_area_();
  void init_area_id_pref_();
  void save_area_id_pref_(uint8_t value);
  void init_version_pref_();
  void save_version_pref_(const char *value);

  void queue_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void process_command_queue_();
  void send_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void send_command_untracked_(uint16_t type, const uint8_t *data, uint8_t len);
  void send_command_internal_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void write_frame_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void send_control_command_(uint32_t command);
  void apply_area_config_();
  void wake_();

  static uint16_t read_u16_be(const uint8_t *data);
  static uint32_t read_u32_le(const uint8_t *data);
  static int32_t read_int32_le(const uint8_t *data);
  static float read_f32_le(const uint8_t *data);
  static void write_u32_le(uint8_t *data, uint32_t value);
  static void write_int32_le(uint8_t *data, int32_t value);
  static void write_f32_le(uint8_t *data, float value);
  static bool should_publish_float(float previous, float next, float epsilon = 0.0001f);

#ifdef USE_SENSOR
  std::array<TargetSensors, MAX_TARGETS> targets_{};
  sensor::Sensor *target_count_sensor_{nullptr};
  sensor::Sensor *point_count_sensor_{nullptr};
  std::array<AreaSensors, AREA_COUNT> interference_areas_{};
  std::array<AreaSensors, AREA_COUNT> detection_areas_{};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *presence_binary_sensor_{nullptr};
  std::array<binary_sensor::BinarySensor *, MAX_TARGETS> target_presence_{};
  std::array<binary_sensor::BinarySensor *, AREA_COUNT> area_presence_{};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *work_mode_text_sensor_{nullptr};
  text_sensor::TextSensor *ota_version_text_sensor_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *hold_delay_number_{nullptr};
  number::Number *z_min_number_{nullptr};
  number::Number *z_max_number_{nullptr};
  number::Number *low_power_sleep_number_{nullptr};

  number::Number *area_x_min_number_{nullptr};
  number::Number *area_x_max_number_{nullptr};
  number::Number *area_y_min_number_{nullptr};
  number::Number *area_y_max_number_{nullptr};
  number::Number *area_z_min_number_{nullptr};
  number::Number *area_z_max_number_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *sensitivity_select_{nullptr};
  select::Select *trigger_speed_select_{nullptr};
  select::Select *installation_select_{nullptr};
  select::Select *area_id_select_{nullptr};
  ESPPreferenceObject installation_pref_{};
  bool installation_pref_initialized_{false};
  ESPPreferenceObject area_id_pref_{};
  bool area_id_pref_initialized_{false};
#endif
  ESPPreferenceObject version_pref_{};
  bool version_pref_initialized_{false};
#ifdef USE_SWITCH
  switch_::Switch *low_power_switch_{nullptr};
  switch_::Switch *point_cloud_switch_{nullptr};
  switch_::Switch *target_display_switch_{nullptr};
#endif

  GPIOPin *wakeup_pin_{nullptr};
  uint32_t wakeup_pulse_ms_{50};
  bool auto_wake_{true};
  uint32_t throttle_ms_{0};
  uint32_t last_target_publish_{0};
  uint32_t last_point_publish_{0};
  uint32_t last_area_publish_{0};

  ParseState parse_state_{ParseState::SOF};
  uint8_t header_pos_{0};
  uint8_t header_xor_{0};
  uint16_t data_len_{0};
  uint16_t frame_type_{0};
  uint16_t frame_id_{0};
  uint16_t data_pos_{0};
  uint8_t data_xor_{0};
  uint32_t discard_remaining_{0};
  size_t max_data_len_{0};
  bool max_data_len_overridden_{false};
  std::vector<uint8_t> data_buf_{};
  uint16_t next_frame_id_{0};

  static constexpr uint8_t CMD_QUEUE_SIZE = 16;
  static constexpr uint32_t CMD_ACK_TIMEOUT_MS = 300;
  static constexpr uint8_t CMD_MAX_RETRIES = 3;

  std::array<PendingCommand, CMD_QUEUE_SIZE> cmd_queue_{};
  uint8_t cmd_head_{0};
  uint8_t cmd_tail_{0};
  uint8_t cmd_count_{0};
  bool command_active_{false};
  bool command_sent_{false};
  PendingCommand active_command_{};
  uint16_t active_frame_id_{0};
  uint8_t retries_left_{0};
  uint32_t last_send_ms_{0};

  float z_min_{NAN};
  float z_max_{NAN};
  float area_x_min_{NAN};
  float area_x_max_{NAN};
  float area_y_min_{NAN};
  float area_y_max_{NAN};
  float area_z_min_{NAN};
  float area_z_max_{NAN};
  std::array<AreaConfig, AREA_COUNT> interference_area_values_{};
  std::array<AreaConfig, AREA_COUNT> detection_area_values_{};
  uint32_t hold_delay_seconds_{0};
  uint8_t area_id_{0xFF};
  bool area_id_set_{false};

  bool target_presence_any_{false};
  bool area_presence_any_{false};
  bool area_write_pending_{false};
  bool work_mode_reported_{false};
  bool low_power_enabled_{false};
  bool low_power_reported_{false};
  bool pending_area_apply_{false};
  uint8_t pending_area_id_{0xFF};
  AreaConfig pending_area_updates_{};
  bool last_work_mode_valid_{false};
  bool last_work_mode_low_power_{false};

  std::array<bool, MAX_TARGETS> last_target_presence_{};
  uint32_t last_target_count_{0xFFFFFFFF};
  uint32_t last_point_count_{0xFFFFFFFF};
  std::array<float, MAX_TARGETS> last_target_x_{{NAN, NAN, NAN}};
  std::array<float, MAX_TARGETS> last_target_y_{{NAN, NAN, NAN}};
  std::array<float, MAX_TARGETS> last_target_z_{{NAN, NAN, NAN}};
  std::array<float, MAX_TARGETS> last_target_dop_{{NAN, NAN, NAN}};
  std::array<float, MAX_TARGETS> last_target_cluster_{{NAN, NAN, NAN}};
};

}  // namespace esphome::ld6002b
