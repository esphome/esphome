#pragma once
// Simulated serial device for serial_proxy tests, on a UART cross-wired to the proxy's UART.
// Commands arrive through the proxy framed as 7E <text> 7E:
//   R        queue one 88 byte reply      S   stream replies back to back
//   A        stream ASH DATA frames every 50 ms and time the tap's ACKs
//   X        stop, log SIMDONE totals, reset the counters
//   B<baud>  switch this UART to <baud> (the client then reconfigures the proxy)
// Reply n: A5, n (u32 BE), then 83 bytes (n + j) % 251.
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart_component.h"
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace spsim {
using namespace esphome;
static const char *const TAG = "spsim";
static constexpr size_t REPLY_SIZE = 88;
static constexpr uint32_t ASH_PERIOD_MS = 50;

inline uart::UARTComponent *bus = nullptr;
// the proxy's own UART: its RX backlog shows bytes held back while the client is blocked
inline uart::UARTComponent *proxy_bus = nullptr;
inline uint32_t backlog_max = 0;
enum Mode : uint8_t { IDLE, REPLY, STREAM, ASH };
inline Mode mode = IDLE;
inline uint32_t queued = 0, replies = 0, bytes = 0, requests = 0;
inline uint8_t out[512];
inline size_t out_len = 0, out_pos = 0;
// ASH timing
inline uint32_t ash_seq = 0, ash_sent = 0, acks = 0, ack_lat_sum = 0, ack_lat_max = 0, last_ash = 0;
inline uint32_t sent_at[8];
inline bool waiting[8];
inline uint32_t win_acks = 0, win_lat_sum = 0, win_lat_max = 0, win_unacked = 0;
// command parser
inline uint8_t cmd[32];
inline size_t cmd_len = 0;
inline uint32_t last_status = 0;

inline void make_reply(uint32_t n) {
  out[0] = 0xA5;
  out[1] = n >> 24;
  out[2] = n >> 16;
  out[3] = n >> 8;
  out[4] = n;
  for (size_t j = 0; j < REPLY_SIZE - 5; j++)
    out[5 + j] = (n + j) % 251;
  out_len = REPLY_SIZE;
  out_pos = 0;
}

inline size_t ash_put(uint8_t *p, uint8_t b) {
  switch (b) {
    case 0x7E:
    case 0x7D:
    case 0x11:
    case 0x13:
    case 0x18:
    case 0x1A:
      p[0] = 0x7D;
      p[1] = b ^ 0x20;
      return 2;
    default:
      p[0] = b;
      return 1;
  }
}

// DATA frame: control (frame number << 4), data = seq (u32 BE) + 20 pattern bytes, CRC-CCITT BE, flag
inline void make_ash(uint32_t seq) {
  uint8_t body[1 + 24 + 2];
  const uint8_t frm = seq & 7;
  body[0] = frm << 4;
  body[1] = seq >> 24;
  body[2] = seq >> 16;
  body[3] = seq >> 8;
  body[4] = seq;
  for (int j = 0; j < 20; j++)
    body[5 + j] = (seq * 3 + j) & 0xFF;
  uint16_t crc = crc16be(body, 25, 0xFFFF);
  body[25] = crc >> 8;
  body[26] = crc;
  size_t n = 0;
  for (uint8_t b : body)
    n += ash_put(out + n, b);
  out[n++] = 0x7E;
  out_len = n;
  out_pos = 0;
  sent_at[frm] = millis();
  if (waiting[frm])
    win_unacked++;
  waiting[frm] = true;
}

inline void log_done() {
  ESP_LOGI(TAG,
           "SIMDONE mode=%u requests=%u replies=%u bytes=%u ash_sent=%u acks=%u lat_avg=%u lat_max=%u heap=%u min=%u",
           mode, requests, replies, bytes, ash_sent, acks, acks ? ack_lat_sum / acks : 0, ack_lat_max,
           (unsigned) esp_get_free_heap_size(), (unsigned) esp_get_minimum_free_heap_size());
}

inline void reset() {
  mode = IDLE;
  queued = replies = bytes = requests = 0;
  out_len = out_pos = 0;
  ash_seq = ash_sent = acks = ack_lat_sum = ack_lat_max = 0;
  win_acks = win_lat_sum = win_lat_max = win_unacked = 0;
  for (auto &w : waiting)
    w = false;
}

inline void on_ack(uint8_t num) {
  // ACK num is the next frame expected, so it acknowledges num - 1
  const uint8_t frm = (num - 1) & 7;
  if (!waiting[frm])
    return;
  waiting[frm] = false;
  const uint32_t lat = millis() - sent_at[frm];
  acks++;
  ack_lat_sum += lat;
  ack_lat_max = std::max(ack_lat_max, lat);
  win_acks++;
  win_lat_sum += lat;
  win_lat_max = std::max(win_lat_max, lat);
}

inline void on_frame() {
  if (cmd_len == 3 && (cmd[0] & 0xF8) == 0x80) {
    on_ack(cmd[0] & 7);
    return;
  }
  if (cmd_len == 0)
    return;
  switch (cmd[0]) {
    case 'R':
      requests++;
      if (mode == IDLE)
        mode = REPLY;
      queued++;
      break;
    case 'S':
      mode = STREAM;
      break;
    case 'A':
      reset();
      mode = ASH;
      break;
    case 'X':
      log_done();
      reset();
      break;
    case 'B': {
      cmd[cmd_len] = 0;
      uint32_t baud = strtoul((const char *) cmd + 1, nullptr, 10);
      bus->flush();
      bus->set_baud_rate(baud);
      bus->load_settings(false);
      ESP_LOGI(TAG, "SIMBAUD %u", baud);
      reset();
      break;
    }
    default:
      ESP_LOGW(TAG, "unknown command %02X len %u", cmd[0], cmd_len);
  }
}

inline void poll() {
  uint8_t b;
  while (bus->available() && bus->read_byte(&b)) {
    if (b == 0x7E) {
      on_frame();
      cmd_len = 0;
    } else if (cmd_len < sizeof(cmd) - 1) {
      cmd[cmd_len++] = b;
    }
  }
  const uint32_t now = millis();
  for (;;) {
    if (out_pos < out_len) {
      size_t room = bus->available_for_write();
      if (room == 0)
        break;
      size_t n = std::min(room, out_len - out_pos);
      bus->write_array(out + out_pos, n);
      out_pos += n;
      bytes += n;
      if (out_pos < out_len)
        break;
    }
    if (mode == REPLY && queued > 0) {
      queued--;
      make_reply(replies++);
    } else if (mode == STREAM) {
      make_reply(replies++);
    } else if (mode == ASH && now - last_ash >= ASH_PERIOD_MS) {
      last_ash = now;
      make_ash(ash_seq++);
      ash_sent++;
    } else {
      break;
    }
  }
  if (proxy_bus != nullptr)
    backlog_max = std::max<uint32_t>(backlog_max, proxy_bus->available());
  if (now - last_status >= 1000) {
    last_status = now;
    if (mode == ASH) {
      ESP_LOGI(TAG, "ASHWIN sent=%u acks=%u win_acks=%u lat_avg=%u lat_max=%u unacked=%u backlog_max=%u", ash_sent,
               acks, win_acks, win_acks ? win_lat_sum / win_acks : 0, win_lat_max, win_unacked, backlog_max);
      backlog_max = 0;
      win_acks = win_lat_sum = win_lat_max = win_unacked = 0;
    } else if (mode != IDLE) {
      ESP_LOGI(TAG, "SIMSTAT requests=%u replies=%u bytes=%u queued=%u backlog_max=%u heap=%u", requests, replies,
               bytes, queued, backlog_max, (unsigned) esp_get_free_heap_size());
      backlog_max = 0;
    }
  }
}

}  // namespace spsim
