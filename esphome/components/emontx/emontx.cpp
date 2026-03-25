#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

namespace esphome::emontx {

static const char *const TAG = "emontx";

void EmonTx::setup() { this->buffer_pos_ = 0; }

/**
 * @brief Implements the main loop for parsing data from the serial port.
 *
 * @details Continuously processes incoming UART data line-by-line:
 * 1. Fire on_data callbacks for all received lines
 * 2. If line starts with '{', parse as JSON and update sensors/callbacks
 */
void EmonTx::loop() {
  // Read all available data to prevent UART buffer overflow
  while (this->available() > 0) {
    uint8_t received = this->read();

    if (received == '\r') {
      continue;  // Ignore CR
    } else if (received == '\n') {
      // End of line - process the buffer
      if (this->buffer_pos_ > 0) {
        std::string line(this->buffer_.data(), this->buffer_pos_);
        this->buffer_pos_ = 0;

        ESP_LOGD(TAG, "Received line: %s", line.c_str());

        // Fire data callbacks for all received lines
        this->data_callbacks_.call(line);

        // Check if this line is JSON (starts with '{')
        if (line[0] == '{') {
          ESP_LOGV(TAG, "Line is JSON, parsing...");
          this->parse_json_(line);
        }
      }
    } else {
      // Regular character - add to buffer
      if (this->buffer_pos_ >= MAX_LINE_LENGTH) {
        ESP_LOGW(TAG, "Buffer overflow (>%zu bytes), discarding buffer", MAX_LINE_LENGTH);
        this->buffer_pos_ = 0;
      } else {
        this->buffer_[this->buffer_pos_++] = static_cast<char>(received);
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
 * 2. Executes all registered JSON callbacks, passing the parsed JsonObject
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

      auto val = root[tag];
      if (val.is<JsonVariant>()) {
        float value = val;
        ESP_LOGV(TAG, "Updating sensor '%s' with value: %.2f", tag, value);
        sensor_ptr->publish_state(value);
      }
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
