#pragma once
// Software I2C target at 0x48 on the classic ESP32 master's own pads (GPIO32 SDA, GPIO33 SCL), after the
// `i2c_isr_fault/inasim.hpp` pattern: a GPIO ISR on core 0 spins through each transaction and pulls a line low by
// switching the pad's output mux to GPIO output 0. ACKs every byte written to 0x48, answers reads with A0, A1, ...;
// NACKs other addresses. Every START, byte (with the ACK bit seen on the wire) and STOP goes into a log.
#include <cstdint>
#include <string>
#include "esphome/core/log.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace pds {
static const char *const TAG = "pds";
constexpr int SDA = 32, SCL = 33;
constexpr uint32_t SDA_BIT = 1u << (SDA - 32), SCL_BIT = 1u << (SCL - 32);
constexpr uint32_t LOW_VAL = SIG_GPIO_OUT_IDX | GPIO_FUNC0_OEN_SEL;
constexpr uint32_t CPU_MHZ = 240;
inline volatile uint32_t &reg(uint32_t a) { return *reinterpret_cast<volatile uint32_t *>(a); }
inline uint32_t out_sel_addr(int pin) { return GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin; }
inline uint32_t in() { return reg(GPIO_IN1_REG); }
inline uint32_t rel_sda = 0;

// log codes: 1 START, 2 RESTART, 3 STOP, 4 written byte / 6 read byte (next: value, ack), 5 idle abort
constexpr size_t LOGN = 4096;
inline DRAM_ATTR uint8_t lg[LOGN];
inline volatile uint32_t ln = 0, lovf = 0;
inline void put(uint8_t v) {
  if (ln < LOGN)
    lg[ln++] = v;
  else
    lovf = lovf + 1;
}

inline void sda_low(bool low) { reg(out_sel_addr(SDA)) = low ? LOW_VAL : rel_sda; }

inline void run_transaction() {
  enum { ADDR, WDATA, RDATA, IGNORE } phase = ADDR;
  uint32_t prev = in(), last = esp_cpu_get_cycle_count();
  uint8_t byte = 0, cnt = 0, out = 0, rd = 0, pend_kind = 4, pend_val = 0;
  bool acked = false;
  put(1);
  reg(GPIO_OUT1_W1TC_REG) = SDA_BIT;
  reg(GPIO_ENABLE1_W1TS_REG) = SDA_BIT;
  for (;;) {
    uint32_t v = in();
    uint32_t now = esp_cpu_get_cycle_count();
    if (v == prev) {
      if (now - last > 20000u * CPU_MHZ) {
        put(5);
        break;
      }
      continue;
    }
    last = now;
    uint32_t ch = v ^ prev;
    prev = v;
    if (ch & SCL_BIT) {
      if (v & SCL_BIT) {
        cnt++;
        if (cnt <= 8 && phase != RDATA)
          byte = (byte << 1) | ((v & SDA_BIT) ? 1 : 0);
        else if (cnt == 9) {
          // the ACK bit as it is on the wire
          bool ack = !(v & SDA_BIT);
          put(pend_kind);
          put(pend_val);
          put(ack ? 1 : 0);
          if (phase == RDATA && !ack)
            phase = IGNORE;
        }
      } else {
        if (cnt == 8) {
          pend_kind = phase == RDATA ? 6 : 4;
          pend_val = phase == RDATA ? out : byte;
          if (phase == ADDR) {
            if ((byte >> 1) == 0x48) {
              sda_low(true);
              acked = true;
              phase = (byte & 1) ? RDATA : WDATA;
              rd = 0;
            } else {
              phase = IGNORE;
            }
          } else if (phase == WDATA) {
            sda_low(true);
            acked = true;
          } else if (phase == RDATA) {
            sda_low(false);  // master's ACK slot
          }
        } else if (cnt == 9) {
          if (acked) {
            sda_low(false);
            acked = false;
          }
          if (phase == RDATA) {
            out = 0xA0 + rd++;
            sda_low(!(out & 0x80));
          }
          cnt = 0;
          byte = 0;
        } else if (cnt >= 1 && cnt <= 7 && phase == RDATA) {
          sda_low(!(out & (0x80 >> cnt)));
        }
      }
    } else if ((ch & SDA_BIT) && (v & SCL_BIT)) {
      if (v & SDA_BIT) {
        put(3);
        break;
      }
      put(2);
      sda_low(false);
      phase = ADDR;
      cnt = 0;
      byte = 0;
      acked = false;
    }
  }
  sda_low(false);
}

inline void slave_isr(void *) {
  reg(GPIO_STATUS1_W1TC_REG) = SDA_BIT;
  uint32_t v = in();
  if (!(v & SCL_BIT) || (v & SDA_BIT))
    return;
  run_transaction();
  reg(GPIO_STATUS1_W1TC_REG) = SDA_BIT;
}

inline volatile bool installed = false;
inline void install_task(void *) {
  rel_sda = reg(out_sel_addr(SDA));
  gpio_install_isr_service(0);
  gpio_set_intr_type((gpio_num_t) SDA, GPIO_INTR_NEGEDGE);
  esp_err_t e = gpio_isr_handler_add((gpio_num_t) SDA, slave_isr, nullptr);
  gpio_intr_enable((gpio_num_t) SDA);
  ESP_LOGI(TAG, "slave on core %d handler %d rel_sda 0x%x", xPortGetCoreID(), e, (unsigned) rel_sda);
  installed = true;
  vTaskDelete(nullptr);
}
inline void install() {
  xTaskCreatePinnedToCore(install_task, "pdsinst", 3072, nullptr, 5, nullptr, 0);
  while (!installed)
    vTaskDelay(1);
}

inline std::string tokens() {
  std::string s;
  char t[8];
  for (uint32_t i = 0; i < ln; i++) {
    switch (lg[i]) {
      case 1:
        s += "S ";
        break;
      case 2:
        s += "Sr ";
        break;
      case 3:
        s += "P ";
        break;
      case 5:
        s += "IDLE ";
        break;
      case 4:
      case 6: {
        if (i + 2 >= ln) {  // byte record cut off by a full log
          s += "? ";
          i = ln;
          break;
        }
        uint8_t b = lg[i + 1], a = lg[i + 2];
        snprintf(t, sizeof t, lg[i] == 6 ? "r%02X%c " : "%02X%c ", b, a ? '+' : '-');
        i += 2;
        s += t;
        break;
      }
      default:
        s += "? ";
    }
  }
  return s;
}
}  // namespace pds
