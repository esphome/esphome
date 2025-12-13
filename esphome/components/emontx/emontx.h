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

// Conditionally include web server for config interface
#ifdef USE_EMONTX_WEB_CONFIG
#include "esphome/components/web_server_base/web_server_base.h"
#ifdef USE_ESP32
#include <esp_http_server.h>
#ifdef USE_ARDUINO
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#else
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#endif
#endif
#endif

#include <map>
#include <vector>

namespace esphome {
namespace emontx {

// Add callback type definition for JSON callbacks
using EmonTxJsonCallback = std::function<void(JsonObject)>;

// Forward declaration
class EmonTx;

#ifdef USE_EMONTX_WEB_CONFIG
// Handler for /emontx/config page
class EmonTxConfigHandler : public AsyncWebHandler {
 public:
  EmonTxConfigHandler(EmonTx *emontx) : emontx_(emontx) {}
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  EmonTx *emontx_;
};

// Handler for /emontx/send POST requests
class EmonTxSendHandler : public AsyncWebHandler {
 public:
  EmonTxSendHandler(EmonTx *emontx) : emontx_(emontx) {}
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;

 protected:
  EmonTx *emontx_;
};

// Handler for /emontx/data - returns last received JSON (polling approach)
class EmonTxDataHandler : public AsyncWebHandler {
 public:
  EmonTxDataHandler(EmonTx *emontx) : emontx_(emontx) {}
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  EmonTx *emontx_;
};
#endif  // USE_EMONTX_WEB_CONFIG

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

  // Add method to get the current buffer
  std::string get_buffer() const { return this->last_valid_json_; }

  // Send command to emonTx via UART
  void send_command(const std::string &command);

#ifdef USE_SENSOR
  void register_sensor(const std::string &tag_name, sensor::Sensor *sensor);
#endif

#ifdef USE_EMONTX_WEB_CONFIG
  void set_web_server(web_server_base::WebServerBase *web_server) { this->web_server_ = web_server; }

  // Web interface methods
  void serve_config_page(AsyncWebServerRequest *request);
  void handle_serial_send(const std::string &data);
  std::string get_last_json() const { return this->last_valid_json_; }
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

#ifdef USE_EMONTX_WEB_CONFIG
  web_server_base::WebServerBase *web_server_{nullptr};
  std::string cached_html_;
  bool html_fetched_{false};

  void fetch_oem_html_();
  std::string patch_html_for_polling_(const std::string &html);
#endif
};

// Action to send command to emonTx
template<typename... Ts> class EmonTxSendCommandAction : public Action<Ts...>, public Parented<EmonTx> {
 public:
  TEMPLATABLE_VALUE(std::string, command)

  void play(Ts... x) override { this->parent_->send_command(this->command_.value(x...)); }
};

}  // namespace emontx
}  // namespace esphome
