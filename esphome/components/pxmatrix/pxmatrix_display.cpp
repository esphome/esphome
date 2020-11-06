#include "pxmatrix_display.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char* TAG = "pxmatrix_display";

namespace esphome {
namespace pxmatrix_display {

static const uint8_t P_LAT = 16;
static const uint8_t P_A = 5;
static const uint8_t P_B = 4;
static const uint8_t P_C = 15;
static const uint8_t P_D = 12;
static const uint8_t P_E = 0;
static const uint8_t P_OE = 2;
static const uint8_t WIDTH = 64;
static const uint8_t HEIGHT = 64;
static const uint8_t BRIGHTNESS = 64;
static const uint8_t DISPLAY_DRAW_TIME = 10;
static const uint8_t ROW_PATTERN = 64;

static const driver_chips DRIVER_CHIP = driver_chips::FM6124;
static const color_orders COLOR_ORDER = color_orders::RRGGBB;
// static const block_patterns BLOCK_PATTERN = block_patterns::ABCD;
static const mux_patterns MUX_PATTERN = mux_patterns::BINARY;
static const scan_patterns SCAN_PATTERN = scan_patterns::LINE;

void PxmatrixDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Starting setup...");
  this->pxMatrix = new PxMATRIX(WIDTH, HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);
  this->pxMatrix->begin(ROW_PATTERN);

  this->pxMatrix->setDriverChip(DRIVER_CHIP);
  this->pxMatrix->setMuxPattern(MUX_PATTERN);
  this->pxMatrix->setScanPattern(SCAN_PATTERN);
  this->pxMatrix->setColorOrder(COLOR_ORDER);
  //  this->pxMatrix->setBlockPattern(DBCA);

  // this->pxMatrix->setRotate(true);
  // this->pxMatrix->setFlip(true);

  // this->pxMatrix->setColorOffset(5, 5,5);
  // this->pxMatrix->setMuxPattern(BINARY);

  // The Delay makes the Display less flickery
  this->pxMatrix->clearDisplay();
  ESP_LOGCONFIG(TAG, "Finished Setup");
}

void HOT PxmatrixDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  uint16_t matrix_color = color.to_bgr_565();
  this->pxMatrix->drawPixel(x, y, matrix_color);
}

void PxmatrixDisplay::fill(Color color) {
  uint16_t matrix_color = color.to_bgr_565();
  this->pxMatrix->fillScreen(matrix_color);
}

void PxmatrixDisplay::update() {
  this->pxMatrix->setBrightness(BRIGHTNESS);
  this->pxMatrix->display(DISPLAY_DRAW_TIME);
}

void HOT PxmatrixDisplay::display() { update(); }

int PxmatrixDisplay::get_width_internal() { return WIDTH; }

int PxmatrixDisplay::get_height_internal() { return HEIGHT; }

}  // namespace pxmatrix_display
}  // namespace esphome