#pragma once

#include <optional>
#include "esphome/components/uart/uart.h"
#include "esphome/core/finite_set_mask.h"

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

  enum class WideVaneMode : uint8_t {
    FAR_LEFT,
    LEFT,
    CENTER,
    RIGHT,
    FAR_RIGHT,
    LEFT_RIGHT,
    SWING,
    UNKNOWN,
  };

  struct Status {
    float target_temperature{NAN};
    float room_temperature{NAN};
    bool power_on{false};
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

  uint32_t get_room_temperature_min_interval() const { return this->room_temperature_min_interval_ms_; }
  bool is_room_temperature_enabled() const { return this->room_temperature_min_interval_ms_ != SCHEDULER_DONT_RUN; }
  void set_room_temperature_min_interval(uint32_t interval_ms) {
    this->room_temperature_min_interval_ms_ = interval_ms;
  }

  const Status &status() const { return this->status_; }
  bool is_status_initialized() const {
    return this->is_room_temperature_enabled() ? !std::isnan(this->status_.room_temperature)
                                               : !std::isnan(this->status_.target_temperature);
  }

  void set_power(bool power_on);
  void set_target_temperature(float target_temperature);
  void set_mode(Mode mode);
  void set_fan_mode(FanMode fan_mode);
  void set_vane_mode(VaneMode vane_mode);
  void set_wide_vane_mode(WideVaneMode mode);
  void set_remote_temperature(float temperature);
  void clear_remote_temperature();

 protected:
  enum class State : uint8_t {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    UPDATING_STATUS,
    STATUS_UPDATED,
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

  enum class UpdateFlag : uint8_t {
    TEMPERATURE = 0,
    POWER = 1,
    MODE = 2,
    FAN = 3,
    VANE = 4,
    WIDE_VANE = 5,
    REMOTE_TEMPERATURE = 6,
  };

  struct UpdateFlags {
    template<typename... Flags> void set(Flags... flags) { (this->mask_.insert(flags), ...); }
    template<typename... Flags> void clear(Flags... flags) { (this->mask_.erase(flags), ...); }
    bool any() const { return !this->mask_.empty(); }
    bool contains(UpdateFlag flag) const { return this->mask_.count(flag); }
    bool contains_only(UpdateFlag flag) const { return this->mask_.get_mask() == Mask{flag}.get_mask(); }

   protected:
    using Mask =
        FiniteSetMask<UpdateFlag, DefaultBitPolicy<UpdateFlag, static_cast<int>(UpdateFlag::REMOTE_TEMPERATURE) + 1>>;

    Mask mask_;
  };

  void set_state_(State new_state);
  void did_transition_(State to);
  bool process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len);
  bool process_status_packet_(const uint8_t *payload, size_t len);
  bool parse_status_payload_(uint8_t msg_type, const uint8_t *payload, size_t len);
  bool parse_status_settings_(const uint8_t *payload, size_t len);
  bool parse_status_room_temperature_(const uint8_t *payload, size_t len);
  void send_packet_(const uint8_t *packet, size_t len);
  void update_status_();
  bool should_request_room_temperature_() const;
  void apply_settings_();
  bool has_timed_out_(uint32_t timeout) const { return ((get_loop_time_ms() - this->operation_start_ms_) >= timeout); }
  void set_remote_temperature_half_deg_(uint8_t temperature_half_deg);
  template<typename T> void send_packet_(const T &packet) { this->send_packet_(packet.data(), packet.size()); }
  static bool should_transition(State from, State to);
  static const LogString *state_to_string(State state);

  uart::UARTDevice &device_;
  uint32_t update_interval_ms_{1000};
  uint32_t status_update_wait_credit_ms_{0};
  uint32_t operation_start_ms_{0};
  uint32_t room_temperature_min_interval_ms_{60000};
  std::optional<uint32_t> last_room_temperature_update_ms_;
  Status status_{};
  State state_{State::NOT_CONNECTED};
  UpdateFlags pending_updates_;
  bool use_temperature_encoding_b_{false};
  bool set_wide_vane_high_bit_{false};
  FrameParser frame_parser_;
  uint8_t current_status_msg_type_{0};

  static constexpr uint8_t REMOTE_TEMPERATURE_DISABLED = 0;
  uint8_t remote_temperature_half_deg_{REMOTE_TEMPERATURE_DISABLED};
};

}  // namespace esphome::mitsubishi_cn105
