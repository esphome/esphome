#include "ds248x.h"
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
    this->sleep_pin_->digital_write(true); // Wake up
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
      delay(100);
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

  // 3. Select Channel 0
  if (!this->select_channel(0)) {
      ESP_LOGE(TAG, "DS248x Channel 0 Selection failed!");
      this->mark_failed();
      return;
  }

  ESP_LOGI(TAG, "DS248x Initialized successfully.");
}

void DS248xComponent::on_shutdown() {
    if (this->sleep_pin_) {
        this->sleep_pin_->digital_write(false); // Sleep
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

void DS248xComponent::register_sensor(DS248xSensor *sensor) {
  this->sensors_.push_back(sensor);
}

void DS248xComponent::update() {
  this->process_next_channel_(0);
}

void DS248xComponent::process_next_channel_(uint8_t channel_idx) {
  if (channel_idx >= this->channel_count_) {
    return; // Done
  }

  // Check if we have sensors on this channel
  bool has_sensors = false;
  bool needs_strong_pullup = this->strong_pullup_enabled_; // Global setting

  for (auto *s : this->sensors_) {
    if (s->get_channel() == channel_idx) {
      has_sensors = true;
      if (s->get_parasitic_mode()) {
          needs_strong_pullup = true;
      }
    }
  }

  if (!has_sensors) {
    this->process_next_channel_(channel_idx + 1);
    return;
  }

  // Start conversion on this channel
  if (!this->select_channel(channel_idx)) {
    ESP_LOGW(TAG, "Failed to select channel %d for update", channel_idx);
    this->process_next_channel_(channel_idx + 1);
    return;
  }

  bool presence;
  if (!this->ow_reset(presence) || !presence) {
    ESP_LOGW(TAG, "No presence on channel %d", channel_idx);
    this->process_next_channel_(channel_idx + 1);
    return;
  }

  // Skip ROM (CC) -> Convert T (44)
  if (!this->skip_rom()) return;
  
  // Enable Strong Pullup for Parasitic Power
  // This must be done BEFORE the command that requires the pullup (Convert T)
  // The DS2482 will activate the pullup immediately after the next byte is written.
  if (needs_strong_pullup) {
      if (!this->set_strong_pullup_mode_(true)) {
          ESP_LOGW(TAG, "Failed to enable Strong Pullup");
      }
  }
  
  if (!this->ow_write_byte(0x44)) return;

  // Wait for conversion (750ms)
  // Note: We assume standard resolution (12-bit) which needs 750ms.
  // The Strong Pullup is active during this time.
  
  this->set_timeout(750, [this, channel_idx]() {
    this->process_channel_readout_(channel_idx);
  });
}

void DS248xComponent::process_channel_readout_(uint8_t channel_idx) {
  // Re-select channel to be safe
  if (!this->select_channel(channel_idx)) {
    this->process_next_channel_(channel_idx + 1);
    return;
  }
  
  this->process_sensor_readout_(channel_idx, 0);
}

void DS248xComponent::process_sensor_readout_(uint8_t channel_idx, uint8_t sensor_idx) {
  DS248xSensor* sensor = nullptr;
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
      this->process_next_channel_(channel_idx + 1);
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
  for (uint8_t & val : scratchpad) {
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

  if (family_code == 0x28 || family_code == 0x22) {
      // DS18B20 (0x28) or DS1822 (0x22)
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
                  this->ow_write_byte(0x4E); // Write Scratchpad
                  this->ow_write_byte(scratchpad[2]); // Th
                  this->ow_write_byte(scratchpad[3]); // Tl
                  this->ow_write_byte(desired_res_byte); // Config
                  
                  // Copy to EEPROM to make it permanent
                  // We must reset again before the next command sequence (Copy Scratchpad)
                  if (this->ow_reset(presence) && presence) {
                      if (this->match_rom(sensor->get_address())) {
                          // Enable SPU if parasitic
                          bool use_spu = sensor->get_parasitic_mode() || this->strong_pullup_enabled_;
                          if (use_spu) {
                              this->set_strong_pullup_mode_(true);
                          }
                          
                          this->ow_write_byte(0x48); // Copy Scratchpad
                          
                          // ASYNC WAIT: Schedule callback instead of delay(10)
                          this->set_timeout(10, [this, channel_idx, next_idx, use_spu]() {
                              if (use_spu) {
                                  this->set_strong_pullup_mode_(false); // Turn off SPU
                                  this->set_read_pointer_(DS248X_POINTER_STATUS); // Restore pointer
                              }
                              // Continue with next sensor
                              this->process_sensor_readout_(channel_idx, next_idx);
                          });
                          return; // EXIT HERE, callback will continue
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
      // float temp_c = ((scratchpad[0] >> 1) | (scratchpad[1] << 7)) - 0.25 + ((float)(scratchpad[7] - scratchpad[6]) / scratchpad[7]);
      
      // Simplified high precision calculation:
      int16_t temp_raw = (scratchpad[1] << 8) | scratchpad[0];
      if (scratchpad[7] != 0) {
           float t = (temp_raw & 0xFFFE) / 2.0f;
           t += 0.75f + ((float)scratchpad[7] - (float)scratchpad[6]) / (float)scratchpad[7];
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

bool DS248xComponent::set_read_pointer_(uint8_t ptr) {
  return this->write_byte(DS248X_COMMAND_SETREADPTR, ptr);
}

bool DS248xComponent::wait_busy_() {
  // We assume the Read Pointer is already at Status Register (F0h).
  // We ensure this by setting the pointer BEFORE issuing any command that causes busyness.
  
  uint32_t start = millis();
  while (millis() - start < 20) {
    uint8_t status;
    if (this->read(&status, 1) != i2c::ERROR_OK) {
        // Read failed. Retry.
        delayMicroseconds(100);
        continue;
    }
    if (!(status & DS248X_STATUS_BUSY)) return true;
    delayMicroseconds(100);
  }
  ESP_LOGW(TAG, "DS248x Busy Timeout");
  // Try to recover?
  this->device_reset_(); 
  this->current_channel_ = -1; // Invalidate state
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
  if (this->read(&status, 1) != i2c::ERROR_OK) return false;
  
  if (!(status & DS248X_STATUS_RST)) {
      ESP_LOGW(TAG, "Device Reset Failed (RST bit not set)");
      return false;
  }
  
  this->current_channel_ = -1;
  return true;
}

bool DS248xComponent::device_configure_() {
  ESP_LOGD(TAG, "Configuring device...");
  uint8_t config = 0;
  if (this->active_pullup_) config |= DS248X_CONFIG_ACTIVE_PULLUP;
  if (this->overdrive_speed_) config |= DS248X_CONFIG_OVERDRIVE;
  
  uint8_t config_byte = (config & 0x0F) | ((~config & 0x0F) << 4);
  
  if (!this->write_byte(DS248X_COMMAND_WRITECONFIG, config_byte)) {
      ESP_LOGW(TAG, "Config Write Failed");
      return false;
  }
  
  // Verify Config (Pointer is at Config Register now)
  uint8_t read_config;
  if (this->read(&read_config, 1) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Config Read Failed");
      return false;
  }
  
  if (read_config != config) {
      ESP_LOGW(TAG, "Config Mismatch! Wrote: 0x%02x, Read: 0x%02x", config, read_config);
      return false;
  }
  
  // Restore pointer to Status
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS)) return false;
  
  ESP_LOGD(TAG, "Configured successfully");

  // DS2484 Configuration
  if (this->ds2484_mode_) {
      if (this->ds2484_trstl_ > 0) this->configure_ds2484_port_(0x0, this->ds2484_trstl_);
      if (this->ds2484_tmsp_ > 0) this->configure_ds2484_port_(0x1, this->ds2484_tmsp_);
      if (this->ds2484_tw0l_ > 0) this->configure_ds2484_port_(0x2, this->ds2484_tw0l_);
      if (this->ds2484_trec0_ > 0) this->configure_ds2484_port_(0x3, this->ds2484_trec0_);
      if (this->ds2484_rwpu_ > 0) this->configure_ds2484_port_(0x4, this->ds2484_rwpu_);
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
  if (this->active_pullup_) config |= DS248X_CONFIG_ACTIVE_PULLUP;
  if (this->overdrive_speed_) config |= DS248X_CONFIG_OVERDRIVE;
  if (enable) config |= DS248X_CONFIG_STRONG_PULLUP;
  
  uint8_t config_byte = (config & 0x0F) | ((~config & 0x0F) << 4);
  
  if (!this->write_byte(DS248X_COMMAND_WRITECONFIG, config_byte)) {
      return false;
  }
  
  // We don't verify here to save time, and we don't reset pointer because
  // usually the next command sets it or doesn't care.
  // But to be safe for wait_busy(), we should probably set pointer to Status?
  // Actually, Write Config sets pointer to Config.
  // So we MUST set it back to Status if we expect wait_busy() to work later.
  return this->set_read_pointer_(DS248X_POINTER_STATUS);
}

bool DS248xComponent::select_channel(uint8_t channel) {
  if (this->channel_count_ <= 1) return true;
  if (channel >= this->channel_count_) return false;
  
  if (this->current_channel_ == channel) return true;

  uint8_t ch_code = 0;
  switch (channel) {
    case 0: ch_code = 0xF0; break;
    case 1: ch_code = 0xE1; break;
    case 2: ch_code = 0xD2; break;
    case 3: ch_code = 0xC3; break;
    case 4: ch_code = 0xB4; break;
    case 5: ch_code = 0xA5; break;
    case 6: ch_code = 0x96; break;
    case 7: ch_code = 0x87; break;
    default: return false;
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
      case 0: expected_read = 0xB8; break;
      case 1: expected_read = 0xB1; break;
      case 2: expected_read = 0xAA; break;
      case 3: expected_read = 0xA3; break;
      case 4: expected_read = 0x9C; break;
      case 5: expected_read = 0x95; break;
      case 6: expected_read = 0x8E; break;
      case 7: expected_read = 0x87; break;
  }
  
  if (read_code != expected_read) {
      ESP_LOGW(TAG, "Channel Select Failed! Expected 0x%02x, got 0x%02x", expected_read, read_code);
      this->current_channel_ = -1;
      return false;
  }

  // Restore pointer to Status
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS)) return false;
  
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

bool DS248xComponent::ow_write_byte(uint8_t byte) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS)) return false;
  
  // Ignore write error (NACK) as device might be busy immediately
  this->write_byte(DS248X_COMMAND_WRITEBYTE, byte);
  
  return this->wait_busy_();
}

bool DS248xComponent::ow_read_byte(uint8_t &byte) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS)) return false;
  
  uint8_t cmd = DS248X_COMMAND_READBYTE;
  // Ignore write error
  this->write(&cmd, 1);
  
  if (!this->wait_busy_()) return false;
  
  if (!this->set_read_pointer_(DS248X_POINTER_DATA)) return false;
  
  if (this->read(&byte, 1) != i2c::ERROR_OK) return false;
  
  return true;
}

bool DS248xComponent::ow_write_bit(bool bit) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS)) return false;
  
  // Command 0x87: Single Bit. MSB indicates the bit value.
  uint8_t cmd = DS248X_COMMAND_SINGLEBIT | (bit ? 0x80 : 0x00);
  this->write(&cmd, 1);
  
  return this->wait_busy_();
}

bool DS248xComponent::ow_read_bit(bool &bit) {
  if (!this->set_read_pointer_(DS248X_POINTER_STATUS)) return false;
  
  // Read slot is generated by writing a 1
  uint8_t cmd = DS248X_COMMAND_SINGLEBIT | 0x80;
  this->write(&cmd, 1);
  
  if (!this->wait_busy_()) return false;
  
  uint8_t status;
  if (this->read(&status, 1) != i2c::ERROR_OK) return false;
  
  // Result is in SBR bit (Bit 5)
  bit = (status & DS248X_STATUS_SBR);
  return true;
}

bool DS248xComponent::skip_rom() {
    return this->ow_write_byte(0xCC);
}

bool DS248xComponent::match_rom(uint64_t address) {
  if (!this->ow_write_byte(0x55)) return false; // Match ROM
  for (int i = 0; i < 8; i++) {
    if (!this->ow_write_byte(static_cast<uint8_t>(address >> (i * 8)))) return false;
  }
  return true;
}

std::string DS248xSensor::get_address_name() {
  if (this->address_ == 0) return "";
  return format_hex(this->address_);
}

}  // namespace ds248x
}  // namespace esphome
