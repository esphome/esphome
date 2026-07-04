#include "epaper_spi_jd79660.h"
#include "colorconv.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.jd79660";

/** Pixel color as 2bpp. Must match IC LUT values. */
enum JD79660Color : uint8_t {
  BLACK = 0b00,
  WHITE = 0b01,
  YELLOW = 0b10,
  RED = 0b11,
};

/** Map RGB color to JD79660 BWYR hex color keys */
static JD79660Color HOT color_to_hex(Color color) {
  return color_to_bwyr(color, JD79660Color::BLACK, JD79660Color::WHITE, JD79660Color::YELLOW, JD79660Color::RED);
}

void EPaperJD79660::fill(Color color) {
  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  const auto pixel_color = color_to_hex(color);

  // We store 4 pixels per byte
  this->buffer_.fill(pixel_color | (pixel_color << 2) | (pixel_color << 4) | (pixel_color << 6));
}

void HOT EPaperJD79660::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const auto pixel_bits = color_to_hex(color);
  const uint32_t pixel_position = x + y * this->get_width_internal();
  // We store 4 pixels per byte at LSB offsets 6, 4, 2, 0
  const uint32_t byte_position = pixel_position / 4;
  const uint32_t bit_offset = 6 - ((pixel_position % 4) * 2);
  const auto original = this->buffer_[byte_position];

  this->buffer_[byte_position] = (original & (~(0b11 << bit_offset))) |  // mask old 2bpp
                                 (pixel_bits << bit_offset);             // add new 2bpp
}

bool EPaperJD79660::reset() {
  // On entry state RESET set step, next state will be RESET_END
  if (this->state_ == EPaperState::RESET) {
    this->step_ = FSMState::RESET_STEP0_H;
  }

  switch (this->step_) {
    case FSMState::RESET_STEP0_H:
      // Step #0: Reset H for some settle time.

      ESP_LOGVV(TAG, "reset #0");
      this->reset_pin_->digital_write(true);

      this->reset_duration_ = SLEEP_MS_RESET0;
      this->step_ = FSMState::RESET_STEP1_L;
      return false;  // another loop: step #1 below

    case FSMState::RESET_STEP1_L:
      // Step #1: Reset L pulse for slightly >1.5ms.
      // This is actual reset trigger.

      ESP_LOGVV(TAG, "reset #1");

      // As commented on SLEEP_MS_RESET1: Reset pulse must happen within time window.
      // So do not use FSM loop, and avoid other calls/logs during pulse below.
      this->reset_pin_->digital_write(false);
      delay(SLEEP_MS_RESET1);
      this->reset_pin_->digital_write(true);

      this->reset_duration_ = SLEEP_MS_RESET2;
      this->step_ = FSMState::RESET_STEP2_IDLECHECK;
      return false;  // another loop: step #2 below

    case FSMState::RESET_STEP2_IDLECHECK:
      // Step #2: Basically finished. Check sanity, and move FSM to INITIALISE state
      ESP_LOGVV(TAG, "reset #2");

      if (!this->is_idle_()) {
        // Expectation: Idle after reset + settle time.
        // Improperly connected/unexpected hardware?
        // Error path reproducable e.g. with disconnected VDD/... pins
        // (optimally while busy_pin configured with local pulldown).
        // -> Mark failed to avoid followup problems.
        this->mark_failed(LOG_STR("Busy after reset"));
      }
      break;  // End state loop below

    default:
      // Unexpected step = bug?
      this->mark_failed();
  }

  this->step_ = FSMState::INIT_STEP0_REGULARINIT;  // reset for initialize state
  return true;
}

bool EPaperJD79660::initialise(bool partial) {
  switch (this->step_) {
    case FSMState::INIT_STEP0_REGULARINIT:
      // Step #0: Regular init sequence
      ESP_LOGVV(TAG, "init #0");
      if (!EPaperBase::initialise(partial)) {  // Call parent impl
        return false;                          // If parent should request another loop, do so
      }

      // Fast init requested + supported?
      if (partial && (this->fast_update_length_ > 0)) {
        this->step_ = FSMState::INIT_STEP1_FASTINIT;
        this->wait_for_idle_(true);  // Must wait for idle before fastinit sequence in next loop
        return false;                // another loop: step #1 below
      }

      break;  // End state loop below

    case FSMState::INIT_STEP1_FASTINIT:
      // Step #1: Fast init sequence
      ESP_LOGVV(TAG, "init #1");
      this->write_fastinit_();
      break;  // End state loop below

    default:
      // Unexpected step = bug?
      this->mark_failed();
  }

  this->step_ = FSMState::NONE;
  return true;  // Finished: State transition waits for idle
}

bool EPaperJD79660::transfer_buffer_chunks_() {
  size_t buf_idx = 0;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  const uint32_t start_time = App.get_loop_component_start_time();
  const auto buffer_length = this->buffer_length_;
  while (this->current_data_index_ != buffer_length) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->disable();
      ESP_LOGVV(TAG, "Wrote %zu bytes at %ums", buf_idx, (unsigned) millis());
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
        return false;
      }
    }
  }

  // Finished the entire dataset
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->disable();
    ESP_LOGVV(TAG, "Wrote %zu bytes at %ums", buf_idx, (unsigned) millis());
  }
  // Cleanup for next transfer
  this->current_data_index_ = 0;

  // Finished with all buffer chunks
  return true;
}

void EPaperJD79660::write_fastinit_() {
  // Undocumented register sequence in vendor register range.
  // Related to Fast Init/Update.
  // Should likely happen after regular init seq and power on, but before refresh.
  // Might only work for some models with certain factory MTP.
  // Please do not change without knowledge to avoid breakage.

  this->send_init_sequence_(this->fast_update_, this->fast_update_length_);
}

bool EPaperJD79660::transfer_data() {
  // For now always send full frame buffer in chunks.
  // JD79660 might support partial window transfers. But sample code missing.
  // And likely minimal impact, solely on SPI transfer time into RAM.

  if (this->current_data_index_ == 0) {
    this->command(CMD_TRANSFER);
  }

  return this->transfer_buffer_chunks_();
}

void EPaperJD79660::refresh_screen([[maybe_unused]] bool partial) {
  ESP_LOGV(TAG, "Refresh");
  this->cmd_data(CMD_REFRESH, {(uint8_t) 0x00});
}

void EPaperJD79660::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->cmd_data(CMD_POWEROFF, {(uint8_t) 0x00});
}

void EPaperJD79660::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  // "Deepsleep between update": Ensure EPD sleep to avoid early hardware wearout!
  this->cmd_data(CMD_DEEPSLEEP, {(uint8_t) 0xA5});

  // Notes:
  // - VDD: Some boards (Waveshare) with "clever reset logic" would allow switching off
  //   EPD VDD by pulling reset pin low for longer time.
  //   However, a) not all boards have this, b) reliable sequence timing is difficult,
  //   c) saving is not worth it after deepsleep command above.
  //   If needed: Better option is to drive VDD via MOSFET with separate enable pin.
  //
  // - Possible safe shutdown:
  //   EPaperBase::on_safe_shutdown() may also trigger deep_sleep() again.
  //   Regularly, in IDLE state, this does not make sense for this "deepsleep between update" model,
  //   but SPI sequence should simply be ignored by sleeping receiver.
  //   But if triggering during lengthy update, this quick SPI sleep sequence may have benefit.
  //   Optimally, EPDs should even be set all white for longer storage.
  //   But full sequence (>15s) not possible w/o app logic.
}

}  // namespace esphome::epaper_spi
