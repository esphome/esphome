#include "pxmatrix_display.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "PxMatrix.h"

#define DISPLAY_DRAW_TIME 50

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#define matrix_width 64
#define matrix_height 32

namespace esphome {
namespace pxmatrix_display {


PxMATRIX matrix(matrix_width,matrix_height,P_LAT, P_OE,P_A,P_B,P_C,P_D);

uint16_t myRED = matrix.color565(255, 0, 0);
uint16_t myGREEN = matrix.color565(0, 255, 0);
uint16_t myBLUE = matrix.color565(0, 0, 255);
uint16_t myWHITE = matrix.color565(255, 255, 255);
uint16_t myYELLOW = matrix.color565(255, 255, 0);
uint16_t myCYAN = matrix.color565(0, 255, 255);
uint16_t myMAGENTA = matrix.color565(255, 0, 255);
uint16_t myBLACK = matrix.color565(0, 0, 0);

// Display updater
void IRAM_ATTR display_updater(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  matrix.display(DISPLAY_DRAW_TIME);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Update enable
void display_update_enable(bool is_enable)
{
  
  if (is_enable)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 9000, true);
    timerAlarmEnable(timer);
  }
  else
  {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }

}


void PxmatrixDisplay::setup() {

  ESP_LOGCONFIG(TAG, "Starting setup...");

  // Define your display layout here, e.g. 1/8 step
  matrix.begin(16);
  //matrix.setDriverChip(FM6126A);

  // Define your scan pattern here {LINE, ZIGZAG, ZAGGIZ, WZAGZIG, VZAG} (default is LINE)
  //matrix.setScanPattern(LINE);

  // Define multiplex implemention here {BINARY, STRAIGHT} (default is BINARY)
  //matrix.setMuxPattern(BINARY);

  matrix.setFastUpdate(false);
  matrix.clearDisplay();

  //display_update_enable(true); // Screen seems to have better performance when it is updated through update() and not the timer

}

void HOT PxmatrixDisplay::draw_absolute_pixel_internal(int x, int y, int color) {
  uint16_t matrix_color;
  if (color){
    matrix_color = myGREEN;
  } else {
    matrix_color = myBLACK;
  }
  matrix.drawPixel(x , y, matrix_color);
}

void PxmatrixDisplay::fill(int color){
  uint16_t matrix_color;
  if (color){
    matrix_color = myGREEN;
  } else {
    matrix_color = myBLACK;
  }

  matrix.fillScreen(matrix_color);
}

void PxmatrixDisplay::update() {
  display_loop++;
  if (display_loop > 1000){
    this->do_update_();
    display_loop = 0;
  }
  this->display();
}

void HOT PxmatrixDisplay::display() {
  display_updater();
}

int PxmatrixDisplay::get_width_internal(){
  return matrix_width;
}

int PxmatrixDisplay::get_height_internal(){
  return matrix_height;
}


}}