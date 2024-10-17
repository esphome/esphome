#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_epaper_4.26in";

WaveshareEPaper4P26In::WaveshareEPaper4P26In() : WaveshareEPaper() { reset_duration_ = 20; }

void WaveshareEPaper4P26In::init_display_async_(bool fast_update, const std::function<void()> &&f) {
  // Reset the display
  this->reset_();
  wait_until_idle_async_([this, fast_update, f] {
    this->command(0x12);  // SWRESET

    wait_until_idle_async_([this, fast_update, f] {
      // Use the internal temperature sensor
      this->command(0x18);
      this->data(0x80);

      // Set soft start
      this->command(0x0C);
      this->data(0xAE);
      this->data(0xC7);
      this->data(0xC3);
      this->data(0xC0);
      this->data(0x80);

      // Drive output control
      this->command(0x01);
      this->data((this->get_height_internal() - 1) % 256);  //  Y
      this->data((this->get_height_internal() - 1) / 256);  //  Y
      this->data(0x02);

      // // Border settings
      // this->command(0x3C);
      // this->data(0x01);

      // Data entry mode setting
      this->command(0x11);
      this->data(0x01);  // x increase, y decrease : as in demo code

      // Set window, this is always the full screen for now
      this->command(0x44);  // SET_RAM_X_ADDRESS_START_END_POSITION
      this->data(0x00);
      this->data(0x00);
      this->data((this->get_width_internal() - 1) & 0xFF);
      this->data(((this->get_width_internal() - 1) >> 8) & 0x03);

      this->command(0x45);  // SET_RAM_Y_ADDRESS_START_END_POSITION
      this->data((this->get_height_internal() - 1) & 0xFF);
      this->data(((this->get_height_internal() - 1) >> 8) & 0x03);
      this->data(0x00);
      this->data(0x00);

      // Set cursor
      this->command(0x4E);  // SET_RAM_X_ADDRESS_COUNTER
      this->data(0x00);
      this->data(0x00);

      this->command(0x4F);  // SET_RAM_Y_ADDRESS_COUNTER
      this->data(0x00);
      this->data(0x00);

      if (fast_update) {
        this->wait_until_idle_async_([this, f] {
          this->command(0x1A);
          this->data(0x5A);

          this->command(0x22);
          this->data(0x91);
          this->command(0x20);

          this->wait_until_idle_async_(std::move(f));
        });
      } else {
        this->wait_until_idle_async_(std::move(f));
      }
    });
  });
}

void WaveshareEPaper4P26In::initialize() {}

void WaveshareEPaper4P26In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.26in");
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void HOT WaveshareEPaper4P26In::display() {
  if (this->is_busy_) {
    ESP_LOGE(TAG, "Skipping display, display is busy");
    return;
  }

  this->is_busy_ = true;

  bool full_update = this->at_update_ == 0;

  if (this->full_update_every_ >= 1) {
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  }

  ESP_LOGI(TAG, "Wake up the display");
  this->init_display_async_(!full_update, [this, full_update] {
    // Border settings
    this->command(0x3C);
    // this->data(full_update ? 0x01 : 0x80);
    this->data(0x01);

    // Write RAM
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    if (full_update) {
      // Write base image again on full refresh
      this->command(0x26);
      this->start_data_();
      this->write_array(this->buffer_, this->get_buffer_length_());
      this->end_data_();
    }

    // Display Update Control
    this->command(0x22);
    this->data(full_update ? 0xF7 : 0xC7);  // 0xF7 = Full update, 0xC7 = Fast update, 0xFF = Partial update

    // Activate Display Update Sequence
    this->command(0x20);

    this->wait_until_idle_async_([this] {
      ESP_LOGI(TAG, "Display updated, set it back to deep sleep");
      this->deep_sleep();
      this->is_busy_ = false;
    });

    this->status_clear_warning();
  });
}

void WaveshareEPaper4P26In::deep_sleep() {
  // Set deep sleep mode
  this->command(0x10);
  this->data(0x01);
}

int WaveshareEPaper4P26In::get_width_internal() { return 800; }

int WaveshareEPaper4P26In::get_height_internal() { return 480; }

uint32_t WaveshareEPaper4P26In::idle_timeout_() { return 4000; }

void WaveshareEPaper4P26In::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

}  // namespace waveshare_epaper
}  // namespace esphome
