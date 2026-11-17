#pragma once
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <atomic>

// Ring of management frames seen by the AP's sniffer: assoc req (0x00), disassoc (0xA0), auth (0xB0), deauth (0xC0).
struct SniffRec {
  int64_t t_us;
  uint8_t fc0, fc1;
  uint16_t seq;
  uint8_t a1[6], a2[6];
  int8_t rssi;
  uint16_t len;
};
static SniffRec s_ring[256];
static std::atomic<uint32_t> s_head{0};
// single producer (sniff_cb on the Wi-Fi task), single consumer (sniff_drain on the main loop)
static std::atomic<uint32_t> s_tail{0};
static std::atomic<uint32_t> s_drops{0};

static void sniff_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;
  auto *p = (wifi_promiscuous_pkt_t *) buf;
  const uint8_t *f = p->payload;
  uint8_t st = f[0] & 0xF0;
  if (st != 0x00 && st != 0xA0 && st != 0xB0 && st != 0xC0)
    return;
  uint32_t h = s_head.load(std::memory_order_relaxed);
  if (h - s_tail.load(std::memory_order_acquire) >= 256) {
    s_drops++;
    return;
  }
  SniffRec &r = s_ring[h & 255];
  r.t_us = esp_timer_get_time();
  r.fc0 = f[0];
  r.fc1 = f[1];
  memcpy(r.a1, f + 4, 6);
  memcpy(r.a2, f + 10, 6);
  r.seq = (f[22] | (f[23] << 8)) >> 4;
  r.rssi = p->rx_ctrl.rssi;
  r.len = p->rx_ctrl.sig_len;
  s_head.store(h + 1, std::memory_order_release);
}

static void sniff_start() {
  wifi_promiscuous_filter_t flt{};
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous_rx_cb(sniff_cb);
  esp_err_t err = esp_wifi_set_promiscuous(true);
  ESP_LOGI("wdap", "SNIFFER start: %d", err);
}

static void sniff_drain() {
  const uint32_t h = s_head.load(std::memory_order_acquire);
  for (uint32_t t = s_tail.load(std::memory_order_relaxed); t != h; t++) {
    const SniffRec &r = s_ring[t & 255];
    const char *kind = "?";
    switch (r.fc0 & 0xF0) {
      case 0x00:
        kind = "ASSOCREQ";
        break;
      case 0xA0:
        kind = "DISASSOC";
        break;
      case 0xB0:
        kind = "AUTH";
        break;
      case 0xC0:
        kind = "DEAUTH";
        break;
    }
    ESP_LOGI("wdap",
             "MGMT %s ta %02X:%02X:%02X:%02X:%02X:%02X ra %02X:%02X:%02X:%02X:%02X:%02X seq %u retry %u prot %u len %u "
             "rssi %d t_us %lld",
             kind, r.a2[0], r.a2[1], r.a2[2], r.a2[3], r.a2[4], r.a2[5], r.a1[0], r.a1[1], r.a1[2], r.a1[3], r.a1[4],
             r.a1[5], r.seq, (r.fc1 >> 3) & 1, (r.fc1 >> 6) & 1, r.len, r.rssi, (long long) r.t_us);
    s_tail.store(t + 1, std::memory_order_release);
  }
  uint32_t d = s_drops.exchange(0);
  if (d)
    ESP_LOGW("wdap", "SNIFF DROPS %u", d);
}
