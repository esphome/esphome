#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/finite_set_mask.h"

#include <cmath>
#include <span>

namespace esphome::mitsubishi_cn105 {

uint32_t get_loop_time_ms();

class MitsubishiCN105 {
 public:
  enum class Mode : uint8_t {
    HEAT,
    DRY,
    COOL,
    FAN_ONLY,
    AUTO,
    UNKNOWN,
  };

  enum class FanMode : uint8_t {
    AUTO,
    QUIET,
    SPEED_1,
    SPEED_2,
    SPEED_3,
    SPEED_4,
    UNKNOWN,
  };

  enum class VaneMode : uint8_t {
    AUTO,
    POSITION_1,
    POSITION_2,
    POSITION_3,
    POSITION_4,
    POSITION_5,
    SWING,
    UNKNOWN,
  };

  // Positions are numbered from the far left (1) to the far right (5).
  enum class WideVaneMode : uint8_t {
    POSITION_1,  // far left
    POSITION_2,  // left
    POSITION_3,  // center
    POSITION_4,  // right
    POSITION_5,  // far right
    SPLIT,       // left and right
    SWING,
    UNKNOWN,
  };

  // Direction of a run in AUTO, which 0x06 does not report; the climate infers it.
  enum class AutoRun : uint8_t {
    AUTO_RUN_UNKNOWN,
    AUTO_RUN_HEATING,
    AUTO_RUN_COOLING,
  };

  struct Status {
    float target_temperature{NAN};
    float room_temperature{NAN};
    bool power_on{false};
    bool operating{false};
    Mode mode{Mode::UNKNOWN};
    FanMode fan_mode{FanMode::UNKNOWN};
    VaneMode vane_mode{VaneMode::UNKNOWN};
    WideVaneMode wide_vane_mode{WideVaneMode::UNKNOWN};
  };

  explicit MitsubishiCN105(uart::UARTDevice &device) : device_(device) {}

  void initialize();
  bool update();

  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }

  uint32_t get_telemetry_request_min_interval() const { return this->telemetry_request_min_interval_ms_; }
  bool is_telemetry_polling_enabled() const { return this->telemetry_request_min_interval_ms_ != SCHEDULER_DONT_RUN; }
  void set_telemetry_request_min_interval(uint32_t interval_ms) {
    this->telemetry_request_min_interval_ms_ = interval_ms;
  }

  const Status &status() const { return this->status_; }
  bool is_status_initialized() const {
    return this->is_telemetry_polling_enabled() ? !std::isnan(this->status_.room_temperature)
                                                : !std::isnan(this->status_.target_temperature);
  }
  bool is_temperature_encoding_b() const { return this->property_context_.use_temperature_encoding_b; }

  void set_power(bool power_on);
  void set_target_temperature(float target_temperature);
  void set_mode(Mode mode);
  void set_fan_mode(FanMode fan_mode);
  void set_vane_mode(VaneMode vane_mode);
  void set_wide_vane_mode(WideVaneMode mode);
  void set_remote_temperature(float temperature);
  void clear_remote_temperature();
  void set_auto_run(AutoRun auto_run) { this->auto_run_ = auto_run; }

 protected:
  enum class State : uint8_t {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    UPDATING_STATUS,
    STATUS_UPDATED,
    DEFERRED_STATUS_REQUEST,
    SCHEDULE_NEXT_STATUS_UPDATE,
    WAITING_FOR_SCHEDULED_STATUS_UPDATE,
    APPLYING_SETTINGS,
    SETTINGS_APPLIED,
    READ_TIMEOUT
  };

  class FrameParser {
   public:
    template<typename Callback> bool read_and_parse(uart::UARTDevice &device, Callback &&callback);
    void reset() { read_pos_ = 0; }
    static void dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len);

   protected:
    void reset_and_dump_buffer_(const char *prefix);

   private:
    static constexpr size_t READ_BUFFER_SIZE = 32;
    uint8_t read_buffer_[READ_BUFFER_SIZE];
    uint8_t read_pos_{0};
  };

  enum class PropertyId : uint8_t {
    TEMPERATURE = 0,
    POWER = 1,
    MODE = 2,
    FAN = 3,
    VANE = 4,
    WIDE_VANE = 5,
    REMOTE_TEMPERATURE = 6
  };

  struct UpdateFlags {
    void set(PropertyId id) { this->mask_.insert(id); }
    void clear(PropertyId id) { this->mask_.erase(id); }
    bool any() const { return !this->mask_.empty(); }
    bool contains(PropertyId id) const { return this->mask_.count(id); }
    bool contains_only(PropertyId id) const { return this->mask_.get_mask() == Mask{id}.get_mask(); }

   protected:
    using Mask =
        FiniteSetMask<PropertyId, DefaultBitPolicy<PropertyId, static_cast<int>(PropertyId::REMOTE_TEMPERATURE) + 1>>;
    Mask mask_;
  };

  // An early refresh reads 0x06 after a mode or power change; it is spent until the next regular telemetry read.
  enum class TelemetryRefresh : uint8_t {
    TELEMETRY_REFRESH_NONE,
    TELEMETRY_REFRESH_REQUESTED,
    TELEMETRY_REFRESH_SPENT,
  };

  struct PropertyContext {
    bool use_temperature_encoding_b : 1 {false};
    bool set_wide_vane_high_bit : 1 {false};
  };

  friend struct Property;

  void set_state_(State new_state);
  void did_transition_(State to);
  bool process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len);
  bool process_status_packet_(const uint8_t *payload, size_t len);
  bool parse_status_payload_(uint8_t msg_type, const uint8_t *payload, size_t len);
  void send_packet_(std::span<const uint8_t> packet);
  void update_status_();
  bool should_request_telemetry_() const;
  bool is_telemetry_interval_elapsed_() const;
  bool is_early_telemetry_refresh_() const;
  bool is_refresh_due_() const;
  void request_telemetry_refresh_();
  uint8_t next_status_msg_type_() const;
  bool take_status_changed_();
  bool is_operating_status_unsupported_() const {
    return this->operating_status_misses_ >= OPERATING_STATUS_MAX_MISSES;
  }
  void hold_if_run_ended_(bool was_on, Mode was_mode);
  void apply_settings_();
  bool has_timed_out_(uint32_t timeout) const { return ((get_loop_time_ms() - this->operation_start_ms_) >= timeout); }
  void set_remote_temperature_half_deg_(uint8_t temperature_half_deg);
  static bool should_transition(State from, State to);
  static const LogString *state_to_string(State state);

  uart::UARTDevice &device_;
  // Default 1s; legacy climate-owned hub compatibility relies on this when update_interval is omitted.
  // Remove legacy note in 2027.2.0.
  uint32_t update_interval_ms_{1000};
  uint32_t status_update_wait_credit_ms_{0};
  uint32_t operation_start_ms_{0};
  // Default 60s; legacy climate-owned hub compatibility relies on this when current_temperature_min_interval is
  // omitted. Remove legacy note in 2027.2.0.
  uint32_t telemetry_request_min_interval_ms_{60000};
  uint32_t last_telemetry_update_ms_{0};
  uint32_t wind_down_read_ms_{0};
  Status status_{};
  State state_{State::NOT_CONNECTED};
  UpdateFlags pending_updates_;
  PropertyContext property_context_;
  FrameParser frame_parser_;
  uint8_t current_status_msg_type_{0};

  static constexpr uint8_t REMOTE_TEMPERATURE_DISABLED = 0;
  uint8_t remote_temperature_half_deg_{REMOTE_TEMPERATURE_DISABLED};
  bool status_changed_ : 1 {false};
  bool has_telemetry_update_ : 1 {false};  // last_telemetry_update_ms_ is set
  TelemetryRefresh telemetry_refresh_ : 2 {TelemetryRefresh::TELEMETRY_REFRESH_NONE};
  // Consecutive 0x06 timeouts; 0x06 is no longer requested once it reaches the maximum.
  uint8_t operating_status_misses_ : 2 {0};
  static constexpr uint8_t OPERATING_STATUS_MAX_MISSES = 3;
  // Non-zero while a "running" 0x06 report is taken as wind-down from the previous mode or power state: one plus the
  // number of 0x06 reads since the hold started. The last read was at wind_down_read_ms_.
  uint8_t wind_down_reads_ : 4 {0};
  AutoRun auto_run_ : 2 {AutoRun::AUTO_RUN_UNKNOWN};
  static constexpr uint8_t WIND_DOWN_HELD_READS = 9;
};

}  // namespace esphome::mitsubishi_cn105
