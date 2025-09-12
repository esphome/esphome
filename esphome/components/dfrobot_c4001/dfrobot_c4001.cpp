#include "dfrobot_c4001.h"
#include "esphome/core/log.h"
#include <string>
#include <cerrno>
#include <stdbool.h>

namespace esphome {
namespace dfrobot_c4001 {

static const char *TAG = "dfrobot_c4001";

/**
 * setup
 * Called once when the component is initialized.
 * We call update_config_param() to load device configuration and publish initial values.
 */
void c4001Component::setup() {
  update_config_param();
}

/**
 * print_config
 * Print current configuration values to the log for debugging.
 */
void c4001Component::print_config() {
  ESP_LOGD(TAG, "min_range_: %.1f m", min_range_);
  ESP_LOGD(TAG, "max_range_: %.1f m", max_range_);
  ESP_LOGD(TAG, "trig_range_: %.1f m", trig_range_);
  ESP_LOGD(TAG, "keep_sensitivity_: %d", keep_sensitivity_);
  ESP_LOGD(TAG, "trig_sensitivity_: %d", trig_sensitivity_);
  ESP_LOGD(TAG, "confirm_delay_: %.1f s", confirm_delay_);
  ESP_LOGD(TAG, "disappear_delay_: %.1f s", disappear_delay_);
  ESP_LOGD(TAG, "threshold_factor_: %d", threshold_factor_);
  ESP_LOGD(TAG, "micro_motion_: %d", micro_motion_);
  ESP_LOGD(TAG, "run_mode_: %d", run_mode_);
}

/**
 * loop
 * Main periodic loop called frequently by ESPHome.
 * We call get_data() every 1000 ms to read and parse UART data.
 */
void c4001Component::loop() {
  // Perform periodic tasks here
  static unsigned long last_time = 0;
  unsigned long now = millis();
  if (now - last_time >= 1000) {  // Execute every 1000ms
    last_time = now;
    get_data();
  }
}

/**
 * parse_dfhpd
 * Parse a $DFHPD line and return the first parameter after the comma (0 or 1).
 * Returns -1 if parsing fails or value is not 0/1.
 *
 * Example: "$DFHPD,1, , , *" -> returns 1
 */
int parse_dfhpd(const std::string &line) {
  // Check if contains $DFHPD
  if (line.find("$DFHPD") == std::string::npos) return -1;

  // Find first comma
  size_t pos1 = line.find(',');
  if (pos1 == std::string::npos) return -1;

  // Find second comma
  size_t pos2 = line.find(',', pos1 + 1);
  if (pos2 == std::string::npos) return -1;

  // Extract first field
  std::string param_str = line.substr(pos1 + 1, pos2 - pos1 - 1);

  // Remove surrounding spaces
  param_str.erase(0, param_str.find_first_not_of(" "));
  param_str.erase(param_str.find_last_not_of(" ") + 1);

  if (param_str.empty()) return -1;

  // Manually check for 0 or 1
  if (param_str == "0") return 0;
  if (param_str == "1") return 1;
  return -1;
}

/**
 * parse_dfdmd
 * Parse a $DFDMD speed-mode line and extract:
 *  - exist (tokens[1], 0/1)
 *  - distance (tokens[3], meters)
 *  - speed (tokens[4], m/s)
 *
 * Returns a MotData with valid=true on success.
 *
 * Example: "$DFDMD,1,1,4.885,-0.464,346, , *"
 */
MotData parse_dfdmd(const std::string &line) {
  MotData result{0, 0.0f, 0.0f, false};

  // Ensure this is a DFDMD line
  if (line.find("$DFDMD") == std::string::npos) {
    return result;
  }

  // Split string into tokens using comma delimiter
  std::vector<std::string> tokens;
  size_t start = 0;
  size_t end = line.find(',');
  while (end != std::string::npos) {
    tokens.push_back(line.substr(start, end - start));
    start = end + 1;
    end = line.find(',', start);
  }
  tokens.push_back(line.substr(start));

  // Ensure we have enough fields
  if (tokens.size() < 5) {
    return result;
  }

  // Safe conversion lambdas to avoid exceptions (embedded builds typically disable exceptions)
  auto safe_stoi = [](const std::string &s, int &out) -> bool {
    if (s.empty()) return false;
    char *endptr;
    out = strtol(s.c_str(), &endptr, 10);
    return (*endptr == '\0');
  };

  auto safe_stof = [](const std::string &s, float &out) -> bool {
    if (s.empty()) return false;
    char *endptr;
    out = strtof(s.c_str(), &endptr);
    return (*endptr == '\0');
  };

  int exist_tmp;
  float dist_tmp, speed_tmp;

  // Parse tokens[1] -> exist, tokens[3] -> distance, tokens[4] -> speed
  if (safe_stoi(tokens[1], exist_tmp) &&
      safe_stof(tokens[3], dist_tmp) &&
      safe_stof(tokens[4], speed_tmp)) {
    result.exist = exist_tmp;
    result.distance = dist_tmp;
    result.speed = speed_tmp;
    result.valid = true;
  }

  return result;
}

/**
 * get_data
 * Read UART and update internal state depending on current run mode.
 * - In MODE_MOTION: parse $DFHPD for exist flag.
 * - In MODE_SPEED: clear buffer and parse $DFDMD for exist/distance/speed.
 *
 * After parsing, update exist_, speed_, distance_ members.
 */
void c4001Component::get_data() {
  MotData data;
  if (run_mode_ == MODE_MOTION) {
    char buf[50];
    int len = uart_read_raw(buf, sizeof(buf) - 1, 100); // Read UART data
    if (len <= 0) {
      ESP_LOGW(TAG, "No data received from UART");
    }
    int result = parse_dfhpd(buf);
    if (result != -1) {
      exist_ = result;
    }
    ESP_LOGD(TAG, "motion = %d", result);

  } else if (run_mode_ == MODE_SPEED) {
    uart_clear_buffer();  // Clear buffer first to avoid stale data
    char buf[50];
    int len = uart_read_raw(buf, sizeof(buf) - 1, 100); // Read UART data
    if (len <= 0) {
      ESP_LOGW(TAG, "No data received from UART");
    }
    data = parse_dfdmd(buf);
    if (data.valid) {
      exist_ = data.exist;
      speed_ = data.speed;
      distance_ = data.distance;
    }
    ESP_LOGD(TAG, "motion = %d  speed %.2f distance = %.2f", data.exist, data.speed, data.distance);
  }
  for (auto &listener : this->listeners_) {
    listener->on_presence(exist_);
  }
  for (auto &listener : this->listeners_) {
    listener->on_distance(distance_);
    listener->on_speed(speed_);
  }
}

/**
 * update_config_param
 * Query device settings and update all configured number entities and other state.
 * This is typically called on setup or when settings change.
 */
void c4001Component::update_config_param() {
  exist_ = 0;
  speed_ = 0.0;
  distance_ = 0.0;
  int mode = get_run_mode();
  run_mode_ = mode;

  // Temporarily stop sensor while fetching settings
  sensor_stop();

  // Read settings from device
  int threshold = get_threshold_uart();
  int trig = get_trig_uart();
  SRange range = get_range_uart();
  DelResult delays = get_delay_uart();
  SenResult sensitivity = getSensitivity();
  int micro = get_micro_uart();

  // Update internal members with values from device
  min_range_ = range.min;
  max_range_ = range.max;
  trig_range_ = trig;
  keep_sensitivity_ = sensitivity.keep;
  trig_sensitivity_ = sensitivity.trig;
  confirm_delay_ = delays.confirm;
  disappear_delay_ = delays.disappear;
  threshold_factor_ = threshold;
  micro_motion_ = micro;

  // Restart sensor after configuration
  sensor_start();

  // Publish the retrieved values to Home Assistant number entities if they exist
  if (min_range_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing min_range_: %.2f", min_range_);
    min_range_number_->publish_state(min_range_);
  }

  if (max_range_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing max_range_: %.2f", max_range_);
    max_range_number_->publish_state(max_range_);
  }

  if (trig_range_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing trig_range_: %.2f", trig_range_);
    trig_range_number_->publish_state(trig_range_);
  }

  if (confirm_delay_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing confirm_delay_: %.2f", confirm_delay_);
    confirm_delay_number_->publish_state(confirm_delay_);
  }

  if (disappear_delay_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing disappear_delay_: %.2f", disappear_delay_);
    disappear_delay_number_->publish_state(disappear_delay_);
  }

  if (threshold_factor_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing threshold_factor_: %d", threshold_factor_);
    threshold_factor_number_->publish_state(threshold_factor_);
  }

  if (keep_sensitivity_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing keep_sensitivity_: %d", keep_sensitivity_);
    keep_sensitivity_number_->publish_state(keep_sensitivity_);
  }

  if (trig_sensitivity_number_ != nullptr) {
    ESP_LOGD(TAG, "Publishing trig_sensitivity_: %d", trig_sensitivity_);
    trig_sensitivity_number_->publish_state(trig_sensitivity_);
  }

  // Publish micro switch state if switch entity exists
  if (motion_switch_ != nullptr) {
    ESP_LOGD(TAG, "Publishing micro_motion_: %d", micro_motion_);
    motion_switch_->publish_state(micro_motion_);
  }

  // Publish operating mode to select entity if configured
  if (operating_selector_ != nullptr) {
    std::string mode_str;
    switch (run_mode_) {
      case 0:  // motion
        mode_str = "motion";
        break;
      case 1:  // speed
        mode_str = "speed";
        break;
      default:
        mode_str = "motion";
        break;
    }
    ESP_LOGD(TAG, "Publishing operating mode: %s", mode_str.c_str());
    operating_selector_->publish_state(mode_str);
  }
}

/**
 * uart_clear_buffer
 * Drain and discard any pending bytes from the UART RX buffer.
 * Useful to ensure subsequent read returns fresh data.
 */
void c4001Component::uart_clear_buffer() {
  uint8_t tmp[64];  // Temporary buffer
  while (this->available() > 0) {
    size_t toread = std::min(static_cast<size_t>(this->available()), sizeof(tmp));
    this->read_array(tmp, toread);  // Discard data
  }
  ESP_LOGD(TAG, "UART buffer cleared");
}

/**
 * uart_read_raw
 * Read raw bytes from UART into buf until timeout or buffer full.
 * Returns number of bytes written (excluding final NUL).
 *
 * Note: bufsize should be >= 2 (we reserve one byte for terminating NUL).
 */
size_t c4001Component::uart_read_raw(char *buf, size_t bufsize, uint32_t timeout_ms) {
  if (!buf || bufsize < 2) return 0;
  size_t idx = 0;
  uint32_t start = millis();
  buf[0] = '\0';
  while ((millis() - start) < timeout_ms && idx < bufsize - 1) {
    size_t avail = this->available();
    if (avail > 0) {
      size_t toread = std::min(avail, bufsize - 1 - idx);
      this->read_array(reinterpret_cast<uint8_t *>(buf + idx), toread);
      idx += toread;
      if (idx >= bufsize - 1) break;
      // Continue reading until timeout or buffer full
      continue;
    }
    // No data available, short delay
    delay(1);
  }
  buf[idx] = '\0';
  return idx;
}

/**
 * str_match
 * Helper to check whether 'pattern' exists inside 'text'.
 */
bool str_match(const std::string &text, const std::string &pattern) {
  return text.find(pattern) != std::string::npos;
}

/**
 * sensor_stop
 * Send a stop command to the device and check the response.
 * Returns true if device indicates it is already stopped.
 */
bool c4001Component::sensor_stop(void) {
  uint8_t len = 0;
  uart_clear_buffer();  // Clear buffer
  // Send stop command
  const char *cmd = "sensor_stop\r\n";
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  char buf[50];
  size_t n = uart_read_raw(buf, sizeof(buf), 200);
  if (str_match(buf, "sensor stopped already")) {
    // Match successful
    return true;
  }
  return false;
}

/**
 * sensor_start
 * Send a start command to the device.
 */
void c4001Component::sensor_start(void) {
  const char *cmd = "sensor_start\r\n";
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
}

/**
 * save_config
 * Send save_config command to persist device configuration.
 */
void c4001Component::save_config(void) {
  const char *cmd = "save_config\r\n";
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
}

/**
 * extract_two_numbers_after_keyword
 * Search for `keyword` in `buf` and parse up to two numeric values after it.
 * Returns the count of parsed numbers (0, 1 or 2). Parsed values are rounded to 1 decimal.
 *
 * Example: buf contains "... Response: 1.2, 3.4 ..." -> out1=1.2 out2=3.4 -> returns 2
 */
int extract_two_numbers_after_keyword(const std::string &buf,
                                      const std::string &keyword,
                                      float *out1, float *out2) {
  if (buf.empty() || keyword.empty()) return 0;

  const char *p = strstr(buf.c_str(), keyword.c_str());
  if (!p) return 0;

  p += keyword.size();

  while (*p == ' ' || *p == '\t' || *p == ':' || *p == '=') p++;

  errno = 0;
  char *endptr = nullptr;

  // First number
  double v1 = strtod(p, &endptr);
  if (p == endptr || errno == ERANGE) return 0;
  if (out1) *out1 = floorf(static_cast<float>(v1) * 10.0f + 0.5f) / 10.0f;

  p = endptr;
  while (*p == ' ' || *p == '\t' || *p == ',') p++;

  if (*p == '\0') {
    return 1;  // Only one parameter parsed
  }

  // Second number
  errno = 0;
  double v2 = strtod(p, &endptr);
  if (p == endptr || errno == ERANGE) return 1;
  if (out2) *out2 = floorf(static_cast<float>(v2) * 10.0f + 0.5f) / 10.0f;

  return 2;
}

/**
 * parse_mode
 * Determine mode by checking the presence of $DFDMD in the line.
 * If found -> MODE_SPEED, otherwise MODE_MOTION.
 */
MotionMode parse_mode(const std::string &line) {
  if (line.find("$DFDMD") != std::string::npos) {
    return MODE_SPEED;
  }
  return MODE_MOTION;
}

/**
 * get_run_mode
 * Read a single UART line and use parse_mode() to determine the current run mode.
 * Returns: 1 for speed, 0 for motion, -1 for unknown/error.
 */
int c4001Component::get_run_mode() {
  char buf[100];
  uart_clear_buffer();  // Clear buffer
  int len = uart_read_raw(buf, sizeof(buf) - 1, 400); // Read UART data
  if (len <= 0) {
    ESP_LOGW(TAG, "No data received from UART");
    return -1; // Unknown mode
  }

  buf[len] = 0; // Ensure null-terminated string

  std::string line(buf);
  ESP_LOGD(TAG, "UART line: %s", line.c_str());

  // Use previously defined parse_mode() function
  MotionMode mode = parse_mode(line);

  switch (mode) {
    case MODE_SPEED:
      ESP_LOGD(TAG, "Detected speed mode");
      return 1; // Return speed
    case MODE_MOTION:
      ESP_LOGD(TAG, "Detected motion mode");
      return 0; // Return motion
    default:
      ESP_LOGW(TAG, "Unknown mode");
      return -1; // Unknown
  }
}

/**
 * getSensitivity
 * Query device for sensitivity settings and return them in SenResult.
 */
SenResult c4001Component::getSensitivity() {
<<<<<<< HEAD
  SenResult result{0, 0};
  float val1 = 0.0, val2 = 0.0;
=======
sSensitivity_t c4001Component::getSensitivity() {
  sSensitivity_t result{0, 0};
  float val1 = 0.0, val2 = 0.0;
>>>>>>> a8f3455730261805ada59c777b5e77763ad17173
=======
  SenResult result{0,0};
  float val1=0.0, val2=0.0;
>>>>>>> 31ed362f2 (update c4001)
  const char *cmd = "getSensitivity\r\n";
  char buf[100];
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  uart_read_raw(buf, sizeof(buf), 100);
  int n = extract_two_numbers_after_keyword(buf, "Response", &val1, &val2);
  ESP_LOGD(TAG, "sensitivity_: %f %f", val1, val2);

  if(n >= 1){
    result.keep = (int)val1;
  }
  if(n >= 2){
    result.trig = (int)val2;
  }
  ESP_LOGD(TAG, "sensitivity_: %d %d", result.keep, result.trig);
  return result;
}

/**
 * get_trig_uart
 * Query device for trigger range value.
 */
float c4001Component::get_trig_uart() {
<<<<<<< HEAD
  float val1 = 0.0, val2 = 0.0;
=======
float c4001Component::getRangeTrig() {
  float val1 = 0.0, val2 = 0.0;
>>>>>>> a8f3455730261805ada59c777b5e77763ad17173
=======
  float val1=0.0, val2=0.0;
>>>>>>> 31ed362f2 (update c4001)
  float result = 0.0f;
  const char *cmd = "getTrigRange\r\n";
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  char buf[100];
  uart_read_raw(buf, sizeof(buf), 100);
  int n = extract_two_numbers_after_keyword(buf, "Response", &val1, &val2);
  if(n >= 1){
    result = val1;
  }
  ESP_LOGD(TAG, "trig range: %f", result);
  return result;
}

/**
 * get_range_uart
 * Query device for min/max detection range.
 */
SRange c4001Component::get_range_uart() {
  SRange range{0, 0};
  float val1 = 0.0f, val2 = 0.0f;
  const char *cmd = "get_range_uart\r\n";
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  char buf[100];
  uart_read_raw(buf, sizeof(buf), 100);
  int n = extract_two_numbers_after_keyword(buf, "Response", &val1, &val2);
  if(n >= 1){
    range.min = val1;
  }
  if(n >= 2){
    range.max = val2;
  }
  return range;
}

/**
 * get_delay_uart
 * Query device for confirm and disappear delays.
 */
DelResult c4001Component::get_delay_uart() {
  DelResult result{0, 0};
  float val1 = 0.0, val2 = 0.0;
  const char *cmd = "getLatency\r\n";
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  char buf[100];
  uart_read_raw(buf, sizeof(buf), 100);
  int n = extract_two_numbers_after_keyword(buf, "Response", &val1, &val2);
  if(n >= 1){
    result.confirm = val1;
  }
  if(n >= 2){
    result.disappear = val2;
  }
  return result;
}

/**
 * get_threshold_uart
 * Query device for threshold factor (integer).
 */
int c4001Component::get_threshold_uart() {
  float val1 = 0.0, val2 = 0.0;
  int result = 0.0f;
  const char *cmd = "getThrFactor\r\n";
  char buf[100];
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  uart_read_raw(buf, sizeof(buf), 100);
  int n = extract_two_numbers_after_keyword(buf, "Response", &val1, &val2);
  if(n >= 1){
    result = (int)val1;
  }
  return result;
}

/**
 * get_micro_uart
 * Query device for micro-motion (microswitch) state.
 */
int c4001Component::get_micro_uart() {
  float val1 = 0.0, val2 = 0.0;
  int result = 0.0f;
  const char *cmd = "getMicroMotion\r\n";
  char buf[100];
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));
  uart_read_raw(buf, sizeof(buf), 100);
  int n = extract_two_numbers_after_keyword(buf, "Response", &val1, &val2);
  if(n >= 1){
    result = (int)val1;
  }
  return result;
}

/**
 * send_cmd_with_param
 * Helper to send a command string (cmd should already include parameters and CRLF).
 * This will stop the sensor, write the command, attempt a read to clear the response,
 * save config and then restart the sensor.
 */
void c4001Component::send_cmd_with_param(const char *cmd) {
  sensor_stop();

  // Send command
  this->write_array(reinterpret_cast<const uint8_t *>(cmd), strlen(cmd));

  // Read data (just to clear buffer)
  char buf[100] = {0};
  size_t n = uart_read_raw(buf, sizeof(buf), 100);
  save_config();
  sensor_start();
}

/**
 * set_min_range / set_max_range / set_trig_range
 * Setter functions that update internal state and send corresponding device commands.
 * Each uses send_cmd_with_param() to perform the command round trip.
 */
void c4001Component::set_min_range(float value) {
  if (value > max_range_) {
    ESP_LOGW(TAG, "Min range %.1f is greater than current max range %.1f, ignoring", value, max_range_);
    return;
  }
  min_range_ = value;
  const char *cmd = "setRange";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %.1f %.1f\r\n", cmd, min_range_, max_range_);
  send_cmd_with_param(full_cmd);
}

void c4001Component::set_max_range(float value) {
  if (value < min_range_) {
    ESP_LOGW(TAG, "Max range %.1f is less than current min range %.1f, ignoring", value, min_range_);
    return;
  }
  max_range_ = value;
  const char *cmd = "setRange";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %.1f %.1f\r\n", cmd, min_range_, max_range_);
  send_cmd_with_param(full_cmd);
}

void c4001Component::set_trig_range(float value) {
  trig_range_ = value;
  ESP_LOGD(TAG, "Trig range set to %.1f", value);
  const char *cmd = "setTrigRange";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %.1f\r\n", cmd, trig_range_);
  send_cmd_with_param(full_cmd);
}

// Set keep sensitivity
void c4001Component::set_keep_sensitivity(int value) {
  keep_sensitivity_ = value;
  const char *cmd = "setSensitivity";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %d %d\r\n", cmd, keep_sensitivity_, trig_sensitivity_);
  send_cmd_with_param(full_cmd);
  ESP_LOGD(TAG, "Keep sensitivity set to %d", value);
}

// Set trigger sensitivity
void c4001Component::set_trig_sensitivity(int value) {
  trig_sensitivity_ = value;
  const char *cmd = "setSensitivity";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %d %d\r\n", cmd, keep_sensitivity_, trig_sensitivity_);

  send_cmd_with_param(full_cmd);
  ESP_LOGD(TAG, "Trig sensitivity set to %d", value);
}

// Set confirmation delay (in seconds)
void c4001Component::set_confirm_delay(float value) {
  confirm_delay_ = value;
  const char *cmd = "setLatency";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %.1f %.1f\r\n", cmd, confirm_delay_, disappear_delay_);
  send_cmd_with_param(full_cmd);
  ESP_LOGD(TAG, "Confirm delay set to %.1f s", value);
}

// Set disappearance delay (in seconds)
void c4001Component::set_disappear_delay(float value) {
  disappear_delay_ = value;
  const char *cmd = "setLatency";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %.1f %.1f\r\n", cmd, confirm_delay_, disappear_delay_);
  send_cmd_with_param(full_cmd);
  ESP_LOGD(TAG, "Disappear delay set to %.1f s", value);
}

// Set threshold factor
void c4001Component::set_threshold_factor(int value) {
  threshold_factor_ = value;
  const char *cmd = "setThrFactor";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %d\r\n", cmd, threshold_factor_);
  send_cmd_with_param(full_cmd);
  ESP_LOGD(TAG, "Threshold factor set to %d", value);
}

/**
 * set_operating_mode
 * Convert a string ("motion" or "speed") into the device run-mode and send command.
 * After changing mode, re-read configuration via update_config_param().
 */
void c4001Component::set_operating_mode(const std::string &state) {
  int value = (state == "motion") ? 0 : 1;
  const char *cmd = "setRunApp";
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %d\r\n", cmd, value);
  send_cmd_with_param(full_cmd);
  if(value != run_mode_) {
    update_config_param();
    run_mode_ = value;
  }

  ESP_LOGD(TAG, "set_operating_mode: %s (len=%zu)", state.c_str(), state.size());
}

/**
 * set_micro_switch_state
 * Toggle micro-motion hardware setting and send command to device.
 */
void c4001Component::set_micro_switch_state(bool state) {
  // Implement hardware control logic here
  const char *cmd = "setMicroMotion";
  int value;
  value = state ? 1 : 0;
  micro_motion_ = value;
  char full_cmd[64];
  snprintf(full_cmd, sizeof(full_cmd), "%s %d\r\n", cmd, micro_motion_);
  send_cmd_with_param(full_cmd);
  ESP_LOGD("dfrobot_c4001", "Setting micro switch to %s", state ? "ON" : "OFF");
}


}  // namespace dfrobot_c4001
}  // namespace esphome
