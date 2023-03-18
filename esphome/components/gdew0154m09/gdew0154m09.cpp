#include "gdew0154m09.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace gdew0154m09 {

void GDEW0154M09::setup_pins_() {
  this->init_internal_(this->get_buffer_length_());
  this->_lastbuff = (uint8_t *)malloc(sizeof(uint8_t) * this->get_buffer_length_());

  this->dc_pin_->setup();
  this->dc_pin_->digital_write(HIGH);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(HIGH);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
  this->spi_setup();
  this->resetDisplayController();

}

void GDEW0154M09::resetDisplayController() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(HIGH);
    delay(10);
    this->reset_pin_->digital_write(LOW);
    delay(100);
    this->reset_pin_->digital_write(HIGH);
    delay(100);
  }
}

void GDEW0154M09::initialize() {
  // this->wait_until_idle_();
  writeInitList(WFT0154CZB3_LIST);
  delay(100);
  this->wait_until_idle_();
  if (this->_lastbuff != nullptr) {
    memset(this->_lastbuff, 0xff, sizeof(uint8_t) * this->get_buffer_length_());
  }
  this->clear(0);
}
//void GDEW0154M09::initialize_partial_mode() {
// code only provided as documentation - not yet functional in ESPHome.
//  command(0x00);  // panel setting
//  data(0xff);
//  data(0x0e);
//
//  int count = 0;
//  command(0x20);
//  for (count = 0; count < 42; count++) {
//    data(*(const unsigned char *) (&lut_vcomDC1[count]));
//  }
//
//  command(0x21);
//  for (count = 0; count < 42; count++) {
//    data(*(const unsigned char *) (&lut_ww1[count]));
//  }
//
//  command(0x22);
//  for (count = 0; count < 42; count++) {
//    data(*(const unsigned char *) (&lut_bw1[count]));
//  }
//
//  command(0x23);
//  for (count = 0; count < 42; count++) {
//    data(*(const unsigned char *) (&lut_wb1[count]));
//  }
//
//  command(0x24);
//  for (count = 0; count < 42; count++) {
//    data(*(const unsigned char *) (&lut_bb1[count]));
//    }
//}

void GDEW0154M09::command(uint8_t cmd) {
  this->cs_->digital_write(LOW);
  this->dc_pin_->digital_write(LOW);
  this->enable();
  this->transfer_byte(cmd);
  this->cs_->digital_write(HIGH);
  this->disable();
}
void GDEW0154M09::data(uint8_t data) {
  this->cs_->digital_write(LOW);
  this->dc_pin_->digital_write(HIGH);
  this->enable();
  this->transfer_byte(data);
  this->cs_->digital_write(HIGH);
  this->disable();
}

void HOT GDEW0154M09::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0) {
    return;
  }
  int32_t pixNum = this->get_width_internal() * y + x;
  if (color.is_on()) {
    this->buffer_[pixNum / 8] &= ~(0x80 >> (pixNum % 8));
  } else {
    this->buffer_[pixNum / 8] |= (0x80 >> (pixNum % 8));
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

int GDEW0154M09::clearDSRAM() {
  int _pixsize = this->get_buffer_length_();
  for (int i = 0; i < 2; i++) {
    command(0x10); // data transmission start DTM1
    for (int count = 0; count < _pixsize; count++) {
      data(0x00); // set every pixel to 0
    }
    delay(2);
    command(0x13); // data transmission start DTM2
    for (int count = 0; count < _pixsize; count++) {
      data(0xff); // set every pixel to ff
    }
  }
  return 0;
}

int GDEW0154M09::clear(int mode) {
  int _pixsize = this->get_buffer_length_();
  if (mode == 0) {
    this->command(0x10);
    for (int count = 0; count < _pixsize; count++) {
      this->data(0xff);
    }
    delay(2);
    this->command(0x13);
    for (int count = 0; count < _pixsize; count++) {
      this->data(0x00);
    }

    delay(2);
    this->command(0x12);
    this->wait_until_idle_();

    this->command(0x10);
    for (int count = 0; count < _pixsize; count++) {
      this->data(0x00);
    }
    delay(2);
    this->command(0x13);
    for (int count = 0; count < _pixsize; count++) {
      this->data(0xff);
    }
    delay(2);
    this->command(0x12);
    this->wait_until_idle_();
  } else if (mode == 1) {
    this->command(0x10);
    for (int count = 0; count < _pixsize; count++) {
      this->data(_lastbuff[count]);
    }
    delay(2);
    this->command(0x13);
    for (int count = 0; count < _pixsize; count++) {
      this->data(0xff);
    }
    delay(2);
    this->command(0x12);
    this->wait_until_idle_();
  }

  return 0;
}

int GDEW0154M09::writeInitList(const unsigned char *list) {
  int listLimit = list[0];
  unsigned char *startPtr = ((unsigned char *)list + 1);
  for (int i = 0; i < listLimit; i++) {
    this->command(*(startPtr + 0));
    for (int dnum = 0; dnum < *(startPtr + 1); dnum++) {
      this->data(*(startPtr + 2 + dnum));
    }
    startPtr += (*(startPtr + 1) + 2);
  }
  return 0;
}

size_t GDEW0154M09::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT GDEW0154M09::display() {
  // this->clearDSRAM();
  this->setDrawAddr(0,0,GDEW0154M09_WIDTH,GDEW0154M09_HEIGHT);
  this->drawBuff(this->_lastbuff, this->buffer_, this->get_buffer_length_());
}

void GDEW0154M09::drawBuff(uint8_t *lastbuff, uint8_t *buff, size_t size) {
  command(0x10);
  for (int i = 0; i < size; i++) {
    data(lastbuff[i]);
  }
  command(0x13);
  for (int i = 0; i < size; i++) {
    data(buff[i]);
    lastbuff[i] = buff[i];
  }
  command(0x12);
  delay(100);
  this->wait_until_idle_();
}

void GDEW0154M09::setDrawAddr(uint16_t posx, uint16_t posy, uint16_t width, uint16_t height) {
  // command(0x91);   // This command makes the display enter partial mode
  command(0x90);   // resolution setting
  data(posx);  // x-start
  data(posx + width - 1);  // x-end
  data(0);                 // x Reserved

  data(posy);           // y-start
  data(0);              // y Reserved
  data(posy + height);  // y-end
  data(0x01);
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
  this->display();
}

void GDEW0154M09::deep_sleep() {
  command(0X50); // vcom and data interval setting
  data(0xf7); // lower interval
  command(0X02);  // power off
  wait_until_idle_(5000);
  command(0X07);  // deep sleep
  data(0xA5); // deep sleep
}

void GDEW0154M09::powerHVON() {
  command(0X50); // vcom and data interval setting
  data(0xd7); // restore interval to regular
  command(0X04); // Power On PON
  wait_until_idle_(5000);
}
void GDEW0154M09::on_safe_shutdown() {
  this->deep_sleep();
}

}  // namespace pcd8544
}  // namespace esphome
