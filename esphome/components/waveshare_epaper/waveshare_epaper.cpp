#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include <cinttypes>
#include <bitset>

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_epaper";

static const uint8_t LUT_SIZE_WAVESHARE = 30;

static const uint8_t FULL_UPDATE_LUT[LUT_SIZE_WAVESHARE] = {0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69,
                                                            0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 0x00, 0x00, 0x00, 0x00,
                                                            0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT[LUT_SIZE_WAVESHARE] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t LUT_SIZE_TTGO = 70;

static const uint8_t FULL_UPDATE_LUT_TTGO[LUT_SIZE_TTGO] = {
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,  // LUT0: BB:     VS 0 ~7
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,  // LUT1: BW:     VS 0 ~7
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,  // LUT2: WB:     VS 0 ~7
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,  // LUT3: WW:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT4: VCOM:   VS 0 ~7
    0x03, 0x03, 0x00, 0x00, 0x02,              // TP0 A~D RP0
    0x09, 0x09, 0x00, 0x00, 0x02,              // TP1 A~D RP1
    0x03, 0x03, 0x00, 0x00, 0x02,              // TP2 A~D RP2
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP3 A~D RP3
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP4 A~D RP4
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP5 A~D RP5
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP6 A~D RP6
};

static const uint8_t PARTIAL_UPDATE_LUT_TTGO[LUT_SIZE_TTGO] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT0: BB:     VS 0 ~7
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT1: BW:     VS 0 ~7
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT2: WB:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT3: WW:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT4: VCOM:   VS 0 ~7
    0x0A, 0x00, 0x00, 0x00, 0x00,              // TP0 A~D RP0
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP1 A~D RP1
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP2 A~D RP2
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP3 A~D RP3
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP4 A~D RP4
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP5 A~D RP5
    0x00, 0x00, 0x00, 0x00, 0x00,              // TP6 A~D RP6
};

static const uint8_t LUT_SIZE_TTGO_B73 = 100;

static const uint8_t FULL_UPDATE_LUT_TTGO_B73[LUT_SIZE_TTGO_B73] = {
    0xA0, 0x90, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x90, 0xA0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xA0, 0x90, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x90, 0xA0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x03, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t PARTIAL_UPDATE_LUT_TTGO_B73[LUT_SIZE_TTGO_B73] = {
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_SIZE_TTGO_B1 = 29;

static const uint8_t FULL_UPDATE_LUT_TTGO_B1[LUT_SIZE_TTGO_B1] = {
    0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x01, 0x00, 0x00, 0x00, 0x00};

static const uint8_t PARTIAL_UPDATE_LUT_TTGO_B1[LUT_SIZE_TTGO_B1] = {
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// clang-format off
// Disable formatting to preserve the same look as in Waveshare examples
static const uint8_t PARTIAL_UPD_2IN9_LUT_SIZE = 159;
static const uint8_t PARTIAL_UPD_2IN9_LUT[PARTIAL_UPD_2IN9_LUT_SIZE] =
{
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x22, 0x17, 0x41, 0xB0, 0x32, 0x36,
};
// clang-format on

void WaveshareEPaperBase::setup() {
  this->init_internal_(this->get_buffer_length_());
  this->setup_pins_();
  this->spi_setup();
  this->reset_();
  this->initialize();
}
void WaveshareEPaperBase::setup_pins_() {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
}
float WaveshareEPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }
void WaveshareEPaperBase::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}
void WaveshareEPaperBase::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

// write a command followed by one or more bytes of data.
// The command is the first byte, length is the total including cmd.
void WaveshareEPaperBase::cmd_data(const uint8_t *c_data, size_t length) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(c_data[0]);
  this->dc_pin_->digital_write(true);
  this->write_array(c_data + 1, length - 1);
  this->disable();
}

bool WaveshareEPaperBase::wait_until_idle_() {
  if (this->busy_pin_ == nullptr || !this->busy_pin_->digital_read()) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    delay(1);
  }
  return true;
}
void WaveshareEPaperBase::update() {
  this->do_update_();
  this->display();
}
void WaveshareEPaper::fill(Color color) {
  // flip logic
  const uint8_t fill = color.is_on() ? 0x00 : 0xFF;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void WaveshareEPaper7C::setup() {
  this->init_internal_7c_(this->get_buffer_length_());
  this->setup_pins_();
  this->spi_setup();
  this->reset_();
  this->initialize();
}
void WaveshareEPaper7C::init_internal_7c_(uint32_t buffer_length) {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  uint32_t small_buffer_length = buffer_length / NUM_BUFFERS;

  for (int i = 0; i < NUM_BUFFERS; i++) {
    this->buffers_[i] = allocator.allocate(small_buffer_length);
    if (this->buffers_[i] == nullptr) {
      ESP_LOGE(TAG, "Could not allocate buffer %d for display!", i);
      for (auto &buffer : this->buffers_) {
        allocator.deallocate(buffer, small_buffer_length);
        buffer = nullptr;
      }
      return;
    }
  }
  this->clear();
}
uint8_t WaveshareEPaper7C::color_to_hex(Color color) {
  uint8_t hex_code;
  if (color.red > 127) {
    if (color.green > 170) {
      if (color.blue > 127) {
        hex_code = 0x1;  // White
      } else {
        hex_code = 0x5;  // Yellow
      }
    } else if (color.green > 85) {
      hex_code = 0x6;  // Orange
    } else {
      hex_code = 0x4;  // Red (or Magenta)
    }
  } else {
    if (color.green > 127) {
      if (color.blue > 127) {
        hex_code = 0x3;  // Cyan -> Blue
      } else {
        hex_code = 0x2;  // Green
      }
    } else {
      if (color.blue > 127) {
        hex_code = 0x3;  // Blue
      } else {
        hex_code = 0x0;  // Black
      }
    }
  }

  return hex_code;
}
void WaveshareEPaper7C::fill(Color color) {
  uint8_t pixel_color;
  if (color.is_on()) {
    pixel_color = this->color_to_hex(color);
  } else {
    pixel_color = 0x1;
  }

  if (this->buffers_[0] == nullptr) {
    ESP_LOGE(TAG, "Buffer unavailable!");
  } else {
    uint32_t small_buffer_length = this->get_buffer_length_() / NUM_BUFFERS;
    for (auto &buffer : this->buffers_) {
      for (uint32_t buffer_pos = 0; buffer_pos < small_buffer_length; buffer_pos += 3) {
        // We store 8 bitset<3> in 3 bytes
        // | byte 1 | byte 2 | byte 3 |
        // |aaabbbaa|abbbaaab|bbaaabbb|
        buffer[buffer_pos + 0] = pixel_color << 5 | pixel_color << 2 | pixel_color >> 1;
        buffer[buffer_pos + 1] = pixel_color << 7 | pixel_color << 4 | pixel_color << 1 | pixel_color >> 2;
        buffer[buffer_pos + 2] = pixel_color << 6 | pixel_color << 3 | pixel_color << 0;
      }
      App.feed_wdt();
    }
  }
}
void WaveshareEPaper7C::send_buffers_() {
  if (this->buffers_[0] == nullptr) {
    ESP_LOGE(TAG, "Buffer unavailable!");
    return;
  }

  uint32_t small_buffer_length = this->get_buffer_length_() / NUM_BUFFERS;
  uint8_t byte_to_send;
  for (auto &buffer : this->buffers_) {
    for (uint32_t buffer_pos = 0; buffer_pos < small_buffer_length; buffer_pos += 3) {
      std::bitset<24> triplet =
          buffer[buffer_pos + 0] << 16 | buffer[buffer_pos + 1] << 8 | buffer[buffer_pos + 2] << 0;
      // 8 bitset<3> are stored in 3 bytes
      // |aaabbbaa|abbbaaab|bbaaabbb|
      // | byte 1 | byte 2 | byte 3 |
      byte_to_send = ((triplet >> 17).to_ulong() & 0b01110000) | ((triplet >> 18).to_ulong() & 0b00000111);
      this->data(byte_to_send);

      byte_to_send = ((triplet >> 11).to_ulong() & 0b01110000) | ((triplet >> 12).to_ulong() & 0b00000111);
      this->data(byte_to_send);

      byte_to_send = ((triplet >> 5).to_ulong() & 0b01110000) | ((triplet >> 6).to_ulong() & 0b00000111);
      this->data(byte_to_send);

      byte_to_send = ((triplet << 1).to_ulong() & 0b01110000) | ((triplet << 0).to_ulong() & 0b00000111);
      this->data(byte_to_send);
    }
    App.feed_wdt();
  }
}
void WaveshareEPaper7C::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(20);
    this->reset_pin_->digital_write(false);
    delay(1);
    this->reset_pin_->digital_write(true);
    delay(20);
  }
}

void HOT WaveshareEPaper::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t pos = (x + y * this->get_width_controller()) / 8u;
  const uint8_t subpos = x & 0x07;
  // flip logic
  if (!color.is_on()) {
    this->buffer_[pos] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos] &= ~(0x80 >> subpos);
  }
}

uint32_t WaveshareEPaper::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 8u;
}  // just a black buffer
uint32_t WaveshareEPaperBWR::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 4u;
}  // black and red buffer
uint32_t WaveshareEPaper7C::get_buffer_length_() {
  return this->get_width_controller() * this->get_height_internal() / 8u * 3u;
}  // 7 colors buffer, 1 pixel = 3 bits, we will store 8 pixels in 24 bits = 3 bytes

void WaveshareEPaperBWR::fill(Color color) {
  this->filled_rectangle(0, 0, this->get_width(), this->get_height(), color);
}
void HOT WaveshareEPaperBWR::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  const uint32_t buf_half_len = this->get_buffer_length_() / 2u;

  const uint32_t pos = (x + y * this->get_width_internal()) / 8u;
  const uint8_t subpos = x & 0x07;
  // flip logic
  if (color.is_on()) {
    this->buffer_[pos] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos] &= ~(0x80 >> subpos);
  }

  // draw red pixels only, if the color contains red only
  if (((color.red > 0) && (color.green == 0) && (color.blue == 0))) {
    this->buffer_[pos + buf_half_len] |= 0x80 >> subpos;
  } else {
    this->buffer_[pos + buf_half_len] &= ~(0x80 >> subpos);
  }
}
void HOT WaveshareEPaper7C::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  uint8_t pixel_bits = this->color_to_hex(color);
  uint32_t small_buffer_length = this->get_buffer_length_() / NUM_BUFFERS;
  uint32_t pixel_position = x + y * this->get_width_controller();
  uint32_t first_bit_position = pixel_position * 3;
  uint32_t byte_position = first_bit_position / 8u;
  uint32_t byte_subposition = first_bit_position % 8u;
  uint32_t buffer_position = byte_position / small_buffer_length;
  uint32_t buffer_subposition = byte_position % small_buffer_length;

  if (byte_subposition <= 5) {
    this->buffers_[buffer_position][buffer_subposition] =
        (this->buffers_[buffer_position][buffer_subposition] & (0xFF ^ (0b111 << (5 - byte_subposition)))) |
        (pixel_bits << (5 - byte_subposition));
  } else {
    this->buffers_[buffer_position][buffer_subposition + 0] =
        (this->buffers_[buffer_position][buffer_subposition + 0] & (0xFF ^ (0b111 >> (byte_subposition - 5)))) |
        (pixel_bits >> (byte_subposition - 5));

    this->buffers_[buffer_position][buffer_subposition + 1] = (this->buffers_[buffer_position][buffer_subposition + 1] &
                                                               (0xFF ^ (0xFF & (0b111 << (13 - byte_subposition))))) |
                                                              (pixel_bits << (13 - byte_subposition));
  }
}
void WaveshareEPaperBase::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void WaveshareEPaperBase::end_command_() { this->disable(); }
void WaveshareEPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void WaveshareEPaperBase::end_data_() { this->disable(); }
void WaveshareEPaperBase::on_safe_shutdown() { this->deep_sleep(); }

// ========================================================
//                          Type A
// ========================================================

void WaveshareEPaperTypeA::initialize() {
  // Achieve display intialization
  this->init_display_();
  // If a reset pin is configured, eligible displays can be set to deep sleep
  // between updates, as recommended by the hardware provider
  if (this->reset_pin_ != nullptr) {
    switch (this->model_) {
      // More models can be added here to enable deep sleep if eligible
      case WAVESHARE_EPAPER_1_54_IN:
      case WAVESHARE_EPAPER_1_54_IN_V2:
        this->deep_sleep_between_updates_ = true;
        ESP_LOGI(TAG, "Set the display to deep sleep");
        this->deep_sleep();
        break;
      default:
        break;
    }
  }
}
void WaveshareEPaperTypeA::init_display_() {
  if (this->model_ == TTGO_EPAPER_2_13_IN_B74 || this->model_ == WAVESHARE_EPAPER_2_13_IN_V2) {
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(false);
      delay(10);
      this->reset_pin_->digital_write(true);
      delay(10);
      this->wait_until_idle_();
    }

    this->command(0x12);  // SWRESET
    this->wait_until_idle_();
  }

  // COMMAND DRIVER OUTPUT CONTROL
  this->command(0x01);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);
  this->data(0x00);  // ? GD = 0, SM = 0, TB = 0

  // COMMAND BOOSTER SOFT START CONTROL
  this->command(0x0C);
  this->data(0xD7);
  this->data(0xD6);
  this->data(0x9D);

  // COMMAND WRITE VCOM REGISTER
  this->command(0x2C);
  this->data(0xA8);

  // COMMAND SET DUMMY LINE PERIOD
  this->command(0x3A);
  this->data(0x1A);

  // COMMAND SET GATE TIME
  this->command(0x3B);
  this->data(0x08);  // 2µs per row

  // COMMAND DATA ENTRY MODE SETTING
  this->command(0x11);
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B1:
      this->data(0x01);  // x increase, y decrease : as in demo code
      break;
    case TTGO_EPAPER_2_13_IN_B74:
    case WAVESHARE_EPAPER_2_9_IN_V2:
      this->data(0x03);  // from top left to bottom right
      // RAM content option for Display Update
      this->command(0x21);
      this->data(0x00);
      this->data(0x80);
      break;
    default:
      this->data(0x03);  // from top left to bottom right
  }
}
void WaveshareEPaperTypeA::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
      ESP_LOGCONFIG(TAG, "  Model: 1.54in");
      break;
    case WAVESHARE_EPAPER_1_54_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 1.54inV2");
      break;
    case WAVESHARE_EPAPER_2_13_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in");
      break;
    case WAVESHARE_EPAPER_2_13_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 2.13inV2");
      break;
    case TTGO_EPAPER_2_13_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO)");
      break;
    case TTGO_EPAPER_2_13_IN_B73:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO B73)");
      break;
    case TTGO_EPAPER_2_13_IN_B74:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO B74)");
      break;
    case TTGO_EPAPER_2_13_IN_B1:
      ESP_LOGCONFIG(TAG, "  Model: 2.13in (TTGO B1)");
      break;
    case WAVESHARE_EPAPER_2_9_IN:
      ESP_LOGCONFIG(TAG, "  Model: 2.9in");
      break;
    case WAVESHARE_EPAPER_2_9_IN_V2:
      ESP_LOGCONFIG(TAG, "  Model: 2.9inV2");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void HOT WaveshareEPaperTypeA::display() {
  bool full_update = this->at_update_ == 0;
  bool prev_full_update = this->at_update_ == 1;

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Wake up the display");
    this->reset_();
    this->wait_until_idle_();
    this->init_display_();
  }

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  if (this->full_update_every_ >= 1) {
    if (full_update != prev_full_update) {
      switch (this->model_) {
        case TTGO_EPAPER_2_13_IN:
        case WAVESHARE_EPAPER_2_13_IN_V2:
          // Waveshare 2.13" V2 uses the same LUTs as TTGO
          this->write_lut_(full_update ? FULL_UPDATE_LUT_TTGO : PARTIAL_UPDATE_LUT_TTGO, LUT_SIZE_TTGO);
          break;
        case TTGO_EPAPER_2_13_IN_B73:
          this->write_lut_(full_update ? FULL_UPDATE_LUT_TTGO_B73 : PARTIAL_UPDATE_LUT_TTGO_B73, LUT_SIZE_TTGO_B73);
          break;
        case TTGO_EPAPER_2_13_IN_B74:
          // there is no LUT
          break;
        case TTGO_EPAPER_2_13_IN_B1:
          this->write_lut_(full_update ? FULL_UPDATE_LUT_TTGO_B1 : PARTIAL_UPDATE_LUT_TTGO_B1, LUT_SIZE_TTGO_B1);
          break;
        default:
          this->write_lut_(full_update ? FULL_UPDATE_LUT : PARTIAL_UPDATE_LUT, LUT_SIZE_WAVESHARE);
      }
    }
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  }

  if (this->model_ == WAVESHARE_EPAPER_2_13_IN_V2) {
    // Set VCOM for full or partial update
    this->command(0x2C);
    this->data(full_update ? 0x55 : 0x26);

    if (!full_update) {
      // Enable "ping-pong"
      this->command(0x37);
      this->data(0x00);
      this->data(0x00);
      this->data(0x00);
      this->data(0x00);
      this->data(0x40);
      this->data(0x00);
      this->data(0x00);
      this->command(0x22);
      this->data(0xc0);
      this->command(0x20);
    }
  }

  // Border waveform
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B74:
      this->command(0x3C);
      this->data(full_update ? 0x05 : 0x80);
      break;
    case WAVESHARE_EPAPER_2_13_IN_V2:
      this->command(0x3C);
      this->data(full_update ? 0x03 : 0x01);
      break;
    default:
      break;
  }

  // Set x & y regions we want to write to (full)
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B1:
      // COMMAND SET RAM X ADDRESS START END POSITION
      this->command(0x44);
      this->data(0x00);
      this->data((this->get_width_controller() - 1) >> 3);
      // COMMAND SET RAM Y ADDRESS START END POSITION
      this->command(0x45);
      this->data(this->get_height_internal() - 1);
      this->data((this->get_height_internal() - 1) >> 8);
      this->data(0x00);
      this->data(0x00);

      // COMMAND SET RAM X ADDRESS COUNTER
      this->command(0x4E);
      this->data(0x00);
      // COMMAND SET RAM Y ADDRESS COUNTER
      this->command(0x4F);
      this->data(this->get_height_internal() - 1);
      this->data((this->get_height_internal() - 1) >> 8);

      break;
    default:
      // COMMAND SET RAM X ADDRESS START END POSITION
      this->command(0x44);
      this->data(0x00);
      this->data((this->get_width_internal() - 1) >> 3);
      // COMMAND SET RAM Y ADDRESS START END POSITION
      this->command(0x45);
      this->data(0x00);
      this->data(0x00);
      this->data(this->get_height_internal() - 1);
      this->data((this->get_height_internal() - 1) >> 8);

      // COMMAND SET RAM X ADDRESS COUNTER
      this->command(0x4E);
      this->data(0x00);
      // COMMAND SET RAM Y ADDRESS COUNTER
      this->command(0x4F);
      this->data(0x00);
      this->data(0x00);
  }

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    return;
  }

  // COMMAND WRITE RAM
  this->command(0x24);
  this->start_data_();
  switch (this->model_) {
    case TTGO_EPAPER_2_13_IN_B1: {  // block needed because of variable initializations
      int16_t wb = ((this->get_width_controller()) >> 3);
      for (int i = 0; i < this->get_height_internal(); i++) {
        for (int j = 0; j < wb; j++) {
          int idx = j + (this->get_height_internal() - 1 - i) * wb;
          this->write_byte(this->buffer_[idx]);
        }
      }
      break;
    }
    default:
      this->write_array(this->buffer_, this->get_buffer_length_());
  }
  this->end_data_();

  if (this->model_ == WAVESHARE_EPAPER_2_13_IN_V2 && full_update) {
    // Write base image again on full refresh
    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  }

  // COMMAND DISPLAY UPDATE CONTROL 2
  this->command(0x22);
  switch (this->model_) {
    case WAVESHARE_EPAPER_2_9_IN_V2:
    case WAVESHARE_EPAPER_1_54_IN_V2:
    case TTGO_EPAPER_2_13_IN_B74:
      this->data(full_update ? 0xF7 : 0xFF);
      break;
    case TTGO_EPAPER_2_13_IN_B73:
      this->data(0xC7);
      break;
    case WAVESHARE_EPAPER_2_13_IN_V2:
      this->data(full_update ? 0xC7 : 0x0C);
      break;
    default:
      this->data(0xC4);
      break;
  }

  // COMMAND MASTER ACTIVATION
  this->command(0x20);
  // COMMAND TERMINATE FRAME READ WRITE
  this->command(0xFF);

  this->status_clear_warning();

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Set the display back to deep sleep");
    this->deep_sleep();
  }
}
int WaveshareEPaperTypeA::get_width_internal() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
    case WAVESHARE_EPAPER_1_54_IN_V2:
      return 200;
    case WAVESHARE_EPAPER_2_13_IN:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN:
    case TTGO_EPAPER_2_13_IN_B73:
    case TTGO_EPAPER_2_13_IN_B74:
    case TTGO_EPAPER_2_13_IN_B1:
      return 122;
    case WAVESHARE_EPAPER_2_9_IN:
    case WAVESHARE_EPAPER_2_9_IN_V2:
      return 128;
  }
  return 0;
}
// The controller of the 2.13" displays has a buffer larger than screen size
int WaveshareEPaperTypeA::get_width_controller() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_2_13_IN:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN:
    case TTGO_EPAPER_2_13_IN_B73:
    case TTGO_EPAPER_2_13_IN_B74:
    case TTGO_EPAPER_2_13_IN_B1:
      return 128;
    default:
      return this->get_width_internal();
  }
}
int WaveshareEPaperTypeA::get_height_internal() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
    case WAVESHARE_EPAPER_1_54_IN_V2:
      return 200;
    case WAVESHARE_EPAPER_2_13_IN:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN:
    case TTGO_EPAPER_2_13_IN_B73:
    case TTGO_EPAPER_2_13_IN_B74:
    case TTGO_EPAPER_2_13_IN_B1:
      return 250;
    case WAVESHARE_EPAPER_2_9_IN:
    case WAVESHARE_EPAPER_2_9_IN_V2:
      return 296;
  }
  return 0;
}
void WaveshareEPaperTypeA::write_lut_(const uint8_t *lut, const uint8_t size) {
  // COMMAND WRITE LUT REGISTER
  this->command(0x32);
  for (uint8_t i = 0; i < size; i++)
    this->data(lut[i]);
}
WaveshareEPaperTypeA::WaveshareEPaperTypeA(WaveshareEPaperTypeAModel model) : model_(model) {}
void WaveshareEPaperTypeA::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

uint32_t WaveshareEPaperTypeA::idle_timeout_() {
  switch (this->model_) {
    case WAVESHARE_EPAPER_1_54_IN:
    case WAVESHARE_EPAPER_1_54_IN_V2:
    case WAVESHARE_EPAPER_2_13_IN_V2:
    case TTGO_EPAPER_2_13_IN_B1:
      return 2500;
    default:
      return WaveshareEPaperBase::idle_timeout_();
  }
}

// ========================================================
//                          Type B
// ========================================================
// Datasheet:
//  - https://www.waveshare.com/w/upload/7/7f/4.2inch-e-paper-b-specification.pdf
//  - https://github.com/soonuse/epd-library-arduino/blob/master/4.2inch_e-paper/epd4in2/

static const uint8_t LUT_VCOM_DC_2_7[44] = {
    0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x32, 0x32, 0x00, 0x00, 0x02, 0x00,
    0x0F, 0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_WHITE_TO_WHITE_2_7[42] = {
    0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0xA0, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_BLACK_TO_WHITE_2_7[42] = {
    0x50, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0xA0, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_WHITE_TO_BLACK_2_7[] = {
    0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0x50, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_BLACK_TO_BLACK_2_7[42] = {
    0xA0, 0x0F, 0x0F, 0x00, 0x00, 0x05, 0x60, 0x32, 0x32, 0x00, 0x00, 0x02, 0x50, 0x0F,
    0x0F, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void WaveshareEPaper2P7In::initialize() {
  // command power setting
  this->command(0x01);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0x09);  // VDHR

  // command booster soft start
  this->command(0x06);
  this->data(0x07);
  this->data(0x07);
  this->data(0x17);

  // Power optimization - ???
  this->command(0xF8);
  this->data(0x60);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x89);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x90);
  this->data(0x00);
  this->command(0xF8);
  this->data(0x93);
  this->data(0x2A);
  this->command(0xF8);
  this->data(0xA0);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0xA1);
  this->data(0x00);
  this->command(0xF8);
  this->data(0x73);
  this->data(0x41);

  // command partial display refresh
  this->command(0x16);
  this->data(0x00);

  // command power on
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);

  // Command panel setting
  this->command(0x00);
  this->data(0xAF);  // KW-BF   KWR-AF    BWROTP 0f
  // command pll control
  this->command(0x30);
  this->data(0x3A);  // 3A 100HZ   29 150Hz 39 200HZ    31 171HZ
  // COMMAND VCM DC SETTING
  this->command(0x82);
  this->data(0x12);

  delay(2);
  // COMMAND LUT FOR VCOM
  this->command(0x20);
  for (uint8_t i : LUT_VCOM_DC_2_7)
    this->data(i);

  // COMMAND LUT WHITE TO WHITE
  this->command(0x21);
  for (uint8_t i : LUT_WHITE_TO_WHITE_2_7)
    this->data(i);
  // COMMAND LUT BLACK TO WHITE
  this->command(0x22);
  for (uint8_t i : LUT_BLACK_TO_WHITE_2_7)
    this->data(i);
  // COMMAND LUT WHITE TO BLACK
  this->command(0x23);
  for (uint8_t i : LUT_WHITE_TO_BLACK_2_7)
    this->data(i);
  // COMMAND LUT BLACK TO BLACK
  this->command(0x24);
  for (uint8_t i : LUT_BLACK_TO_BLACK_2_7)
    this->data(i);
}
void HOT WaveshareEPaper2P7In::display() {
  uint32_t buf_len = this->get_buffer_length_();

  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }
  delay(2);

  // COMMAND DATA START TRANSMISSION 2
  this->command(0x13);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper2P7In::get_width_internal() { return 176; }
int WaveshareEPaper2P7In::get_height_internal() { return 264; }
void WaveshareEPaper2P7In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P7InV2::initialize() {
  this->reset_();
  this->wait_until_idle_();

  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  // SET WINDOWS
  // XRAM_START_AND_END_POSITION
  this->command(0x44);
  this->data(0x00);
  this->data(((this->get_width_controller() - 1) >> 3) & 0xFF);
  // YRAM_START_AND_END_POSITION
  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data((get_height_internal() - 1) & 0xFF);
  this->data(((get_height_internal() - 1) >> 8) & 0xFF);

  // SET CURSOR
  // XRAM_ADDRESS
  this->command(0x4E);
  this->data(0x00);
  // YRAM_ADDRESS
  this->command(0x4F);
  this->data(0x00);
  this->data(0x00);

  this->command(0x11);  // data entry mode
  this->data(0x03);
}
void HOT WaveshareEPaper2P7InV2::display() {
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x22);
  this->data(0xF7);
  this->command(0x20);
}
int WaveshareEPaper2P7InV2::get_width_internal() { return 176; }
int WaveshareEPaper2P7InV2::get_height_internal() { return 264; }
void WaveshareEPaper2P7InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in V2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//                          1.54inch_v2_e-paper_b
// ========================================================
// Datasheet:
//  - https://files.waveshare.com/upload/9/9e/1.54inch-e-paper-b-v2-specification.pdf
//  - https://www.waveshare.com/wiki/1.54inch_e-Paper_Module_(B)_Manual

void WaveshareEPaper1P54InBV2::initialize() {
  this->reset_();

  this->wait_until_idle_();

  this->command(0x12);
  this->wait_until_idle_();

  this->command(0x01);
  this->data(0xC7);
  this->data(0x00);
  this->data(0x01);

  this->command(0x11);  // data entry mode
  this->data(0x01);

  this->command(0x44);  // set Ram-X address start/end position
  this->data(0x00);
  this->data(0x18);  // 0x18-->(24+1)*8=200

  this->command(0x45);  // set Ram-Y address start/end position
  this->data(0xC7);     // 0xC7-->(199+1)=200
  this->data(0x00);
  this->data(0x00);
  this->data(0x00);

  this->command(0x3C);  // BorderWavefrom
  this->data(0x05);

  this->command(0x18);  // Read built-in temperature sensor
  this->data(0x80);

  this->command(0x4E);  // set RAM x address count to 0;
  this->data(0x00);
  this->command(0x4F);  // set RAM y address count to 0x199;
  this->data(0xC7);
  this->data(0x00);

  this->wait_until_idle_();
}

void HOT WaveshareEPaper1P54InBV2::display() {
  uint32_t buf_len_half = this->get_buffer_length_() >> 1;
  this->initialize();

  // COMMAND DATA START TRANSMISSION 1 (BLACK)
  this->command(0x24);
  delay(2);
  for (uint32_t i = 0; i < buf_len_half; i++) {
    this->data(~this->buffer_[i]);
  }
  delay(2);

  // COMMAND DATA START TRANSMISSION 2  (RED)
  this->command(0x26);
  delay(2);
  for (uint32_t i = buf_len_half; i < buf_len_half * 2u; i++) {
    this->data(this->buffer_[i]);
  }
  this->command(0x22);
  this->data(0xf7);
  this->command(0x20);
  this->wait_until_idle_();

  this->deep_sleep();
}
int WaveshareEPaper1P54InBV2::get_height_internal() { return 200; }
int WaveshareEPaper1P54InBV2::get_width_internal() { return 200; }
void WaveshareEPaper1P54InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 1.54in V2 B");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//                          2.7inch_e-paper_b
// ========================================================
// Datasheet:
//  - https://www.waveshare.com/w/upload/d/d8/2.7inch-e-paper-b-specification.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in7b.c

static const uint8_t LUT_VCOM_DC_2_7B[44] = {0x00, 0x00, 0x00, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x00, 0x0A, 0x0A,
                                             0x00, 0x00, 0x08, 0x00, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x00, 0x0A,
                                             0x0A, 0x00, 0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00,
                                             0x03, 0x0E, 0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_WHITE_TO_WHITE_2_7B[42] = {0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x40, 0x0A, 0x0A, 0x00, 0x00,
                                                    0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x80, 0x0A, 0x0A, 0x00,
                                                    0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0E,
                                                    0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_BLACK_TO_WHITE_2_7B[42] = {0xA0, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x00, 0x0A, 0x0A, 0x00, 0x00,
                                                    0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x90, 0x0A, 0x0A, 0x00,
                                                    0x00, 0x08, 0xB0, 0x04, 0x10, 0x00, 0x00, 0x05, 0xB0, 0x03, 0x0E,
                                                    0x00, 0x00, 0x0A, 0xC0, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_WHITE_TO_BLACK_2_7B[] = {0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x20, 0x0A, 0x0A, 0x00, 0x00,
                                                  0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x10, 0x0A, 0x0A, 0x00,
                                                  0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0E,
                                                  0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

static const uint8_t LUT_BLACK_TO_BLACK_2_7B[42] = {0x90, 0x1A, 0x1A, 0x00, 0x00, 0x01, 0x40, 0x0A, 0x0A, 0x00, 0x00,
                                                    0x08, 0x84, 0x0E, 0x01, 0x0E, 0x01, 0x10, 0x80, 0x0A, 0x0A, 0x00,
                                                    0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0E,
                                                    0x00, 0x00, 0x0A, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01};

void WaveshareEPaper2P7InB::initialize() {
  this->reset_();

  // command power on
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);

  // Command panel setting
  this->command(0x00);
  this->data(0xAF);  // KW-BF   KWR-AF    BWROTP 0f
  // command pll control
  this->command(0x30);
  this->data(0x3A);  // 3A 100HZ   29 150Hz 39 200HZ    31 171HZ

  // command power setting
  this->command(0x01);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0x09);  // VDHR

  // command booster soft start
  this->command(0x06);
  this->data(0x07);
  this->data(0x07);
  this->data(0x17);

  // Power optimization - ???
  this->command(0xF8);
  this->data(0x60);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x89);
  this->data(0xA5);
  this->command(0xF8);
  this->data(0x90);
  this->data(0x00);
  this->command(0xF8);
  this->data(0x93);
  this->data(0x2A);
  this->command(0xF8);
  this->data(0x73);
  this->data(0x41);

  // COMMAND VCM DC SETTING
  this->command(0x82);
  this->data(0x12);

  // VCOM_AND_DATA_INTERVAL_SETTING
  this->command(0x50);
  this->data(0x87);  // define by OTP

  delay(2);
  // COMMAND LUT FOR VCOM
  this->command(0x20);
  for (uint8_t i : LUT_VCOM_DC_2_7B)
    this->data(i);
  // COMMAND LUT WHITE TO WHITE
  this->command(0x21);
  for (uint8_t i : LUT_WHITE_TO_WHITE_2_7B)
    this->data(i);
  // COMMAND LUT BLACK TO WHITE
  this->command(0x22);
  for (uint8_t i : LUT_BLACK_TO_WHITE_2_7B)
    this->data(i);
  // COMMAND LUT WHITE TO BLACK
  this->command(0x23);
  for (uint8_t i : LUT_WHITE_TO_BLACK_2_7B) {
    this->data(i);
  }
  // COMMAND LUT BLACK TO BLACK
  this->command(0x24);

  for (uint8_t i : LUT_BLACK_TO_BLACK_2_7B) {
    this->data(i);
  }

  delay(2);
}

void HOT WaveshareEPaper2P7InB::display() {
  uint32_t buf_len_half = this->get_buffer_length_() >> 1;
  this->initialize();

  // TCON_RESOLUTION
  this->command(0x61);
  this->data(this->get_width_controller() >> 8);
  this->data(this->get_width_controller() & 0xff);  // 176
  this->data(this->get_height_internal() >> 8);
  this->data(this->get_height_internal() & 0xff);  // 264

  // COMMAND DATA START TRANSMISSION 1 (BLACK)
  this->command(0x10);
  delay(2);
  for (uint32_t i = 0; i < buf_len_half; i++) {
    this->data(this->buffer_[i]);
  }
  this->command(0x11);
  delay(2);

  // COMMAND DATA START TRANSMISSION 2  (RED)
  this->command(0x13);
  delay(2);
  for (uint32_t i = buf_len_half; i < buf_len_half * 2u; i++) {
    this->data(this->buffer_[i]);
  }
  this->command(0x11);

  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  this->wait_until_idle_();

  this->deep_sleep();
}
int WaveshareEPaper2P7InB::get_width_internal() { return 176; }
int WaveshareEPaper2P7InB::get_height_internal() { return 264; }
void WaveshareEPaper2P7InB::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in B");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//                          2.7inch_e-paper_b_v2
// ========================================================
// Datasheet:
//  - https://www.waveshare.com/w/upload/7/7b/2.7inch-e-paper-b-v2-specification.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in7b_V2.c

void WaveshareEPaper2P7InBV2::initialize() {
  this->reset_();

  this->wait_until_idle_();
  this->command(0x12);
  this->wait_until_idle_();

  this->command(0x00);
  this->data(0x27);
  this->data(0x01);
  this->data(0x00);

  this->command(0x11);
  this->data(0x03);

  // self.SetWindows(0, 0, self.width-1, self.height-1)
  // SetWindows(self, Xstart, Ystart, Xend, Yend):

  uint32_t xend = this->get_width_controller() - 1;
  uint32_t yend = this->get_height_internal() - 1;
  this->command(0x44);
  this->data(0x00);
  this->data((xend >> 3) & 0xff);

  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data(yend & 0xff);
  this->data((yend >> 8) & 0xff);

  // SetCursor(self, Xstart, Ystart):
  this->command(0x4E);
  this->data(0x00);
  this->command(0x4F);
  this->data(0x00);
  this->data(0x00);
}

void HOT WaveshareEPaper2P7InBV2::display() {
  uint32_t buf_len = this->get_buffer_length_();
  // COMMAND DATA START TRANSMISSION 1 (BLACK)
  this->command(0x24);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }
  delay(2);

  // COMMAND DATA START TRANSMISSION 2  (RED)
  this->command(0x26);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }

  delay(2);

  this->command(0x20);

  this->wait_until_idle_();
}
int WaveshareEPaper2P7InBV2::get_width_internal() { return 176; }
int WaveshareEPaper2P7InBV2::get_height_internal() { return 264; }
void WaveshareEPaper2P7InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.7in B V2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               2.90in Type B (LUT from OTP)
// Datasheet:
//  - https://www.waveshare.com/w/upload/b/bb/2.9inch-e-paper-b-specification.pdf
//  - https://github.com/soonuse/epd-library-arduino/blob/master/2.9inch_e-paper_b/epd2in9b/epd2in9b.cpp
// ========================================================

void WaveshareEPaper2P9InB::initialize() {
  // from https://www.waveshare.com/w/upload/b/bb/2.9inch-e-paper-b-specification.pdf, page 37
  // EPD hardware init start
  this->reset_();

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);
  this->data(0x17);
  this->data(0x17);

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  // 128x296 resolution:        10
  // LUT from OTP:              0
  // B/W mode (doesn't work):   1
  // scan-up:                   1
  // shift-right:               1
  // booster ON:                1
  // no soft reset:             1
  this->data(0x9F);

  // COMMAND RESOLUTION SETTING
  // set to 128x296 by COMMAND PANEL SETTING

  // COMMAND VCOM AND DATA INTERVAL SETTING
  // use defaults for white border and ESPHome image polarity

  // EPD hardware init end
}
void HOT WaveshareEPaper2P9InB::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(2);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper2P9InB::get_width_internal() { return 128; }
int WaveshareEPaper2P9InB::get_height_internal() { return 296; }
void WaveshareEPaper2P9InB::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in (B)");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//  Waveshare 2.9-inch E-Paper (Type D)
//  Waveshare WIKI: https://www.waveshare.com/wiki/Pico-ePaper-2.9-D
//  Datasheet: https://www.waveshare.com/w/upload/b/b5/2.9inch_e-Paper_(D)_Specification.pdf
// ========================================================

void WaveshareEPaper2P9InD::initialize() {
  // EPD hardware init start
  this->reset_();

  // Booster Soft Start
  this->command(0x06);  // Command: BTST
  this->data(0x17);     // Soft start configuration Phase A
  this->data(0x17);     // Soft start configuration Phase B
  this->data(0x17);     // Soft start configuration Phase C

  // Power Setting
  this->command(0x01);  // Command: PWR
  this->data(0x03);     // Intern DC/DC for VDH/VDL and VGH/VGL
  this->data(0x00);     // Default configuration VCOM_HV and VGHL_LV
  this->data(0x2b);     // VDH = 10.8 V
  this->data(0x2b);     // VDL = -10.8 V

  // Power ON
  this->command(0x04);  // Command: PON
  this->wait_until_idle_();

  // Panel settings
  this->command(0x00);  // Command: PSR
  this->data(0x1F);     // LUT from OTP, black and white mode, default scan

  // PLL Control
  this->command(0x30);  // Command: PLL
  this->data(0x3A);     // Default PLL frequency

  // Resolution settings
  this->command(0x61);  // Command: TRES
  this->data(0x80);     // Width: 128
  this->data(0x01);     // Height MSB: 296
  this->data(0x28);     // Height LSB: 296

  // VCOM and data interval settings
  this->command(0x50);  // Command: CDI
  this->data(0x77);

  // VCOM_DC settings
  this->command(0x82);  // Command: VDCS
  this->data(0x12);     // Dafault VCOM_DC
}

void WaveshareEPaper2P9InD::display() {
  // Start transmitting old data (clearing buffer)
  this->command(0x10);  // Command: DTM1 (OLD frame data)
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // Start transmitting new data (updated content)
  this->command(0x13);  // Command: DTM2 (NEW frame data)
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // Refresh Display
  this->command(0x12);  // Command: DRF
  this->wait_until_idle_();

  // Enter Power Off
  this->command(0x02);  // Command: POF
  this->wait_until_idle_();

  // Enter Deep Sleep
  this->command(0x07);  // Command: DSLP
  this->data(0xA5);
}

int WaveshareEPaper2P9InD::get_width_internal() { return 128; }
int WaveshareEPaper2P9InD::get_height_internal() { return 296; }
void WaveshareEPaper2P9InD::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in (D)");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// DKE 2.9
// https://www.badge.team/docs/badges/sha2017/hardware/#e-ink-display-the-dke-group-depg0290b1
// https://www.badge.team/docs/badges/sha2017/hardware/DEPG0290B01V3.0.pdf
static const uint8_t LUT_SIZE_DKE = 70;
static const uint8_t UPDATE_LUT_DKE[LUT_SIZE_DKE] = {
    0xA0, 0x90, 0x50, 0x0,  0x0,  0x0,  0x0, 0x50, 0x90, 0xA0, 0x0,  0x0,  0x0,  0x0, 0xA0, 0x90, 0x50, 0x0,
    0x0,  0x0,  0x0,  0x50, 0x90, 0xA0, 0x0, 0x0,  0x0,  0x0,  0x00, 0x00, 0x00, 0x0, 0x0,  0x0,  0x0,  0xF,
    0xF,  0x0,  0x0,  0x0,  0xF,  0xF,  0x0, 0x0,  0x02, 0xF,  0xF,  0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0,
    0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0,  0x0,
};
static const uint8_t PART_UPDATE_LUT_DKE[LUT_SIZE_DKE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xa0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x50, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0x05, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t FULL_UPDATE_LUT_DKE[LUT_SIZE_DKE] = {
    0x90, 0x50, 0xa0, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x10, 0xa0, 0xa0, 0x80, 0x00, 0x90, 0x50, 0xa0, 0x50,
    0x50, 0x00, 0x00, 0x00, 0x00, 0x10, 0xa0, 0xa0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17,
    0x04, 0x00, 0x00, 0x00, 0x0b, 0x04, 0x00, 0x00, 0x00, 0x06, 0x05, 0x00, 0x00, 0x00, 0x04, 0x05, 0x00, 0x00,
    0x00, 0x01, 0x0e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void WaveshareEPaper2P9InDKE::initialize() {
  // Hardware reset
  delay(10);
  this->reset_pin_->digital_write(false);
  delayMicroseconds(200);
  this->reset_pin_->digital_write(true);
  delayMicroseconds(200);
  // Wait for busy low
  this->wait_until_idle_();
  // Software reset
  this->command(0x12);
  // Wait for busy low
  this->wait_until_idle_();
  // Set Analog Block Control
  this->command(0x74);
  this->data(0x54);
  // Set Digital Block Control
  this->command(0x7E);
  this->data(0x3B);
  // Set display size and driver output control
  this->command(0x01);
  // this->data(0x27);
  // this->data(0x01);
  // this->data(0x00);
  this->data(this->get_height_internal() - 1);
  this->data((this->get_height_internal() - 1) >> 8);
  this->data(0x00);  // ? GD = 0, SM = 0, TB = 0
  // Ram data entry mode
  this->command(0x11);
  this->data(0x03);
  // Set Ram X address
  this->command(0x44);
  this->data(0x00);
  this->data(0x0F);
  // Set Ram Y address
  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data(0x27);
  this->data(0x01);
  // Set border
  this->command(0x3C);
  // this->data(0x80);
  this->data(0x01);
  // Set VCOM value
  this->command(0x2C);
  this->data(0x26);
  // Gate voltage setting
  this->command(0x03);
  this->data(0x17);
  // Source voltage setting
  this->command(0x04);
  this->data(0x41);
  this->data(0x00);
  this->data(0x32);
  // Frame setting 50hz
  this->command(0x3A);
  this->data(0x30);
  this->command(0x3B);
  this->data(0x0A);
  // Load LUT
  this->command(0x32);
  for (uint8_t v : FULL_UPDATE_LUT_DKE)
    this->data(v);
}

void HOT WaveshareEPaper2P9InDKE::display() {
  ESP_LOGI(TAG, "Performing e-paper update.");
  // Set Ram X address counter
  this->command(0x4e);
  this->data(0);
  // Set Ram Y address counter
  this->command(0x4f);
  this->data(0);
  this->data(0);
  // Load image (128/8*296)
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  // Image update
  this->command(0x22);
  this->data(0xC7);
  this->command(0x20);
  // Wait for busy low
  this->wait_until_idle_();
  // Enter deep sleep mode
  this->command(0x10);
  this->data(0x01);
}
int WaveshareEPaper2P9InDKE::get_width_internal() { return 128; }
int WaveshareEPaper2P9InDKE::get_height_internal() { return 296; }
void WaveshareEPaper2P9InDKE::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in DKE");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void WaveshareEPaper2P9InDKE::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

// ========================================================
//               2.90in Type B (LUT from OTP)
// Datasheet:
//  - https://files.waveshare.com/upload/a/af/2.9inch-e-paper-b-v3-specification.pdf
// ========================================================

void WaveshareEPaper2P9InBV3::initialize() {
  // from https://github.com/waveshareteam/e-Paper/blob/master/Arduino/epd2in9b_V3/epd2in9b_V3.cpp
  this->reset_();

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0F);
  this->data(0x89);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x80);
  this->data(0x01);
  this->data(0x28);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);
}
void HOT WaveshareEPaper2P9InBV3::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  this->command(0x92);
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0xFF);
  this->end_data_();
  this->command(0x92);
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(2);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper2P9InBV3::get_width_internal() { return 128; }
int WaveshareEPaper2P9InBV3::get_height_internal() { return 296; }
void WaveshareEPaper2P9InBV3::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in (B) V3");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               2.90in v2 rev2
// based on SDK and examples in ZIP file from:
// https://www.waveshare.com/pico-epaper-2.9.htm
// ========================================================

void WaveshareEPaper2P9InV2R2::initialize() {
  this->reset_();
  this->wait_until_idle_();

  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  this->command(0x01);
  this->data(0x27);
  this->data(0x01);
  this->data(0x00);

  this->command(0x11);
  this->data(0x03);

  // SetWindows(0, 0, w, h)
  this->command(0x44);
  this->data(0x00);
  this->data(((this->get_width_controller() - 1) >> 3) & 0xFF);

  this->command(0x45);
  this->data(0x00);
  this->data(0x00);
  this->data((this->get_height_internal() - 1) & 0xFF);
  this->data(((this->get_height_internal() - 1) >> 8) & 0xFF);

  this->command(0x21);
  this->data(0x00);
  this->data(0x80);

  // SetCursor(0, 0)
  this->command(0x4E);
  this->data(0x00);
  this->command(0x4f);
  this->data(0x00);
  this->data(0x00);

  this->wait_until_idle_();
}

WaveshareEPaper2P9InV2R2::WaveshareEPaper2P9InV2R2() { this->reset_duration_ = 10; }

void WaveshareEPaper2P9InV2R2::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(reset_duration_);  // NOLINT
    this->reset_pin_->digital_write(true);
    delay(reset_duration_);  // NOLINT
  }
}

void WaveshareEPaper2P9InV2R2::display() {
  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    ESP_LOGE(TAG, "fail idle 1");
    return;
  }

  if (this->full_update_every_ == 1) {
    // do single full update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay
    this->command(0x22);
    this->data(0xF7);
    this->command(0x20);
    return;
  }

  // if (this->full_update_every_ == 1 ||
  if (this->at_update_ == 0) {
    // do base update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay
    this->command(0x22);
    this->data(0xF7);
    this->command(0x20);
  } else {
    // do partial update
    this->reset_();

    this->write_lut_(PARTIAL_UPD_2IN9_LUT, PARTIAL_UPD_2IN9_LUT_SIZE);

    this->command(0x37);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x40);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);

    this->command(0x3C);
    this->data(0x80);

    this->command(0x22);
    this->data(0xC0);
    this->command(0x20);

    if (!this->wait_until_idle_()) {
      ESP_LOGE(TAG, "fail idle 2");
    }

    // SetWindows(0, 0, w, h)
    this->command(0x44);
    this->data(0x00);
    this->data(((this->get_width_controller() - 1) >> 3) & 0xFF);

    this->command(0x45);
    this->data(0x00);
    this->data(0x00);
    this->data((this->get_height_internal() - 1) & 0xFF);
    this->data(((this->get_height_internal() - 1) >> 8) & 0xFF);

    // SetCursor(0, 0)
    this->command(0x4E);
    this->data(0x00);
    this->command(0x4f);
    this->data(0x00);
    this->data(0x00);

    // write b/w
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplayPartial
    this->command(0x22);
    this->data(0x0F);
    this->command(0x20);
  }

  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
}

void WaveshareEPaper2P9InV2R2::write_lut_(const uint8_t *lut, const uint8_t size) {
  // COMMAND WRITE LUT REGISTER
  this->command(0x32);
  for (uint8_t i = 0; i < size; i++)
    this->data(lut[i]);
}

void WaveshareEPaper2P9InV2R2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9inV2R2");
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P9InV2R2::deep_sleep() {
  this->command(0x10);
  this->data(0x01);
}

int WaveshareEPaper2P9InV2R2::get_width_internal() { return 128; }
int WaveshareEPaper2P9InV2R2::get_height_internal() { return 296; }
int WaveshareEPaper2P9InV2R2::get_width_controller() { return this->get_width_internal(); }
void WaveshareEPaper2P9InV2R2::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}
// ========================================================
//     Good Display 2.9in black/white
// Datasheet:
//  - https://files.seeedstudio.com/wiki/Other_Display/29-epaper/GDEY029T94.pdf
//  -
//  https://github.com/Allen-Kuang/e-ink_Demo/blob/main/2.9%20inch%20E-paper%20-%20monocolor%20128x296/example/Display_EPD_W21.cpp
// ========================================================

void GDEY029T94::initialize() {
  // EPD hardware init start
  this->reset_();

  this->wait_until_idle_();
  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  this->command(0x01);  // Driver output control
  this->data((this->get_height_internal() - 1) % 256);
  this->data((this->get_height_internal() - 1) / 256);
  this->data(0x00);

  this->command(0x11);  // data entry mode
  this->data(0x03);

  this->command(0x44);  // set Ram-X address start/end position
  this->data(0x00);
  this->data(this->get_width_internal() / 8 - 1);

  this->command(0x45);  // set Ram-Y address start/end position
  this->data(0x00);
  this->data(0x00);
  this->data((this->get_height_internal() - 1) % 256);
  this->data((this->get_height_internal() - 1) / 256);

  this->command(0x3C);  // BorderWavefrom
  this->data(0x05);

  this->command(0x21);  //  Display update control
  this->data(0x00);
  this->data(0x80);

  this->command(0x18);  // Read built-in temperature sensor
  this->data(0x80);

  this->command(0x4E);  // set RAM x address count to 0;
  this->data(0x00);
  this->command(0x4F);  // set RAM y address count to 0x199;
  this->command(0x00);
  this->command(0x00);
  this->wait_until_idle_();
}
void HOT GDEY029T94::display() {
  this->command(0x24);  // write RAM for black(0)/white (1)
  this->start_data_();
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
    this->write_byte(this->buffer_[i]);
  }
  this->end_data_();
  this->command(0x22);  // Display Update Control
  this->data(0xF7);
  this->command(0x20);  // Activate Display Update Sequence
  this->wait_until_idle_();
}
int GDEY029T94::get_width_internal() { return 128; }
int GDEY029T94::get_height_internal() { return 296; }
void GDEY029T94::dump_config() {
  LOG_DISPLAY("", "E-Paper (Good Display)", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in GDEY029T94");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//     Good Display 2.9in black/white
// Datasheet:
//  - https://v4.cecdn.yun300.cn/100001_1909185148/SSD1680.pdf
//  - https://github.com/adafruit/Adafruit_EPD/blob/master/src/panels/ThinkInk_290_Grayscale4_T5.h
//  - https://github.com/ZinggJM/GxEPD2/blob/master/src/epd/GxEPD2_290_T5.cpp
//  - http://www.e-paper-display.com/GDEW029T5%20V3.1%20Specification5c22.pdf?
// ========================================================

// full screen update LUT
static const uint8_t LUT_20_VCOMDC_29_5[] = {
    0x00, 0x08, 0x00, 0x00, 0x00, 0x02, 0x60, 0x28, 0x28, 0x00, 0x00, 0x01, 0x00, 0x14, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x12, 0x12, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_21_WW_29_5[] = {
    0x40, 0x08, 0x00, 0x00, 0x00, 0x02, 0x90, 0x28, 0x28, 0x00, 0x00, 0x01, 0x40, 0x14,
    0x00, 0x00, 0x00, 0x01, 0xA0, 0x12, 0x12, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_22_BW_29_5[] = {
    0x40, 0x08, 0x00, 0x00, 0x00, 0x02, 0x90, 0x28, 0x28, 0x00, 0x00, 0x01, 0x40, 0x14,
    0x00, 0x00, 0x00, 0x01, 0xA0, 0x12, 0x12, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_23_WB_29_5[] = {
    0x80, 0x08, 0x00, 0x00, 0x00, 0x02, 0x90, 0x28, 0x28, 0x00, 0x00, 0x01, 0x80, 0x14,
    0x00, 0x00, 0x00, 0x01, 0x50, 0x12, 0x12, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_24_BB_29_5[] = {
    0x80, 0x08, 0x00, 0x00, 0x00, 0x02, 0x90, 0x28, 0x28, 0x00, 0x00, 0x01, 0x80, 0x14,
    0x00, 0x00, 0x00, 0x01, 0x50, 0x12, 0x12, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// partial screen update LUT
static const uint8_t LUT_20_VCOMDC_PARTIAL_29_5[] = {
    0x00, 0x20, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_21_WW_PARTIAL_29_5[] = {
    0x00, 0x20, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_22_BW_PARTIAL_29_5[] = {
    0x80, 0x20, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_23_WB_PARTIAL_29_5[] = {
    0x40, 0x20, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_24_BB_PARTIAL_29_5[] = {
    0x00, 0x20, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void GDEW029T5::power_on_() {
  if (!this->power_is_on_) {
    this->command(0x04);
    this->wait_until_idle_();
  }
  this->power_is_on_ = true;
}

void GDEW029T5::power_off_() {
  this->command(0x02);
  this->wait_until_idle_();
  this->power_is_on_ = false;
}

void GDEW029T5::deep_sleep() {
  this->power_off_();
  if (this->deep_sleep_between_updates_) {
    this->command(0x07);  // deep sleep
    this->data(0xA5);     // check code
    ESP_LOGD(TAG, "go to deep sleep");
    this->is_deep_sleep_ = true;
  }
}

void GDEW029T5::init_display_() {
  // from https://github.com/ZinggJM/GxEPD2/blob/master/src/epd/GxEPD2_290_T5.cpp

  // Hardware Initialization
  if (this->deep_sleep_between_updates_ && this->is_deep_sleep_) {
    ESP_LOGI(TAG, "wake up from deep sleep");
    this->reset_();
    this->is_deep_sleep_ = false;
  }

  // COMMAND POWER SETTINGS
  this->command(0x01);
  this->data(0x03);
  this->data(0x00);
  this->data(0x2b);
  this->data(0x2b);
  this->data(0x03); /* for b/w */

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);
  this->data(0x17);
  this->data(0x17);

  this->power_on_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  // 128x296 resolution:        10
  // LUT from register:         1
  // B/W mode (doesn't work):   1
  // scan-up:                   1
  // shift-right:               1
  // booster ON:                1
  // no soft reset:             1
  this->data(0b10111111);
  this->data(0x0d);     // VCOM to 0V fast
  this->command(0x30);  // PLL setting
  this->data(0x3a);     // 3a 100HZ   29 150Hz 39 200HZ 31 171HZ
  this->command(0x61);  // resolution setting
  this->data(this->get_width_internal());
  this->data(this->get_height_internal() >> 8);
  this->data(this->get_height_internal() & 0xFF);

  ESP_LOGD(TAG, "panel setting done");
}

void GDEW029T5::initialize() {
  // from https://www.waveshare.com/w/upload/b/bb/2.9inch-e-paper-b-specification.pdf, page 37
  if (this->reset_pin_ != nullptr)
    this->deep_sleep_between_updates_ = true;

  // old buffer for partial update
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->old_buffer_ = allocator.allocate(this->get_buffer_length_());
  if (this->old_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate old buffer for display!");
    return;
  }
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    this->old_buffer_[i] = 0xFF;
  }
}

// initialize for full(normal) update
void GDEW029T5::init_full_() {
  this->init_display_();
  this->command(0x82);  // vcom_DC setting
  this->data(0x08);
  this->command(0x50);  // VCOM AND DATA INTERVAL SETTING
  this->data(0x97);     // WBmode:VBDF 17|D7 VBDW 97 VBDB 57   WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7
  this->command(0x20);
  this->write_lut_(LUT_20_VCOMDC_29_5, sizeof(LUT_20_VCOMDC_29_5));
  this->command(0x21);
  this->write_lut_(LUT_21_WW_29_5, sizeof(LUT_21_WW_29_5));
  this->command(0x22);
  this->write_lut_(LUT_22_BW_29_5, sizeof(LUT_22_BW_29_5));
  this->command(0x23);
  this->write_lut_(LUT_23_WB_29_5, sizeof(LUT_23_WB_29_5));
  this->command(0x24);
  this->write_lut_(LUT_24_BB_29_5, sizeof(LUT_24_BB_29_5));
  ESP_LOGD(TAG, "initialized full update");
}

// initialzie for partial update
void GDEW029T5::init_partial_() {
  this->init_display_();
  this->command(0x82);  // vcom_DC setting
  this->data(0x08);
  this->command(0x50);  // VCOM AND DATA INTERVAL SETTING
  this->data(0x17);     // WBmode:VBDF 17|D7 VBDW 97 VBDB 57   WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7
  this->command(0x20);
  this->write_lut_(LUT_20_VCOMDC_PARTIAL_29_5, sizeof(LUT_20_VCOMDC_PARTIAL_29_5));
  this->command(0x21);
  this->write_lut_(LUT_21_WW_PARTIAL_29_5, sizeof(LUT_21_WW_PARTIAL_29_5));
  this->command(0x22);
  this->write_lut_(LUT_22_BW_PARTIAL_29_5, sizeof(LUT_22_BW_PARTIAL_29_5));
  this->command(0x23);
  this->write_lut_(LUT_23_WB_PARTIAL_29_5, sizeof(LUT_23_WB_PARTIAL_29_5));
  this->command(0x24);
  this->write_lut_(LUT_24_BB_PARTIAL_29_5, sizeof(LUT_24_BB_PARTIAL_29_5));
  ESP_LOGD(TAG, "initialized partial update");
}

void HOT GDEW029T5::display() {
  bool full_update = this->at_update_ == 0;
  if (full_update) {
    this->init_full_();
  } else {
    this->init_partial_();
    this->command(0x91);  // partial in
    // set partial window
    this->command(0x90);
    // this->data(0);
    this->data(0);
    // this->data(0);
    this->data((this->get_width_internal() - 1) % 256);
    this->data(0);
    this->data(0);
    this->data(((this->get_height_internal() - 1)) / 256);
    this->data(((this->get_height_internal() - 1)) % 256);
    this->data(0x01);
  }
  // input old buffer data
  this->command(0x10);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    this->write_byte(this->old_buffer_[i]);
  }
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (B/W only)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    this->write_byte(this->buffer_[i]);
    this->old_buffer_[i] = this->buffer_[i];
  }
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(2);
  this->wait_until_idle_();

  if (full_update) {
    ESP_LOGD(TAG, "full update done");
  } else {
    this->command(0x92);  // partial out
    ESP_LOGD(TAG, "partial update done");
  }

  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  // COMMAND deep sleep
  this->deep_sleep();
}

void GDEW029T5::write_lut_(const uint8_t *lut, const uint8_t size) {
  // COMMAND WRITE LUT REGISTER
  this->start_data_();
  for (uint8_t i = 0; i < size; i++)
    this->write_byte(lut[i]);
  this->end_data_();
}

void GDEW029T5::set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }

int GDEW029T5::get_width_internal() { return 128; }
int GDEW029T5::get_height_internal() { return 296; }
void GDEW029T5::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper (Good Display)", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.9in Greyscale GDEW029T5");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//     Good Display 1.54in black/white/grey GDEW0154M09
// As used in M5Stack Core Ink
// Datasheet:
//  - https://v4.cecdn.yun300.cn/100001_1909185148/GDEW0154M09-200709.pdf
//  - https://github.com/m5stack/M5Core-Ink
// Reference code from GoodDisplay:
//  - https://github.com/GoodDisplay/E-paper-Display-Library-of-GoodDisplay/
//  -> /Monochrome_E-paper-Display/1.54inch_JD79653_GDEW0154M09_200x200/ESP32-Arduino%20IDE/GDEW0154M09_Arduino.ino
// M5Stack Core Ink spec:
//  - https://docs.m5stack.com/en/core/coreink
// ========================================================

void GDEW0154M09::initialize() {
  this->init_internal_();
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->lastbuff_ = allocator.allocate(this->get_buffer_length_());
  if (this->lastbuff_ != nullptr) {
    memset(this->lastbuff_, 0xff, sizeof(uint8_t) * this->get_buffer_length_());
  }
  this->clear_();
}

void GDEW0154M09::reset_() {
  // RST is inverse from other einks in this project
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void GDEW0154M09::init_internal_() {
  this->reset_();

  // clang-format off
  // 200x200 resolution:        11
  // LUT from OTP:              0
  // B/W mode (doesn't work):   1
  // scan-up:                   1
  // shift-right:               1
  // booster ON:                1
  // no soft reset:             1
  const uint8_t panel_setting_1 = 0b11011111;

  // VCOM status off            0
  // Temp sensing default       1
  // VGL Power Off Floating     1
  // NORG expect refresh        1
  // VCOM Off on displ off      0
  const uint8_t panel_setting_2 = 0b01110;

  const uint8_t wf_t0154_cz_b3_list[] = {
      11, //  11 commands in list
      CMD_PSR_PANEL_SETTING, 2, panel_setting_1, panel_setting_2,
      CMD_UNDOCUMENTED_0x4D, 1, 0x55,
      CMD_UNDOCUMENTED_0xAA, 1, 0x0f,
      CMD_UNDOCUMENTED_0xE9, 1, 0x02,
      CMD_UNDOCUMENTED_0xB6, 1, 0x11,
      CMD_UNDOCUMENTED_0xF3, 1, 0x0a,
      CMD_TRES_RESOLUTION_SETTING, 3, 0xc8, 0x00, 0xc8,
      CMD_TCON_TCONSETTING, 1, 0x00,
      CMD_CDI_VCOM_DATA_INTERVAL, 1, 0xd7,
      CMD_PWS_POWER_SAVING, 1, 0x00,
      CMD_PON_POWER_ON, 0
  };
  // clang-format on

  this->write_init_list_(wf_t0154_cz_b3_list);
  delay(100);  // NOLINT
  this->wait_until_idle_();
}

void GDEW0154M09::write_init_list_(const uint8_t *list) {
  uint8_t list_limit = list[0];
  uint8_t *start_ptr = ((uint8_t *) list + 1);
  for (uint8_t i = 0; i < list_limit; i++) {
    this->command(*(start_ptr + 0));
    for (uint8_t dnum = 0; dnum < *(start_ptr + 1); dnum++) {
      this->data(*(start_ptr + 2 + dnum));
    }
    start_ptr += (*(start_ptr + 1) + 2);
  }
}

void GDEW0154M09::clear_() {
  uint32_t pixsize = this->get_buffer_length_();
  for (uint8_t j = 0; j < 2; j++) {
    this->command(CMD_DTM1_DATA_START_TRANS);
    for (int count = 0; count < pixsize; count++) {
      this->data(0x00);
    }
    this->command(CMD_DTM2_DATA_START_TRANS2);
    for (int count = 0; count < pixsize; count++) {
      this->data(0xff);
    }
    this->command(CMD_DISPLAY_REFRESH);
    delay(10);
    this->wait_until_idle_();
  }
}

void HOT GDEW0154M09::display() {
  this->init_internal_();
  // "Mode 0 display" for now
  this->command(CMD_DTM1_DATA_START_TRANS);
  for (int i = 0; i < this->get_buffer_length_(); i++) {
    this->data(0xff);
  }
  this->command(CMD_DTM2_DATA_START_TRANS2);  // write 'new' data to SRAM
  for (int i = 0; i < this->get_buffer_length_(); i++) {
    this->data(this->buffer_[i]);
  }
  this->command(CMD_DISPLAY_REFRESH);
  delay(10);
  this->wait_until_idle_();
  this->deep_sleep();
}

void GDEW0154M09::deep_sleep() {
  // COMMAND DEEP SLEEP
  this->command(CMD_POF_POWER_OFF);
  this->wait_until_idle_();
  delay(1000);  // NOLINT
  this->command(CMD_DSLP_DEEP_SLEEP);
  this->data(DATA_DSLP_DEEP_SLEEP);
}

int GDEW0154M09::get_width_internal() { return 200; }
int GDEW0154M09::get_height_internal() { return 200; }
void GDEW0154M09::dump_config() {
  LOG_DISPLAY("", "M5Stack CoreInk E-Paper (Good Display)", this);
  ESP_LOGCONFIG(TAG, "  Model: 1.54in Greyscale GDEW0154M09");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//     Good Display 4.2in black/white GDEY042T81 (SSD1683)
// Product page:
//  - https://www.good-display.com/product/386.html
// Datasheet:
//  - https://v4.cecdn.yun300.cn/100001_1909185148/GDEY042T81.pdf
//  - https://v4.cecdn.yun300.cn/100001_1909185148/SSD1683.PDF
// Reference code from GoodDisplay:
//  - https://www.good-display.com/companyfile/1572.html (2024-08-01 15:40:41)
// Other reference code:
//  - https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp
// ========================================================

void GDEY042T81::initialize() {
  this->init_display_();
  ESP_LOGD(TAG, "Initialization complete, set the display to deep sleep");
  this->deep_sleep();
}

// conflicting documentation / examples regarding reset timings
//   https://v4.cecdn.yun300.cn/100001_1909185148/SSD1683.PDF -> 10ms
//   GD sample code (Display_EPD_W21.cpp, see above) -> 10 ms
//   https://v4.cecdn.yun300.cn/100001_1909185148/GDEY042T81.pdf (section 14.2) -> 0.2ms (200us)
//   https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp#L351
//   -> 10ms
//  10 ms seems to work, so we use this
GDEY042T81::GDEY042T81() { this->reset_duration_ = 10; }

void GDEY042T81::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(reset_duration_);  // NOLINT
    this->reset_pin_->digital_write(true);
    delay(reset_duration_);  // NOLINT
  }
}

void GDEY042T81::init_display_() {
  this->reset_();

  this->wait_until_idle_();
  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  // Specify number of lines for the driver: 300 (MUX 300)
  // https://v4.cecdn.yun300.cn/100001_1909185148/SSD1683.PDF (section 8.1)
  // https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp#L354
  this->command(0x01);  //  driver output control
  this->data(0x2B);     // (height - 1) % 256
  this->data(0x01);     // (height - 1) / 256
  this->data(0x00);

  // https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp#L360
  this->command(0x3C);  // BorderWaveform
  this->data(0x01);
  this->command(0x18);  // Read built-in temperature sensor
  this->data(0x80);

  // GD sample code (Display_EPD_W21.cpp@90ff)
  this->command(0x11);  // data entry mode
  this->data(0x03);
  // set windows (0,0,400,300)
  this->command(0x44);  // set Ram-X address start/end position
  this->data(0);
  this->data(0x31);  // (width / 8 -1)

  this->command(0x45);  //  set Ram-y address start/end position
  this->data(0);
  this->data(0);
  this->data(0x2B);  // (height - 1) % 256
  this->data(0x01);  // (height - 1) / 256

  // set cursor (0,0)
  this->command(0x4E);  // set RAM x address count to 0;
  this->data(0);
  this->command(0x4F);  // set RAM y address count to 0;
  this->data(0);
  this->data(0);

  this->wait_until_idle_();
}

// https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp#L366
void GDEY042T81::update_full_() {
  this->command(0x21);  // display update control
  this->data(0x40);     // bypass RED as 0
  this->data(0x00);     // single chip application

  // only ever do a fast update because slow updates are only relevant
  // for lower operating temperatures
  // see
  // https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_290_GDEY029T94.h#L30
  //
  // Should slow/fast updates be made configurable similar to how GxEPD2 does it? No idea if anyone would need it...
  this->command(0x1A);  // Write to temperature register
  this->data(0x6E);
  this->command(0x22);
  this->data(0xd7);

  this->command(0x20);
  this->wait_until_idle_();
}

// https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp#L389
void GDEY042T81::update_part_() {
  this->command(0x21);  // display update control
  this->data(0x00);     // RED normal
  this->data(0x00);     // single chip application

  this->command(0x22);
  this->data(0xfc);

  this->command(0x20);
  this->wait_until_idle_();
}

void HOT GDEY042T81::display() {
  ESP_LOGD(TAG, "Wake up the display");
  this->init_display_();

  if (!this->wait_until_idle_()) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Failed to perform update, display is busy");
    return;
  }

  // basic code structure copied from WaveshareEPaper2P9InV2R2
  if (this->full_update_every_ == 1) {
    ESP_LOGD(TAG, "Full update");
    // do single full update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay
    this->update_full_();
    return;
  }

  // if (this->full_update_every_ == 1 ||
  if (this->at_update_ == 0) {
    ESP_LOGD(TAG, "Update");
    // do base update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    this->command(0x26);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay;
    this->update_full_();
  } else {
    // do partial update (full screen)
    // no need to load a LUT for GoodDisplays as they seem to have the LUT onboard
    // GD example code (Display_EPD_W21.cpp@283ff)
    //
    // not setting the BorderWaveform here again (contrary to the GD example) because according to
    // https://github.com/ZinggJM/GxEPD2/blob/03d8e7a533c1493f762e392ead12f1bcb7fab8f9/src/gdey/GxEPD2_420_GDEY042T81.cpp#L358
    // it seems to be enough to set it during display initialization
    ESP_LOGD(TAG, "Partial update");
    this->reset_();
    if (!this->wait_until_idle_()) {
      this->status_set_warning();
      ESP_LOGE(TAG, "Failed to perform partial update, display is busy");
      return;
    }

    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // TurnOnDisplay
    this->update_part_();
  }

  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
  this->wait_until_idle_();
  ESP_LOGD(TAG, "Set the display back to deep sleep");
  this->deep_sleep();
}
void GDEY042T81::set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }
int GDEY042T81::get_width_internal() { return 400; }
int GDEY042T81::get_height_internal() { return 300; }
uint32_t GDEY042T81::idle_timeout_() { return 5000; }
void GDEY042T81::dump_config() {
  LOG_DISPLAY("", "GoodDisplay E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in B/W GDEY042T81");
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

static const uint8_t LUT_VCOM_DC_4_2[] = {
    0x00, 0x17, 0x00, 0x00, 0x00, 0x02, 0x00, 0x17, 0x17, 0x00, 0x00, 0x02, 0x00, 0x0A, 0x01,
    0x00, 0x00, 0x01, 0x00, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t LUT_WHITE_TO_WHITE_4_2[] = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x40, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t LUT_BLACK_TO_WHITE_4_2[] = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x40, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_BLACK_TO_BLACK_4_2[] = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x80, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_WHITE_TO_BLACK_4_2[] = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02, 0x90, 0x17, 0x17, 0x00, 0x00, 0x02, 0x80, 0x0A,
    0x01, 0x00, 0x00, 0x01, 0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void WaveshareEPaper4P2In::initialize() {
  // https://www.waveshare.com/w/upload/7/7f/4.2inch-e-paper-b-specification.pdf - page 8

  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x03);  // VDS_EN, VDG_EN
  this->data(0x00);  // VCOM_HV, VGHL_LV[1], VGHL_LV[0]
  this->data(0x2B);  // VDH
  this->data(0x2B);  // VDL
  this->data(0xFF);  // VDHR

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);  // PHA
  this->data(0x17);  // PHB
  this->data(0x17);  // PHC

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xBF);  // KW-BF   KWR-AF  BWROTP 0f
  this->data(0x0B);
  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);  // 3A 100HZ   29 150Hz 39 200HZ  31 171HZ

  delay(2);
  // COMMAND LUT FOR VCOM
  this->command(0x20);
  for (uint8_t i : LUT_VCOM_DC_4_2)
    this->data(i);
  // COMMAND LUT WHITE TO WHITE
  this->command(0x21);
  for (uint8_t i : LUT_WHITE_TO_WHITE_4_2)
    this->data(i);
  // COMMAND LUT BLACK TO WHITE
  this->command(0x22);
  for (uint8_t i : LUT_BLACK_TO_WHITE_4_2)
    this->data(i);
  // COMMAND LUT WHITE TO BLACK
  this->command(0x23);
  for (uint8_t i : LUT_WHITE_TO_BLACK_4_2)
    this->data(i);
  // COMMAND LUT BLACK TO BLACK
  this->command(0x24);
  for (uint8_t i : LUT_BLACK_TO_BLACK_4_2)
    this->data(i);
}
void HOT WaveshareEPaper4P2In::display() {
  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x01);
  this->data(0x90);
  this->data(0x01);
  this->data(0x2C);

  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x12);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x97);

  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);
  // COMMAND DATA START TRANSMISSION 2
  this->command(0x13);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper4P2In::get_width_internal() { return 400; }
int WaveshareEPaper4P2In::get_height_internal() { return 300; }
void WaveshareEPaper4P2In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               4.20in Type B (LUT from OTP)
// Datasheet:
//  - https://www.waveshare.com/w/upload/2/20/4.2inch-e-paper-module-user-manual-en.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in2b_V2.c
// ========================================================
void WaveshareEPaper4P2InBV2::initialize() {
  // these exact timings are required for a proper reset/init
  this->reset_pin_->digital_write(false);
  delay(2);
  this->reset_pin_->digital_write(true);
  delay(200);  // NOLINT

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0f);  // LUT from OTP
}

void HOT WaveshareEPaper4P2InBV2::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0xFF);
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper4P2InBV2::get_width_internal() { return 400; }
int WaveshareEPaper4P2InBV2::get_height_internal() { return 300; }
void WaveshareEPaper4P2InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in (B V2)");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//    4.20in Type B With Red colour support (LUT from OTP)
// Datasheet:
//  - https://www.waveshare.com/w/upload/2/20/4.2inch-e-paper-module-user-manual-en.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in2b_V2.c
// The implementation is an adaptation of WaveshareEPaper4P2InBV2 class
// ========================================================
void WaveshareEPaper4P2InBV2BWR::initialize() {
  // these exact timings are required for a proper reset/init
  this->reset_pin_->digital_write(false);
  delay(2);
  this->reset_pin_->digital_write(true);
  delay(200);  // NOLINT

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0f);  // LUT from OTP
}

void HOT WaveshareEPaper4P2InBV2BWR::display() {
  const uint32_t buf_len = this->get_buffer_length_() / 2u;

  this->command(0x10);  // Send BW data Transmission
  delay(2);             // Delay to prevent Watchdog error
  for (uint32_t i = 0; i < buf_len; ++i) {
    this->data(this->buffer_[i]);
  }

  this->command(0x13);  // Send red data Transmission
  delay(2);             // Delay to prevent Watchdog error
  for (uint32_t i = 0; i < buf_len; ++i) {
    // Red color need to flip bit from the buffer. Otherwise, red will conqure the screen!
    this->data(~this->buffer_[buf_len + i]);
  }

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  // NOTE: power off < deep sleep
  this->command(0x02);
}
int WaveshareEPaper4P2InBV2BWR::get_width_internal() { return 400; }
int WaveshareEPaper4P2InBV2BWR::get_height_internal() { return 300; }
void WaveshareEPaper4P2InBV2BWR::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 4.2in (B V2) BWR-Mode");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper5P8In::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x37);
  this->data(0x00);

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xCF);
  this->data(0x0B);

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x28);

  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);

  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);

  // COMMAND TEMPERATURE SENSOR CALIBRATION
  this->command(0x41);
  this->data(0x00);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);
  this->data(0x58);
  this->data(0x01);
  this->data(0xC0);

  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x1E);

  this->command(0xE5);
  this->data(0x03);
}
void HOT WaveshareEPaper5P8In::display() {
  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);

  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    uint8_t temp1 = this->buffer_[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t temp2;
      if (temp1 & 0x80) {
        temp2 = 0x03;
      } else {
        temp2 = 0x00;
      }

      temp2 <<= 4;
      temp1 <<= 1;
      j++;
      if (temp1 & 0x80) {
        temp2 |= 0x03;
      } else {
        temp2 |= 0x00;
      }
      temp1 <<= 1;
      this->write_byte(temp2);
    }

    App.feed_wdt();
  }
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper5P8In::get_width_internal() { return 600; }
int WaveshareEPaper5P8In::get_height_internal() { return 448; }
void WaveshareEPaper5P8In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 5.83in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//               5.83in V2
// Datasheet/Specification/Reference:
//  - https://www.waveshare.com/w/upload/3/37/5.83inch_e-Paper_V2_Specification.pdf
//  - https://github.com/waveshare/e-Paper/blob/master/Arduino/epd5in83_V2/epd5in83_V2.cpp
// ========================================================
void WaveshareEPaper5P8InV2::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);
  this->data(0x3f);
  this->data(0x3f);

  // COMMAND POWER ON
  this->command(0x04);
  delay(10);
  this->wait_until_idle_();

  // PANNEL SETTING
  this->command(0x00);
  this->data(0x1F);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);
  this->data(0x88);
  this->data(0x01);
  this->data(0xE0);

  this->command(0x15);
  this->data(0x00);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // Do we need this?
  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);  // 3A 100HZ   29 150Hz 39 200HZ  31 171HZ
}
void HOT WaveshareEPaper5P8InV2::display() {
  // Reuse the code from WaveshareEPaper4P2In::display()
  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x12);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x97);

  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2
  this->command(0x13);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper5P8InV2::get_width_internal() { return 648; }
int WaveshareEPaper5P8InV2::get_height_internal() { return 480; }
void WaveshareEPaper5P8InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 5.83inv2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// ========================================================
//     Good Display 5.83in black/white GDEY0583T81
// Product page:
//  - https://www.good-display.com/product/440.html
//  - https://www.seeedstudio.com/5-83-Monochrome-ePaper-Display-with-648x480-Pixels-p-5785.html
// Datasheet:
//  -
//  https://www.good-display.com/public/html/pdfjs/viewer/viewernew.html?file=https://v4.cecdn.yun300.cn/100001_1909185148/GDEY0583T81-new.pdf
//  - https://v4.cecdn.yun300.cn/100001_1909185148/GDEY0583T81-new.pdf
// Reference code from GoodDisplay:
//  - https://www.good-display.com/companyfile/903.html
// ========================================================

void GDEY0583T81::initialize() {
  // Allocate buffer for old data for partial updates
  RAMAllocator<uint8_t> allocator{};
  this->old_buffer_ = allocator.allocate(this->get_buffer_length_());
  if (this->old_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate old buffer for display!");
    return;
  }
  memset(this->old_buffer_, 0xFF, this->get_buffer_length_());

  this->init_full_();

  this->wait_until_idle_();

  this->deep_sleep();
}

void GDEY0583T81::power_on_() {
  if (!this->power_is_on_) {
    this->command(0x04);
    this->wait_until_idle_();
  }
  this->power_is_on_ = true;
  this->is_deep_sleep_ = false;
}

void GDEY0583T81::power_off_() {
  this->command(0x02);
  this->wait_until_idle_();
  this->power_is_on_ = false;
}

void GDEY0583T81::deep_sleep() {
  if (this->is_deep_sleep_) {
    return;
  }

  // VCOM and data interval setting (CDI)
  this->command(0x50);
  this->data(0xf7);

  this->power_off_();
  delay(10);

  // Deep sleep (DSLP)
  this->command(0x07);
  this->data(0xA5);
  this->is_deep_sleep_ = true;
}

void GDEY0583T81::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

// Initialize for full screen update in fast mode
void GDEY0583T81::init_full_() {
  this->init_display_();

  // Based on the GD sample code
  // VCOM and data interval setting (CDI)
  this->command(0x50);
  this->data(0x29);
  this->data(0x07);

  // Cascade Setting (CCSET)
  this->command(0xE0);
  this->data(0x02);

  // Force Temperature (TSSET)
  this->command(0xE5);
  this->data(0x5A);
}

// Initialize for a partial update of the full screen
void GDEY0583T81::init_partial_() {
  this->init_display_();

  // Cascade Setting (CCSET)
  this->command(0xE0);
  this->data(0x02);

  // Force Temperature (TSSET)
  this->command(0xE5);
  this->data(0x6E);
}

void GDEY0583T81::init_display_() {
  this->reset_();

  // Panel Setting (PSR)
  this->command(0x00);
  // Sets: REG=0, LUT from OTP (set by CDI)
  // KW/R=1, Sets KW mode (Black/White)
  //         as opposed to the default KWR mode (Black/White/Red)
  // UD=1, Gate Scan Direction, 1 = up (default)
  // SHL=1, Source Shift Direction, 1 = right (default)
  // SHD_N=1, Booster Switch, 1 = ON (default)
  // RST_N=1, Soft reset, 1 = No effect (default)
  this->data(0x1F);

  // Resolution setting (TRES)
  this->command(0x61);

  // Horizontal display resolution (HRES)
  this->data(get_width_internal() / 256);
  this->data(get_width_internal() % 256);

  // Vertical display resolution (VRES)
  this->data(get_height_internal() / 256);
  this->data(get_height_internal() % 256);

  this->power_on_();
}

void HOT GDEY0583T81::display() {
  bool full_update = this->at_update_ == 0;
  if (full_update) {
    this->init_full_();
  } else {
    this->init_partial_();

    // VCOM and data interval setting (CDI)
    this->command(0x50);
    this->data(0xA9);
    this->data(0x07);

    // Partial In (PTIN), makes the display enter partial mode
    this->command(0x91);

    // Partial Window (PTL)
    //  We use the full screen as the window
    this->command(0x90);

    // Horizontal start/end channel bank (HRST/HRED)
    this->data(0);
    this->data(0);
    this->data((get_width_internal() - 1) / 256);
    this->data((get_width_internal() - 1) % 256);

    // Vertical start/end line (VRST/VRED)
    this->data(0);
    this->data(0);
    this->data((get_height_internal() - 1) / 256);
    this->data((get_height_internal() - 1) % 256);

    this->data(0x01);

    // Display Start Transmission 1 (DTM1)
    //  in KW mode this writes "OLD" data to SRAM
    this->command(0x10);
    this->start_data_();
    this->write_array(this->old_buffer_, this->get_buffer_length_());
    this->end_data_();
  }

  // Display Start Transmission 2 (DTM2)
  //  in KW mode this writes "NEW" data to SRAM
  this->command(0x13);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    this->old_buffer_[i] = this->buffer_[i];
  }

  // Display Refresh (DRF)
  this->command(0x12);
  delay(10);
  this->wait_until_idle_();

  if (full_update) {
    ESP_LOGD(TAG, "Full update done");
  } else {
    // Partial out (PTOUT), makes the display exit partial mode
    this->command(0x92);
    ESP_LOGD(TAG, "Partial update done, next full update after %d cycles",
             this->full_update_every_ - this->at_update_ - 1);
  }

  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  this->deep_sleep();
}

void GDEY0583T81::set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }
int GDEY0583T81::get_width_internal() { return 648; }
int GDEY0583T81::get_height_internal() { return 480; }
uint32_t GDEY0583T81::idle_timeout_() { return 5000; }
void GDEY0583T81::dump_config() {
  LOG_DISPLAY("", "GoodDisplay E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 5.83in B/W GDEY0583T81");
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InBV2::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);  // VGH=20V,VGL=-20V
  this->data(0x3f);  // VDH=15V
  this->data(0x3f);  // VDL=-15V
  // COMMAND POWER ON
  this->command(0x04);
  delay(100);  // NOLINT
  this->wait_until_idle_();
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0F);     // KW3f, KWR-2F, BWROTP 0f, BWOTP 1f
  this->command(0x61);  // tres
  this->data(0x03);     // 800px
  this->data(0x20);
  this->data(0x01);  // 400px
  this->data(0xE0);
  this->command(0x15);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x11);
  this->data(0x07);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  this->command(0x82);
  this->data(0x08);
  this->command(0x30);
  this->data(0x06);

  // COMMAND RESOLUTION SETTING
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);
}
void HOT WaveshareEPaper7P5InBV2::display() {
  // COMMAND DATA START TRANSMISSION 1 (B/W data)
  this->command(0x10);
  delay(2);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();
  delay(2);

  // COMMAND DATA START TRANSMISSION 2 (RED data)
  this->command(0x13);
  delay(2);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();
  delay(2);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(100);  // NOLINT
  this->wait_until_idle_();
  this->deep_sleep();
}
int WaveshareEPaper7P5InBV2::get_width_internal() { return 800; }
int WaveshareEPaper7P5InBV2::get_height_internal() { return 480; }
void WaveshareEPaper7P5InBV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bv2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InBV3::initialize() { this->init_display_(); }
bool WaveshareEPaper7P5InBV3::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGI(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  delay(200);  // NOLINT
  return true;
};
void WaveshareEPaper7P5InBV3::init_display_() {
  this->reset_();

  // COMMAND POWER SETTING
  this->command(0x01);

  // 1-0=11: internal power
  this->data(0x07);
  this->data(0x17);  // VGH&VGL
  this->data(0x3F);  // VSH
  this->data(0x26);  // VSL
  this->data(0x11);  // VSHR

  // VCOM DC Setting
  this->command(0x82);
  this->data(0x24);  // VCOM

  // Booster Setting
  this->command(0x06);
  this->data(0x27);
  this->data(0x27);
  this->data(0x2F);
  this->data(0x17);

  // POWER ON
  this->command(0x04);

  delay(100);  // NOLINT
  this->wait_until_idle_();
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x3F);  // KW-3f   KWR-2F BWROTP 0f BWOTP 1f

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);  // source 800
  this->data(0x20);
  this->data(0x01);  // gate 480
  this->data(0xE0);
  // COMMAND ...?
  this->command(0x15);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x00);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);
  // Resolution setting
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);

  uint8_t lut_vcom_7_i_n5_v2[] = {
      0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_ww_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bw_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_wb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x3, 0x84, 0xF, 0x1, 0xF, 0x1, 0x4, 0x40, 0xF, 0xF, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t count;
  this->command(0x20);  // VCOM
  for (count = 0; count < 42; count++)
    this->data(lut_vcom_7_i_n5_v2[count]);

  this->command(0x21);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_ww_7_i_n5_v2[count]);

  this->command(0x22);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_bw_7_i_n5_v2[count]);

  this->command(0x23);  // LUTWB
  for (count = 0; count < 42; count++)
    this->data(lut_wb_7_i_n5_v2[count]);

  this->command(0x24);  // LUTBB
  for (count = 0; count < 42; count++)
    this->data(lut_bb_7_i_n5_v2[count]);
};
void HOT WaveshareEPaper7P5InBV3::display() {
  this->init_display_();
  uint32_t buf_len = this->get_buffer_length_();

  this->command(0x10);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(0xFF);
  }

  this->command(0x13);  // Start Transmission
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(~this->buffer_[i]);
  }

  this->command(0x12);  // Display Refresh
  delay(100);           // NOLINT
  this->wait_until_idle_();
  this->deep_sleep();
}
int WaveshareEPaper7P5InBV3::get_width_internal() { return 800; }
int WaveshareEPaper7P5InBV3::get_height_internal() { return 480; }
void WaveshareEPaper7P5InBV3::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bv3");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InBV3BWR::initialize() { this->init_display_(); }
bool WaveshareEPaper7P5InBV3BWR::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGI(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  delay(200);  // NOLINT
  return true;
};
void WaveshareEPaper7P5InBV3BWR::init_display_() {
  this->reset_();

  // COMMAND POWER SETTING
  this->command(0x01);

  // 1-0=11: internal power
  this->data(0x07);  // VRS_EN=1, VS_EN=1, VG_EN=1
  this->data(0x17);  // VGH&VGL ??? VCOM_SLEW=1 but this is fixed, VG_LVL[2:0]=111 => VGH=20V VGL=-20V, it could be 0x07
  this->data(0x3F);  // VSH=15V?
  this->data(0x26);  // VSL=-9.4V?
  this->data(0x11);  // VSHR=5.8V?

  // VCOM DC Setting
  this->command(0x82);
  this->data(0x24);  // VCOM=-1.9V

  // POWER ON
  this->command(0x04);
  delay(100);  // NOLINT
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x0F);  // KW-3f   KWR-2F BWROTP 0f BWOTP 1f

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);  // source 800
  this->data(0x20);
  this->data(0x01);  // gate 480
  this->data(0xE0);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x20);
  this->data(0x00);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // Resolution setting
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);
};
void HOT WaveshareEPaper7P5InBV3BWR::display() {
  this->init_display_();
  const uint32_t buf_len = this->get_buffer_length_() / 2u;

  this->command(0x10);  // Send BW data Transmission
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i]);
  }

  this->command(0x13);  // Send red data Transmission
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(this->buffer_[i + buf_len]);
  }

  this->command(0x12);  // Display Refresh
  delay(100);           // NOLINT
  this->wait_until_idle_();
  this->deep_sleep();
}
int WaveshareEPaper7P5InBV3BWR::get_width_internal() { return 800; }
int WaveshareEPaper7P5InBV3BWR::get_height_internal() { return 480; }
void WaveshareEPaper7P5InBV3BWR::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bv3 BWR-Mode");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5In::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x37);
  this->data(0x00);
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xCF);
  this->data(0x0B);
  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x28);
  // COMMAND POWER ON
  this->command(0x04);
  this->wait_until_idle_();
  delay(10);
  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3C);
  // COMMAND TEMPERATURE SENSOR CALIBRATION
  this->command(0x41);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);
  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);
  this->data(0x80);
  this->data(0x01);
  this->data(0x80);
  // COMMAND VCM DC SETTING REGISTER
  this->command(0x82);
  this->data(0x1E);
  this->command(0xE5);
  this->data(0x03);
}
void HOT WaveshareEPaper7P5In::display() {
  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    uint8_t temp1 = this->buffer_[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t temp2;
      if (temp1 & 0x80) {
        temp2 = 0x03;
      } else {
        temp2 = 0x00;
      }
      temp2 <<= 4;
      temp1 <<= 1;
      j++;
      if (temp1 & 0x80) {
        temp2 |= 0x03;
      } else {
        temp2 |= 0x00;
      }
      temp1 <<= 1;
      this->write_byte(temp2);
    }
    App.feed_wdt();
  }
  this->end_data_();
  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}
int WaveshareEPaper7P5In::get_width_internal() { return 640; }
int WaveshareEPaper7P5In::get_height_internal() { return 384; }
void WaveshareEPaper7P5In::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

// Waveshare 5.65F ========================================================

namespace cmddata_5P65InF {
// WaveshareEPaper5P65InF commands
// https://www.waveshare.com/wiki/5.65inch_e-Paper_Module_(F)

// R00H (PSR): Panel setting Register
// UD(1): scan up
// SHL(1) shift right
// SHD_N(1) DC-DC on
// RST_N(1) no reset
static const uint8_t R00_CMD_PSR[] = {0x00, 0xEF, 0x08};

// R01H (PWR): Power setting Register
// internal DC-DC power generation
static const uint8_t R01_CMD_PWR[] = {0x01, 0x07, 0x00, 0x00, 0x00};

// R02H (POF): Power OFF Command
static const uint8_t R02_CMD_POF[] = {0x02};

// R03H (PFS): Power off sequence setting Register
// T_VDS_OFF (00) = 1 frame
static const uint8_t R03_CMD_PFS[] = {0x03, 0x00};

// R04H (PON): Power ON Command
static const uint8_t R04_CMD_PON[] = {0x04};

// R06h (BTST): Booster Soft Start
static const uint8_t R06_CMD_BTST[] = {0x06, 0xC7, 0xC7, 0x1D};

// R07H (DSLP): Deep sleep#
// Note Documentation @  Waveshare shows cmd code as 0x10 in table, but
// 0x10 is DTM1.
static const uint8_t R07_CMD_DSLP[] = {0x07, 0xA5};

// R10H (DTM1): Data Start Transmission 1

static const uint8_t R10_CMD_DTM1[] = {0x10};

// R11H (DSP): Data Stop
static const uint8_t R11_CMD_DSP[] = {0x11};

// R12H (DRF): Display Refresh
static const uint8_t R12_CMD_DRF[] = {0x12};

// R13H (IPC): Image Process Command
static const uint8_t R13_CMD_IPC[] = {0x13, 0x00};

// R30H (PLL): PLL Control
// 0x3C = 50Hz
static const uint8_t R30_CMD_PLL[] = {0x30, 0x3C};

// R41H (TSE): Temperature Sensor Enable
// TSE(0) enable, TO(0000) +0 degree offset
static const uint8_t R41_CMD_TSE[] = {0x41, 0x00};

// R50H (CDI) VCOM and Data interval setting
// CDI(0111) 10
// DDX(1), VBD(001) Border output "White"
static const uint8_t R50_CMD_CDI[] = {0x50, 0x37};

// R60H (TCON) Gate and Source non overlap period command
// S2G(10) 12 units
// G2S(10) 12 units
static const uint8_t R60_CMD_TCON[] = {0x60, 0x22};

// R61H (TRES) Resolution Setting
// 0x258 = 600
// 0x1C0 = 448
static const uint8_t R61_CMD_TRES[] = {0x61, 0x02, 0x58, 0x01, 0xC0};

// RE3H (PWS) Power Savings
static const uint8_t RE3_CMD_PWS[] = {0xE3, 0xAA};
}  // namespace cmddata_5P65InF

void WaveshareEPaper5P65InF::initialize() {
  if (this->buffers_[0] == nullptr) {
    ESP_LOGE(TAG, "Buffer unavailable!");
    return;
  }

  this->reset_();
  delay(20);
  this->wait_until_(IDLE);

  using namespace cmddata_5P65InF;

  this->cmd_data(R00_CMD_PSR, sizeof(R00_CMD_PSR));
  this->cmd_data(R01_CMD_PWR, sizeof(R01_CMD_PWR));
  this->cmd_data(R03_CMD_PFS, sizeof(R03_CMD_PFS));
  this->cmd_data(R06_CMD_BTST, sizeof(R06_CMD_BTST));
  this->cmd_data(R30_CMD_PLL, sizeof(R30_CMD_PLL));
  this->cmd_data(R41_CMD_TSE, sizeof(R41_CMD_TSE));
  this->cmd_data(R50_CMD_CDI, sizeof(R50_CMD_CDI));
  this->cmd_data(R60_CMD_TCON, sizeof(R60_CMD_TCON));
  this->cmd_data(R61_CMD_TRES, sizeof(R61_CMD_TRES));
  this->cmd_data(RE3_CMD_PWS, sizeof(RE3_CMD_PWS));

  delay(100);  // NOLINT
  this->cmd_data(R50_CMD_CDI, sizeof(R50_CMD_CDI));

  ESP_LOGI(TAG, "Display initialized successfully");
}

void HOT WaveshareEPaper5P65InF::display() {
  // INITIALIZATION
  ESP_LOGI(TAG, "Initialise the display");
  this->initialize();

  using namespace cmddata_5P65InF;

  // COMMAND DATA START TRANSMISSION
  ESP_LOGI(TAG, "Sending data to the display");
  this->cmd_data(R61_CMD_TRES, sizeof(R61_CMD_TRES));
  this->cmd_data(R10_CMD_DTM1, sizeof(R10_CMD_DTM1));
  this->send_buffers_();

  // COMMAND POWER ON
  ESP_LOGI(TAG, "Power on the display");
  this->cmd_data(R04_CMD_PON, sizeof(R04_CMD_PON));
  this->wait_until_(IDLE);

  // COMMAND REFRESH SCREEN
  ESP_LOGI(TAG, "Refresh the display");
  this->cmd_data(R12_CMD_DRF, sizeof(R12_CMD_DRF));
  this->wait_until_(IDLE);

  // COMMAND POWER OFF
  ESP_LOGI(TAG, "Power off the display");
  this->cmd_data(R02_CMD_POF, sizeof(R02_CMD_POF));
  this->wait_until_(BUSY);

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Set the display to deep sleep");
    this->cmd_data(R07_CMD_DSLP, sizeof(R07_CMD_DSLP));
  }
}

int WaveshareEPaper5P65InF::get_width_internal() { return 600; }
int WaveshareEPaper5P65InF::get_height_internal() { return 448; }
uint32_t WaveshareEPaper5P65InF::idle_timeout_() { return 35000; }

void WaveshareEPaper5P65InF::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 5.65in-F");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

bool WaveshareEPaper5P65InF::wait_until_(WaitForState busy_state) {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (busy_state != this->busy_pin_->digital_read()) {
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  return true;
}

void WaveshareEPaper7P3InF::initialize() {
  if (this->buffers_[0] == nullptr) {
    ESP_LOGE(TAG, "Buffer unavailable!");
    return;
  }

  this->reset_();
  delay(20);
  this->wait_until_idle_();

  // COMMAND CMDH
  this->command(0xAA);
  this->data(0x49);
  this->data(0x55);
  this->data(0x20);
  this->data(0x08);
  this->data(0x09);
  this->data(0x18);

  this->command(0x01);
  this->data(0x3F);
  this->data(0x00);
  this->data(0x32);
  this->data(0x2A);
  this->data(0x0E);
  this->data(0x2A);

  this->command(0x00);
  this->data(0x5F);
  this->data(0x69);

  this->command(0x03);
  this->data(0x00);
  this->data(0x54);
  this->data(0x00);
  this->data(0x44);

  this->command(0x05);
  this->data(0x40);
  this->data(0x1F);
  this->data(0x1F);
  this->data(0x2C);

  this->command(0x06);
  this->data(0x6F);
  this->data(0x1F);
  this->data(0x1F);
  this->data(0x22);

  this->command(0x08);
  this->data(0x6F);
  this->data(0x1F);
  this->data(0x1F);
  this->data(0x22);

  // COMMAND IPC
  this->command(0x13);
  this->data(0x00);
  this->data(0x04);

  this->command(0x30);
  this->data(0x3C);

  // COMMAND TSE
  this->command(0x41);
  this->data(0x00);

  this->command(0x50);
  this->data(0x3F);

  this->command(0x60);
  this->data(0x02);
  this->data(0x00);

  this->command(0x61);
  this->data(0x03);
  this->data(0x20);
  this->data(0x01);
  this->data(0xE0);

  this->command(0x82);
  this->data(0x1E);

  this->command(0x84);
  this->data(0x00);

  // COMMAND AGID
  this->command(0x86);
  this->data(0x00);

  this->command(0xE3);
  this->data(0x2F);

  // COMMAND CCSET
  this->command(0xE0);
  this->data(0x00);

  // COMMAND TSSET
  this->command(0xE6);
  this->data(0x00);

  ESP_LOGI(TAG, "Display initialized successfully");
}
void HOT WaveshareEPaper7P3InF::display() {
  // INITIALIZATION
  ESP_LOGI(TAG, "Initialise the display");
  this->initialize();

  // COMMAND DATA START TRANSMISSION
  ESP_LOGI(TAG, "Sending data to the display");
  this->command(0x10);
  this->send_buffers_();

  // COMMAND POWER ON
  ESP_LOGI(TAG, "Power on the display");
  this->command(0x04);
  this->wait_until_idle_();

  // COMMAND REFRESH SCREEN
  ESP_LOGI(TAG, "Refresh the display");
  this->command(0x12);
  this->data(0x00);
  this->wait_until_idle_();

  // COMMAND POWER OFF
  ESP_LOGI(TAG, "Power off the display");
  this->command(0x02);
  this->data(0x00);
  this->wait_until_idle_();

  if (this->deep_sleep_between_updates_) {
    ESP_LOGI(TAG, "Set the display to deep sleep");
    this->command(0x07);
    this->data(0xA5);
  }
}
int WaveshareEPaper7P3InF::get_width_internal() { return 800; }
int WaveshareEPaper7P3InF::get_height_internal() { return 480; }
uint32_t WaveshareEPaper7P3InF::idle_timeout_() { return 35000; }
void WaveshareEPaper7P3InF::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.3in-F");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

bool WaveshareEPaper7P3InF::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  delay(200);  // NOLINT
  return true;
}
bool WaveshareEPaper7P5InV2::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  return true;
}
void WaveshareEPaper7P5InV2::initialize() {
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);
  this->data(0x3f);
  this->data(0x3f);

  // We don't want the display to be powered at this point

  delay(100);  // NOLINT
  this->wait_until_idle_();

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x07);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x1F);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);
  this->data(0x20);
  this->data(0x01);
  this->data(0xE0);

  // COMMAND DUAL SPI MM_EN, DUSPI_EN
  this->command(0x15);
  this->data(0x00);

  // COMMAND POWER DRIVER HAT DOWN
  // This command will turn off booster, controller, source driver, gate driver, VCOM, and
  // temperature sensor, but register data will be kept until VDD turned OFF or Deep Sleep Mode.
  // Source/Gate/Border/VCOM will be released to floating.
  this->command(0x02);
}
void HOT WaveshareEPaper7P5InV2::display() {
  uint32_t buf_len = this->get_buffer_length_();

  // COMMAND POWER ON
  ESP_LOGI(TAG, "Power on the display and hat");

  // This command will turn on booster, controller, regulators, and temperature sensor will be
  // activated for one-time sensing before enabling booster. When all voltages are ready, the
  // BUSY_N signal will return to high.
  this->command(0x04);
  delay(200);  // NOLINT
  this->wait_until_idle_();

  // COMMAND DATA START TRANSMISSION NEW DATA
  this->command(0x13);
  delay(2);
  for (uint32_t i = 0; i < buf_len; i++) {
    this->data(~(this->buffer_[i]));
  }

  delay(100);  // NOLINT
  this->wait_until_idle_();

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
  delay(100);  // NOLINT
  this->wait_until_idle_();

  ESP_LOGV(TAG, "Before command(0x02) (>> power off)");
  this->command(0x02);
  this->wait_until_idle_();
  ESP_LOGV(TAG, "After command(0x02) (>> power off)");
}

int WaveshareEPaper7P5InV2::get_width_internal() { return 800; }
int WaveshareEPaper7P5InV2::get_height_internal() { return 480; }
uint32_t WaveshareEPaper7P5InV2::idle_timeout_() { return 10000; }
void WaveshareEPaper7P5InV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5inV2rev2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

/* 7.50inV2alt */
bool WaveshareEPaper7P5InV2alt::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGI(TAG, "Timeout while displaying image!");
      return false;
    }
    delay(10);
  }
  return true;
}

void WaveshareEPaper7P5InV2alt::initialize() {
  this->reset_();

  // COMMAND POWER SETTING
  this->command(0x01);

  // 1-0=11: internal power
  this->data(0x07);
  this->data(0x17);  // VGH&VGL
  this->data(0x3F);  // VSH
  this->data(0x26);  // VSL
  this->data(0x11);  // VSHR

  // VCOM DC Setting
  this->command(0x82);
  this->data(0x24);  // VCOM

  // Booster Setting
  this->command(0x06);
  this->data(0x27);
  this->data(0x27);
  this->data(0x2F);
  this->data(0x17);

  // POWER ON
  this->command(0x04);

  delay(100);  // NOLINT
  this->wait_until_idle_();
  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x3F);  // KW-3f   KWR-2F BWROTP 0f BWOTP 1f

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);  // source 800
  this->data(0x20);
  this->data(0x01);  // gate 480
  this->data(0xE0);
  // COMMAND ...?
  this->command(0x15);
  this->data(0x00);
  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x00);
  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);
  // Resolution setting
  this->command(0x65);
  this->data(0x00);
  this->data(0x00);  // 800*480
  this->data(0x00);
  this->data(0x00);

  this->wait_until_idle_();

  uint8_t lut_vcom_7_i_n5_v2[] = {
      0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_ww_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bw_7_i_n5_v2[] = {
      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_wb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x3, 0x84, 0xF, 0x1, 0xF, 0x1, 0x4, 0x40, 0xF, 0xF, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t lut_bb_7_i_n5_v2[] = {
      0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  };

  uint8_t count;
  this->command(0x20);  // VCOM
  for (count = 0; count < 42; count++)
    this->data(lut_vcom_7_i_n5_v2[count]);

  this->command(0x21);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_ww_7_i_n5_v2[count]);

  this->command(0x22);  // LUTBW
  for (count = 0; count < 42; count++)
    this->data(lut_bw_7_i_n5_v2[count]);

  this->command(0x23);  // LUTWB
  for (count = 0; count < 42; count++)
    this->data(lut_wb_7_i_n5_v2[count]);

  this->command(0x24);  // LUTBB
  for (count = 0; count < 42; count++)
    this->data(lut_bb_7_i_n5_v2[count]);
}

void WaveshareEPaper7P5InV2alt::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5inV2");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

/* 7.50inV2 with partial and fast refresh */
bool WaveshareEPaper7P5InV2P::wait_until_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    this->command(0x71);
    if (millis() - start > this->idle_timeout_()) {
      ESP_LOGE(TAG, "Timeout while displaying image!");
      return false;
    }
    App.feed_wdt();
    delay(10);
  }
  return true;
}

void WaveshareEPaper7P5InV2P::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(20);
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
    delay(20);
  }
}

void WaveshareEPaper7P5InV2P::turn_on_display_() {
  this->command(0x12);
  delay(100);  // NOLINT
  this->wait_until_idle_();
}

void WaveshareEPaper7P5InV2P::initialize() {
  this->reset_();

  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x07);
  this->data(0x07);
  this->data(0x3f);
  this->data(0x3f);

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0x17);
  this->data(0x17);
  this->data(0x28);
  this->data(0x17);

  // COMMAND POWER DRIVER HAT UP
  this->command(0x04);
  delay(100);  // NOLINT
  this->wait_until_idle_();

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0x1F);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x03);
  this->data(0x20);
  this->data(0x01);
  this->data(0xE0);

  // COMMAND DUAL SPI MM_EN, DUSPI_EN
  this->command(0x15);
  this->data(0x00);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x10);
  this->data(0x07);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND ENABLE FAST UPDATE
  this->command(0xE0);
  this->data(0x02);
  this->command(0xE5);
  this->data(0x5A);

  // COMMAND POWER DRIVER HAT DOWN
  this->command(0x02);
}

void HOT WaveshareEPaper7P5InV2P::display() {
  uint32_t buf_len = this->get_buffer_length_();

  // COMMAND POWER ON
  ESP_LOGI(TAG, "Power on the display and hat");

  this->command(0x04);
  delay(200);  // NOLINT
  this->wait_until_idle_();

  if (this->full_update_every_ == 1) {
    this->command(0x13);
    for (uint32_t i = 0; i < buf_len; i++) {
      this->data(~(this->buffer_[i]));
    }

    this->turn_on_display_();

    this->command(0x02);
    this->wait_until_idle_();
    return;
  }

  this->command(0x50);
  this->data(0xA9);
  this->data(0x07);

  if (this->at_update_ == 0) {
    // Enable fast refresh
    this->command(0xE5);
    this->data(0x5A);

    this->command(0x92);

    this->command(0x10);
    delay(2);
    for (uint32_t i = 0; i < buf_len; i++) {
      this->data(~(this->buffer_[i]));
    }

    delay(100);  // NOLINT
    this->wait_until_idle_();

    this->command(0x13);
    delay(2);
    for (uint32_t i = 0; i < buf_len; i++) {
      this->data(this->buffer_[i]);
    }

    delay(100);  // NOLINT
    this->wait_until_idle_();

    this->turn_on_display_();

  } else {
    // Enable partial refresh
    this->command(0xE5);
    this->data(0x6E);

    // Activate partial refresh and set window bounds
    this->command(0x91);
    this->command(0x90);

    this->data(0x00);
    this->data(0x00);
    this->data((get_width_internal() - 1) >> 8 & 0xFF);
    this->data((get_width_internal() - 1) & 0xFF);

    this->data(0x00);
    this->data(0x00);
    this->data((get_height_internal() - 1) >> 8 & 0xFF);
    this->data((get_height_internal() - 1) & 0xFF);

    this->data(0x01);

    this->command(0x13);
    delay(2);
    for (uint32_t i = 0; i < buf_len; i++) {
      this->data(this->buffer_[i]);
    }

    delay(100);  // NOLINT
    this->wait_until_idle_();

    this->turn_on_display_();
  }

  ESP_LOGV(TAG, "Before command(0x02) (>> power off)");
  this->command(0x02);
  this->wait_until_idle_();
  ESP_LOGV(TAG, "After command(0x02) (>> power off)");

  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;
}

int WaveshareEPaper7P5InV2P::get_width_internal() { return 800; }
int WaveshareEPaper7P5InV2P::get_height_internal() { return 480; }
uint32_t WaveshareEPaper7P5InV2P::idle_timeout_() { return 10000; }
void WaveshareEPaper7P5InV2P::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.50inv2p");
  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void WaveshareEPaper7P5InV2P::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

/* 7.50in-bc */
void WaveshareEPaper7P5InBC::initialize() {
  /* The command sequence is similar to the 7P5In display but differs in subtle ways
  to allow for faster updates. */
  // COMMAND POWER SETTING
  this->command(0x01);
  this->data(0x37);
  this->data(0x00);

  // COMMAND PANEL SETTING
  this->command(0x00);
  this->data(0xCF);
  this->data(0x08);

  // COMMAND PLL CONTROL
  this->command(0x30);
  this->data(0x3A);

  // COMMAND VCM_DC_SETTING: all temperature range
  this->command(0x82);
  this->data(0x28);

  // COMMAND BOOSTER SOFT START
  this->command(0x06);
  this->data(0xC7);
  this->data(0xCC);
  this->data(0x15);

  // COMMAND VCOM AND DATA INTERVAL SETTING
  this->command(0x50);
  this->data(0x77);

  // COMMAND TCON SETTING
  this->command(0x60);
  this->data(0x22);

  // COMMAND FLASH CONTROL
  this->command(0x65);
  this->data(0x00);

  // COMMAND RESOLUTION SETTING
  this->command(0x61);
  this->data(0x02);  // 640 >> 8
  this->data(0x80);
  this->data(0x01);  // 384 >> 8
  this->data(0x80);

  // COMMAND FLASH MODE
  this->command(0xE5);
  this->data(0x03);
}

void HOT WaveshareEPaper7P5InBC::display() {
  // COMMAND DATA START TRANSMISSION 1
  this->command(0x10);
  this->start_data_();

  for (size_t i = 0; i < this->get_buffer_length_(); i++) {
    // A line of eight source pixels (each a bit in this byte)
    uint8_t eight_pixels = this->buffer_[i];

    for (uint8_t j = 0; j < 8; j += 2) {
      /* For bichromatic displays, each byte represents two pixels. Each nibble encodes a pixel: 0=white, 3=black,
      4=color. Therefore, e.g. 0x44 = two adjacent color pixels, 0x33 is two adjacent black pixels, etc. If you want
      to draw using the color pixels, change '0x30' with '0x40' and '0x03' with '0x04' below. */
      uint8_t left_nibble = (eight_pixels & 0x80) ? 0x30 : 0x00;
      eight_pixels <<= 1;
      uint8_t right_nibble = (eight_pixels & 0x80) ? 0x03 : 0x00;
      eight_pixels <<= 1;
      this->write_byte(left_nibble | right_nibble);
    }
    App.feed_wdt();
  }
  this->end_data_();

  // Unlike the 7P5In display, we send the "power on" command here rather than during initialization
  // COMMAND POWER ON
  this->command(0x04);

  // COMMAND DISPLAY REFRESH
  this->command(0x12);
}

int WaveshareEPaper7P5InBC::get_width_internal() { return 640; }

int WaveshareEPaper7P5InBC::get_height_internal() { return 384; }

void WaveshareEPaper7P5InBC::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-bc");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper7P5InHDB::initialize() {
  this->command(0x12);  // SWRESET

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x46);  // Auto Write RAM
  this->data(0xF7);

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x47);  // Auto Write RAM
  this->data(0xF7);

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x0C);  // Soft start setting
  this->data(0xAE);
  this->data(0xC7);
  this->data(0xC3);
  this->data(0xC0);
  this->data(0x40);

  this->command(0x01);  // Set MUX as 527
  this->data(0xAF);
  this->data(0x02);
  this->data(0x01);

  this->command(0x11);  // Data entry mode
  this->data(0x01);

  this->command(0x44);
  this->data(0x00);  // RAM x address start at 0
  this->data(0x00);
  this->data(0x6F);  // RAM x address end at 36Fh -> 879
  this->data(0x03);

  this->command(0x45);
  this->data(0xAF);  // RAM y address start at 20Fh;
  this->data(0x02);
  this->data(0x00);  // RAM y address end at 00h;
  this->data(0x00);

  this->command(0x3C);  // VBD
  this->data(0x01);     // LUT1, for white

  this->command(0x18);
  this->data(0x80);

  this->command(0x22);
  this->data(0xB1);  // Load Temperature and waveform setting.

  this->command(0x20);

  this->wait_until_idle_();  // waiting for the electronic paper IC to release the idle signal

  this->command(0x4E);
  this->data(0x00);
  this->data(0x00);

  this->command(0x4F);
  this->data(0xAF);
  this->data(0x02);
}

void HOT WaveshareEPaper7P5InHDB::display() {
  this->command(0x4F);
  this->data(0xAf);
  this->data(0x02);

  // BLACK
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // RED
  this->command(0x26);
  this->start_data_();
  for (size_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();

  this->command(0x22);
  this->data(0xC7);
  this->command(0x20);
  delay(100);  // NOLINT
}

int WaveshareEPaper7P5InHDB::get_width_internal() { return 880; }

int WaveshareEPaper7P5InHDB::get_height_internal() { return 528; }

void WaveshareEPaper7P5InHDB::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.5in-HD-b");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

static const uint8_t LUT_SIZE_TTGO_DKE_PART = 153;

static const uint8_t PART_UPDATE_LUT_TTGO_DKE[LUT_SIZE_TTGO_DKE_PART] = {
    0x0, 0x40, 0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x80, 0x80, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x40, 0x40, 0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x80, 0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0xF, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x4,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x1, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,  0x1,  0x0, 0x0, 0x0,  0x0,  0x0, 0x0, 0x0, 0x0,  0x0, 0x0,
    0x0, 0x0,  0x0, 0x0, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
    // 0x22,   0x17,   0x41,   0x0,    0x32,   0x32
};

void WaveshareEPaper2P13InDKE::initialize() {}
void HOT WaveshareEPaper2P13InDKE::display() {
  bool partial = this->at_update_ != 0;
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  if (partial) {
    ESP_LOGI(TAG, "Performing partial e-paper update.");
  } else {
    ESP_LOGI(TAG, "Performing full e-paper update.");
  }

  // start and set up data format
  this->command(0x12);
  this->wait_until_idle_();

  this->command(0x11);
  this->data(0x03);
  this->command(0x44);
  this->data(1);
  this->data(this->get_width_internal() / 8);
  this->command(0x45);
  this->data(0);
  this->data(0);
  this->data(this->get_height_internal());
  this->data(0);
  this->command(0x4e);
  this->data(1);
  this->command(0x4f);
  this->data(0);
  this->data(0);

  if (!partial) {
    // send data
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // commit
    this->command(0x20);
    this->wait_until_idle_();
  } else {
    // set up partial update
    this->command(0x32);
    this->start_data_();
    this->write_array(PART_UPDATE_LUT_TTGO_DKE, sizeof(PART_UPDATE_LUT_TTGO_DKE));
    this->end_data_();
    this->command(0x3F);
    this->data(0x22);

    this->command(0x03);
    this->data(0x17);
    this->command(0x04);
    this->data(0x41);
    this->data(0x00);
    this->data(0x32);
    this->command(0x2C);
    this->data(0x32);

    this->command(0x37);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x40);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);

    this->command(0x3C);
    this->data(0x80);
    this->command(0x22);
    this->data(0xC0);
    this->command(0x20);
    this->wait_until_idle_();

    // send data
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();

    // commit as partial
    this->command(0x22);
    this->data(0xCF);
    this->command(0x20);
    this->wait_until_idle_();

    // data must be sent again on partial update
    this->command(0x24);
    this->start_data_();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->end_data_();
  }

  ESP_LOGI(TAG, "Completed e-paper update.");
}

int WaveshareEPaper2P13InDKE::get_width_internal() { return 128; }
int WaveshareEPaper2P13InDKE::get_height_internal() { return 250; }
uint32_t WaveshareEPaper2P13InDKE::idle_timeout_() { return 5000; }
void WaveshareEPaper2P13InDKE::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.13inDKE");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P13InDKE::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

// ========================================================
//               13.3in (K version)
// Datasheet/Specification/Reference:
//  - https://files.waveshare.com/wiki/13.3inch-e-Paper-HAT-(K)/13.3-inch-e-Paper-(K)-user-manual.pdf
//  - https://github.com/waveshareteam/e-Paper/tree/master/Arduino/epd13in3k
// ========================================================

// using default wait_until_idle_() function
void WaveshareEPaper13P3InK::initialize() {
  this->wait_until_idle_();
  this->command(0x12);  // SWRESET
  this->wait_until_idle_();

  this->command(0x0c);  // set soft start
  this->data(0xae);
  this->data(0xc7);
  this->data(0xc3);
  this->data(0xc0);
  this->data(0x80);

  this->command(0x01);                            // driver output control
  this->data((get_height_internal() - 1) % 256);  // Y
  this->data((get_height_internal() - 1) / 256);  // Y
  this->data(0x00);

  this->command(0x11);  // data entry mode
  this->data(0x03);

  // SET WINDOWS
  // XRAM_START_AND_END_POSITION
  this->command(0x44);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
  this->data((get_width_internal() - 1) & 0xFF);
  this->data(((get_width_internal() - 1) >> 8) & 0x03);
  // YRAM_START_AND_END_POSITION
  this->command(0x45);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
  this->data((get_height_internal() - 1) & 0xFF);
  this->data(((get_height_internal() - 1) >> 8) & 0x03);

  this->command(0x3C);  // Border setting
  this->data(0x01);

  this->command(0x18);  // use the internal temperature sensor
  this->data(0x80);

  // SET CURSOR
  // XRAM_ADDRESS
  this->command(0x4E);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
  // YRAM_ADDRESS
  this->command(0x4F);
  this->data(0 & 0xFF);
  this->data((0 >> 8) & 0x03);
}
void HOT WaveshareEPaper13P3InK::display() {
  // do single full update
  this->command(0x24);
  this->start_data_();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->end_data_();

  // COMMAND DISPLAY REFRESH
  this->command(0x22);
  this->data(0xF7);
  this->command(0x20);
}

int WaveshareEPaper13P3InK::get_width_internal() { return 960; }
int WaveshareEPaper13P3InK::get_height_internal() { return 680; }
uint32_t WaveshareEPaper13P3InK::idle_timeout_() { return 10000; }
void WaveshareEPaper13P3InK::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 13.3inK");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace waveshare_epaper
}  // namespace esphome
