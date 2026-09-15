#include "apc1.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>
#include <cstring>

namespace esphome::apc1 {

static const char *const TAG = "apc1";

static constexpr uint8_t APC1_MEASUREMENT_OFFSET_AQI = 58;
static constexpr uint8_t APC1_MEASUREMENT_OFFSET_ERROR_CODE = 61;

static constexpr uint8_t APC1_DEVICE_INFO_OFFSET_MODULE = 4;
static constexpr uint8_t APC1_DEVICE_INFO_MODULE_LEN = 6;
static constexpr uint8_t APC1_DEVICE_INFO_OFFSET_SERIAL = 10;
static constexpr uint8_t APC1_DEVICE_INFO_SERIAL_LEN = 8;
static constexpr uint8_t APC1_DEVICE_INFO_OFFSET_FW = 19;

static constexpr uint8_t APC1_COMMAND_RESPONSE_OFFSET_CMD = 4;
static constexpr uint8_t APC1_COMMAND_RESPONSE_OFFSET_DATA = 5;

void APC1Component::setup() {
  if (this->set_pin_ != nullptr) {
    this->set_pin_->setup();
    this->set_pin_->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  // Attempt initial communication immediately for warm restarts
  this->initialize_device_();

  // Schedule delayed initialization to catch sensors during cold power-on bootup
  this->set_timeout(2000, [this]() { this->initialize_device_(); });
}

void APC1Component::initialize_device_() {
  this->last_init_attempt_ = App.get_loop_component_start_time();
  if (this->active_mode_) {
    this->set_active_mode();
  }
  this->send_command_(APC1Command::APC1_COMMAND_READ_SENSOR_VERSION, 0x0000);
}

void APC1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "APC1:");
  LOG_PIN("  Set Pin: ", this->set_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "PM1.0 Standard", this->pm_1_0_std_sensor_);
  LOG_SENSOR("  ", "PM2.5 Standard", this->pm_2_5_std_sensor_);
  LOG_SENSOR("  ", "PM10.0 Standard", this->pm_10_0_std_sensor_);
  LOG_SENSOR("  ", "PM >0.3um", this->pm_0_3um_sensor_);
  LOG_SENSOR("  ", "PM >0.5um", this->pm_0_5um_sensor_);
  LOG_SENSOR("  ", "PM >1.0um", this->pm_1_0um_sensor_);
  LOG_SENSOR("  ", "PM >2.5um", this->pm_2_5um_sensor_);
  LOG_SENSOR("  ", "PM >5.0um", this->pm_5_0um_sensor_);
  LOG_SENSOR("  ", "PM >10.0um", this->pm_10_0um_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  LOG_SENSOR("  ", "eCO2", this->eco2_sensor_);
  LOG_SENSOR("  ", "AQI", this->aqi_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Raw Temperature", this->raw_temperature_sensor_);
  LOG_SENSOR("  ", "Raw Humidity", this->raw_humidity_sensor_);
  LOG_SENSOR("  ", "RS0", this->rs0_sensor_);
  LOG_SENSOR("  ", "RS2", this->rs2_sensor_);
  LOG_SENSOR("  ", "RS3", this->rs3_sensor_);
  LOG_SENSOR("  ", "Error Code", this->error_code_sensor_);
  this->check_uart_settings(9600);
}

void APC1Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // If in active mode and no valid data has been received yet, retry periodically
  if (this->active_mode_) {
    if (this->last_valid_frame_ == 0) {
      if (now - this->last_init_attempt_ >= 10000) {
        ESP_LOGD(TAG, "Waiting for APC1 sensor to respond, sending init commands...");
        this->initialize_device_();
      }
    } else if (now - this->last_valid_frame_ > 15000 && now - this->last_init_attempt_ >= 10000) {
      ESP_LOGW(TAG, "No data received from APC1 for %" PRIu32 " ms, re-initializing...", now - this->last_valid_frame_);
      this->initialize_device_();
    }
  }

  if (this->rx_index_ > 0 && now - this->last_transmission_ > 200) {
    ESP_LOGV(TAG, "Timed out waiting for data, resetting buffer");
    this->rx_index_ = 0;
  }

  while (this->available()) {
    this->last_transmission_ = now;
    uint8_t byte;
    this->read_byte(&byte);

    if (this->rx_index_ == 0) {
      if (byte == APC1_HEADER_1) {
        this->rx_buffer_[this->rx_index_++] = byte;
      }
      continue;
    }

    if (this->rx_index_ == 1) {
      if (byte == APC1_HEADER_2) {
        this->rx_buffer_[this->rx_index_++] = byte;
      } else if (byte == APC1_HEADER_1) {
        this->rx_index_ = 1;
      } else {
        this->rx_index_ = 0;
      }
      continue;
    }

    if (this->rx_index_ < this->rx_buffer_.size()) {
      this->rx_buffer_[this->rx_index_++] = byte;
    } else {
      this->rx_index_ = 0;
      continue;
    }

    // Once the frame header (2 magic bytes + 2 length bytes) is received, calculate total frame length
    if (this->rx_index_ >= APC1_FRAME_HEADER_SIZE) {
      uint16_t payload_len = encode_uint16(this->rx_buffer_[2], this->rx_buffer_[3]);
      uint16_t total_len = payload_len + APC1_FRAME_HEADER_SIZE;

      if (total_len > this->rx_buffer_.size()) {
        ESP_LOGW(TAG, "Frame length too large: %u", total_len);
        this->rx_index_ = 0;
        continue;
      }

      if (this->rx_index_ == total_len) {
        this->parse_frame_(total_len);
        this->rx_index_ = 0;
      }
    }
  }
}

void APC1Component::parse_frame_(uint16_t total_len) {
  uint16_t expected_checksum = encode_uint16(this->rx_buffer_[total_len - 2], this->rx_buffer_[total_len - 1]);
  uint16_t calculated_checksum = 0;
  for (size_t i = 0; i < total_len - 2; i++) {
    calculated_checksum += this->rx_buffer_[i];
  }
  if (expected_checksum != calculated_checksum) {
    ESP_LOGW(TAG, "APC1 checksum mismatch: calculated 0x%04X != expected 0x%04X", calculated_checksum,
             expected_checksum);
    return;
  }

  if (total_len == APC1_FRAME_SIZE_MEASUREMENT) {
    this->parse_measurement_frame_();
  } else if (total_len == APC1_FRAME_SIZE_DEVICE_INFO) {
    this->parse_device_info_frame_();
  } else if (total_len == APC1_FRAME_SIZE_COMMAND_RESPONSE) {
    this->parse_command_response_frame_();
  } else {
    ESP_LOGD(TAG, "Received frame of unknown length %u", total_len);
  }
}

void APC1Component::parse_measurement_frame_() {
  const uint32_t now = App.get_loop_component_start_time();
  this->last_valid_frame_ = now;

  uint8_t error_code = this->rx_buffer_[APC1_MEASUREMENT_OFFSET_ERROR_CODE];
  if (error_code != this->last_error_code_) {
    if (error_code != 0) {
      if (error_code & 0x01) {
        ESP_LOGW(TAG, "APC1: Fan restart error (too many restarts)");
      }
      if (error_code & 0x02) {
        ESP_LOGW(TAG, "APC1: Fan speed low");
      }
      if (error_code & 0x04) {
        ESP_LOGW(TAG, "APC1: Photodiode fault");
      }
      if (error_code & 0x08) {
        ESP_LOGW(TAG, "APC1: Fan stopped / startup error");
      }
      if (error_code & 0x10) {
        ESP_LOGW(TAG, "APC1: Laser fault");
      }
      if (error_code & 0x20) {
        ESP_LOGW(TAG, "APC1: VOC sensor fault");
      }
      if (error_code & 0x40) {
        ESP_LOGW(TAG, "APC1: RHT sensor fault");
      }
      this->status_set_warning("Sensor reported hardware error");
    } else {
      ESP_LOGI(TAG, "APC1: Hardware error cleared");
      this->status_clear_warning();
    }
    this->last_error_code_ = error_code;
  }

  if (this->error_code_sensor_ != nullptr) {
    this->error_code_sensor_->publish_state(error_code);
  }

  uint8_t aqi = this->rx_buffer_[APC1_MEASUREMENT_OFFSET_AQI];
  if (aqi == 0) {
    if (!this->gas_warming_up_) {
      this->gas_warming_up_ = true;
      ESP_LOGI(TAG, "APC1: Gas sensor warming up (AQI=0)");
    }
  } else if (this->gas_warming_up_) {
    this->gas_warming_up_ = false;
    ESP_LOGI(TAG, "APC1: Gas sensor warm-up complete (AQI=%u)", aqi);
  }

  if (this->update_interval_ > 0 && now - this->last_update_ < this->update_interval_) {
    return;
  }
  this->last_update_ = now;

  // Measurement payload data (bytes 4-57): big-endian sensor readings
  uint16_t pm_1_0_std = encode_uint16(this->rx_buffer_[4], this->rx_buffer_[5]);
  uint16_t pm_2_5_std = encode_uint16(this->rx_buffer_[6], this->rx_buffer_[7]);
  uint16_t pm_10_0_std = encode_uint16(this->rx_buffer_[8], this->rx_buffer_[9]);

  uint16_t pm_1_0 = encode_uint16(this->rx_buffer_[10], this->rx_buffer_[11]);
  uint16_t pm_2_5 = encode_uint16(this->rx_buffer_[12], this->rx_buffer_[13]);
  uint16_t pm_10_0 = encode_uint16(this->rx_buffer_[14], this->rx_buffer_[15]);

  uint16_t pm_0_3um = encode_uint16(this->rx_buffer_[16], this->rx_buffer_[17]);
  uint16_t pm_0_5um = encode_uint16(this->rx_buffer_[18], this->rx_buffer_[19]);
  uint16_t pm_1_0um = encode_uint16(this->rx_buffer_[20], this->rx_buffer_[21]);
  uint16_t pm_2_5um = encode_uint16(this->rx_buffer_[22], this->rx_buffer_[23]);
  uint16_t pm_5_0um = encode_uint16(this->rx_buffer_[24], this->rx_buffer_[25]);
  uint16_t pm_10_0um = encode_uint16(this->rx_buffer_[26], this->rx_buffer_[27]);

  uint16_t tvoc = encode_uint16(this->rx_buffer_[28], this->rx_buffer_[29]);
  uint16_t eco2 = encode_uint16(this->rx_buffer_[30], this->rx_buffer_[31]);

  float temp_comp = static_cast<int16_t>(encode_uint16(this->rx_buffer_[34], this->rx_buffer_[35])) / 10.0f;
  float hum_comp = encode_uint16(this->rx_buffer_[36], this->rx_buffer_[37]) / 10.0f;

  float temp_raw = static_cast<int16_t>(encode_uint16(this->rx_buffer_[38], this->rx_buffer_[39])) / 10.0f;
  float hum_raw = encode_uint16(this->rx_buffer_[40], this->rx_buffer_[41]) / 10.0f;

  uint32_t rs0 = encode_uint32(this->rx_buffer_[42], this->rx_buffer_[43], this->rx_buffer_[44], this->rx_buffer_[45]);
  uint32_t rs2 = encode_uint32(this->rx_buffer_[50], this->rx_buffer_[51], this->rx_buffer_[52], this->rx_buffer_[53]);
  uint32_t rs3 = encode_uint32(this->rx_buffer_[54], this->rx_buffer_[55], this->rx_buffer_[56], this->rx_buffer_[57]);

  ESP_LOGD(TAG, "APC1: PM1.0: %u, PM2.5: %u, PM10: %u, TVOC: %u, eCO2: %u, AQI: %u, Temp: %.1f°C, Hum: %.1f%%", pm_1_0,
           pm_2_5, pm_10_0, tvoc, eco2, aqi, temp_comp, hum_comp);

  if (this->pm_1_0_std_sensor_ != nullptr)
    this->pm_1_0_std_sensor_->publish_state(pm_1_0_std);
  if (this->pm_2_5_std_sensor_ != nullptr)
    this->pm_2_5_std_sensor_->publish_state(pm_2_5_std);
  if (this->pm_10_0_std_sensor_ != nullptr)
    this->pm_10_0_std_sensor_->publish_state(pm_10_0_std);

  if (this->pm_1_0_sensor_ != nullptr)
    this->pm_1_0_sensor_->publish_state(pm_1_0);
  if (this->pm_2_5_sensor_ != nullptr)
    this->pm_2_5_sensor_->publish_state(pm_2_5);
  if (this->pm_10_0_sensor_ != nullptr)
    this->pm_10_0_sensor_->publish_state(pm_10_0);

  if (this->pm_0_3um_sensor_ != nullptr)
    this->pm_0_3um_sensor_->publish_state(pm_0_3um);
  if (this->pm_0_5um_sensor_ != nullptr)
    this->pm_0_5um_sensor_->publish_state(pm_0_5um);
  if (this->pm_1_0um_sensor_ != nullptr)
    this->pm_1_0um_sensor_->publish_state(pm_1_0um);
  if (this->pm_2_5um_sensor_ != nullptr)
    this->pm_2_5um_sensor_->publish_state(pm_2_5um);
  if (this->pm_5_0um_sensor_ != nullptr)
    this->pm_5_0um_sensor_->publish_state(pm_5_0um);
  if (this->pm_10_0um_sensor_ != nullptr)
    this->pm_10_0um_sensor_->publish_state(pm_10_0um);

  if (this->tvoc_sensor_ != nullptr)
    this->tvoc_sensor_->publish_state(tvoc);
  if (this->eco2_sensor_ != nullptr)
    this->eco2_sensor_->publish_state(eco2);
  if (this->aqi_sensor_ != nullptr) {
    if (aqi > 0) {
      this->aqi_sensor_->publish_state(aqi);
    } else {
      this->aqi_sensor_->publish_state(NAN);
    }
  }

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temp_comp);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(hum_comp);

  if (this->raw_temperature_sensor_ != nullptr)
    this->raw_temperature_sensor_->publish_state(temp_raw);
  if (this->raw_humidity_sensor_ != nullptr)
    this->raw_humidity_sensor_->publish_state(hum_raw);

  if (this->rs0_sensor_ != nullptr)
    this->rs0_sensor_->publish_state(rs0);
  if (this->rs2_sensor_ != nullptr)
    this->rs2_sensor_->publish_state(rs2);
  if (this->rs3_sensor_ != nullptr)
    this->rs3_sensor_->publish_state(rs3);
}

void APC1Component::parse_device_info_frame_() {
  std::array<char, APC1_DEVICE_INFO_MODULE_LEN + 1> module_name{};
  std::memcpy(module_name.data(), this->rx_buffer_.data() + APC1_DEVICE_INFO_OFFSET_MODULE,
              APC1_DEVICE_INFO_MODULE_LEN);

  uint64_t serial_number = 0;
  for (size_t i = APC1_DEVICE_INFO_OFFSET_SERIAL; i < APC1_DEVICE_INFO_OFFSET_SERIAL + APC1_DEVICE_INFO_SERIAL_LEN;
       i++) {
    serial_number = (serial_number << 8) | this->rx_buffer_[i];
  }

  uint16_t fw_version =
      encode_uint16(this->rx_buffer_[APC1_DEVICE_INFO_OFFSET_FW], this->rx_buffer_[APC1_DEVICE_INFO_OFFSET_FW + 1]);
  ESP_LOGI(TAG, "APC1: Module: %s, Serial: %08" PRIX32 "%08" PRIX32 ", Firmware: 0x%04X (%u)", module_name.data(),
           static_cast<uint32_t>(serial_number >> 32), static_cast<uint32_t>(serial_number & 0xFFFFFFFF), fw_version,
           fw_version);
}

void APC1Component::parse_command_response_frame_() {
  uint8_t cmd = this->rx_buffer_[APC1_COMMAND_RESPONSE_OFFSET_CMD];
  uint8_t data = this->rx_buffer_[APC1_COMMAND_RESPONSE_OFFSET_DATA];
  ESP_LOGD(TAG, "APC1 command response: cmd=0x%02X, data=0x%02X", cmd, data);
}

void APC1Component::set_active_mode() {
  this->active_mode_ = true;
  this->send_command_(APC1Command::APC1_COMMAND_MEASUREMENT_MODE, APC1_MEASUREMENT_MODE_ACTIVE);
}

void APC1Component::set_passive_mode() {
  this->active_mode_ = false;
  this->send_command_(APC1Command::APC1_COMMAND_MEASUREMENT_MODE, APC1_MEASUREMENT_MODE_PASSIVE);
}

void APC1Component::request_measurement() {
  this->send_command_(APC1Command::APC1_COMMAND_REQUEST_MEASUREMENT, 0x0000);
}

void APC1Component::set_idle_mode() {
  this->send_command_(APC1Command::APC1_COMMAND_OPERATION_MODE, APC1_OPERATING_MODE_IDLE);
}

void APC1Component::set_measurement_mode() {
  this->send_command_(APC1Command::APC1_COMMAND_OPERATION_MODE, APC1_OPERATING_MODE_STANDARD);
}

void APC1Component::send_command_(APC1Command cmd, uint16_t data) {
  std::array<uint8_t, APC1_COMMAND_FRAME_SIZE> buf{
      APC1_HEADER_1,
      APC1_HEADER_2,
      static_cast<uint8_t>(cmd),
      static_cast<uint8_t>((data >> 8) & 0xFF),
      static_cast<uint8_t>(data & 0xFF),
      0,
      0,
  };
  uint16_t checksum = 0;
  for (size_t i = 0; i < 5; i++) {
    checksum += buf[i];
  }
  buf[5] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
  buf[6] = static_cast<uint8_t>(checksum & 0xFF);
  this->write_array(buf);
}

}  // namespace esphome::apc1
