#include "emontx.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/components/json/json_util.h"

namespace esphome::emontx {

ESPHOME_LOG_TAG(TAG, "emontx");

static constexpr uint32_t UNLOCK_ID = 0;
// The emonTx joins bytes received while a command is pending into that command, so the next one waits
static constexpr uint32_t COMMAND_GAP_MS = 250;

#ifdef USE_EMONTX_VERSION
static constexpr uint32_t VERSION_REQUEST_ID = 1;
// Without a reply, 'v' is sent again every VERSION_RETRY_FRAMES data lines, VERSION_MAX_ATTEMPTS times in all
static constexpr uint8_t VERSION_RETRY_FRAMES = 2;
static constexpr uint8_t VERSION_MAX_ATTEMPTS = 3;
// Line of the 'v' reply (and of the boot banner) carrying the release, e.g. "  - Version:    v1.1.1"
static const char VERSION_PREFIX[] PROGMEM = "  - Version:";
// Every data line starts with the message counter, in JSON or text format
static const char MSG_JSON_PREFIX[] PROGMEM = "{\"MSG\":";
static const char MSG_TEXT_PREFIX[] PROGMEM = "MSG:";
// First firmware with 'emonlock' and 'emonunlock'; older firmware takes them for the bootloader command
static constexpr uint32_t LOCK_MIN_MAJOR = 1;
static constexpr uint32_t LOCK_MIN_MINOR = 2;

// True if line starts with the PROGMEM prefix and has more after it
static bool starts_with(const char *line, size_t len, const char *prefix, size_t prefix_len) {
  return len > prefix_len && ESPHOME_strncmp_P(line, prefix, prefix_len) == 0;
}

// Reads the decimal number (up to 9 digits) at pos and moves pos past it; false if there is none
static bool read_number(const char *&pos, const char *end, uint32_t &value) {
  const char *start = pos;
  value = 0;
  while (pos < end && pos - start < 9 && *pos >= '0' && *pos <= '9')
    value = value * 10 + (*pos++ - '0');
  return pos != start;
}

// True for a release such as "v1.2.0" or "v1.3.0-rc1" from LOCK_MIN_MAJOR.LOCK_MIN_MINOR on
static bool supports_lock(const char *version, const char *end) {
  if (version < end && (*version == 'v' || *version == 'V'))
    version++;
  uint32_t major;
  if (!read_number(version, end, major) || version == end || *version != '.')
    return false;
  version++;
  uint32_t minor;
  if (!read_number(version, end, minor))
    return false;
  return major > LOCK_MIN_MAJOR || (major == LOCK_MIN_MAJOR && minor >= LOCK_MIN_MINOR);
}
#endif

void EmonTx::setup() {
  if (this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_ALWAYS) {
    // The line feed ends any boot noise
    this->write_byte('\n');
    this->set_timeout(UNLOCK_ID, COMMAND_GAP_MS, [this]() {
      this->unlock_();
#ifdef USE_EMONTX_VERSION
      if (this->wants_version_())
        this->request_version_();
#endif
    });
    return;
  }
#ifdef USE_EMONTX_VERSION
  // With AUTO, the reply decides whether to unlock
  if (this->wants_version_())
    this->request_version_();
#endif
}

void EmonTx::unlock_() {
  ESP_LOGI(TAG, "Unlocking emonTx");
  this->send_command("emonunlock");
}

void EmonTx::on_shutdown() {
  bool lock = this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_ALWAYS;
#ifdef USE_EMONTX_VERSION
  lock |= this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_AUTO && this->lock_supported_;
#endif
  if (!lock)
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
    bool discard = true;
    uint8_t dropped;
    while (this->available() > 0 && this->read_byte(&dropped))
      discard = dropped != '\n';
    if (discard) {
      this->line_reader_.discard_line();
    } else {
      this->line_reader_.reset();
    }
#ifdef USE_EMONTX_VERSION
    // The emonTx may have been flashed, or a request dropped, during the pause
    if (this->wants_version_())
      this->request_version_();
#endif
  }

  // Read all available data to prevent UART buffer overflow
  while (this->available() > 0) {
    uint8_t received;
    if (!this->read_byte(&received))
      break;

    const char *data = this->line_reader_.feed(received);
    if (this->line_reader_.overflowed()) {
      ESP_LOGW(TAG, "Buffer overflow (>%zu bytes), discarding line", MAX_LINE_LENGTH);
    } else if (data != nullptr && data[0] != '\0') {
      size_t len = this->line_reader_.length();
      StringRef line(data, len);
      ESP_LOGD(TAG, "Received line: %s", line.c_str());

      // Fire data callbacks for all received lines
      this->data_callbacks_.call(line);

      // Check if this line is JSON (starts with '{')
      if (data[0] == '{') {
        ESP_LOGV(TAG, "Line is JSON, parsing...");
        this->parse_json_(data, len);
      }
#ifdef USE_EMONTX_VERSION
      if (this->wants_version_()) {
        this->parse_msg_number_(data, len);
        this->parse_version_line_(data, len);
      }
#endif
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

#ifdef USE_EMONTX_VERSION
void EmonTx::request_version_() {
  // The firmware may have changed; until the reply, an unknown version gets no lock commands
  this->lock_supported_ = false;
  this->version_frames_left_ = VERSION_RETRY_FRAMES * VERSION_MAX_ATTEMPTS;
  this->send_version_request_();
}

// The line feed ends any pending input; the reply is picked up in loop() by parse_version_line_()
void EmonTx::send_version_request_() {
  if (this->is_paused())
    return;
  this->write_byte('\n');
  this->set_timeout(VERSION_REQUEST_ID, COMMAND_GAP_MS, [this]() { this->send_command("v"); });
}

void EmonTx::parse_msg_number_(const char *line, size_t len) {
  const char *pos;
  if (starts_with(line, len, MSG_JSON_PREFIX, sizeof(MSG_JSON_PREFIX) - 1)) {
    pos = line + sizeof(MSG_JSON_PREFIX) - 1;
  } else if (starts_with(line, len, MSG_TEXT_PREFIX, sizeof(MSG_TEXT_PREFIX) - 1)) {
    pos = line + sizeof(MSG_TEXT_PREFIX) - 1;
  } else {
    return;
  }
  uint32_t msg_number;
  if (!read_number(pos, line + len, msg_number))
    return;

  // MSG restarts at 1 when the emonTx reboots, as it does after a firmware update
  if (msg_number <= this->last_msg_number_) {
    this->request_version_();
  } else if (this->version_frames_left_ > 0) {
    if (--this->version_frames_left_ == 0) {
      ESP_LOGW(TAG, "No firmware version from the emonTx, it may still be locked");
    } else if (this->version_frames_left_ % VERSION_RETRY_FRAMES == 0) {
      this->send_version_request_();
    }
  }
  this->last_msg_number_ = msg_number;
}

void EmonTx::parse_version_line_(const char *line, size_t len) {
  constexpr size_t prefix_len = sizeof(VERSION_PREFIX) - 1;
  if (!starts_with(line, len, VERSION_PREFIX, prefix_len))
    return;
  const char *version = line + prefix_len;
  const char *end = line + len;
  while (version < end && *version == ' ')
    version++;
  if (version == end)
    return;
  ESP_LOGI(TAG, "emonTx firmware version: %.*s", static_cast<int>(end - version), version);
  this->version_frames_left_ = 0;
#ifdef USE_EMONTX_VERSION_TEXT_SENSOR
  if (this->version_text_sensor_ != nullptr)
    this->version_text_sensor_->publish_state(version, end - version);
#endif

  bool supported = supports_lock(version, end);
  if (this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_AUTO && supported && !this->unlock_sent_) {
    // Once the rest of the reply has gone out; a pause drops it, and the version request on resume retries
    this->set_timeout(UNLOCK_ID, COMMAND_GAP_MS, [this]() {
      if (this->is_paused())
        return;
      this->unlock_();
      this->unlock_sent_ = true;
    });
  }
  this->lock_supported_ = supported;
}
#endif

/**
 * @brief Logs the EmonTx component configuration details.
 */
void EmonTx::dump_config() {
  ESP_LOGCONFIG(TAG, "EmonTx:");
  const char *lock = LOG_STR_LITERAL("no");
  if (this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_ALWAYS) {
    lock = LOG_STR_LITERAL("yes");
  } else if (this->lock_on_reboot_ == LockOnReboot::LOCK_ON_REBOOT_AUTO) {
    lock = LOG_STR_LITERAL("auto");
  }
  ESP_LOGCONFIG(TAG, "  Lock on reboot: %s", lock);

#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Registered sensors: %zu", this->sensors_.size());
  for (const auto &sensor_pair : this->sensors_) {
    ESP_LOGCONFIG(TAG, "    Sensor: %s", sensor_pair.first);
  }
#else
  ESP_LOGCONFIG(TAG, "  Sensor support: DISABLED");
#endif
#ifdef USE_EMONTX_VERSION_TEXT_SENSOR
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
