#include "epaper_spi_ssd1677.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.ssd1677";

bool EPaperSSD1677::reset() {
  if (this->reset_pin_ == nullptr) {
    return true;
  }

  if (this->state_ == EPaperState::RESET) {
    this->step_ = FSMState::RESET_STEP0_H;
  }

  switch (this->step_) {
    case FSMState::RESET_STEP0_H:
      ESP_LOGVV(TAG, "reset step 0");
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = RESET_HIGH_MS_0;
      this->step_ = FSMState::RESET_STEP1_L;
      return false;

    case FSMState::RESET_STEP1_L:
      ESP_LOGVV(TAG, "reset step 1");
      this->reset_pin_->digital_write(false);
      delay(RESET_LOW_MS);
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = RESET_HIGH_MS_1;
      this->step_ = FSMState::RESET_STEP2_IDLECHECK;
      return false;

    case FSMState::RESET_STEP2_IDLECHECK:
      ESP_LOGVV(TAG, "reset step 2");
      if (!this->is_idle_()) {
        this->mark_failed(LOG_STR("Busy after reset"));
      }
      this->step_ = FSMState::INIT_STEP0_SWRESET;
      return true;

    default:
      this->mark_failed();
      return true;
  }
}

bool EPaperSSD1677::initialise(bool partial) {
  (void) partial;

  switch (this->step_) {
    case FSMState::INIT_STEP0_SWRESET:
      ESP_LOGVV(TAG, "init step 0: swreset");
      this->command(0x12);  // SWRESET
      this->step_ = FSMState::INIT_STEP1_REGULARINIT;
      this->wait_for_idle_(true);
      return false;

    case FSMState::INIT_STEP1_REGULARINIT:
      ESP_LOGVV(TAG, "init step 1: regular init sequence");
      if (!EPaperBase::initialise(false)) {
        return false;
      }
      this->step_ = FSMState::NONE;
      return true;

    default:
      this->mark_failed();
      return true;
  }
}

bool EPaperSSD1677::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  size_t buf_idx = 0;

  // Demo's stable full/base path writes both 0x24 and 0x26 with the same 1bpp image.
  // Phase 1: write BW RAM (0x24)
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "transfer: write RAM 0x24");
    this->command(0x24);
  }

  if (this->current_data_index_ < buffer_length) {
    this->start_data_();
    while (this->current_data_index_ < buffer_length) {
      bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

      if (buf_idx == sizeof(bytes_to_send)) {
        this->write_array(bytes_to_send, buf_idx);
        buf_idx = 0;

        if (millis() - start_time > MAX_TRANSFER_TIME) {
          this->disable();
          return false;
        }
      }
    }
    if (buf_idx != 0) {
      this->write_array(bytes_to_send, buf_idx);
      buf_idx = 0;
    }
    this->disable();
  }

  // Phase 2: write second RAM (0x26) with same data
  if (this->current_data_index_ == buffer_length) {
    ESP_LOGV(TAG, "transfer: write RAM 0x26");
    this->command(0x26);
  }

  if (this->current_data_index_ < (buffer_length * 2)) {
    this->start_data_();
    while (this->current_data_index_ < (buffer_length * 2)) {
      const size_t src_index = this->current_data_index_ - buffer_length;
      bytes_to_send[buf_idx++] = this->buffer_[src_index];
      this->current_data_index_++;

      if (buf_idx == sizeof(bytes_to_send)) {
        this->write_array(bytes_to_send, buf_idx);
        buf_idx = 0;

        if (millis() - start_time > MAX_TRANSFER_TIME) {
          this->disable();
          return false;
        }
      }
    }
    if (buf_idx != 0) {
      this->write_array(bytes_to_send, buf_idx);
    }
    this->disable();
  }

  this->current_data_index_ = 0;
  return true;
}

void EPaperSSD1677::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "refresh");
  this->cmd_data(0x22, {partial ? (uint8_t) 0xFF : (uint8_t) 0xF7});
  this->command(0x20);
}

void EPaperSSD1677::deep_sleep() {
  ESP_LOGV(TAG, "deep sleep");
  this->cmd_data(0x10, {0x01});
}

}  // namespace esphome::epaper_spi
