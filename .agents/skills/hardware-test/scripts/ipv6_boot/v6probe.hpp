#pragma once
// Boot and reconnect timing probe for IPv6 link-local and SLAAC (#22364). Call tick(cap_ms) every 50 ms.
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>

namespace v6probe {
static const char *const PTAG = "v6probe";
struct Times {
  uint32_t assoc, v4, ll, gl, full;
};
static int phase = 0;  // 0 boot, 1 wifi disabled, 2 reconnecting, 3 done
static uint32_t start = 0;
static Times t{};

inline void tick(uint32_t cap_ms) {
  using esphome::millis;
  uint32_t now = millis();
  if (phase == 3)
    return;
  if (phase == 1) {
    if (now - start >= 3000) {
      ESP_LOGI(PTAG, "REENABLE");
      t = {};
      start = now;
      phase = 2;
      esphome::wifi::global_wifi_component->enable();
    }
    return;
  }
  uint32_t rel = (now - start) | 1;  // never 0, 0 means not seen
  wifi_ap_record_t ap;
  if (!t.assoc && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    t.assoc = rel;
    ESP_LOGI(PTAG, "MS assoc %u", rel);
  }
  // a missing netif still has to reach the cap_ms timeout below
  esp_netif_t *nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_ip_info_t ip;
  if (!t.v4 && nif != nullptr && esp_netif_get_ip_info(nif, &ip) == ESP_OK && ip.ip.addr != 0) {
    t.v4 = rel;
    ESP_LOGI(PTAG, "MS ipv4 %u", rel);
  }
  esp_ip6_addr_t a[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  int n = 0;
  if (nif != nullptr) {
    esphome::LwIPLock lock;
    n = esp_netif_get_all_ip6(nif, a);
  }
  bool ll = false, gl = false;
  for (int i = 0; i < n; i++) {
    if (esp_netif_ip6_get_addr_type(&a[i]) == ESP_IP6_ADDR_IS_LINK_LOCAL)
      ll = true;
    else
      gl = true;
  }
  if (!t.ll && ll) {
    t.ll = rel;
    ESP_LOGI(PTAG, "MS linklocal %u", rel);
  }
  if (!t.gl && gl) {
    t.gl = rel;
    ESP_LOGI(PTAG, "MS slaac %u", rel);
  }
  if (!t.full && esphome::wifi::global_wifi_component->is_connected()) {
    t.full = rel;
    ESP_LOGI(PTAG, "MS connected %u", rel);
  }
  bool done = t.gl && t.full;
  if (done || now - start >= cap_ms) {
    size_t used = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) - heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(PTAG, "RESULT %s assoc=%u v4=%u ll=%u gl=%u full=%u n6=%d timeout=%d heap_used=%u",
             phase == 0 ? "boot" : "reconnect", t.assoc, t.v4, t.ll, t.gl, t.full, n, done ? 0 : 1, (unsigned) used);
    if (phase == 0) {
      phase = 1;
      start = now;
      esphome::wifi::global_wifi_component->disable();
    } else {
      phase = 3;
      esphome::App.safe_reboot();
    }
  }
}
}  // namespace v6probe

#ifdef V6_INJECT
// Fault injection: esp-netif's STA_CONNECTED handler (which brings the netif up) is delayed by 300 ms in the
// event task, after the wifi component's own handler has already queued the event for the main loop.
extern "C" void __real_esp_netif_action_connected(void *esp_netif, esp_event_base_t base, int32_t id, void *data);
extern "C" void __wrap_esp_netif_action_connected(void *esp_netif, esp_event_base_t base, int32_t id, void *data) {
  ESP_LOGI("v6probe", "INJECT hold start");
  vTaskDelay(pdMS_TO_TICKS(300));
  ESP_LOGI("v6probe", "INJECT hold end");
  __real_esp_netif_action_connected(esp_netif, base, id, data);
}
#endif
