#include "i2s_clockless_led_strip.h"

#if defined(USE_ESP32) && SOC_I2S_SUPPORTS_TDM

#include <algorithm>
#include <cinttypes>

#include "esphome/core/helpers.h"

#include <driver/i2s_tdm.h>
#include <esp_attr.h>

namespace esphome::i2s_clockless_led_strip {

constexpr const char *const TAG = "i2s_clockless_led_strip";

constexpr const char *const ERROR_ALLOCATION = "Allocation error";
constexpr const char *const ERROR_I2S = "I2S error";

// These values are hardcoded for an LED strip at 800 kbps.
//
// Each bit of LED strip data gets expanded into 3 bits of I2C data, where a 0-bit expands into 100
// (1/3 duty cycle) and a 1-bit expands into 110 (2/3 duty cycle.  Each byte of LED strip data becomes
// one 24-bit I2S sample to be transmitted at a rate of 100000 samples per second in I2C TDM mode.
constexpr uint32_t I2S_SAMPLE_RATE_HZ = 100000;
constexpr size_t I2S_BYTES_PER_SAMPLE = 3;

// Determine how many I2S samples are needed to feed the output for the specified duration and vice-versa.
constexpr size_t num_i2s_samples_from_duration_us(uint32_t duration_us) {
  return (duration_us * I2S_SAMPLE_RATE_HZ + 999999) / 1000000;
}
constexpr uint32_t num_i2s_samples_to_duration_us(size_t samples) { return samples * 1000000ull / I2S_SAMPLE_RATE_HZ; }

// Determine the number of I2S samples to pad with zeros after the LED data to encode the LED strip reset signal.
constexpr uint32_t I2S_RESET_DURATION_US = 50;
constexpr size_t I2S_RESET_SAMPLES = num_i2s_samples_from_duration_us(I2S_RESET_DURATION_US);
constexpr size_t I2S_RESET_BYTES = I2S_RESET_SAMPLES * I2S_BYTES_PER_SAMPLE;

// Determine the number of I2S samples per DMA buffer to ensure that the interrupt-driven I2S on_sent callback
// doesn't run too frequently.  For example, if the buffers were sized to hold just 1 RGBW pixel then they would be
// recycled approximately every 40 us (25000 Hz) which is way too fast.  The buffer duration also imposes an upper
// bound on the maximum LED strip refresh rate.  Round up to a multiple of 3 for `dma_frame_num` as required by
// the ESP-IDF programming guide when using 24-bit samples.
constexpr uint32_t I2S_BUFFER_DURATION_US_IDEAL = 2500;
constexpr size_t I2S_BUFFER_SAMPLES = (num_i2s_samples_from_duration_us(I2S_BUFFER_DURATION_US_IDEAL) + 2) / 3 * 3;
constexpr uint32_t I2S_BUFFER_DURATION_US_ACTUAL = num_i2s_samples_to_duration_us(I2S_BUFFER_SAMPLES);

I2SClocklessLedStrip::I2SClocklessLedStrip(uint8_t pin, uint16_t num_leds, light::ChannelColors channel_colors)
    : pin_(pin),
      num_leds_(num_leds),
      channel_colors_(channel_colors),
      color_data_bytes_(num_leds * channel_colors_.bytes_per_led()) {}

void I2SClocklessLedStrip::dump_config() {
  ESP_LOGCONFIG(TAG,
                "I2S Clockless LED Strip:\n"
                "  Pin: %u",
                this->pin_);
  char channel_colors[5];
  ESP_LOGCONFIG(TAG,
                "  Channel colors: %s\n"
                "  Number of LEDs: %" PRIu16 "\n"
                "  DMA buffer: %" PRIuPTR " samples (%" PRIu32 " us)",
                this->channel_colors_.to_string(channel_colors), this->num_leds_, I2S_BUFFER_SAMPLES,
                I2S_BUFFER_DURATION_US_ACTUAL);
}

float I2SClocklessLedStrip::get_setup_priority() const { return setup_priority::IO; }

void I2SClocklessLedStrip::setup() {
  const size_t i2s_data_bytes = this->color_data_bytes_ * I2S_BYTES_PER_SAMPLE;

  RAMAllocator<uint8_t> allocator;
  if ((this->i2s_data_ = allocator.allocate(i2s_data_bytes)) == nullptr ||
      (this->color_data_ = allocator.allocate(this->color_data_bytes_)) == nullptr ||
      (this->effect_data_ = allocator.allocate(this->num_leds_)) == nullptr) {
    allocator.deallocate(this->i2s_data_, i2s_data_bytes);
    allocator.deallocate(this->color_data_, this->color_data_bytes_);
    allocator.deallocate(this->effect_data_, this->num_leds_);
    this->mark_failed(LOG_STR(ERROR_ALLOCATION));
    return;
  }
  memset(this->i2s_data_, 0, i2s_data_bytes);
  memset(this->color_data_, 0, this->color_data_bytes_);
  memset(this->effect_data_, 0, this->num_leds_);

  i2s_chan_config_t chan_config = {
      .id = I2S_NUM_AUTO,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 2,
      .dma_frame_num = I2S_BUFFER_SAMPLES,
      .auto_clear_after_cb = false,
      .auto_clear_before_cb = false,
      .allow_pd = false,
      .intr_priority = 0,
  };
  esp_err_t err;
  if ((err = i2s_new_channel(&chan_config, &this->tx_handle_, NULL)) != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_new_channel: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR(ERROR_I2S));
    return;
  }

  i2s_tdm_config_t tdm_config = {
      .clk_cfg =
          {
              .sample_rate_hz = I2S_SAMPLE_RATE_HZ,
              .clk_src = I2S_CLK_SRC_DEFAULT,
              .mclk_multiple = I2S_MCLK_MULTIPLE_384,
          },
      .slot_cfg =
          {
              .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT,
              .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
              .slot_mode = I2S_SLOT_MODE_MONO,
              .slot_mask = I2S_TDM_SLOT0,
              .ws_width = 1,
              .big_endian = true,
              .total_slot = I2S_TDM_AUTO_SLOT_NUM,
          },
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = I2S_GPIO_UNUSED,
              .ws = I2S_GPIO_UNUSED,
              .dout = gpio_num_t(this->pin_),
              .din = I2S_GPIO_UNUSED,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };
  if ((err = i2s_channel_init_tdm_mode(this->tx_handle_, &tdm_config)) != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_channel_init_tdm_mode: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR(ERROR_I2S));
    return;
  }

  i2s_event_callbacks_t event_callbacks = {
      .on_sent = i2s_on_sent_callback,
  };
  if ((err = i2s_channel_register_event_callback(this->tx_handle_, &event_callbacks, this)) != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_channel_register_event_callback: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR(ERROR_I2S));
    return;
  }

  if ((err = i2s_channel_enable(this->tx_handle_)) != ESP_OK) {
    ESP_LOGE(TAG, "Error in i2s_channel_enable: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR(ERROR_I2S));
    return;
  }
}

light::LightTraits I2SClocklessLedStrip::get_traits() {
  auto traits = light::LightTraits();
  if (this->channel_colors_.has_white()) {
    traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE});
  } else {
    traits.set_supported_color_modes({light::ColorMode::RGB});
  }
  return traits;
}

void I2SClocklessLedStrip::write_state(light::LightState *state) {
  if (this->is_failed())
    return;

  if (this->i2s_data_ready_.load(std::memory_order_acquire)) {
    // Busy sending the last frame, try again later.
    this->schedule_show();
    return;
  }

  const size_t color_data_bytes = this->color_data_bytes_;
  uint8_t *color_data = this->color_data_;
  uint8_t *i2s_data = this->i2s_data_;
  for (size_t i = 0; i < color_data_bytes; i++) {
    const uint8_t color_byte = *(color_data++);
    *(i2s_data++) = 0b10010010 | ((color_byte & 0x80) >> 1) | ((color_byte & 0x40) >> 3) | ((color_byte & 0x20) >> 5);
    *(i2s_data++) = 0b01001001 | ((color_byte & 0x10) << 1) | ((color_byte & 0x08) >> 1);
    *(i2s_data++) = 0b00100100 | ((color_byte & 0x04) << 5) | ((color_byte & 0x02) << 3) | ((color_byte & 0x01) << 1);
  }

  this->i2s_data_sent_ = 0;
  this->i2s_data_ready_.store(true, std::memory_order_release);

  this->mark_shown_();
}

bool IRAM_ATTR HOT I2SClocklessLedStrip::i2s_on_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event,
                                                              void *user_ctx) {
  auto *const self = static_cast<I2SClocklessLedStrip *>(user_ctx);
  auto *const dma_buf = static_cast<uint8_t *>(event->dma_buf);
  const size_t dma_buf_size = event->size;

  bool dma_buf_filled = false;
  if (self->i2s_data_ready_.load(std::memory_order_acquire)) {
    const size_t i2s_data_bytes = self->color_data_bytes_ * I2S_BYTES_PER_SAMPLE;
    if (self->i2s_data_sent_ < i2s_data_bytes) {
      const size_t i2s_data_remaining = i2s_data_bytes - self->i2s_data_sent_;
      if (i2s_data_remaining >= dma_buf_size) {
        memcpy(dma_buf, self->i2s_data_ + self->i2s_data_sent_, dma_buf_size);
      } else {
        memcpy(dma_buf, self->i2s_data_ + self->i2s_data_sent_, i2s_data_remaining);
        memset(dma_buf + i2s_data_remaining, 0, dma_buf_size - i2s_data_remaining);
      }
      dma_buf_filled = true;
    }
    self->i2s_data_sent_ += dma_buf_size;
    if (self->i2s_data_sent_ >= i2s_data_bytes + I2S_RESET_BYTES) {
      self->i2s_data_ready_.store(false, std::memory_order_release);
    }
  }
  // We only need to clear the buffer when the first byte is non-zero.  Zero is not a value
  // that can appear within the I2S data that gets copied into the front of the buffer so
  // if the first byte is zero then the buffer must have already been cleared.
  if (!dma_buf_filled && dma_buf[0] != 0) {
    memset(dma_buf, 0, dma_buf_size);
  }
  return false;
}

void I2SClocklessLedStrip::clear_effect_data() { memset(this->effect_data_, 0, this->num_leds_); }

light::ESPColorView I2SClocklessLedStrip::get_view_internal(int32_t index) const {
  const light::ChannelColors &colors = this->channel_colors_;
  uint8_t *led = this->color_data_ + (index * colors.bytes_per_led());
  return {led + colors.r,
          led + colors.g,
          led + colors.b,
          colors.has_white() ? led + colors.w : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

}  // namespace esphome::i2s_clockless_led_strip

#endif
