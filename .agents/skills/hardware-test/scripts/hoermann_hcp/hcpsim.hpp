#pragma once
// Fake Hoermann HCP2 bus master (the door motor) on a UART cross-wired to the hoermann_hcp modbus server UART.
// Runs a fixed scenario in its own FreeRTOS task and logs every frame it sends and every reply it gets.
// Main-loop calls into the component (open_door, set_light) are handed over through `want`.
#include <atomic>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace hcpsim {
static const char *const TAG = "hcpsim";

struct Master {
  esphome::uart::UARTComponent *u{nullptr};
  std::atomic<int> want{0};  // 1 open_door, 2 light on: run by main()
  uint8_t counter{0};
  uint16_t state_reg{0x4000};  // closed
  uint8_t pos{0};
  uint16_t light_reg{0x0000};
  int pass{0}, fail{0};
  uint8_t rx[300];
  size_t rxn{0};
  uint32_t last_latency{0};

  void hex(const char *what, const uint8_t *b, size_t n) {
    char s[3 * 64 + 1];
    size_t k = 0;
    for (size_t i = 0; i < n && i < 64; i++)
      k += snprintf(s + k, sizeof(s) - k, "%02X ", b[i]);
    s[k] = 0;
    ESP_LOGI(TAG, "%s (%u): %s", what, (unsigned) n, s);
  }
  void check(bool ok, const char *what) {
    if (ok)
      pass++;
    else
      fail++;
    ESP_LOGI(TAG, "%s %s", ok ? "CHECK PASS" : "CHECK FAIL", what);
  }
  void drain() {
    uint8_t b;
    while (u->available())
      u->read_byte(&b);
  }
  void send(uint8_t *f, size_t n, bool log = true) {
    uint16_t crc = esphome::crc16(f, n);
    f[n] = crc & 0xFF;
    f[n + 1] = crc >> 8;
    drain();
    if (log)
      hex("TX", f, n + 2);
    u->write_array(f, n + 2);
    u->flush();
  }
  // Collect a reply: up to timeout_ms for the first byte, then until 15 ms of silence.
  size_t recv(uint32_t timeout_ms) {
    rxn = 0;
    uint32_t start = esphome::millis(), last = 0;
    while (true) {
      uint32_t now = esphome::millis();
      if (u->available()) {
        while (u->available() && rxn < sizeof(rx))
          u->read_byte(&rx[rxn++]);
        if (last == 0)
          last_latency = now - start;
        last = now;
        continue;
      }
      if (rxn == 0 && now - start > timeout_ms)
        return 0;
      if (rxn > 0 && now - last > 15)
        return rxn;
      vTaskDelay(1);
    }
  }
  bool crc_ok() { return rxn >= 4 && esphome::crc16(rx, rxn) == 0; }

  // FC 0x17: read `rq` registers at rd, write `wn` registers at wr. Returns the number of registers read back,
  // -1 for no reply, -2 for an exception or a bad frame. regs gets the values.
  int rw(uint8_t addr, uint16_t rd, uint16_t rq, uint16_t wr, const uint16_t *w, uint8_t wn, uint16_t *regs,
         bool log = true) {
    uint8_t f[64];
    size_t n = 0;
    f[n++] = addr;
    f[n++] = 0x17;
    f[n++] = rd >> 8;
    f[n++] = rd;
    f[n++] = rq >> 8;
    f[n++] = rq;
    f[n++] = wr >> 8;
    f[n++] = wr;
    f[n++] = 0;
    f[n++] = wn;
    f[n++] = wn * 2;
    for (int i = 0; i < wn; i++) {
      f[n++] = w[i] >> 8;
      f[n++] = w[i];
    }
    send(f, n, log);
    if (recv(300) == 0) {
      ESP_LOGW(TAG, "no reply from 0x%02X", addr);
      return -1;
    }
    if (log || !crc_ok() || rx[1] != 0x17)
      hex("RX", rx, rxn);
    if (log)
      ESP_LOGI(TAG, "reply after %u ms", (unsigned) last_latency);
    if (!crc_ok()) {
      ESP_LOGW(TAG, "bad CRC");
      return -2;
    }
    if (rx[1] & 0x80) {
      ESP_LOGW(TAG, "exception 0x%02X code %u", rx[1], rx[2]);
      return -2;
    }
    int cnt = rx[2] / 2;
    for (int i = 0; i < cnt; i++)
      regs[i] = (rx[3 + 2 * i] << 8) | rx[4 + 2 * i];
    return cnt;
  }

  // Bus scan as an HCP2 motor sends it: write 3 to 0x9C41, read 5 from 0x9CB9.
  int scan(uint8_t addr, uint8_t cnt, uint8_t cmd, uint16_t *regs) {
    uint16_t w[3] = {(uint16_t) (cnt << 8 | cmd), 0x0000, 0x0102};
    int r = rw(addr, 0x9CB9, 5, 0x9C41, w, 3, regs);
    if (r == 5)
      ESP_LOGI(TAG, "SCAN reply regs %04X %04X %04X %04X %04X", regs[0], regs[1], regs[2], regs[3], regs[4]);
    return r;
  }
  // Status poll: write 2, read 8. Returns the command pair in regs[2], regs[3].
  int poll(uint16_t *regs, bool log = false) {
    counter++;
    uint16_t w[2] = {(uint16_t) (counter << 8 | 0x03), 0x0000};
    int r = rw(0x02, 0x9CB9, 8, 0x9C41, w, 2, regs, log);
    if (r == 8 && (regs[2] != 0 || regs[3] != 0))
      ESP_LOGI(TAG, "POLL got command %04X %04X", regs[2], regs[3]);
    return r;
  }
  void broadcast() {
    uint8_t f[40];
    size_t n = 0;
    uint16_t r[9] = {0x0000, pos, state_reg, 0x0000, 0x0000, 0x0000, light_reg, 0x0000, 0x0000};
    f[n++] = 0x00;
    f[n++] = 0x10;
    f[n++] = 0x9D;
    f[n++] = 0x31;
    f[n++] = 0;
    f[n++] = 9;
    f[n++] = 18;
    for (int i = 0; i < 9; i++) {
      f[n++] = r[i] >> 8;
      f[n++] = r[i];
    }
    send(f, n, false);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  // One bus cycle: a broadcast and a status poll, then idle.
  uint16_t cycle_regs[8];
  int cycle() {
    broadcast();
    vTaskDelay(pdMS_TO_TICKS(20));
    int r = poll(cycle_regs);
    vTaskDelay(pdMS_TO_TICKS(60));
    return r;
  }
  int run_cycles(int n, uint16_t watch = 0) {
    int seen = 0;
    for (int i = 0; i < n; i++) {
      if (cycle() == 8 && watch != 0 && cycle_regs[2] == watch)
        seen++;
    }
    return seen;
  }

  void scenario() {
    uint16_t regs[16];
    vTaskDelay(pdMS_TO_TICKS(4000));
    ESP_LOGI(TAG, "=== case scan_first (counter 00, command 02)");
    int r = scan(0x02, 0x00, 0x02, regs);
    check(r == 5, "scan answered with 5 registers and a valid CRC");
    bool official =
        r == 5 && regs[0] == 0x0000 && regs[1] == 0x022E && regs[2] == 0x0520 && regs[3] == 0x0800 && regs[4] == 0x0000;
    bool legacy =
        r == 5 && regs[0] == 0x0000 && regs[1] == 0x0205 && regs[2] == 0x0430 && regs[3] == 0x10FF && regs[4] == 0xA845;
    ESP_LOGI(TAG, "SCAN answer kind: %s", official ? "official gateway (0x2E)" : legacy ? "UAP (0x05)" : "other");

    ESP_LOGI(TAG, "=== case scan_echo (counter 34, command 07)");
    r = scan(0x02, 0x34, 0x07, regs);
    check(r == 5 && regs[0] == 0x3400 && (regs[1] >> 8) == 0x07, "scan echoes counter and command");

    ESP_LOGI(TAG, "=== case run_closed");
    run_cycles(30);
    check(cycle_regs[1] == (uint16_t) (0x0301), "status poll answers 03 01");

    ESP_LOGI(TAG, "=== case open_command");
    want = 1;
    int seen = 0;
    for (int i = 0; i < 20 && seen == 0; i++)
      if (cycle() == 8 && cycle_regs[2] == 0x0110)
        seen++;
    check(seen == 1, "open command 0x0110 fetched by status poll");
    state_reg = 0x0100;  // opening
    for (int p = 0; p <= 200; p += 20) {
      pos = p;
      run_cycles(1);
    }
    state_reg = 0x2000;  // open
    run_cycles(10);

    ESP_LOGI(TAG, "=== case light_on");
    want = 2;
    seen = 0;
    for (int i = 0; i < 20 && seen == 0; i++)
      if (cycle() == 8 && cycle_regs[2] == 0x0880)
        seen++;
    check(seen == 1, "light on command 0x0880 fetched");
    light_reg = 0x0010;
    run_cycles(10);

    ESP_LOGI(TAG, "=== case bus_silent_3s");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "=== case rescan_after_silence");
    r = scan(0x02, 0x00, 0x02, regs);
    check(r == 5, "rescan after silence answered");
    run_cycles(20);

    ESP_LOGI(TAG, "=== case scan_other_address");
    r = scan(0x03, 0x00, 0x02, regs);
    check(r == -1, "scan of address 3 not answered");

    ESP_LOGI(TAG, "=== case read_6_registers");
    uint16_t w3[3] = {0x0002, 0x0000, 0x0102};
    r = rw(0x02, 0x9CB9, 6, 0x9C41, w3, 3, regs);
    check(r == 6, "unknown read length padded");

    ESP_LOGI(TAG, "=== case read_wrong_address");
    r = rw(0x02, 0x1234, 5, 0x9C41, w3, 3, regs);
    check(r == -2, "wrong read address rejected with an exception");

    ESP_LOGI(TAG, "=== case scan_back_to_back x20");
    int ok = 0;
    for (int i = 0; i < 20; i++) {
      if (scan(0x02, (uint8_t) i, 0x02, regs) == 5 && (regs[1] & 0xFF) == (official ? 0x2E : 0x05))
        ok++;
      vTaskDelay(pdMS_TO_TICKS(30));
    }
    ESP_LOGI(TAG, "back to back scans answered: %d/20", ok);
    check(ok == 20, "20 back to back scans answered consistently");
    run_cycles(20);
    ESP_LOGI(TAG, "DONE pass=%d fail=%d answer=%s", pass, fail,
             official ? "0x2E official"
             : legacy ? "0x05 UAP"
                      : "other");
  }

  static void task(void *arg) {
    static_cast<Master *>(arg)->scenario();
    vTaskDelete(nullptr);
  }
  void start(esphome::uart::UARTComponent *uart) {
    u = uart;
    xTaskCreatePinnedToCore(task, "hcpsim", 6144, this, 5, nullptr, 0);
  }
};

inline Master master;
}  // namespace hcpsim
