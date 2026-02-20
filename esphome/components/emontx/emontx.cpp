#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/homeassistant_service.h"
#endif

namespace esphome::emontx {

static const char *const TAG = "emontx";

/**
 * @brief Initializes the EmonTx component.
 *
 * @details Sets up the initial state of the component by:
 * 1. Setting the state machine to OFF
 * 2. Pre-allocating buffer memory to avoid reallocation overhead
 * 3. Registering the send_command service (when config_panel is enabled)
 *
 * This method is called once during device startup. After setup completes,
 * the component will wait for update() to be called before starting to
 * process any incoming data.
 */
void EmonTx::setup() {
  this->state_ = ParseState::OFF;

  // Pre-allocate buffer to maximum size to prevent reallocation overhead
  // during JSON message collection.
  // 198 bytes should be enough to contain a full session in historical mode with
  // three phases. But go with 1024 just to be sure.
  this->buffer_.reserve(1024);

#ifdef USE_API_CUSTOM_SERVICES
  // Auto-register send_command service when config_panel is enabled
  // Uses a lambda wrapper because register_service requires std::string by value
  if (this->config_panel_) {
    this->register_service(&EmonTx::on_send_command_service_, "send_command", {"command"});
  }
#endif
}

/**
 * @brief Activates the state machine for continuous JSON data processing.
 *
 * @details This method is called periodically according to the update_interval configured
 * in the YAML. It activates the component if it's in OFF state (initial startup or after
 * component.suspend).
 *
 * Once activated, the state machine runs continuously in loop(), processing all incoming
 * JSON objects without waiting for subsequent update() calls. This prevents data loss
 * when multiple JSON messages arrive between polling intervals.
 *
 * The update_interval serves as a heartbeat/watchdog rather than controlling data processing.
 */
void EmonTx::update() {
  ESP_LOGD(TAG, "Updating EmonTx state...");

  if (this->state_ == ParseState::OFF) {
    this->buffer_.clear();
    this->state_ = ParseState::WAITING_FOR_START;
    ESP_LOGD(TAG, "EmonTx activated and ready to receive data.");
  } else {
    ESP_LOGV(TAG, "EmonTx already active (state: %d)", static_cast<int>(this->state_));
  }
}

/**
 * @brief Implements the main loop for parsing data from the serial port.
 *
 * @details The loop continuously processes incoming UART data line-by-line:
 * - OFF: Component is inactive, waiting for update() to activate it.
 * - WAITING_FOR_START: Component is active, reading and processing serial lines.
 *
 * Each line received is processed as follows:
 * 1. Fire esphome.emontx_raw event (when config_panel is enabled)
 * 2. Fire on_data callbacks for all received lines
 * 3. If line starts with '{', parse as JSON and update sensors/listeners
 *
 * This continuous processing ensures no data is lost when multiple messages
 * arrive in quick succession between polling intervals.
 */
void EmonTx::loop() {
  if (this->state_ == ParseState::OFF) {
    return;
  }

  // Read all available data to prevent UART buffer overflow
  // No artificial limit - drain the hardware buffer completely each loop
  while (this->available() > 0) {
    uint8_t received = this->read();

    if (received == '\r') {
      continue;  // Ignore CR
    } else if (received == '\n') {
      // End of line - process the buffer
      if (!this->buffer_.empty()) {
        // Use static string to avoid repeated allocations
        // Reserve same capacity as buffer_ to maintain allocation across swaps
        static std::string line = []() {
          std::string s;
          s.reserve(1024);
          return s;
        }();
        // Swap pointers with buffer_ (O(1), zero copy, both reuse allocations)
        line.swap(this->buffer_);
        // Clear buffer_ for next line (it now contains old line data from previous iteration)
        this->buffer_.clear();

        ESP_LOGD(TAG, "Received line: %s", line.c_str());

#ifdef USE_API_HOMEASSISTANT_SERVICES
        // Fire esphome.emontx_raw event for config panel
        if (this->config_panel_) {
          if (api::global_api_server == nullptr) {
            ESP_LOGW(TAG, "Cannot fire event: api_server is null");
          } else if (!api::global_api_server->is_connected()) {
            ESP_LOGV(TAG, "Cannot fire event: api_server not connected");
          } else {
            static constexpr auto SERVICE_EMONTX_RAW = StringRef::from_lit("esphome.emontx_raw");
            static constexpr auto DEVICE_ID_KEY = StringRef::from_lit("device_id");
            static constexpr auto LINE_KEY = StringRef::from_lit("line");

            api::HomeassistantActionRequest resp;

            resp.service = SERVICE_EMONTX_RAW;
            resp.is_event = true;

            resp.data.init(2);

            auto &kv1 = resp.data.emplace_back();
            kv1.key = DEVICE_ID_KEY;
            kv1.value = StringRef(App.get_name());

            auto &kv2 = resp.data.emplace_back();
            kv2.key = LINE_KEY;
            kv2.value = StringRef(line);

            api::global_api_server->send_homeassistant_action(resp);
            ESP_LOGV(TAG, "Fired esphome.emontx_raw event");
          }
        }
#endif

        // Fire data callbacks for all received lines
        this->data_callbacks_.call(line);

        // Check if this line is JSON (starts with '{')
        if (!line.empty() && line[0] == '{') {
          ESP_LOGV(TAG, "Line is JSON, parsing...");
          this->parse_json_(line);
        }
      }
    } else {
      // Regular character - add to buffer
      if (this->buffer_.length() >= 1024) {
        ESP_LOGW(TAG, "Buffer overflow (>1024 bytes), discarding buffer");
        this->buffer_.clear();
      } else {
        this->buffer_ += static_cast<char>(received);
      }
    }
  }
}

/**
 * @brief Parses a JSON string and updates associated sensors.
 *
 * @details This method takes a string containing JSON data and attempts to parse it.
 * If parsing is successful, it performs the following operations:
 * 1. Updates all registered sensors that have matching keys in the JSON
 * 2. Fires Home Assistant events with the received data (when config_panel is enabled)
 * 3. Executes all registered JSON callbacks, passing the parsed JsonObject
 *
 * @param data The JSON string to parse
 */
void EmonTx::parse_json_(const std::string &data) {
  ESP_LOGV(TAG, "Parsing JSON: %s", data.c_str());

  bool success = json::parse_json(data, [this, &data](JsonObject root) {
#ifdef USE_SENSOR
    // Update all registered sensors
    for (auto &sensor_pair : this->sensors_) {
      const char *tag = sensor_pair.first;
      sensor::Sensor *sensor_ptr = sensor_pair.second;

      if (root[tag].is<JsonVariant>()) {
        float value = root[tag];
        ESP_LOGV(TAG, "Updating sensor '%s' with value: %.2f", tag, value);
        sensor_ptr->publish_state(value);
      }
    }
#endif

#ifdef USE_API_HOMEASSISTANT_SERVICES
    // Fire Home Assistant event with the received data
    if (this->config_panel_ && api::global_api_server != nullptr && api::global_api_server->is_connected()) {
      static constexpr auto SERVICE_EMONTX_JSON = StringRef::from_lit("esphome.emontx_json");
      static constexpr auto DATA_KEY = StringRef::from_lit("data");

      api::HomeassistantActionRequest resp;

      resp.service = SERVICE_EMONTX_JSON;
      resp.is_event = true;

      resp.data.init(1);

      auto &kv = resp.data.emplace_back();
      kv.key = DATA_KEY;
      kv.value = StringRef(data);

      api::global_api_server->send_homeassistant_action(resp);
      ESP_LOGV(TAG, "Fired esphome.emontx_json event");
    }
#endif

    // Execute all registered JSON callbacks
    this->json_callbacks_.call(root, data);

    return true;
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON");
  }
}

/**
 * @brief Logs the EmonTx component configuration details.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx:");
  ESP_LOGCONFIG(TAG, "  Config panel: %s", this->config_panel_ ? "ENABLED" : "DISABLED");

#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Registered sensors: %zu", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "    Sensor: %s", sensor_pair.first);
  }
#else
  ESP_LOGCONFIG(TAG, "  Sensor support: DISABLED");
#endif
}

/**
 * @brief Sends a command string to the emonTx device via UART.
 *
 * @param command The command string to send (LF will be appended automatically).
 */
void EmonTx::send_command(const std::string &command) {
  ESP_LOGD(TAG, "Sending command to emonTx: %s", command.c_str());
  this->write_str(command.c_str());
  this->write_byte('\n');
}

#ifdef USE_SENSOR
/**
 * @brief Registers a sensor to receive updates for a specific JSON tag.
 *
 * @param tag_name The JSON key to monitor for this sensor (must be a string literal).
 * @param sensor Pointer to the sensor that will receive value updates.
 */
void EmonTx::register_sensor(const char *tag_name, sensor::Sensor *sensor) {
  ESP_LOGCONFIG(TAG, "Registering sensor for tag: %s", tag_name);
  this->sensors_.emplace_back(tag_name, sensor);
}
#endif

}  // namespace esphome::emontx
