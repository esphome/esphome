#include "epaper_spi_t133a01.h"

#include <algorithm>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.t133a01";
static constexpr uint8_t GRAY_THRESHOLD = 50;

// --- T133A01 controller register/command constants ---
static constexpr uint8_t R00_PSR = 0x00;
static constexpr uint8_t R01_PWR = 0x01;
static constexpr uint8_t R02_POF = 0x02;
static constexpr uint8_t R04_PON = 0x04;
static constexpr uint8_t R05_BTST_N = 0x05;
static constexpr uint8_t R06_BTST_P = 0x06;
static constexpr uint8_t R10_DTM = 0x10;
static constexpr uint8_t R12_DRF = 0x12;
static constexpr uint8_t R50_CDI = 0x50;
static constexpr uint8_t R61_TRES = 0x61;
static constexpr uint8_t RE0_CCSET = 0xE0;
static constexpr uint8_t RE3_PWS = 0xE3;
static constexpr uint8_t SLEEP_V[] = {0xA5};

static constexpr uint8_t POF_V[] = {0x00};
static constexpr uint8_t DRF_V[] = {0x01};
static constexpr uint8_t CCSET_V_CUR[] = {0x01};

static constexpr uint32_t PON_BUSY_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t POF_BUSY_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t DRF_BUSY_TIMEOUT_MS = 180 * 1000;

// Default palette indices used by the manufacturer library in 6-color mode
static constexpr uint8_t TFT_WHITE = 0x0;
static constexpr uint8_t TFT_GREEN = 0x2;
static constexpr uint8_t TFT_RED = 0x6;
static constexpr uint8_t TFT_YELLOW = 0xB;
static constexpr uint8_t TFT_BLUE = 0xD;
static constexpr uint8_t TFT_BLACK = 0xF;

static uint8_t color_to_palette(Color color) {
  unsigned char max_rgb = std::max({color.r, color.g, color.b});
  unsigned char min_rgb = std::min({color.r, color.g, color.b});

  if ((max_rgb - min_rgb) < GRAY_THRESHOLD) {
    if ((static_cast<int>(color.r) + color.g + color.b) > 382) {
      return TFT_WHITE;
    }
    return TFT_BLACK;
  }

  bool r_on = (color.r > 128);
  bool g_on = (color.g > 128);
  bool b_on = (color.b > 128);

  if (r_on && g_on && !b_on) {
    return TFT_YELLOW;
  }
  if (r_on && !g_on && !b_on) {
    return TFT_RED;
  }
  if (!r_on && g_on && !b_on) {
    return TFT_GREEN;
  }
  if (!r_on && !g_on && b_on) {
    return TFT_BLUE;
  }
  if (!r_on && g_on && b_on) {
    // Cyan -> closest is Green
    return TFT_GREEN;
  }
  if (r_on && !g_on) {
    // Magenta -> closest is Red
    return TFT_RED;
  }
  if (r_on) {
    return TFT_WHITE;
  }
  return TFT_BLACK;
}

static constexpr uint8_t color_get(uint8_t nibble) {
  // Mapping from manufacturer library (T133A01_Defines.h)
  // nibble is one pixel in the Seeed palette (0..15)
  switch (nibble & 0x0F) {
    case 0x0F:
      return 0x00;  // black
    case 0x00:
      return 0x01;  // white
    case 0x02:
      return 0x06;  // green
    case 0x0B:
      return 0x02;  // yellow
    case 0x0D:
      return 0x05;  // blue
    case 0x06:
      return 0x03;  // red
    default:
      return 0x01;  // map unknown to white
  }
}

void EPaperT133A01::setup() {
  EPaperBase::setup();
  if (this->is_failed())
    return;

  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
  }

  if (this->cs1_pin_ == nullptr) {
    this->mark_failed(LOG_STR("'cs1_pin' is required for T133A01"));
    return;
  }

  // Ensure CS1 is inactive before registering the second SPI device
  this->cs1_pin_->setup();
  this->cs1_pin_->digital_write(true);

  this->cs1_device_.set_spi_parent(this->parent_);
  this->cs1_device_.set_cs_pin(this->cs1_pin_);
  this->cs1_device_.set_data_rate(this->data_rate_);
  this->cs1_device_.set_bit_order(this->bit_order_);
  this->cs1_device_.set_mode(this->mode_);
  this->cs1_device_.set_write_only(true);
  this->cs1_device_.spi_setup();
}

void EPaperT133A01::cs1_command_(uint8_t value) {
  ESP_LOGV(TAG, "CS1 Command: 0x%02X", value);
  this->dc_pin_->digital_write(false);
  this->cs1_device_.enable();
  this->cs1_device_.write_byte(value);
  this->cs1_device_.disable();
}

void EPaperT133A01::cs1_cmd_data_(uint8_t command, const uint8_t *data, size_t length) {
  ESP_LOGV(TAG, "CS1 Cmd: 0x%02X, len=%u", command, (unsigned) length);
  this->dc_pin_->digital_write(false);
  this->cs1_device_.enable();
  this->cs1_device_.write_byte(command);
  if (length > 0) {
    this->dc_pin_->digital_write(true);
    this->cs1_device_.write_array(data, length);
  }
  this->cs1_device_.disable();
}

void EPaperT133A01::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  CS1 Pin: ", this->cs1_pin_);
  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
}

bool EPaperT133A01::reset() {
  if (this->reset_pin_ != nullptr) {
    if (this->state_ == EPaperState::RESET) {
      this->reset_pin_->digital_write(false);
      return false;
    }
    this->reset_pin_->digital_write(true);
    // Manufacturer code waits 20ms after releasing reset
    this->next_delay_ = 20;
  }
  return true;
}

bool EPaperT133A01::initialise(bool partial) {
  (void) partial;

  this->transfer_prologue_phase_ = 0;

  this->send_init_sequence_dual_(this->init_sequence_, this->init_sequence_length_);
  return true;
}

void EPaperT133A01::send_init_sequence_dual_(const uint8_t *sequence, size_t length) {
  // The T133A01 panel uses two controller halves (CS + CS1). The vendor init sequence
  // issues some commands on CS1 and later commands on CS. In practice we mirror the
  // early setup commands to both controllers and keep the remainder on CS only.
  //
  // The init sequence is provided from Python (flattened like other epaper_spi models).
  if (sequence == nullptr || length == 0) {
    this->mark_failed(LOG_STR("Missing init sequence"));
    return;
  }

  auto mirror_to_cs1 = [](uint8_t cmd) -> bool {
    switch (cmd) {
      case 0xF0:
      case R00_PSR:
      case R50_CDI:
      case 0x60:
      case 0x86:
      case RE3_PWS:
      case R61_TRES:
        return true;
      default:
        return false;
    }
  };

  size_t index = 0;
  while (index != length) {
    if (length - index < 2) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    const uint8_t cmd = sequence[index++];
    const uint8_t len_or_flag = sequence[index++];

    const uint8_t num_args = len_or_flag & 0x7F;
    if (length - index < num_args) {
      ESP_LOGE(TAG, "Malformed init sequence, cmd = %X, num_args = %u", cmd, num_args);
      this->mark_failed();
      return;
    }

    this->cmd_data(cmd, sequence + index, num_args);
    if (mirror_to_cs1(cmd)) {
      this->cs1_cmd_data_(cmd, sequence + index, num_args);
    }
    index += num_args;
  }
}

void EPaperT133A01::wait_for_idle_with_timeout_(uint32_t timeout_ms, const char *label) const {
  if (this->busy_pin_ == nullptr)
    return;

  const uint32_t start = millis();
  uint32_t last_log = start;

  while (!this->is_idle_()) {
    const uint32_t now = millis();
    const uint32_t elapsed = now - start;
    if (elapsed >= timeout_ms) {
      ESP_LOGW(TAG, "BUSY timeout waiting for %s (%u ms), continuing", label, (unsigned) elapsed);
      return;
    }
    App.feed_wdt(now);
    if (now - last_log >= 1000) {
      last_log = now;
      ESP_LOGV(TAG, "BUSY waiting (%s): %u ms (pin=%d)", label, (unsigned) elapsed,
               this->busy_pin_ != nullptr ? (int) this->busy_pin_->digital_read() : -1);
    }
    delay(10);
  }

  ESP_LOGV(TAG, "BUSY cleared (%s) after %u ms (pin=%d)", label, (unsigned) (millis() - start),
           this->busy_pin_ != nullptr ? (int) this->busy_pin_->digital_read() : -1);
}

void EPaperT133A01::power_on() {
  ESP_LOGV(TAG, "Power on");
  if (this->busy_pin_ != nullptr) {
    ESP_LOGV(TAG, "BUSY before PON: %d", (int) this->busy_pin_->digital_read());
  }

  this->cs1_pin_->digital_write(false);
  this->command(R04_PON);
  this->wait_for_idle_with_timeout_(PON_BUSY_TIMEOUT_MS, "PON");
  this->cs1_pin_->digital_write(true);
}

void EPaperT133A01::refresh_screen(bool partial) {
  (void) partial;
  ESP_LOGV(TAG, "Refresh");
  if (this->busy_pin_ != nullptr) {
    ESP_LOGV(TAG, "BUSY before DRF: %d", (int) this->busy_pin_->digital_read());
  }

  this->cs1_pin_->digital_write(false);
  this->cmd_data(R12_DRF, DRF_V, sizeof(DRF_V));
  this->wait_for_idle_with_timeout_(DRF_BUSY_TIMEOUT_MS, "DRF");
  this->cs1_pin_->digital_write(true);
}

void EPaperT133A01::power_off() {
  ESP_LOGV(TAG, "Power off");
  if (this->busy_pin_ != nullptr) {
    ESP_LOGV(TAG, "BUSY before POF: %d", (int) this->busy_pin_->digital_read());
  }

  this->cs1_pin_->digital_write(false);
  this->cmd_data(R02_POF, POF_V, sizeof(POF_V));
  this->wait_for_idle_with_timeout_(POF_BUSY_TIMEOUT_MS, "POF");
  this->cs1_pin_->digital_write(true);
}

void EPaperT133A01::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, SLEEP_V, sizeof(SLEEP_V));
  this->cs1_cmd_data_(0x07, SLEEP_V, sizeof(SLEEP_V));
}

void EPaperT133A01::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  const uint8_t pixel = color_to_palette(color) & 0x0F;
  this->buffer_.fill(pixel | (pixel << 4));
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

void EPaperT133A01::clear() { this->fill(COLOR_ON); }

void HOT EPaperT133A01::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  uint8_t pixel_bits = color_to_palette(color) & 0x0F;
  uint32_t pixel_position = x + y * this->get_width_internal();
  uint32_t byte_position = pixel_position / 2;
  uint8_t original = this->buffer_[byte_position];
  if ((pixel_position & 1U) != 0U) {
    this->buffer_[byte_position] = (original & 0xF0) | pixel_bits;
  } else {
    this->buffer_[byte_position] = (original & 0x0F) | (pixel_bits << 4);
  }
  App.feed_wdt();
}

bool HOT EPaperT133A01::transfer_data() {
  const uint16_t width = this->get_width_internal();
  const uint16_t height = this->get_height_internal();

  // The manufacturer driver pushes the screen in two halves (two chip selects).
  // Each row is (width / 2) bytes in our 4bpp buffer. Each controller consumes half that.
  const uint16_t bytes_per_block_row = width / 4;
  const uint16_t stride = bytes_per_block_row * 2;

  const size_t half_frame_len = static_cast<size_t>(height) * bytes_per_block_row;

  if (!this->transfer_prologue_done_) {
    // Transfer prologue (equivalent to EPD_PUSH_NEW_COLORS preamble) without blocking.
    // Vendor does: CCSET -> CHECK_BUSY -> delay(10)
    if (this->transfer_prologue_phase_ == 0) {
      // Send CCSET to both controllers before streaming pixels.
      this->cmd_data(RE0_CCSET, CCSET_V_CUR, sizeof(CCSET_V_CUR));
      this->cs1_cmd_data_(RE0_CCSET, CCSET_V_CUR, sizeof(CCSET_V_CUR));
      this->transfer_prologue_phase_ = 1;
      return false;  // EPaperBase will wait for idle before calling again
    }
    if (this->transfer_prologue_phase_ == 1) {
      this->delay_until_ = millis() + 10;
      this->transfer_prologue_phase_ = 2;
      return false;
    }

    this->transfer_index_ = 0;
    this->transfer_on_cs1_ = false;
    this->transfer_dtm_sent_ = false;
    this->transfer_streaming_ = false;
    this->transfer_prologue_done_ = true;
    this->transfer_prologue_phase_ = 0;
  }

  // Manufacturer implementation (EPD_PUSH_NEW_COLORS) streams the entire first half (CS),
  // then the entire second half (CS1). It sends DTM (0x10) once per half and relies on
  // the controller auto-incrementing the write address.
  // Interleaving per-row (or re-sending DTM repeatedly) can result in a blank screen.

  // Progress logging: keep it low-noise (every N rows).
  static constexpr uint16_t ROW_LOG_STEP = 100;
  uint16_t last_logged_row = 0xFFFF;
  bool last_logged_cs1 = false;

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  while (true) {
    // Feed the watchdog while streaming to avoid WDT resets on long transfers.
    App.feed_wdt(millis());
    // Important: the manufacturer driver keeps CS asserted for the entire (half) frame transfer.
    // Toggling CS between the 0x10 (DTM) command and subsequent data can result in the controller
    // ignoring the data stream and leaving the screen blank.
    if (!this->transfer_dtm_sent_) {
      this->dc_pin_->digital_write(false);
      if (!this->transfer_on_cs1_) {
        this->enable();
        this->write_byte(R10_DTM);
      } else {
        this->cs1_device_.enable();
        this->cs1_device_.write_byte(R10_DTM);
      }
      this->dc_pin_->digital_write(true);
      this->transfer_dtm_sent_ = true;
      this->transfer_streaming_ = true;
    }

    size_t out_idx = 0;

    while (this->transfer_index_ < half_frame_len && out_idx < sizeof(bytes_to_send)) {
      const size_t pos = this->transfer_index_++;
      const uint16_t row = pos / bytes_per_block_row;
      const uint16_t col = pos % bytes_per_block_row;

      if (row != last_logged_row && (row == 0 || (row % ROW_LOG_STEP) == 0)) {
        // Note: height is the number of rows.
        ESP_LOGV(TAG, "Updating row %u/%u (%s)", (unsigned) row, (unsigned) height,
                 this->transfer_on_cs1_ ? "CS1" : "CS");
        last_logged_row = row;
        last_logged_cs1 = this->transfer_on_cs1_;
      } else if (this->transfer_on_cs1_ != last_logged_cs1) {
        // When switching halves, make sure we log at least once.
        ESP_LOGV(TAG, "Updating row %u/%u (%s)", (unsigned) row, (unsigned) height,
                 this->transfer_on_cs1_ ? "CS1" : "CS");
        last_logged_row = row;
        last_logged_cs1 = this->transfer_on_cs1_;
      }

      const size_t base = (static_cast<size_t>(row) * stride) + (this->transfer_on_cs1_ ? bytes_per_block_row : 0);
      const uint8_t b = this->buffer_[base + col];
      const uint8_t hi = (b >> 4) & 0x0F;
      const uint8_t lo = b & 0x0F;
      bytes_to_send[out_idx++] = static_cast<uint8_t>((color_get(hi) << 4) | color_get(lo));
    }

    if (out_idx > 0) {
      if (!this->transfer_on_cs1_) {
        this->write_array(bytes_to_send, out_idx);
      } else {
        this->cs1_device_.write_array(bytes_to_send, out_idx);
      }
      // Feed the watchdog periodically during long frame transfers to avoid WDT resets.
      App.feed_wdt(millis());
    }

    if (this->transfer_index_ >= half_frame_len) {
      if (!this->transfer_on_cs1_) {
        // Finished first half (CS). Release CS before switching to CS1.
        if (this->transfer_streaming_) {
          this->disable();
        }
        // Switch to the second half (CS1)
        this->transfer_on_cs1_ = true;
        this->transfer_index_ = 0;
        this->transfer_dtm_sent_ = false;
        this->transfer_streaming_ = false;
        continue;  // Continue with CS1 half in this call
      }
      // Finished second half (CS1). Release CS1.
      if (this->transfer_streaming_) {
        this->cs1_device_.disable();
      }
      break;  // Finished CS1 half too
    }
  }

  // Done
  this->transfer_index_ = 0;
  this->transfer_on_cs1_ = false;
  this->transfer_dtm_sent_ = false;
  this->transfer_prologue_done_ = false;
  this->transfer_streaming_ = false;
  return true;
}

}  // namespace esphome::epaper_spi
