#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>

namespace esphome {
namespace mitsubishi_cn105 {

enum class ClimateMode : uint8_t {
  HEAT,
  DRY,
  COOL,
  FAN_ONLY,
  AUTO,
  UNKNOWN,
};

enum class ClimateFanMode : uint8_t {
  AUTO,
  QUIET,
  SPEED_1,
  SPEED_2,
  SPEED_3,
  SPEED_4,
  UNKNOWN,
};

enum class ClimateVaneMode : uint8_t {
  AUTO,        // AUTO
  POSITION_1,  // 1
  POSITION_2,  // 2
  POSITION_3,  // 3
  POSITION_4,  // 4
  POSITION_5,  // 5
  SWING,       // SWING
  UNKNOWN,
};

struct ClimateSettings {
  bool operator==(const ClimateSettings &) const = default;

  bool power_on{false};
  float target_temperature{NAN};
  ClimateMode mode{ClimateMode::UNKNOWN};
  ClimateFanMode fan_mode{ClimateFanMode::UNKNOWN};
  ClimateVaneMode vane{ClimateVaneMode::UNKNOWN};
};

struct ClimateStatus {
  bool operator==(const ClimateStatus &) const = default;

  ClimateSettings settings{};
  float room_temperature{NAN};
};

class MitsubishiCN105Transport {
 public:
  virtual ~MitsubishiCN105Transport() = default;

  virtual size_t available() = 0;
  virtual void flush() = 0;
  virtual bool read_byte(uint8_t *data) = 0;
  virtual void write_array(const uint8_t *data, size_t len) = 0;
};

class MitsubishiCN105 {
 public:
  explicit MitsubishiCN105(MitsubishiCN105Transport &transport) : transport_(transport) {}

  void init();
  bool sync();

  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }

  const ClimateStatus &status() const { return this->current_status_; }
  bool is_status_initialized() const { return this->status_initialized_; }

  void set_target_temperature(float target_temperature);
  void set_power(bool power_on);
  void set_mode(ClimateMode mode);
  void set_fan_mode(ClimateFanMode fan_mode);
  void set_vane(ClimateVaneMode vane);

  void set_connection_state_callback(std::function<void(bool)> &&callback);

 private:
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

  enum class UpdateFlag : uint8_t {
    TEMPERATURE = 1 << 0,
    POWER = 1 << 1,
    MODE = 1 << 2,
    FAN = 1 << 3,
    VANE = 1 << 4,
  };

  struct UpdateFlags {
    void set(UpdateFlag f) { flags_ |= static_cast<uint8_t>(f); }
    void clear() { flags_ = 0; }
    bool any() const { return flags_ != 0; }
    bool has(UpdateFlag f) const { return (flags_ & static_cast<uint8_t>(f)) != 0; }

   private:
    uint8_t flags_{0};
  };

 private:
  void set_state_(State new_state);
  static bool should_transition_(State from, State to);
  void did_transition_(State from, State to);
  void cancel_waiting_and_transit_to_(State state);

  void connect_();

  void send_packet_(const uint8_t *packet, size_t size);
  void response_received_();
  void update_status_();

  bool read_incoming_bytes_();
  void add_byte_to_read_buffer_(uint8_t value);
  bool process_incoming_packet_(const uint8_t *packet, uint8_t length, uint8_t received_checksum);
  bool parse_values_(const uint8_t *data, size_t length);
  static std::optional<State> check_incoming_packet_(const uint8_t *packet, uint8_t length, uint8_t received_checksum);

  void apply_settings_();

  template<size_t N> void send_packet_(const uint8_t (&packet)[N]) { this->send_packet_(packet, N); }

  template<typename T, size_t N>
  std::optional<uint8_t> pending_update_for_(UpdateFlag flag, const std::array<std::optional<T>, N> &map,
                                             T value) const;

  template<typename T, size_t N, typename F>
  void apply_to_(UpdateFlag flag, const std::array<std::optional<T>, N> &table, uint8_t value, F &&callback) const;

  static void dump_buffer_vv_(const char *prefix, const uint8_t *data, size_t len);
  static const char *state_to_string_(State state);
  static const char *update_flag_to_string_(UpdateFlag flag);

 private:
  MitsubishiCN105Transport &transport_;

  State state_{State::NOT_CONNECTED};
  ClimateStatus current_status_;
  UpdateFlags pending_updates_;

  static constexpr size_t READ_BUFFER_SIZE = 32;
  uint8_t read_buffer_[READ_BUFFER_SIZE];
  uint8_t read_pos_{0};

  bool status_initialized_{false};
  uint8_t info_mode_index_{0};
  uint32_t update_interval_ms_{1000};
  bool temp_mode_{false};

  std::optional<uint32_t> status_update_start_ms_;
  std::optional<uint32_t> write_timeout_start_ms_;

  std::function<void(bool)> connection_state_callback_{};
};

}  // namespace mitsubishi_cn105
}  // namespace esphome
