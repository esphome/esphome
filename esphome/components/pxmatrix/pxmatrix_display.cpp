#include "pxmatrix_display.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *TAG = "pxmatrix_display";

namespace esphome {
namespace pxmatrix_display {

void PxmatrixDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Starting setup...");
  this->pxMatrix = new PxMATRIX(width_, height_, pin_latch_->get_pin(), pin_oe_->get_pin(), pin_a_->get_pin(),
                                pin_b_->get_pin(), pin_c_->get_pin(), pin_d_->get_pin(), pin_e_->get_pin());
  this->pxMatrix->begin(row_pattern_);

  this->pxMatrix->setDriverChip((driver_chips) driver_chips_);
  this->pxMatrix->setMuxPattern((mux_patterns) mux_patterns_);
  this->pxMatrix->setScanPattern((scan_patterns) scan_patterns_);
  this->pxMatrix->setColorOrder((color_orders) color_orders_);
  //  this->pxMatrix->setBlockPattern(DBCA);

  this->pxMatrix->setBrightness(brightness_);
  // this->pxMatrix->setRotate(true);
  // this->pxMatrix->setFlip(true);

  // this->pxMatrix->setColorOffset(5, 5,5);
  // this->pxMatrix->setMuxPattern(BINARY);
  ESP_LOGI(TAG, "Finished Setup");
}

void HOT PxmatrixDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  uint16_t matrix_color = color.to_bgr_565();
  this->pxMatrix->drawPixelRGB565(x, y, matrix_color);
}

void PxmatrixDisplay::fill(Color color) {
  uint16_t matrix_color = color.to_bgr_565();
  this->pxMatrix->fillScreen(matrix_color);
}

void PxmatrixDisplay::update() {
  this->do_update_();
  this->display();
}

void HOT PxmatrixDisplay::display() {
  this->pxMatrix->display();
}

void PxmatrixDisplay::set_pin_latch(GPIOPin *pin_latch) { this->pin_latch_ = pin_latch; }

void PxmatrixDisplay::set_pin_a(GPIOPin *pin_a) { this->pin_a_ = pin_a; }

void PxmatrixDisplay::set_pin_b(GPIOPin *pin_b) { this->pin_b_ = pin_b; }

void PxmatrixDisplay::set_pin_c(GPIOPin *pin_c) { this->pin_c_ = pin_c; }

void PxmatrixDisplay::set_pin_d(GPIOPin *pin_d) { this->pin_d_ = pin_d; }

void PxmatrixDisplay::set_pin_e(GPIOPin *pin_e) { this->pin_e_ = pin_e; }

void PxmatrixDisplay::set_pin_oe(GPIOPin *pin_oe) { this->pin_oe_ = pin_oe; }

void PxmatrixDisplay::set_width(uint8_t width) { this->width_ = width; }

void PxmatrixDisplay::set_height(uint8_t height) { this->height_ = height; }

void PxmatrixDisplay::set_brightness(uint8_t brightness) { this->brightness_ = brightness; }

void PxmatrixDisplay::set_row_patter(uint8_t row_pattern) { this->row_pattern_ = row_pattern; }

void PxmatrixDisplay::set_driver_chips(DriverChips driver_chips) { this->driver_chips_ = driver_chips; }

void PxmatrixDisplay::set_color_orders(ColorOrders color_orders) { this->color_orders_ = color_orders; }

void PxmatrixDisplay::set_scan_patterns(ScanPatterns scan_patterns) { this->scan_patterns_ = scan_patterns; }

void PxmatrixDisplay::set_mux_patterns(MuxPatterns mux_patterns) { this->mux_patterns_ = mux_patterns; }

int PxmatrixDisplay::get_width_internal() { return this->width_; }

int PxmatrixDisplay::get_height_internal() { return this->height_; }

}  // namespace pxmatrix_display
}  // namespace esphome
