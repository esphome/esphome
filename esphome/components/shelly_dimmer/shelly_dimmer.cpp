#include "shelly_dimmer.h"
#include "firmware_v515.h"
#include "stm32flash.h"

namespace esphome {
namespace shelly {

static const char *TAG = "shelly";

static const uint16_t SHELLY_DIMMER_BUFFER_SIZE = 256;
static const uint8_t SHELLY_DIMMER_ACK_TIMEOUT = 200; // ms
static const uint8_t SHELLY_DIMMER_MAX_RETRIES = 3;
static const uint16_t SHELLY_DIMMER_MAX_BRIGHTNESS = 1000; // 100%

// Protocol framing.
static const uint8_t SHELLY_DIMMER_PROTO_START_BYTE = 0x01;
static const uint8_t SHELLY_DIMMER_PROTO_END_BYTE = 0x04;

// Supported commands.
static const uint8_t SHELLY_DIMMER_PROTO_CMD_SWITCH = 0x01;
static const uint8_t SHELLY_DIMMER_PROTO_CMD_POLL = 0x10;
static const uint8_t SHELLY_DIMMER_PROTO_CMD_VERSION = 0x11;
static const uint8_t SHELLY_DIMMER_PROTO_CMD_SETTINGS = 0x20;

// Command payload sizes.
static const uint8_t SHELLY_DIMMER_PROTO_CMD_SWITCH_SIZE = 2;
static const uint8_t SHELLY_DIMMER_PROTO_CMD_SETTINGS_SIZE = 10;
static const uint8_t SHELLY_DIMMER_PROTO_MAX_FRAME_SIZE = 4 + 72 + 3;

/// Computes a crappy checksum as defined by the Shelly Dimmer protocol.
uint16_t shelly_dimmer_checksum(uint8_t *buf, int len) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++) {
    sum += buf[i];
  }
  return sum;
}

void ShellyDimmer::setup() {
  this->buffer_ = new uint8_t[SHELLY_DIMMER_BUFFER_SIZE];
  this->pin_nrst_->setup();
  this->pin_boot0_->setup();
  this->serial_ = &Serial;

  // Reset the STM32 and check the firmware version.
  for (int i = 0; i < 2; i++) {
    this->reset_normal_boot_();
    this->send_command_(SHELLY_DIMMER_PROTO_CMD_VERSION, 0, 0);
    ESP_LOGI(TAG, "STM32 firmware version: %d.%d", this->version_major_, this->version_minor_);
    if (this->version_major_ != SHD_FIRMWARE_MAJOR_VERSION ||
        this->version_minor_ != SHD_FIRMWARE_MINOR_VERSION) {
      // Update firmware if needed.
      ESP_LOGW(TAG, "Unsupported STM32 firmware version, flashing");
      if (i > 0) {
        // Upgrade was already performed but the reported version is still not right.
        ESP_LOGE(TAG, "STM32 firmware upgrade already performed, but version is still incorrect");
        this->mark_failed();
        return;
      }

      if (!this->upgrade_firmware_()) {
        ESP_LOGW(TAG, "Failed to upgrade firmware");
        this->mark_failed();
        return;
      }

      // Firmware upgrade completed, do the checks again.
      continue;
    }
    break;
  }

  // Poll the dimmer roughly every 10s.
  this->set_interval("poll", 10000, [this]() {
    this->send_command_(SHELLY_DIMMER_PROTO_CMD_POLL, 0, 0);
  });

  this->send_settings_();
  // Do an immediate poll to refresh current state.
  this->send_command_(SHELLY_DIMMER_PROTO_CMD_POLL, 0, 0);

  this->ready_ = true;
}

void ShellyDimmer::write_state(light::LightState *state) {
  if (!this->ready_) {
    return;
  }

  float brightness;
  state->current_values_as_brightness(&brightness);

  uint16_t brightness_int = this->convert_brightness_(brightness);
  if (brightness_int == this->brightness_) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  ESP_LOGD(TAG, "Brightness update: %d (raw: %f)", brightness_int, brightness);

  this->send_brightness_(brightness_int);
}

bool ShellyDimmer::upgrade_firmware_() {
  ESP_LOGW(TAG, "Starting STM32 firmware upgrade");
  this->reset_dfu_boot_();

  stm32_t *stm32 = stm32_init(this->serial_, STREAM_SERIAL, 1);
  if (!stm32) {
    ESP_LOGW(TAG, "Failed to initialize STM32");
    return false;
  }

  // Erase STM32 flash.
  if (stm32_erase_memory(stm32, 0, STM32_MASS_ERASE) != STM32_ERR_OK) {
    ESP_LOGW(TAG, "Failed to erase STM32 flash memory");
    stm32_close(stm32);
    return false;
  }

  const uint8_t *fw = stm_firmware;
  uint32_t fw_len = sizeof(stm_firmware);

  // Copy the STM32 firmware over in 256-byte chunks. Note that the firmware is stored
  // in flash memory so all accesses need to be 4-byte aligned.
  uint8_t buffer[256];
  const uint8_t *p = fw;
  uint32_t offset = 0;
  uint32_t addr = stm32->dev->fl_start;
  uint32_t end = addr + sizeof(stm_firmware);

  while (addr < end && offset < fw_len) {
    uint32_t left = end - addr;
    uint32_t len = sizeof(buffer) > left ? left : sizeof(buffer);
    len = (len > (fw_len - offset)) ? (fw_len - offset) : len;

    if (len == 0) {
      break;
    }

    memcpy(buffer, p, sizeof(buffer));
    p += sizeof(buffer);

    if (stm32_write_memory(stm32, addr, buffer, len) != STM32_ERR_OK) {
      ESP_LOGW(TAG, "Failed to write to STM32 flash memory");
      stm32_close(stm32);
      return false;
    }

    addr += len;
    offset += len;
  }

  stm32_close(stm32);
  ESP_LOGI(TAG, "STM32 firmware upgrade successful");

  return true;
}

uint16_t ShellyDimmer::convert_brightness_(float brightness) {
  // Special case for zero as only zero means turn off completely.
  if (brightness == 0.0) {
    return 0;
  }

  uint16_t brightness_int = static_cast<uint16_t>(brightness * (this->max_brightness_ - this->min_brightness_));
  brightness_int = brightness_int + this->min_brightness_;
  return std::min(brightness_int, SHELLY_DIMMER_MAX_BRIGHTNESS);
}

void ShellyDimmer::send_brightness_(uint16_t brightness) {
  uint8_t payload[SHELLY_DIMMER_PROTO_CMD_SWITCH_SIZE];
  // Brightness (%) * 10.
  payload[0] = brightness & 0xff,
  payload[1] = brightness >> 8,

  this->send_command_(
    SHELLY_DIMMER_PROTO_CMD_SWITCH,
    payload,
    SHELLY_DIMMER_PROTO_CMD_SWITCH_SIZE
  );

  this->brightness_ = brightness;
}

void ShellyDimmer::send_settings_() {
  uint16_t fade_rate = std::min(uint16_t{100}, this->fade_rate_);

  float brightness = 0.0;
  if (this->state_ != nullptr) {
    this->state_->current_values_as_brightness(&brightness);
  }
  uint16_t brightness_int = this->convert_brightness_(brightness);
  ESP_LOGD(TAG, "Brightness update: %d (raw: %f)", brightness_int, brightness);

  uint8_t payload[SHELLY_DIMMER_PROTO_CMD_SETTINGS_SIZE];
  // Brightness (%) * 10.
  payload[0] = brightness_int & 0xff;
  payload[1] = brightness_int >> 8;
  // Leading / trailing edge [0x01 = leading, 0x02 = trailing].
  payload[2] = this->leading_edge_ ? 0x01 : 0x02;
  payload[3] = 0x00;
  // Fade rate.
  payload[4] = this->fade_rate_ & 0xff;
  payload[5] = this->fade_rate_ >> 8;
  // Warmup brightness.
  payload[6] = this->warmup_brightness_ & 0xff;
  payload[7] = this->warmup_brightness_ >> 8;
  // Warmup time.
  payload[8] = this->warmup_time_ & 0xff;
  payload[9] = this->warmup_time_ >> 8;

  this->send_command_(
    SHELLY_DIMMER_PROTO_CMD_SETTINGS,
    payload,
    SHELLY_DIMMER_PROTO_CMD_SETTINGS_SIZE
  );

  // Also send brightness separately as it is ignored above.
  this->send_brightness_(brightness_int);
}

bool ShellyDimmer::send_command_(uint8_t cmd, uint8_t *payload, uint8_t len) {
  ESP_LOGD(TAG, "Sending command: 0x%02x (%d bytes)", cmd, len);

  // Prepare a command frame.
  uint8_t frame[SHELLY_DIMMER_PROTO_MAX_FRAME_SIZE];
  size_t frame_len = this->frame_command_(frame, cmd, payload, len);

  // Write the frame and wait for acknowledgement.
  int retries = SHELLY_DIMMER_MAX_RETRIES;
  while (retries--) {
    this->serial_->write(frame, frame_len);
    this->serial_->flush();

    ESP_LOGD(TAG, "Command sent, waiting for reply");
    uint32_t tx_time = millis();
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

size_t ShellyDimmer::frame_command_(uint8_t *data, uint8_t cmd, uint8_t *payload, size_t len) {
  size_t pos = 0;

  // Generate a frame.
  data[0] = SHELLY_DIMMER_PROTO_START_BYTE;
  data[1] = ++this->seq_;
  data[2] = cmd;
  data[3] = len;
  pos += 4;

  if (payload != nullptr) {
    memcpy(data + 4, payload, len);
    pos += len;
  }

  // Calculate checksum for the payload.
  uint16_t csum = shelly_dimmer_checksum(data + 1, 3 + len);
  data[pos++] = csum >> 8;
  data[pos++] = csum & 0xff;
  data[pos++] = SHELLY_DIMMER_PROTO_END_BYTE;
  return pos;
}

int ShellyDimmer::handle_byte_(uint8_t c) {
  uint8_t pos = this->buffer_pos_;

  if (pos == 0) {
    // Must be start byte.
    return c == SHELLY_DIMMER_PROTO_START_BYTE ? 1 : -1;
  } else if (pos < 4) {
    // Header.
    return 1;
  }

  // Decode payload length from header.
  uint8_t payload_len = this->buffer_[3];
  if ((4 + payload_len + 3) > SHELLY_DIMMER_BUFFER_SIZE) {
    return -1;
  }

  if (pos < 4 + payload_len + 1) {
    // Payload.
    return 1;
  }

  if (pos == 4 + payload_len + 1) {
    // Verify checksum.
    uint16_t csum = (this->buffer_[pos - 1] << 8 | c);
    uint16_t csum_verify = shelly_dimmer_checksum(&this->buffer_[1], 3 + payload_len);
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
  while (this->serial_->available()) {
    uint8_t c = this->serial_->read();
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
  uint8_t seq = this->buffer_[1];
  uint8_t cmd = this->buffer_[2];
  uint8_t payload_len = this->buffer_[3];

  ESP_LOGD(TAG, "Got frame: 0x%02x", cmd);

  // Compare with expected identifier as the frame is always a response to
  // our previously sent command.
  if (seq != this->seq_) {
    return false;
  }

  uint8_t *payload = &this->buffer_[4];

  // Handle response.
  switch (cmd) {
    case SHELLY_DIMMER_PROTO_CMD_POLL: {
      if (payload_len < 16) {
        return false;
      }

      uint8_t hw_version = payload[0];
      // payload[1] is unused.
      uint16_t brightness = payload[3] << 8 |
        payload[2];

      uint32_t power_raw = payload[7] << 24 |
        payload[6] << 16 |
        payload[5] << 8 |
        payload[4];

      uint32_t voltage_raw = payload[11] << 24 |
        payload[10] << 16 |
        payload[9] << 8 |
        payload[8];

      uint32_t current_raw = payload[15] << 24 |
        payload[14] << 16 |
        payload[13] << 8 |
        payload[12];

      uint16_t fade_rate = payload[16];

      float power = 0;
      if (power_raw > 0) {
        power = 880373 / (float) power_raw;
      }

      float voltage = 0;
      if (voltage_raw > 0) {
        voltage = 347800 / (float) voltage_raw;
      }

      float current = 0;
      if (current_raw > 0) {
        current = 1448 / (float) current_raw;
      }

      ESP_LOGI(TAG, "Got dimmer data:");
      ESP_LOGI(TAG, "  HW version: %d", hw_version);
      ESP_LOGI(TAG, "  Brightness: %d", brightness);
      ESP_LOGI(TAG, "  Fade rate:  %d", fade_rate);
      ESP_LOGI(TAG, "  Power:      %f W", power);
      ESP_LOGI(TAG, "  Voltage:    %f V", voltage);
      ESP_LOGI(TAG, "  Current:    %f A", current);

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
      if (payload_len < 1 || payload[0] != 0x01) {
        return false;
      }
      return true;
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
  delay(50);

  // Clear receive buffer.
  while (this->serial_->available()) {
    this->serial_->read();
  }

  this->pin_nrst_->digital_write(true);
  // Wait 50ms for the STM32 to boot.
  delay(50);

  ESP_LOGD(TAG, "Reset STM32 done");
}

void ShellyDimmer::reset_normal_boot_() {
  this->serial_->end();
  this->serial_->begin(115200, SERIAL_8N1);
  this->serial_->flush();

  this->reset_(false);
}

void ShellyDimmer::reset_dfu_boot_() {
  this->serial_->end();
  this->serial_->begin(115200, SERIAL_8E1);
  this->serial_->flush();

  this->reset_(true);
}

}
}
