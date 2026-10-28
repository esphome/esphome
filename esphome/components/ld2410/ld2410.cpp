#include "ld2410.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

namespace esphome::ld2410 {

ESPHOME_LOG_TAG(TAG, "ld2410");

enum LightFunction : uint8_t {
  LIGHT_FUNCTION_OFF = 0x00,
  LIGHT_FUNCTION_BELOW = 0x01,
  LIGHT_FUNCTION_ABOVE = 0x02,
};

enum OutPinLevel : uint8_t {
  OUT_PIN_LEVEL_LOW = 0x00,
  OUT_PIN_LEVEL_HIGH = 0x01,
};

enum PeriodicData : uint8_t {
  DATA_TYPES = 6,
  TARGET_STATES = 8,
  MOVING_TARGET_LOW = 9,
  MOVING_TARGET_HIGH = 10,
  MOVING_ENERGY = 11,
  STILL_TARGET_LOW = 12,
  STILL_TARGET_HIGH = 13,
  STILL_ENERGY = 14,
  DETECT_DISTANCE_LOW = 15,
  DETECT_DISTANCE_HIGH = 16,
  MOVING_SENSOR_START = 19,
  STILL_SENSOR_START = 28,
  LIGHT_SENSOR = 37,
  OUT_PIN_SENSOR = 38,
};

enum PeriodicDataValue : uint8_t {
  HEADER = 0xAA,
  FOOTER = 0x55,
  CHECK = 0x00,
};

enum AckData : uint8_t {
  COMMAND = 6,
  COMMAND_STATUS = 7,
};

// Index + 1 is the BaudRate value; the select options use the same order
PROGMEM_STRING_TABLE(BaudRateStrings, "9600", "19200", "38400", "57600", "115200", "230400", "256000", "460800");
static_assert(BAUD_RATE_9600 == 1 && BAUD_RATE_460800 == BaudRateStrings::COUNT, "BaudRate is index + 1");
// Same order as DISTANCE_RESOLUTIONS
PROGMEM_STRING_TABLE(DistanceResolutionStrings, "0.2m", "0.75m");
// The light function and out pin level select options are in enum order
static_assert(LIGHT_FUNCTION_OFF == 0 && LIGHT_FUNCTION_BELOW == 1 && LIGHT_FUNCTION_ABOVE == 2,
              "LightFunction is the select option index");
static_assert(OUT_PIN_LEVEL_LOW == 0 && OUT_PIN_LEVEL_HIGH == 1, "OutPinLevel is the select option index");

static constexpr uint32_t BAUD_RATES[] PROGMEM = {9600, 19200, 38400, 57600, 115200, 230400, 256000, 460800};

// Commands
static constexpr uint8_t CMD_ENABLE_CONF = 0xFF;
static constexpr uint8_t CMD_DISABLE_CONF = 0xFE;
static constexpr uint8_t CMD_ENABLE_ENG = 0x62;
static constexpr uint8_t CMD_DISABLE_ENG = 0x63;
static constexpr uint8_t CMD_MAXDIST_DURATION = 0x60;
static constexpr uint8_t CMD_QUERY = 0x61;
static constexpr uint8_t CMD_GATE_SENS = 0x64;
static constexpr uint8_t CMD_QUERY_VERSION = 0xA0;
static constexpr uint8_t CMD_QUERY_DISTANCE_RESOLUTION = 0xAB;
static constexpr uint8_t CMD_SET_DISTANCE_RESOLUTION = 0xAA;
static constexpr uint8_t CMD_QUERY_LIGHT_CONTROL = 0xAE;
static constexpr uint8_t CMD_SET_LIGHT_CONTROL = 0xAD;
static constexpr uint8_t CMD_SET_BAUD_RATE = 0xA1;
static constexpr uint8_t CMD_BT_PASSWORD = 0xA9;
static constexpr uint8_t CMD_QUERY_MAC_ADDRESS = 0xA5;
static constexpr uint8_t CMD_RESET = 0xA2;
static constexpr uint8_t CMD_RESTART = 0xA3;
static constexpr uint8_t CMD_BLUETOOTH = 0xA4;
#ifdef USE_LD2410_BG_CORRECTION
// Background noise correction (firmware V2.44+): 0x0B starts it, 0x1B polls it
static constexpr uint8_t CMD_BG_CORRECTION_START = 0x0B;
static constexpr uint8_t CMD_BG_CORRECTION_QUERY = 0x1B;
static constexpr uint8_t BG_CORRECTION_IDLE = 0x00;
static constexpr uint8_t BG_CORRECTION_COMPLETED = 0x02;
static constexpr uint32_t BG_CORRECTION_POLL_INTERVAL_MS = 1000;
// About 10 s of warm-up before measuring, plus margin
static constexpr uint32_t BG_CORRECTION_EXTRA_MS = 13000;
static constexpr uint32_t BG_CORRECTION_POLL_ID = 0;
static constexpr uint32_t BG_CORRECTION_DEADLINE_ID = 1;
#endif
// Commands values
static constexpr uint8_t CMD_MAX_MOVE_VALUE = 0x00;
static constexpr uint8_t CMD_MAX_STILL_VALUE = 0x01;
static constexpr uint8_t CMD_DURATION_VALUE = 0x02;
// Bitmasks for target states
static constexpr uint8_t MOVE_BITMASK = 0x01;
static constexpr uint8_t STILL_BITMASK = 0x02;
// While correcting: 0x04 running, 0x05 success, 0x06 failure
static constexpr uint8_t TARGET_STATE_CALIBRATING_MIN = 0x04;
#ifdef USE_LD2410_BG_CORRECTION
static constexpr uint8_t TARGET_STATE_BG_SUCCESS = 0x05;
static constexpr uint8_t TARGET_STATE_BG_FAILED = 0x06;
PROGMEM_STRING_TABLE(BgCorrectionResultStrings, "Success", "Failed", "Unconfirmed", "Refused", "Timed out", "Running");
// An unknown result never reads as a pass
static constexpr uint8_t UNCONFIRMED_INDEX = static_cast<uint8_t>(BgCorrectionResult::BG_CORRECTION_RESULT_UNCONFIRMED);
#endif
using ld24xx::CMD_FRAME_FOOTER;
using ld24xx::CMD_FRAME_HEADER;
using ld24xx::HEADER_FOOTER_SIZE;
using ld24xx::validate_header_footer;
// Smallest frames whose values can all be read: the last value, data footer and check, then the frame footer
static constexpr uint8_t PERIODIC_FRAME_MIN = DETECT_DISTANCE_HIGH + 3 + HEADER_FOOTER_SIZE;
static constexpr uint8_t ENGINEERING_FRAME_MIN = OUT_PIN_SENSOR + 3 + HEADER_FOOTER_SIZE;
// Data Header & Footer
static constexpr uint8_t DATA_FRAME_HEADER[HEADER_FOOTER_SIZE] = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t DATA_FRAME_FOOTER[HEADER_FOOTER_SIZE] = {0xF8, 0xF7, 0xF6, 0xF5};
// MAC address the module uses when Bluetooth is disabled
static constexpr uint8_t NO_MAC[] = {0x08, 0x05, 0x04, 0x03, 0x02, 0x01};

void LD2410Component::dump_config() {
  char mac_s[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  char version_s[20];
  const char *mac_str = ld24xx::format_mac_str(this->mac_address_, mac_s);
  ld24xx::format_version_str(this->version_, version_s);
  ESP_LOGCONFIG(TAG,
                "LD2410:\n"
                "  Firmware version: %s\n"
                "  MAC address: %s",
                version_s, mac_str);
#ifdef USE_BINARY_SENSOR
  ESP_LOGCONFIG(TAG, "Binary Sensors:");
  LOG_BINARY_SENSOR("  ", "Target", this->target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "MovingTarget", this->moving_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "StillTarget", this->still_target_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "OutPinPresenceStatus", this->out_pin_presence_status_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Calibration", this->calibration_binary_sensor_);
#endif
#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "Sensors:");
  LOG_SENSOR_WITH_DEDUP("  ", "Light", this->light_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "DetectionDistance", this->detection_distance_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "MovingTargetDistance", this->moving_target_distance_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "MovingTargetEnergy", this->moving_target_energy_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "StillTargetDistance", this->still_target_distance_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "StillTargetEnergy", this->still_target_energy_sensor_);
  for (auto &s : this->gate_move_sensors_) {
    LOG_SENSOR_WITH_DEDUP("  ", "GateMove", s);
  }
  for (auto &s : this->gate_still_sensors_) {
    LOG_SENSOR_WITH_DEDUP("  ", "GateStill", s);
  }
#endif
#ifdef USE_TEXT_SENSOR
  ESP_LOGCONFIG(TAG, "Text Sensors:");
  LOG_TEXT_SENSOR("  ", "Mac", this->mac_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "CalibrationResult", this->calibration_result_text_sensor_);
#endif
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "Numbers:");
  LOG_NUMBER("  ", "LightThreshold", this->light_threshold_number_);
  LOG_NUMBER("  ", "MaxMoveDistanceGate", this->max_move_distance_gate_number_);
  LOG_NUMBER("  ", "MaxStillDistanceGate", this->max_still_distance_gate_number_);
  LOG_NUMBER("  ", "Timeout", this->timeout_number_);
  for (number::Number *n : this->gate_move_threshold_numbers_) {
    LOG_NUMBER("  ", "MoveThreshold", n);
  }
  for (number::Number *n : this->gate_still_threshold_numbers_) {
    LOG_NUMBER("  ", "StillThreshold", n);
  }
#endif
#ifdef USE_SELECT
  ESP_LOGCONFIG(TAG, "Selects:");
  LOG_SELECT("  ", "BaudRate", this->baud_rate_select_);
  LOG_SELECT("  ", "DistanceResolution", this->distance_resolution_select_);
  LOG_SELECT("  ", "LightFunction", this->light_function_select_);
  LOG_SELECT("  ", "OutPinLevel", this->out_pin_level_select_);
#endif
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "Switches:");
  LOG_SWITCH("  ", "Bluetooth", this->bluetooth_switch_);
  LOG_SWITCH("  ", "EngineeringMode", this->engineering_mode_switch_);
#endif
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "Buttons:");
  LOG_BUTTON("  ", "FactoryReset", this->factory_reset_button_);
  LOG_BUTTON("  ", "Query", this->query_button_);
  LOG_BUTTON("  ", "Restart", this->restart_button_);
  LOG_BUTTON("  ", "BackgroundCorrection", this->background_correction_button_);
#endif
}

void LD2410Component::setup() {
#ifdef USE_LD2410_BG_CORRECTION
  this->publish_calibration_running_(false);
#endif
  this->read_all_info();
}

void LD2410Component::read_all_info() {
  this->set_config_mode_(true);
  this->get_version_();
  this->get_mac_();
  this->get_distance_resolution_();
  this->query_light_control_();
  this->query_parameters_();
  this->set_config_mode_(false);
#ifdef USE_SELECT
  if (this->baud_rate_select_ != nullptr) {
    if (auto index = ld24xx::find_index(BAUD_RATES, this->parent_->get_baud_rate())) {
      this->baud_rate_select_->publish_state(*index);
    }
  }
#endif
}

void LD2410Component::restart_and_read_all_info() {
  this->set_config_mode_(true);
  this->restart_();
  this->set_timeout(1000, [this]() { this->read_all_info(); });
}

void LD2410Component::loop() {
  this->read_available<MAX_LINE_LENGTH>([this](uint8_t b) { this->readline_(b); });
}

void LD2410Component::send_command_(uint8_t command, const uint8_t *command_value, uint8_t command_value_len) {
  ESP_LOGV(TAG, "Sending COMMAND %02X", command);
  // frame header bytes
  this->write_array(CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER));
  // length bytes
  uint8_t len = 2;
  if (command_value != nullptr) {
    len += command_value_len;
  }
  // 2 length bytes (low, high) + 2 command bytes (low, high)
  uint8_t len_cmd[] = {len, 0x00, command, 0x00};
  this->write_array(len_cmd, sizeof(len_cmd));
  // command value bytes
  if (command_value != nullptr) {
    this->write_array(command_value, command_value_len);
  }
  // frame footer bytes
  this->write_array(CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER));

  if (command != CMD_ENABLE_CONF && command != CMD_DISABLE_CONF) {
    delay(50);  // NOLINT
  }
}

void LD2410Component::handle_periodic_data_() {
  // 4 frame header bytes + 2 length bytes + 1 data end byte + 1 crc byte + 4 frame footer bytes
  // data header=0xAA, data footer=0x55, crc=0x00
  if (this->buffer_pos_ < 12 || !ld2410::validate_header_footer(DATA_FRAME_HEADER, this->buffer_data_) ||
      this->buffer_data_[7] != HEADER || this->buffer_data_[this->buffer_pos_ - 6] != FOOTER ||
      this->buffer_data_[this->buffer_pos_ - 5] != CHECK) {
    return;
  }
  /*
    Data Type: 7th
    0x01: Engineering mode
    0x02: Normal mode
  */
  bool engineering_mode = this->buffer_data_[DATA_TYPES] == 0x01;
  if (this->buffer_pos_ < (engineering_mode ? ENGINEERING_FRAME_MIN : PERIODIC_FRAME_MIN)) {
    return;  // too short to hold the values
  }
#ifdef USE_SWITCH
  if (this->engineering_mode_switch_ != nullptr) {
    this->engineering_mode_switch_->publish_state(engineering_mode);
  }
#endif
  /*
    Target states: 9th
    0x00 = No target
    0x01 = Moving targets
    0x02 = Still targets
    0x03 = Moving+Still targets
  */
  const uint8_t target_state = this->buffer_data_[TARGET_STATES];
#ifdef USE_LD2410_BG_CORRECTION
  // Pass or fail comes only from these frames
  if (this->bg_correction_running_) {
    if (target_state == TARGET_STATE_BG_SUCCESS) {
      this->bg_correction_result_ = BgCorrectionResult::BG_CORRECTION_RESULT_SUCCESS;
    } else if (target_state == TARGET_STATE_BG_FAILED) {
      this->bg_correction_result_ = BgCorrectionResult::BG_CORRECTION_RESULT_FAILED;
    }
  }
#endif
#ifdef USE_BINARY_SENSOR
  if (target_state < TARGET_STATE_CALIBRATING_MIN) {
    if (this->target_binary_sensor_ != nullptr) {
      this->target_binary_sensor_->publish_state(target_state != 0x00);
    }
    if (this->moving_target_binary_sensor_ != nullptr) {
      this->moving_target_binary_sensor_->publish_state(target_state & MOVE_BITMASK);
    }
    if (this->still_target_binary_sensor_ != nullptr) {
      this->still_target_binary_sensor_->publish_state(target_state & STILL_BITMASK);
    }
  }
#endif
  /*
    Moving target distance: 10~11th bytes
    Moving target energy: 12th byte
    Still target distance: 13~14th bytes
    Still target energy: 15th byte
    Detect distance: 16~17th bytes
  */
#ifdef USE_SENSOR
  SAFE_PUBLISH_SENSOR(this->moving_target_distance_sensor_,
                      encode_uint16(this->buffer_data_[MOVING_TARGET_HIGH], this->buffer_data_[MOVING_TARGET_LOW]));
  SAFE_PUBLISH_SENSOR(this->moving_target_energy_sensor_, this->buffer_data_[MOVING_ENERGY]);
  SAFE_PUBLISH_SENSOR(this->still_target_distance_sensor_,
                      encode_uint16(this->buffer_data_[STILL_TARGET_HIGH], this->buffer_data_[STILL_TARGET_LOW]));
  SAFE_PUBLISH_SENSOR(this->still_target_energy_sensor_, this->buffer_data_[STILL_ENERGY]);
  SAFE_PUBLISH_SENSOR(this->detection_distance_sensor_,
                      encode_uint16(this->buffer_data_[DETECT_DISTANCE_HIGH], this->buffer_data_[DETECT_DISTANCE_LOW]));

  if (engineering_mode) {
    /*
      Moving distance range: 18th byte
      Still distance range: 19th byte
      Moving energy: 20~28th bytes
    */
    for (uint8_t i = 0; i < TOTAL_GATES; i++) {
      SAFE_PUBLISH_SENSOR(this->gate_move_sensors_[i], this->buffer_data_[MOVING_SENSOR_START + i]);
    }
    /*
      Still energy: 29~37th bytes
    */
    for (uint8_t i = 0; i < TOTAL_GATES; i++) {
      SAFE_PUBLISH_SENSOR(this->gate_still_sensors_[i], this->buffer_data_[STILL_SENSOR_START + i]);
    }
    /*
      Light sensor: 38th bytes
    */
    SAFE_PUBLISH_SENSOR(this->light_sensor_, this->buffer_data_[LIGHT_SENSOR]);
  } else {
    for (auto &gate_move_sensor : this->gate_move_sensors_) {
      SAFE_PUBLISH_SENSOR_UNKNOWN(gate_move_sensor);
    }
    for (auto &gate_still_sensor : this->gate_still_sensors_) {
      SAFE_PUBLISH_SENSOR_UNKNOWN(gate_still_sensor);
    }
    SAFE_PUBLISH_SENSOR_UNKNOWN(this->light_sensor_);
  }
#endif
#ifdef USE_BINARY_SENSOR
  if (this->out_pin_presence_status_binary_sensor_ != nullptr) {
    this->out_pin_presence_status_binary_sensor_->publish_state(
        engineering_mode ? this->buffer_data_[OUT_PIN_SENSOR] == 0x01 : false);
  }
#endif
}

#ifdef USE_NUMBER
void set_number_value(number::Number *n, float value) {
  if (n != nullptr && (!n->has_state() || n->state != value)) {
    n->publish_state(value);
  }
}
#endif

void LD2410Component::handle_ack_data_() {
  ESP_LOGV(TAG, "Handling ACK DATA for COMMAND %02X", this->buffer_data_[COMMAND]);
  if (this->buffer_data_[COMMAND_STATUS] != 0x01) {
    ESP_LOGE(TAG, "Invalid status");
    return;
  }
  if (this->buffer_data_[8] || this->buffer_data_[9]) {
    ESP_LOGW(TAG, "Invalid command: %02X, %02X", this->buffer_data_[8], this->buffer_data_[9]);
#ifdef USE_LD2410_BG_CORRECTION
    // Only a refused start ends the run; a refused poll waits for the next poll or the deadline
    if (this->buffer_data_[COMMAND] == CMD_BG_CORRECTION_START) {
      this->finish_background_correction_(BgCorrectionResult::BG_CORRECTION_RESULT_REFUSED);
    }
#endif
    return;
  }

  switch (this->buffer_data_[COMMAND]) {
    case CMD_ENABLE_CONF:
      ESP_LOGV(TAG, "Enable conf");
      break;

    case CMD_DISABLE_CONF:
      ESP_LOGV(TAG, "Disabled conf");
      break;

    case CMD_SET_BAUD_RATE:
      ESP_LOGV(TAG, "Baud rate change");
#ifdef USE_SELECT
      if (this->baud_rate_select_ != nullptr) {
        auto baud = this->baud_rate_select_->current_option();
        ESP_LOGE(TAG, "Change baud rate to %.*s and reinstall", (int) baud.size(), baud.c_str());
      }
#endif
      break;

    case CMD_QUERY_VERSION: {
      if (this->buffer_pos_ < 12 + sizeof(this->version_) + HEADER_FOOTER_SIZE) {
        break;  // too short to hold a version
      }
      std::memcpy(this->version_, &this->buffer_data_[12], sizeof(this->version_));
      char version_s[20];
      ld24xx::format_version_str(this->version_, version_s);
      ESP_LOGV(TAG, "Firmware version: %s", version_s);
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr) {
        this->version_text_sensor_->publish_state(version_s);
      }
#endif
      break;
    }

    case CMD_QUERY_DISTANCE_RESOLUTION: {
      if (this->buffer_pos_ < 11 + HEADER_FOOTER_SIZE) {
        break;  // too short to hold the resolution
      }
      const auto index = ld24xx::find_index(DISTANCE_RESOLUTIONS, this->buffer_data_[10]);
      if (!index) {
        ESP_LOGW(TAG, "Unknown distance resolution %u", this->buffer_data_[10]);
        break;
      }
      ESP_LOGV(TAG, "Distance resolution: %s",
               LOG_STR_ARG(DistanceResolutionStrings::get_log_str(*index, DistanceResolutionStrings::LAST_INDEX)));
#ifdef USE_SELECT
      if (this->distance_resolution_select_ != nullptr) {
        this->distance_resolution_select_->publish_state(*index);
      }
#endif
      break;
    }

    case CMD_QUERY_LIGHT_CONTROL: {
      if (this->buffer_pos_ < 13 + HEADER_FOOTER_SIZE) {
        break;  // too short to hold the light settings
      }
      this->light_function_ = this->buffer_data_[10];
      this->light_threshold_ = this->buffer_data_[11];
      this->out_pin_level_ = this->buffer_data_[12];
      ESP_LOGV(TAG, "Light function: %u, threshold: %u, out pin level: %u", this->light_function_,
               this->light_threshold_, this->out_pin_level_);
#ifdef USE_SELECT
      if (this->light_function_select_ != nullptr) {
        this->light_function_select_->publish_state(static_cast<size_t>(this->light_function_));
      }
      if (this->out_pin_level_select_ != nullptr) {
        this->out_pin_level_select_->publish_state(static_cast<size_t>(this->out_pin_level_));
      }
#endif
#ifdef USE_NUMBER
      if (this->light_threshold_number_ != nullptr) {
        this->light_threshold_number_->publish_state(static_cast<float>(this->light_threshold_));
      }
#endif
      break;
    }
    case CMD_QUERY_MAC_ADDRESS: {
      if (this->buffer_pos_ < 20) {
        break;  // too short to hold a MAC address
      }

      this->bluetooth_on_ = std::memcmp(&this->buffer_data_[10], NO_MAC, sizeof(NO_MAC)) != 0;
      if (this->bluetooth_on_) {
        std::memcpy(this->mac_address_, &this->buffer_data_[10], sizeof(this->mac_address_));
      }

      char mac_s[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      const char *mac_str = ld24xx::format_mac_str(this->mac_address_, mac_s);
      ESP_LOGV(TAG, "MAC address: %s", mac_str);
#ifdef USE_TEXT_SENSOR
      if (this->mac_text_sensor_ != nullptr) {
        this->mac_text_sensor_->publish_state(mac_str);
      }
#endif
#ifdef USE_SWITCH
      if (this->bluetooth_switch_ != nullptr) {
        this->bluetooth_switch_->publish_state(this->bluetooth_on_);
      }
#endif
      break;
    }

    case CMD_GATE_SENS:
      ESP_LOGV(TAG, "Sensitivity");
      break;

#ifdef USE_LD2410_BG_CORRECTION
    case CMD_BG_CORRECTION_START:
      ESP_LOGV(TAG, "Background correction started");
      break;

    case CMD_BG_CORRECTION_QUERY: {
      const uint8_t status = this->buffer_data_[10];
      ESP_LOGV(TAG, "Background correction status: %u", status);
      // The module stops at idle after a fail frame; idle before the run starts is ignored
      if (status == BG_CORRECTION_COMPLETED ||
          (status == BG_CORRECTION_IDLE &&
           this->bg_correction_result_ == BgCorrectionResult::BG_CORRECTION_RESULT_FAILED)) {
        this->finish_background_correction_(this->bg_correction_result_);
      }
      break;
    }
#endif

    case CMD_BLUETOOTH:
      ESP_LOGV(TAG, "Bluetooth");
      break;

    case CMD_SET_DISTANCE_RESOLUTION:
      ESP_LOGV(TAG, "Set distance resolution");
      break;

    case CMD_SET_LIGHT_CONTROL:
      ESP_LOGV(TAG, "Set light control");
      break;

    case CMD_BT_PASSWORD:
      ESP_LOGV(TAG, "Set bluetooth password");
      break;

    case CMD_QUERY: {  // Query parameters response
      if (this->buffer_pos_ < 38) {
        break;  // too short to hold the parameters
      }
      if (this->buffer_data_[10] != HEADER)
        return;  // value head=0xAA
#ifdef USE_NUMBER
      /*
        Moving distance range: 13th byte
        Still distance range: 14th byte
      */
      set_number_value(this->max_move_distance_gate_number_, this->buffer_data_[12]);
      set_number_value(this->max_still_distance_gate_number_, this->buffer_data_[13]);
      /*
        Moving Sensitivities: 15~23th bytes
      */
      for (size_t i = 0; i < TOTAL_GATES; i++) {
        set_number_value(this->gate_move_threshold_numbers_[i], this->buffer_data_[14 + i]);
      }
      /*
        Still Sensitivities: 24~32th bytes
      */
      for (size_t i = 0; i < TOTAL_GATES; i++) {
        set_number_value(this->gate_still_threshold_numbers_[i], this->buffer_data_[23 + i]);
      }
      /*
        None Duration: 33~34th bytes
      */
      set_number_value(this->timeout_number_, encode_uint16(this->buffer_data_[33], this->buffer_data_[32]));
#endif
      break;
    }
    default:
      break;
  }
}

void LD2410Component::readline_(int readch) {
  if (readch < 0) {
    return;  // No data available
  }
  // Periodic and command frames both carry their count after the header
  ld24xx::feed_frame_byte(
      this->buffer_data_, this->buffer_pos_, readch, DATA_FRAME_HEADER, ld24xx::cmd_frame_length,
      [this](uint8_t frame_len) { return this->handle_frame_(frame_len); }, []() { ESP_LOGW(TAG, "Invalid length"); });
}

// Dispatches the frame of frame_len bytes at the start of the buffer when its footer is in place
bool LD2410Component::handle_frame_(uint8_t frame_len) {
  this->buffer_pos_ = frame_len;  // the handlers take the frame length from here
  const uint8_t *tail = &this->buffer_data_[frame_len - HEADER_FOOTER_SIZE];
  if (this->buffer_data_[0] == DATA_FRAME_HEADER[0]) {
    if (!ld2410::validate_header_footer(DATA_FRAME_FOOTER, tail)) {
      return false;
    }
    ESP_LOGV_HEX(TAG, this->buffer_data_, this->buffer_pos_, MAX_LINE_LENGTH, "Handling Periodic Data: %s");
    this->handle_periodic_data_();
    return true;
  }
  if (!ld2410::validate_header_footer(CMD_FRAME_FOOTER, tail)) {
    return false;
  }
  ESP_LOGV_HEX(TAG, this->buffer_data_, this->buffer_pos_, MAX_LINE_LENGTH, "Handling Ack Data: %s");
  this->handle_ack_data_();
  return true;
}

void LD2410Component::set_config_mode_(bool enable) {
  const uint8_t cmd = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  const uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(cmd, enable ? cmd_value : nullptr, sizeof(cmd_value));
}

void LD2410Component::set_bluetooth(bool enable) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {enable ? (uint8_t) 0x01 : (uint8_t) 0x00, 0x00};
  this->send_command_(CMD_BLUETOOTH, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

void LD2410Component::set_distance_resolution(const char *state) {
  const int16_t index = progmem_string_table_find<DistanceResolutionStrings>(state, strlen(state));
  if (index >= 0) {
    this->set_distance_resolution(static_cast<DistanceResolution>(progmem_read_byte(&DISTANCE_RESOLUTIONS[index])));
  }
}

void LD2410Component::set_distance_resolution(DistanceResolution resolution) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {resolution, 0x00};
  this->send_command_(CMD_SET_DISTANCE_RESOLUTION, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

void LD2410Component::set_baud_rate(const char *state) {
  const int16_t index = progmem_string_table_find<BaudRateStrings>(state, strlen(state));
  if (index >= 0) {
    this->set_baud_rate(static_cast<BaudRate>(index + 1));
  }
}

void LD2410Component::set_baud_rate(BaudRate rate) {
  this->set_config_mode_(true);
  const uint8_t cmd_value[2] = {rate, 0x00};
  this->send_command_(CMD_SET_BAUD_RATE, cmd_value, sizeof(cmd_value));
  this->set_timeout(200, [this]() { this->restart_(); });
}

void LD2410Component::set_bluetooth_password(const std::string &password) {
  if (password.length() != 6) {
    ESP_LOGE(TAG, "Password must be exactly 6 chars");
    return;
  }
  this->set_config_mode_(true);
  uint8_t cmd_value[6];
  std::copy(password.begin(), password.end(), std::begin(cmd_value));
  this->send_command_(CMD_BT_PASSWORD, cmd_value, sizeof(cmd_value));
  this->set_config_mode_(false);
}

void LD2410Component::set_engineering_mode(bool enable) {
  const uint8_t cmd = enable ? CMD_ENABLE_ENG : CMD_DISABLE_ENG;
  this->set_config_mode_(true);
  this->send_command_(cmd, nullptr, 0);
  this->set_config_mode_(false);
}

void LD2410Component::factory_reset() {
  this->set_config_mode_(true);
  this->send_command_(CMD_RESET, nullptr, 0);
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
}

void LD2410Component::restart_() { this->send_command_(CMD_RESTART, nullptr, 0); }

void LD2410Component::query_parameters_() { this->send_command_(CMD_QUERY, nullptr, 0); }

void LD2410Component::get_version_() { this->send_command_(CMD_QUERY_VERSION, nullptr, 0); }

void LD2410Component::get_mac_() {
  const uint8_t cmd_value[2] = {0x01, 0x00};
  this->send_command_(CMD_QUERY_MAC_ADDRESS, cmd_value, sizeof(cmd_value));
}

void LD2410Component::get_distance_resolution_() { this->send_command_(CMD_QUERY_DISTANCE_RESOLUTION, nullptr, 0); }

void LD2410Component::query_light_control_() { this->send_command_(CMD_QUERY_LIGHT_CONTROL, nullptr, 0); }

#ifdef USE_LD2410_BG_CORRECTION
void LD2410Component::start_background_correction() {
  if (this->bg_correction_running_) {
    ESP_LOGW(TAG, "Background correction already running");
    return;
  }
  ESP_LOGI(TAG, "Starting background correction (%u s); the room must be empty", this->bg_correction_duration_);
  this->set_config_mode_(true);
  const uint8_t value[2] = {this->bg_correction_duration_, 0x00};
  this->send_command_(CMD_BG_CORRECTION_START, value, sizeof(value));
  this->set_config_mode_(false);
  this->bg_correction_result_ = BgCorrectionResult::BG_CORRECTION_RESULT_UNCONFIRMED;
  this->bg_correction_running_ = true;
  this->publish_calibration_running_(true);
  this->publish_calibration_result_(BgCorrectionResult::BG_CORRECTION_RESULT_RUNNING);
  this->set_interval(BG_CORRECTION_POLL_ID, BG_CORRECTION_POLL_INTERVAL_MS, [this]() {
    this->set_config_mode_(true);
    this->send_command_(CMD_BG_CORRECTION_QUERY, nullptr, 0);
    this->set_config_mode_(false);
  });
  // Firmware without 0x1B never reports completion
  this->set_timeout(
      BG_CORRECTION_DEADLINE_ID, this->bg_correction_duration_ * 1000U + BG_CORRECTION_EXTRA_MS,
      [this]() { this->finish_background_correction_(BgCorrectionResult::BG_CORRECTION_RESULT_TIMED_OUT); });
}

void LD2410Component::finish_background_correction_(BgCorrectionResult result) {
  if (!this->bg_correction_running_) {
    return;
  }
  this->cancel_interval(BG_CORRECTION_POLL_ID);
  this->cancel_timeout(BG_CORRECTION_DEADLINE_ID);
  ESP_LOGI(TAG, "Background correction: %s",
           LOG_STR_ARG(BgCorrectionResultStrings::get_log_str(static_cast<uint8_t>(result), UNCONFIRMED_INDEX)));
  this->publish_calibration_result_(result);
  // The module may have rewritten the gate thresholds
  this->set_config_mode_(true);
  this->query_parameters_();
  this->set_config_mode_(false);
  // Last, so only an automation on this state can start the next run
  this->bg_correction_running_ = false;
  this->publish_calibration_running_(false);
}

void LD2410Component::publish_calibration_running_(bool running) {
#ifdef USE_BINARY_SENSOR
  if (this->calibration_binary_sensor_ != nullptr) {
    this->calibration_binary_sensor_->publish_state(running);
  }
#endif
}

void LD2410Component::publish_calibration_result_(BgCorrectionResult result) {
#ifdef USE_TEXT_SENSOR
  if (this->calibration_result_text_sensor_ != nullptr) {
    this->calibration_result_text_sensor_->publish_state(
        BgCorrectionResultStrings::get_progmem_str(static_cast<uint8_t>(result), UNCONFIRMED_INDEX));
  }
#endif
}
#endif

#ifdef USE_NUMBER
void LD2410Component::set_max_distances_timeout() {
  if (!this->max_move_distance_gate_number_->has_state() || !this->max_still_distance_gate_number_->has_state() ||
      !this->timeout_number_->has_state()) {
    return;
  }
  int max_moving_distance_gate_range = static_cast<int>(this->max_move_distance_gate_number_->state);
  int max_still_distance_gate_range = static_cast<int>(this->max_still_distance_gate_number_->state);
  int timeout = static_cast<int>(this->timeout_number_->state);
  uint8_t value[18] = {0x00,
                       0x00,
                       lowbyte(max_moving_distance_gate_range),
                       highbyte(max_moving_distance_gate_range),
                       0x00,
                       0x00,
                       0x01,
                       0x00,
                       lowbyte(max_still_distance_gate_range),
                       highbyte(max_still_distance_gate_range),
                       0x00,
                       0x00,
                       0x02,
                       0x00,
                       lowbyte(timeout),
                       highbyte(timeout),
                       0x00,
                       0x00};
  this->set_config_mode_(true);
  this->send_command_(CMD_MAXDIST_DURATION, value, sizeof(value));
  this->query_parameters_();
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
  this->set_config_mode_(false);
}

void LD2410Component::set_gate_threshold(number::Number *n) {
  // Found by pointer: up to 18 compares per write beat storing the gate in each number
  uint8_t gate = 0;
  while (gate < TOTAL_GATES && this->gate_move_threshold_numbers_[gate] != n &&
         this->gate_still_threshold_numbers_[gate] != n)
    gate++;
  if (gate == TOTAL_GATES) {
    ESP_LOGW(TAG, "Threshold number is not a known gate");
    return;
  }
  number::Number *motionsens = this->gate_move_threshold_numbers_[gate];
  number::Number *stillsens = this->gate_still_threshold_numbers_[gate];

  if (!motionsens->has_state() || !stillsens->has_state()) {
    return;
  }
  int motion = static_cast<int>(motionsens->state);
  int still = static_cast<int>(stillsens->state);

  this->set_config_mode_(true);
  // reference
  // https://drive.google.com/drive/folders/1p4dhbEJA3YubyIjIIC7wwVsSo8x29Fq-?spm=a2g0o.detail.1000023.17.93465697yFwVxH
  //   Send data: configure the motion sensitivity of distance gate 3 to 40, and the static sensitivity of 40
  // 00 00 (gate)
  // 03 00 00 00 (gate number)
  // 01 00 (motion sensitivity)
  // 28 00 00 00 (value)
  // 02 00 (still sensitivtiy)
  // 28 00 00 00 (value)
  uint8_t value[18] = {0x00, 0x00, lowbyte(gate),   highbyte(gate),   0x00, 0x00,
                       0x01, 0x00, lowbyte(motion), highbyte(motion), 0x00, 0x00,
                       0x02, 0x00, lowbyte(still),  highbyte(still),  0x00, 0x00};
  this->send_command_(CMD_GATE_SENS, value, sizeof(value));
  this->query_parameters_();
  this->set_config_mode_(false);
}

void LD2410Component::set_gate_still_threshold_number(uint8_t gate, number::Number *n) {
  this->gate_still_threshold_numbers_[gate] = n;
}

void LD2410Component::set_gate_move_threshold_number(uint8_t gate, number::Number *n) {
  this->gate_move_threshold_numbers_[gate] = n;
}
#endif

void LD2410Component::set_light_out_control() {
#ifdef USE_NUMBER
  if (this->light_threshold_number_ != nullptr && this->light_threshold_number_->has_state()) {
    this->light_threshold_ = static_cast<uint8_t>(this->light_threshold_number_->state);
  }
#endif
#ifdef USE_SELECT
  if (this->light_function_select_ != nullptr) {
    if (const auto index = this->light_function_select_->active_index()) {
      this->light_function_ = *index;
    }
  }
  if (this->out_pin_level_select_ != nullptr) {
    if (const auto index = this->out_pin_level_select_->active_index()) {
      this->out_pin_level_ = *index;
    }
  }
#endif
  this->set_config_mode_(true);
  uint8_t value[4] = {this->light_function_, this->light_threshold_, this->out_pin_level_, 0x00};
  this->send_command_(CMD_SET_LIGHT_CONTROL, value, sizeof(value));
  this->query_light_control_();
  this->set_timeout(200, [this]() { this->restart_and_read_all_info(); });
  this->set_config_mode_(false);
}

#ifdef USE_SENSOR
void LD2410Component::set_gate_move_sensor(uint8_t gate, sensor::Sensor *s) {
  this->gate_move_sensors_[gate].set_sensor(s);
}

void LD2410Component::set_gate_still_sensor(uint8_t gate, sensor::Sensor *s) {
  this->gate_still_sensors_[gate].set_sensor(s);
}
#endif

}  // namespace esphome::ld2410
