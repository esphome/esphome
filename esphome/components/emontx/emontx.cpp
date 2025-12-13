#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

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
  // during JSON message collection
  this->buffer_.reserve(1024);

  ESP_LOGCONFIG(TAG, "Setting up EmonTx component");

#ifdef USE_SENSOR
  // Log sensors at setup time
  ESP_LOGCONFIG(TAG, "Currently registered sensors: %u", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor '%s' registered", sensor_pair.first.c_str());
  }
#else
  ESP_LOGCONFIG(TAG, "Sensor support: DISABLED");
#endif

#ifdef USE_EMONTX_WEB_CONFIG
  if (this->web_server_ != nullptr) {
    this->web_server_->add_handler(new EmonTxConfigHandler(this));
    this->web_server_->add_handler(new EmonTxSendHandler(this));
    this->web_server_->add_handler(new EmonTxDataHandler(this));
    ESP_LOGI(TAG, "Web config interface available at /emontx/config");
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

  if (state_ == OFF) {
    buffer_.clear();  // Clear the buffer for new data
    state_ = WAITING_FOR_START;
    ESP_LOGD(TAG, "EmonTx activated and ready to receive data.");
  } else {
    ESP_LOGV(TAG, "EmonTx already active (state: %d)", state_);
  }
}

/**
 * @brief Reads characters from UART until a specific target character is found.
 *
 * @details This method is used by the JSON parsing state machine to extract complete JSON objects.
 * It operates in two modes controlled by the drop parameter:
 * - When drop=true: Discards all characters except the target (used to find the opening brace)
 * - When drop=false: Collects all characters until the target is found (used to collect JSON content)
 *
 * The method enforces the EmonTx protocol requirement that JSON messages must not contain
 * newline characters within the message body. The EmonTx device sends compact JSON without
 * newlines, so any newline encountered indicates a protocol error or message boundary.
 * It processes up to 512 bytes per loop() iteration to maintain system responsiveness.
 *
 * @param drop If true, discard all characters except the target character.
 * @param c The target character to look for.
 * @return true If the target character was found.
 * @return false If an error occurred (newline in JSON, buffer overflow) or target not found yet.
 */
bool EmonTx::read_chars_until_(bool drop, uint8_t c) {
  uint8_t received;
  uint16_t bytes_read = 0;

  // Process up to 512 bytes per iteration to maintain system responsiveness
  // while allowing large JSON messages to be collected across multiple loop() calls
  while (available() > 0 && bytes_read < 512) {
    bytes_read++;
    received = read();

    if (drop && received != c)
      continue;

    // If we're collecting JSON data (not dropping) and receive a newline,
    // this indicates a protocol error or message boundary - discard the buffer
    if (!drop && (received == '\r' || received == '\n')) {
      ESP_LOGW(TAG, "Newline found within JSON data, discarding buffer");
      buffer_.clear();
      state_ = WAITING_FOR_START;
      return false;
    }

    // Prevent buffer overflow
    if (buffer_.length() >= 1024) {
      ESP_LOGW(TAG, "Buffer overflow (>1024 bytes), discarding buffer");
      buffer_.clear();
      state_ = WAITING_FOR_START;
      return false;
    }

    buffer_ += received;

    if (received == c) {
      return true;
    }
  }

  // Log if we hit the per-iteration limit with more data available
  if (bytes_read >= 512 && available() > 0) {
    ESP_LOGV(TAG, "Reached per-iteration read limit (512 bytes), will continue in next loop()");
  }

  return false;
}

/**
 * @brief Implements the main state machine for parsing JSON data from the serial port.
 *
 * @details The state machine continuously processes incoming UART data through these states:
 * - OFF: Initial state, waiting for update() to activate the component.
 * - WAITING_FOR_START: Looks for the opening brace '{' of a JSON object.
 * - COLLECTING_JSON: Collects characters until the closing brace '}' is found.
 *   Any newline characters during this phase will cause the buffer to be discarded.
 * - JSON_COLLECTED: Processes the complete JSON object, updating sensors and
 *   executing callbacks, then immediately returns to WAITING_FOR_START to process
 *   the next JSON object.
 *
 * This continuous processing ensures no data is lost when multiple JSON messages
 * arrive in quick succession between polling intervals.
 */
void EmonTx::loop() {
  switch (state_) {
    case OFF:
      // Do nothing, waiting for setup
      break;

    case WAITING_FOR_START:
      if (read_chars_until_(true, '{')) {
        // Start of JSON object detected
        state_ = COLLECTING_JSON;
      }
      // Ignore any other characters
      break;

    case COLLECTING_JSON:
      if (read_chars_until_(false, '}')) {
        state_ = JSON_COLLECTED;
      }
      break;

    case JSON_COLLECTED:
      if (!buffer_.empty()) {
        ESP_LOGI(TAG, "Received data: %s", buffer_.c_str());
        parse_json_(buffer_);
      } else {
        ESP_LOGW(TAG, "Received empty buffer, skipping JSON parsing");
      }
      buffer_.clear();             // Clear buffer for next JSON object
      state_ = WAITING_FOR_START;  // Continue processing immediately
      break;
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

    // Save the valid JSON data for callbacks
    this->last_valid_json_ = data;

    // Execute all registered JSON callbacks
    if (!this->json_callbacks_.empty()) {
      ESP_LOGV(TAG, "Executing %d JSON callbacks", (int) this->json_callbacks_.size());
      for (const auto &callback : this->json_callbacks_) {
        callback(root);
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

#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Registered sensors: %u", this->sensors_.size());

  // List all registered sensors with their tags
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor_pair.first.c_str());
  }
#else
  ESP_LOGCONFIG(TAG, "  Sensor support: DISABLED");
#endif

#ifdef USE_EMONTX_WEB_CONFIG
  ESP_LOGCONFIG(TAG, "  Web config: ENABLED");
  ESP_LOGCONFIG(TAG, "    Config URL: /emontx/config");
  ESP_LOGCONFIG(TAG, "    Serial Data: /emontx/data (polling)");
  ESP_LOGCONFIG(TAG, "    Serial Send: /emontx/send (POST)");
  ESP_LOGCONFIG(TAG, "    OEM HTML cached: %s", this->html_fetched_ ? "yes" : "no");
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
#ifdef USE_SENSOR
void EmonTx::register_emontx_listener(EmonTxListener *listener) { emontx_listeners_.push_back(listener); }

void EmonTx::register_sensor(const std::string &tag_name, sensor::Sensor *sensor) {
  ESP_LOGCONFIG(TAG, "Registering sensor for tag: %s", tag_name.c_str());
  this->sensors_.emplace_back(tag_name, sensor);
}
#endif

#ifdef USE_EMONTX_WEB_CONFIG

// URL for OEM serial config interface (hosted version with proper styling)
static const char *OEM_SERIAL_URL = "https://openenergymonitor.org/serial/";

// JavaScript patch to replace Web Serial API with polling + fetch
static const char *POLLING_PATCH = R"(
<script>
// Polling + fetch bridge - replaces Web Serial API
var pollInterval = null;
var lastJson = "";
var outputStream = null;

async function connect() {
    return new Promise((resolve, reject) => {
        app.connected = true;
        app.button_connect_text = "Connected";

        outputStream = {
            getWriter: () => ({
                write: (data) => {
                    fetch("/emontx/send", {
                        method: "POST",
                        body: data,
                        headers: {"Content-Type": "text/plain"}
                    });
                },
                releaseLock: () => {}
            })
        };

        // Start polling for data
        pollInterval = setInterval(async () => {
            try {
                const response = await fetch("/emontx/data");
                const data = await response.text();
                if (data && data !== lastJson && data !== "{}") {
                    lastJson = data;
                    log.textContent += data + "\n";
                    log.scrollTop = log.scrollHeight;
                    process_line(data);
                }
            } catch (e) {
                console.error("Poll error:", e);
            }
        }, 500);

        setTimeout(() => {
            if (!app.config_received) {
                writeToStream("l");
            }
        }, 1000);

        resolve();
    });
}

document.addEventListener('DOMContentLoaded', () => {
    setTimeout(() => connect().catch(e => console.log('Auto-connect failed:', e)), 500);
});
</script>
)";

// EmonTxConfigHandler implementation
bool EmonTxConfigHandler::canHandle(AsyncWebServerRequest *request) const {
  return request->method() == HTTP_GET && request->url() == "/emontx/config";
}

void EmonTxConfigHandler::handleRequest(AsyncWebServerRequest *request) { this->emontx_->serve_config_page(request); }

// EmonTxSendHandler implementation
bool EmonTxSendHandler::canHandle(AsyncWebServerRequest *request) const {
  return request->method() == HTTP_POST && request->url() == "/emontx/send";
}

void EmonTxSendHandler::handleRequest(AsyncWebServerRequest *request) { request->send(200, "text/plain", "OK"); }

void EmonTxSendHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                   size_t total) {
  std::string cmd(reinterpret_cast<char *>(data), len);
  ESP_LOGD(TAG, "Web -> UART: %s", cmd.c_str());
  this->emontx_->handle_serial_send(cmd);
}

// EmonTxDataHandler implementation - returns last received JSON for polling
bool EmonTxDataHandler::canHandle(AsyncWebServerRequest *request) const {
  return request->method() == HTTP_GET && request->url() == "/emontx/data";
}

void EmonTxDataHandler::handleRequest(AsyncWebServerRequest *request) {
  std::string json = this->emontx_->get_last_json();
  if (json.empty()) {
    json = "{}";
  }
  request->send(200, "application/json", json.c_str());
}

// EmonTx web config methods
void EmonTx::serve_config_page(AsyncWebServerRequest *request) {
  ESP_LOGI(TAG, "Config page requested");

  if (this->html_fetched_ && !this->cached_html_.empty()) {
    ESP_LOGI(TAG, "Serving cached HTML (%d bytes)", this->cached_html_.size());
    request->send(200, "text/html", this->cached_html_.c_str());
    return;
  }

  ESP_LOGI(TAG, "HTML not cached, fetching...");
  this->fetch_oem_html_();

  if (this->html_fetched_ && !this->cached_html_.empty()) {
    ESP_LOGI(TAG, "Serving freshly fetched HTML (%d bytes)", this->cached_html_.size());
    request->send(200, "text/html", this->cached_html_.c_str());
  } else {
    ESP_LOGE(TAG, "Failed to fetch HTML, sending error response");
    request->send(502, "text/plain", "Failed to fetch OEM interface. Check network connection.");
  }
}

void EmonTx::fetch_oem_html_() {
#ifdef USE_ESP32
  if (this->html_fetched_) {
    return;
  }

  ESP_LOGI(TAG, "Fetching OEM interface from openenergymonitor.org...");

  std::string html;

#ifdef USE_ARDUINO
  // Arduino framework - use HTTPClient with WiFiClientSecure for HTTPS
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate verification

  HTTPClient http;

  http.begin(client, OEM_SERIAL_URL);
  http.setTimeout(15000);
  ESP_LOGI(TAG, "Sending HTTP GET request...");
  int httpCode = http.GET();
  ESP_LOGI(TAG, "HTTP response code: %d", httpCode);
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    ESP_LOGI(TAG, "Received %d bytes from server", response.length());
    html = response.c_str();
  } else {
    ESP_LOGE(TAG, "Failed to fetch OEM interface: %d", httpCode);
    http.end();
    return;
  }
  http.end();

#else
  // ESP-IDF framework - use esp_http_client with certificate bundle for HTTPS
  esp_http_client_config_t config = {};
  config.url = OEM_SERIAL_URL;
  config.timeout_ms = 15000;
  config.buffer_size = 2048;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    ESP_LOGE(TAG, "Failed to init HTTP client");
    return;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return;
  }

  int content_length = esp_http_client_fetch_headers(client);
  if (content_length < 0) {
    ESP_LOGE(TAG, "Failed to fetch headers");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return;
  }

  int status = esp_http_client_get_status_code(client);
  if (status != 200) {
    ESP_LOGE(TAG, "HTTP error: %d", status);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return;
  }

  char buffer[512];
  int read_len;
  while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[read_len] = '\0';
    html += buffer;
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
#endif

  this->cached_html_ = this->patch_html_for_polling_(html);
  this->html_fetched_ = true;

  ESP_LOGI(TAG, "OEM interface fetched and cached (%d bytes)", this->cached_html_.size());
#endif
}

std::string EmonTx::patch_html_for_polling_(const std::string &html) {
  std::string patched = html;

  // Remove the original connect() function and Web Serial API code
  // The hosted version has JavaScript that uses Web Serial API which we replace
  size_t pos = patched.find("async function connect()");
  if (pos != std::string::npos) {
    size_t script_start = patched.rfind("<script>", pos);
    size_t script_end = patched.find("</script>", pos);
    if (script_start != std::string::npos && script_end != std::string::npos) {
      patched.erase(script_start, script_end - script_start + 9);
    }
  }

  // Insert our polling patch before closing </body>
  pos = patched.find("</body>");
  if (pos != std::string::npos) {
    patched.insert(pos, POLLING_PATCH);
  } else {
    patched += POLLING_PATCH;
  }

  return patched;
}

void EmonTx::handle_serial_send(const std::string &data) { this->write_str(data.c_str()); }

#endif  // USE_EMONTX_WEB_CONFIG

}  // namespace emontx
}  // namespace esphome
