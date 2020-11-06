#include "pxmatrix_display.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *TAG = "pxmatrix_display";

namespace esphome {
namespace pxmatrix_display {

void PxmatrixDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Starting setup...");
  this->pxMatrix = new PxMATRIX(width_, height_, pin_LATCH_->get_pin(), pin_OE_->get_pin(), pin_A_->get_pin(),
                                pin_B_->get_pin(), pin_C_->get_pin(), pin_D_->get_pin(), pin_E_->get_pin());
  this->pxMatrix->begin(row_pattern_);

  this->pxMatrix->setDriverChip((driver_chips) driver_chips_);
  this->pxMatrix->setMuxPattern((mux_patterns) mux_patterns_);
  this->pxMatrix->setScanPattern((scan_patterns) scan_patterns_);
  this->pxMatrix->setColorOrder((color_orders) color_orders_);
  //  this->pxMatrix->setBlockPattern(DBCA);

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

void PxmatrixDisplay::loop() { this->display(); }

void HOT PxmatrixDisplay::display() {
  this->pxMatrix->setBrightness(brightness_);
  this->pxMatrix->display();
}

void PxmatrixDisplay::set_pin_latch(GPIOPin *P_Latch) { this->pin_LATCH_ = P_Latch; }

void PxmatrixDisplay::set_pin_a(GPIOPin *Pin_A) { this->pin_A_ = Pin_A; }

void PxmatrixDisplay::set_pin_b(GPIOPin *Pin_B) { this->pin_B_ = Pin_B; }

void PxmatrixDisplay::set_pin_c(GPIOPin *Pin_C) { this->pin_C_ = Pin_C; }

void PxmatrixDisplay::set_pin_d(GPIOPin *Pin_D) { this->pin_D_ = Pin_D; }

void PxmatrixDisplay::set_pin_e(GPIOPin *Pin_E) { this->pin_E_ = Pin_E; }

void PxmatrixDisplay::set_pin_oe(GPIOPin *Pin_OE) { this->pin_OE_ = Pin_OE; }

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
