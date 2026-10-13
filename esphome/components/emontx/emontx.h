#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/json/json_util.h"

#include <array>
#include <atomic>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_EMONTX_VERSION
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::emontx {

/// Maximum line length in bytes (plus one byte reserved for null terminator)
static constexpr size_t MAX_LINE_LENGTH = 1024;

/**
 * @class EmonTx
 * @brief Main class for the EmonTx component.
 *
 * The EmonTx processes incoming data frames via UART,
 * extracts tags and values, and publishes them to registered sensors.
 */
class EmonTx final : public Component, public uart::UARTDevice {
#ifdef USE_EMONTX_VERSION
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
  /// as a command, and unlock it in setup(). Needs emonTx firmware v1.2.0 or later.
  void set_lock_on_reboot(bool lock_on_reboot) { this->lock_on_reboot_ = lock_on_reboot; }

#ifdef USE_SENSOR
  void init_sensors(size_t count) { this->sensors_.init(count); }
  void register_sensor(const char *tag_name, sensor::Sensor *sensor);
#endif

 protected:
  void parse_json_(const char *data, size_t len);
#ifdef USE_EMONTX_VERSION
  void request_version_();
  void parse_version_line_(const char *line, size_t len);
#endif

#ifdef USE_SENSOR
  FixedVector<std::pair<const char *, sensor::Sensor *>> sensors_{};
#endif
  LazyCallbackManager<void(JsonObject, StringRef)> json_callbacks_;
  LazyCallbackManager<void(StringRef)> data_callbacks_;
  uint16_t buffer_pos_{0};
  std::atomic<bool> paused_{false};
  bool was_paused_{false};
  bool lock_on_reboot_{false};
  bool discarding_line_{false};  // Set after an overflow or a pause until the end of that line
#ifdef USE_EMONTX_VERSION
  uint32_t last_msg_number_{0};
#endif
  std::array<char, MAX_LINE_LENGTH + 1> buffer_{};
};

}  // namespace esphome::emontx
