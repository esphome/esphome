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

#include <map>
#include <vector>

namespace esphome {
namespace emontx {

// Add callback type definition for JSON callbacks
using EmonTxJsonCallback = std::function<void(JsonObject)>;

// Forward declaration
class EmonTx;

/*
 * 198 bytes should be enough to contain a full session in historical mode with
 * three phases. But go with 1024 just to be sure.
 */
/**
 * @class EmonTxListener
 * @brief Listener interface for receiving updates from the EmonTx.
 *
 * This class allows other components to register as listeners to receive updates
 * for specific tags published by the EmonTx.
 */
class EmonTxListener {
 public:
  std::string tag;
  virtual void publish_val(const std::string &val){};
};

// Define the enum for publish modes
enum class MqttPublishMode { JSON, INDIVIDUAL };

/**
 * @class EmonTx
 * @brief Main class for the EmonTx component.
 *
 * The EmonTx processes incoming data frames via UART, validates their CRC,
 * extracts tags and values, and publishes them to registered listeners.
 */
class EmonTx : public PollingComponent, public uart::UARTDevice {
 public:
  EmonTx() = default;

  void register_emontx_listener(EmonTxListener *listener);
  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;
  std::vector<EmonTxListener *> emontx_listeners_{};

  // Add method to register JSON callbacks
  void add_on_json_callback(const EmonTxJsonCallback &callback) { this->json_callbacks_.push_back(callback); }

  // Add method to register line callbacks (for config responses)
  using EmonTxLineCallback = std::function<void(const std::string &)>;
  void add_on_line_callback(const EmonTxLineCallback &callback) { this->line_callbacks_.push_back(callback); }

  // Add method to get the current buffer
  std::string get_buffer() const { return this->last_valid_json_; }

  // Send command to emonTx via UART
  void send_command(const std::string &command);

#ifdef USE_SENSOR
  void register_sensor(const std::string &tag_name, sensor::Sensor *sensor);
#endif

 protected:
#ifdef USE_SENSOR
  std::vector<std::pair<std::string, sensor::Sensor *>> sensors_{};
#endif
  std::string buffer_;
  std::string last_valid_json_;

  enum ParseState {
    OFF,
    WAITING_FOR_START,  // Waiting for '{' character
    COLLECTING_JSON,    // Collecting characters until '}'
    JSON_COLLECTED,     // Waiting for '\r' or '\n' after JSON
  } state_{OFF};

  bool read_chars_until_(bool drop, uint8_t c);

  void parse_json_(const std::string &data);
  void publish_value_(const std::string &tag, const std::string &val);

  // Add storage for JSON callbacks
  std::vector<EmonTxJsonCallback> json_callbacks_{};

  // Add storage for line callbacks (raw serial data)
  std::vector<EmonTxLineCallback> line_callbacks_{};
};

// Action to send command to emonTx
template<typename... Ts> class EmonTxSendCommandAction : public Action<Ts...>, public Parented<EmonTx> {
 public:
  TEMPLATABLE_VALUE(std::string, command)

  void play(Ts... x) override { this->parent_->send_command(this->command_.value(x...)); }
};

}  // namespace emontx
}  // namespace esphome
