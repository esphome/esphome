#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/json/json_util.h"

#include <array>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::emontx {

static constexpr size_t MAX_LINE_LENGTH = 1024;

/**
 * @class EmonTx
 * @brief Main class for the EmonTx component.
 *
 * The EmonTx processes incoming data frames via UART,
 * extracts tags and values, and publishes them to registered sensors.
 */
class EmonTx : public Component, public uart::UARTDevice {
 public:
  EmonTx() = default;

  void loop() override;
  void setup() override;
  void dump_config() override;

  void add_on_json_callback(std::function<void(JsonObject, const std::string &)> &&callback) {
    this->json_callbacks_.add(std::move(callback));
  }

  void add_on_data_callback(std::function<void(const std::string &)> &&callback) {
    this->data_callbacks_.add(std::move(callback));
  }

  // Send command to emonTx via UART
  void send_command(const std::string &command);

#ifdef USE_SENSOR
  void init_sensors(size_t count) { this->sensors_.init(count); }
  void register_sensor(const char *tag_name, sensor::Sensor *sensor);
#endif

 protected:
  void parse_json_(const std::string &data);

#ifdef USE_SENSOR
  FixedVector<std::pair<const char *, sensor::Sensor *>> sensors_{};
#endif
  LazyCallbackManager<void(JsonObject, const std::string &)> json_callbacks_;
  LazyCallbackManager<void(const std::string &)> data_callbacks_;
  size_t buffer_pos_{0};
  std::array<char, MAX_LINE_LENGTH> buffer_{};
};

// Action to send command to emonTx
template<typename... Ts> class EmonTxSendCommandAction : public Action<Ts...>, public Parented<EmonTx> {
 public:
  TEMPLATABLE_VALUE(std::string, command)

  void play(const Ts &...x) override { this->parent_->send_command(this->command_.value(x...)); }
};

}  // namespace esphome::emontx
