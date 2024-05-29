/*
  sonoff_d1.cpp - Sonoff D1 Dimmer support for ESPHome

  Copyright © 2021 Anatoly Savchenkov
  Copyright © 2020 Jeff Rescignano

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  and associated documentation files (the “Software”), to deal in the Software without
  restriction, including without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  -----

  If modifying this file, in addition to the license above, please ensure to include links back to the original code:
  https://jeffresc.dev/blog/2020-10-10
  https://github.com/JeffResc/Sonoff-D1-Dimmer
  https://github.com/arendst/Tasmota/blob/2d4a6a29ebc7153dbe2717e3615574ac1c84ba1d/tasmota/xdrv_37_sonoff_d1.ino#L119-L131

  -----
*/

/*********************************************************************************************\
 * Sonoff D1 dimmer 433
 * Mandatory/Optional
 *  ^  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F 10
 *  M AA 55                                              - Header
 *  M       01 04                                        - Version?
 *  M             00 0A                                  - Following data length (10 bytes)
 *  O                   01                               - Power state (00 = off, 01 = on, FF = ignore)
 *  O                      64                            - Dimmer percentage (01 to 64 = 1 to 100%, 0 - ignore)
 *  O                         FF FF FF FF FF FF FF FF    - Not used
 *  M                                                 6C - CRC over bytes 2 to F (Addition)
\*********************************************************************************************/
#include "sonoff_d1.h"

namespace esphome {
namespace sonoff_d1 {

static const char *const TAG = "sonoff_d1";

uint8_t SonoffD1Output::calc_checksum_(const uint8_t *cmd, const size_t len) {
  uint8_t crc = 0;
  for (int i = 2; i < len - 1; i++) {
    crc += cmd[i];
  }
  return crc;
}

void SonoffD1Output::populate_checksum_(uint8_t *cmd, const size_t len) {
  // Update the checksum
  cmd[len - 1] = this->calc_checksum_(cmd, len);
}

void SonoffD1Output::skip_command_() {
  size_t garbage = 0;
  // Read out everything from the UART FIFO
  while (this->available()) {
    uint8_t value = this->read();
    ESP_LOGW(TAG, "[%04d] Skip %02d: 0x%02x from the dimmer", this->write_count_, garbage, value);
    garbage++;
  }

  // Warn about unexpected bytes in the protocol with UART dimmer
  if (garbage) {
    ESP_LOGW(TAG, "[%04d] Skip %d bytes from the dimmer", this->write_count_, garbage);
  }
}

// This assumes some data is already available
bool SonoffD1Output::read_command_(uint8_t *cmd, size_t &len) {
  // Do consistency check
  if (cmd == nullptr || len < 7) {
    ESP_LOGW(TAG, "[%04d] Too short command buffer (actual len is %d bytes, minimal is 7)", this->write_count_, len);
    return false;
  }

  // Read a minimal packet
  if (this->read_array(cmd, 6)) {
    ESP_LOGV(TAG, "[%04d] Reading from dimmer:", this->write_count_);
    ESP_LOGV(TAG, "[%04d] %s", this->write_count_, format_hex_pretty(cmd, 6).c_str());

    if (cmd[0] != 0xAA || cmd[1] != 0x55) {
      ESP_LOGW(TAG, "[%04d] RX: wrong header (%x%x, must be AA55)", this->write_count_, cmd[0], cmd[1]);
      this->skip_command_();
      return false;
    }
    if ((cmd[5] + 7 /*mandatory header + crc suffix length*/) > len) {
      ESP_LOGW(TAG, "[%04d] RX: Payload length is unexpected (%d, max expected %d)", this->write_count_, cmd[5],
               len - 7);
      this->skip_command_();
      return false;
    }
    if (this->read_array(&cmd[6], cmd[5] + 1 /*checksum suffix*/)) {
      ESP_LOGV(TAG, "[%04d] %s", this->write_count_, format_hex_pretty(&cmd[6], cmd[5] + 1).c_str());

      // Check the checksum
      uint8_t valid_checksum = this->calc_checksum_(cmd, cmd[5] + 7);
      if (valid_checksum != cmd[cmd[5] + 7 - 1]) {
        ESP_LOGW(TAG, "[%04d] RX: checksum mismatch (%d, expected %d)", this->write_count_, cmd[cmd[5] + 7 - 1],
                 valid_checksum);
        this->skip_command_();
        return false;
      }
      len = cmd[5] + 7 /*mandatory header + suffix length*/;

      // Read remaining gardbled data (just in case, I don't see where this can appear now)
      this->skip_command_();
      return true;
    }
  } else {
    ESP_LOGW(TAG, "[%04d] RX: feedback timeout", this->write_count_);
    this->skip_command_();
  }
  return false;
}

bool SonoffD1Output::read_ack_(const uint8_t *cmd, const size_t len) {
  // Expected acknowledgement from rf chip
  uint8_t ref_buffer[7] = {0xAA, 0x55, cmd[2], cmd[3], 0x00, 0x00, 0x00};
  uint8_t buffer[sizeof(ref_buffer)] = {0};
  uint32_t pos = 0;
  size_t buf_len = sizeof(ref_buffer);

  // Update the reference checksum
  this->populate_checksum_(ref_buffer, sizeof(ref_buffer));

  // Read ack code, this either reads 7 bytes or exits with a timeout
  this->read_command_(buffer, buf_len);

  // Compare response with expected response
  while (pos < sizeof(ref_buffer) && ref_buffer[pos] == buffer[pos]) {
    pos++;
  }
  if (pos == sizeof(ref_buffer)) {
    ESP_LOGD(TAG, "[%04d] Acknowledge received", this->write_count_);
    return true;
  } else {
    ESP_LOGW(TAG, "[%04d] Unexpected acknowledge received (possible clash of RF/HA commands), expected ack was:",
             this->write_count_);
    ESP_LOGW(TAG, "[%04d] %s", this->write_count_, format_hex_pretty(ref_buffer, sizeof(ref_buffer)).c_str());
  }
  return false;
}

bool SonoffD1Output::write_command_(uint8_t *cmd, const size_t len, bool needs_ack) {
  // Do some consistency checks
  if (len < 7) {
    ESP_LOGW(TAG, "[%04d] Too short command (actual len is %d bytes, minimal is 7)", this->write_count_, len);
    return false;
  }
  if (cmd[0] != 0xAA || cmd[1] != 0x55) {
    ESP_LOGW(TAG, "[%04d] Wrong header (%x%x, must be AA55)", this->write_count_, cmd[0], cmd[1]);
    return false;
  }
  if ((cmd[5] + 7 /*mandatory header + suffix length*/) != len) {
    ESP_LOGW(TAG, "[%04d] Payload length field does not match packet length (%d, expected %d)", this->write_count_,
             cmd[5], len - 7);
    return false;
  }
  this->populate_checksum_(cmd, len);

  // Need retries here to handle the following cases:
  // 1. On power up companion MCU starts to respond with a delay, so few first commands are ignored
  // 2. UART command initiated by this component can clash with a command initiated by RF
  uint32_t retries = 10;
  do {
    ESP_LOGV(TAG, "[%04d] Writing to the dimmer:", this->write_count_);
    ESP_LOGV(TAG, "[%04d] %s", this->write_count_, format_hex_pretty(cmd, len).c_str());
    this->write_array(cmd, len);
    this->write_count_++;
    if (!needs_ack)
      return true;
    retries--;
  } while (!this->read_ack_(cmd, len) && retries > 0);

  if (retries) {
    return true;
  } else {
    ESP_LOGE(TAG, "[%04d] Unable to write to the dimmer", this->write_count_);
  }
  return false;
}

bool SonoffD1Output::control_dimmer_(const bool binary, const uint8_t brightness) {
  // Include our basic code from the Tasmota project, thank you again!
  //                    0     1     2     3     4     5     6     7     8
  uint8_t cmd[17] = {0xAA, 0x55, 0x01, 0x04, 0x00, 0x0A, 0x00, 0x00, 0xFF,
                     // 9     10    11    12    13    14    15    16
                     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

  cmd[6] = binary;
  cmd[7] = remap<uint8_t, uint8_t>(brightness, 0, 100, this->min_value_, this->max_value_);
  ESP_LOGI(TAG, "[%04d] Setting dimmer state to %s, raw brightness=%d", this->write_count_, ONOFF(binary), cmd[7]);
  return this->write_command_(cmd, sizeof(cmd));
}

void SonoffD1Output::process_command_(const uint8_t *cmd, const size_t len) {
  if (cmd[2] == 0x01 && cmd[3] == 0x04 && cmd[4] == 0x00 && cmd[5] == 0x0A) {
    uint8_t ack_buffer[7] = {0xAA, 0x55, cmd[2], cmd[3], 0x00, 0x00, 0x00};
    // Ack a command from RF to ESP to prevent repeating commands
    this->write_command_(ack_buffer, sizeof(ack_buffer), false);
    ESP_LOGI(TAG, "[%04d] RF sets dimmer state to %s, raw brightness=%d", this->write_count_, ONOFF(cmd[6]), cmd[7]);
    const uint8_t new_brightness = remap<uint8_t, uint8_t>(cmd[7], this->min_value_, this->max_value_, 0, 100);
    const bool new_state = cmd[6];

    // Got light change state command. In all cases we revert the command immediately
    // since we want to rely on ESP controlled transitions
    if (new_state != this->last_binary_ || new_brightness != this->last_brightness_) {
      this->control_dimmer_(this->last_binary_, this->last_brightness_);
    }

    if (!this->use_rm433_remote_) {
      // If RF remote is not used, this is a known ghost RF command
      ESP_LOGI(TAG, "[%04d] Ghost command from RF detected, reverted", this->write_count_);
    } else {
      // If remote is used, initiate transition to the new state
      this->publish_state_(new_state, new_brightness);
    }
  } else {
    ESP_LOGW(TAG, "[%04d] Unexpected command received", this->write_count_);
  }
}

void SonoffD1Output::publish_state_(const bool is_on, const uint8_t brightness) {
  if (light_state_) {
    ESP_LOGV(TAG, "Publishing new state: %s, brightness=%d", ONOFF(is_on), brightness);
    auto call = light_state_->make_call();
    call.set_state(is_on);
    if (brightness != 0) {
      // Brightness equal to 0 has a special meaning.
      // D1 uses 0 as "previously set brightness".
      // Usually zero brightness comes inside light ON command triggered by RF remote.
      // Since we unconditionally override commands coming from RF remote in process_command_(),
      // here we mimic the original behavior but with LightCall functionality
      call.set_brightness((float) brightness / 100.0f);
    }
    call.perform();
  }
}

// Set the device's traits
light::LightTraits SonoffD1Output::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void SonoffD1Output::write_state(light::LightState *state) {
  bool binary;
  float brightness;

  // Fill our variables with the device's current state
  state->current_values_as_binary(&binary);
  state->current_values_as_brightness(&brightness);

  // Convert ESPHome's brightness (0-1) to the device's internal brightness (0-100)
  const uint8_t calculated_brightness = (uint8_t) roundf(brightness * 100);

  if (calculated_brightness == 0) {
    // if(binary) ESP_LOGD(TAG, "current_values_as_binary() returns true for zero brightness");
    binary = false;
  }

  // If a new value, write to the dimmer
  if (binary != this->last_binary_ || calculated_brightness != this->last_brightness_) {
    if (this->control_dimmer_(binary, calculated_brightness)) {
      this->last_brightness_ = calculated_brightness;
      this->last_binary_ = binary;
    } else {
      // Return to original value if failed to write to the dimmer
      // TODO: Test me, can be tested if high-voltage part is not connected
      ESP_LOGW(TAG, "Failed to update the dimmer, publishing the previous state");
      this->publish_state_(this->last_binary_, this->last_brightness_);
    }
  }
}

void SonoffD1Output::dump_config() {
  ESP_LOGCONFIG(TAG, "Sonoff D1 Dimmer: '%s'", this->light_state_ ? this->light_state_->get_name().c_str() : "");
  ESP_LOGCONFIG(TAG, "  Use RM433 Remote: %s", ONOFF(this->use_rm433_remote_));
  ESP_LOGCONFIG(TAG, "  Minimal brightness: %d", this->min_value_);
  ESP_LOGCONFIG(TAG, "  Maximal brightness: %d", this->max_value_);
}

void SonoffD1Output::loop() {
  // Read commands from the dimmer
  // RF chip notifies ESP about remotely changed state with the same commands as we send
  if (this->available()) {
    ESP_LOGV(TAG, "Have some UART data in loop()");
    uint8_t buffer[17] = {0};
    size_t len = sizeof(buffer);
    if (this->read_command_(buffer, len)) {
      this->process_command_(buffer, len);
    }
  }
}

}  // namespace sonoff_d1
}  // namespace esphome
