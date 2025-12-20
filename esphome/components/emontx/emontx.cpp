#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/homeassistant_service.h"
#endif

namespace esphome {
namespace emontx {

static const char *const TAG = "emontx";

/**
 * @brief Initializes the EmonTx component.
 *
 * @details Sets up the initial state of the component by:
 * 1. Setting the state machine to OFF
 * 2. Pre-allocating buffer memory to avoid reallocation overhead
 * 3. Logging the component configuration
 * 4. Registering and logging all configured sensors (when sensor support is enabled)
 *
 * This method is called once during device startup. After setup completes,
 * the component will wait for update() to be called before starting to
 * process any incoming data.
 */
void EmonTx::setup() {
  state_ = OFF;

  // Pre-allocate buffer to maximum size to prevent reallocation overhead
  // during JSON message collection.
  // 198 bytes should be enough to contain a full session in historical mode with
  // three phases. But go with 1024 just to be sure.
  this->buffer_.reserve(1024);

  ESP_LOGCONFIG(TAG, "Setting up EmonTx component");

#ifdef USE_API
  // Auto-register send_command service when config_panel is enabled
  if (this->config_panel_) {
    this->register_service(&EmonTx::send_command_service, "send_command", {"command"});
    ESP_LOGCONFIG(TAG, "Registered send_command service");
  }
#endif

#ifdef USE_SENSOR
  // Log sensors at setup time
  ESP_LOGCONFIG(TAG, "Currently registered sensors: %u", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor '%s' registered", sensor_pair.first.c_str());
  }
#else
  ESP_LOGCONFIG(TAG, "Sensor support: DISABLED");
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

  if (state_ == OFF) {
    buffer_.clear();  // Clear the buffer for new data
    state_ = WAITING_FOR_START;
    ESP_LOGD(TAG, "EmonTx activated and ready to receive data.");
  } else {
    ESP_LOGV(TAG, "EmonTx already active (state: %d)", state_);
  }
}

/**
 * @brief Implements the main state machine for parsing data from the serial port.
 *
 * @details The state machine continuously processes incoming UART data through these states:
 * - OFF: Initial state, waiting for update() to activate the component.
 * - WAITING_FOR_START: Looks for the opening brace '{' of a JSON object OR collects
 *   characters for plain text lines (when web_config is enabled).
 * - COLLECTING_JSON: Collects characters until the closing brace '}' is found.
 * - JSON_COLLECTED: Processes the complete JSON object or plain text line.
 *
 * When web_config is enabled, all received lines are captured and forwarded via
 * line callbacks. JSON lines are additionally parsed for sensor updates.
 *
 * This continuous processing ensures no data is lost when multiple messages
 * arrive in quick succession between polling intervals.
 */
void EmonTx::loop() {
  if (state_ == OFF) {
    return;
  }

  uint16_t bytes_read = 0;
  while (available() > 0 && bytes_read < 512) {
    bytes_read++;
    uint8_t received = read();

    // Handle different characters
    if (received == '\r') {
      continue;  // Ignore CR
    } else if (received == '\n') {
      // End of line - process the buffer
      if (!buffer_.empty()) {
        std::string line = buffer_;
        buffer_.clear();

        ESP_LOGD(TAG, "Received line: %s", line.c_str());

#ifdef USE_API_HOMEASSISTANT_SERVICES
        // Fire esphome.emontx_raw event for config panel
        if (this->config_panel_) {
          if (api::global_api_server == nullptr) {
            ESP_LOGW(TAG, "Cannot fire event: api_server is null");
          } else if (!api::global_api_server->is_connected()) {
            ESP_LOGV(TAG, "Cannot fire event: api_server not connected");
          } else {
            api::HomeassistantActionRequest resp;
            resp.set_service(StringRef("esphome.emontx_raw"));
            resp.is_event = true;
            resp.data.init(2);
            auto &kv1 = resp.data.emplace_back();
            kv1.set_key(StringRef("device_id"));
            kv1.value = App.get_name();
            auto &kv2 = resp.data.emplace_back();
            kv2.set_key(StringRef("line"));
            kv2.value = line;
            api::global_api_server->send_homeassistant_action(resp);
            ESP_LOGV(TAG, "Fired esphome.emontx_raw event");
          }
        }
#endif

        // Fire data callbacks for ALL received lines (config responses)
        if (!this->data_callbacks_.empty()) {
          for (const auto &callback : this->data_callbacks_) {
            callback(line);
          }
        }

        // Check if this line is JSON (starts with '{')
        if (!line.empty() && line[0] == '{') {
          ESP_LOGV(TAG, "Line is JSON, parsing...");
          parse_json_(line);
        }
      }
    } else {
      // Regular character - add to buffer
      if (buffer_.length() >= 1024) {
        ESP_LOGW(TAG, "Buffer overflow (>1024 bytes), discarding buffer");
        buffer_.clear();
      } else {
        buffer_ += received;
      }
    }
  }

  if (bytes_read >= 512 && available() > 0) {
    ESP_LOGV(TAG, "Reached per-iteration read limit (512 bytes), will continue in next loop()");
  }
}

/**
 * @brief Parses a JSON string and updates associated sensors and listeners.
 *
 * @details This method takes a string containing JSON data and attempts to parse it.
 * If parsing is successful, it performs the following operations:
 * 1. Updates all registered sensors that have matching keys in the JSON
 * 2. Updates all registered listeners with their corresponding values
 * 3. Stores the successfully parsed JSON string for use in callbacks
 * 4. Executes all registered JSON callbacks, passing the parsed JsonObject
 *
 * The method handles both sensor updates and general-purpose callbacks, allowing
 * the component to integrate with multiple parts of the ESPHome system.
 *
 * @param data The JSON string to parse
 */
void EmonTx::parse_json_(const std::string &data) {
  ESP_LOGV(TAG, "Parsing JSON: %s", data.c_str());
  ESP_LOGV(TAG, "Listener list contains '%d' items", (int) this->emontx_listeners_.size());

  bool success = json::parse_json(data, [this, &data](JsonObject root) {
#ifdef USE_SENSOR
    // Update all registered sensors
    for (auto &sensor_pair : this->sensors_) {
      const std::string &tag = sensor_pair.first;
      sensor::Sensor *sensor = sensor_pair.second;

      if (root[tag].is<JsonVariant>()) {
        float value = root[tag];
        ESP_LOGV(TAG, "Updating sensor '%s' with value: %.2f", tag.c_str(), value);
        sensor->publish_state(value);
      }
    }
#endif

    // Also update all listeners
    ESP_LOGV(TAG, "Listener list contains '%d' items", (int) this->emontx_listeners_.size());
    for (auto *listener : this->emontx_listeners_) {
      if (root[listener->tag].is<JsonVariant>()) {
        const auto value = root[listener->tag].as<std::string>();

        ESP_LOGD(TAG, "  Publish to listener '%s' with value '%s'", listener->tag.c_str(), value.c_str());

        listener->publish_val(value);
      }
    }

#ifdef USE_API_HOMEASSISTANT_SERVICES
    // Fire Home Assistant event with the received data
    if (api::global_api_server != nullptr && api::global_api_server->is_connected()) {
      api::HomeassistantActionRequest resp;
      resp.set_service(StringRef("esphome.emontx_json"));
      resp.is_event = true;
      resp.data.init(1);
      auto &kv = resp.data.emplace_back();
      kv.set_key(StringRef("data"));
      kv.value = data;
      api::global_api_server->send_homeassistant_action(resp);
      ESP_LOGV(TAG, "Fired esphome.emontx_json event");
    }
#endif

    // Execute all registered JSON callbacks
    if (!this->json_callbacks_.empty()) {
      ESP_LOGV(TAG, "Executing %d JSON callbacks", (int) this->json_callbacks_.size());
      for (const auto &callback : this->json_callbacks_) {
        callback(root, data);  // Pass both JsonObject and raw JSON string
      }
    }

    return true;  // Parsing was handled successfully
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON");
  }
}

/**
 * @brief Logs the EmonTx component configuration details.
 *
 * @details This method is called during startup to output the component's
 * configuration to the log. It provides information about:
 * - The component identification
 * - Number of registered sensors (when sensor support is enabled)
 * - List of all registered sensors with their tag names
 *
 * This information is valuable for debugging and verifying that the
 * component is correctly configured according to the YAML definition.
 * The method is automatically called by ESPHome's core during device startup.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx:");
  ESP_LOGCONFIG(TAG, "  Config panel: %s", this->config_panel_ ? "ENABLED" : "DISABLED");

#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Registered sensors: %u", this->sensors_.size());

  // List all registered sensors with their tags
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor_pair.first.c_str());
  }
#else
  ESP_LOGCONFIG(TAG, "  Sensor support: DISABLED");
#endif
}

/**
 * @brief Publishes a value to all registered listeners that match the given tag.
 *
 * @details This method iterates through all registered EmonTx listeners and forwards
 * the provided value to any listener whose tag matches the specified tag parameter.
 * This internal helper method is used to distribute received data to the appropriate
 * components within the ESPHome system.
 *
 * @param tag The tag identifier to match against registered listeners.
 * @param val The string value to publish to matching listeners.
 */
void EmonTx::publish_value_(const std::string &tag, const std::string &val) {
  for (auto *element : emontx_listeners_) {
    if (tag != element->tag)
      continue;
    element->publish_val(val);
  }
}

/**
 * @brief Registers a listener to receive updates for specific JSON data tags.
 *
 * @details This method adds the provided listener to the internal list of EmonTx listeners.
 * When JSON data is received and successfully parsed, any listener whose tag matches
 * a key in the JSON will receive the corresponding value through its publish_val() method.
 *
 * This registration mechanism allows other ESPHome components to subscribe to specific
 * data points from the EmonTx JSON stream without having to implement their own parsing logic.
 *
 * @param listener Pointer to the listener object to register. The listener must have a 'tag'
 *                 property that identifies which JSON key it's interested in.
 */
void EmonTx::send_command(std::string command) {
  ESP_LOGD(TAG, "Sending command to emonTx: %s", command.c_str());
  // Append CR+LF as required by emonTx firmware
  command += "\r\n";
  this->write_str(command.c_str());
}

#ifdef USE_SENSOR
void EmonTx::register_emontx_listener(EmonTxListener *listener) { emontx_listeners_.push_back(listener); }

void EmonTx::register_sensor(const std::string &tag_name, sensor::Sensor *sensor) {
  ESP_LOGCONFIG(TAG, "Registering sensor for tag: %s", tag_name.c_str());
  this->sensors_.emplace_back(tag_name, sensor);
}
#endif

}  // namespace emontx
}  // namespace esphome
