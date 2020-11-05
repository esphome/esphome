#include "pxmatrix_display.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pxmatrix_display {

static const char* TAG = "pxmatrix_display";

static const uint8_t P_LAT = 22;
static const uint8_t P_A = 19;
static const uint8_t P_B = 23;
static const uint8_t P_C = 18;
static const uint8_t P_D = 5;
static const uint8_t P_E = 15;
static const uint8_t P_OE = 2;
static const uint8_t WIDTH = 64;
static const uint8_t HEIGHT = 32;
static const uint8_t DISPLAY_DRAW_TIME = 50;

//static const int DRIVER_CHIPS = driver_chips.FM6124;
//static const color_orders COLOR_ORDERS = RRGGBB;
//static const block_patterns BLOCK_PATTERNS = ABCD;
//static const mux_patterns MUX_PATTERNS = BINARY;
//static const scan_patterns SCAN_PATTERNS = LINE;

void PxmatrixDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Starting setup...");
  this->pxMatrix = PxMATRIX(WIDTH, HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D);

  this->pxMatrix.begin();

  this->pxMatrix.setDriverChip(FM6126A);
//  this->pxMatrix.setScanPattern(LINE);
//  this->pxMatrix.setMuxPattern(BINARY);

  this->pxMatrix.setFastUpdate(false);
  this->pxMatrix.clearDisplay();
}

void HOT PxmatrixDisplay::draw_absolute_pixel_internal(int x, int y, int color) {
  uint16_t matrix_color = color;
  this->pxMatrix.drawPixel(x, y, matrix_color);
}

void PxmatrixDisplay::fill(int color) {
  uint16_t matrix_color = color;
  this->pxMatrix.fillScreen(matrix_color);
}

void PxmatrixDisplay::update() {
  display_loop++;
  if (display_loop > 1000) {
    this->do_update_();
    display_loop = 0;
  }
  this->display();
}

//// Display updater
//void display_updater() {
//  // Increment the counter and set the time of ISR
////  portENTER_CRITICAL_ISR(&timerMux);
//  this->pxMatrix.display(DISPLAY_DRAW_TIME);
////  portEXIT_CRITICAL_ISR(&timerMux);
//}

void HOT PxmatrixDisplay::display() { update(); }

int PxmatrixDisplay::get_width_internal() { return WIDTH; }

int PxmatrixDisplay::get_height_internal() { return HEIGHT; }
}  // namespace pxmatrix_display
}  // namespace esphome