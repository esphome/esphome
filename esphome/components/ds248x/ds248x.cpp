#include "ds248x.h"
#include "one_wire_bus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ds248x {

static const char *const TAG = "ds248x";

void DS248xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS248x...");

  if (this->sleep_pin_) {
    this->sleep_pin_->setup();
    this->sleep_pin_->pin_mode(esphome::gpio::FLAG_OUTPUT);
    this->sleep_pin_->digital_write(true);  // Wake up
    delay(10);
  }

  // Give the device some time to wake up
  delay(10);

  // PROBE LOOP
  ESP_LOGI(TAG, "Probing DS248x...");
  uint8_t status;
  bool found = false;
  for (int i = 0; i < 5; i++) {
    if (this->read(&status, 1) == i2c::ERROR_OK) {
      ESP_LOGI(TAG, "Device responded! Status: 0x%02x", status);
      found = true;
      break;
    }
    delay(10);
  }

  if (!found) {
    ESP_LOGW(TAG, "Device did not respond to Read Status. Trying Reset anyway...");
  } else {
    delay(100);  // NOLINT
  }

  // 1. Device Reset
  // We try a few times because sometimes the bus needs to settle
  bool reset_success = false;
  for (int i = 0; i < 3; i++) {
    if (this->device_reset_()) {
      reset_success = true;
      break;
    }
    delay(10);
  }

  if (!reset_success) {
    ESP_LOGW(TAG, "DS248x Reset failed during setup! Status was 0x%02x. Trying to configure anyway...", status);
  }

  // 2. Device Configure
  if (!this->device_configure_()) {
    ESP_LOGE(TAG, "DS248x Configure failed during setup!");
    this->mark_failed();
    return;
  }

  // 3. Search all channels
  for (uint8_t i = 0; i < this->channel_count_; i++) {
    if (this->select_channel(i)) {
      if (this->channel_count_ > 1) {
        ESP_LOGI(TAG, "Scanning Channel %d...", i);
      }
      this->search();
      if (this->alarm_search_on_boot_) {
        this->alarm_search();
      }
    }
  }

  // Reset to Channel 0
  this->select_channel(0);

  ESP_LOGI(TAG, "DS248x Initialized successfully.");
}

void DS248xComponent::on_shutdown() {
  if (this->sleep_pin_ && (this->hub_sleep_ || this->bus_sleep_)) {
    this->sleep_pin_->digital_write(false);  // Sleep
  }
}

void DS248xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DS248x:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Channel Count: %d", this->channel_count_);
  for (auto *sensor : this->sensors_) {
    LOG_SENSOR("  ", "Device", sensor);
    ESP_LOGCONFIG(TAG, "    Address: %s", sensor->get_address_name().c_str());
    ESP_LOGCONFIG(TAG, "    Channel: %d", sensor->get_channel());
  }
}

void DS248xComponent::register_sensor(DS248xSensor *sensor) { this->sensors_.push_back(sensor); }

void DS248xComponent::register_bus(DS248xOneWireBus *bus) { this->buses_.push_back(bus); }

void DS248xComponent::update() {
  if (this->is_updating_) {
    ESP_LOGW(TAG, "Overlapping updates detected. Skipping update.");
    return;
  }
  if (this->sleep_pin_ && this->bus_sleep_) {
    // Wake bus before starting an update cycle
    this->sleep_pin_->digital_write(true);
    delay(1);
  }
  this->is_updating_ = true;
  this->process_next_channel_(0);
}

void DS248xComponent::process_next_channel_(uint8_t channel_idx) {
  if (channel_idx >= this->channel_count_) {
    this->is_updating_ = false;
    return;  // Done
  }

  // Check if we have sensors on this channel
  bool has_sensors = false;
  bool needs_strong_pullup = this->strong_pullup_enabled_;  // Global setting
  uint8_t max_resolution = 9;

  for (auto *s : this->sensors_) {
    if (s->get_channel() == channel_idx) {
      has_sensors = true;
      if (s->get_parasitic_mode()) {
        needs_strong_pullup = true;
      }
      if (s->get_resolution() > max_resolution) {
        max_resolution = s->get_resolution();
      }
    }
  }

  if (!has_sensors) {
    this->finish_update_(channel_idx + 1);
    return;
  }

  // Start conversion on this channel
  if (!this->select_channel(channel_idx)) {
    ESP_LOGW(TAG, "Failed to select channel %d for update", channel_idx);
    if (!this->recover_device_(channel_idx, "select_channel")) {
      this->finish_update_(channel_idx + 1);
    } else {
      this->process_next_channel_(channel_idx);
    }
    return;
  }

  bool presence;
  if (!this->ow_reset(presence) || !presence) {
    ESP_LOGW(TAG, "No presence on channel %d", channel_idx);
    this->finish_update_(channel_idx + 1);
    return;
  }

  // Skip ROM (CC) -> Convert T (44)
  if (!this->skip_rom()) {
    ESP_LOGW(TAG, "Failed to skip ROM on channel %d", channel_idx);
    this->finish_update_(channel_idx + 1);
    return;
  }

  // Enable Strong Pullup for Parasitic Power
  // This must be done BEFORE the command that requires the pullup (Convert T)
  // The DS2482 will activate the pullup immediately after the next byte is written.
  if (needs_strong_pullup) {
    ESP_LOGV(TAG, "Enabling Strong Pullup for channel %d", channel_idx);
    if (!this->set_strong_pullup_mode_(true)) {
      ESP_LOGW(TAG, "Failed to enable Strong Pullup");
    }
    // Give it a moment to settle
    delay(10);
  }

  if (!this->ow_write_byte(0x44, needs_strong_pullup)) {
    if (needs_strong_pullup) {
      this->set_strong_pullup_mode_(false);
    }
    ESP_LOGW(TAG, "Failed to write Convert T command on channel %d", channel_idx);
    this->finish_update_(channel_idx + 1);
    return;
  }

  // Decide between polling and fixed delay.
  if (this->conversion_mode_ == CONVERSION_POLL && !needs_strong_pullup) {
    // Non-parasitic can be polled safely.
    this->set_timeout(100, [this, channel_idx]() { this->check_conversion_status_(channel_idx, millis()); });
  } else {
    uint32_t delay_ms = this->compute_conversion_delay_ms_(max_resolution, needs_strong_pullup);
    this->set_timeout(delay_ms, [this, channel_idx]() { this->process_channel_readout_(channel_idx); });
  }
}

void DS248xComponent::finish_update_(uint8_t next_channel_idx) {
  if (next_channel_idx >= this->channel_count_) {
    this->is_updating_ = false;
    if (this->sleep_pin_ && this->bus_sleep_) {
      // Allow bus to sleep between update cycles
      this->sleep_pin_->digital_write(false);
    }
    return;
  }
  this->process_next_channel_(next_channel_idx);
}

uint32_t DS248xComponent::compute_conversion_delay_ms_(uint8_t max_resolution, bool needs_spu) const {
  uint32_t delay_ms = 750;
  switch (max_resolution) {
    case 9:
      delay_ms = 94;
      break;
    case 10:
      delay_ms = 188;
      break;
    case 11:
      delay_ms = 375;
      break;
    case 12:
    default:
      delay_ms = 750;
      break;
  }

  if (needs_spu) {
    // Parasitic power needs more time/margin
    delay_ms = static_cast<uint32_t>(delay_ms * 1.6f);
    if (delay_ms < 150)
      delay_ms = 150;
  } else {
    // Small margin for standard power
    delay_ms += 5;
  }
  return delay_ms;
}

bool DS248xComponent::recover_device_(uint8_t channel_idx, const char *reason) {
  if (this->recovering_) {
    ESP_LOGW(TAG, "Recovery already in progress, skip (%s)", reason);
    return false;
  }
  this->recovering_ = true;
  ESP_LOGW(TAG, "Attempting DS248x recovery after %s", reason);
  if (!this->device_reset_()) {
    this->recovering_ = false;
    return false;
  }
  if (!this->device_configure_()) {
    this->recovering_ = false;
    return false;
  }
  if (channel_idx < this->channel_count_) {
    if (!this->select_channel(channel_idx)) {
      this->recovering_ = false;
      return false;
    }
  }
  this->recovering_ = false;
  return true;
}

void DS248xComponent::check_conversion_status_(uint8_t channel_idx, uint32_t start_time) {
  bool bit;
  if (!this->ow_read_bit(bit)) {
    ESP_LOGW(TAG, "Failed to read status bit on channel %d", channel_idx);
    if (!this->recover_device_(channel_idx, "read_bit")) {
      this->finish_update_(channel_idx + 1);
      return;
    }
    this->process_channel_readout_(channel_idx);
    return;
  }

  if (bit) {
    this->process_channel_readout_(channel_idx);
  } else {
    if (millis() - start_time > 1500) {
      ESP_LOGW(TAG, "Conversion timeout on channel %d in poll mode", channel_idx);
      this->process_channel_readout_(channel_idx);
    } else {
      this->set_timeout(20, [this, channel_idx, start_time]() { this->check_conversion_status_(channel_idx, start_time); });
    }
  }
}

void DS248xComponent::process_channel_readout_(uint8_t channel_idx) {
  // Check for Short Detected bit
  uint8_t status;
  if (this->read(&status, 1) == i2c::ERROR_OK) {
      if (status & DS248X_STATUS_SD) {
          ESP_LOGW(TAG, "Short Detected on channel %d! Status: 0x%02x", channel_idx, status);
      }
      if (status & DS248X_STATUS_RST) {
          ESP_LOGW(TAG, "Device Reset Detected on channel %d! Status: 0x%02x", channel_idx, status);
      }
  }

    // Ensure Strong Pullup is disabled after the conversion time if it is active
    if (this->strong_pullup_active_) {
      ESP_LOGV(TAG, "Disabling Strong Pullup for channel %d", channel_idx);
      this->set_strong_pullup_mode_(false);
    }

  // Re-select channel to be safe
  if (!this->select_channel(channel_idx)) {
    if (this->recover_device_(channel_idx, "reselect_channel")) {
      // Retry channel selection once after recovery
      if (!this->select_channel(channel_idx)) {
        ESP_LOGW(TAG, "Channel reselect failed after recovery on channel %d", channel_idx);
        this->finish_update_(channel_idx + 1);
        return;
      }
    } else {
      this->finish_update_(channel_idx + 1);
      return;
    }
  }

  this->process_sensor_readout_(channel_idx, 0);
}

void DS248xComponent::process_sensor_readout_(uint8_t channel_idx, uint8_t sensor_idx) {
  DS248xSensor *sensor = nullptr;
  uint8_t next_idx = sensor_idx + 1;

  // Find the next sensor on this channel
  for (size_t i = sensor_idx; i < this->sensors_.size(); i++) {
    if (this->sensors_[i]->get_channel() == channel_idx) {
      sensor = this->sensors_[i];
      next_idx = i + 1;
      break;
    }
  }

  if (!sensor) {
    // No more sensors on this channel
    this->finish_update_(channel_idx + 1);
    return;
  }

  bool presence;
  if (!this->ow_reset(presence) || !presence) {
    ESP_LOGW(TAG, "Sensor %s lost presence", sensor->get_name().c_str());
    sensor->publish_state(NAN);
    this->defer([this, channel_idx, next_idx]() { this->process_sensor_readout_(channel_idx, next_idx); });
    return;
  }

  if (!this->match_rom(sensor->get_address())) {
    ESP_LOGW(TAG, "Sensor %s match rom failed", sensor->get_name().c_str());
    this->defer([this, channel_idx, next_idx]() { this->process_sensor_readout_(channel_idx, next_idx); });
    return;
  }

  // Read Scratchpad (BE)
  if (!this->ow_write_byte(0xBE)) {
    this->defer([this, channel_idx, next_idx]() { this->process_sensor_readout_(channel_idx, next_idx); });
    return;
  }

  uint8_t scratchpad[9];
  for (uint8_t &val : scratchpad) {
    if (!this->ow_read_byte(val)) {
      this->defer([this, channel_idx, next_idx]() { this->process_sensor_readout_(channel_idx, next_idx); });
      return;
    }
  }

  // Check CRC
  if (crc8(scratchpad, 8) != scratchpad[8]) {
    ESP_LOGW(TAG, "Sensor %s CRC failed", sensor->get_name().c_str());
    sensor->publish_state(NAN);
    this->defer([this, channel_idx, next_idx]() { this->process_sensor_readout_(channel_idx, next_idx); });
    return;
  }

  uint8_t family_code = sensor->get_address() & 0xFF;

  if (family_code == 0x28 || family_code == 0x22 || family_code == 0x3B || family_code == 0x42) {
    // DS18B20 (0x28), DS1822 (0x22), DS1825 (0x3B), DS28EA00 (0x42)
    int16_t temp_raw = (scratchpad[1] << 8) | scratchpad[0];
    float temp_c = temp_raw / 16.0f;
    sensor->publish_state(temp_c);

    // Check and set resolution if needed
    uint8_t current_res_byte = scratchpad[4];
    uint8_t desired_res_byte = 0x1F | ((sensor->get_resolution() - 9) << 5);

    if (current_res_byte != desired_res_byte && !sensor->has_resolution_update_attempted()) {
      ESP_LOGD(TAG, "Updating resolution for %s to %d bit", sensor->get_name().c_str(), sensor->get_resolution());
      sensor->set_resolution_update_attempted(true);

      bool presence;
      // We must reset before starting a new command sequence (Write Scratchpad)
      if (this->ow_reset(presence) && presence) {
        if (this->match_rom(sensor->get_address())) {
          this->ow_write_byte(0x4E);              // Write Scratchpad
          this->ow_write_byte(scratchpad[2]);     // Th
          this->ow_write_byte(scratchpad[3]);     // Tl
          this->ow_write_byte(desired_res_byte);  // Config

          // Copy to EEPROM to make it permanent
          // We must reset again before the next command sequence (Copy Scratchpad)
          if (this->ow_reset(presence) && presence) {
            if (this->match_rom(sensor->get_address())) {
              // Enable SPU if parasitic
              bool use_spu = sensor->get_parasitic_mode() || this->strong_pullup_enabled_;
              if (use_spu) {
                this->set_strong_pullup_mode_(true);
              }

              this->ow_write_byte(0x48);  // Copy Scratchpad

              // ASYNC WAIT: Schedule callback instead of delay(10)
              this->set_timeout(10, [this, channel_idx, next_idx, use_spu]() {
                if (use_spu) {
                  this->set_strong_pullup_mode_(false);            // Turn off SPU
                  this->set_read_pointer_(DS248X_POINTER_STATUS);  // Restore pointer
                }
                // Continue with next sensor
                this->process_sensor_readout_(channel_idx, next_idx);
              });
              return;  // EXIT HERE, callback will continue
            }
          }
        }
      } else {
        ESP_LOGW(TAG, "Reset failed during resolution update for %s", sensor->get_name().c_str());
      }
    }

  } else if (family_code == 0x10) {
    // DS18S20 (0x10) - Old model, 9-bit resolution but high precision mode available
    // Standard 9-bit reading:
    // float temp_c = ((scratchpad[0] >> 1) | (scratchpad[1] << 7)) - 0.25 + ((float)(scratchpad[7] - scratchpad[6]) /
    // scratchpad[7]);

    // Simplified high precision calculation:
    int16_t temp_raw = (scratchpad[1] << 8) | scratchpad[0];
    if (scratchpad[7] != 0) {
      float t = (temp_raw & 0xFFFE) / 2.0f;
      t += 0.75f + ((float) scratchpad[7] - (float) scratchpad[6]) / (float) scratchpad[7];
      sensor->publish_state(t);
    } else {
      sensor->publish_state(temp_raw / 2.0f);
    }

  } else {
    // TODO: Add support for other devices
    // 0x3A: DS2413 (Dual Channel Addressable Switch)
    // 0x26: DS2438 (Smart Battery Monitor)
    // 0x29: DS2408 (8-Channel Addressable Switch)
    ESP_LOGW(TAG, "Unsupported device family 0x%02x for sensor %s", family_code, sensor->get_name().c_str());
    sensor->publish_state(NAN);
  }

  // Continue with next sensor immediately
  this->defer([this, channel_idx, next_idx]() { this->process_sensor_readout_(channel_idx, next_idx); });
}

// --- Core Implementation ---

bool DS248xComponent::set_read_pointer_(uint8_t ptr) { return this->write_byte(DS248X_COMMAND_SETREADPTR, ptr); }

bool DS248xComponent::wait_busy_() {
  // We assume the Read Pointer is already at Status Register (F0h).
  // We ensure this by setting the pointer BEFORE issuing any command that causes busyness.

  uint32_t start = millis();
  while (millis() - start < this->busy_timeout_ms_) {
    uint8_t status;
    if (this->read(&status, 1) != i2c::ERROR_OK) {
      // Read failed. Retry.
      delayMicroseconds(100);
      continue;
    }
    if (!(status & DS248X_STATUS_BUSY))
      return true;
    delayMicroseconds(100);
  }
  ESP_LOGW(TAG, "DS248x Busy Timeout");
  // Try to recover?
  this->device_reset_();
  this->current_channel_ = -1;  // Invalidate state
  return false;
}

bool DS248xComponent::device_reset_() {
  ESP_LOGD(TAG, "Resetting device...");
  // Device Reset (F0) automatically sets the Read Pointer to Status Register.
  uint8_t cmd = DS248X_COMMAND_RESET;

  // We use a simple 1-byte write.
  // If this fails, it might be because the device is already resetting or busy,
  // but we should try to verify the status anyway.
  this->write(&cmd, 1);
  delay(1);

  // Verify reset success (Status bit RST=1)
  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK)
    return false;

  if (!(status & DS248X_STATUS_RST)) {
    ESP_LOGW(TAG, "Device Reset Failed (RST bit not set)");
    return false;
  }

  this->current_channel_ = -1;
  this->last_config_byte_ = 0xFF;
  this->strong_pullup_active_ = false;
  return true;
}

bool DS248xComponent::device_configure_() {
  ESP_LOGD(TAG, "Configuring device...");

  if (!this->set_strong_pullup_mode_(false)) {
    ESP_LOGW(TAG, "Config write/verify failed");
    return false;
  }

  ESP_LOGD(TAG, "Configured successfully");

  // DS2484 Configuration
  if (this->ds2484_mode_) {
    // TODO: Auto-detect DS2483/DS2484 variants and apply timing defaults per datasheet.
    if (this->ds2484_trstl_ > 0)
      this->configure_ds2484_port_(0x0, this->ds2484_trstl_);
    if (this->ds2484_tmsp_ > 0)
      this->configure_ds2484_port_(0x1, this->ds2484_tmsp_);
    if (this->ds2484_tw0l_ > 0)
      this->configure_ds2484_port_(0x2, this->ds2484_tw0l_);
    if (this->ds2484_trec0_ > 0)
      this->configure_ds2484_port_(0x3, this->ds2484_trec0_);
    if (this->ds2484_rwpu_ > 0)
      this->configure_ds2484_port_(0x4, this->ds2484_rwpu_);
  }

  return true;
}

bool DS248xComponent::configure_ds2484_port_(uint8_t param, uint8_t val) {
  // DS2484 Adjust 1-Wire Port (C3h)
  // Data Byte: [7:4] Param Select, [3:0] Param Value
  uint8_t cmd = 0xC3;
  uint8_t data = (param << 4) | (val & 0x0F);

  if (!this->write_byte(cmd, data)) {
    ESP_LOGW(TAG, "DS2484 Port Config Failed (Param %d)", param);
    return false;
  }

  // Verify? The datasheet says the response is the value read back.
  // But write_byte doesn't read back.
  // We can read back the port config register if we want, but let's trust it for now or check busy.
  // DS2484 doesn't go busy after port adjust? Datasheet says "No 1-Wire activity".
  // But we should probably reset pointer to Status just in case.
  return this->set_read_pointer_(DS248X_POINTER_STATUS);
}

bool DS248xComponent::set_strong_pullup_mode_(bool enable) {
  uint8_t config = 0;
  if (this->active_pullup_ || enable)
    config |= DS248X_CONFIG_ACTIVE_PULLUP;
  if (this->overdrive_speed_)
    config |= DS248X_CONFIG_OVERDRIVE;
  if (enable)
    config |= DS248X_CONFIG_STRONG_PULLUP;

  uint8_t config_byte = (config & 0x0F) | ((~config & 0x0F) << 4);
  if (config_byte == this->last_config_byte_) {
    this->strong_pullup_active_ = enable;
    return this->set_read_pointer_(DS248X_POINTER_STATUS);
  }

  if (!this->write_byte(DS248X_COMMAND_WRITECONFIG, config_byte)) {
    ESP_LOGW(TAG, "Failed to write config byte");
    return false;
  }

  delay(1);
  if (!this->set_read_pointer_(DS248X_POINTER_CONFIG)) {
    return false;
  }

  uint8_t read_config;
  if (this->read(&read_config, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read back config byte");
    return false;
  }

  if ((read_config & 0x0F) != (config_byte & 0x0F)) {
    ESP_LOGW(TAG, "Config mismatch! Wrote 0x%02x, Read 0x%02x", config_byte, read_config);
    return false;
  }

  this->last_config_byte_ = config_byte;
  this->strong_pullup_active_ = enable;
  return this->set_read_pointer_(DS248X_POINTER_STATUS);
}

bool DS248xComponent::select_channel(uint8_t channel) {
  if (this->channel_count_ <= 1)
    return true;
  if (channel >= this->channel_count_)
    return false;

  if (this->current_channel_ == channel)
    return true;

  uint8_t ch_code = 0;
  switch (channel) {
    case 0:
      ch_code = 0xF0;
      break;
    case 1:
      ch_code = 0xE1;
      break;
    case 2:
      ch_code = 0xD2;
      break;
    case 3:
      ch_code = 0xC3;
      break;
    case 4:
      ch_code = 0xB4;
      break;
    case 5:
      ch_code = 0xA5;
      break;
    case 6:
      ch_code = 0x96;
      break;
    case 7:
      ch_code = 0x87;
      break;
    default:
      return false;
  }

  if (!this->write_byte(DS248X_COMMAND_CHANNELSELECT, ch_code)) {
    this->current_channel_ = -1;
    return false;
  }

  // Verify Channel (Pointer is at Channel Select Register now)
  uint8_t read_code;
  if (this->read(&read_code, 1) != i2c::ERROR_OK) {
    this->current_channel_ = -1;
    return false;
  }

  uint8_t expected_read = 0;
  switch (channel) {
    case 0:
      expected_read = 0xB8;
      break;
    case 1:
      expected_read = 0xB1;
      break;
    case 2:
      expected_read = 0xAA;
      break;
    case 3:
      expected_read = 0xA3;
      break;
    case 4:
      expected_read = 0x9C;
      break;
    case 5:
      expected_read = 0x95;
      break;
    case 6:
      expected_read = 0x8E;
      break;
    case 7:
      expected_read = 0x87;
      break;
  }

  if (read_code != expected_read) {
    ESP_LOGW(TAG, "Channel Select Failed! Expected 0x%02x, got 0x%02x", expected_read, read_code);
    this->current_channel_ = -1;
    return false;
  }

  // Restore pointer to Status
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  this->current_channel_ = channel;
  return true;
}

bool DS248xComponent::ow_reset(bool &presence) {
  // Ensure Read Pointer is at Status Register before command
  // We ignore the result here because if the device is already in a weird state,
  // we just want to try to send the reset command.
  this->set_read_pointer_(DS248X_POINTER_STATUS);

  // Send 1-Wire Reset command
  uint8_t cmd = DS248X_COMMAND_RESETWIRE;
  // We ignore the write result. If the device NACKs (e.g. because it went busy immediately),
  // we still check the status register to see if it's actually busy.
  this->write(&cmd, 1);

  if (!this->wait_busy_()) {
    ESP_LOGW(TAG, "ow_reset: Wait Busy Failed");
    return false;
  }

  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "ow_reset: Read Status Failed");
    return false;
  }

  if (status & DS248X_STATUS_SD) {
    ESP_LOGW(TAG, "Short detected on 1-Wire bus!");
    return false;
  }

  presence = (status & DS248X_STATUS_PPD);
  return true;
}

bool DS248xComponent::ow_write_byte(uint8_t byte, bool keep_strong_pullup) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  if (keep_strong_pullup) {
    // Ensure SPU is armed for the command that requires it (e.g., Convert T)
    if (!this->set_strong_pullup_mode_(true)) {
      ESP_LOGW(TAG, "Failed to arm Strong Pullup for byte 0x%02x", byte);
      return false;
    }
  }

  // Ensure device is not busy before sending command
  if (!this->wait_busy_()) {
     ESP_LOGW(TAG, "Device busy before writing byte 0x%02x", byte);
     return false;
  }

  // Send Command using raw write to ensure correct I2C sequence
  uint8_t cmd[2] = {DS248X_COMMAND_WRITEBYTE, byte};
  if (this->write(cmd, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C Write Failed for byte 0x%02x", byte);
    return false;
  }

  // Wait for the operation to complete
  // The DS2482 BUSY bit indicates the 1-Wire master is processing the command.
  // Even with SPU enabled, the BUSY bit goes low once the command byte is shifted out.
  // The SPU activates at the rising edge of the last bit and stays active until the next command.
  // So we just wait for BUSY to clear.
  if (!this->wait_busy_()) {
    ESP_LOGW(TAG, "Timeout waiting for write byte to complete!");
    return false;
  }

  if (!keep_strong_pullup && this->strong_pullup_active_) {
    // Caller did not request SPU hold; release to minimize config churn
    this->set_strong_pullup_mode_(false);
  }

  return true;
}

bool DS248xComponent::ow_read_byte(uint8_t &byte) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  uint8_t cmd = DS248X_COMMAND_READBYTE;
  // Ignore write error
  this->write(&cmd, 1);

  if (!this->wait_busy_())
    return false;

  if (!this->set_read_pointer_(DS248X_POINTER_DATA))
    return false;

  if (this->read(&byte, 1) != i2c::ERROR_OK)
    return false;

  return true;
}

bool DS248xComponent::ow_write_bit(bool bit) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  // Command 0x87: Single Bit. MSB indicates the bit value.
  // If bit is 1, use 0x87. If bit is 0, use 0x07.
  uint8_t cmd = (bit ? 0x87 : 0x07);
  this->write(&cmd, 1);

  return this->wait_busy_();
}

bool DS248xComponent::ow_read_bit(bool &bit) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return false;

  // Read slot is generated by writing a 1
  uint8_t cmd = 0x87;
  this->write(&cmd, 1);

  if (!this->wait_busy_())
    return false;

  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK)
    return false;

  // Result is in SBR bit (Bit 5)
  bit = (status & DS248X_STATUS_SBR) != 0;
  ESP_LOGVV(TAG, "ow_read_bit: status=0x%02x, bit=%d", status, bit);
  return true;
}

bool DS248xComponent::skip_rom() { return this->ow_write_byte(0xCC); }

bool DS248xComponent::match_rom(uint64_t address) {
  if (!this->ow_write_byte(0x55))
    return false;  // Match ROM
  for (int i = 0; i < 8; i++) {
    if (!this->ow_write_byte(static_cast<uint8_t>(address >> (i * 8))))
      return false;
  }
  return true;
}

uint8_t DS248xComponent::search_triplet(bool search_direction) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS))
    return 0;

  // Command 0x78: Triplet. MSB indicates the search direction.
  uint8_t cmd = DS248X_COMMAND_TRIPLET | (search_direction ? 0x80 : 0x00);
  this->write(&cmd, 1);

  if (!this->wait_busy_())
    return 0;

  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK)
    return 0;

  return status;
}

void DS248xComponent::search() { this->run_search_(0xF0, "Search ROM"); }

void DS248xComponent::alarm_search() { this->run_search_(0xEC, "Alarm Search"); }

void DS248xComponent::run_search_(uint8_t command, const char *label) {  // NOLINT(readability-identifier-naming)
  ESP_LOGI(TAG, "%s...", label);
  // TODO: Provide async/search action hook to trigger at runtime without blocking setup.
  uint8_t rom[8];
  int last_discrepancy = 0;
  bool last_device_flag = false;
  bool first = true;

  while (!last_device_flag) {
    if (first) {
      for (uint8_t &byte : rom) {
        byte = 0;
      }
      first = false;
    }

    bool presence;
    if (!this->ow_reset(presence) || !presence) {
      ESP_LOGW(TAG, "  No presence detected during search (channel %d)", this->current_channel_);
      // Treat empty channel as finished to avoid endless recovery loops
      break;
    }

    if (!this->ow_write_byte(command)) {
      ESP_LOGW(TAG, "  Failed to start %s", label);
      uint8_t recovery_channel = (this->current_channel_ >= 0) ? static_cast<uint8_t>(this->current_channel_) : 0;
      if (!this->recover_device_(recovery_channel, "search_command")) {
        return;
      }
      if (!this->select_channel(recovery_channel)) {
        ESP_LOGW(TAG, "  Channel reselect failed after search command recovery");
        return;
      }
      continue;
    }

    int id_bit_number = 1;
    int last_zero = 0;
    int rom_byte_number = 0;
    uint8_t rom_byte_mask = 1;
    bool search_result = false;
    uint8_t crc8 = 0;  // NOLINT(clang-diagnostic-unused-variable)

    do {
      // Determine Search Direction
      bool search_direction = false;
      if (id_bit_number < last_discrepancy) {
        search_direction = (rom[rom_byte_number] & rom_byte_mask) > 0;
      } else {
        // if equal to last pick 1, if not then pick 0
        search_direction = id_bit_number == last_discrepancy;
      }

      // Perform Triplet
      uint8_t status = this->search_triplet(search_direction);

      bool id_bit = (status & DS248X_STATUS_SBR);
      bool cmp_id_bit = (status & DS248X_STATUS_TSB);
      bool dir_taken = (status & DS248X_STATUS_DIR);

        if (id_bit && cmp_id_bit) {
          // No devices
          break;
      } else {
        if (!id_bit && !cmp_id_bit && !dir_taken) {
          last_zero = id_bit_number;
          // if (last_zero < 9) LastFamilyDiscrepancy = last_zero;
        }

        if (dir_taken) {
          rom[rom_byte_number] |= rom_byte_mask;
        } else {
          rom[rom_byte_number] &= ~rom_byte_mask;
        }

        id_bit_number++;
        rom_byte_mask <<= 1;

        if (rom_byte_mask == 0) {
          // Accumulate CRC
          // doc says: calc_crc8(ROM_NO[rom_byte_number]);
          // We can just check CRC at the end
          rom_byte_number++;
          rom_byte_mask = 1;
        }
        }
      } while (rom_byte_number < 8);

    if (id_bit_number >= 65) {
      // Search successful
      last_discrepancy = last_zero;
      if (last_discrepancy == 0)
        last_device_flag = true;
      search_result = true;
    }

    if (search_result) {
      // Check CRC
      if (esphome::crc8(rom, 7) != rom[7]) {
        ESP_LOGW(TAG, "  CRC Error for found device");
      } else {
        uint64_t addr = 0;
        for (int i = 7; i >= 0; i--) {
          addr = (addr << 8) | rom[i];
        }
        ESP_LOGI(TAG, "  Found device: %s", format_hex(addr).c_str());
      }
    } else {
      break;
    }
  }
  ESP_LOGI(TAG, "%s finished.", label);
}

std::string DS248xSensor::get_address_name() {
  if (this->address_ == 0)
    return "";
  return format_hex(this->address_);
}

}  // namespace ds248x
}  // namespace esphome
