#include "apc1.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome::apc1 {

ESPHOME_LOG_TAG(TAG, "apc1");

static constexpr uint8_t APC1_MEASUREMENT_OFFSET_AQI = 58;
static constexpr uint8_t APC1_MEASUREMENT_OFFSET_ERROR_CODE = 61;

static constexpr uint8_t APC1_DEVICE_INFO_OFFSET_MODULE = 4;
static constexpr uint8_t APC1_DEVICE_INFO_OFFSET_SERIAL = 10;
static constexpr uint8_t APC1_DEVICE_INFO_OFFSET_FW = 19;

// Give a freshly powered sensor time to boot before the first retry
static constexpr uint32_t APC1_COLD_BOOT_INIT_DELAY_MS = 2000;
// Re-send the init commands at most this often while no frames arrive
static constexpr uint32_t APC1_INIT_RETRY_INTERVAL_MS = 10000;
// Active mode streams a frame every second; this long without one means the sensor stopped
static constexpr uint32_t APC1_NO_DATA_TIMEOUT_MS = 15000;
// Drop a partial frame when the bytes stop mid-way
static constexpr uint32_t APC1_RX_IDLE_TIMEOUT_MS = 200;

// Frames and commands end with the 16-bit sum of every byte before it
static uint16_t apc1_checksum(const uint8_t *data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

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
  this->set_timeout(APC1_COLD_BOOT_INIT_DELAY_MS, [this]() { this->initialize_device_(); });
}

void APC1Component::initialize_device_() {
  this->last_init_attempt_ = App.get_loop_component_start_time();
  if (this->active_mode_ && !this->idle_) {
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
}

void APC1Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // If in active mode and not idle, retry periodically if no valid data received
  if (this->active_mode_ && !this->idle_) {
    if (this->last_valid_frame_ == 0) {
      if (now - this->last_init_attempt_ >= APC1_INIT_RETRY_INTERVAL_MS) {
        ESP_LOGD(TAG, "Waiting for APC1 sensor to respond, sending init commands...");
        this->initialize_device_();
      }
    } else if (now - this->last_valid_frame_ > APC1_NO_DATA_TIMEOUT_MS &&
               now - this->last_init_attempt_ >= APC1_INIT_RETRY_INTERVAL_MS) {
      ESP_LOGW(TAG, "No data received from APC1 for %" PRIu32 " ms, re-initializing...", now - this->last_valid_frame_);
      this->initialize_device_();
    }
  }

  if (this->rx_index_ > 0 && now - this->last_transmission_ > APC1_RX_IDLE_TIMEOUT_MS) {
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
      } else if (byte != APC1_HEADER_1) {
        this->rx_index_ = 0;
      }
      continue;
    }

    this->rx_buffer_[this->rx_index_++] = byte;
    if (this->rx_index_ < APC1_FRAME_HEADER_SIZE) {
      continue;
    }

    // The payload carries at least the checksum and must fit the buffer; checking it before the sum keeps a
    // corrupt length from wrapping the total and walking past the buffer
    const uint16_t payload_len = encode_uint16(this->rx_buffer_[2], this->rx_buffer_[3]);
    if (payload_len < 2 || payload_len > this->rx_buffer_.size() - APC1_FRAME_HEADER_SIZE) {
      ESP_LOGW(TAG, "Invalid frame length: %u", payload_len);
      this->rx_index_ = 0;
      continue;
    }
    const uint8_t total_len = payload_len + APC1_FRAME_HEADER_SIZE;
    if (this->rx_index_ == total_len) {
      this->parse_frame_(total_len);
      this->rx_index_ = 0;
    }
  }
}

void APC1Component::parse_frame_(uint16_t total_len) {
  uint16_t expected_checksum = encode_uint16(this->rx_buffer_[total_len - 2], this->rx_buffer_[total_len - 1]);
  uint16_t calculated_checksum = apc1_checksum(this->rx_buffer_.data(), total_len - 2);
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
    ESP_LOGD(TAG, "APC1 command response: cmd=0x%02X, data=0x%02X", this->rx_buffer_[4], this->rx_buffer_[5]);
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
      // One message per error bit, bit 0 first
      static const LogString *const ERROR_BITS[] = {
          LOG_STR("Fan restart error (too many restarts)"),
          LOG_STR("Fan speed low"),
          LOG_STR("Photodiode fault"),
          LOG_STR("Fan stopped / startup error"),
          LOG_STR("Laser fault"),
          LOG_STR("VOC sensor fault"),
          LOG_STR("RHT sensor fault"),
      };
      for (uint8_t bit = 0; bit < sizeof(ERROR_BITS) / sizeof(ERROR_BITS[0]); bit++) {
        if (error_code & (1 << bit)) {
          ESP_LOGW(TAG, "APC1: %s", LOG_STR_ARG(ERROR_BITS[bit]));
        }
      }
      this->status_set_warning(LOG_STR("Sensor reported hardware error"));
    } else {
      ESP_LOGI(TAG, "APC1: Hardware error cleared");
      this->status_clear_warning();
    }
    this->last_error_code_ = error_code;
    if (this->error_code_sensor_ != nullptr) {
      this->error_code_sensor_->publish_state(error_code);
    }
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

  // Measurement payload data (bytes 4-57): big-endian sensor readings.
  // The values below feed the debug summary, so they are always decoded; every
  // other reading is only decoded when its sensor is configured.
  const auto u16 = [this](uint8_t offset) {
    return encode_uint16(this->rx_buffer_[offset], this->rx_buffer_[offset + 1]);
  };
  const auto u32 = [this](uint8_t offset) {
    return encode_uint32(this->rx_buffer_[offset], this->rx_buffer_[offset + 1], this->rx_buffer_[offset + 2],
                         this->rx_buffer_[offset + 3]);
  };
  uint16_t pm_1_0 = u16(10);
  uint16_t pm_2_5 = u16(12);
  uint16_t pm_10_0 = u16(14);
  uint16_t tvoc = u16(28);
  uint16_t eco2 = u16(30);
  float temp_comp = static_cast<int16_t>(u16(34)) / 10.0f;
  float hum_comp = u16(36) / 10.0f;

  ESP_LOGD(TAG, "APC1: PM1.0: %u, PM2.5: %u, PM10: %u, TVOC: %u, eCO2: %u, AQI: %u, Temp: %.1f°C, Hum: %.1f%%", pm_1_0,
           pm_2_5, pm_10_0, tvoc, eco2, aqi, temp_comp, hum_comp);

  if (this->pm_1_0_std_sensor_ != nullptr)
    this->pm_1_0_std_sensor_->publish_state(u16(4));
  if (this->pm_2_5_std_sensor_ != nullptr)
    this->pm_2_5_std_sensor_->publish_state(u16(6));
  if (this->pm_10_0_std_sensor_ != nullptr)
    this->pm_10_0_std_sensor_->publish_state(u16(8));

  if (this->pm_1_0_sensor_ != nullptr)
    this->pm_1_0_sensor_->publish_state(pm_1_0);
  if (this->pm_2_5_sensor_ != nullptr)
    this->pm_2_5_sensor_->publish_state(pm_2_5);
  if (this->pm_10_0_sensor_ != nullptr)
    this->pm_10_0_sensor_->publish_state(pm_10_0);

  if (this->pm_0_3um_sensor_ != nullptr)
    this->pm_0_3um_sensor_->publish_state(u16(16));
  if (this->pm_0_5um_sensor_ != nullptr)
    this->pm_0_5um_sensor_->publish_state(u16(18));
  if (this->pm_1_0um_sensor_ != nullptr)
    this->pm_1_0um_sensor_->publish_state(u16(20));
  if (this->pm_2_5um_sensor_ != nullptr)
    this->pm_2_5um_sensor_->publish_state(u16(22));
  if (this->pm_5_0um_sensor_ != nullptr)
    this->pm_5_0um_sensor_->publish_state(u16(24));
  if (this->pm_10_0um_sensor_ != nullptr)
    this->pm_10_0um_sensor_->publish_state(u16(26));

  if (this->tvoc_sensor_ != nullptr)
    this->tvoc_sensor_->publish_state(tvoc);
  if (this->eco2_sensor_ != nullptr)
    this->eco2_sensor_->publish_state(eco2);
  if (this->aqi_sensor_ != nullptr)
    this->aqi_sensor_->publish_state(aqi > 0 ? aqi : NAN);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temp_comp);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(hum_comp);

  if (this->raw_temperature_sensor_ != nullptr) {
    this->raw_temperature_sensor_->publish_state(static_cast<int16_t>(u16(38)) / 10.0f);
  }
  if (this->raw_humidity_sensor_ != nullptr)
    this->raw_humidity_sensor_->publish_state(u16(40) / 10.0f);

  if (this->rs0_sensor_ != nullptr) {
    this->rs0_sensor_->publish_state(u32(42));
  }
  if (this->rs2_sensor_ != nullptr) {
    this->rs2_sensor_->publish_state(u32(50));
  }
  if (this->rs3_sensor_ != nullptr) {
    this->rs3_sensor_->publish_state(u32(54));
  }
}

void APC1Component::parse_device_info_frame_() {
  const uint8_t *serial = this->rx_buffer_.data() + APC1_DEVICE_INFO_OFFSET_SERIAL;
  uint16_t fw_version =
      encode_uint16(this->rx_buffer_[APC1_DEVICE_INFO_OFFSET_FW], this->rx_buffer_[APC1_DEVICE_INFO_OFFSET_FW + 1]);
  ESP_LOGI(TAG, "APC1: Module: %.6s, Serial: %08" PRIX32 "%08" PRIX32 ", Firmware: 0x%04X (%u)",
           reinterpret_cast<const char *>(this->rx_buffer_.data() + APC1_DEVICE_INFO_OFFSET_MODULE),
           encode_uint32(serial[0], serial[1], serial[2], serial[3]),
           encode_uint32(serial[4], serial[5], serial[6], serial[7]), fw_version, fw_version);
}

void APC1Component::arm_watchdog_() {
  this->last_valid_frame_ = App.get_loop_component_start_time();
  this->last_init_attempt_ = this->last_valid_frame_;
}

void APC1Component::set_active_mode() {
  if (!this->active_mode_) {
    this->arm_watchdog_();
  }
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
  this->idle_ = true;
  this->send_command_(APC1Command::APC1_COMMAND_OPERATION_MODE, APC1_OPERATING_MODE_IDLE);
}

void APC1Component::set_measurement_mode() {
  this->idle_ = false;
  this->arm_watchdog_();
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
  uint16_t checksum = apc1_checksum(buf.data(), 5);
  buf[5] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
  buf[6] = static_cast<uint8_t>(checksum & 0xFF);
  this->write_array(buf);
}

}  // namespace esphome::apc1
