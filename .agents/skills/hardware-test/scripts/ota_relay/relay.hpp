#pragma once
// Serial <-> TCP relay for OTA tests: the host talks to RELAY_TARGET through this board, two channels.
// Frames both ways: A5 5A type len_lo len_hi payload, type = op | channel << 4.
// Host->dev ops: 0 PING, 1 OPEN(port u16), 2 DATA, 3 CLOSE, 5 DROP(ms u16, RELAY_AP only).
// Dev->host ops: 0 PONG(ready u8), 1 OPENED(err u8), 2 DATA, 3 CLOSED(reason u8), 4 ACK (one per DATA frame
// written to the socket), 5 DROP done.
// Build flags: -DRELAY_UART (link on UART0 at 921600, else USB Serial/JTAG), -DRELAY_TARGET='"a.b.c.d"'
// (default the ESPHome AP address), -DRELAY_AP (this board hosts the test AP: PONG reports a station on it, DROP
// renames the AP for ms so its stations lose the network).
// Host->dev DATA payload starts with seq u8 and sum8 u8; a lost or damaged frame closes the connection with reason 9
// instead of passing a hole on.
#ifdef RELAY_UART
#include <driver/uart.h>
static inline int relay_write(const void *d, size_t n) { return uart_write_bytes(UART_NUM_0, d, n); }
static inline int relay_read(void *d, size_t n, TickType_t t) { return uart_read_bytes(UART_NUM_0, d, n, t); }
#else
#include <driver/usb_serial_jtag.h>
static inline int relay_write(const void *d, size_t n) {
  return usb_serial_jtag_write_bytes(d, n, pdMS_TO_TICKS(2000));
}
static inline int relay_read(void *d, size_t n, TickType_t t) { return usb_serial_jtag_read_bytes(d, n, t); }
#endif
#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <cstring>
#include "esphome/components/wifi/wifi_component.h"
#ifndef RELAY_TARGET
#define RELAY_TARGET "192.168.4.1"
#endif

namespace relay {
static constexpr int NCH = 2;
static int socks[NCH] = {-1, -1};
static uint8_t seqs[NCH] = {0, 0};
// Largest frame payload; relay_host.py's MAX_FRAME must match
static constexpr uint16_t MAX_FRAME = 2100;
static void put(uint8_t type, const uint8_t *data, uint16_t len) {
  uint8_t h[5] = {0xA5, 0x5A, type, (uint8_t) (len & 0xFF), (uint8_t) (len >> 8)};
  relay_write(h, 5);
  if (len)
    relay_write(data, len);
}
static void close_quiet(int ch) {
  if (socks[ch] >= 0) {
    ::close(socks[ch]);
    socks[ch] = -1;
  }
}
static void close_sock(int ch, uint8_t reason) {
  close_quiet(ch);
  put(3 | (ch << 4), &reason, 1);
}
static void handle(uint8_t type, uint8_t *p, uint16_t len) {
  int ch = (type >> 4) & 1;
  uint8_t op = type & 0x0F;
  if (op == 0) {
#ifdef RELAY_AP
    wifi_sta_list_t sl{};
    uint8_t c = (esp_wifi_ap_get_sta_list(&sl) == ESP_OK && sl.num > 0) ? 1 : 0;
#else
    uint8_t c = esphome::wifi::global_wifi_component->is_connected() ? 1 : 0;
#endif
    put(0 | (ch << 4), &c, 1);
  } else if (op == 1 && len >= 2) {
    close_quiet(ch);
    seqs[ch] = 0;
    uint16_t port = p[0] | (p[1] << 8);
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = inet_addr(RELAY_TARGET);
    struct timeval tv = {10, 0};
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    uint8_t err = 0;
    if (::connect(s, (struct sockaddr *) &a, sizeof(a)) != 0) {
      err = (uint8_t) (errno ? errno : 255);
      ::close(s);
    } else {
      socks[ch] = s;
    }
    put(1 | (ch << 4), &err, 1);
  } else if (op == 2) {
    if (socks[ch] < 0)
      return;
    if (len < 2)
      return;
    uint8_t sum = 0;
    for (uint16_t k = 2; k < len; k++)
      sum += p[k];
    if (p[0] != seqs[ch] || p[1] != sum) {
      close_sock(ch, 9);
      return;
    }
    seqs[ch]++;
    p += 2;
    len -= 2;
    size_t off = 0;
    while (off < len) {
      int n = ::send(socks[ch], p + off, len - off, 0);
      if (n <= 0) {
        close_sock(ch, 1);
        return;
      }
      off += n;
    }
    put(4 | (ch << 4), nullptr, 0);
  } else if (op == 3) {
    close_sock(ch, 0);
#ifdef RELAY_AP
  } else if (op == 5 && len >= 2) {
    // Rename the AP: every station is kicked and cannot find the network, while this side's netif and sockets stay up
    uint16_t ms = p[0] | (p[1] << 8);
    wifi_config_t orig{}, tmp{};
    esp_wifi_get_config(WIFI_IF_AP, &orig);
    tmp = orig;
    static const char *const GONE = "relay-ap-gone";
    memset(tmp.ap.ssid, 0, sizeof(tmp.ap.ssid));
    memcpy(tmp.ap.ssid, GONE, strlen(GONE));
    tmp.ap.ssid_len = strlen(GONE);
    esp_wifi_deauth_sta(0);
    esp_wifi_set_config(WIFI_IF_AP, &tmp);
    vTaskDelay(pdMS_TO_TICKS(ms));
    esp_wifi_set_config(WIFI_IF_AP, &orig);
    put(5 | (ch << 4), nullptr, 0);
#endif
  }
}
static void task(void *) {
#ifdef RELAY_UART
  uart_config_t uc{};
  uc.baud_rate = 921600;
  uc.data_bits = UART_DATA_8_BITS;
  uc.parity = UART_PARITY_DISABLE;
  uc.stop_bits = UART_STOP_BITS_1;
  uc.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uc.source_clk = UART_SCLK_DEFAULT;
  uart_driver_install(UART_NUM_0, 8192, 8192, 0, nullptr, 0);
  uart_param_config(UART_NUM_0, &uc);
  uart_set_pin(UART_NUM_0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
#else
  usb_serial_jtag_driver_config_t cfg{};
  cfg.rx_buffer_size = 4096;
  cfg.tx_buffer_size = 4096;
  usb_serial_jtag_driver_install(&cfg);
#endif
  static uint8_t in[512];
  static uint8_t fr[MAX_FRAME];
  static uint8_t out[1460];
  int st = 0;
  uint8_t type = 0;
  uint16_t len = 0, got = 0;
  for (;;) {
    bool any = socks[0] >= 0 || socks[1] >= 0;
    int n = relay_read(in, sizeof(in), pdMS_TO_TICKS(any ? 2 : 20));
    for (int i = 0; i < n; i++) {
      uint8_t b = in[i];
      switch (st) {
        case 0:
          st = b == 0xA5 ? 1 : 0;
          break;
        case 1:
          st = b == 0x5A ? 2 : (b == 0xA5 ? 1 : 0);
          break;
        case 2:
          type = b;
          st = 3;
          break;
        case 3:
          len = b;
          st = 4;
          break;
        case 4:
          len |= b << 8;
          got = 0;
          if (len > sizeof(fr)) {
            st = 0;
          } else if (len == 0) {
            handle(type, fr, 0);
            st = 0;
          } else {
            st = 5;
          }
          break;
        case 5:
          fr[got++] = b;
          if (got == len) {
            handle(type, fr, len);
            st = 0;
          }
          break;
      }
    }
    for (int ch = 0; ch < NCH; ch++) {
      if (socks[ch] < 0)
        continue;
      int r = ::recv(socks[ch], out, sizeof(out), MSG_DONTWAIT);
      if (r > 0) {
        put(2 | (ch << 4), out, r);
      } else if (r == 0) {
        close_sock(ch, 0);
      } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        close_sock(ch, 2);
      }
    }
  }
}
static void start() { xTaskCreate(task, "relay", 6144, nullptr, 5, nullptr); }
}  // namespace relay
