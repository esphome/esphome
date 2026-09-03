#include "ufm01.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>

namespace esphome::ufm01 {

static const char *const TAG = "ufm01";

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
static constexpr uint8_t FRAME_START_BYTE_1 = 0x3C;
static constexpr uint8_t FRAME_START_BYTE_2 = 0x32;
static constexpr uint8_t PASSIVE_START_BYTE_2 = 0x64;
static constexpr uint8_t PASSIVE_START_BYTE_2_WITH_ID = 0x96;
static constexpr uint8_t FRAME_STOP_BYTE = 0x16;
static constexpr size_t PASSIVE_WITH_ID_DEVICE_ID_INDEX = 2;
static constexpr size_t PASSIVE_WITH_ID_ACC_FLOW_FLAG_INDEX = 8;
static constexpr size_t PASSIVE_WITH_ID_ACC_FLOW_INDEX = 9;
static constexpr size_t PASSIVE_WITH_ID_INSTANT_FLOW_FLAG_INDEX = 22;
static constexpr size_t PASSIVE_WITH_ID_INSTANT_FLOW_INDEX = 23;
static constexpr size_t PASSIVE_WITH_ID_TEMP_FLAG_INDEX = 31;
static constexpr size_t PASSIVE_WITH_ID_TEMP_INDEX = 32;
static constexpr size_t PASSIVE_WITH_ID_ST1_INDEX = 35;
static constexpr size_t PASSIVE_WITH_ID_ST2_INDEX = 36;
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

static constexpr size_t SOFTWARE_VERSION_CHECKSUM_INDEX = 5;
static constexpr size_t SOFTWARE_VERSION_STOP_INDEX = 6;

static float to_float(uint8_t data) { return (data >> 4) * 10 + (data & 0x0F); }

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
  if (all_zero)
    return false;
  return true;
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
  uint8_t sum = 0;
  for (size_t i = 0; i < FRAME_CHECKSUM_INDEX; ++i)
    sum += data[i];
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
  uint8_t sum = 0;
  for (size_t i = 0; i < 21; ++i)
    sum += data[i];
  return data[21] == (sum & 0xFF);
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

static void passive_no_id_to_active_frame(const uint8_t passive[PASSIVE_FRAME_SIZE], uint8_t active[FRAME_SIZE]) {
  std::memset(active, 0, FRAME_SIZE);
  active[0] = FRAME_START_BYTE_1;
  active[1] = FRAME_START_BYTE_2;
  active[7] = 0x01;
  active[8] = passive[2];
  for (size_t i = 0; i < 6; ++i)
    active[9 + i] = passive[3 + i];
  active[15] = passive[9];
  for (size_t i = 0; i < 5; ++i)
    active[16 + i] = passive[10 + i];
  active[21] = FRAME_FLAG_RESERVED_SECTION;
  active[24] = passive[15];
  for (size_t i = 0; i < 3; ++i)
    active[25 + i] = passive[16 + i];
  active[28] = passive[19];
  active[29] = passive[20];
  active[30] = passive[21];
  active[31] = FRAME_STOP_BYTE;
}

#ifdef USE_UFM01_METER_ID
static void passive_with_id_to_active_frame(const uint8_t passive[PASSIVE_FRAME_WITH_ID_SIZE],
                                            uint8_t active[FRAME_SIZE]) {
  std::memset(active, 0, FRAME_SIZE);
  active[0] = FRAME_START_BYTE_1;
  active[1] = FRAME_START_BYTE_2;
  for (size_t i = 0; i < DEVICE_ID_LENGTH; ++i)
    active[FRAME_DEVICE_ID_INDEX + i] = passive[PASSIVE_WITH_ID_DEVICE_ID_INDEX + i];
  active[7] = passive[7];
  active[8] = passive[PASSIVE_WITH_ID_ACC_FLOW_FLAG_INDEX];
  for (size_t i = 0; i < 6; ++i)
    active[9 + i] = passive[PASSIVE_WITH_ID_ACC_FLOW_INDEX + i];
  active[15] = passive[PASSIVE_WITH_ID_INSTANT_FLOW_FLAG_INDEX];
  for (size_t i = 0; i < 5; ++i)
    active[16 + i] = passive[PASSIVE_WITH_ID_INSTANT_FLOW_INDEX + i];
  active[21] = FRAME_FLAG_RESERVED_SECTION;
  active[24] = passive[PASSIVE_WITH_ID_TEMP_FLAG_INDEX];
  for (size_t i = 0; i < 3; ++i)
    active[25 + i] = passive[PASSIVE_WITH_ID_TEMP_INDEX + i];
  active[28] = passive[PASSIVE_WITH_ID_ST1_INDEX];
  active[29] = passive[PASSIVE_WITH_ID_ST2_INDEX];
  active[30] = passive[PASSIVE_WITH_ID_CHECKSUM_INDEX];
  active[31] = FRAME_STOP_BYTE;
}
#endif

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
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
  }
  this->read_index_ = 0;
}

void UFM01Component::send_command_no_wait_(const std::array<uint8_t, 7> &command) {
  this->flush_rx_();
  this->write_array(command);
  this->flush();
}

// Drains whatever is currently in the RX buffer, looking for a command ACK.
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
  switch (this->operating_mode_) {
    case OperatingMode::ACTIVE_STREAM:
      return true;
    case OperatingMode::PASSIVE_POLL:
#ifdef USE_UFM01_SOFTWARE_VERSION
      return !this->passive_read_pending_ && !this->software_version_read_pending_;
#else
      return !this->passive_read_pending_;
#endif
    case OperatingMode::STARTUP:
    case OperatingMode::ENTERING_PASSIVE:
      return false;
  }
  return false;
}

bool UFM01Component::request_clear_accumulated_flow_(ClearAccumulatedFlowActionInterface *action) {
  if (this->pending_clear_action_ != nullptr) {
    ESP_LOGW(TAG, "Clear accumulated flow already in progress, ignoring request");
    return false;
  }
  this->pending_clear_action_ = action;
  this->pending_clear_sent_ = false;
  this->pending_clear_start_ms_ = millis();
  return true;
}

void UFM01Component::cancel_pending_clear_action_(ClearAccumulatedFlowActionInterface *action) {
  if (this->pending_clear_action_ != action)
    return;
  this->pending_clear_action_ = nullptr;
  this->pending_clear_sent_ = false;
}

void UFM01Component::finish_pending_clear_action_() {
  ClearAccumulatedFlowActionInterface *action = this->pending_clear_action_;
  this->pending_clear_action_ = nullptr;
  this->pending_clear_sent_ = false;
  if (action != nullptr)
    action->complete();
}

void UFM01Component::loop_pending_clear_action_() {
  if (this->pending_clear_action_ == nullptr)
    return;
  if (!this->pending_clear_sent_) {
    if (!this->can_start_clear_action_()) {
      if (millis() - this->pending_clear_start_ms_ < CLEAR_QUEUE_TIMEOUT_MS)
        return;
      ESP_LOGW(TAG, "Clear accumulated flow timed out waiting for an idle UART");
      this->finish_pending_clear_action_();
      return;
    }
    this->pending_clear_start_ms_ = millis();
    this->pending_clear_sent_ = true;
    this->send_command_no_wait_(CLEAR_ACCUMULATED_FLOW);
    return;
  }
  if (this->consume_ack_()) {
    ESP_LOGI(TAG, "Clear accumulated flow acknowledged");
    this->finish_pending_clear_action_();
    return;
  }
  if (millis() - this->pending_clear_start_ms_ < COMMAND_ACK_TIMEOUT_MS)
    return;
  ESP_LOGW(TAG, "Clear accumulated flow not acknowledged");
  this->finish_pending_clear_action_();
}
#endif  // USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION

float UFM01Component::get_setup_priority() const { return setup_priority::LATE; }

void UFM01Component::setup() {
  ESP_LOGI(TAG, "Setting up UFM-01...");
  this->startup_wait_ms_ = STARTUP_DELAY_MS;
  this->set_startup_phase_(StartupPhase::WAIT);
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

void UFM01Component::publish_stale_flow_and_temperature_() {
#ifdef USE_SENSOR
  if (this->flow_sensor_ != nullptr)
    this->flow_sensor_->publish_state(NAN);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(NAN);
#endif
}

#if defined(USE_UFM01_METER_ID)
void UFM01Component::publish_meter_id_from_frame_(const uint8_t data[FRAME_SIZE]) {
  if (this->meter_id_text_sensor_ == nullptr || this->meter_id_published_)
    return;

  char meter_id_str[DEVICE_ID_STRING_LENGTH + 1];
  if (!bcd_to_digits(&data[FRAME_DEVICE_ID_INDEX], DEVICE_ID_LENGTH, meter_id_str))
    return;

  this->meter_id_text_sensor_->publish_state(meter_id_str);
  this->meter_id_published_ = true;
  ESP_LOGI(TAG, "UFM-01 meter ID: %s", meter_id_str);
}
#endif

#ifdef USE_UFM01_SOFTWARE_VERSION
void UFM01Component::start_software_version_read_() {
  this->send_command_no_wait_(GET_SOFTWARE_VERSION);
  this->software_version_index_ = 0;
  this->software_version_start_ms_ = millis();
  this->software_version_read_pending_ = true;
}

SoftwareVersionReadResult UFM01Component::continue_software_version_read_() {
  while (this->available() && this->software_version_index_ < SOFTWARE_VERSION_RESPONSE_SIZE) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    this->software_version_frame_[this->software_version_index_++] = byte;
  }

  if (this->software_version_index_ < SOFTWARE_VERSION_RESPONSE_SIZE) {
    if (millis() - this->software_version_start_ms_ < SOFTWARE_VERSION_READ_TIMEOUT_MS)
      return SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_PENDING;
    ESP_LOGD(TAG, "software version read timeout (%zu/%zu bytes)", this->software_version_index_,
             SOFTWARE_VERSION_RESPONSE_SIZE);
    return SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE;
  }

  if (!validate_software_version_response(this->software_version_frame_)) {
    log_hex(this->software_version_frame_, SOFTWARE_VERSION_RESPONSE_SIZE);
    ESP_LOGW(TAG, "invalid software version response");
    return SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE;
  }

  if (this->software_version_text_sensor_ != nullptr && !this->software_version_published_) {
    char version_str[SOFTWARE_VERSION_STRING_LENGTH + 1];
    if (!bcd_to_digits(&this->software_version_frame_[1], SOFTWARE_VERSION_LENGTH, version_str)) {
      ESP_LOGW(TAG, "invalid BCD data in software version response");
      return SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE;
    }
    this->software_version_text_sensor_->publish_state(version_str);
    this->software_version_published_ = true;
    ESP_LOGI(TAG, "UFM-01 software version: %s", version_str);
  }

  return SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_SUCCESS;
}
#endif  // USE_UFM01_SOFTWARE_VERSION

void UFM01Component::on_active_frame_(uint8_t data[FRAME_SIZE]) {
#if defined(USE_UFM01_METER_ID)
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
    if (this->flow_sensor_ != nullptr)
      this->flow_sensor_->publish_state(NAN);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(NAN);
  } else {
    if (this->flow_sensor_ != nullptr)
      this->flow_sensor_->publish_state(read_flow(data));
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(read_temperature(data));
  }
#endif
  this->last_valid_frame_ms_ = millis();
  this->status_clear_warning();
  this->status_clear_error();
}

bool UFM01Component::process_active_stream_() {
  bool got_valid_frame = false;

  while (this->available()) {
    if (!this->read_byte(&this->data_[this->read_index_])) {
      ESP_LOGW(TAG, "unable to read byte");
      this->read_index_ = 0;
      continue;
    }
    if ((this->read_index_ == 0 && this->data_[0] != FRAME_START_BYTE_1) ||
        (this->read_index_ == 1 && this->data_[1] != FRAME_START_BYTE_2)) {
      ESP_LOGD(TAG, "not start of data at %d (is 0x%02X)", this->read_index_, this->data_[this->read_index_]);
      this->read_index_ = 0;
      continue;
    }
    if (++this->read_index_ < static_cast<int32_t>(FRAME_SIZE))
      continue;

    if (validate_active_frame(this->data_)) {
      this->on_active_frame_(this->data_);
      this->read_index_ = 0;
      got_valid_frame = true;
      continue;
    }

    log_hex(this->data_, sizeof(this->data_));
    ESP_LOGW(TAG, "unable to read data");
    for (int32_t i = 2;
         i < static_cast<int32_t>(FRAME_STOP_INDEX) && this->read_index_ == static_cast<int32_t>(FRAME_SIZE); ++i) {
      if ((this->data_[i] == FRAME_START_BYTE_1) && (this->data_[i + 1] == FRAME_START_BYTE_2)) {
        for (int32_t j = i; j < static_cast<int32_t>(FRAME_SIZE); ++j)
          this->data_[j - i] = this->data_[j];
        this->read_index_ = static_cast<int32_t>(FRAME_SIZE) - i;
      }
    }
    if (this->read_index_ == static_cast<int32_t>(FRAME_SIZE))
      this->read_index_ = 0;
  }

  return got_valid_frame;
}

void UFM01Component::set_startup_phase_(StartupPhase phase) {
  this->startup_phase_ = phase;
  this->phase_start_ms_ = millis();
}

void UFM01Component::enter_active_stream_(const char *reason) {
  ESP_LOGI(TAG, "UFM-01 active stream %s", reason);
  this->operating_mode_ = OperatingMode::ACTIVE_STREAM;
  this->passive_read_pending_ = false;
#ifdef USE_UFM01_SOFTWARE_VERSION
  this->software_version_read_pending_ = false;
#endif
  this->consecutive_passive_failures_ = 0;
}

void UFM01Component::enter_passive_from_stale_() {
  ESP_LOGW(TAG, "Active stream stale, switching to passive polling");
  this->publish_stale_flow_and_temperature_();
  // Flush any leftover active-stream bytes, then tell the device to stop streaming
  this->send_command_no_wait_(PASSIVE_MODE);
  this->operating_mode_ = OperatingMode::ENTERING_PASSIVE;
  this->phase_start_ms_ = millis();
  this->passive_read_pending_ = false;
  this->last_poll_ms_ = 0;
  this->consecutive_passive_failures_ = 0;
  this->status_set_warning("UFM-01 passive poll");
}

void UFM01Component::restart_startup_(const char *reason) {
  ESP_LOGW(TAG, "%s, re-initializing UFM-01", reason);
  this->publish_stale_flow_and_temperature_();
  this->operating_mode_ = OperatingMode::STARTUP;
  this->passive_read_pending_ = false;
#ifdef USE_UFM01_SOFTWARE_VERSION
  this->software_version_read_pending_ = false;
#endif
  this->consecutive_passive_failures_ = 0;
  this->reset_retried_ = false;
  this->status_set_warning("re-initializing UFM-01");
  this->send_command_no_wait_(RESET_DEVICE);
  this->set_startup_phase_(StartupPhase::RESET_WAIT_ACK);
}

void UFM01Component::note_passive_poll_result_(PassiveReadResult result) {
  if (result == PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS) {
    this->consecutive_passive_failures_ = 0;
    return;
  }
  if (result != PassiveReadResult::PASSIVE_READ_RESULT_FAILURE)
    return;

  this->status_set_warning("UFM-01 passive poll failed");
  if (++this->consecutive_passive_failures_ < PASSIVE_FAIL_ESCALATE_COUNT)
    return;
  this->restart_startup_("Passive poll failed repeatedly");
}

#ifdef USE_UFM01_SOFTWARE_VERSION
void UFM01Component::try_pending_software_version_read_() {
  if (this->software_version_text_sensor_ == nullptr || this->software_version_published_ ||
      this->software_version_read_pending_ || this->passive_read_pending_)
    return;
  if (this->last_software_version_attempt_ms_ != 0 &&
      millis() - this->last_software_version_attempt_ms_ < SOFTWARE_VERSION_RETRY_MS)
    return;

  this->start_software_version_read_();
}
#endif

size_t UFM01Component::passive_expected_frame_size_() const {
#ifdef USE_UFM01_METER_ID
  return this->passive_expects_id_ ? PASSIVE_FRAME_WITH_ID_SIZE : PASSIVE_FRAME_SIZE;
#else
  return PASSIVE_FRAME_SIZE;
#endif
}

void UFM01Component::start_passive_read_() {
#if defined(USE_UFM01_METER_ID)
  if (this->meter_id_text_sensor_ != nullptr && !this->meter_id_published_) {
    this->passive_expects_id_ = true;
    this->send_command_no_wait_(READ_SENSOR_DATA_WITH_ID);
  } else {
    this->passive_expects_id_ = false;
    this->send_command_no_wait_(READ_SENSOR_DATA_NO_ID);
  }
#else
  this->send_command_no_wait_(READ_SENSOR_DATA_NO_ID);
#endif
  this->passive_index_ = 0;
  this->passive_start_ms_ = millis();
}

// Accumulates the reply to a passive read request across loop iterations.
PassiveReadResult UFM01Component::continue_passive_read_() {
  const size_t expected_size = this->passive_expected_frame_size_();
#ifdef USE_UFM01_METER_ID
  const uint8_t expected_start_byte_2 = this->passive_expects_id_ ? PASSIVE_START_BYTE_2_WITH_ID : PASSIVE_START_BYTE_2;
#else
  const uint8_t expected_start_byte_2 = PASSIVE_START_BYTE_2;
#endif

  while (this->available() && this->passive_index_ < expected_size) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;

    if (this->passive_index_ == 0 && byte != FRAME_START_BYTE_1)
      continue;
    if (this->passive_index_ == 1 && byte != expected_start_byte_2) {
      // The mismatched byte may itself be the start of the real frame
      this->passive_index_ = (byte == FRAME_START_BYTE_1) ? 1 : 0;
      continue;
    }
    this->passive_frame_[this->passive_index_++] = byte;
  }

  if (this->passive_index_ < expected_size) {
    if (millis() - this->passive_start_ms_ < PASSIVE_READ_TIMEOUT_MS)
      return PassiveReadResult::PASSIVE_READ_RESULT_PENDING;
    ESP_LOGD(TAG, "passive read timeout (%zu/%zu bytes)", this->passive_index_, expected_size);
    return PassiveReadResult::PASSIVE_READ_RESULT_FAILURE;
  }

  uint8_t active_frame[FRAME_SIZE];
#ifdef USE_UFM01_METER_ID
  if (this->passive_expects_id_) {
    if (!validate_passive_with_id_frame(this->passive_frame_)) {
      log_hex(this->passive_frame_, PASSIVE_FRAME_WITH_ID_SIZE);
      ESP_LOGW(TAG, "invalid passive frame with ID");
      return PassiveReadResult::PASSIVE_READ_RESULT_FAILURE;
    }
    passive_with_id_to_active_frame(this->passive_frame_, active_frame);
  } else
#endif
  {
    if (!validate_passive_frame(this->passive_frame_)) {
      log_hex(this->passive_frame_, PASSIVE_FRAME_SIZE);
      ESP_LOGW(TAG, "invalid passive frame");
      return PassiveReadResult::PASSIVE_READ_RESULT_FAILURE;
    }
    passive_no_id_to_active_frame(this->passive_frame_, active_frame);
  }
  this->on_active_frame_(active_frame);
  return PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS;
}

void UFM01Component::loop_startup_() {
  const uint32_t elapsed = millis() - this->phase_start_ms_;

  switch (this->startup_phase_) {
    case StartupPhase::WAIT:
      // Pick up an already-streaming device without resetting it
      if (this->process_active_stream_()) {
#ifdef USE_UFM01_SOFTWARE_VERSION
        if (this->software_version_text_sensor_ != nullptr && !this->software_version_published_) {
          this->start_software_version_read_();
          this->set_startup_phase_(StartupPhase::SOFTWARE_VERSION_WAIT_REPLY);
          return;
        }
#endif
        this->enter_active_stream_("started");
        return;
      }
      if (elapsed < this->startup_wait_ms_)
        return;
      ESP_LOGD(TAG, "Running startup sequence");
      this->status_set_warning("initializing UFM-01");
      this->reset_retried_ = false;
      this->send_command_no_wait_(RESET_DEVICE);
      this->set_startup_phase_(StartupPhase::RESET_WAIT_ACK);
      return;

    case StartupPhase::RESET_WAIT_ACK:
      if (this->consume_ack_()) {
        this->set_startup_phase_(StartupPhase::POST_RESET_WAIT);
        return;
      }
      if (elapsed < COMMAND_ACK_TIMEOUT_MS)
        return;
      if (!this->reset_retried_) {
        ESP_LOGW(TAG, "Reset not acknowledged, retrying in %" PRIu32 " ms", RESET_RETRY_DELAY_MS);
        this->set_startup_phase_(StartupPhase::RESET_RETRY_WAIT);
      } else {
        ESP_LOGW(TAG, "Reset failed during startup");
        this->set_startup_phase_(StartupPhase::POST_RESET_WAIT);
      }
      return;

    case StartupPhase::RESET_RETRY_WAIT:
      if (elapsed < RESET_RETRY_DELAY_MS)
        return;
      this->reset_retried_ = true;
      this->send_command_no_wait_(RESET_DEVICE);
      this->set_startup_phase_(StartupPhase::RESET_WAIT_ACK);
      return;

    case StartupPhase::POST_RESET_WAIT:
      if (elapsed < POST_RESET_DELAY_MS)
        return;
#ifdef USE_UFM01_SOFTWARE_VERSION
      if (this->software_version_text_sensor_ != nullptr && !this->software_version_published_) {
        this->start_software_version_read_();
        this->set_startup_phase_(StartupPhase::SOFTWARE_VERSION_WAIT_REPLY);
        return;
      }
#endif
      this->send_command_no_wait_(ACTIVE_MODE);
      this->set_startup_phase_(StartupPhase::ACTIVE_WAIT_FRAME);
      return;

#ifdef USE_UFM01_SOFTWARE_VERSION
    case StartupPhase::SOFTWARE_VERSION_WAIT_REPLY:
      switch (this->continue_software_version_read_()) {
        case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_PENDING:
          return;
        case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_SUCCESS:
          ESP_LOGD(TAG, "Software version read during startup");
          break;
        case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE:
          ESP_LOGW(TAG, "Software version read failed during startup, continuing");
          break;
      }
      this->software_version_read_pending_ = false;
      this->last_software_version_attempt_ms_ = millis();
      this->send_command_no_wait_(ACTIVE_MODE);
      this->set_startup_phase_(StartupPhase::ACTIVE_WAIT_FRAME);
      return;
#endif

    case StartupPhase::ACTIVE_WAIT_FRAME:
      // The command ACK (0xE5) is consumed by the frame parser as noise
      if (this->process_active_stream_()) {
        this->enter_active_stream_("started");
        return;
      }
      if (elapsed < ACTIVE_FRAME_TIMEOUT_MS)
        return;
      this->send_command_no_wait_(PASSIVE_MODE);
      this->set_startup_phase_(StartupPhase::SET_PASSIVE_WAIT_ACK);
      return;

    case StartupPhase::SET_PASSIVE_WAIT_ACK:
      if (this->consume_ack_()) {
        ESP_LOGD(TAG, "Passive mode acknowledged during startup");
      } else if (elapsed < COMMAND_ACK_TIMEOUT_MS) {
        return;
      } else {
        ESP_LOGW(TAG, "SET_PASSIVE_MODE not acknowledged during startup, continuing");
      }
      this->start_passive_read_();
      this->set_startup_phase_(StartupPhase::PASSIVE_WAIT_REPLY);
      return;

    case StartupPhase::PASSIVE_WAIT_REPLY:
      switch (this->continue_passive_read_()) {
        case PassiveReadResult::PASSIVE_READ_RESULT_PENDING:
          return;
        case PassiveReadResult::PASSIVE_READ_RESULT_SUCCESS:
          ESP_LOGI(TAG, "UFM-01 using passive polling");
          this->operating_mode_ = OperatingMode::PASSIVE_POLL;
          this->passive_read_pending_ = false;
          this->last_poll_ms_ = millis();
          return;
        case PassiveReadResult::PASSIVE_READ_RESULT_FAILURE:
          ESP_LOGW(TAG, "Startup failed, retrying in %" PRIu32 " ms", STARTUP_RETRY_MS);
          this->startup_wait_ms_ = STARTUP_RETRY_MS;
          this->set_startup_phase_(StartupPhase::WAIT);
          return;
      }
  }
}

void UFM01Component::loop_active_stream_() {
#ifdef USE_UFM01_SOFTWARE_VERSION
  if (this->software_version_read_pending_) {
    switch (this->continue_software_version_read_()) {
      case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_PENDING:
        return;
      case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_SUCCESS:
      case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE:
        this->software_version_read_pending_ = false;
        this->last_software_version_attempt_ms_ = millis();
        break;
    }
    return;
  }
#endif

  this->process_active_stream_();

#ifdef USE_UFM01_SOFTWARE_VERSION
  this->try_pending_software_version_read_();
  if (this->software_version_read_pending_)
    return;
#endif

  if (this->last_valid_frame_ms_ != 0 && millis() - this->last_valid_frame_ms_ > ACTIVE_STALE_MS) {
    this->enter_passive_from_stale_();
  }
}

void UFM01Component::loop_entering_passive_() {
  if (this->consume_ack_()) {
    ESP_LOGI(TAG, "UFM-01 passive mode acknowledged");
    this->operating_mode_ = OperatingMode::PASSIVE_POLL;
    return;
  }
  if (millis() - this->phase_start_ms_ < COMMAND_ACK_TIMEOUT_MS)
    return;
  ESP_LOGW(TAG, "SET_PASSIVE_MODE not acknowledged, continuing with passive poll");
  this->operating_mode_ = OperatingMode::PASSIVE_POLL;
}

void UFM01Component::loop_passive_poll_() {
#ifdef USE_UFM01_SOFTWARE_VERSION
  if (this->software_version_read_pending_) {
    switch (this->continue_software_version_read_()) {
      case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_PENDING:
        return;
      case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_SUCCESS:
      case SoftwareVersionReadResult::SOFTWARE_VERSION_READ_RESULT_FAILURE:
        this->software_version_read_pending_ = false;
        this->last_software_version_attempt_ms_ = millis();
        break;
    }
    return;
  }
#endif

  if (this->passive_read_pending_) {
    const PassiveReadResult result = this->continue_passive_read_();
    if (result == PassiveReadResult::PASSIVE_READ_RESULT_PENDING)
      return;
    this->passive_read_pending_ = false;
    this->note_passive_poll_result_(result);
    return;
  }

  if (this->process_active_stream_()) {
    this->enter_active_stream_("resumed");
    return;
  }

  if (millis() - this->last_poll_ms_ >= PASSIVE_POLL_INTERVAL_MS) {
    this->last_poll_ms_ = millis();
#ifdef USE_UFM01_SOFTWARE_VERSION
    this->try_pending_software_version_read_();
    if (this->software_version_read_pending_)
      return;
#endif
    this->start_passive_read_();
    this->passive_read_pending_ = true;
  }
}

void UFM01Component::loop() {
#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  this->loop_pending_clear_action_();
  if (this->pending_clear_action_ != nullptr && this->pending_clear_sent_)
    return;
#endif
  switch (this->operating_mode_) {
    case OperatingMode::STARTUP:
      this->loop_startup_();
      return;
    case OperatingMode::ACTIVE_STREAM:
      this->loop_active_stream_();
      return;
    case OperatingMode::ENTERING_PASSIVE:
      this->loop_entering_passive_();
      return;
    case OperatingMode::PASSIVE_POLL:
      this->loop_passive_poll_();
      return;
  }
}

}  // namespace esphome::ufm01
