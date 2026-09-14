//
// bb_epaper wrapper library for ESPHome
//
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Larry Bank <bitbank@pobox.com>
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
//
#include "bb_epaperbase.h"
#include <bitset>
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Screen update/refresh options
const char *szUpdates[] = {"full", "fast", "partial"};
const int iUpdates[] = {REFRESH_FULL, REFRESH_FAST, REFRESH_PARTIAL};

// Names of the supported raw panels
const char *szPanels[] = {"undefined",
                          "EP42_400x300",
                          "EP42B_400x300",
                          "EP213_122x250",
                          "EP213B_122x250",
                          "EP293_128x296",
                          "EP294_128x296",
                          "EP295_128x296",
                          "EP295_128x296_4GRAY",
                          "EP266_152x296",
                          "EP102_80x128",
                          "EP27B_176x264",
                          "EP29R_128x296",
                          "EP122_192x176",
                          "EP154R_152x152",
                          "EP42R_400x300",
                          "EP42R2_400x300",
                          "EP37_240x416",
                          "EP37B_240x416",
                          "EP213_104x212",
                          "EP75_800x480",
                          "EP75_800x480_GEN2",
                          "EP75_800x480_4GRAY",
                          "EP75_800x480_4GRAY_GEN2",
                          "EP75_800x480_4GRAY_V2",
                          "EP29_128x296",
                          "EP29_128x296_4GRAY",
                          "EP213R_122x250",
                          "EP154_200x200",
                          "EP154B_200x200",
                          "EP266YR_184x360",
                          "EP29YR_128x296",
                          "EP29YR_168x384",
                          "EP583_648x480",
                          "EP296_128x296",
                          "EP26R_152x296",
                          "EP73_800x480",
                          "EP73_SPECTRA_800x480",
                          "EP74R_640x384",
                          "EP583R_600x448",
                          "EP75R_800x480",
                          "EP426_800x480",
                          "EP426_800x480_4GRAY",
                          "EP29R2_128x296",
                          "EP41_640x400",
                          "EP81_SPECTRA_1024x576",
                          "EP7_960x640",
                          "EP213R2_122x250",
                          "EP29Z_128x296",
                          "EP29Z_128x296_4GRAY",
                          "EP213Z_122x250",
                          "EP213Z_122x250_4GRAY",
                          "EP154Z_152x152",
                          "EP579_792x272",
                          "EP213YR_122x250",
                          "EP37YR_240x416",
                          "EP35YR_184x384",
                          "EP397YR_800x480",
                          "EP154YR_200x200",
                          "EP266YR2_184x360",
                          "EP42YR_400x300",
                          "EP215YR_160x296",
                          "EP1085_1360x480",
                          "EP31_240x320",
                          "EP75YR_800x480",
                          "EP154_200x200_4GRAY",
                          "EP42B_400x300_4GRAY",
                          "EP397_800x480",
                          "EP397_800x480_4GRAY",
                          nullptr};

namespace esphome {
namespace bb_epaper {

bb_epaper *pThis;

static const char *const TAG = "bb_epaper";

int bb_epaper::get_height_internal() {
  //  ESP_LOGCONFIG(TAG, "get_height_internal() %d", _bbepaper._bbep.native_height);
  return _bbepaper._bbep.native_height;
}
int bb_epaper::get_width_internal() {
  //  ESP_LOGCONFIG(TAG, "get_width_internal() %d", _bbepaper._bbep.native_width);
  return _bbepaper._bbep.native_width;
}

void spi_write(const uint8_t *pData, int iLen) {
  // ESP_LOGCONFIG(TAG, "Writing %d bytes to SPI", iLen);
  pThis->write_array(pData, iLen);
} /* spi_write() */

void set_gpio(uint8_t pin, uint8_t value) {
  // Figure out which pin because ESPHome uses a pointer to a GPIO structure
  if (pin == pThis->_bbepaper._bbep.iDCPin) {
    pThis->dc_pin_->digital_write(value);
  } else if (pin == pThis->_bbepaper._bbep.iCSPin) {
    pThis->cs_pin_->digital_write(value);
    //    if (value) {
    //      pThis->disable();  // SPI CS
    //    } else {
    //      pThis->enable();
    //    }
  } else if (pin == pThis->_bbepaper._bbep.iRSTPin) {
    pThis->reset_pin_->digital_write(value);
  }
} /* set_gpio() */

uint8_t get_gpio(uint8_t pin) {
  // This is only used for the BUSY pin, so hard code it
  (void) pin;
  return (uint8_t) pThis->busy_pin_->digital_read();
} /* get_gpio() */

void bb_epaper::setup() {
  int i, iModel = -1;

  iCount = 0;  // number of update counts
  LOG_DISPLAY("setup:", "bb_epaper", this);
  // search for the raw panel name
  i = 0;
  while (szPanels[i] != nullptr) {
    if (strcasecmp(szPanels[i], model_name.c_str()) == 0) {
      iModel = i;
      break;
    }
    i++;
  }                               // while
  this->_refresh = REFRESH_FULL;  // assume full refresh unless told otherwise
  if (refresh_type.length()) {
    i = 0;
    while (szUpdates[i] != nullptr) {
      if (strcasecmp(szUpdates[i], refresh_type.c_str()) == 0) {
        this->_refresh = iUpdates[i];
        break;
      }
      i++;
    }  // while
  }
  if (iModel < 0) {  // error!
    ESP_LOGCONFIG(TAG, "setup error! no matching panel for %d", model_name.c_str());
    return;
  }
  this->setup_pins_();
  this->spi_setup();
  // Set up the function pointers to allow bb_epaper to access SPI+GPIO
  _bbepaper.setWritefn(spi_write);
  _bbepaper.setSetGPIOfn(set_gpio);
  _bbepaper.setGetGPIOfn(get_gpio);
  _bbepaper._bbep.iSpeed = 1;  // Tell it not to use Bit Bang for SPI
  _bbepaper.setPanelType(iModel);
  _bbepaper.allocBuffer();
  pThis = this;
  _bbepaper.fillScreen(BBEP_WHITE);
  _bbepaper.writePlane(PLANE_DUPLICATE);
  _bbepaper.refresh(REFRESH_FAST);
} /* setup() */

void bb_epaper::setup_pins_() {
  LOG_DISPLAY("setup_pins:", "bb_epaper", this);
  this->_bbepaper._bbep.iRSTPin = this->reset_pin_->get_pin();
  this->_bbepaper._bbep.iDCPin = this->dc_pin_->get_pin();
  this->_bbepaper._bbep.iCSPin = this->cs_pin_->get_pin();
  this->_bbepaper._bbep.iBUSYPin = this->busy_pin_->get_pin();
  ESP_LOGCONFIG(TAG, "Pins - CS:%d DC:%d RST:%d BUSY:%d", this->cs_pin_->get_pin(), this->dc_pin_->get_pin(),
                this->reset_pin_->get_pin(), this->busy_pin_->get_pin());
  pinMode(this->cs_pin_->get_pin(), OUTPUT);
  this->cs_pin_->digital_write(true);
  pinMode(this->dc_pin_->get_pin(), OUTPUT);
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    pinMode(this->reset_pin_->get_pin(), OUTPUT);
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    pinMode(this->busy_pin_->get_pin(), INPUT);
  }
}
float bb_epaper::get_setup_priority() const { return setup_priority::PROCESSOR; }

void bb_epaper::dump_config() {
  LOG_DISPLAY("dump_config()", "bb_epaper", this);
  ESP_LOGCONFIG(TAG, "  Model: %d", this->_bbepaper.getPanelType());
  //  ESP_LOGCONFIG(TAG, "  Full Update Every: %" PRIu32, this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void bb_epaper::display() {
  if (_refresh == REFRESH_PARTIAL && (iCount & 15) == 0) {  // do a fast update every 16
    _bbepaper.writePlane();
    _bbepaper.refresh(REFRESH_FAST);
  } else {                              // do the users' choice of refresh type
    if (_refresh == REFRESH_PARTIAL) {  // make sure we get the desired results
      _bbepaper.writePlane(PLANE_FALSE_DIFF);
    } else {
      _bbepaper.writePlane();  // default type
    }
    _bbepaper.refresh(_refresh);
  }
  iCount++;
} /* display() */

void bb_epaper::update() {
  LOG_DISPLAY("update()", "bb_epaper", this);
  this->do_update_();
  this->display();
} /* update() */

void bb_epaper::fill(Color color) {
  const uint8_t fill_color = color.is_on() ? BBEP_BLACK : BBEP_WHITE;
  ESP_LOGCONFIG(TAG, "fill with %d", fill_color);
  _bbepaper.fillScreen(fill_color);
} /* fill() */

void HOT bb_epaper::draw_absolute_pixel_internal(int x, int y, Color color) {
  //  ESP_LOGCONFIG(TAG, "draw pixel at %d,%d", x, y);
  _bbepaper.drawPixel(x, y, (color.is_on() ? BBEP_BLACK : BBEP_WHITE));
} /* draw_absolute_pixel_internal() */

void bb_epaper::on_safe_shutdown() { this->deep_sleep(); }

}  // namespace bb_epaper
}  // namespace esphome
