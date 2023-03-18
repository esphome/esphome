#include "gdew0154m09.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace gdew0154m09 {

void GDEW0154M09::setup_pins_() {
  this->init_internal_(this->get_buffer_length_());
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->_lastbuff = allocator.allocate(this->get_buffer_length_());

  this->dc_pin_->setup();
  this->dc_pin_->digital_write(true);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
  this->spi_setup();
  this->reset_display_controller_();

}

void GDEW0154M09::reset_display_controller_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false);
    delay(100); // NOLINT
    this->reset_pin_->digital_write(true);
    delay(100); // NOLINT
  }
}

void GDEW0154M09::initialize_() {
  // this->wait_until_idle_();
  write_init_list_(WFT0154CZB3_LIST);
  delay(100); // NOLINT
  this->wait_until_idle_();
  if (this->_lastbuff != nullptr) {
    memset(this->_lastbuff, 0xff, sizeof(uint8_t) * this->get_buffer_length_());
  }
  this->clear_(0);
}
//void GDEW0154M09::initialize_partial_mode() {
// code only provided as documentation - not yet functional in ESPHome.
//  command_(0x00);  // panel setting
//  data_(0xff);
//  data_(0x0e);
//
//  int count = 0;
//  command_(0x20);
//  for (count = 0; count < 42; count++) {
//    data_(*(const unsigned char *) (&lut_vcomDC1[count]));
//  }
//
//  command_(0x21);
//  for (count = 0; count < 42; count++) {
//    data_(*(const unsigned char *) (&lut_ww1[count]));
//  }
//
//  command_(0x22);
//  for (count = 0; count < 42; count++) {
//    data_(*(const unsigned char *) (&lut_bw1[count]));
//  }
//
//  command_(0x23);
//  for (count = 0; count < 42; count++) {
//    data_(*(const unsigned char *) (&lut_wb1[count]));
//  }
//
//  command_(0x24);
//  for (count = 0; count < 42; count++) {
//    data_(*(const unsigned char *) (&lut_bb1[count]));
//    }
//}

void GDEW0154M09::command_(uint8_t cmd) {
  this->cs_->digital_write(false);
  this->dc_pin_->digital_write(false);
  this->enable();
  this->transfer_byte(cmd);
  this->cs_->digital_write(true);
  this->disable();
}
void GDEW0154M09::data_(uint8_t data) {
  this->cs_->digital_write(false);
  this->dc_pin_->digital_write(true);
  this->enable();
  this->transfer_byte(data);
  this->cs_->digital_write(true);
  this->disable();
}

void HOT GDEW0154M09::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0) {
    return;
  }
  int32_t pix_num = this->get_width_internal() * y + x;
  if (color.is_on()) {
    this->buffer_[pix_num / 8] &= ~(0x80 >> (pix_num % 8));
  } else {
    this->buffer_[pix_num / 8] |= (0x80 >> (pix_num % 8));
  }
}

bool GDEW0154M09::wait_until_idle_(uint32_t timeout) {
  if (this->busy_pin_ == nullptr) {
    ESP_LOGV(TAG, "busypin is not defined, not waiting.");
    return true;
  }

  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    uint32_t waittime = millis() - start;
    if (waittime > timeout) {
      ESP_LOGE(TAG, "Timeout while displaying image! was %dms", waittime);
      return false;
    }
    delay(10);
    App.feed_wdt();
  }
  return true;
}

int GDEW0154M09::clear_dsram_() {
  int pixsize = this->get_buffer_length_();
  for (int i = 0; i < 2; i++) {
    command_(0x10); // data transmission start DTM1
    for (int count = 0; count < pixsize; count++) {
      data_(0x00); // set every pixel to 0
    }
    delay(2);
    command_(0x13); // data transmission start DTM2
    for (int count = 0; count < pixsize; count++) {
      data_(0xff); // set every pixel to ff
    }
  }
  return 0;
}

int GDEW0154M09::clear(int mode) {
  int pixsize = this->get_buffer_length_();
  if (mode == 0) {
    this->command_(0x10);
    for (int count = 0; count < pixsize; count++) {
      this->data_(0xff);
    }
    delay(2);
    this->command_(0x13);
    for (int count = 0; count < pixsize; count++) {
      this->data_(0x00);
    }

    delay(2);
    this->command_(0x12);
    this->wait_until_idle_();

    this->command_(0x10);
    for (int count = 0; count < pixsize; count++) {
      this->data_(0x00);
    }
    delay(2);
    this->command_(0x13);
    for (int count = 0; count < pixsize; count++) {
      this->data_(0xff);
    }
    delay(2);
    this->command_(0x12);
    this->wait_until_idle_();
  } else if (mode == 1) {
    this->command_(0x10);
    for (int count = 0; count < pixsize; count++) {
      this->data_(_lastbuff[count]);
    }
    delay(2);
    this->command_(0x13);
    for (int count = 0; count < pixsize; count++) {
      this->data_(0xff);
    }
    delay(2);
    this->command_(0x12);
    this->wait_until_idle_();
  }

  return 0;
}

int GDEW0154M09::write_init_list_(const unsigned char *list) {
  int list_limit = list[0];
  unsigned char *start_ptr = ((unsigned char *)list + 1);
  for (int i = 0; i < list_limit; i++) {
    this->command_(*(start_ptr + 0));
    for (int dnum = 0; dnum < *(start_ptr + 1); dnum++) {
      this->data_(*(start_ptr + 2 + dnum));
    }
    start_ptr += (*(start_ptr + 1) + 2);
  }
  return 0;
}

size_t GDEW0154M09::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT GDEW0154M09::display_() {
  // this->clear_dsram_();
  this->set_draw_addr_(0, 0, GDEW0154M09_WIDTH, GDEW0154M09_HEIGHT);
  this->draw_buff_(this->_lastbuff, this->buffer_, this->get_buffer_length_());
}

void GDEW0154M09::draw_buff_(uint8_t *lastbuff, uint8_t *buff, size_t size) {
  command_(0x10);
  for (int i = 0; i < size; i++) {
    data_(lastbuff[i]);
  }
  command_(0x13);
  for (int i = 0; i < size; i++) {
    data_(buff[i]);
    lastbuff[i] = buff[i];
  }
  command_(0x12);
  delay(100); // NOLINT
  this->wait_until_idle_();
}

void GDEW0154M09::set_draw_addr_(uint16_t posx, uint16_t posy, uint16_t width, uint16_t height) {
  // command_(0x91);   // This command makes the display enter partial mode
  command_(0x90);   // resolution setting
  data_(posx);  // x-start
  data_(posx + width - 1);  // x-end
  data_(0);                 // x Reserved

  data_(posy);           // y-start
  data_(0);              // y Reserved
  data_(posy + height);  // y-end
  data_(0x01);
}

void GDEW0154M09::dump_config() {
  LOG_DISPLAY("", "GDEW0154M09", this);
  ESP_LOGCONFIG(TAG, "  Configured");
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void GDEW0154M09::update() {
  this->do_update_();
  this->display_();
}

void GDEW0154M09::deep_sleep_() {
  command_(0X50); // vcom and data interval setting
  data_(0xf7); // lower interval
  command_(0X02);  // power off
  wait_until_idle_(5000);
  command_(0X07);  // deep sleep
  data_(0xA5); // deep sleep
}

void GDEW0154M09::power_hv_on_() {
  command_(0X50); // vcom and data interval setting
  data_(0xd7); // restore interval to regular
  command_(0X04); // Power On PON
  wait_until_idle_(5000);
}
void GDEW0154M09::on_safe_shutdown() {
  this->deep_sleep_();
}

}  // namespace gdew0154m09
}  // namespace esphome
