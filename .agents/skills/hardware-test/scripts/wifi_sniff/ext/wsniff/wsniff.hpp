#pragma once
// Test only: see __init__.py. Logs "SNIF ch=<channel> dt=<ms> from_target=<frames> | <destination>=<count>
// fc=<frame control> rssi=<dBm> ..." every 5 s.
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <cstring>
#include <freertos/FreeRTOS.h>

namespace esphome::wsniff {
static constexpr const char *const TAG = "wsniff";

struct Dest {
  uint8_t mac[6];
  uint32_t count;
  uint16_t fc;
  int8_t rssi;
};

class WSniff : public Component {
 public:
  void set_target(uint64_t t) {
    for (int i = 0; i < 6; i++)
      this->target_[i] = (t >> (8 * (5 - i))) & 0xFF;
  }
  void set_channel(uint8_t c) { this->fixed_channel_ = c; }
  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override {
    inst_ = this;  // the promiscuous callback is a plain function
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));  // STA so the MAC hardware ACKs frames sent to it

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    wifi_promiscuous_filter_t f{};
    f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
    esp_err_t e_flt = esp_wifi_set_promiscuous_filter(&f);
    esp_err_t e_cb = esp_wifi_set_promiscuous_rx_cb(&WSniff::rx_cb_);
    esp_err_t err = esp_wifi_set_promiscuous(true);
    if (e_flt != ESP_OK || e_cb != ESP_OK || err != ESP_OK)
      ESP_LOGE(TAG, "sniffer start failed: filter %d callback %d enable %d", e_flt, e_cb, err);
    this->channel_ = this->fixed_channel_ ? this->fixed_channel_ : 1;
    esp_wifi_set_channel(this->channel_, WIFI_SECOND_CHAN_NONE);
    this->locked_ = this->fixed_channel_ != 0;
    this->last_ = this->hop_t_ = millis();
    uint8_t own[6];
    esp_wifi_get_mac(WIFI_IF_STA, own);
    ESP_LOGI(TAG, "sniffer up, own STA MAC %02x:%02x:%02x:%02x:%02x:%02x, target %02x:%02x:%02x:%02x:%02x:%02x", own[0],
             own[1], own[2], own[3], own[4], own[5], this->target_[0], this->target_[1], this->target_[2],
             this->target_[3], this->target_[4], this->target_[5]);
  }

  void loop() override {
    uint32_t now = App.get_loop_component_start_time();
    if (!this->locked_) {
      if (now - this->hop_t_ >= 400) {
        uint32_t seen = this->seen_;
        if (seen >= 3) {
          this->locked_ = true;
          ESP_LOGI(TAG, "LOCK channel %u (%u target frames in 400 ms)", this->channel_, (unsigned) seen);
        } else {
          this->channel_ = this->channel_ % 13 + 1;
          esp_wifi_set_channel(this->channel_, WIFI_SECOND_CHAN_NONE);
        }
        this->seen_ = 0;
        this->hop_t_ = now;
      }
      return;
    }
    if (now - this->last_ < 5000)
      return;
    uint32_t dt = now - this->last_;
    this->last_ = now;
    Dest snap[12];
    size_t n;
    uint32_t all;
    portENTER_CRITICAL(&mux_);
    n = this->n_;
    memcpy(snap, this->dests_, n * sizeof(Dest));
    this->n_ = 0;  // free the slots so new destinations get one
    all = this->seen_;
    this->seen_ = 0;
    portEXIT_CRITICAL(&mux_);
    char buf[700];
    int p =
        snprintf(buf, sizeof(buf), "SNIF ch=%u dt=%u from_target=%u", this->channel_, (unsigned) dt, (unsigned) all);
    for (size_t i = 0; i < n && p < (int) sizeof(buf) - 70; i++) {
      p += snprintf(buf + p, sizeof(buf) - p, " | %02x:%02x:%02x:%02x:%02x:%02x=%u fc=%04x rssi=%d", snap[i].mac[0],
                    snap[i].mac[1], snap[i].mac[2], snap[i].mac[3], snap[i].mac[4], snap[i].mac[5],
                    (unsigned) snap[i].count, snap[i].fc, snap[i].rssi);
    }
    ESP_LOGI(TAG, "%s", buf);
    if (all == 0) {
      if (++this->empty_ >= 4 && this->fixed_channel_ == 0) {
        ESP_LOGW(TAG, "no target frames for 20 s, rescanning");
        this->locked_ = false;
        this->empty_ = 0;
      }
    } else {
      this->empty_ = 0;
    }
  }

 protected:
  static void rx_cb_(void *buf, wifi_promiscuous_pkt_type_t type) {
    auto *pkt = static_cast<wifi_promiscuous_pkt_t *>(buf);
    WSniff *s = inst_;
    if (pkt->rx_ctrl.sig_len < 24)
      return;
    const uint8_t *h = pkt->payload;
    if (memcmp(h + 10, s->target_, 6) != 0)
      return;
    uint16_t fc = h[0] | (h[1] << 8);
    portENTER_CRITICAL_ISR(&mux_);
    s->seen_++;
    size_t i = 0;
    for (; i < s->n_; i++)
      if (memcmp(s->dests_[i].mac, h + 4, 6) == 0 && (s->dests_[i].fc & 0xFF) == h[0])
        break;
    if (i < s->n_) {
      s->dests_[i].count++;
    } else if (s->n_ < 12) {
      memcpy(s->dests_[i].mac, h + 4, 6);
      s->dests_[i].count = 1;
      s->n_++;
    }
    if (i < s->n_) {
      s->dests_[i].fc = fc;
      s->dests_[i].rssi = pkt->rx_ctrl.rssi;
    }
    portEXIT_CRITICAL_ISR(&mux_);
  }

  static inline WSniff *inst_{nullptr};
  static inline portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  uint8_t target_[6]{};
  uint8_t fixed_channel_{0};
  uint8_t channel_{1};
  bool locked_{false};
  uint8_t empty_{0};
  uint32_t last_{0}, hop_t_{0};
  volatile uint32_t seen_{0};
  Dest dests_[12]{};
  size_t n_{0};
};
}  // namespace esphome::wsniff
