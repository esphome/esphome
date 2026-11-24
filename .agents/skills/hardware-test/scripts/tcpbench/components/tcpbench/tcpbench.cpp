#include "tcpbench.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <atomic>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "lwip/sockets.h"

// NOLINTBEGIN: bench-only socket tasks
namespace esphome::tcpbench {
static constexpr const char *const TAG = "tcpbench";
static std::atomic<uint32_t> g_rx_bytes{0}, g_tx_bytes{0};
static uint8_t s_buf[16384];
static uint8_t s_txbuf[16384];
static uint8_t s_udp_buf[2048];

static int bind_on(int port, int type) {
  int s = socket(AF_INET, type, 0);
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(s, (sockaddr *) &a, sizeof(a));
  if (type == SOCK_STREAM)
    listen(s, 1);
  return s;
}

// Port 5001: RX sink. On EOF replies 8 bytes total + 4 bytes ms (first byte to EOF).
static void rx_task(void *) {
  int ls = bind_on(5001, SOCK_STREAM);
  for (;;) {
    int c = accept(ls, nullptr, nullptr);
    if (c < 0) {
      vTaskDelay(100);
      continue;
    }
    uint64_t total = 0;
    uint32_t t0 = 0, t1 = 0;
    for (;;) {
      int n = recv(c, s_buf, sizeof(s_buf), 0);
      if (n <= 0)
        break;
      if (total == 0)
        t0 = millis();
      total += n;
      t1 = millis();
      g_rx_bytes += n;
    }
    uint8_t rep[12];
    memcpy(rep, &total, 8);
    uint32_t ms = t1 - t0;
    memcpy(rep + 8, &ms, 4);
    send(c, rep, sizeof(rep), 0);
    shutdown(c, SHUT_RDWR);
    close(c);
    ESP_LOGI(TAG, "RX done: %llu bytes in %u ms = %.1f KB/s", (unsigned long long) total, (unsigned) ms,
             ms ? total / 1.024 / ms : 0.0);
  }
}

// Port 5002: TX source. Reads 4 bytes = seconds, then sends that long and closes.
static void tx_task(void *) {
  int ls = bind_on(5002, SOCK_STREAM);
  memset(s_txbuf, 0x5a, sizeof(s_txbuf));
  for (;;) {
    int c = accept(ls, nullptr, nullptr);
    if (c < 0) {
      vTaskDelay(100);
      continue;
    }
    uint32_t secs = 0;
    recv(c, &secs, 4, MSG_WAITALL);
    if (secs > 120)
      secs = 120;
    uint64_t total = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < secs * 1000) {
      int n = send(c, s_txbuf, sizeof(s_txbuf), 0);
      if (n <= 0)
        break;
      total += n;
      g_tx_bytes += n;
    }
    uint32_t ms = millis() - t0;
    shutdown(c, SHUT_RDWR);
    close(c);
    ESP_LOGI(TAG, "TX done: %llu bytes in %u ms = %.1f KB/s", (unsigned long long) total, (unsigned) ms,
             ms ? total / 1.024 / ms : 0.0);
  }
}

// Port 5003 UDP: counts datagrams; a datagram starting with "Q" is answered with the count since the last query.
static void udp_task(void *) {
  int s = bind_on(5003, SOCK_DGRAM);
  int rcv = 65535;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));
  uint32_t count = 0;
  for (;;) {
    sockaddr_in from{};
    socklen_t fl = sizeof(from);
    int n = recvfrom(s, s_udp_buf, sizeof(s_udp_buf), 0, (sockaddr *) &from, &fl);
    if (n <= 0)
      continue;
    if (s_udp_buf[0] == 'Q') {
      sendto(s, &count, 4, 0, (sockaddr *) &from, fl);
      count = 0;
    } else {
      count++;
    }
  }
}

void TcpBench::setup() {
  ESP_LOGI(TAG, "sdkconfig: WIFI_RMT static_rx=%d dyn_rx=%d tx_type=%d rx_ba=%d tx_ba=%d",
           CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM, CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM, CONFIG_WIFI_RMT_TX_BUFFER_TYPE,
           CONFIG_WIFI_RMT_RX_BA_WIN, CONFIG_WIFI_RMT_TX_BA_WIN);
  ESP_LOGI(TAG, "sdkconfig: ESP_WIFI static_rx=%d dyn_rx=%d rx_ba=%d tx_ba=%d", CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM,
           CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM, CONFIG_ESP_WIFI_RX_BA_WIN, CONFIG_ESP_WIFI_TX_BA_WIN);
  xTaskCreate(rx_task, "tb_rx", 4096, nullptr, 5, nullptr);
  xTaskCreate(tx_task, "tb_tx", 4096, nullptr, 5, nullptr);
  xTaskCreate(udp_task, "tb_udp", 4096, nullptr, 5, nullptr);
  this->set_interval(1000, [this]() {
    uint32_t rx = g_rx_bytes, tx = g_tx_bytes;
    if (rx != this->prx_ || tx != this->ptx_) {
      ESP_LOGI(TAG, "rate rx=%u KB/s tx=%u KB/s", (unsigned) ((rx - this->prx_) / 1024),
               (unsigned) ((tx - this->ptx_) / 1024));
    }
    this->prx_ = rx;
    this->ptx_ = tx;
  });
  this->set_interval(10000, []() {
    ESP_LOGI(TAG, "heap free=%u internal=%u min_internal=%u psram=%u",
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  });
}
}  // namespace esphome::tcpbench
// NOLINTEND
