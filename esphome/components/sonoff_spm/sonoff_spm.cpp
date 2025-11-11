#include "sonoff_spm.h"
#include "switch/sonoff_spm_switch.h"
#include "sensor/sonoff_spm_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sonoff_spm {

static const char *const TAG = "sonoff_spm";

void SonoffSPM::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Sonoff SPM...");
  this->state_ = SPM_STATE_RESET;
}

void SonoffSPM::loop() {
  // Process incoming serial data
  this->process_serial_();

  // Update state machine
  this->update_state_machine_();

  // Request energy updates periodically (every 5 seconds)
  uint32_t now = millis();
  if (this->state_ == SPM_STATE_RUNNING && now - this->last_energy_request_ > 5000) {
    this->request_energy_updates_();
    this->last_energy_request_ = now;
  }
}

void SonoffSPM::dump_config() {
  ESP_LOGCONFIG(TAG, "Sonoff SPM:");
  ESP_LOGCONFIG(TAG, "  Max Modules: %d (up to %d relays)", this->module_count_, this->module_count_ * 4);
  ESP_LOGCONFIG(TAG, "  Scanned Modules: %d (%d relays)", this->scanned_modules_, this->scanned_modules_ * 4);
  ESP_LOGCONFIG(TAG, "  Main Version: %d.%d.%d", (this->main_version_ >> 16) & 0xFF, (this->main_version_ >> 8) & 0xFF,
                this->main_version_ & 0xFF);
  ESP_LOGCONFIG(TAG, "  Registered Switches: %d", this->switches_.size());
  ESP_LOGCONFIG(TAG, "  Registered Sensors: %d", this->sensors_.size());
}

void SonoffSPM::register_switch(SonoffSPMSwitch *switch_obj, uint8_t relay_id) {
  if (relay_id >= SPM_MAX_RELAYS) {
    ESP_LOGE(TAG, "Invalid relay ID: %d", relay_id);
    return;
  }
  this->switches_.push_back(switch_obj);
}

void SonoffSPM::register_sensor(SonoffSPMSensor *sensor, uint8_t relay_id) {
  if (relay_id >= SPM_MAX_RELAYS) {
    ESP_LOGE(TAG, "Invalid relay ID: %d", relay_id);
    return;
  }
  this->sensors_.push_back(sensor);
}

void SonoffSPM::set_relay_state(uint8_t relay_id, bool state) {
  if (relay_id >= SPM_MAX_RELAYS) {
    ESP_LOGE(TAG, "Invalid relay ID: %d", relay_id);
    return;
  }
  this->send_set_relay_(relay_id, state);
}

const ChannelData *SonoffSPM::get_channel_data(uint8_t relay_id) const {
  if (relay_id >= SPM_MAX_RELAYS) {
    return nullptr;
  }
  return &this->channels_[relay_id];
}

bool SonoffSPM::get_relay_state(uint8_t relay_id) const {
  if (relay_id >= SPM_MAX_RELAYS) {
    return false;
  }
  return this->channels_[relay_id].relay_state;
}

uint16_t SonoffSPM::calculate_crc_(const uint8_t *data, size_t len) {
  // CRC-16/ARC (polynomial 0x8005 reflected as 0xA001)
  uint16_t crc = 0;
  for (size_t i = 2; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

void SonoffSPM::init_send_() {
  memset(this->buffer_.data(), 0, 19);
  this->buffer_[0] = 0xAA;
  this->buffer_[1] = 0x55;
  this->buffer_[2] = 0x01;
}

void SonoffSPM::send_buffer_(size_t size) {
  uint16_t crc = this->calculate_crc_(this->buffer_.data(), size - 2);
  this->buffer_[size - 2] = (crc >> 8) & 0xFF;
  this->buffer_[size - 1] = crc & 0xFF;

  ESP_LOGVV(TAG, "TX: %s", format_hex_pretty(this->buffer_.data(), size).c_str());
  this->write_array(this->buffer_.data(), size);
  this->flush();
}

void SonoffSPM::send_command_(uint8_t command) {
  this->init_send_();
  this->buffer_[16] = command;
  if (command != SPM_FUNC_FIND) {
    this->command_sequence_++;
  } else {
    this->command_sequence_ = 0;
  }
  this->buffer_[19] = this->command_sequence_;
  this->send_buffer_(22);
}

void SonoffSPM::send_find_() {
  this->init_send_();
  this->buffer_[15] = 0x80;  // Ack
  this->buffer_[18] = 1;
  this->buffer_[19] = 0;
  this->send_buffer_(23);
}

void SonoffSPM::send_init_scan_() {
  memset(this->buffer_.data(), 0xFF, 15);
  this->buffer_[0] = 0xAA;
  this->buffer_[1] = 0x55;
  this->buffer_[2] = 0x01;
  this->buffer_[15] = 0;
  this->buffer_[16] = SPM_FUNC_INIT_SCAN;
  this->buffer_[17] = 0;
  this->buffer_[18] = 0;
  this->command_sequence_++;
  this->buffer_[19] = this->command_sequence_;
  this->send_buffer_(22);
  ESP_LOGD(TAG, "Starting module scan...");
}

void SonoffSPM::send_get_main_version_() { this->send_command_(SPM_FUNC_GET_MAIN_VERSION); }

void SonoffSPM::send_set_relay_(uint8_t relay_id, bool state) {
  uint8_t module = relay_id / SPM_CHANNELS_PER_MODULE;
  uint8_t channel = relay_id % SPM_CHANNELS_PER_MODULE;

  if (module >= this->scanned_modules_) {
    ESP_LOGE(TAG, "Module %d not found", module);
    return;
  }

  uint8_t channel_mask = 1 << channel;
  if (state) {
    channel_mask |= (channel_mask << 4);
  }

  this->init_send_();
  memcpy(&this->buffer_[3], this->modules_[module].module_id.data(), SPM_MODULE_NAME_SIZE);
  this->buffer_[16] = SPM_FUNC_SET_RELAY;
  this->buffer_[18] = 0x01;
  this->buffer_[19] = channel_mask;
  this->command_sequence_++;
  this->buffer_[20] = this->command_sequence_;
  this->send_buffer_(23);
}

void SonoffSPM::send_get_module_state_(uint8_t module) {
  if (module >= this->scanned_modules_) {
    return;
  }

  this->init_send_();
  memcpy(&this->buffer_[3], this->modules_[module].module_id.data(), SPM_MODULE_NAME_SIZE);
  this->buffer_[16] = SPM_FUNC_GET_MODULE_STATE;
  this->buffer_[18] = 0x01;
  this->buffer_[19] = 0x0F;  // State of all four relays
  this->command_sequence_++;
  this->buffer_[20] = this->command_sequence_;
  this->send_buffer_(23);
}

void SonoffSPM::send_ack_(uint8_t sequence) {
  this->buffer_[15] = 0x80;
  this->buffer_[17] = 0x00;
  this->buffer_[18] = 0x01;
  this->buffer_[19] = 0x00;
  this->buffer_[20] = sequence;
  this->send_buffer_(23);
}

float SonoffSPM::get_value_(const uint8_t *buffer) {
  // Return float from three bytes in buffer
  return (buffer[0] << 8) + buffer[1] + static_cast<float>(buffer[2]) / 100.0f;
}

void SonoffSPM::set_value_(uint8_t *buffer, float value) {
  // Store float in three bytes
  uint32_t integer = static_cast<uint32_t>(value);
  buffer[0] = (integer >> 8) & 0xFF;
  buffer[1] = integer & 0xFF;
  buffer[2] = static_cast<uint8_t>((value * 100.0f) - (integer * 100.0f));
}

void SonoffSPM::process_serial_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    // Check for start of message
    if ((byte == 0x01) && (this->serial_in_byte_counter_ >= 2) &&
        (this->buffer_[this->serial_in_byte_counter_ - 1] == 0x55) &&
        (this->buffer_[this->serial_in_byte_counter_ - 2] == 0xAA)) {
      // Start of new message
      this->expected_bytes_ = 0;
      this->buffer_[0] = 0xAA;
      this->buffer_[1] = 0x55;
      this->serial_in_byte_counter_ = 2;
    }

    if (this->serial_in_byte_counter_ < SPM_SERIAL_BUFFER_SIZE - 1) {
      this->buffer_[this->serial_in_byte_counter_++] = byte;

      // Calculate expected message size after receiving header
      if ((this->serial_in_byte_counter_ == 19) && (this->buffer_[0] == 0xAA) && (this->buffer_[1] == 0x55) &&
          (this->buffer_[2] == 0x01)) {
        this->expected_bytes_ = 22 + (this->buffer_[17] << 8) + this->buffer_[18];
      }

      // Check if complete message received
      if (this->serial_in_byte_counter_ == this->expected_bytes_) {
        ESP_LOGVV(TAG, "RX: %s", format_hex_pretty(this->buffer_.data(), this->serial_in_byte_counter_).c_str());

        // Verify CRC
        uint16_t crc_received =
            (this->buffer_[this->serial_in_byte_counter_ - 2] << 8) | this->buffer_[this->serial_in_byte_counter_ - 1];
        uint16_t crc_calculated = this->calculate_crc_(this->buffer_.data(), this->serial_in_byte_counter_ - 2);

        if (crc_received == crc_calculated) {
          this->handle_received_data_();
        } else {
          ESP_LOGW(TAG, "CRC error: expected 0x%04X, got 0x%04X", crc_calculated, crc_received);
        }

        this->serial_in_byte_counter_ = 0;
        this->expected_bytes_ = 0;
      }
    } else {
      ESP_LOGW(TAG, "Serial buffer overflow");
      this->serial_in_byte_counter_ = 0;
      this->expected_bytes_ = 0;
    }
  }
}

void SonoffSPM::handle_received_data_() {
  bool ack = (this->buffer_[15] == 0x80);
  uint8_t command = this->buffer_[16];
  uint16_t size = (this->buffer_[17] << 8) + this->buffer_[18];
  uint8_t status = this->buffer_[19];

  ESP_LOGV(TAG, "Received command 0x%02X, ack=%d, size=%d, status=%d", command, ack, size, status);

  if (ack) {
    // Handle acknowledged commands
    switch (command) {
      case SPM_FUNC_GET_MAIN_VERSION:
        if (size >= 4) {
          this->main_version_ = (this->buffer_[20] << 16) | (this->buffer_[21] << 8) | this->buffer_[22];
          ESP_LOGI(TAG, "Main firmware version: %d.%d.%d", this->buffer_[20], this->buffer_[21], this->buffer_[22]);
          this->state_ = SPM_STATE_START_SCAN;
        }
        break;

      case SPM_FUNC_INIT_SCAN:
        ESP_LOGD(TAG, "Scan initiated");
        this->state_ = SPM_STATE_SCANNING;
        break;

      case SPM_FUNC_GET_MODULE_STATE:
        this->handle_module_state_();
        break;

      default:
        break;
    }
  } else {
    // Handle unsolicited messages from ARM
    uint8_t cmd_sequence = this->buffer_[19 + size];

    switch (command) {
      case SPM_FUNC_ENERGY_RESULT:
        this->handle_energy_result_();
        this->send_ack_(cmd_sequence);
        break;

      case SPM_FUNC_KEY_PRESS:
        this->handle_key_press_();
        this->send_ack_(cmd_sequence);
        break;

      case SPM_FUNC_SCAN_START:
        ESP_LOGD(TAG, "Scan started");
        this->send_ack_(cmd_sequence);
        break;

      case SPM_FUNC_SCAN_RESULT:
        this->handle_scan_result_();
        this->send_ack_(cmd_sequence);
        break;

      case SPM_FUNC_SCAN_DONE:
        this->handle_scan_done_();
        this->send_ack_(cmd_sequence);
        break;

      default:
        ESP_LOGV(TAG, "Unknown command: 0x%02X", command);
        break;
    }
  }
}

void SonoffSPM::handle_energy_result_() {
  // Extract module ID from bytes 19-20
  uint16_t module_id_short = (this->buffer_[19] << 8) | this->buffer_[20];

  // Find module by ID
  uint8_t module = 0xFF;
  for (uint8_t i = 0; i < this->scanned_modules_; i++) {
    uint16_t stored_id = (this->modules_[i].module_id[0] << 8) | this->modules_[i].module_id[1];
    if (stored_id == module_id_short) {
      module = i;
      break;
    }
  }

  if (module == 0xFF) {
    ESP_LOGV(TAG, "Energy result from unknown module");
    return;
  }

  uint8_t channel_mask = this->buffer_[31];
  size_t offset = 32;
  size_t max_offset = this->buffer_[18] + 18;

  for (uint8_t channel = 0; channel < 4; channel++) {
    if (!(channel_mask & (1 << channel))) {
      continue;
    }

    uint8_t relay_id = (module * 4) + channel;
    if (offset + 14 > max_offset) {
      break;
    }

    // Only update if relay is on
    if (this->channels_[relay_id].relay_state) {
      this->channels_[relay_id].current =
          this->buffer_[offset] + static_cast<float>(this->buffer_[offset + 1]) / 100.0f;
      this->channels_[relay_id].voltage = this->get_value_(&this->buffer_[offset + 2]);
      this->channels_[relay_id].active_power = this->get_value_(&this->buffer_[offset + 5]);
      this->channels_[relay_id].reactive_power = this->get_value_(&this->buffer_[offset + 8]);
      this->channels_[relay_id].apparent_power = this->get_value_(&this->buffer_[offset + 11]);

      if (this->channels_[relay_id].apparent_power > 0) {
        this->channels_[relay_id].power_factor =
            this->channels_[relay_id].active_power / this->channels_[relay_id].apparent_power;
        if (this->channels_[relay_id].power_factor > 1.0f) {
          this->channels_[relay_id].power_factor = 1.0f;
        }
      }

      ESP_LOGV(TAG, "Relay %d: %.1fV, %.2fA, %.1fW", relay_id, this->channels_[relay_id].voltage,
               this->channels_[relay_id].current, this->channels_[relay_id].active_power);
    }

    offset += 14;
  }
}

void SonoffSPM::handle_key_press_() {
  // Extract module ID from bytes 19-20
  uint16_t module_id_short = (this->buffer_[19] << 8) | this->buffer_[20];

  // Find module by ID
  uint8_t module = 0xFF;
  for (uint8_t i = 0; i < this->scanned_modules_; i++) {
    uint16_t stored_id = (this->modules_[i].module_id[0] << 8) | this->modules_[i].module_id[1];
    if (stored_id == module_id_short) {
      module = i;
      break;
    }
  }

  if (module == 0xFF) {
    return;
  }

  uint8_t relay_mask = this->buffer_[31] & 0x0F;
  uint8_t relay_state = this->buffer_[31] >> 4;

  for (uint8_t channel = 0; channel < 4; channel++) {
    if (relay_mask & (1 << channel)) {
      uint8_t relay_id = (module * 4) + channel;
      bool state = (relay_state & (1 << channel)) != 0;
      this->channels_[relay_id].relay_state = state;
      ESP_LOGD(TAG, "Relay %d state changed to %s", relay_id, state ? "ON" : "OFF");

      // Notify switches
      for (auto *switch_obj : this->switches_) {
        // Switch will check if it matches this relay_id
        switch_obj->publish_state(state);
      }
    }
  }
}

void SonoffSPM::handle_scan_result_() {
  if (this->scanned_modules_ >= SPM_MAX_MODULES) {
    ESP_LOGW(TAG, "Too many modules scanned");
    return;
  }

  // Copy module ID (bytes 19-30)
  memcpy(this->modules_[this->scanned_modules_].module_id.data(), &this->buffer_[19], SPM_MODULE_NAME_SIZE);

  // Extract firmware version
  this->modules_[this->scanned_modules_].firmware_version =
      (this->buffer_[37] << 16) | (this->buffer_[38] << 8) | this->buffer_[39];
  this->modules_[this->scanned_modules_].online = true;

  ESP_LOGI(TAG, "Module %d: FW v%d.%d.%d", this->scanned_modules_, this->buffer_[37], this->buffer_[38],
           this->buffer_[39]);

  this->scanned_modules_++;
}

void SonoffSPM::handle_scan_done_() {
  ESP_LOGI(TAG, "Scan complete: %d modules found", this->scanned_modules_);

  if (this->scanned_modules_ > 0) {
    this->current_module_ = 0;
    this->state_ = SPM_STATE_GET_STATES;
  } else {
    ESP_LOGW(TAG, "No modules found");
    this->state_ = SPM_STATE_IDLE;
  }
}

void SonoffSPM::handle_module_state_() {
  if (this->buffer_[18] < 6) {
    return;
  }

  // Extract module ID from bytes 3-4
  uint16_t module_id_short = (this->buffer_[3] << 8) | this->buffer_[4];

  // Find module by ID
  uint8_t module = 0xFF;
  for (uint8_t i = 0; i < this->scanned_modules_; i++) {
    uint16_t stored_id = (this->modules_[i].module_id[0] << 8) | this->modules_[i].module_id[1];
    if (stored_id == module_id_short) {
      module = i;
      break;
    }
  }

  if (module == 0xFF) {
    return;
  }

  uint8_t relay_state = this->buffer_[20] >> 4;

  for (uint8_t channel = 0; channel < 4; channel++) {
    uint8_t relay_id = (module * 4) + channel;
    bool state = (relay_state & (1 << channel)) != 0;
    this->channels_[relay_id].relay_state = state;
    ESP_LOGD(TAG, "Module %d, relay %d initial state: %s", module, channel, state ? "ON" : "OFF");
  }

  // Move to next module
  this->current_module_++;
  if (this->current_module_ >= this->scanned_modules_) {
    ESP_LOGI(TAG, "All module states retrieved, entering running mode");
    this->state_ = SPM_STATE_RUNNING;
  }
}

void SonoffSPM::update_state_machine_() {
  uint32_t now = millis();

  switch (this->state_) {
    case SPM_STATE_IDLE:
      // Do nothing
      break;

    case SPM_STATE_RESET:
      // Wait a bit after setup
      if (now > 1000) {
        this->send_get_main_version_();
        this->state_ = SPM_STATE_WAIT_VERSION;
      }
      break;

    case SPM_STATE_WAIT_VERSION:
      // Waiting for version response
      break;

    case SPM_STATE_START_SCAN:
      this->scanned_modules_ = 0;
      this->send_init_scan_();
      this->last_scan_time_ = now;
      this->state_ = SPM_STATE_SCANNING;
      break;

    case SPM_STATE_SCANNING:
      // Waiting for scan to complete (handled by SCAN_DONE message)
      // Timeout after 30 seconds
      if (now - this->last_scan_time_ > 30000) {
        ESP_LOGW(TAG, "Scan timeout");
        this->state_ = SPM_STATE_IDLE;
      }
      break;

    case SPM_STATE_GET_STATES:
      if (this->current_module_ < this->scanned_modules_) {
        this->send_get_module_state_(this->current_module_);
      }
      break;

    case SPM_STATE_RUNNING:
      // Normal operation
      break;
  }
}

void SonoffSPM::start_scan_() { this->state_ = SPM_STATE_START_SCAN; }

void SonoffSPM::request_energy_updates_() {
  // Energy updates are sent automatically by the ARM processor
  // No explicit request needed
}

}  // namespace sonoff_spm
}  // namespace esphome
