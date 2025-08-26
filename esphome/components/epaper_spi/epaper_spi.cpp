#include "epaper_spi.h"
#include <bitset>
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi";

static const uint8_t MAX_TRANSFER_TIME = 10;  // Transfer in 10ms blocks to allow the loop to run

void EPaperBase::setup() {
  this->init_internal_(this->get_buffer_length());
  this->setup_pins_();
  this->spi_setup();
}

void EPaperBase::setup_pins_() {
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

float EPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }

void EPaperBase::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void EPaperBase::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

// write a command followed by one or more bytes of data.
// The command is the first byte, length is the total including cmd.
void EPaperBase::cmd_data(const uint8_t *c_data, size_t length) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(c_data[0]);
  this->dc_pin_->digital_write(true);
  this->write_array(c_data + 1, length - 1);
  this->disable();
}

bool EPaperBase::is_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  return !this->busy_pin_->digital_read();
}

void EPaperBase::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    this->disable_loop();
    this->set_timeout(this->reset_duration_, [this] {
      this->reset_pin_->digital_write(true);
      this->set_timeout(20, [this] {
        this->state_ = RESET_DONE;
        this->enable_loop();
      });
    });
  }
}

void EPaperBase::update() {
  if (this->state_ != IDLE) {
    ESP_LOGE(TAG, "Display update already in progress");
    return;
  }
  this->do_update_();  // Calls ESPHome (current page) lambda
  this->state_ = UPDATING;
  this->enable_loop();
}

void EPaperBase::loop() {
  switch (this->state_) {
    case IDLE:
      this->disable_loop();
      break;
    case UPDATING:
      this->reset_();
      this->state_ = RESETTING;
      break;
    case RESETTING:
      // Nothing to do here, next state change is handled by timeouts
      break;
    case RESET_DONE:
      if (this->is_idle_()) {
        this->initialize();
        this->state_ = INITIALIZING;
      }
      break;
    case INITIALIZING:
      if (this->is_idle_()) {
        ESP_LOGI(TAG, "Display initialized successfully");
        this->state_ = TRANSFERING_DATA;
      }
      break;
    case TRANSFERING_DATA:
      this->transfer_data();
      break;
    case TRANSFER_DONE:
      this->power_on();
      this->state_ = POWERING_ON;
      break;
    case POWERING_ON:
      if (this->is_idle_()) {
        this->refresh_screen();
        this->state_ = REFRESHING_SCREEN;
      }
      break;
    case REFRESHING_SCREEN:
      if (this->is_idle_()) {
        this->power_off();
        this->state_ = POWERING_OFF;
      }
      break;
    case POWERING_OFF:
      if (this->is_idle_()) {
        this->deep_sleep();
      }
      this->state_ = IDLE;
      this->disable_loop();
      break;
  }
}

void EPaper6Color::setup() {
  if (!this->init_internal_6c_(this->get_buffer_length())) {
    this->mark_failed("Failed to initialize buffer");
    return;
  }
  this->setup_pins_();
  this->spi_setup();
}

bool EPaper6Color::init_internal_6c_(uint32_t buffer_length) {
  if (!this->buffer_.init(buffer_length)) {
    return false;
  }
  this->clear();
  return true;
}

uint8_t EPaper6Color::color_to_hex(Color color) {
  uint8_t hex_code;
  if (color.red > 127) {
    if (color.green > 170) {
      if (color.blue > 127) {
        hex_code = 0x1;  // White
      } else {
        hex_code = 0x2;  // Yellow
      }
    } else {
      hex_code = 0x3;  // Red (or Magenta)
    }
  } else {
    if (color.green > 127) {
      if (color.blue > 127) {
        hex_code = 0x5;  // Cyan -> Blue
      } else {
        hex_code = 0x6;  // Green
      }
    } else {
      if (color.blue > 127) {
        hex_code = 0x5;  // Blue
      } else {
        hex_code = 0x0;  // Black
      }
    }
  }

  return hex_code;
}
void EPaper6Color::fill(Color color) {
  uint8_t pixel_color;
  if (color.is_on()) {
    pixel_color = this->color_to_hex(color);
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

uint32_t EPaper6Color::get_buffer_length() {
  // 6 colors buffer, 1 pixel = 3 bits, we will store 8 pixels in 24 bits = 3 bytes
  return this->get_width_controller() * this->get_height_internal() / 8u * 3u;
}

void HOT EPaper6Color::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  uint8_t pixel_bits = this->color_to_hex(color);
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

void EPaperBase::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}

void EPaperBase::end_command_() { this->disable(); }

void EPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void EPaperBase::end_data_() { this->disable(); }

void EPaperBase::on_safe_shutdown() { this->deep_sleep(); }

void EPaper7p3InE::initialize() {
  static const uint8_t cmdh_data[] = {0xAA, 0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
  this->cmd_data(cmdh_data, sizeof(cmdh_data));

  static const uint8_t data_01[] = {0x01, 0x3F};
  this->cmd_data(data_01, sizeof(data_01));

  static const uint8_t data_02[] = {0x00, 0x5F, 0x69};
  this->cmd_data(data_02, sizeof(data_02));

  static const uint8_t data_03[] = {0x03, 0x00, 0x54, 0x00, 0x44};
  this->cmd_data(data_03, sizeof(data_03));

  static const uint8_t data_04[] = {0x05, 0x40, 0x1F, 0x1F, 0x2C};
  this->cmd_data(data_04, sizeof(data_04));

  static const uint8_t data_05[] = {0x06, 0x6F, 0x1F, 0x17, 0x49};
  this->cmd_data(data_05, sizeof(data_05));

  static const uint8_t data_06[] = {0x08, 0x6F, 0x1F, 0x1F, 0x22};
  this->cmd_data(data_06, sizeof(data_06));

  static const uint8_t data_07[] = {0x30, 0x03};
  this->cmd_data(data_07, sizeof(data_07));

  static const uint8_t data_08[] = {0x50, 0x3F};
  this->cmd_data(data_08, sizeof(data_08));

  static const uint8_t data_09[] = {0x60, 0x02, 0x00};
  this->cmd_data(data_09, sizeof(data_09));

  static const uint8_t data_10[] = {0x61, 0x03, 0x20, 0x01, 0xE0};
  this->cmd_data(data_10, sizeof(data_10));

  static const uint8_t data_11[] = {0x84, 0x01};
  this->cmd_data(data_11, sizeof(data_11));

  static const uint8_t data_12[] = {0xE3, 0x2F};
  this->cmd_data(data_12, sizeof(data_12));

  this->power_on();
}

void HOT EPaper7p3InE::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  if (this->current_data_index_ == 0) {
    ESP_LOGI(TAG, "Sending data to the display");
    this->command(0x10);
  }

  uint8_t bytes_to_send[4]{0};
  const size_t buffer_length = this->get_buffer_length();
  for (size_t i = this->current_data_index_; i < buffer_length; i += 3) {
    std::bitset<24> triplet = encode_uint24(this->buffer_[i + 0], this->buffer_[i + 1], this->buffer_[i + 2]);
    // 8 bitset<3> are stored in 3 bytes
    // |aaabbbaa|abbbaaab|bbaaabbb|
    // | byte 1 | byte 2 | byte 3 |
    bytes_to_send[0] = ((triplet >> 17).to_ulong() & 0b01110000) | ((triplet >> 18).to_ulong() & 0b00000111);
    bytes_to_send[1] = ((triplet >> 11).to_ulong() & 0b01110000) | ((triplet >> 12).to_ulong() & 0b00000111);
    bytes_to_send[2] = ((triplet >> 5).to_ulong() & 0b01110000) | ((triplet >> 6).to_ulong() & 0b00000111);
    bytes_to_send[3] = ((triplet << 1).to_ulong() & 0b01110000) | ((triplet << 0).to_ulong() & 0b00000111);

    this->start_data_();
    this->write_array(bytes_to_send, sizeof(bytes_to_send));
    this->end_data_();

    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->current_data_index_ = i + 3;
      return;
    }
  }
  // Finished the entire dataset
  this->current_data_index_ = 0;
  this->state_ = TRANSFER_DONE;
}

void EPaper7p3InE::power_on() {
  ESP_LOGI(TAG, "Power on the display");
  this->command(0x04);
}

void EPaper7p3InE::power_off() {
  ESP_LOGI(TAG, "Power off the display");
  this->command(0x02);
  this->data(0x00);
}

void EPaper7p3InE::refresh_screen() {
  this->command(0x12);
  this->data(0x00);
}

void EPaper7p3InE::deep_sleep() {
  ESP_LOGI(TAG, "Set the display to deep sleep");
  this->command(0x07);
  this->data(0xA5);
}

void EPaper7p3InE::dump_config() {
  LOG_DISPLAY("", "E-Paper SPI", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.3in-E");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
