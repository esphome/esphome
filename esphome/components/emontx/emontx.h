#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::emontx {

/**
 * @class EmonTx
 * @brief Main class for the EmonTx component.
 *
 * The EmonTx processes incoming data frames via UART,
 * extracts tags and values, and publishes them to registered sensors.
 */
class EmonTx : public PollingComponent,
               public uart::UARTDevice
#ifdef USE_API
    ,
               public api::CustomAPIDevice
#endif
{
 public:
  EmonTx() = default;

  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;

  void add_on_json_callback(std::function<void(JsonObject, const std::string &)> &&callback) {
    this->json_callbacks_.add(std::move(callback));
  }

  void add_on_data_callback(std::function<void(const std::string &)> &&callback) {
    this->data_callbacks_.add(std::move(callback));
  }

  // Send command to emonTx via UART
  void send_command(const std::string &command);

  // Enable/disable config panel (auto-fires esphome.emontx_raw events)
  void set_config_panel(bool enabled) { this->config_panel_ = enabled; }

#ifdef USE_SENSOR
  void register_sensor(const char *tag_name, sensor::Sensor *sensor);
#endif

 protected:
#ifdef USE_SENSOR
  std::vector<std::pair<const char *, sensor::Sensor *>> sensors_{};
#endif
  std::string buffer_;

  enum class ParseState {
    OFF,
    WAITING_FOR_START,
  };
  ParseState state_{ParseState::OFF};

  void parse_json_(const std::string &data);

  // Service callback wrapper (register_service requires std::string by value)
  void on_send_command_service_(std::string command) { this->send_command(command); }  // NOLINT

  CallbackManager<void(JsonObject, const std::string &)> json_callbacks_;
  CallbackManager<void(const std::string &)> data_callbacks_;

  bool config_panel_{false};
};

// Action to send command to emonTx
template<typename... Ts> class EmonTxSendCommandAction : public Action<Ts...>, public Parented<EmonTx> {
 public:
  TEMPLATABLE_VALUE(std::string, command)

  void play(const Ts &...x) override { this->parent_->send_command(this->command_.value(x...)); }
};

}  // namespace esphome::emontx
