#pragma once

// Shared base for the Hi-Link 60 GHz radars that speak the TinyFrame serial protocol: the LD6002B and
// the LD6004. The LD6001A uses AT commands and another report format and is a separate component.
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/gpio.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/ld600x/ld600x_frame.h"
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
#include <cmath>

namespace esphome::ld600x {

static constexpr uint8_t AREA_COUNT = 4;
// Interference areas own ids 0..AREA_COUNT-1 and detection areas the next four, so
// this is the whole id space TYPE_SET_AREA accepts.
static constexpr uint8_t AREA_ID_COUNT = AREA_COUNT * LD600X_AREA_KINDS;
static constexpr size_t DEFAULT_MAX_DATA_LEN = 1024;
static constexpr size_t DEFAULT_MAX_DATA_LEN_POINT_CLOUD = 4096;
// Largest protocol payload is TYPE_SET_AREA: int32 area id + 6 floats = 28 bytes.
static constexpr size_t CMD_MAX_DATA_LEN = 28;

static constexpr uint8_t LD600X_MODEL_KIND_BASE = 0x80;

enum AreaKind : uint8_t { AREA_KIND_INTERFERENCE, AREA_KIND_DETECTION, AREA_KIND_DWELL };
enum AreaAxis : uint8_t {
  AREA_AXIS_X_MIN,
  AREA_AXIS_X_MAX,
  AREA_AXIS_Y_MIN,
  AREA_AXIS_Y_MAX,
  AREA_AXIS_Z_MIN,
  AREA_AXIS_Z_MAX,
};

enum NumberType : uint8_t {
  NUMBER_HOLD_DELAY,
  NUMBER_Z_MIN,
  NUMBER_Z_MAX,
  NUMBER_LOW_POWER_SLEEP,
  NUMBER_AREA_X_MIN,
  NUMBER_AREA_X_MAX,
  NUMBER_AREA_Y_MIN,
  NUMBER_AREA_Y_MAX,
  NUMBER_AREA_Z_MIN,
  NUMBER_AREA_Z_MAX,
};

enum SelectType : uint8_t {
  SELECT_SENSITIVITY,
  SELECT_TRIGGER_SPEED,
  SELECT_INSTALLATION_MODE,
  SELECT_AREA_ID,
};

enum SwitchType : uint8_t {
  SWITCH_LOW_POWER,
  SWITCH_POINT_CLOUD,
  SWITCH_TARGET_DISPLAY,
};

enum ButtonType : uint8_t {
  BUTTON_APPLY_AREA,
  BUTTON_AUTO_INTERFERENCE,
  BUTTON_GET_AREAS,
  BUTTON_CLEAR_INTERFERENCE,
  BUTTON_RESET_DETECTION_AREA,
  BUTTON_GET_DELAY,
  BUTTON_GET_SENSITIVITY,
  BUTTON_GET_TRIGGER_SPEED,
  BUTTON_GET_Z_RANGE,
  BUTTON_GET_INSTALLATION,
  BUTTON_GET_LOW_POWER_MODE,
  BUTTON_GET_LOW_POWER_SLEEP_TIME,
  BUTTON_RESET_UNATTENDED,
  BUTTON_WAKE,
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
  char value[20];
};

class LD600XComponent : public Component, public uart::UARTDevice {
 public:
  // Both strings are literals that live in flash: the model name is printed by dump_config and the log
  // tag keeps the model's own name on every log line, so a logger `logs:` filter on `ld6002b` keeps working.
  LD600XComponent(const char *model_name, const char *log_tag) : model_name_(model_name), log_tag_(log_tag) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_wakeup_pin(GPIOPin *pin) { this->wakeup_pin_ = pin; }
  void set_wakeup_pulse_ms(uint32_t ms) { this->wakeup_pulse_ms_ = ms; }
  void set_auto_wake(bool enable) { this->auto_wake_ = enable; }

#ifdef USE_SENSOR
  void set_target_count_sensor(sensor::Sensor *sensor) { this->target_count_sensor_ = sensor; }
  void set_point_count_sensor(sensor::Sensor *sensor) { this->point_count_sensor_ = sensor; }

  void set_target_x_sensor(uint8_t target, sensor::Sensor *sensor) {
    if (target >= LD600X_MAX_TARGETS)
      return;
    this->targets_[target].x = sensor;
  }
  void set_target_y_sensor(uint8_t target, sensor::Sensor *sensor) {
    if (target >= LD600X_MAX_TARGETS)
      return;
    this->targets_[target].y = sensor;
  }
  void set_target_z_sensor(uint8_t target, sensor::Sensor *sensor) {
    if (target >= LD600X_MAX_TARGETS)
      return;
    this->targets_[target].z = sensor;
  }
  void set_target_dop_idx_sensor(uint8_t target, sensor::Sensor *sensor) {
    if (target >= LD600X_MAX_TARGETS)
      return;
    this->targets_[target].dop_idx = sensor;
  }
  void set_target_cluster_id_sensor(uint8_t target, sensor::Sensor *sensor) {
    if (target >= LD600X_MAX_TARGETS)
      return;
    this->targets_[target].cluster_id = sensor;
  }
  void set_area_sensor(uint8_t kind, uint8_t area, uint8_t axis, sensor::Sensor *sensor);
#endif

#ifdef USE_BINARY_SENSOR
  void set_presence_binary_sensor(binary_sensor::BinarySensor *sensor) { this->presence_binary_sensor_ = sensor; }
  void set_target_presence_binary_sensor(uint8_t target, binary_sensor::BinarySensor *sensor) {
    if (target >= LD600X_MAX_TARGETS)
      return;
    this->target_presence_[target] = sensor;
  }
  void set_area_presence_binary_sensor(uint8_t area, binary_sensor::BinarySensor *sensor) {
    if (area >= AREA_COUNT)
      return;
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

#ifdef USE_NUMBER
  virtual void set_number_value(uint8_t kind, float value);
#endif
#ifdef USE_SELECT
  virtual void set_select_value(uint8_t kind, size_t index);
#endif
#ifdef USE_SWITCH
  virtual void set_switch_state(uint8_t kind, bool state);
#endif
#ifdef USE_BUTTON
  virtual void press_button(uint8_t kind);
#endif

 protected:
  virtual bool handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) { return false; }
  // The base derives a two-state work mode from presence and low power for the text sensor, which
  // also makes that sensor a consumer of the target stream. A model whose firmware reports the mode
  // itself returns false here so neither the fallback nor the stream is switched on for it.
  virtual bool work_mode_uses_fallback() const { return true; }
  virtual void setup_model() {}
  virtual void dump_model_config() {}

  const char *model_name_;
  const char *log_tag_;

  struct PendingCommand {
    uint16_t type{0};
    uint8_t len{0};
    std::array<uint8_t, CMD_MAX_DATA_LEN> data{};
  };

  void handle_frame_(uint16_t type, const uint8_t *data, uint16_t len);
  void handle_target_report_(const uint8_t *data, uint16_t len);
  void handle_point_cloud_(const uint8_t *data, uint16_t len);
  void handle_area_presence_(const uint8_t *data, uint16_t len);
  void handle_area_report_(uint8_t kind, const uint8_t *data, uint16_t len);
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
  // Drops every target-derived reading and the slot table they are indexed by.
  void clear_target_state_();
  void clear_area_presence_();
  void restore_deferred_edits_();
  void publish_area_numbers_();
#ifdef USE_SENSOR
  void clear_target_slot_(uint8_t index);
#endif
#ifdef USE_NUMBER
  void publish_number_clamped_(number::Number *number, float value);
#endif
  void update_area_numbers_(const AreaConfig &area);
  void update_area_numbers_for_id_(uint8_t area_id);
  bool queue_area_config_(uint8_t area_id, const AreaConfig &desired);
  void try_apply_pending_area_(uint8_t reported_kind);
  void init_area_id_pref_();
  void save_area_id_pref_(uint8_t value);
  void init_version_pref_();
  void save_version_pref_(const char *value);

  // Returns whether the command was queued: it is dropped, with a log line, when
  // the payload is too long or the ring is full.
  bool queue_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void process_command_queue_();
  void send_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void send_command_internal_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void write_frame_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  // Returns whether the command reached the queue; see queue_command_.
  bool send_control_command_(uint32_t command);
  void send_z_range_();
  void apply_area_config_();
  void wake_();

#ifdef USE_SENSOR
  std::array<TargetSensors, LD600X_MAX_TARGETS> targets_{};
  sensor::Sensor *target_count_sensor_{nullptr};
  sensor::Sensor *point_count_sensor_{nullptr};
  std::array<AreaSensors, AREA_COUNT * LD600X_AREA_KINDS> areas_{};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *presence_binary_sensor_{nullptr};
  std::array<binary_sensor::BinarySensor *, LD600X_MAX_TARGETS> target_presence_{};
  std::array<binary_sensor::BinarySensor *, AREA_COUNT> area_presence_{};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *work_mode_text_sensor_{nullptr};
  text_sensor::TextSensor *ota_version_text_sensor_{nullptr};
  ESPPreferenceObject version_pref_{};
  bool version_pref_initialized_{false};
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
  ESPPreferenceObject area_id_pref_{};
  bool area_id_pref_initialized_{false};
#endif
#ifdef USE_SWITCH
  switch_::Switch *low_power_switch_{nullptr};
  switch_::Switch *point_cloud_switch_{nullptr};
  switch_::Switch *target_display_switch_{nullptr};
#endif

  GPIOPin *wakeup_pin_{nullptr};
  uint32_t wakeup_pulse_ms_{50};
  bool auto_wake_{true};

  ld600x::FrameParser parser_;
  size_t max_data_len_{0};
  uint8_t *data_buf_{nullptr};
  uint16_t next_frame_id_{0};

  // Sized for the two bursts that reach it, both counted as what is still queued
  // once the first command is dequeued: boot leaves 11 with every platform
  // configured, and pressing all fourteen buttons before an ack lands leaves 15.
  // Neither overflowed 16, but one free slot is not headroom, and overflowing is a
  // dropped command with only a log line to show for it.  Costs 256 bytes more per
  // configured instance, and this component is MULTI_CONF.
  static constexpr uint8_t CMD_QUEUE_SIZE = 24;
  static constexpr uint32_t CMD_ACK_TIMEOUT_MS = 300;
  // A sleeping module consumes the first frame to wake and answers only the one after it.
  static constexpr uint32_t CMD_FIRST_ACK_TIMEOUT_MS = 600;
  // How long the module stays awake after any frame, and so still answers the next one.
  static constexpr uint32_t MODULE_AWAKE_MS = 10000;
  static constexpr uint8_t CMD_MAX_RETRIES = 3;
  // Named so a repeated press replaces its own pending timeout instead of stacking
  // another, and so the command path can cancel it when it takes the pin over.
  static constexpr const char *WAKE_BUTTON_TIMEOUT = "wake_button";
  // Named so a burst of writes collapses to one read once they settle, rather than
  // one read per write.
  static constexpr const char *AREA_REFRESH_TIMEOUT = "area_refresh";
  // A reply cannot trail the frame that earned it for longer than this; the field worst case is ~726ms.
  static constexpr uint32_t STALE_ACK_MAX_AGE_MS = 1000;

  std::array<PendingCommand, CMD_QUEUE_SIZE> cmd_queue_{};
  uint8_t cmd_head_{0};
  uint8_t cmd_tail_{0};
  uint8_t cmd_count_{0};
  bool command_active_{false};
  bool command_sent_{false};
  PendingCommand active_command_{};
  // Frame a pending wake pulse will write, snapshotted because active_command_ may move on first.
  std::array<uint8_t, CMD_MAX_DATA_LEN> wake_scratch_{};
  bool wake_pulse_pending_{false};
  uint8_t retries_left_{0};
  uint32_t last_send_ms_{0};
  // Last frame seen in either direction; any traffic keeps the module awake.
  uint32_t last_traffic_ms_{0};
  // Frames transmitted for the command in flight, including retries; drives the retry budget.
  uint8_t attempts_sent_{0};
  // Subset of those the module can actually answer: a frame that woke it is consumed, not replied to.
  uint8_t acks_expected_{0};
  // ACKs still owed for superseded attempts; they carry no id, only their arrival order.
  uint16_t stale_ack_type_{0};
  uint8_t stale_ack_count_{0};
  // When that debt was booked, so a debt no reply can still settle expires instead of eating a live ACK.
  uint32_t stale_ack_ms_{0};
  // Bumped whenever the active command changes, so a deferred send can tell it was retired.
  uint8_t send_generation_{0};

  float z_min_{NAN};
  float z_max_{NAN};
  float area_x_min_{NAN};
  float area_x_max_{NAN};
  float area_y_min_{NAN};
  float area_y_max_{NAN};
  float area_z_min_{NAN};
  float area_z_max_{NAN};
  // What the user has typed and not yet applied; NaN per axis means "nothing of
  // mine here, take the module's value".  Same sentinel shape as
  // pending_area_updates_.  Exactly two things empty it: the area_id select moving
  // to another area, and an apply that was accepted.  A write the bounds guard
  // refused leaves it alone, and a deferred apply that had to be dropped hands its
  // staged values back here -- but only while the user is still on the area they
  // were staged for.  Either way the values stay the user's to fix.
  AreaConfig area_edits_{};
  std::array<AreaConfig, AREA_COUNT * LD600X_AREA_KINDS> area_values_{};
  uint8_t area_id_{0xFF};
  bool area_id_set_{false};

  // Which person owns each target_N slot, so a slot survives the module re-sorting its array.
  std::array<int32_t, LD600X_MAX_TARGETS> slot_cluster_{};
  std::array<bool, LD600X_MAX_TARGETS> slot_occupied_{};

  bool target_presence_any_{false};
  // What the switches and setup asked the module for, which is not the same as
  // what it is doing yet: a stream keeps sending until it acts on the command.
  // The report handlers read these and drop anything a stopped stream still emits.
  bool target_display_enabled_{false};
  bool point_cloud_enabled_{false};
  bool area_presence_any_{false};
  bool area_write_in_flight_{false};
  bool work_mode_reported_{false};
  bool low_power_enabled_{false};
  bool low_power_reported_{false};
  bool deferred_apply_pending_{false};
  uint8_t pending_area_id_{0xFF};
  AreaConfig pending_area_updates_{};
  bool last_work_mode_valid_{false};
  bool last_work_mode_low_power_{false};

#ifdef USE_SENSOR
  std::array<bool, LD600X_MAX_TARGETS> last_target_presence_{};  // one-shot NAN clear for target sensors
  // A cluster id names a person, so like the counts it is published on change, not per frame.
  std::array<int32_t, LD600X_MAX_TARGETS> last_cluster_id_{};
  std::array<bool, LD600X_MAX_TARGETS> last_cluster_id_valid_{};
  uint32_t last_target_count_{0xFFFFFFFF};
  uint32_t last_point_count_{0xFFFFFFFF};
#endif
};

}  // namespace esphome::ld600x
