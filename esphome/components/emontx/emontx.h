#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/line_reader.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/json/json_util.h"

#include <atomic>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_EMONTX_VERSION_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::emontx {

/// Maximum line length in characters
static constexpr size_t MAX_LINE_LENGTH = 1024;

enum class LockOnReboot : uint8_t {
  LOCK_ON_REBOOT_NEVER,
  LOCK_ON_REBOOT_ALWAYS,
  LOCK_ON_REBOOT_AUTO,  // Only when the emonTx reports firmware v1.2.0 or later
};

/**
 * @class EmonTx
 * @brief Main class for the EmonTx component.
 *
 * The EmonTx processes incoming data frames via UART,
 * extracts tags and values, and publishes them to registered sensors.
 */
class EmonTx final : public Component, public uart::UARTDevice {
#ifdef USE_EMONTX_VERSION_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
#endif
 public:
  EmonTx() = default;

  void loop() override;
  void setup() override;
  void dump_config() override;
  void on_shutdown() override;

  template<typename F> void add_on_json_callback(F &&callback) { this->json_callbacks_.add(std::forward<F>(callback)); }

  template<typename F> void add_on_data_callback(F &&callback) { this->data_callbacks_.add(std::forward<F>(callback)); }

  // Send command to emonTx via UART
  void send_command(const char *command);
  void send_command(const std::string &command) { this->send_command(command.c_str()); }

  /// Stop reading the UART and sending commands, so another component can use it.
  /// Atomic, so it can be called from any task. Hold the pause for at least one loop() pass.
  void set_paused(bool paused) { this->paused_.store(paused, std::memory_order_relaxed); }

  bool is_paused() const { return this->paused_.load(std::memory_order_relaxed); }

  /// Lock the emonTx ('emonlock') before a reboot, so serial garbage during the reboot is not taken
  /// as a command, and unlock it after boot. Needs emonTx firmware v1.2.0 or later, which AUTO checks.
  void set_lock_on_reboot(LockOnReboot lock_on_reboot) { this->lock_on_reboot_ = lock_on_reboot; }

#ifdef USE_SENSOR
  void init_sensors(size_t count) { this->sensors_.init(count); }
  void register_sensor(const char *tag_name, sensor::Sensor *sensor);
#endif

 protected:
  void parse_json_(const char *data, size_t len);
  void unlock_();
#ifdef USE_EMONTX_VERSION
  bool wants_version_() const {
#ifdef USE_EMONTX_VERSION_TEXT_SENSOR
    if (this->version_text_sensor_ != nullptr)
      return true;
#endif
    return this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_AUTO;
  }
  void request_version_();
  void send_version_request_();
  void parse_msg_number_(const char *line, size_t len);
  void parse_version_line_(const char *line, size_t len);
#endif

#ifdef USE_SENSOR
  FixedVector<std::pair<const char *, sensor::Sensor *>> sensors_{};
#endif
  LazyCallbackManager<void(JsonObject, StringRef)> json_callbacks_;
  LazyCallbackManager<void(StringRef)> data_callbacks_;
  std::atomic<bool> paused_{false};
  bool was_paused_{false};
  LockOnReboot lock_on_reboot_{LockOnReboot::LOCK_ON_REBOOT_NEVER};
#ifdef USE_EMONTX_VERSION
  bool lock_supported_{false};      // The last version reported is v1.2.0 or later
  bool unlock_sent_{false};         // AUTO has sent 'emonunlock' since boot
  uint8_t version_frames_left_{0};  // Data lines still to wait for a version reply; 0 when none is owed
  uint32_t last_msg_number_{0};
#endif
  LineReader<MAX_LINE_LENGTH + 1> line_reader_;
};

}  // namespace esphome::emontx
