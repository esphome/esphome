#pragma once
// Scenario runner for the fake SX1276 (sxsim.hpp), esphome#22733. Called every 20 ms from an interval; each step logs
// "RES ..." lines. Build with -DSXSIM_PR for a driver with the asynchronous API (start_transmit_async and friends).
// Boot cases run through software restarts (RTC_NOINIT): 0 main LoRa+FSK sequence ending with a runtime mode change
// failure, 1 blocking send that never completes, 2 wrong chip version at setup, 3 mode change failure at setup,
// 4 done.
#include <cmath>
#include <esp_attr.h>
#include <esp_system.h>
#include "sxsim.hpp"

namespace sxrun {
using namespace esphome;
using namespace esphome::sx127x;
using sxsim::M;
static const char *const TAG = "ev";

RTC_NOINIT_ATTR static uint32_t g_case;
RTC_NOINIT_ATTR static uint32_t g_magic;
static constexpr uint32_t MAGIC = 0x5A127A55;
static uint32_t boot_case = 0;

inline const char *ename(SX127xError e) {
  static const char *const N[] = {"NONE", "TIMEOUT", "INVALID_PARAMS", "NOT_READY", "BUSY", "CANCELLED"};
  int i = (int) e;
  return (i >= 0 && i < 6) ? N[i] : "?";
}

struct PktL : SX127xListener {
  uint32_t n{0};
  void on_packet(const std::vector<uint8_t> &p, float rssi, float snr) override {
    n++;
    char h[80];
    sxsim::hex(h, sizeof(h), p.data(), std::min<size_t>(p.size(), 32));
    ESP_LOGI(TAG, "EV packet n=%u len=%u data=[%s] rssi=%.1f snr=%s%.2f", (unsigned) n, (unsigned) p.size(), h, rssi,
             std::isnan(snr) ? "NAN:" : "", snr);
  }
} P;

#ifdef SXSIM_PR
struct TxL : SX127xTransmitListener {
  const char *name;
  uint32_t n{0}, t0{0};
  SX127xError last{SX127xError::SX127X_ERROR_NONE};
  explicit TxL(const char *nm) : name(nm) {}
  void on_transmit_complete(SX127xError r) override {
    n++;
    last = r;
    ESP_LOGI(TAG, "EV txdone %s result=%s n=%u after=%ums chip=%s", name, ename(r), (unsigned) n,
             (unsigned) (millis() - t0), sxsim::mode_name());
  }
} L("L"), L2("L2");

inline SX127xError start_async(SX127x *r, TxL &l, std::initializer_list<uint8_t> d, uint32_t timeout = 4000) {
  std::vector<uint8_t> v(d);
  l.t0 = millis();
  uint32_t us = micros();
  SX127xError e = r->start_transmit_async(&l, v.data(), v.size(), timeout);
  us = micros() - us;
  ESP_LOGI(TAG, "RES async_start %s timeout=%u -> %s in %uus is_transmitting=%d chip=%s", l.name, (unsigned) timeout,
           ename(e), (unsigned) us, r->is_transmitting(), sxsim::mode_name());
  return e;
}
#endif

inline SX127xError blocking(SX127x *r, const char *label, std::initializer_list<uint8_t> d, bool trace = false) {
  std::vector<uint8_t> v(d);
  if (trace)
    sxsim::trace_start();
  uint32_t t = millis();
  SX127xError e = r->transmit_packet(v);
  t = millis() - t;
  if (trace) {
    sxsim::trace_stop();
    sxsim::log_trace(label);
  }
  ESP_LOGI(TAG, "RES %s blocking -> %s in %ums warn=%d failed=%d", label, ename(e), (unsigned) t,
           r->status_has_warning(), r->is_failed());
  sxsim::log_state(label);
  return e;
}

inline void to_fsk(SX127x *r) {
  r->set_modulation(MOD_FSK);
  r->set_bandwidth(SX127X_BW_50_0);
  r->set_bitrate(4800);
  r->set_deviation(5000);
  r->set_packet_mode(true);
  r->set_bitsync(true);
  r->set_sync_value({0x33, 0x33});
  r->configure();
}

inline void reboot_to(uint32_t c) {
  g_case = c;
  g_magic = MAGIC;
  ESP_LOGI(TAG, "REBOOT to case %u", (unsigned) c);
  delay(200);
  esp_restart();
}

// on_boot priority 800, after sxsim::install
inline void boot(SX127x *r) {
  boot_case = (esp_reset_reason() == ESP_RST_SW && g_magic == MAGIC) ? g_case : 0;
  g_magic = 0;
  ESP_LOGI(TAG, "BOOT case %u", (unsigned) boot_case);
  if (boot_case == 2)
    M.version = 0x22;
  if (boot_case == 3)
    M.fail_mode_writes = 1000000;
  r->register_listener(&P);
}

static int step = 0;
static uint32_t next_ms = 0;
static uint32_t prev_warn = 2, prev_failed = 2;
static uint32_t watch_until = 0;

inline void watch(SX127x *r) {
  uint32_t w = r->status_has_warning(), f = r->is_failed();
  if (w != prev_warn || f != prev_failed) {
    ESP_LOGI(TAG, "RES watch warn=%u failed=%u chip=%s ign_fault=%u resets=%u", (unsigned) w, (unsigned) f,
             sxsim::mode_name(), (unsigned) M.mode_ignored_fault, (unsigned) M.resets);
    prev_warn = w;
    prev_failed = f;
  }
}

#define WAIT(ms) next_ms = millis() + (ms)

inline void run(SX127x *r, bool *want_action) {
  if (millis() < watch_until)
    watch(r);
  if ((int32_t) (millis() - next_ms) < 0)
    return;
  const int s = step++;
  if (boot_case == 0) {
    switch (s) {
      case 0:
        ESP_LOGI(TAG, "RES setup failed=%d warn=%d", r->is_failed(), r->status_has_warning());
        sxsim::log_state("boot");
        WAIT(500);
        return;
      // ---------------- LoRa ----------------
      case 1:
        ESP_LOGI(TAG, "PHASE lora_blocking");
        blocking(r, "L1", {0x01, 0x02, 0x03, 0x04, 0x05}, true);
        WAIT(200);
        return;
      case 2: {
        int ok = 0;
        for (int i = 0; i < 5; i++)
          ok += blocking(r, "L1x", {(uint8_t) i, 0xAA, 0x55}) == SX127xError(0);
        ESP_LOGI(TAG, "RES L1x ok=%d of 5", ok);
        WAIT(200);
        return;
      }
      case 3:
        ESP_LOGI(TAG, "PHASE lora_action");
        *want_action = true;
        WAIT(500);
        return;
      case 4:
        sxsim::log_state("L2action");
        ESP_LOGI(TAG, "PHASE lora_receive");
        M.pkt_rssi = 100;
        M.pkt_snr = 22;
        sxsim::inject({0xA1, 0xA2, 0xA3, 0xA4});
        WAIT(300);
        return;
      case 5:
        ESP_LOGI(TAG, "RES L3 packets=%u", (unsigned) P.n);
        sxsim::inject({0xC1, 0xC2}, true);  // CRC error: no packet
        WAIT(300);
        return;
      case 6:
        ESP_LOGI(TAG, "RES L3crc packets=%u", (unsigned) P.n);
        sxsim::log_state("L3");
        ESP_LOGI(TAG, "PHASE lora_rx_before_send");
        sxsim::arm_rx_on_standby({0xB1, 0xB2, 0xB3});
        blocking(r, "L4", {0x07, 0x07, 0x07}, true);
        WAIT(500);
        return;
      case 7:
        ESP_LOGI(TAG, "RES L4 after packets=%u", (unsigned) P.n);
        sxsim::log_state("L4after");
        // the next send works and the radio receives again
        blocking(r, "L4next", {0x08, 0x08});
        sxsim::inject({0xD1, 0xD2});
        WAIT(300);
        return;
      case 8:
        ESP_LOGI(TAG, "RES L4next packets=%u", (unsigned) P.n);
        ESP_LOGI(TAG, "PHASE lora_raw_tx_mode");
        sxsim::trace_start();
        r->set_mode_tx();
        WAIT(400);
        return;
      case 9:
        sxsim::trace_stop();
        sxsim::log_trace("L5");
        ESP_LOGI(TAG, "RES L5 packets=%u", (unsigned) P.n);
        sxsim::log_state("L5");
        r->set_mode_rx();
        sxsim::log_state("L5rx");
        WAIT(200);
        return;
#ifdef SXSIM_PR
      case 10:
        ESP_LOGI(TAG, "PHASE lora_async");
        M.tx_ms = 80;
        start_async(r, L, {0x11, 0x12, 0x13, 0x14, 0x15});
        start_async(r, L2, {0x21});
        ESP_LOGI(TAG, "RES second_blocking -> %s", ename(r->transmit_packet(std::vector<uint8_t>{0x31})));
        WAIT(400);
        return;
      case 11:
        ESP_LOGI(TAG, "RES A1 L.n=%u L.last=%s L2.n=%u is_tx=%d", (unsigned) L.n, ename(L.last), (unsigned) L2.n,
                 r->is_transmitting());
        sxsim::log_state("A1");
        start_async(r, L2, {0x41}, 0);
        start_async(r, L2, {0x41}, 262144);
        ESP_LOGI(TAG, "RES null_listener -> %s", ename(r->start_transmit_async(nullptr, std::vector<uint8_t>{1})));
        start_async(r, L2, {0x42}, 262143);
        WAIT(400);
        return;
      case 12:
        ESP_LOGI(TAG, "RES A2 L2.n=%u L2.last=%s", (unsigned) L2.n, ename(L2.last));
        ESP_LOGI(TAG, "PHASE lora_async_timeout");
        M.tx_never = true;
        start_async(r, L, {0x51, 0x52}, 300);
        WAIT(800);
        return;
      case 13:
        M.tx_never = false;
        ESP_LOGI(TAG, "RES A3 L.n=%u L.last=%s warn=%d", (unsigned) L.n, ename(L.last), r->status_has_warning());
        sxsim::log_state("A3");
        ESP_LOGI(TAG, "PHASE lora_async_cancel");
        M.tx_ms = 1000;
        start_async(r, L, {0x61, 0x62});
        WAIT(50);
        return;
      case 14:
        ESP_LOGI(TAG, "RES cancel_other -> %d", r->cancel_transmit_async(&L2));
        ESP_LOGI(TAG, "RES cancel_owner -> %d (L.n=%u last=%s) is_tx=%d chip=%s", r->cancel_transmit_async(&L),
                 (unsigned) L.n, ename(L.last), r->is_transmitting(), sxsim::mode_name());
        ESP_LOGI(TAG, "RES cancel_again -> %d", r->cancel_transmit_async(&L));
        WAIT(1300);
        return;
      case 15:
        ESP_LOGI(TAG, "RES A4 L.n=%u last=%s warn=%d", (unsigned) L.n, ename(L.last), r->status_has_warning());
        sxsim::log_state("A4");
        ESP_LOGI(TAG, "PHASE lora_actions_during_send");
        M.tx_ms = 500;
        start_async(r, L, {0x71});
        {
          uint32_t w0 = M.mode_writes;
          r->set_mode_standby();
          r->set_mode_sleep();
          r->set_mode_rx();
          r->set_mode_tx();
          r->run_image_cal();
          r->configure();
          ESP_LOGI(TAG, "RES A5 mode_writes_during=%u chip=%s is_tx=%d warn=%d", (unsigned) (M.mode_writes - w0),
                   sxsim::mode_name(), r->is_transmitting(), r->status_has_warning());
        }
        WAIT(800);
        return;
      case 16:
        ESP_LOGI(TAG, "RES A5 L.n=%u last=%s", (unsigned) L.n, ename(L.last));
        sxsim::log_state("A5");
        ESP_LOGI(TAG, "PHASE lora_async_rx_before_send");
        M.tx_ms = 50;
        sxsim::arm_rx_on_standby({0xE1, 0xE2, 0xE3});
        {
          uint32_t n0 = L.n, p0 = P.n;
          start_async(r, L, {0x81});
          uint32_t w0 = M.mode_writes;
          r->set_mode_sleep();  // ignored while the packet is held
          ESP_LOGI(TAG, "RES A6 completions=%u packets_now=%u mode_writes_by_action=%u chip=%s", (unsigned) (L.n - n0),
                   (unsigned) (P.n - p0), (unsigned) (M.mode_writes - w0), sxsim::mode_name());
        }
        WAIT(300);
        return;
      case 17:
        ESP_LOGI(TAG, "RES A6 after packets=%u L.n=%u", (unsigned) P.n, (unsigned) L.n);
        sxsim::log_state("A6");
        start_async(r, L, {0x82});
        WAIT(300);
        return;
      case 18:
        ESP_LOGI(TAG, "RES A6b L.n=%u last=%s", (unsigned) L.n, ename(L.last));
        sxsim::log_state("A6b");
        WAIT(10);
        return;
#endif
      // ---------------- FSK ----------------
      case 20:
        ESP_LOGI(TAG, "PHASE fsk_configure");
        M.tx_ms = 50;
        to_fsk(r);
        sxsim::log_state("F0");
        ESP_LOGI(TAG, "RES F0 warn=%d failed=%d", r->status_has_warning(), r->is_failed());
        WAIT(300);
        return;
      case 21:
        ESP_LOGI(TAG, "PHASE fsk_blocking");
        blocking(r, "F1", {0x01, 0x02, 0x03, 0x04, 0x05}, true);
        WAIT(200);
        return;
      case 22:
        *want_action = true;
        WAIT(500);
        return;
      case 23:
        sxsim::log_state("F2action");
        ESP_LOGI(TAG, "PHASE fsk_receive");
        sxsim::inject({0xA5, 0xA6, 0xA7});
        WAIT(300);
        return;
      case 24:
        ESP_LOGI(TAG, "RES F3 packets=%u", (unsigned) P.n);
        ESP_LOGI(TAG, "PHASE fsk_rx_before_send");
        sxsim::arm_rx_on_standby({0xB4, 0xB5, 0xB6});
        blocking(r, "F4", {0x09, 0x09, 0x09, 0x09}, true);
        WAIT(500);
        return;
      case 25:
        ESP_LOGI(TAG, "RES F4 after packets=%u", (unsigned) P.n);
        sxsim::log_state("F4after");
        blocking(r, "F4next", {0x0A});
        WAIT(200);
        return;
      case 26:
        ESP_LOGI(TAG, "PHASE fsk_raw_tx_mode");
        r->set_mode_tx();
        WAIT(400);
        return;
      case 27:
        ESP_LOGI(TAG, "RES F5 packets=%u", (unsigned) P.n);
        sxsim::log_state("F5");
        r->set_mode_rx();
        sxsim::log_state("F5rx");
        WAIT(200);
        return;
#ifdef SXSIM_PR
      case 28:
        ESP_LOGI(TAG, "PHASE fsk_async");
        M.tx_ms = 80;
        start_async(r, L, {0x91, 0x92});
        start_async(r, L2, {0x93});
        WAIT(400);
        return;
      case 29:
        ESP_LOGI(TAG, "RES FA1 L.n=%u last=%s L2.n=%u", (unsigned) L.n, ename(L.last), (unsigned) L2.n);
        sxsim::log_state("FA1");
        M.tx_never = true;
        start_async(r, L, {0x94}, 300);
        WAIT(800);
        return;
      case 30:
        M.tx_never = false;
        ESP_LOGI(TAG, "RES FA2 L.n=%u last=%s warn=%d", (unsigned) L.n, ename(L.last), r->status_has_warning());
        sxsim::log_state("FA2");
        M.tx_ms = 1000;
        start_async(r, L, {0x95});
        WAIT(50);
        return;
      case 31:
        ESP_LOGI(TAG, "RES FA3 cancel -> %d L.n=%u last=%s chip=%s", r->cancel_transmit_async(&L), (unsigned) L.n,
                 ename(L.last), sxsim::mode_name());
        WAIT(1300);
        return;
      case 32:
        ESP_LOGI(TAG, "RES FA3 later L.n=%u warn=%d", (unsigned) L.n, r->status_has_warning());
        sxsim::log_state("FA3");
        M.tx_ms = 50;
        sxsim::arm_rx_on_standby({0xE4, 0xE5});
        start_async(r, L, {0x96});
        WAIT(300);
        return;
      case 33:
        ESP_LOGI(TAG, "RES FA4 packets=%u L.n=%u", (unsigned) P.n, (unsigned) L.n);
        sxsim::log_state("FA4");
        WAIT(10);
        return;
#endif
      // ---------------- runtime mode change failure ----------------
      case 40:
        ESP_LOGI(TAG, "PHASE runtime_mode_fail");
        M.tx_ms = 50;
        prev_warn = prev_failed = 2;
        watch_until = millis() + 6000;
        M.fail_mode_writes = 3;
        r->set_mode_standby();
        ESP_LOGI(TAG, "RES R1 right_after warn=%d failed=%d", r->status_has_warning(), r->is_failed());
        ESP_LOGI(TAG, "RES R1 send_while_broken -> %s", ename(r->transmit_packet(std::vector<uint8_t>{0x01})));
        WAIT(6000);
        return;
      case 41:
        ESP_LOGI(TAG, "RES R1 end warn=%d failed=%d", r->status_has_warning(), r->is_failed());
        sxsim::log_state("R1");
        blocking(r, "R1send", {0x0B, 0x0C});
        sxsim::inject({0xF1});
        WAIT(300);
        return;
      case 42:
        ESP_LOGI(TAG, "RES R1rx packets=%u", (unsigned) P.n);
        reboot_to(1);
        return;
      default:
        if (s > 42)
          step = 43;
        return;
    }
  }
  if (boot_case == 1) {
    switch (s) {
      case 0:
        ESP_LOGI(TAG, "PHASE blocking_timeout");
        M.tx_never = true;
        blocking(r, "T1", {0x01, 0x02});
        M.tx_never = false;
        ESP_LOGI(TAG, "RES T1 chip=%s", sxsim::mode_name());
        WAIT(1500);
        return;
      case 1:
        ESP_LOGI(TAG, "RES T1 later warn=%d failed=%d chip=%s", r->status_has_warning(), r->is_failed(),
                 sxsim::mode_name());
        blocking(r, "T2", {0x03});
        WAIT(300);
        return;
      case 2:
        reboot_to(2);
        return;
      default:
        return;
    }
  }
  if (boot_case == 2 || boot_case == 3) {
    switch (s) {
      case 0:
        ESP_LOGI(TAG, "RES S%u setup failed=%d warn=%d", (unsigned) boot_case, r->is_failed(), r->status_has_warning());
        blocking(r, boot_case == 2 ? "S2" : "S3", {0x01});
        sxsim::log_state("S");
        WAIT(3500);
        return;
      case 1:
        ESP_LOGI(TAG, "RES S%u later failed=%d", (unsigned) boot_case, r->is_failed());
        sxsim::log_state("Slater");
        reboot_to(boot_case + 1);
        return;
      default:
        return;
    }
  }
  if (boot_case == 4 && s == 0) {
    ESP_LOGI(TAG, "RES S4 setup failed=%d", r->is_failed());
    ESP_LOGI(TAG, "ALLDONE");
  }
}
}  // namespace sxrun
