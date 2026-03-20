// This Esphome TM1651 component for use with Mini Battery Displays (7 LED levels)
// and removes the Esphome dependency on the TM1651 Arduino library.
// It was largely based on the work of others as set out below.
// @mrtoy-me July 2025
// ==============================================================================================
// Original Arduino TM1651 library:
// Author:Fred.Chu
// Date:14 August, 2014
// Applicable Module: Battery Display v1.0
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU
// Lesser General Public License for more details.
// Modified record:
// Author:  Detlef Giessmann Germany
// Mail:    mydiyp@web.de
// Demo for the new 7 LED Battery-Display 2017
// IDE:     Arduino-1.6.5
// Type:    OPEN-SMART CX10*4RY68  4Color
// Date:    01.05.2017
// ==============================================================================================
// Esphome component using arduino TM1651 library:
// MIT License
// Copyright (c) 2019 freekode
// ==============================================================================================
// Library and command-line (python) program to control mini battery displays on Raspberry Pi:
// MIT License
// Copyright (c) 2020 Koen Vervloese
// ==============================================================================================
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "tm1651.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tm1651 {

static const char *const TAG = "tm1651.display";

static const bool LINE_HIGH = true;
static const bool LINE_LOW = false;

// TM1651 maximum frequency is 500 kHz (duty ratio 50%) = 2 microseconds / cycle
static const uint8_t CLOCK_CYCLE = 8;

static const uint8_t HALF_CLOCK_CYCLE = CLOCK_CYCLE / 2;
static const uint8_t QUARTER_CLOCK_CYCLE = CLOCK_CYCLE / 4;

static const uint8_t ADDR_FIXED = 0x44;  // fixed address mode
static const uint8_t ADDR_START = 0xC0;  // address of the display register

static const uint8_t DISPLAY_OFF = 0x80;
static const uint8_t DISPLAY_ON = 0x88;

static const uint8_t MAX_DISPLAY_LEVELS = 7;

static const uint8_t PERCENT100 = 100;
static const uint8_t PERCENT50 = 50;

static const uint8_t TM1651_BRIGHTNESS_DARKEST = 0;
static const uint8_t TM1651_BRIGHTNESS_TYPICAL = 2;
static const uint8_t TM1651_BRIGHTNESS_BRIGHTEST = 7;

static const uint8_t TM1651_LEVEL_TAB[] = {0b00000000, 0b00000001, 0b00000011, 0b00000111,
                                           0b00001111, 0b00011111, 0b00111111, 0b01111111};

// public

void TM1651Display::setup() {
  this->clk_pin_->setup();
  this->clk_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->dio_pin_->setup();
  this->dio_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->brightness_ = TM1651_BRIGHTNESS_TYPICAL;

  // clear display
  this->display_level_();
  this->update_brightness_(DISPLAY_ON);
}

void TM1651Display::dump_config() {
  ESP_LOGCONFIG(TAG, "Battery Display");
  LOG_PIN("  CLK: ", clk_pin_);
  LOG_PIN("  DIO: ", dio_pin_);
}

void TM1651Display::set_brightness(uint8_t new_brightness) {
  this->brightness_ = this->remap_brightness_(new_brightness);
  if (this->display_on_) {
    this->update_brightness_(DISPLAY_ON);
  }
}

void TM1651Display::set_level(uint8_t new_level) {
  if (new_level > MAX_DISPLAY_LEVELS)
    new_level = MAX_DISPLAY_LEVELS;
  this->level_ = new_level;
  if (this->display_on_) {
    this->display_level_();
  }
}

void TM1651Display::set_level_percent(uint8_t percentage) {
  this->level_ = this->calculate_level_(percentage);
  if (this->display_on_) {
    this->display_level_();
  }
}

void TM1651Display::turn_off() {
  this->display_on_ = false;
  this->update_brightness_(DISPLAY_OFF);
}

void TM1651Display::turn_on() {
  this->display_on_ = true;
  // display level as it could have been changed when display turned off
  this->display_level_();
  this->update_brightness_(DISPLAY_ON);
}

// protected

uint8_t TM1651Display::calculate_level_(uint8_t percentage) {
  if (percentage > PERCENT100)
    percentage = PERCENT100;
  // scale 0-100% to 0-7 display levels
  // use integer arithmetic with rounding
  uint16_t initial_scaling = (percentage * MAX_DISPLAY_LEVELS) + PERCENT50;
  return (uint8_t) (initial_scaling / PERCENT100);
}

void TM1651Display::display_level_() {
  this->start_();
  this->write_byte_(ADDR_FIXED);
  this->stop_();

  this->start_();
  this->write_byte_(ADDR_START);
  this->write_byte_(TM1651_LEVEL_TAB[this->level_]);
  this->stop_();
}

uint8_t TM1651Display::remap_brightness_(uint8_t new_brightness) {
  if (new_brightness <= 1)
    return TM1651_BRIGHTNESS_DARKEST;
  if (new_brightness == 2)
    return TM1651_BRIGHTNESS_TYPICAL;

  // new_brightness >= 3
  return TM1651_BRIGHTNESS_BRIGHTEST;
}

void TM1651Display::update_brightness_(uint8_t on_off_control) {
  this->start_();
  this->write_byte_(on_off_control | this->brightness_);
  this->stop_();
}

// low level functions

bool TM1651Display::write_byte_(uint8_t data) {
  // data bit written to DIO when CLK is low
  for (uint8_t i = 0; i < 8; i++) {
    this->half_cycle_clock_low_((bool) (data & 0x01));
    this->half_cycle_clock_high_();
    data >>= 1;
  }

  // start 9th cycle, setting DIO high and look for ack
  this->half_cycle_clock_low_(LINE_HIGH);
  return this->half_cycle_clock_high_ack_();
}

void TM1651Display::half_cycle_clock_low_(bool data_bit) {
  // first half cycle, clock low and write data bit
  this->clk_pin_->digital_write(LINE_LOW);
  delayMicroseconds(QUARTER_CLOCK_CYCLE);

  this->dio_pin_->digital_write(data_bit);
  delayMicroseconds(QUARTER_CLOCK_CYCLE);
}

void TM1651Display::half_cycle_clock_high_() {
  // second half cycle, clock high
  this->clk_pin_->digital_write(LINE_HIGH);
  delayMicroseconds(HALF_CLOCK_CYCLE);
}

bool TM1651Display::half_cycle_clock_high_ack_() {
  // second half cycle, clock high and check for ack
  this->clk_pin_->digital_write(LINE_HIGH);
  delayMicroseconds(QUARTER_CLOCK_CYCLE);

  this->dio_pin_->pin_mode(gpio::FLAG_INPUT);
  // valid ack on DIO is low
  bool ack = (!this->dio_pin_->digital_read());

  this->dio_pin_->pin_mode(gpio::FLAG_OUTPUT);

  // ack should be set DIO low by now
  // if its not, set DIO low before the next cycle
  if (!ack) {
    this->dio_pin_->digital_write(LINE_LOW);
  }
  delayMicroseconds(QUARTER_CLOCK_CYCLE);

  // begin next cycle
  this->clk_pin_->digital_write(LINE_LOW);

  return ack;
}

void TM1651Display::start_() {
  // start data transmission
  this->delineate_transmission_(LINE_HIGH);
}

void TM1651Display::stop_() {
  // stop data transmission
  this->delineate_transmission_(LINE_LOW);
}

void TM1651Display::delineate_transmission_(bool dio_state) {
  // delineate data transmission
  // DIO changes its value while CLK is high

  this->dio_pin_->digital_write(dio_state);
  delayMicroseconds(HALF_CLOCK_CYCLE);

  this->clk_pin_->digital_write(LINE_HIGH);
  delayMicroseconds(QUARTER_CLOCK_CYCLE);

  this->dio_pin_->digital_write(!dio_state);
  delayMicroseconds(QUARTER_CLOCK_CYCLE);
}

}  // namespace tm1651
}  // namespace esphome
