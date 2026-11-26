#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP32
// Fake TCA9548A at 0x70 (one data byte sets the channel mask, a read returns it) with devices 0x3C on channel 2 and
// 0x48 on channel 5, plus the SCL rate of each transaction (code 7). It extends `../dev/pdslave.hpp`: a software target
// on the classic ESP32 master's own pads (GPIO32 SDA, GPIO33 SCL), a GPIO ISR on core 0 that spins through each
// transaction and pulls a line low through the pad's output mux. Reads of a device return A0, A1, ...; other addresses
// are NACKed. Every START, byte (with the ACK bit seen on the wire) and STOP goes into a log.
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
inline volatile uint8_t mux_mask = 0;
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
  uint8_t byte = 0, cnt = 0, out = 0, rd = 0, pend_kind = 4, pend_val = 0, addr = 0;
  uint32_t t1 = 0, t2 = 0;
  int wbytes = 0;
  bool acked = false, stopped = false;
  const uint32_t first = esp_cpu_get_cycle_count();
  put(1);
  reg(GPIO_OUT1_W1TC_REG) = SDA_BIT;
  reg(GPIO_ENABLE1_W1TS_REG) = SDA_BIT;
  for (;;) {
    uint32_t v = in();
    uint32_t now = esp_cpu_get_cycle_count();
    if (v == prev) {
      // after a STOP keep listening a little: a START right behind it would otherwise be lost while the ISR exits
      if (stopped && now - last > 500u * CPU_MHZ)
        break;
      if (!stopped && now - last > 20000u * CPU_MHZ) {
        put(5);
        break;
      }
      continue;
    }
    last = now;
    // never hold core 0 near the interrupt watchdog (raised to 1 s in the config)
    if (now - first > 600000u * CPU_MHZ) {
      put(5);
      break;
    }
    uint32_t ch = v ^ prev;
    prev = v;
    if (ch & SCL_BIT) {
      if (v & SCL_BIT) {
        cnt++;
        if (phase == ADDR && cnt == 2)
          t1 = now;
        else if (phase == ADDR && cnt == 3)
          t2 = now;
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
            put(7);
            put(t2 > t1 ? (uint8_t) ((CPU_MHZ * 1000u) / (t2 - t1)) : 0);
            addr = byte >> 1;
            wbytes = 0;
            bool match = addr == 0x70 || (addr == 0x3C && (mux_mask & 0x04)) || (addr == 0x48 && (mux_mask & 0x20));
            if (match) {
              sda_low(true);
              acked = true;
              phase = (byte & 1) ? RDATA : WDATA;
              rd = 0;
            } else {
              phase = IGNORE;
            }
          } else if (phase == WDATA) {
            if (addr == 0x70 && wbytes == 0)
              mux_mask = byte;
            wbytes++;
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
            out = addr == 0x70 ? mux_mask : (uint8_t) (0xA0 + rd++);
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
        stopped = true;
        phase = IGNORE;
        cnt = 0;
        continue;
      }
      put(stopped ? 1 : 2);
      stopped = false;
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
  // a full log is marked up front so the case shows as truncated, not as a matching stream
  std::string s = lovf ? "OVF " : "";
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
      case 7: {
        snprintf(t, sizeof t, "@%uk ", lg[i + 1]);
        s += t;
        i += 1;
        break;
      }
      case 4:
      case 6: {
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
#endif  // USE_ESP32
