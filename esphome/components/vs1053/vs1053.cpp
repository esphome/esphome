#include "vs1053.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "vs1053_reg.h"

namespace esphome {
namespace vs1053 {

static const char* TAG = "VS1053";

void VS1053Component::setup() {
  // Setup pins
  if (this->reset_pin_)
    this->reset_pin_->setup();
  this->dreq_pin_->setup();

  // Initialize SPI devices
  this->sci_spi_->spi_setup();
  this->sdi_spi_->spi_setup();

  // Reset and configure device
  if (!this->init_()) {
    this->mark_failed();
    return;
  }

  // Init appears to be OK
  // Check status register for device version
  uint16_t status = this->command_read_(SCI_REG_STATUS);
  uint8_t version = (status >> 4) & 0x0F;

  ESP_LOGD(TAG, "Status 0x%04X, Version: %d", status, version);

  // Validate device version
  if (version != VS1053_VERSION) {
    ESP_LOGE(TAG, "Initialization failed. SS_VER %d != %d", version, VS1053_VERSION);
    this->mark_failed();
    return;
  }
}

void VS1053Component::loop() {
  // Start playback
  // Send initial data
  // Get fill byte
  //
  // STOP
  // Send until end of file
  // Send fill data
  // Set cancel bit
  // Send fill data until cancel cleared -> timeout to fault
  //
  // CANCEL
  // Set cancel bit
  // Keep sending file data until cancel cleared -> timeout to fault
  // Send fill data

  // Device encountered an error. Attempt recovery with a soft reset
  if (this->state_ == PlaybackState::Error) {
    ESP_LOGI(TAG, "Playback error. Attempting soft reset.");
    if (!this->init_(true)) {
      this->mark_failed();
      return;
    }

    // Device successfully reset
    this->state_ = PlaybackState::Idle;
    return;
  }

  // Nothing to do
  if (this->state_ == PlaybackState::Idle)
    return;

  // Not ready for data
  if (!this->data_ready_())
    return;

  // Get timestamp
  uint32_t start = micros();

  // Send fill data
  if (this->fill_remaining_) {
    ESP_LOGD(TAG, "%d bytes of fill data remaining.", this->fill_remaining_);

    // Send as much fill data as possible
    while (this->data_ready_() && this->fill_remaining_ > 0) {
      size_t write_length = std::min(this->fill_remaining_, VS1053_TRANSFER_SIZE);
      this->data_write_(this->fill_buffer_, write_length);

      this->fill_remaining_ -= write_length;

      // Don't tie up the system for too long
      if ((micros() - start) > VS1053_LOOP_TIMEOUT_US)
        break;
    }

    // Transition to idle if playback was cancelled and all fill is sent
    if (this->state_ == PlaybackState::Cancelled && this->fill_remaining_ == 0) {
      this->state_ = PlaybackState::Idle;
    }

    return;
  }

  // Handle end of playback
  if (this->state_ == PlaybackState::Stop)
    return this->finish_playback_();

  // Write as much file data as possible
  size_t remaining = this->buffer_end_ - this->buffer_;
  ESP_LOGD(TAG, "%d bytes of file data remaining.", remaining);

  while (this->data_ready_() && remaining > 0) {
    size_t write_length = std::min(remaining, VS1053_TRANSFER_SIZE);
    this->data_write_(this->buffer_, write_length);

    this->buffer_ += write_length;
    remaining -= write_length;

    // Sample current time
    uint32_t now = micros();

    // Don't tie up the system for too long
    if ((now - start) > VS1053_LOOP_TIMEOUT_US)
      break;

    // Handle playback cancellation
    if (this->state_ == PlaybackState::Cancel) {
      if (!this->get_cancel_bit_()) {
        // Start fill if cancel bit is cleared
        ESP_LOGI(TAG, "Playback cancelled. Sending fill data.");
        this->fill_remaining_ = VS1053_FILL_LENGTH;
        this->state_ = PlaybackState::Cancelled;
      } else if ((now - this->cancel_start_) > VS1053_CANCEL_TIMEOUT_US) {
        // Device has failed to cancel after 1 second
        ESP_LOGE(TAG, "Playback cancellation failed.");
        this->state_ = PlaybackState::Error;
      }

      return;
    }
  }

  // End of file, stop playback
  if (this->buffer_ >= this->buffer_end_) {
    ESP_LOGI(TAG, "Playback complete. Sending fill data.");
    this->fill_remaining_ = VS1053_FILL_LENGTH;
    this->state_ = PlaybackState::Stop;
  }
}

void VS1053Component::dump_config() {
  ESP_LOGCONFIG(TAG, "VS1053:");
  LOG_PIN("  RESET Pin: ", this->reset_pin_);
  LOG_PIN("  DREQ Pin: ", this->dreq_pin_);
  // TODO LOG SPI devices?
  // ESP_LOGCONFIG(TAG, "  SCI SPI ", this->address_);
}

void VS1053Component::set_volume(uint8_t left, uint8_t right) {
  ESP_LOGD(TAG, "Set volume L %d, R %d.", left, right);

  // Save volume
  this->volume_left_ = left;
  this->volume_right_ = right;

  // Convert incoming volume to attenuation
  auto convert = [](uint8_t v) {
    return 0xFF - v;
  };

  // Register sets attenuation in 0.5 dB steps
  // Maximum volume 0x0000, minimum 0xFEFE, analog powerdown 0xFFFF
  uint16_t vol = convert(left) << 8 | convert(right);
  this->command_write_(SCI_REG_VOLUME, vol);
}

void VS1053Component::cancel_playback() {
  // Can't cancel playback if we're not playing
  if (this->state_ != PlaybackState::Playing)
    return;

  ESP_LOGI(TAG, "Cancelling playback.");

  // Set cancel bit
  this->set_cancel_bit_();

  // Transition to cancel state and save timestamp
  this->state_ = PlaybackState::Cancel;
  this->cancel_start_ = micros();
}

void VS1053Component::play_file(const uint8_t* data, size_t length) {
  // Ensure device is idle
  if (this->state_ != PlaybackState::Idle) {
    ESP_LOGW(TAG, "State not idle. Current state %d.", this->state_);
    return;
  }

  // Wait for data ready
  ESP_LOGI(TAG, "Waiting for data ready.");
  if (!this->wait_data_ready_(1000)) {
    ESP_LOGE(TAG, "Playback failed. Not ready for data.");
    return;
  }

  ESP_LOGI(TAG, "Starting playback.");
  // Save buffer
  this->buffer_ = data;
  this->buffer_end_ = data + length;

  // Update state
  this->state_ = PlaybackState::Playing;

  // Attempt to fill entire FIFO
  size_t remaining = std::min(length, VS1053_FIFO_LENGTH);
  while (this->data_ready_() && remaining > 0) {
    size_t write_length = std::min(remaining, VS1053_TRANSFER_SIZE);
    this->data_write_(this->buffer_, write_length);

    this->buffer_ += write_length;
    remaining -= write_length;
  }

  // File should be playing now, get fill byte and populate fill buffer
  uint8_t fill_byte = this->get_fill_byte_();
  ESP_LOGD(TAG, "End of stream fill byte 0x%02X", fill_byte);

  memset(this->fill_buffer_, fill_byte, sizeof(this->fill_buffer_));
  this->fill_remaining_ = 0;
}

void VS1053Component::play_test_sine(uint16_t ms, uint32_t freq_hz, uint32_t sample_rate_hz) {
  // Perform the "new" sine test via SCI

  // Re-init device
  this->init_();  // TODO check failure? // TODO soft reset instead?

  // New sine test
  // AICTRLn = Fsin X 65536 / Fs
  // AICTRLn max value 0x8000
  // AICTRLn 3 LSB should be zero for best SNR

  // Set sample rate TODO??
  this->command_write_(SCI_REG_AUDATA, sample_rate_hz & 0xFFFE);

  // Configure sine frequency
  uint16_t aictrl = std::min((freq_hz * 65536) / sample_rate_hz, 0x8000ul) & ~0x0007;
  this->command_write_(SCI_REG_AICTRL0, aictrl);  // Left channel
  this->command_write_(SCI_REG_AICTRL1, aictrl);  // Right channel

  this->command_write_(SCI_REG_AIADDR, 0x4020);  // Start test

  // Stop test after duration
  this->set_timeout(ms, [this]() {
    // this->command_write_(SCI_REG_AIADDR, 0);  // TODO Stop tests? Nothing is docs
    this->init_(true);
  });
}

void VS1053Component::play_test_sine_sdi(uint16_t ms) {
  // Perform the "old" sine test via SDI

  // Re-init device
  this->init_();  // TODO check failure?

  // Enable test modes
  uint16_t mode = this->command_read_(SCI_REG_MODE);
  mode |= MODE_SM_TESTS;
  this->command_write_(SCI_REG_MODE, mode);

  // Wait for data ready
  this->wait_data_ready_(1000);

  // Start test with fixed frequency
  const uint8_t sine_start[16] = {
      0x53, 0xEF, 0x6E, 0x44,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  this->data_write_(sine_start, sizeof(sine_start));

  // Stop test after duration
  this->set_timeout(ms, [this]() {
    uint8_t sine_stop[16] = {
        0x45, 0x78, 0x69, 0x74,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};
    this->data_write_(sine_stop, sizeof(sine_stop));
  });
}

void VS1053Component::finish_playback_() {
  ESP_LOGD(TAG, "Finishing playback.");

  // Set cancel bit
  this->set_cancel_bit_();

  // Continue writing fill until cancel bit clears
  size_t fill_sent = 0;
  while (this->get_cancel_bit_()) {
    this->data_write_(this->fill_buffer_, VS1053_TRANSFER_SIZE);

    fill_sent += VS1053_TRANSFER_SIZE;

    // Cancel bit won't clear, device is in error
    if (fill_sent >= VS1053_STOP_FILL_LENGTH) {
      ESP_LOGE(TAG, "Playback stop failed.");
      this->state_ = PlaybackState::Error;
      return;
    }
  }

  // Playback complete transition to idle
  this->state_ = PlaybackState::Idle;
}

bool VS1053Component::init_(bool soft_reset) {
  ESP_LOGI(TAG, "Starting init.");

  // Soft reset if requested or reset pin is not available
  if (soft_reset || !this->reset_pin_) {
    ESP_LOGI(TAG, "Soft reset");

    // Assert soft reset
    this->command_write_(SCI_REG_MODE, MODE_SM_SDINEW | MODE_SM_RESET);

    // Datasheet says to wait 2 us, then check DREQ
    delayMicroseconds(10);
  } else {
    // Hard reset device via pin
    ESP_LOGI(TAG, "Hard reset");

    this->reset_pin_->digital_write(false);
    delayMicroseconds(100);
    this->reset_pin_->digital_write(true);
  }

  // Datasheet says DREQ will assert in 1.8 ms @ 12.288 MHz
  if (!(this->wait_data_ready_(4000))) {
    ESP_LOGE(TAG, "Initialization failed. DREQ not asserted.");
    return false;
  }

  // Configure clocks
  // CLKI = 3x XTALI, XTALI = 12.288 MHz
  // TOOD data sheet recommends 3.5x 0x9800
  ESP_LOGD(TAG, "Configuring clocks.");
  this->command_write_(SCI_REG_CLOCKF, 0x6000);

  // Set volume
  this->set_volume(this->volume_left_, this->volume_right_);

  return true;
}

bool VS1053Component::get_cancel_bit_() const {
  return this->command_read_(SCI_REG_MODE) & MODE_SM_CANCEL;
}

void VS1053Component::set_cancel_bit_() const {
  // Set cancel bit
  uint16_t mode = this->command_read_(SCI_REG_MODE);
  this->command_write_(SCI_REG_MODE, mode | MODE_SM_CANCEL);
}

uint8_t VS1053Component::get_fill_byte_() const {
  return get_parameter_(PARAMETER_END_FILL_BYTE_ADDR) & 0xFF;
}

uint16_t VS1053Component::get_parameter_(uint16_t addr) const {
  // Read parameter from RAM
  this->command_write_(SCI_REG_WRAMADDR, addr);
  return this->command_read_(SCI_REG_WRAM);
}

bool VS1053Component::wait_data_ready_(uint32_t timeout_us) const {
  ESP_LOGD(TAG, "Waiting %d us for DREQ...", timeout_us);

  uint32_t start = micros();

  // Wait for DREQ to assert
  while (!this->data_ready_()) {
    if ((micros() - start) > timeout_us)
      return false;
  }

  return true;
}

void VS1053Component::data_write_(const uint8_t* buffer, size_t length) const {
  // ESP_LOGV(TAG, "SDI write %d bytes: %s", length, format_hex_pretty(buffer, length).c_str());

  this->sdi_spi_->enable();
  this->sdi_spi_->write_array(buffer, length);
  this->sdi_spi_->disable();
}

uint16_t VS1053Component::command_transfer_(uint8_t instruction, uint8_t addr, uint16_t data) const {
  ESP_LOGV(TAG, "SCI transfer %02X %02X %04X", instruction, addr, data);

  // Assert XCS
  this->sci_spi_->enable();

  uint8_t buffer[4] = {
      instruction,
      addr,
      static_cast<uint8_t>(data >> 8),
      static_cast<uint8_t>(data & 0xFF),
  };
  this->sci_spi_->transfer_array(buffer, sizeof(buffer));

  // Deassert XCS
  this->sci_spi_->disable();

  ESP_LOGV(TAG, "SCI transfer result %02X %02X", buffer[2], buffer[3]);

  return buffer[2] << 8 | buffer[3];
}

}  // namespace vs1053
}  // namespace esphome
