#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/components/json/json_util.h"

namespace esphome::emontx {

ESPHOME_LOG_TAG(TAG, "emontx");

#ifdef USE_EMONTX_VERSION
static constexpr uint32_t VERSION_REQUEST_ID = 0;
// The emonTx joins bytes received while a command is pending into that command, so 'v' waits
static constexpr uint32_t VERSION_REQUEST_DELAY_MS = 250;
// Line of the 'v' reply (and of the boot banner) carrying the release, e.g. "  - Version:    v1.1.1"
static const char VERSION_PREFIX[] PROGMEM = "  - Version:";
#endif

void EmonTx::setup() {
  this->buffer_pos_ = 0;
  if (this->lock_on_reboot_) {
    // The line feed ends any boot noise first
    ESP_LOGI(TAG, "Unlocking emonTx after boot");
    this->write_byte('\n');
    this->send_command("emonunlock");
  }
#ifdef USE_EMONTX_VERSION
  if (this->version_text_sensor_ != nullptr)
    this->request_version_();
#endif
}

void EmonTx::on_shutdown() {
  if (!this->lock_on_reboot_)
    return;
  if (this->is_paused()) {
    ESP_LOGW(TAG, "Not locking emonTx before shutdown, UART is paused");
    return;
  }
  ESP_LOGI(TAG, "Locking emonTx before shutdown");
  this->send_command("emonlock");
  this->flush();
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
#ifdef USE_EMONTX_VERSION
    // The emonTx may have been flashed, or a request dropped, during the pause
    if (this->version_text_sensor_ != nullptr)
      this->request_version_();
#endif
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
#ifdef USE_EMONTX_VERSION
        if (this->version_text_sensor_ != nullptr)
          this->parse_version_line_(this->buffer_.data(), len);
#endif
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
#ifdef USE_EMONTX_VERSION
    auto msg = root["MSG"];
    if (this->version_text_sensor_ != nullptr && msg.is<uint32_t>()) {
      // MSG restarts at 1 when the emonTx reboots, as it does after a firmware update
      uint32_t msg_number = msg;
      if (msg_number <= this->last_msg_number_)
        this->request_version_();
      this->last_msg_number_ = msg_number;
    }
#endif

    this->json_callbacks_.call(root, StringRef(data, len));
    return true;
  });

  if (!success) {
    ESP_LOGW(TAG, "Failed to parse JSON");
  }
}

#ifdef USE_EMONTX_VERSION
// The line feed ends any pending input; the reply is picked up in loop() by parse_version_line_()
void EmonTx::request_version_() {
  if (this->is_paused())
    return;
  this->write_byte('\n');
  this->set_timeout(VERSION_REQUEST_ID, VERSION_REQUEST_DELAY_MS, [this]() { this->send_command("v"); });
}

void EmonTx::parse_version_line_(const char *line, size_t len) {
  constexpr size_t prefix_len = sizeof(VERSION_PREFIX) - 1;
  if (len <= prefix_len || ESPHOME_strncmp_P(line, VERSION_PREFIX, prefix_len) != 0)
    return;
  const char *version = line + prefix_len;
  const char *end = line + len;
  while (version < end && *version == ' ')
    version++;
  if (version == end)
    return;
  ESP_LOGI(TAG, "emonTx firmware version: %.*s", static_cast<int>(end - version), version);
  this->version_text_sensor_->publish_state(version, end - version);
}
#endif

/**
 * @brief Logs the EmonTx component configuration details.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx:");
  ESP_LOGCONFIG(TAG, "  Lock on reboot: %s", YESNO(this->lock_on_reboot_));

#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Registered sensors: %zu", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "    Sensor: %s", sensor_pair.first);
  }
#else
  ESP_LOGCONFIG(TAG, "  Sensor support: DISABLED");
#endif
#ifdef USE_EMONTX_VERSION
  LOG_TEXT_SENSOR("  ", "Firmware version", this->version_text_sensor_);
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
