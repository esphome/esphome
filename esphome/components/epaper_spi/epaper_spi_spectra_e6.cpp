#include "epaper_spi_spectra_e6.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.6c";

static inline uint8_t color_to_hex(Color color) {
  if (color.red > 127) {
    if (color.green > 170) {
      if (color.blue > 127) {
        return 0x1;  // White
      } else {
        return 0x2;  // Yellow
      }
    } else {
      return 0x3;  // Red (or Magenta)
    }
  } else {
    if (color.green > 127) {
      if (color.blue > 127) {
        return 0x5;  // Cyan -> Blue
      } else {
        return 0x6;  // Green
      }
    } else {
      if (color.blue > 127) {
        return 0x5;  // Blue
      } else {
        return 0x0;  // Black
      }
    }
  }
}

void EPaperSpectraE6::fill(Color color) {
  uint8_t pixel_color;
  if (color.is_on()) {
    pixel_color = color_to_hex(color);
  } else {
    pixel_color = 0x1;
  }

  // We store 8 bitset<3> in 3 bytes
  // | byte 1 | byte 2 | byte 3 |
  // |aaabbbaa|abbbaaab|bbaaabbb|
  uint8_t byte_1 = pixel_color << 5 | pixel_color << 2 | pixel_color >> 1;
  uint8_t byte_2 = pixel_color << 7 | pixel_color << 4 | pixel_color << 1 | pixel_color >> 2;
  uint8_t byte_3 = pixel_color << 6 | pixel_color << 3 | pixel_color << 0;

  const size_t buffer_length = this->get_buffer_length();
  for (size_t i = 0; i < buffer_length; i += 3) {
    this->buffer_[i + 0] = byte_1;
    this->buffer_[i + 1] = byte_2;
    this->buffer_[i + 2] = byte_3;
  }
}

uint32_t EPaperSpectraE6::get_buffer_length() {
  // 6 colors buffer, 1 pixel = 3 bits, we will store 8 pixels in 24 bits = 3 bytes
  return this->get_width_controller() * this->get_height_internal() / 8u * 3u;
}

void HOT EPaperSpectraE6::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  uint8_t pixel_bits = color_to_hex(color);
  uint32_t pixel_position = x + y * this->get_width_controller();
  uint32_t first_bit_position = pixel_position * 3;
  uint32_t byte_position = first_bit_position / 8u;
  uint32_t byte_subposition = first_bit_position % 8u;

  if (byte_subposition <= 5) {
    this->buffer_[byte_position] = (this->buffer_[byte_position] & (0xFF ^ (0b111 << (5 - byte_subposition)))) |
                                   (pixel_bits << (5 - byte_subposition));
  } else {
    this->buffer_[byte_position] = (this->buffer_[byte_position] & (0xFF ^ (0b111 >> (byte_subposition - 5)))) |
                                   (pixel_bits >> (byte_subposition - 5));

    this->buffer_[byte_position + 1] =
        (this->buffer_[byte_position + 1] & (0xFF ^ (0xFF & (0b111 << (13 - byte_subposition))))) |
        (pixel_bits << (13 - byte_subposition));
  }
}

bool HOT EPaperSpectraE6::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "Sending data");
    this->command(0x10);
  }

  uint8_t bytes_to_send[4]{0};
  const size_t buffer_length = this->get_buffer_length();
  for (size_t i = this->current_data_index_; i < buffer_length; i += 3) {
    const uint32_t triplet = encode_uint24(this->buffer_[i + 0], this->buffer_[i + 1], this->buffer_[i + 2]);
    // 8 pixels are stored in 3 bytes
    // |aaabbbaa|abbbaaab|bbaaabbb|
    // | byte 1 | byte 2 | byte 3 |
    bytes_to_send[0] = ((triplet >> 17) & 0b01110000) | ((triplet >> 18) & 0b00000111);
    bytes_to_send[1] = ((triplet >> 11) & 0b01110000) | ((triplet >> 12) & 0b00000111);
    bytes_to_send[2] = ((triplet >> 5) & 0b01110000) | ((triplet >> 6) & 0b00000111);
    bytes_to_send[3] = ((triplet << 1) & 0b01110000) | ((triplet << 0) & 0b00000111);

    this->start_data_();
    this->write_array(bytes_to_send, sizeof(bytes_to_send));
    this->end_data_();

    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->current_data_index_ = i + 3;
      return false;
    }
  }
  // Finished the entire dataset
  this->current_data_index_ = 0;
  return true;
}

void EPaperSpectraE6::reset() {
  if (this->reset_pin_ != nullptr) {
    this->disable_loop();
    this->reset_pin_->digital_write(true);
    this->set_timeout(20, [this] {
      this->reset_pin_->digital_write(false);
      delay(2);
      this->reset_pin_->digital_write(true);
      this->set_timeout(20, [this] { this->enable_loop(); });
    });
  }
}

}  // namespace esphome::epaper_spi
