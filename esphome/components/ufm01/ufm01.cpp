#include "ufm01.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>

namespace esphome::ufm01 {

ESPHOME_LOG_TAG(TAG, "ufm01");

static constexpr uint8_t COMMAND_ACK = 0xE5;
static constexpr uint32_t COMMAND_ACK_TIMEOUT_MS = 500;
static constexpr uint32_t CLEAR_QUEUE_TIMEOUT_MS = 15000;
static constexpr uint32_t STARTUP_DELAY_MS = 2000;
static constexpr uint32_t POST_RESET_DELAY_MS = 2000;
static constexpr uint32_t RESET_RETRY_DELAY_MS = 800;
static constexpr uint32_t STARTUP_RETRY_MS = 3000;
static constexpr uint32_t PASSIVE_POLL_INTERVAL_MS = 1000;
static constexpr uint32_t ACTIVE_STALE_MS = 5000;
static constexpr uint32_t PASSIVE_READ_TIMEOUT_MS = 1000;
static constexpr uint32_t SOFTWARE_VERSION_READ_TIMEOUT_MS = 2000;
static constexpr uint32_t SOFTWARE_VERSION_RETRY_MS = 30000;
static constexpr uint8_t SOFTWARE_VERSION_MAX_ATTEMPTS = 3;
static constexpr uint32_t ACTIVE_FRAME_TIMEOUT_MS = 3000;
// After this many consecutive passive poll failures, re-run the reset/startup sequence
static constexpr uint8_t PASSIVE_FAIL_ESCALATE_COUNT = 8;

static constexpr float L_PER_M3 = 1000.0f;
static constexpr float M3_PER_L = 1.0f / L_PER_M3;

static constexpr std::array<uint8_t, 7> ACTIVE_MODE = {0xFE, 0xFE, 0x11, 0x5C, 0x00, 0x5C, 0x16};
static constexpr std::array<uint8_t, 7> PASSIVE_MODE = {0xFE, 0xFE, 0x11, 0x5C, 0x01, 0x5D, 0x16};
static constexpr std::array<uint8_t, 7> CLEAR_ACCUMULATED_FLOW = {0xFE, 0xFE, 0x11, 0x5A, 0xFD, 0x57, 0x16};
static constexpr std::array<uint8_t, 7> RESET_DEVICE = {0xFE, 0xFE, 0x11, 0x5D, 0xCB, 0x28, 0x16};
static constexpr std::array<uint8_t, 7> READ_SENSOR_DATA_NO_ID = {0xFE, 0xFE, 0x11, 0x5B, 0x0F, 0x6A, 0x16};
static constexpr std::array<uint8_t, 7> READ_SENSOR_DATA_WITH_ID = {0xFE, 0xFE, 0x11, 0x5B, 0xCB, 0x26, 0x16};
static constexpr std::array<uint8_t, 7> GET_SOFTWARE_VERSION = {0xFE, 0xFE, 0x11, 0x5E, 0x62, 0xC0, 0x16};

// Active-mode frame layout (datasheet Table 7)
static constexpr size_t FRAME_DEVICE_ID_INDEX = 2;
static constexpr size_t FRAME_CHECKSUM_INDEX = 30;
static constexpr size_t FRAME_STOP_INDEX = 31;
static constexpr uint8_t PASSIVE_START_BYTE_2 = 0x64;
static constexpr uint8_t PASSIVE_START_BYTE_2_WITH_ID = 0x96;
static constexpr uint8_t FRAME_STOP_BYTE = 0x16;
static constexpr size_t PASSIVE_WITH_ID_DEVICE_ID_INDEX = 2;
static constexpr uint8_t PASSIVE_WITH_ID_ACC_FLOW_FLAG_INDEX = 8;
static constexpr uint8_t PASSIVE_WITH_ID_INSTANT_FLOW_FLAG_INDEX = 22;
static constexpr uint8_t PASSIVE_WITH_ID_TEMP_FLAG_INDEX = 31;
static constexpr uint8_t PASSIVE_WITH_ID_ST1_INDEX = 35;
static constexpr size_t PASSIVE_WITH_ID_CHECKSUM_INDEX = 37;
static constexpr size_t PASSIVE_WITH_ID_STOP_INDEX = 38;
static constexpr uint8_t FRAME_INDEX_INSTANT_FLOW_FLAG = 15;
static constexpr uint8_t FRAME_INDEX_RESERVED_SECTION = 21;
static constexpr uint8_t FRAME_INDEX_TEMP_FLAG = 24;
static constexpr uint8_t FRAME_FLAG_INSTANT_FLOW = 0x0B;
static constexpr uint8_t FRAME_FLAG_RESERVED_SECTION = 0x0C;
static constexpr uint8_t FRAME_FLAG_TEMP = 0x0D;

// Measurement decoding
static constexpr uint8_t FRAME_ACC_FLOW_FLAG_INDEX = 8;
static constexpr uint8_t ACC_FLOW_M3_FLAG = 0x1A;
static constexpr uint8_t FRAME_FLOW_SIGN_INDEX = 20;
static constexpr uint8_t FLOW_NEGATIVE_SIGN = 0x80;

// Status bytes (datasheet ST1 / ST2)
static constexpr uint8_t FRAME_ST1_INDEX = 28;
static constexpr uint8_t FRAME_ST2_INDEX = 29;
static constexpr uint8_t ST1_EMPTY_TUBE_MASK = 0x20;
static constexpr uint8_t ST2_UFC_ERROR_MASK = 0x20;
static constexpr uint8_t ST2_FLOW_DIRECTION_WRONG_MASK = 0x08;
static constexpr uint8_t ST2_FLOW_RATE_OUT_OF_RANGE_MASK = 0x04;

static_assert(SOFTWARE_VERSION_RESPONSE_SIZE <= PASSIVE_FRAME_MAX_SIZE, "software version read reuses passive_frame_");
static constexpr size_t SOFTWARE_VERSION_CHECKSUM_INDEX = 5;
static constexpr size_t SOFTWARE_VERSION_STOP_INDEX = 6;

static float to_float(uint8_t data) { return bcd_to_uint8(data); }

static bool bcd_to_digits(const uint8_t *data, size_t len, char *out) {
  bool all_zero = true;
  for (size_t i = 0; i < len; ++i) {
    const uint8_t lo = data[i] & 0x0F;
    const uint8_t hi = data[i] >> 4;
    if (lo > 9 || hi > 9)
      return false;
    const size_t pos = (len - 1 - i) * 2;
    if (hi != 0)
      all_zero = false;
    out[pos] = static_cast<char>('0' + hi);
    if (lo != 0)
      all_zero = false;
    out[pos + 1] = static_cast<char>('0' + lo);
  }
  out[len * 2] = '\0';
  return !all_zero;
}

static bool validate_software_version_response(const uint8_t data[SOFTWARE_VERSION_RESPONSE_SIZE]) {
  if (data[0] != COMMAND_ACK || data[SOFTWARE_VERSION_STOP_INDEX] != FRAME_STOP_BYTE)
    return false;
  uint8_t sum = 0;
  for (size_t i = 1; i < SOFTWARE_VERSION_CHECKSUM_INDEX; ++i)
    sum += data[i];
  return data[SOFTWARE_VERSION_CHECKSUM_INDEX] == sum;
}

static bool check_byte(const uint8_t data[FRAME_SIZE], size_t index, uint8_t expected, const char *name) {
  if (data[index] == expected)
    return true;
  ESP_LOGW(TAG, "%s (byte %zu) - expected 0x%02X, but was 0x%02X", name, index, expected, data[index]);
  return false;
}

static bool validate_active_frame(const uint8_t data[FRAME_SIZE]) {
  const uint8_t sum = sum_bytes(data, FRAME_CHECKSUM_INDEX);
  return check_byte(data, 0, FRAME_START_BYTE_1, "start byte 1") &&
         check_byte(data, 1, FRAME_START_BYTE_2, "start byte 2") &&
         check_byte(data, FRAME_INDEX_INSTANT_FLOW_FLAG, FRAME_FLAG_INSTANT_FLOW, "instant flow flag") &&
         check_byte(data, FRAME_INDEX_RESERVED_SECTION, FRAME_FLAG_RESERVED_SECTION, "reserved section flag") &&
         check_byte(data, FRAME_INDEX_TEMP_FLAG, FRAME_FLAG_TEMP, "temperature flag") &&
         check_byte(data, FRAME_CHECKSUM_INDEX, sum, "checksum") &&
         check_byte(data, FRAME_STOP_INDEX, FRAME_STOP_BYTE, "stop byte");
}

static bool validate_passive_frame(const uint8_t data[PASSIVE_FRAME_SIZE]) {
  if (data[0] != FRAME_START_BYTE_1 || data[1] != PASSIVE_START_BYTE_2 || data[22] != FRAME_STOP_BYTE)
    return false;
  return data[21] == sum_bytes(data, 21);
}

#ifdef USE_UFM01_METER_ID
static bool validate_passive_with_id_frame(const uint8_t data[PASSIVE_FRAME_WITH_ID_SIZE]) {
  if (data[0] != FRAME_START_BYTE_1 || data[1] != PASSIVE_START_BYTE_2_WITH_ID ||
      data[PASSIVE_WITH_ID_STOP_INDEX] != FRAME_STOP_BYTE)
    return false;
  uint8_t sum = 0;
  for (size_t i = 0; i < PASSIVE_WITH_ID_CHECKSUM_INDEX; ++i)
    sum += data[i];
  return data[PASSIVE_WITH_ID_CHECKSUM_INDEX] == sum;
}
#endif

// Start index of each run in a passive reply; each run maps to the matching run of the active frame
struct PassiveLayout {
  uint8_t acc_flow;      // flag + 6 bytes, active 8..14
  uint8_t instant_flow;  // flag + 5 bytes, active 15..20
  uint8_t temperature;   // flag + 3 bytes, active 24..27
  uint8_t status;        // ST1, ST2, checksum, active 28..30
};
static constexpr PassiveLayout PASSIVE_NO_ID_LAYOUT{2, 9, 15, 19};
#ifdef USE_UFM01_METER_ID
static constexpr PassiveLayout PASSIVE_WITH_ID_LAYOUT{PASSIVE_WITH_ID_ACC_FLOW_FLAG_INDEX,
                                                      PASSIVE_WITH_ID_INSTANT_FLOW_FLAG_INDEX,
                                                      PASSIVE_WITH_ID_TEMP_FLAG_INDEX, PASSIVE_WITH_ID_ST1_INDEX};
#endif

static void passive_to_active_frame(const uint8_t *passive, const PassiveLayout &layout, uint8_t active[FRAME_SIZE]) {
  std::memset(active, 0, FRAME_SIZE);
  active[0] = FRAME_START_BYTE_1;
  active[1] = FRAME_START_BYTE_2;
  std::copy_n(&passive[layout.acc_flow], 7, &active[FRAME_ACC_FLOW_FLAG_INDEX]);
  std::copy_n(&passive[layout.instant_flow], 6, &active[FRAME_INDEX_INSTANT_FLOW_FLAG]);
  active[FRAME_INDEX_RESERVED_SECTION] = FRAME_FLAG_RESERVED_SECTION;
  std::copy_n(&passive[layout.temperature], 4, &active[FRAME_INDEX_TEMP_FLAG]);
  std::copy_n(&passive[layout.status], 3, &active[FRAME_ST1_INDEX]);
  active[FRAME_STOP_INDEX] = FRAME_STOP_BYTE;
}

static float read_accumulated_flow(const uint8_t data[FRAME_SIZE]) {
  return (data[FRAME_ACC_FLOW_FLAG_INDEX] == ACC_FLOW_M3_FLAG ? L_PER_M3 : 1.0f) *
         (to_float(data[14]) * 10000000.0f + to_float(data[13]) * 100000.0f + to_float(data[12]) * 1000.0f +
          to_float(data[11]) * 10.0f + to_float(data[10]) * 0.1f + to_float(data[9]) * 0.001f);
}

static float read_flow(const uint8_t data[FRAME_SIZE]) {
  return (data[FRAME_FLOW_SIGN_INDEX] == FLOW_NEGATIVE_SIGN ? -1.0f : 1.0f) *
         (to_float(data[19]) * 10000.0f + to_float(data[18]) * 100.0f + to_float(data[17]) +
          to_float(data[16]) * 0.01f) *
         M3_PER_L;
}

static constexpr size_t LOG_HEX_MAX_SIZE = std::max(FRAME_SIZE, PASSIVE_FRAME_MAX_SIZE);

static void log_hex(const uint8_t *data, size_t len) {
  char hex_buf[format_hex_pretty_size(LOG_HEX_MAX_SIZE)];
  ESP_LOGD(TAG, "%s", format_hex_pretty_to(hex_buf, data, len, ' '));
}

static float read_temperature(const uint8_t data[FRAME_SIZE]) {
  // happens sometimes before getting a real reading
  if (data[27] == 0x00 && (data[26] == 0x00 || data[26] == 0x70) && data[25] == 0x00) {
    return NAN;
  }
  return to_float(data[27]) * 100.0f + to_float(data[26]) + to_float(data[25]) * 0.01f;
}

static bool read_ufc_chip_error(const uint8_t data[FRAME_SIZE]) { return data[FRAME_ST2_INDEX] & ST2_UFC_ERROR_MASK; }

static bool read_flow_direction_wrong(const uint8_t data[FRAME_SIZE]) {
  return data[FRAME_ST2_INDEX] & ST2_FLOW_DIRECTION_WRONG_MASK;
}

static bool read_empty_tube(const uint8_t data[FRAME_SIZE]) { return data[FRAME_ST1_INDEX] & ST1_EMPTY_TUBE_MASK; }

static bool read_flow_rate_out_of_range(const uint8_t data[FRAME_SIZE]) {
  return data[FRAME_ST2_INDEX] & ST2_FLOW_RATE_OUT_OF_RANGE_MASK;
}

void UFM01Component::flush_rx_() {
  this->discard_available();
  this->active_reader_.reset();
}

void UFM01Component::send_command_no_wait_(const std::array<uint8_t, 7> &command) {
  this->flush_rx_();
  this->write_array(command);
  this->flush();
}

bool UFM01Component::consume_ack_() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      return false;
    if (byte == COMMAND_ACK)
      return true;
    ESP_LOGV(TAG, "Unexpected byte while waiting for command ACK: 0x%02X", byte);
  }
  return false;
}

#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
bool UFM01Component::can_start_clear_action_() const {
  return
#ifdef USE_UFM01_SOFTWARE_VERSION
      !this->software_version_read_pending_ &&
#endif
      (this->operating_mode_ == OperatingMode::ACTIVE_STREAM ||
       (this->operating_mode_ == OperatingMode::PASSIVE_POLL && !this->passive_read_pending_));
}

void UFM01Component::request_clear_accumulated_flow() {
  if (this->clear_pending_) {
    ESP_LOGD(TAG, "Clear accumulated flow already pending");
    return;
  }
  this->clear_pending_ = true;
  this->pending_clear_sent_ = false;
  this->pending_clear_start_ms_ = App.get_loop_component_start_time();
}

void UFM01Component::finish_pending_clear_() {
  this->clear_pending_ = false;
  this->pending_clear_sent_ = false;
}

void UFM01Component::loop_pending_clear_action_(uint32_t now) {
  if (!this->clear_pending_)
    return;
  if (!this->pending_clear_sent_) {
    if (!this->can_start_clear_action_()) {
      if (now - this->pending_clear_start_ms_ < CLEAR_QUEUE_TIMEOUT_MS)
        return;
      ESP_LOGW(TAG, "Clear accumulated flow timed out waiting for an idle UART");
      this->finish_pending_clear_();
      return;
    }
    this->pending_clear_start_ms_ = now;
    this->pending_clear_sent_ = true;
    this->send_command_no_wait_(CLEAR_ACCUMULATED_FLOW);
    return;
  }
  if (this->consume_ack_()) {
    ESP_LOGI(TAG, "Clear accumulated flow acknowledged");
    this->finish_pending_clear_();
    return;
  }
  if (now - this->pending_clear_start_ms_ < COMMAND_ACK_TIMEOUT_MS)
    return;
  ESP_LOGW(TAG, "Clear accumulated flow not acknowledged");
  this->finish_pending_clear_();
}
#endif  // USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION

float UFM01Component::get_setup_priority() const { return setup_priority::LATE; }

void UFM01Component::setup() {
  ESP_LOGI(TAG, "Setting up UFM-01...");
  this->startup_wait_ms_ = STARTUP_DELAY_MS;
  this->set_startup_phase_(StartupPhase::WAIT, App.get_loop_component_start_time());
}

void UFM01Component::dump_config() {
  ESP_LOGCONFIG(TAG, "UFM-01:");
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Accumulated Flow", this->accumulated_flow_sensor_);
  LOG_SENSOR("  ", "Flow", this->flow_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "UFC Chip Error", this->ufc_chip_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Flow Direction Wrong", this->flow_direction_wrong_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Empty Tube", this->empty_tube_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Flow Rate Out Of Range", this->flow_rate_out_of_range_binary_sensor_);
#endif
#ifdef USE_UFM01_METER_ID
  LOG_TEXT_SENSOR("  ", "Meter ID", this->meter_id_text_sensor_);
#endif
#ifdef USE_UFM01_SOFTWARE_VERSION
  LOG_TEXT_SENSOR("  ", "Software Version", this->software_version_text_sensor_);
#endif
}

// The binary sensors keep their last state while readings are stale; only flow and temperature go unknown
void UFM01Component::publish_stale_flow_and_temperature_() {
#ifdef USE_SENSOR
  if (this->flow_sensor_ != nullptr)
    this->flow_sensor_->publish_state(NAN);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(NAN);
#endif
}

#ifdef USE_UFM01_METER_ID
void UFM01Component::publish_meter_id_from_frame_(const uint8_t data[FRAME_SIZE]) {
  if (this->meter_id_text_sensor_ == nullptr || this->meter_id_published_)
    return;

  char meter_id_str[DEVICE_ID_STRING_LENGTH + 1];
  if (!bcd_to_digits(&data[FRAME_DEVICE_ID_INDEX], DEVICE_ID_LENGTH, meter_id_str)) {
    if (!this->meter_id_invalid_logged_) {
      ESP_LOGW(TAG, "Meter ID in frame is zero or not BCD, leaving the sensor unknown");
      this->meter_id_invalid_logged_ = true;
    }
    return;
  }

  this->meter_id_text_sensor_->publish_state(meter_id_str);
  this->meter_id_published_ = true;
  ESP_LOGI(TAG, "UFM-01 meter ID: %s", meter_id_str);
}
#endif

#ifdef USE_UFM01_SOFTWARE_VERSION
void UFM01Component::start_software_version_read_(uint32_t now) {
  this->send_command_no_wait_(GET_SOFTWARE_VERSION);
  this->passive_index_ = 0;
  this->passive_start_ms_ = now;
  this->software_version_read_pending_ = true;
}

ReadResult UFM01Component::continue_software_version_read_(uint32_t now) {
  while (this->available() && this->passive_index_ < SOFTWARE_VERSION_RESPONSE_SIZE) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    // The tail of an in-flight active-stream frame can arrive before the reply
    if (this->passive_index_ == 0 && byte != COMMAND_ACK)
      continue;
    this->passive_frame_[this->passive_index_++] = byte;
  }

  if (this->passive_index_ < SOFTWARE_VERSION_RESPONSE_SIZE) {
    if (now - this->passive_start_ms_ < SOFTWARE_VERSION_READ_TIMEOUT_MS)
      return ReadResult::READ_RESULT_PENDING;
    ESP_LOGD(TAG, "software version read timeout (%u/%u bytes)", this->passive_index_,
             static_cast<unsigned>(SOFTWARE_VERSION_RESPONSE_SIZE));
    return ReadResult::READ_RESULT_FAILURE;
  }

  if (!validate_software_version_response(this->passive_frame_)) {
    log_hex(this->passive_frame_, SOFTWARE_VERSION_RESPONSE_SIZE);
    ESP_LOGW(TAG, "invalid software version response");
    return ReadResult::READ_RESULT_FAILURE;
  }

  if (this->software_version_text_sensor_ != nullptr && !this->software_version_published_) {
    char version_str[SOFTWARE_VERSION_STRING_LENGTH + 1];
    if (!bcd_to_digits(&this->passive_frame_[1], SOFTWARE_VERSION_LENGTH, version_str)) {
      ESP_LOGW(TAG, "invalid BCD data in software version response");
      return ReadResult::READ_RESULT_FAILURE;
    }
    this->software_version_text_sensor_->publish_state(version_str);
    this->software_version_published_ = true;
    ESP_LOGI(TAG, "UFM-01 software version: %s", version_str);
  }

  return ReadResult::READ_RESULT_SUCCESS;
}

void UFM01Component::finish_software_version_read_(ReadResult result, uint32_t now) {
  this->software_version_read_pending_ = false;
  this->last_software_version_attempt_ms_ = now;
  if (result == ReadResult::READ_RESULT_FAILURE &&
      ++this->software_version_failures_ == SOFTWARE_VERSION_MAX_ATTEMPTS) {
    ESP_LOGW(TAG, "Software version not read after %u attempts, giving up", SOFTWARE_VERSION_MAX_ATTEMPTS);
  }
}

// Returns true while a software version read owns the UART, so the caller skips its own work
bool UFM01Component::service_software_version_(uint32_t now) {
  if (this->software_version_read_pending_) {
    const ReadResult result = this->continue_software_version_read_(now);
    if (result != ReadResult::READ_RESULT_PENDING)
      this->finish_software_version_read_(result, now);
    return true;
  }
  if (this->software_version_text_sensor_ == nullptr || this->software_version_published_ ||
      this->software_version_failures_ >= SOFTWARE_VERSION_MAX_ATTEMPTS || this->passive_read_pending_)
    return false;
  if (this->last_software_version_attempt_ms_ != 0 &&
      now - this->last_software_version_attempt_ms_ < SOFTWARE_VERSION_RETRY_MS)
    return false;
  this->start_software_version_read_(now);
  return true;
}
#endif  // USE_UFM01_SOFTWARE_VERSION

void UFM01Component::on_active_frame_(const uint8_t data[FRAME_SIZE], uint32_t now) {
#ifdef USE_UFM01_METER_ID
  this->publish_meter_id_from_frame_(data);
#endif
  bool empty_tube = read_empty_tube(data);
#ifdef USE_BINARY_SENSOR
  if (this->ufc_chip_error_binary_sensor_ != nullptr)
    this->ufc_chip_error_binary_sensor_->publish_state(read_ufc_chip_error(data));
  if (this->flow_direction_wrong_binary_sensor_ != nullptr)
    this->flow_direction_wrong_binary_sensor_->publish_state(read_flow_direction_wrong(data));
  if (this->empty_tube_binary_sensor_ != nullptr)
    this->empty_tube_binary_sensor_->publish_state(empty_tube);
  if (this->flow_rate_out_of_range_binary_sensor_ != nullptr)
    this->flow_rate_out_of_range_binary_sensor_->publish_state(read_flow_rate_out_of_range(data));
#endif

#ifdef USE_SENSOR
  // Total volume remains valid when the tube is dry; flow and temperature are not.
  if (this->accumulated_flow_sensor_ != nullptr)
    this->accumulated_flow_sensor_->publish_state(read_accumulated_flow(data));

  if (empty_tube) {
    this->publish_stale_flow_and_temperature_();
  } else {
    if (this->flow_sensor_ != nullptr)
      this->flow_sensor_->publish_state(read_flow(data));
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(read_temperature(data));
  }
#endif
  this->last_valid_frame_ms_ = now;
  this->status_clear_warning();
  this->status_clear_error();
}

bool UFM01Component::process_active_stream_(uint32_t now) {
  bool got_valid_frame = false;
  const bool read_ok = this->read_available([this, now, &got_valid_frame](uint8_t byte) {
    const uint8_t *frame = this->active_reader_.feed(byte);
    if (frame == nullptr) {
      if (this->active_reader_.length() == 0)
        ESP_LOGD(TAG, "not start of data (is 0x%02X)", byte);
      return;
    }
    if (validate_active_frame(frame)) {
      this->on_active_frame_(frame, now);
      got_valid_frame = true;
      return;
    }
    log_hex(frame, FRAME_SIZE);
    ESP_LOGW(TAG, "unable to read data");
    this->active_reader_.resync();
  });
  if (!read_ok) {
    ESP_LOGW(TAG, "unable to read byte");
    this->active_reader_.reset();
  }

  return got_valid_frame;
}

void UFM01Component::set_mode_(OperatingMode mode, uint32_t now) {
  this->operating_mode_ = mode;
  this->phase_start_ms_ = now;
  this->passive_read_pending_ = false;
#ifdef USE_UFM01_SOFTWARE_VERSION
  this->software_version_read_pending_ = false;
#endif
  this->consecutive_passive_failures_ = 0;
}

void UFM01Component::set_startup_phase_(StartupPhase phase, uint32_t now) {
  this->startup_phase_ = phase;
  this->phase_start_ms_ = now;
}

void UFM01Component::send_reset_(const LogString *status, uint32_t now) {
  this->status_set_warning(status);
  this->reset_retried_ = false;
  this->send_command_no_wait_(RESET_DEVICE);
  this->set_startup_phase_(StartupPhase::RESET_WAIT_ACK, now);
}

void UFM01Component::enter_active_stream_(const LogString *reason, uint32_t now) {
  ESP_LOGI(TAG, "UFM-01 active stream %s", LOG_STR_ARG(reason));
  this->set_mode_(OperatingMode::ACTIVE_STREAM, now);
}

void UFM01Component::enter_passive_from_stale_(uint32_t now) {
  ESP_LOGW(TAG, "Active stream stale, switching to passive polling");
  this->publish_stale_flow_and_temperature_();
  // Flushes leftover stream bytes first; the ACK is not awaited, the next command's flush discards it
  this->send_command_no_wait_(PASSIVE_MODE);
  this->set_mode_(OperatingMode::PASSIVE_POLL, now);
  this->last_poll_ms_ = now;
  this->status_set_warning(LOG_STR("UFM-01 passive poll"));
}

void UFM01Component::restart_startup_(const LogString *reason, uint32_t now) {
  ESP_LOGW(TAG, "%s, re-initializing UFM-01", LOG_STR_ARG(reason));
  this->publish_stale_flow_and_temperature_();
  this->set_mode_(OperatingMode::STARTUP, now);
  this->send_reset_(LOG_STR("re-initializing UFM-01"), now);
}

void UFM01Component::note_passive_poll_result_(bool success, uint32_t now) {
  if (success) {
    this->consecutive_passive_failures_ = 0;
    return;
  }
  this->status_set_warning(LOG_STR("UFM-01 passive poll failed"));
  if (++this->consecutive_passive_failures_ < PASSIVE_FAIL_ESCALATE_COUNT)
    return;
  this->restart_startup_(LOG_STR("Passive poll failed repeatedly"), now);
}

void UFM01Component::start_passive_read_(uint32_t now) {
#ifdef USE_UFM01_METER_ID
  if (this->meter_id_text_sensor_ != nullptr && !this->meter_id_published_) {
    this->passive_expected_size_ = PASSIVE_FRAME_WITH_ID_SIZE;
    this->passive_start_byte_2_ = PASSIVE_START_BYTE_2_WITH_ID;
    this->send_command_no_wait_(READ_SENSOR_DATA_WITH_ID);
  } else
#endif
  {
    this->passive_expected_size_ = PASSIVE_FRAME_SIZE;
    this->passive_start_byte_2_ = PASSIVE_START_BYTE_2;
    this->send_command_no_wait_(READ_SENSOR_DATA_NO_ID);
  }
  this->passive_index_ = 0;
  this->passive_start_ms_ = now;
}

ReadResult UFM01Component::continue_passive_read_(uint32_t now) {
  const uint8_t expected_size = this->passive_expected_size_;

  while (this->available() && this->passive_index_ < expected_size) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;

    if (this->passive_index_ == 0 && byte != FRAME_START_BYTE_1)
      continue;
    if (this->passive_index_ == 1 && byte != this->passive_start_byte_2_) {
      // The mismatched byte may itself be the start of the real frame
      this->passive_index_ = (byte == FRAME_START_BYTE_1) ? 1 : 0;
      continue;
    }
    this->passive_frame_[this->passive_index_++] = byte;
  }

  if (this->passive_index_ < expected_size) {
    if (now - this->passive_start_ms_ < PASSIVE_READ_TIMEOUT_MS)
      return ReadResult::READ_RESULT_PENDING;
    ESP_LOGD(TAG, "passive read timeout (%u/%u bytes)", this->passive_index_, expected_size);
    return ReadResult::READ_RESULT_FAILURE;
  }

  uint8_t active_frame[FRAME_SIZE];
#ifdef USE_UFM01_METER_ID
  if (expected_size == PASSIVE_FRAME_WITH_ID_SIZE) {
    if (!validate_passive_with_id_frame(this->passive_frame_)) {
      log_hex(this->passive_frame_, PASSIVE_FRAME_WITH_ID_SIZE);
      ESP_LOGW(TAG, "invalid passive frame with ID");
      return ReadResult::READ_RESULT_FAILURE;
    }
    passive_to_active_frame(this->passive_frame_, PASSIVE_WITH_ID_LAYOUT, active_frame);
    // Device ID and the byte after it
    std::copy_n(&this->passive_frame_[PASSIVE_WITH_ID_DEVICE_ID_INDEX], DEVICE_ID_LENGTH + 1,
                &active_frame[FRAME_DEVICE_ID_INDEX]);
  } else
#endif
  {
    if (!validate_passive_frame(this->passive_frame_)) {
      log_hex(this->passive_frame_, PASSIVE_FRAME_SIZE);
      ESP_LOGW(TAG, "invalid passive frame");
      return ReadResult::READ_RESULT_FAILURE;
    }
    passive_to_active_frame(this->passive_frame_, PASSIVE_NO_ID_LAYOUT, active_frame);
    active_frame[FRAME_DEVICE_ID_INDEX + DEVICE_ID_LENGTH] = 0x01;
  }
  this->on_active_frame_(active_frame, now);
  return ReadResult::READ_RESULT_SUCCESS;
}

void UFM01Component::loop_startup_(uint32_t now) {
  const uint32_t elapsed = now - this->phase_start_ms_;

  switch (this->startup_phase_) {
    case StartupPhase::WAIT:
      // Pick up an already-streaming device without resetting it
      if (this->process_active_stream_(now)) {
        this->enter_active_stream_(LOG_STR("started"), now);
        return;
      }
      if (elapsed < this->startup_wait_ms_)
        return;
      ESP_LOGD(TAG, "Running startup sequence");
      this->send_reset_(LOG_STR("initializing UFM-01"), now);
      return;

    case StartupPhase::RESET_WAIT_ACK:
      if (this->consume_ack_()) {
        this->set_startup_phase_(StartupPhase::POST_RESET_WAIT, now);
        return;
      }
      if (elapsed < COMMAND_ACK_TIMEOUT_MS)
        return;
      if (!this->reset_retried_) {
        ESP_LOGW(TAG, "Reset not acknowledged, retrying in %" PRIu32 " ms", RESET_RETRY_DELAY_MS);
        this->set_startup_phase_(StartupPhase::RESET_RETRY_WAIT, now);
      } else {
        ESP_LOGW(TAG, "Reset failed during startup");
        this->set_startup_phase_(StartupPhase::POST_RESET_WAIT, now);
      }
      return;

    case StartupPhase::RESET_RETRY_WAIT:
      if (elapsed < RESET_RETRY_DELAY_MS)
        return;
      this->reset_retried_ = true;
      this->send_command_no_wait_(RESET_DEVICE);
      this->set_startup_phase_(StartupPhase::RESET_WAIT_ACK, now);
      return;

    case StartupPhase::POST_RESET_WAIT:
      if (elapsed < POST_RESET_DELAY_MS)
        return;
#ifdef USE_UFM01_SOFTWARE_VERSION
      if (this->software_version_text_sensor_ != nullptr && !this->software_version_published_ &&
          this->software_version_failures_ < SOFTWARE_VERSION_MAX_ATTEMPTS) {
        this->start_software_version_read_(now);
        this->set_startup_phase_(StartupPhase::SOFTWARE_VERSION_WAIT_REPLY, now);
        return;
      }
#endif
      this->send_command_no_wait_(ACTIVE_MODE);
      this->set_startup_phase_(StartupPhase::ACTIVE_WAIT_FRAME, now);
      return;

#ifdef USE_UFM01_SOFTWARE_VERSION
    case StartupPhase::SOFTWARE_VERSION_WAIT_REPLY: {
      const ReadResult result = this->continue_software_version_read_(now);
      if (result == ReadResult::READ_RESULT_PENDING)
        return;
      if (result == ReadResult::READ_RESULT_SUCCESS) {
        ESP_LOGD(TAG, "Software version read during startup");
      } else {
        ESP_LOGW(TAG, "Software version read failed during startup, continuing");
      }
      this->finish_software_version_read_(result, now);
      this->send_command_no_wait_(ACTIVE_MODE);
      this->set_startup_phase_(StartupPhase::ACTIVE_WAIT_FRAME, now);
      return;
    }
#endif

    case StartupPhase::ACTIVE_WAIT_FRAME:
      // The command ACK (0xE5) is consumed by the frame parser as noise
      if (this->process_active_stream_(now)) {
        this->enter_active_stream_(LOG_STR("started"), now);
        return;
      }
      if (elapsed < ACTIVE_FRAME_TIMEOUT_MS)
        return;
      this->send_command_no_wait_(PASSIVE_MODE);
      this->set_startup_phase_(StartupPhase::SET_PASSIVE_WAIT_ACK, now);
      return;

    case StartupPhase::SET_PASSIVE_WAIT_ACK:
      if (this->consume_ack_()) {
        ESP_LOGD(TAG, "Passive mode acknowledged during startup");
      } else if (elapsed < COMMAND_ACK_TIMEOUT_MS) {
        return;
      } else {
        ESP_LOGW(TAG, "SET_PASSIVE_MODE not acknowledged during startup, continuing");
      }
      this->start_passive_read_(now);
      this->set_startup_phase_(StartupPhase::PASSIVE_WAIT_REPLY, now);
      return;

    case StartupPhase::PASSIVE_WAIT_REPLY:
      switch (this->continue_passive_read_(now)) {
        case ReadResult::READ_RESULT_PENDING:
          return;
        case ReadResult::READ_RESULT_SUCCESS:
          ESP_LOGI(TAG, "UFM-01 using passive polling");
          this->set_mode_(OperatingMode::PASSIVE_POLL, now);
          this->last_poll_ms_ = now;
          return;
        case ReadResult::READ_RESULT_FAILURE:
          ESP_LOGW(TAG, "Startup failed, retrying in %" PRIu32 " ms", STARTUP_RETRY_MS);
          this->startup_wait_ms_ = STARTUP_RETRY_MS;
          this->set_startup_phase_(StartupPhase::WAIT, now);
          return;
      }
  }
}

void UFM01Component::loop_active_stream_(uint32_t now) {
#ifdef USE_UFM01_SOFTWARE_VERSION
  if (this->service_software_version_(now))
    return;
#endif

  this->process_active_stream_(now);

  if (this->last_valid_frame_ms_ != 0 && now - this->last_valid_frame_ms_ > ACTIVE_STALE_MS)
    this->enter_passive_from_stale_(now);
}

void UFM01Component::loop_passive_poll_(uint32_t now) {
#ifdef USE_UFM01_SOFTWARE_VERSION
  if (this->service_software_version_(now))
    return;
#endif

  if (this->passive_read_pending_) {
    const ReadResult result = this->continue_passive_read_(now);
    if (result == ReadResult::READ_RESULT_PENDING)
      return;
    this->passive_read_pending_ = false;
    this->note_passive_poll_result_(result == ReadResult::READ_RESULT_SUCCESS, now);
    return;
  }

  if (this->process_active_stream_(now)) {
    this->enter_active_stream_(LOG_STR("resumed"), now);
    return;
  }

  if (now - this->last_poll_ms_ >= PASSIVE_POLL_INTERVAL_MS) {
    this->last_poll_ms_ = now;
    this->start_passive_read_(now);
    this->passive_read_pending_ = true;
  }
}

void UFM01Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();
#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  this->loop_pending_clear_action_(now);
  if (this->clear_pending_ && this->pending_clear_sent_)
    return;
#endif
  switch (this->operating_mode_) {
    case OperatingMode::STARTUP:
      this->loop_startup_(now);
      return;
    case OperatingMode::ACTIVE_STREAM:
      this->loop_active_stream_(now);
      return;
    case OperatingMode::PASSIVE_POLL:
      this->loop_passive_poll_(now);
      return;
  }
}

}  // namespace esphome::ufm01
