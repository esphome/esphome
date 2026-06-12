#include "ufm01.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <array>

namespace esphome::ufm01 {

static const char *const TAG = "ufm01";

static constexpr uint8_t COMMAND_ACK = 0xE5;
static constexpr uint32_t COMMAND_ACK_TIMEOUT_MS = 200;

static constexpr float L_PER_M3 = 1000.0f;
static constexpr float M3_PER_L = 1.0f / L_PER_M3;

static constexpr std::array<uint8_t, 7> ACTIVE_MODE = {0xFE, 0xFE, 0x11, 0x5C, 0x00, 0x5C, 0x16};
static constexpr std::array<uint8_t, 7> CLEAR_ACCUMULATED_FLOW = {0xFE, 0xFE, 0x11, 0x5A, 0xFD, 0x57, 0x16};
static constexpr std::array<uint8_t, 7> RESET_DEVICE = {0xFE, 0xFE, 0x11, 0x5D, 0xCB, 0x28, 0x16};

// Active-mode frame layout (datasheet Table 7)
static constexpr size_t FRAME_CHECKSUM_INDEX = 30;
static constexpr size_t FRAME_STOP_INDEX = 31;
static constexpr uint8_t FRAME_START_BYTE_1 = 0x3C;
static constexpr uint8_t FRAME_START_BYTE_2 = 0x32;
static constexpr uint8_t FRAME_STOP_BYTE = 0x16;
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

static float to_float(uint8_t data) { return (data >> 4) * 10 + (data & 0x0F); }

static bool check_byte(const uint8_t data[FRAME_SIZE], size_t index, uint8_t expected, const char *name) {
  if (data[index] == expected)
    return true;
  ESP_LOGW(TAG, "%s (byte %zu) - expected 0x%02X, but was 0x%02X", name, index, expected, data[index]);
  return false;
}

static bool validate_data(uint8_t data[FRAME_SIZE]) {
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

static float read_accumulated_flow(uint8_t data[FRAME_SIZE]) {
  return (data[FRAME_ACC_FLOW_FLAG_INDEX] == ACC_FLOW_M3_FLAG ? L_PER_M3 : 1.0f) *
         (to_float(data[14]) * 10000000.0f + to_float(data[13]) * 100000.0f + to_float(data[12]) * 1000.0f +
          to_float(data[11]) * 10.0f + to_float(data[10]) * 0.1f + to_float(data[9]) * 0.001f);
}

static float read_flow(uint8_t data[FRAME_SIZE]) {
  return (data[FRAME_FLOW_SIGN_INDEX] == FLOW_NEGATIVE_SIGN ? -1.0f : 1.0f) *
         (to_float(data[19]) * 10000.0f + to_float(data[18]) * 100.0f + to_float(data[17]) +
          to_float(data[16]) * 0.01f) *
         M3_PER_L;
}

static void log_hex(const uint8_t *data, size_t len) {
  char hex_buf[format_hex_pretty_size(FRAME_SIZE)];
  ESP_LOGD(TAG, "%s", format_hex_pretty_to(hex_buf, data, len, ' '));
}

static float read_temperature(uint8_t data[FRAME_SIZE]) {
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

bool UFM01Component::send_command_(const std::array<uint8_t, 7> &command) {
  this->write_array(command);
  this->flush();
  const uint32_t start = millis();
  while (millis() - start < COMMAND_ACK_TIMEOUT_MS) {
    if (this->available()) {
      uint8_t byte;
      if (this->read_byte(&byte)) {
        if (byte == COMMAND_ACK)
          return true;
        ESP_LOGV(TAG, "Unexpected byte while waiting for command ACK: 0x%02X", byte);
      }
    }
    delay(1);
  }
  return false;
}

bool UFM01Component::reset_device_() { return this->send_command_(RESET_DEVICE); }

bool UFM01Component::clear_accumulated_flow_() { return this->send_command_(CLEAR_ACCUMULATED_FLOW); }

bool UFM01Component::set_active_mode_() { return this->send_command_(ACTIVE_MODE); }

float UFM01Component::get_setup_priority() const { return setup_priority::IO; }

void UFM01Component::setup() {
  ESP_LOGI(TAG, "Setting up UFM-01...");
  if (!this->set_active_mode_()) {
    ESP_LOGW(TAG, "Failed to set active mode (no ACK from device)");
    this->mark_failed();
  }
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
  this->check_uart_settings(2400, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
  if (this->is_failed()) {
    ESP_LOGW(TAG, "Setup failed: active mode not acknowledged by device");
  }
}

void UFM01Component::on_data_(uint8_t data[FRAME_SIZE]) {
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
}

void UFM01Component::loop() {
  // Drain the UART buffer each loop, reading one byte at a time into the frame
  while (this->available()) {
    if (!this->read_byte(&this->data_[this->read_index_])) {
      ESP_LOGW(TAG, "unable to read byte");
      this->read_index_ = 0;
      continue;
    }
    if ((this->read_index_ == 0 && this->data_[0] != FRAME_START_BYTE_1) ||
        (this->read_index_ == 1 && this->data_[1] != FRAME_START_BYTE_2)) {
      ESP_LOGW(TAG, "not start of data at %d (is 0x%02X)", this->read_index_, this->data_[this->read_index_]);
      this->read_index_ = 0;
      continue;
    }
    if (++this->read_index_ < static_cast<int32_t>(FRAME_SIZE))
      continue;

    // Full frame received
    if (validate_data(this->data_)) {
      this->on_data_(this->data_);
      this->read_index_ = 0;
      continue;
    }

    // Invalid frame: try to resync on the next start marker within the buffer
    log_hex(this->data_, sizeof(this->data_));
    ESP_LOGE(TAG, "unable to read data");
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
}

}  // namespace esphome::ufm01
