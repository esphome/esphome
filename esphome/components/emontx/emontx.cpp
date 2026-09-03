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
        // Null-terminate for safe logging and c_str() use
        size_t len = this->buffer_pos_;
        this->buffer_[len] = '\0';
        this->buffer_pos_ = 0;

        StringRef line(this->buffer_.data(), len);
        ESP_LOGD(TAG, "Received line: %s", line.c_str());

        // Fire data callbacks for all received lines
        this->data_callbacks_.call(line);

        // Check if this line is JSON (starts with '{')
        if (this->buffer_[0] == '{') {
          ESP_LOGV(TAG, "Line is JSON, parsing...");
          this->parse_json_(this->buffer_.data(), len);
        }
      }
    } else if (this->buffer_pos_ >= MAX_LINE_LENGTH) {
      ESP_LOGW(TAG, "Buffer overflow (>%zu bytes), discarding buffer", MAX_LINE_LENGTH);
      this->buffer_pos_ = 0;
    } else {
      this->buffer_[this->buffer_pos_++] = static_cast<char>(received);
    }
  }
}

void EmonTx::parse_json_(const char *data, size_t len) {
  bool success = json::parse_json(reinterpret_cast<const uint8_t *>(data), len, [this, data, len](JsonObject root) {
#ifdef USE_SENSOR
    for (auto &sensor_pair : this->sensors_) {
      auto val = root[sensor_pair.first];
      if (val.is<JsonVariant>()) {
        float value = val;
        ESP_LOGV(TAG, "Updating sensor '%s' with value: %.2f", sensor_pair.first, value);
        sensor_pair.second->publish_state(value);
      }
    }
#endif

    this->json_callbacks_.call(root, StringRef(data, len));
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
