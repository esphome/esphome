#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

namespace esphome::emontx {

ESPHOME_LOG_TAG(TAG, "emontx");

void EmonTx::setup() {
  this->buffer_pos_ = 0;
  if (this->unlock_delay_ > 0) {
    // Lost if paused at that time, so the emonTx stays locked
    this->set_timeout(this->unlock_delay_, [this]() {
      ESP_LOGI(TAG, "Unlocking emonTx after boot");
      this->send_command("emonunlock");
    });
  }
}

void EmonTx::on_shutdown() {
  if (this->unlock_delay_ > 0) {
    ESP_LOGI(TAG, "Locking emonTx before reboot");
    this->send_command("emonlock");
    this->flush();
  }
}

/**
 * @brief Implements the main loop for parsing data from the serial port.
 *
 * @details Continuously processes incoming UART data line-by-line:
 * 1. Fire on_data callbacks for all received lines
 * 2. If line starts with '{', parse as JSON and update sensors/callbacks
 */
void EmonTx::loop() {
  if (this->is_paused()) {
    this->was_paused_ = true;
    return;
  }
  if (this->was_paused_) {
    // Drop what arrived during the pause and resync at the next line start
    this->was_paused_ = false;
    this->buffer_pos_ = 0;
    this->discarding_line_ = true;
    uint8_t dropped;
    while (this->available() > 0 && this->read_byte(&dropped))
      this->discarding_line_ = dropped != '\n';
  }

  // Read all available data to prevent UART buffer overflow
  while (this->available() > 0) {
    uint8_t received;
    if (!this->read_byte(&received))
      break;

    if (received == '\n') {
      // End of line - process the buffer
      if (this->discarding_line_) {
        this->discarding_line_ = false;
      } else if (this->buffer_pos_ > 0) {
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
    } else if (received == '\r' || this->discarding_line_) {
      // Ignore CR, and the rest of a line that overflowed or was interrupted by a pause
    } else if (this->buffer_pos_ >= MAX_LINE_LENGTH) {
      ESP_LOGW(TAG, "Buffer overflow (>%zu bytes), discarding line", MAX_LINE_LENGTH);
      this->buffer_pos_ = 0;
      this->discarding_line_ = true;
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
  if (this->unlock_delay_ > 0) {
    ESP_LOGCONFIG(TAG, "  Unlock delay: %" PRIu32 " ms", this->unlock_delay_);
  }

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
void EmonTx::send_command(const char *command) {
  if (this->is_paused()) {
    ESP_LOGW(TAG, "Not sending command, UART is paused: %s", command);
    return;
  }
  ESP_LOGD(TAG, "Sending command to emonTx: %s", command);
  this->write_str(command);
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
