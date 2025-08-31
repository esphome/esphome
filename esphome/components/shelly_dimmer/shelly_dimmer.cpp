#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP8266

#include "shelly_dimmer.h"
#ifdef USE_SHD_FIRMWARE_DATA
#include "stm32flash.h"
#endif

#ifndef USE_ESP_IDF
#include <HardwareSerial.h>
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <numeric>

namespace {

constexpr char TAG[] = "shelly_dimmer";

constexpr float CALIBRATION_STEP = 0.05f;
constexpr uint32_t RESTORE_STATE_VERSION = 0x362A4931UL;

constexpr uint8_t SHELLY_DIMMER_ACK_TIMEOUT = 200;  // ms
constexpr uint8_t SHELLY_DIMMER_MAX_RETRIES = 3;
constexpr uint16_t SHELLY_DIMMER_MAX_BRIGHTNESS = 1000;  // 100%

// Protocol framing.
constexpr uint8_t SHELLY_DIMMER_PROTO_START_BYTE = 0x01;
constexpr uint8_t SHELLY_DIMMER_PROTO_END_BYTE = 0x04;

// Supported commands.
constexpr uint8_t SHELLY_DIMMER_PROTO_CMD_SWITCH = 0x01;
constexpr uint8_t SHELLY_DIMMER_PROTO_CMD_POLL = 0x10;
constexpr uint8_t SHELLY_DIMMER_PROTO_CMD_VERSION = 0x11;
constexpr uint8_t SHELLY_DIMMER_PROTO_CMD_SETTINGS = 0x20;

// Command payload sizes.
constexpr uint8_t SHELLY_DIMMER_PROTO_CMD_SWITCH_SIZE = 2;
constexpr uint8_t SHELLY_DIMMER_PROTO_CMD_SETTINGS_SIZE = 10;
constexpr uint8_t SHELLY_DIMMER_PROTO_MAX_FRAME_SIZE = 4 + 72 + 3;

// STM Firmware
#ifdef USE_SHD_FIRMWARE_DATA
constexpr uint8_t STM_FIRMWARE[] PROGMEM = USE_SHD_FIRMWARE_DATA;
constexpr uint32_t STM_FIRMWARE_SIZE_IN_BYTES = sizeof(STM_FIRMWARE);
#endif

// Scaling Constants
constexpr float POWER_SCALING_FACTOR = 880373;
constexpr float VOLTAGE_SCALING_FACTOR = 347800;
constexpr float CURRENT_SCALING_FACTOR = 1448;

// Essentially std::size() for pre c++17
template<typename T, size_t N> constexpr size_t size(const T (&/*unused*/)[N]) noexcept { return N; }

}  // Anonymous namespace

namespace esphome {
namespace shelly_dimmer {

/// Computes a crappy checksum as defined by the Shelly Dimmer protocol.
uint16_t shelly_dimmer_checksum(const uint8_t *buf, int len) {
  return std::accumulate<decltype(buf), uint16_t>(buf, buf + len, 0);
}

bool ShellyDimmer::is_running_configured_version() const {
  return this->version_major_ == USE_SHD_FIRMWARE_MAJOR_VERSION &&
         this->version_minor_ == USE_SHD_FIRMWARE_MINOR_VERSION;
}

void ShellyDimmer::handle_firmware() {
  // Reset the STM32 and check the firmware version.
  this->reset_normal_boot_();
  this->send_command_(SHELLY_DIMMER_PROTO_CMD_VERSION, nullptr, 0);
  ESP_LOGI(TAG, "STM32 current firmware version: %d.%d, desired version: %d.%d", this->version_major_,
           this->version_minor_, USE_SHD_FIRMWARE_MAJOR_VERSION, USE_SHD_FIRMWARE_MINOR_VERSION);

  if (!is_running_configured_version()) {
#ifdef USE_SHD_FIRMWARE_DATA
    if (!this->upgrade_firmware_()) {
      ESP_LOGW(TAG, "Failed to upgrade firmware");
      this->mark_failed();
      return;
    }

    this->reset_normal_boot_();
    this->send_command_(SHELLY_DIMMER_PROTO_CMD_VERSION, nullptr, 0);
    if (!is_running_configured_version()) {
      ESP_LOGE(TAG, "STM32 firmware upgrade already performed, but version is still incorrect");
      this->mark_failed();
      return;
    }
#else
    ESP_LOGW(TAG, "Firmware version mismatch, put 'update: true' in the yaml to flash an update.");
#endif
  }
}

void ShellyDimmer::setup() {
  this->pin_nrst_->setup();
  this->pin_boot0_->setup();

  this->handle_firmware();

  this->send_settings_();
  // Do an immediate poll to refresh current state.
  this->send_command_(SHELLY_DIMMER_PROTO_CMD_POLL, nullptr, 0);

  this->calibration_data_.fill(0);
  this->rtc_ = global_preferences->make_preference<std::array<float, 20>>(this->state_->get_object_id_hash() ^
                                                                          RESTORE_STATE_VERSION);
  if (this->rtc_.load(&this->calibration_data_)) {
    ESP_LOGD(TAG, "Loaded calibration from flash");
    for (float value : this->calibration_data_) {
      ESP_LOGV(TAG, "%f", value);
    }
  }

  this->ready_ = true;
}

void ShellyDimmer::update() {
  this->send_command_(SHELLY_DIMMER_PROTO_CMD_POLL, nullptr, 0);

  if (this->calibrating_) {
    this->perform_calibration_measurement_();
  }
}

void ShellyDimmer::dump_config() {
  ESP_LOGCONFIG(TAG, "ShellyDimmer:");
  LOG_PIN("  NRST Pin: ", this->pin_nrst_);
  LOG_PIN("  BOOT0 Pin: ", this->pin_boot0_);

  ESP_LOGCONFIG(TAG,
                "  Leading Edge: %s\n"
                "  Warmup Brightness: %d\n"
                "  Minimum Brightness: %d\n"
                "  Maximum Brightness: %d",
                YESNO(this->leading_edge_), this->warmup_brightness_, this->min_brightness_, this->max_brightness_);
  // ESP_LOGCONFIG(TAG, "  Warmup Time: %d", this->warmup_time_);
  // ESP_LOGCONFIG(TAG, "  Fade Rate: %d", this->fade_rate_);

  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG,
                "  STM32 current firmware version: %d.%d \n"
                "  STM32 required firmware version: %d.%d",
                this->version_major_, this->version_minor_, USE_SHD_FIRMWARE_MAJOR_VERSION,
                USE_SHD_FIRMWARE_MINOR_VERSION);
  ESP_LOGCONFIG(TAG, "  Calibrated: %s", YESNO(this->calibration_data_[0] != 0));

  if (this->version_major_ != USE_SHD_FIRMWARE_MAJOR_VERSION ||
      this->version_minor_ != USE_SHD_FIRMWARE_MINOR_VERSION) {
    ESP_LOGE(TAG, "  Firmware version mismatch, put 'update: true' in the yaml to flash an update.");
  }
}

void ShellyDimmer::write_state(light::LightState *state) {
  if (!this->ready_) {
    return;
  }

  float brightness;
  state->current_values_as_brightness(&brightness);

  // If we are in a process of calibration, don't mess with brightness.
  // Also check whether we have calibration data and wheteher edge values were requested.
  if (!this->calibrating_ && this->calibration_data_[0] != 0.0f && brightness != 0 && brightness != 1.0f) {
    // We have calibration data, find the nearest range and remap value
    uint32_t pos;
    for (pos = 0; pos < this->calibration_data_.size(); ++pos) {
      if (this->calibration_data_[pos] < brightness) {
        break;
      }
    }
    if (pos == this->calibration_data_.size() || pos == 0) {
      ESP_LOGW(TAG, "Failed to find suitable calibration range for brightness %f", brightness);
    } else {
      float min = this->calibration_data_[pos];
      float max = this->calibration_data_[pos - 1];
      float min_out = 1 - (float) pos * CALIBRATION_STEP;
      float max_out = min_out + CALIBRATION_STEP;
      float remapped = remap(brightness, min, max, min_out, max_out);

      ESP_LOGD(TAG, "Remapped %f to %f (min %f, max %f, min_out %f, max_out %f)", brightness, remapped, min, max,
               min_out, max_out);

      brightness = remapped;
    }
  }

  const uint16_t brightness_int = this->convert_brightness_(brightness);
  if (brightness_int == this->brightness_) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  ESP_LOGD(TAG, "Brightness update: %d (raw: %f)", brightness_int, brightness);

  this->send_brightness_(brightness_int);
}
#ifdef USE_SHD_FIRMWARE_DATA
bool ShellyDimmer::upgrade_firmware_() {
  ESP_LOGW(TAG, "Starting STM32 firmware upgrade");
  this->reset_dfu_boot_();

  // Cleanup with RAII
  auto stm32 = stm32_init(this, STREAM_SERIAL, 1);

  if (!stm32) {
    ESP_LOGW(TAG, "Failed to initialize STM32");
    return false;
  }

  // Erase STM32 flash.
  if (stm32_erase_memory(stm32, 0, STM32_MASS_ERASE) != STM32_ERR_OK) {
    ESP_LOGW(TAG, "Failed to erase STM32 flash memory");
    return false;
  }

  static constexpr uint32_t BUFFER_SIZE = 256;

  // Copy the STM32 firmware over in 256-byte chunks. Note that the firmware is stored
  // in flash memory so all accesses need to be 4-byte aligned.
  uint8_t buffer[BUFFER_SIZE];
  const uint8_t *p = STM_FIRMWARE;
  uint32_t offset = 0;
  uint32_t addr = stm32->dev->fl_start;
  const uint32_t end = addr + STM_FIRMWARE_SIZE_IN_BYTES;

  while (addr < end && offset < STM_FIRMWARE_SIZE_IN_BYTES) {
    const uint32_t left_of_buffer = std::min(end - addr, BUFFER_SIZE);
    const uint32_t len = std::min(left_of_buffer, STM_FIRMWARE_SIZE_IN_BYTES - offset);

    if (len == 0) {
      break;
    }

    std::memcpy(buffer, p, BUFFER_SIZE);
    p += BUFFER_SIZE;

    if (stm32_write_memory(stm32, addr, buffer, len) != STM32_ERR_OK) {
      ESP_LOGW(TAG, "Failed to write to STM32 flash memory");
      return false;
    }

    addr += len;
    offset += len;
  }

  ESP_LOGI(TAG, "STM32 firmware upgrade successful");

  return true;
}
#endif

uint16_t ShellyDimmer::convert_brightness_(float brightness) {
  // Special case for zero as only zero means turn off completely.
  if (brightness == 0.0) {
    return 0;
  }

  return remap<uint16_t, float>(brightness, 0.0f, 1.0f, this->min_brightness_, this->max_brightness_);
}

void ShellyDimmer::send_brightness_(uint16_t brightness) {
  const uint8_t payload[] = {
      // Brightness (%) * 10.
      static_cast<uint8_t>(brightness & 0xff),
      static_cast<uint8_t>(brightness >> 8),
  };
  static_assert(size(payload) == SHELLY_DIMMER_PROTO_CMD_SWITCH_SIZE, "Invalid payload size");

  this->send_command_(SHELLY_DIMMER_PROTO_CMD_SWITCH, payload, SHELLY_DIMMER_PROTO_CMD_SWITCH_SIZE);

  this->brightness_ = brightness;
}

void ShellyDimmer::send_settings_() {
  const uint16_t fade_rate = std::min(uint16_t{100}, this->fade_rate_);

  float brightness = 0.0;
  if (this->state_ != nullptr) {
    this->state_->current_values_as_brightness(&brightness);
  }
  const uint16_t brightness_int = this->convert_brightness_(brightness);
  ESP_LOGD(TAG, "Brightness update: %d (raw: %f)", brightness_int, brightness);

  const uint8_t payload[] = {
      // Brightness (%) * 10.
      static_cast<uint8_t>(brightness_int & 0xff),
      static_cast<uint8_t>(brightness_int >> 8),
      // Leading / trailing edge [0x01 = leading, 0x02 = trailing].
      this->leading_edge_ ? uint8_t{0x01} : uint8_t{0x02},
      0x00,
      // Fade rate.
      static_cast<uint8_t>(fade_rate & 0xff),
      static_cast<uint8_t>(fade_rate >> 8),
      // Warmup brightness.
      static_cast<uint8_t>(this->warmup_brightness_ & 0xff),
      static_cast<uint8_t>(this->warmup_brightness_ >> 8),
      // Warmup time.
      static_cast<uint8_t>(this->warmup_time_ & 0xff),
      static_cast<uint8_t>(this->warmup_time_ >> 8),
  };
  static_assert(size(payload) == SHELLY_DIMMER_PROTO_CMD_SETTINGS_SIZE, "Invalid payload size");

  this->send_command_(SHELLY_DIMMER_PROTO_CMD_SETTINGS, payload, SHELLY_DIMMER_PROTO_CMD_SETTINGS_SIZE);

  // Also send brightness separately as it is ignored above.
  this->send_brightness_(brightness_int);
}

bool ShellyDimmer::send_command_(uint8_t cmd, const uint8_t *const payload, uint8_t len) {
  ESP_LOGD(TAG, "Sending command: 0x%02x (%d bytes) payload 0x%s", cmd, len, format_hex(payload, len).c_str());

  // Prepare a command frame.
  uint8_t frame[SHELLY_DIMMER_PROTO_MAX_FRAME_SIZE];
  const size_t frame_len = this->frame_command_(frame, cmd, payload, len);

  // Write the frame and wait for acknowledgement.
  int retries = SHELLY_DIMMER_MAX_RETRIES;
  while (retries--) {
    this->write_array(frame, frame_len);
    this->flush();

    ESP_LOGD(TAG, "Command sent, waiting for reply");
    const uint32_t tx_time = millis();
    while (millis() - tx_time < SHELLY_DIMMER_ACK_TIMEOUT) {
      if (this->read_frame_()) {
        return true;
      }
      delay(1);
    }
    ESP_LOGW(TAG, "Timeout while waiting for reply");
  }
  ESP_LOGW(TAG, "Failed to send command");
  return false;
}

size_t ShellyDimmer::frame_command_(uint8_t *data, uint8_t cmd, const uint8_t *const payload, size_t len) {
  size_t pos = 0;

  // Generate a frame.
  data[0] = SHELLY_DIMMER_PROTO_START_BYTE;
  data[1] = ++this->seq_;
  data[2] = cmd;
  data[3] = len;
  pos += 4;

  if (payload != nullptr) {
    std::memcpy(data + 4, payload, len);
    pos += len;
  }

  // Calculate checksum for the payload.
  const uint16_t csum = shelly_dimmer_checksum(data + 1, 3 + len);
  data[pos++] = static_cast<uint8_t>(csum >> 8);
  data[pos++] = static_cast<uint8_t>(csum & 0xff);
  data[pos++] = SHELLY_DIMMER_PROTO_END_BYTE;
  return pos;
}

int ShellyDimmer::handle_byte_(uint8_t c) {
  const uint8_t pos = this->buffer_pos_;

  if (pos == 0) {
    // Must be start byte.
    return c == SHELLY_DIMMER_PROTO_START_BYTE ? 1 : -1;
  } else if (pos < 4) {
    // Header.
    return 1;
  }

  // Decode payload length from header.
  const uint8_t payload_len = this->buffer_[3];
  if ((4 + payload_len + 3) > SHELLY_DIMMER_BUFFER_SIZE) {
    return -1;
  }

  if (pos < 4 + payload_len + 1) {
    // Payload.
    return 1;
  }

  if (pos == 4 + payload_len + 1) {
    // Verify checksum.
    const uint16_t csum = (this->buffer_[pos - 1] << 8 | c);
    const uint16_t csum_verify = shelly_dimmer_checksum(&this->buffer_[1], 3 + payload_len);
    if (csum != csum_verify) {
      return -1;
    }
    return 1;
  }

  if (pos == 4 + payload_len + 2) {
    // Must be end byte.
    return c == SHELLY_DIMMER_PROTO_END_BYTE ? 0 : -1;
  }
  return -1;
}

bool ShellyDimmer::read_frame_() {
  while (this->available()) {
    const uint8_t c = this->read();
    this->buffer_[this->buffer_pos_] = c;

    ESP_LOGV(TAG, "Read byte: 0x%02x (pos %d)", c, this->buffer_pos_);

    switch (this->handle_byte_(c)) {
      case 0: {
        // Frame successfully received.
        this->handle_frame_();
        this->buffer_pos_ = 0;
        return true;
      }
      case -1: {
        // Failure.
        this->buffer_pos_ = 0;
        break;
      }
      case 1: {
        // Need more data.
        this->buffer_pos_++;
        break;
      }
    }
  }
  return false;
}

bool ShellyDimmer::handle_frame_() {
  const uint8_t seq = this->buffer_[1];
  const uint8_t cmd = this->buffer_[2];
  const uint8_t payload_len = this->buffer_[3];

  ESP_LOGD(TAG, "Got frame: 0x%02x", cmd);

  // Compare with expected identifier as the frame is always a response to
  // our previously sent command.
  if (seq != this->seq_) {
    return false;
  }

  const uint8_t *payload = &this->buffer_[4];

  // Handle response.
  switch (cmd) {
    case SHELLY_DIMMER_PROTO_CMD_POLL: {
      if (payload_len < 16) {
        return false;
      }

      const uint8_t hw_version = payload[0];
      // payload[1] is unused.
      const uint16_t brightness = encode_uint16(payload[3], payload[2]);

      const uint32_t power_raw = encode_uint32(payload[7], payload[6], payload[5], payload[4]);

      const uint32_t voltage_raw = encode_uint32(payload[11], payload[10], payload[9], payload[8]);

      const uint32_t current_raw = encode_uint32(payload[15], payload[14], payload[13], payload[12]);

      const uint16_t fade_rate = payload[16];

      float power = 0;
      if (power_raw > 0) {
        power = POWER_SCALING_FACTOR / static_cast<float>(power_raw);
      }

      float voltage = 0;
      if (voltage_raw > 0) {
        voltage = VOLTAGE_SCALING_FACTOR / static_cast<float>(voltage_raw);
      }

      float current = 0;
      if (current_raw > 0) {
        current = CURRENT_SCALING_FACTOR / static_cast<float>(current_raw);
      }

      ESP_LOGD(TAG, "Got dimmer data:");
      ESP_LOGD(TAG, "  HW version: %d", hw_version);
      ESP_LOGD(TAG, "  Brightness: %d", brightness);
      ESP_LOGD(TAG, "  Fade rate:  %d", fade_rate);
      ESP_LOGD(TAG, "  Power:      %f W", power);
      ESP_LOGD(TAG, "  Voltage:    %f V", voltage);
      ESP_LOGD(TAG, "  Current:    %f A", current);

      // Update sensors.
      if (this->power_sensor_ != nullptr) {
        this->power_sensor_->publish_state(power);
      }
      if (this->voltage_sensor_ != nullptr) {
        this->voltage_sensor_->publish_state(voltage);
      }
      if (this->current_sensor_ != nullptr) {
        this->current_sensor_->publish_state(current);
      }

      return true;
    }
    case SHELLY_DIMMER_PROTO_CMD_VERSION: {
      if (payload_len < 2) {
        return false;
      }

      this->version_minor_ = payload[0];
      this->version_major_ = payload[1];
      return true;
    }
    case SHELLY_DIMMER_PROTO_CMD_SWITCH:
    case SHELLY_DIMMER_PROTO_CMD_SETTINGS: {
      return payload_len >= 1 && payload[0] == 0x01;
    }
    default: {
      return false;
    }
  }
}

void ShellyDimmer::reset_(bool boot0) {
  ESP_LOGD(TAG, "Reset STM32, boot0=%d", boot0);

  this->pin_boot0_->digital_write(boot0);
  this->pin_nrst_->digital_write(false);

  // Wait 50ms for the STM32 to reset.
  delay(50);  // NOLINT

  // Clear receive buffer.
  while (this->available()) {
    this->read();
  }

  this->pin_nrst_->digital_write(true);
  // Wait 50ms for the STM32 to boot.
  delay(50);  // NOLINT

  ESP_LOGD(TAG, "Reset STM32 done");
}

void ShellyDimmer::reset_normal_boot_() {
  // set NONE parity in normal mode

#ifndef USE_ESP_IDF  // workaround for reconfiguring the uart
  Serial.end();
  Serial.begin(115200, SERIAL_8N1);
  Serial.flush();
#endif

  this->flush();
  this->reset_(false);
}

void ShellyDimmer::reset_dfu_boot_() {
  // set EVEN parity in bootloader mode

#ifndef USE_ESP_IDF  // workaround for reconfiguring the uart
  Serial.end();
  Serial.begin(115200, SERIAL_8E1);
  Serial.flush();
#endif

  this->flush();
  this->reset_(true);
}

void ShellyDimmer::start_calibration() {
  ESP_LOGD(TAG, "Setting update interval to 1 second");
  this->update_interval_original_ = this->get_update_interval();
  this->stop_poller();
  this->set_update_interval(1000);
  this->start_poller();

  ESP_LOGI(TAG, "Starting calibration");
  // Turn on the light, disable transition, set max brightness
  this->set_brightness_no_transition_(1);

  // Init calibration data
  this->calibration_data_.fill(0);
  this->calibration_measurements_.fill(0);
  this->calibration_measurement_cnt_ = 0;
  this->calibration_step_ = -20;
  this->calibrating_ = true;
}

void ShellyDimmer::perform_calibration_measurement_() {
  if (!this->power_sensor_->has_state())  // Wait for power sensor to receive data
    return;

  if (this->calibration_step_ < 0) {
    ESP_LOGD(TAG, "Calibration warmup. Steps till calibration: %d", -this->calibration_step_);
    this->calibration_step_++;
    if (this->calibration_step_ == 0) {
      ESP_LOGD(TAG, "Calibration warmup complete");
    }
    return;
  }

  ESP_LOGD(TAG, "Calibration step %d, measurement %d", this->calibration_step_ + 1,
           this->calibration_measurement_cnt_ + 1);

  this->calibration_measurements_[this->calibration_measurement_cnt_] = this->power_sensor_->state;
  this->calibration_measurement_cnt_++;

  if ((uint32_t) this->calibration_measurement_cnt_ >= this->calibration_measurements_.size()) {
    this->complete_calibration_step_();
  }
}

void ShellyDimmer::complete_calibration_step_() {
  // Calculate mean power across measurements at this step
  float result = 0;
  for (float measurement : this->calibration_measurements_) {
    result += measurement;
  }

  result = result / (float) this->calibration_measurements_.size();

  ESP_LOGD(TAG, "Mean power at step %d: %f", this->calibration_step_ + 1, result);

  // Prepare for the next measurement
  this->calibration_data_[this->calibration_step_] = result;
  this->calibration_step_++;
  this->calibration_measurement_cnt_ = 0;
  this->calibration_measurements_.fill(0);

  // If all measurements collected, finish calibration
  if ((uint32_t) this->calibration_step_ >= this->calibration_data_.size()) {
    this->complete_calibration_();
    return;
  }

  // Decrease brightness for next set of measurements
  float cur_brightness = 0;
  this->state_->current_values_as_brightness(&cur_brightness);
  this->set_brightness_no_transition_(cur_brightness - CALIBRATION_STEP);
}

void ShellyDimmer::complete_calibration_() {
  this->calibrating_ = false;

  // Sort the values, since power readings can be jittery due to voltage fluctuations
  std::sort(this->calibration_data_.begin(), this->calibration_data_.end(), std::greater{});

  // Normalize values in the range of [0..1]
  float max = this->calibration_data_[0];
  float min = this->calibration_data_[this->calibration_data_.size() - 1];
  for (float &value : this->calibration_data_) {
    value = remap(value, min, max, 0.0f, 1.0f);
  }

  this->save_calibration_();

  ESP_LOGD(TAG, "Finished calibration. Values:");
  for (float value : this->calibration_data_) {
    ESP_LOGD(TAG, "%f", value);
  }

  this->set_brightness_no_transition_(1);

  uint32_t update_interval = this->update_interval_original_ == 0 ? 10000 : this->update_interval_original_;
  ESP_LOGD(TAG, "Reverting update interval to %d seconds", update_interval / 1000);
  this->stop_poller();
  this->set_update_interval(update_interval);
  this->start_poller();
}
void ShellyDimmer::save_calibration_() {
  if (this->rtc_.save(&this->calibration_data_)) {
    ESP_LOGD(TAG, "Saved calibration to flash");
  } else {
    ESP_LOGW(TAG, "Couldn't save calibration to flash");
  }
}
void ShellyDimmer::set_brightness_no_transition_(float brightness) {
  auto call = this->state_->make_call();
  call.set_brightness(brightness);
  call.set_transition_length(0);
  call.set_state(true);
  call.perform();
}
void ShellyDimmer::clear_calibration() {
  this->calibration_data_.fill(0);
  this->save_calibration_();
}

}  // namespace shelly_dimmer
}  // namespace esphome

#endif  // USE_ESP8266
