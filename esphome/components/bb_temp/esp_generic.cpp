//
// bb_temp I/O wrapper functions for Espressif esp-idf
// Copyright (c) 2025 BitBank Software, Inc.
// Written by Larry Bank (bitbank@pobox.com)
//
// SPDX-License-Identifier: Apache-2.0
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "driver/i2c.h"
#include "esp_generic.h"
#include "esphome/core/log.h"

#define I2C_MASTER_NUM 0
// Since the Espressif I2C driver seems to corrupt memory with it's frequent allocs and frees, use bit banging
static uint8_t u8SDA_Pin, u8SCL_Pin;
static int iDelay = 1;

// GPIO modes
#ifdef FUTURE
#define HIGH 1
#define LOW 0
#define DISABLED 0
#define INPUT 1
#define INPUT_PULLUP 2
#define OUTPUT 3
#define INPUT_PULLDOWN 4
#endif

int I2CWrite(BBI2C *pI2C, unsigned char iAddr, unsigned char *pData, int iLen);

// static unsigned long micros(void)
//{
//     return (unsigned long)(esp_timer_get_time());
// }

#ifndef ARDUINO
void delayMicroseconds(uint32_t us) { ets_delay_us(us); }

void delay(uint32_t ms) {
  if (ms >= 10) {
    vTaskDelay(ms / 10);
  }
  delayMicroseconds((ms % 10) * 1000);
}
#endif  // !ARDUINO

int I2CTest(BBI2C *pI2C, uint8_t addr) {
  int response = 0;

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == NULL) {
    ESP_LOGE("bb_temperature", "insufficient memory for I2C transaction");
  }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  response = (ret == ESP_OK);

  return response;
} /* I2CTest() */
//
// Initialize the Wire library on the given SDA/SCL GPIOs
//
void I2CInit(BBI2C *pI2C, unsigned int iClock) {
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = pI2C->iSDA;
  conf.scl_io_num = pI2C->iSCL;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = 10000;  // iClock;
  conf.clk_flags = 0;
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
  pI2C->file_i2c = I2C_NUM_0;  // I2C handle used
} /* I2CInit() */

int I2CWrite(BBI2C *pI2C, unsigned char iAddr, unsigned char *pData, int iLen) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == NULL) {
    // ESP_LOGE("bb_epdiy", "insufficient memory for I2C transaction");
  }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (iAddr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write(cmd, pData, iLen, true);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return (ret == ESP_OK);
} /* I2CWrite() */

int I2CRead(BBI2C *pI2C, unsigned char iAddr, unsigned char *pData, int iLen) {
  int i = 0;

  if (!pI2C->bWire) {
    i = I2CRead(pI2C, iAddr, pData, iLen);
  } else {
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
      ESP_LOGE("bb_temperature", "insufficient memory for I2C transaction");
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (iAddr << 1) | I2C_MASTER_READ, true);
    if (iLen > 1) {
      i2c_master_read(cmd, pData, iLen - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, pData + iLen - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
      i = iLen;
    }
    i2c_cmd_link_delete(cmd);
  }
  return i;
} /* I2CRead() */

int I2CReadRegister(BBI2C *pI2C, unsigned char iAddr, unsigned char u8Register, unsigned char *pData, int iLen) {
  I2CWrite(pI2C, iAddr, &u8Register, 1);
  I2CRead(pI2C, iAddr, pData, iLen);
  return iLen;
} /* I2CReadRegister() */
