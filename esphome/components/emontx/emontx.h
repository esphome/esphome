#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif

// Conditionally include sensor
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <vector>

namespace esphome {
namespace emontx {

// Add callback type definition for JSON callbacks
using EmonTxJsonCallback = std::function<void(JsonObject, const std::string &)>;

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

  // Add method to register JSON callbacks
  void add_on_json_callback(const EmonTxJsonCallback &callback) { this->json_callbacks_.push_back(callback); }

  // Add method to register data callbacks (for all serial data)
  using EmonTxDataCallback = std::function<void(const std::string &)>;
  void add_on_data_callback(const EmonTxDataCallback &callback) { this->data_callbacks_.push_back(callback); }

  // Send command to emonTx via UART
  void send_command(std::string command);

  // Enable/disable config panel (auto-fires esphome.emontx_raw events)
  void set_config_panel(bool enabled) { this->config_panel_ = enabled; }

#ifdef USE_SENSOR
  void register_sensor(const std::string &tag_name, sensor::Sensor *sensor);
#endif

 protected:
#ifdef USE_SENSOR
  std::vector<std::pair<std::string, sensor::Sensor *>> sensors_{};
#endif
  std::string buffer_;

  enum class ParseState {
    OFF,
    WAITING_FOR_START,
  };
  ParseState state_{ParseState::OFF};

  void parse_json_(const std::string &data);

  // Add storage for JSON callbacks
  std::vector<EmonTxJsonCallback> json_callbacks_{};

  // Add storage for line callbacks (raw serial data)
  std::vector<EmonTxDataCallback> data_callbacks_{};

  // Config panel enabled flag
  bool config_panel_{false};
};

// Action to send command to emonTx
template<typename... Ts> class EmonTxSendCommandAction : public Action<Ts...>, public Parented<EmonTx> {
 public:
  TEMPLATABLE_VALUE(std::string, command)

  void play(const Ts &...x) override { this->parent_->send_command(this->command_.value(x...)); }
};

}  // namespace emontx
}  // namespace esphome
