#pragma once
// Fake BME280/BMP280 I2C target on a Pico 2 W (I2C0, GP4 SDA, GP5 SCL), register file answered byte by byte.
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace picofake {
static const char *const TAG = "picofake";
static uint8_t regs[256];
static volatile uint8_t ptr = 0;
static volatile bool first = true;
static volatile uint32_t n_rd = 0, n_data = 0, n_ctrl = 0, n_hum = 0, n_abrt = 0;
static volatile int mode = 0;

static void put16(uint8_t r, uint16_t v) { esphome::put_le16(&regs[r], v); }

// mode 0: datasheet example raw (T 519888, P 415148, H 30000); 1: 0x80000/0x80000/0x8000
inline void set_mode(int m) {
  const uint8_t d0[8] = {0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x00, 0x75, 0x30};
  const uint8_t d1[8] = {0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00};
  memcpy(&regs[0xF7], m ? d1 : d0, 8);
  mode = m;
  ESP_LOGI(TAG, "MODE %d", m);
}

static void irq() {
  i2c_hw_t *hw = i2c_get_hw(i2c0);
  uint32_t st = hw->intr_stat;
  if (st & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
    hw->clr_tx_abrt;
    n_abrt++;
  }
  if (st & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
    while (i2c_get_read_available(i2c0)) {
      uint8_t b = (uint8_t) hw->data_cmd;
      if (first) {
        ptr = b;
        first = false;
      } else {
        uint8_t a = ptr++;
        if (a == 0xF4)
          n_ctrl++;
        else if (a == 0xF2)
          n_hum++;
        if (a >= 0xF2 && a <= 0xF5)
          regs[a] = b;
      }
    }
  }
  if (st & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
    if (ptr == 0xF7 || ptr == 0xFA)
      n_data++;
    n_rd++;
    hw->data_cmd = regs[ptr++];
    hw->clr_rd_req;
  }
  if (st & I2C_IC_INTR_STAT_R_START_DET_BITS) {
    hw->clr_start_det;
    first = true;
  }
  if (st & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
    hw->clr_stop_det;
    first = true;
  }
  if (st & I2C_IC_INTR_STAT_R_RESTART_DET_BITS) {
    hw->clr_restart_det;
    first = true;
  }
}

inline void start(uint8_t chip_id) {
  memset(regs, 0, sizeof(regs));
  regs[0xD0] = chip_id;
  put16(0x88, 27504);
  put16(0x8A, 26435);
  put16(0x8C, (uint16_t) -1000);
  put16(0x8E, 36477);
  put16(0x90, (uint16_t) -10685);
  put16(0x92, 3024);
  put16(0x94, 2855);
  put16(0x96, 140);
  put16(0x98, (uint16_t) -7);
  put16(0x9A, 15500);
  put16(0x9C, (uint16_t) -14600);
  put16(0x9E, 6000);
  if (chip_id == 0x60) {
    regs[0xA1] = 75;
    put16(0xE1, 362);
    regs[0xE3] = 0;
    const int h4 = 313, h5 = 50;
    regs[0xE4] = h4 >> 4;
    regs[0xE5] = (h4 & 0x0F) | ((h5 & 0x0F) << 4);
    regs[0xE6] = h5 >> 4;
    regs[0xE7] = 30;
  }
  set_mode(0);
  i2c_init(i2c0, 100000);
  i2c_set_slave_mode(i2c0, true, 0x76);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);
  i2c_hw_t *hw = i2c_get_hw(i2c0);
  hw->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS | I2C_IC_INTR_MASK_M_RD_REQ_BITS | I2C_IC_INTR_MASK_M_TX_ABRT_BITS |
                  I2C_IC_INTR_MASK_M_STOP_DET_BITS | I2C_IC_INTR_MASK_M_START_DET_BITS |
                  I2C_IC_INTR_MASK_M_RESTART_DET_BITS;
  irq_set_exclusive_handler(I2C0_IRQ, irq);
  irq_set_enabled(I2C0_IRQ, true);
  ESP_LOGI(TAG, "started chip_id=0x%02X", chip_id);
}

inline void report() {
  ESP_LOGI(TAG, "STAT mode=%d rd=%u data=%u ctrl=%u hum=%u abrt=%u", mode, (unsigned) n_rd, (unsigned) n_data,
           (unsigned) n_ctrl, (unsigned) n_hum, (unsigned) n_abrt);
}
}  // namespace picofake
