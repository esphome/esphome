#pragma once
// Software INA219 at 0x40 answered on the master's own pads (GPIO32 SDA, GPIO33 SCL) plus fault injection.
// The slave drives a line low by switching the pad's output mux from the I2C0 signal to the plain GPIO output
// (level 0), which is a wired AND with the master. It runs inside a GPIO ISR on core 0 that spins through one
// transaction. Long stuck lines are held by a task on core 0 that re-asserts every tick.
#include <cstdint>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"
#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace inasim {
static const char *const TAG = "inasim";
constexpr int SDA = 32, SCL = 33;
constexpr uint32_t SDA_BIT = 1u << (SDA - 32), SCL_BIT = 1u << (SCL - 32);
constexpr uint32_t LOW_VAL = SIG_GPIO_OUT_IDX | GPIO_FUNC0_OEN_SEL;
constexpr uint32_t CPU_MHZ = 240;

inline volatile uint32_t &reg(uint32_t a) { return *reinterpret_cast<volatile uint32_t *>(a); }
inline uint32_t out_sel_addr(int pin) { return GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin; }
inline uint32_t in() { return reg(GPIO_IN1_REG); }

enum Fault : uint8_t {
  F_GARBAGE = 0,  // flip a bit in returned data
  F_NACK,         // do not ack the address
  F_STRETCH,      // hold SCL low 0.2..20 ms inside the transaction, then continue
  F_HOLD_SCL,     // SCL stuck low 50..3000 ms (task)
  F_HOLD_SDA,     // SDA stuck low 50..3000 ms (task)
  F_GLITCH_SCL,   // 1..4 us low pulse on SCL while it is high
  F_GLITCH_SDA,   // 1..4 us low pulse on SDA while SCL is high (false START/STOP)
  F_VANISH,       // slave stops responding mid transaction
  F_COUNT
};

struct Stats {
  volatile uint32_t starts, matched, stops, idle_abort, cap_abort, rd_bytes, wr_bytes;
  volatile uint32_t fault[F_COUNT];
};
inline Stats st{};
inline uint32_t rel_sda = 0, rel_scl = 0;
inline volatile bool armed = false;
inline volatile uint32_t p_fault = 0;        // per mille of matched transactions
inline volatile uint32_t fault_mask = 0xFF;  // which faults may be chosen
inline volatile uint32_t hold_max_ms = 3000;
inline uint16_t regs[6] = {0x399F, 0x044C, (3410 << 3) | 2, 0, 0, 0};
inline uint8_t ptr = 0;
inline uint32_t rng = 0x2270A5C3;
inline uint32_t rnd() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
struct FaultEv {
  uint32_t ms;
  uint8_t type;
  uint32_t arg;
};
inline FaultEv ring[64];
inline volatile uint32_t ring_w = 0;
inline uint32_t ring_r = 0;
inline void note(uint8_t type, uint32_t arg) {
  FaultEv &e = ring[ring_w & 63];
  e.ms = (uint32_t) (esp_timer_get_time() / 1000);
  e.type = type;
  e.arg = arg;
  ring_w = ring_w + 1;
}
inline TaskHandle_t holder = nullptr;
inline volatile uint32_t hold_mask = 0, hold_ms = 0;
inline volatile bool holding = false;

inline void drive_low(uint32_t bits) {
  reg(GPIO_OUT1_W1TC_REG) = bits;
  reg(GPIO_ENABLE1_W1TS_REG) = bits;
  if (bits & SDA_BIT)
    reg(out_sel_addr(SDA)) = LOW_VAL;
  if (bits & SCL_BIT)
    reg(out_sel_addr(SCL)) = LOW_VAL;
}
inline void release(uint32_t bits) {
  if (bits & SDA_BIT)
    reg(out_sel_addr(SDA)) = rel_sda;
  if (bits & SCL_BIT)
    reg(out_sel_addr(SCL)) = rel_scl;
}
inline void spin_us(uint32_t us) {
  uint32_t s = esp_cpu_get_cycle_count();
  while (esp_cpu_get_cycle_count() - s < us * CPU_MHZ) {
  }
}

inline void update_derived() {
  // current = shunt * cal / 4096, power = current * bus / 5000 (datasheet scaling)
  int32_t shunt = (int16_t) regs[1];
  uint32_t cal = regs[5];
  int32_t cur = (int32_t) ((int64_t) shunt * cal / 4096);
  regs[4] = (uint16_t) cur;
  regs[3] = (uint16_t) ((int64_t) (cur < 0 ? -cur : cur) * (regs[2] >> 3) / 5000);
}

inline void start_hold(uint32_t bits, uint32_t ms, BaseType_t *woken) {
  hold_mask = bits;
  hold_ms = ms;
  holding = true;
  drive_low(bits);
  vTaskNotifyGiveFromISR(holder, woken);
}

// Spins through one transaction from a START. Returns when STOP, idle or cap hits.
inline void run_transaction(BaseType_t *woken) {
  enum { ADDR, WDATA, RDATA, IGNORE } phase = ADDR;
  uint32_t prev = in();
  uint32_t start = esp_cpu_get_cycle_count(), last = start;
  uint8_t byte = 0, cnt = 0, out = 0, rbyte_idx = 0, wbyte_idx = 0, wmsb = 0;
  bool acked = false, mack = false, sda_driven = false;
  int fault = -1;
  uint32_t fault_at = 0, rises = 0;
  bool garbage_done = false;
  auto set_sda = [&](bool low) {
    if (low && !sda_driven) {
      drive_low(SDA_BIT);
      sda_driven = true;
    } else if (!low && sda_driven) {
      release(SDA_BIT);
      sda_driven = false;
    }
  };
  auto load_byte = [&]() {
    uint16_t v = ptr < 6 ? regs[ptr] : 0xFFFF;
    out = rbyte_idx == 0 ? (v >> 8) : (rbyte_idx == 1 ? (v & 0xFF) : 0xFF);
    if (fault == F_GARBAGE && !garbage_done) {
      out ^= 1u << (rnd() & 7);
      note(F_GARBAGE, out);
      garbage_done = true;
    }
    rbyte_idx++;
  };
  for (;;) {
    uint32_t v = in();
    uint32_t now = esp_cpu_get_cycle_count();
    if (v == prev) {
      if (now - last > 15000u * CPU_MHZ) {
        st.idle_abort++;
        break;
      }
      if (now - start > 60000u * CPU_MHZ) {
        st.cap_abort++;
        break;
      }
      continue;
    }
    last = now;
    uint32_t ch = v ^ prev;
    prev = v;
    if (ch & SCL_BIT) {
      if (v & SCL_BIT) {
        // rising edge: sample
        cnt++;
        rises++;
        if (cnt <= 8 && (phase == ADDR || phase == WDATA))
          byte = (byte << 1) | ((v & SDA_BIT) ? 1 : 0);
        else if (cnt == 9 && phase == RDATA)
          mack = !(v & SDA_BIT);
        if (fault >= 0 && rises == fault_at) {
          if (fault == F_GLITCH_SCL || fault == F_GLITCH_SDA) {
            spin_us(2);
            uint32_t b = fault == F_GLITCH_SCL ? SCL_BIT : SDA_BIT;
            bool was = b == SDA_BIT && sda_driven;
            if (!was) {
              note(fault, rises);
              drive_low(b);
              spin_us(1 + (rnd() & 3));
              release(b);
            }
            fault = -1;
            prev = in();
          }
        }
      } else {
        // falling edge: drive
        if (fault >= 0 && rises == fault_at) {
          if (fault == F_STRETCH) {
            drive_low(SCL_BIT);
            uint32_t us = 200 + rnd() % 19800;
            note(F_STRETCH, us);
            spin_us(us);
            release(SCL_BIT);
            fault = -1;
            prev = in();
            last = esp_cpu_get_cycle_count();
          } else if (fault == F_HOLD_SCL) {
            set_sda(false);
            uint32_t ms = 50 + rnd() % hold_max_ms;
            note(F_HOLD_SCL, ms);
            start_hold(SCL_BIT, ms, woken);
            return;
          } else if (fault == F_HOLD_SDA) {
            drive_low(SDA_BIT);
            sda_driven = false;
            uint32_t ms = 50 + rnd() % hold_max_ms;
            note(F_HOLD_SDA, ms);
            start_hold(SDA_BIT, ms, woken);
            return;
          } else if (fault == F_VANISH) {
            note(F_VANISH, rises);
            set_sda(false);
            return;
          }
        }
        if (cnt == 8) {
          if (phase == ADDR) {
            bool match = (byte >> 1) == 0x40;
            if (match) {
              st.matched++;
              if (armed && p_fault && (rnd() % 1000) < p_fault) {
                uint32_t f;
                do {
                  f = rnd() % F_COUNT;
                } while (!(fault_mask & (1u << f)));
                fault = f;
                fault_at = rises + 1 + rnd() % 30;
                st.fault[f]++;
              }
            }
            if (match && fault == F_NACK)
              note(F_NACK, 0);
            if (match && fault != F_NACK) {
              set_sda(true);
              acked = true;
              phase = (byte & 1) ? RDATA : WDATA;
              rbyte_idx = 0;
              wbyte_idx = 0;
            } else {
              phase = IGNORE;
              acked = false;
            }
          } else if (phase == WDATA) {
            st.wr_bytes++;
            if (wbyte_idx == 0)
              ptr = byte;
            else if (wbyte_idx == 1)
              wmsb = byte;
            else if (wbyte_idx == 2 && ptr < 6) {
              uint16_t w = (wmsb << 8) | byte;
              if (ptr == 0 && (w & 0x8000))
                w = 0x399F;
              if (ptr == 0 || ptr == 5)
                regs[ptr] = w;
              update_derived();
            }
            wbyte_idx++;
            set_sda(true);
            acked = true;
          } else if (phase == RDATA) {
            set_sda(false);  // master acks
            acked = false;
          }
        } else if (cnt == 9) {
          if (acked) {
            set_sda(false);
            acked = false;
          }
          if (phase == RDATA) {
            if (rbyte_idx == 0 || mack) {
              load_byte();
              st.rd_bytes++;
              set_sda(!(out & 0x80));
            } else {
              phase = IGNORE;
            }
          }
          cnt = 0;
          byte = 0;
        } else if (cnt >= 1 && cnt <= 7 && phase == RDATA) {
          set_sda(!(out & (0x80 >> cnt)));
        }
      }
    } else if ((ch & SDA_BIT) && (v & SCL_BIT)) {
      if (v & SDA_BIT) {
        st.stops++;
        break;
      }
      // repeated START
      st.starts++;
      set_sda(false);
      phase = ADDR;
      cnt = 0;
      byte = 0;
      acked = false;
    }
  }
  set_sda(false);
}

inline void slave_isr(void *) {
  reg(GPIO_STATUS1_W1TC_REG) = SDA_BIT;
  if (holding)
    return;
  uint32_t v = in();
  if (!(v & SCL_BIT) || (v & SDA_BIT))
    return;
  st.starts++;
  BaseType_t woken = pdFALSE;
  run_transaction(&woken);
  reg(GPIO_STATUS1_W1TC_REG) = SDA_BIT;
  if (woken)
    portYIELD_FROM_ISR();
}

inline void holder_task(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint32_t end = (esphome::millis) () + hold_ms;
    while ((int32_t) (end - (esphome::millis) ()) > 0) {
      drive_low(hold_mask);
      vTaskDelay(1);
    }
    release(hold_mask);
    holding = false;
  }
}

inline volatile uint32_t hb = 0;
inline volatile bool diag_recover = false;
constexpr uint32_t I2C0 = 0x3FF53000;
inline void dump_i2c(const char *why, uint32_t stale) {
  esp_rom_printf("DIAG %s stale=%ums ctr=%08x sr=%08x to=%08x raw=%08x ena=%08x st=%08x fifo=%08x hold=%d in=%x\n", why,
                 (unsigned) stale, (unsigned) reg(I2C0 + 0x04), (unsigned) reg(I2C0 + 0x08),
                 (unsigned) reg(I2C0 + 0x0c), (unsigned) reg(I2C0 + 0x20), (unsigned) reg(I2C0 + 0x28),
                 (unsigned) reg(I2C0 + 0x2c), (unsigned) reg(I2C0 + 0x18), (int) holding, (unsigned) (in() & 3));
  esp_rom_printf("DIAG cmd:");
  for (int i = 0; i < 16; i++)
    esp_rom_printf(" %08x", (unsigned) reg(I2C0 + 0x58 + 4 * i));
  esp_rom_printf("\nDIAG app_intr_status %08x %08x %08x pro %08x %08x %08x\n", (unsigned) reg(0x3FF000F8),
                 (unsigned) reg(0x3FF000FC), (unsigned) reg(0x3FF00100), (unsigned) reg(0x3FF000EC),
                 (unsigned) reg(0x3FF000F0), (unsigned) reg(0x3FF000F4));
}
inline void monitor_task(void *) {
  uint32_t stage = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(50));
    uint32_t now = (esphome::millis) ();
    uint32_t h = hb;
    if (h == 0)
      continue;
    uint32_t stale = now - h;
    if (stale < 1000) {
      if (stage >= 3)
        esp_rom_printf("DIAG loop resumed after %u ms, stage %u\n", (unsigned) stale, (unsigned) stage);
      stage = 0;
      continue;
    }
    if (stage == 0 && stale >= 1000) {
      dump_i2c("stuck1", stale);
      stage = 1;
    } else if (stage == 1 && stale >= 1500) {
      dump_i2c("stuck2", stale);
      stage = 2;
    } else if (stage == 2 && stale >= 2500) {
      dump_i2c("stuck3", stale);
      if (diag_recover) {
        reg(I2C0 + 0x28) = 0;
        reg(I2C0 + 0x24) = 0xFFFF;
        esp_rom_printf("DIAG cleared I2C0 int_ena\n");
      }
      stage = 3;
    } else if (stage == 3 && stale >= 3500) {
      dump_i2c("stuck4", stale);
      stage = 4;
    }
  }
}
inline volatile bool installed = false;
inline void install_task(void *) {
  rel_sda = reg(out_sel_addr(SDA));
  rel_scl = reg(out_sel_addr(SCL));
  // ESP_ERR_INVALID_STATE: the service is already installed, which is fine
  esp_err_t e1 = gpio_install_isr_service(0);
  esp_err_t et = gpio_set_intr_type((gpio_num_t) SDA, GPIO_INTR_NEGEDGE);
  esp_err_t e2 = gpio_isr_handler_add((gpio_num_t) SDA, slave_isr, nullptr);
  ESP_LOGI(TAG, "slave on core %d: isr service %d, intr type %d, handler %d, rel_sda 0x%x rel_scl 0x%x",
           xPortGetCoreID(), e1, et, e2, (unsigned) rel_sda, (unsigned) rel_scl);
  if ((e1 == ESP_OK || e1 == ESP_ERR_INVALID_STATE) && et == ESP_OK && e2 == ESP_OK) {
    installed = true;
  } else {
    ESP_LOGE(TAG, "slave ISR install failed: service %s, intr type %s, handler %s", esp_err_to_name(e1),
             esp_err_to_name(et), esp_err_to_name(e2));
  }
  vTaskDelete(nullptr);
}

inline void install() {
  if (xTaskCreatePinnedToCore(holder_task, "inahold", 3072, nullptr, configMAX_PRIORITIES - 2, &holder, 0) != pdPASS ||
      xTaskCreatePinnedToCore(monitor_task, "inamon", 3072, nullptr, configMAX_PRIORITIES - 1, nullptr, 0) != pdPASS ||
      xTaskCreatePinnedToCore(install_task, "inainst", 3072, nullptr, 5, nullptr, 0) != pdPASS) {
    ESP_LOGE(TAG, "task creation failed, slave not installed");
    return;
  }
  for (int i = 0; i < 1000 && !installed; i++)
    vTaskDelay(1);
  if (!installed)
    ESP_LOGE(TAG, "slave install did not finish");
}

inline void drain() {
  static const char *const NAMES[F_COUNT] = {"garbage",     "nack",       "stretch_us", "hold_scl_ms",
                                             "hold_sda_ms", "glitch_scl", "glitch_sda", "vanish"};
  const uint32_t w = ring_w;
  if (w - ring_r > 64) {
    ESP_LOGW(TAG, "F dropped %u events (ring overrun)", (unsigned) (w - ring_r - 64));
    ring_r = w - 64;
  }
  while (ring_r != w) {
    const FaultEv &e = ring[ring_r & 63];
    ESP_LOGI(TAG, "F (%u) %s %u", (unsigned) e.ms, NAMES[e.type], (unsigned) e.arg);
    ring_r++;
  }
}
inline void log_stats() {
  ESP_LOGI(TAG,
           "SLAVE starts=%u matched=%u stops=%u idle_abort=%u cap_abort=%u rd=%u wr=%u | faults garbage=%u nack=%u "
           "stretch=%u hold_scl=%u hold_sda=%u glitch_scl=%u glitch_sda=%u vanish=%u",
           (unsigned) st.starts, (unsigned) st.matched, (unsigned) st.stops, (unsigned) st.idle_abort,
           (unsigned) st.cap_abort, (unsigned) st.rd_bytes, (unsigned) st.wr_bytes, (unsigned) st.fault[0],
           (unsigned) st.fault[1], (unsigned) st.fault[2], (unsigned) st.fault[3], (unsigned) st.fault[4],
           (unsigned) st.fault[5], (unsigned) st.fault[6], (unsigned) st.fault[7]);
}
}  // namespace inasim
