#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/json/json_util.h"

#include <array>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
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
 public:
  EmonTx() = default;

  void loop() override;
  void setup() override;
  void dump_config() override;

  template<typename F> void add_on_json_callback(F &&callback) { this->json_callbacks_.add(std::forward<F>(callback)); }

  template<typename F> void add_on_data_callback(F &&callback) { this->data_callbacks_.add(std::forward<F>(callback)); }

  // Send command to emonTx via UART
  void send_command(const std::string &command);

  /**
   * Suspend or resume UART processing.
   *
   * When paused, loop() is removed from the scheduler and returns immediately
   * without consuming any bytes from the UART buffer. This lets another
   * component (e.g. a firmware updater) take exclusive ownership of a shared
   * UART bus while it is active. Must be called from the main loop task, like
   * any other component-to-component call.
   *
   * Pausing drops any partially-received line, since the bytes that would
   * complete it may be consumed by whichever component takes over the bus.
   */
  void set_paused(bool paused) {
    if (paused == this->is_idle())
      return;
    this->buffer_pos_ = 0;
    if (paused) {
      this->disable_loop();
    } else {
      this->enable_loop();
    }
  }

#ifdef USE_SENSOR
  void init_sensors(size_t count) { this->sensors_.init(count); }
  void register_sensor(const char *tag_name, sensor::Sensor *sensor);
#endif

 protected:
  void parse_json_(const char *data, size_t len);

#ifdef USE_SENSOR
  FixedVector<std::pair<const char *, sensor::Sensor *>> sensors_{};
#endif
  LazyCallbackManager<void(JsonObject, StringRef)> json_callbacks_;
  LazyCallbackManager<void(StringRef)> data_callbacks_;
  uint16_t buffer_pos_{0};
  std::array<char, MAX_LINE_LENGTH + 1> buffer_{};
};

// Action to send command to emonTx
template<typename... Ts> class EmonTxSendCommandAction final : public Action<Ts...>, public Parented<EmonTx> {
 public:
  TEMPLATABLE_VALUE(std::string, command)

  void play(const Ts &...x) override { this->parent_->send_command(this->command_.value(x...)); }
};

}  // namespace esphome::emontx
