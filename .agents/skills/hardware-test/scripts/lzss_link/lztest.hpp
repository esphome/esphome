#pragma once
// Checked JSON / hex / binary streams over a packet_interface stack with LZSS compression (esphome#22471).
// Every message: crc32 of bytes [0, n-10) as 8 hex chars at [n-10, n-2), then `"}`.
// JSON types start {"t":"S","seq":N  (type at byte 6); H/B/X start with the type char then the seq digits.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "esphome/core/log.h"
#ifdef USE_ESP8266
#include <Esp.h>
#else
#include <esp_heap_caps.h>
#endif
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/packet_interface/packet_interface.h"
#ifdef USE_PACKET_INTERFACE_LZSS
#include "esphome/components/packet_interface/lzss_compression.h"
#include "esphome/components/lzss/lzss.h"
#endif

namespace lztest {
#ifdef USE_PACKET_INTERFACE_LZSS
using esphome::packet_interface::LzssCompression;
#endif
using esphome::packet_interface::PacketBuffer;
using esphome::packet_interface::PacketInterface;
using esphome::packet_interface::PacketMetaData;

static const char *const TAG = "lztest";

#ifdef USE_PACKET_INTERFACE_LZSS
// LzssCompression is final; an explicit instantiation may name its protected members.
template<typename Tag, typename Tag::type M> struct Rob {
  friend typename Tag::type get(Tag) { return M; }
};
struct LowerTag {
  using type = PacketInterface *LzssCompression::*;
  friend type get(LowerTag);
};
struct CheckTag {
  using type = const uint8_t LzssCompression::*;
  friend type get(CheckTag);
};
struct CodecTag {
  using type = const esphome::lzss::Lzss LzssCompression::*;
  friend type get(CodecTag);
};
template struct Rob<LowerTag, &LzssCompression::lower_>;
template struct Rob<CheckTag, &LzssCompression::check_>;
template struct Rob<CodecTag, &LzssCompression::codec_>;
#endif

static uint32_t rng_state = 1;
static uint32_t rnd() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

static const char TYPES[] = "SWDHBX";
static int type_index(char t) {
  const char *p = strchr(TYPES, t);
  return (p && t) ? int(p - TYPES) : -1;
}

struct TypeStats {
  // sender
  uint32_t tx{0}, ref{0};
  uint32_t enc_n{0}, enc_sum{0}, enc_max{0};
  // receiver, top
  uint32_t rx{0}, ok{0}, bad{0}, gap{0}, dup{0};
  int32_t last{-1};
  // receiver, wire (below compression)
  uint32_t w_raw_bad{0};
  uint32_t dec_n{0}, dec_sum{0}, dec_max{0};
  uint16_t seq{0};
};

static TypeStats st[6];
static PacketInterface *g_top = nullptr, *g_low = nullptr;
static int g_node = 0;
static bool g_oversize = false;
static uint8_t g_check = 0;
static uint32_t g_foreign = 0, g_other_wire = 0, g_wrong_check = 0, g_sess_up = 0, g_sess_down = 0;
// pairing between the top listener and the wire listener (same dispatch, top runs first)
static bool g_fresh = false;
static int g_fresh_type = -1;
static uint32_t g_fresh_len = 0;

#ifdef USE_PACKET_INTERFACE_LZSS
static const esphome::lzss::Lzss *g_codec = nullptr;
static uint16_t chains[esphome::lzss::Lzss::CHAIN_WORDS];
#endif
static uint8_t scratch[1200];
// on_top and on_wire run one after the other in a dispatch, so they share one receive buffer
static uint8_t rx_buf[1200];

static uint32_t free_heap() {
#ifdef USE_ESP8266
  return ESP.getFreeHeap();
#else
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#endif
}

static void hex8(char *o, uint32_t v) { snprintf(o, 9, "%08X", (unsigned) v); }

// Builds a message of type t into buf, returns its length.
static size_t build(char t, uint16_t seq, uint8_t *buf) {
  char *b = (char *) buf;
  int n = 0;
  float temp = 15.0f + (rnd() % 1500) / 100.0f;
  float hum = 30.0f + (rnd() % 600) / 10.0f;
  int bat = 20 + rnd() % 80;
  switch (t) {
    case 'S':
      n = sprintf(b, R"({"t":"S","seq":%u,"node":"%s","temperature":%.1f,"humidity":%.1f,"battery":%d,"state":"%s",)",
                  seq, g_node == 1 ? "garden" : "garage", temp, hum, bat, (rnd() & 1) ? "ON" : "OFF");
      break;
    case 'W': {
      static const char *const dirs[] = {"N", "NNE", "NE", "E", "SE", "S", "SW", "W", "NW", "WNW"};
      n = sprintf(b,
                  R"({"t":"W","seq":%u,"station":"%s","temperature":%.2f,"feels_like":%.2f,"humidity":%.1f,)"
                  R"("pressure":%.2f,"wind_speed":%.1f,"wind_gust":%.1f,"wind_direction":"%s","rain_rate":%.1f,)"
                  R"("rain_today":%.1f,"uv_index":%d,"solar_radiation":%.1f,"battery":%d,"rssi":%d,)",
                  seq, g_node == 1 ? "backyard" : "rooftop", temp, temp - 0.5f, hum, 990.0f + (rnd() % 4000) / 100.0f,
                  (rnd() % 150) / 10.0f, (rnd() % 250) / 10.0f, dirs[rnd() % 10], (rnd() % 50) / 10.0f,
                  (rnd() % 300) / 10.0f, int(rnd() % 11), (rnd() % 10000) / 10.0f, bat, -40 - int(rnd() % 50));
      break;
    }
    case 'D': {
      static const char *const rooms[] = {"Kitchen", "Living Room", "Bedroom", "Office", "Hallway", "Garage", "Porch"};
      n = sprintf(b, R"({"t":"D","seq":%u,"devices":[)", seq);
      for (int i = 0; i < 6; i++) {
        n += sprintf(b + n, R"(%s{"id":"sensor_%02d","name":"%s","state":"%s","value":%.1f})", i ? "," : "",
                     int(rnd() % 40), rooms[rnd() % 7], (rnd() & 1) ? "ON" : "OFF", (rnd() % 1000) / 10.0f);
      }
      n += sprintf(b + n, "],");
      break;
    }
    case 'H':
      n = sprintf(b, "H%u:", seq);
      for (int i = 0; i < 120; i++)
        b[n++] = "0123456789abcdef"[rnd() & 15];
      break;
    case 'B':
    case 'X': {
      n = sprintf(b, "%c%u:", t, seq);
      int len = t == 'B' ? 100 : 330;
      for (int i = 0; i < len; i++)
        buf[n++] = rnd() & 0xFF;
      break;
    }
  }
  if (t == 'S' || t == 'W' || t == 'D')
    n += sprintf(b + n, R"("crc":")");
  uint32_t c = esphome::crc32(0, buf, n);
  hex8(b + n, c);
  n += 8;
  b[n++] = '"';
  b[n++] = '}';
  return n;
}

// Needs n >= 12; sets type (-1 when unknown) and seq, returns whether the CRC and the tail match.
static bool parse(const uint8_t *buf, size_t n, int &type, uint32_t &seq) {
  const char *b = (const char *) buf;
  const char *p;
  if (b[0] == '{') {
    type = type_index(b[6]);
    p = b + 15;  // {"t":"S","seq":
  } else {
    type = type_index(b[0]);
    p = b + 1;
  }
  if (type < 0)
    return false;
  seq = strtoul(p, nullptr, 10);
  char want[9];
  hex8(want, esphome::crc32(0, buf, n - 10));
  return memcmp(want, b + n - 10, 8) == 0 && buf[n - 2] == '"' && buf[n - 1] == '}';
}

static void on_top(const PacketBuffer &d) {
  uint8_t *buf = rx_buf;
  size_t n = d.size();
  if (n > sizeof(rx_buf)) {
    g_foreign++;
    ESP_LOGW(TAG, "RXBAD oversize delivery %u", (unsigned) n);
    return;
  }
  int type = -1;
  uint32_t seq = 0;
  bool good = false;
  if (n >= 12) {
    d.copy_to(buf, n);
    good = parse(buf, n, type, seq);
  }
  if (type < 0) {
    g_foreign++;
    ESP_LOGW(TAG, "RXBAD foreign delivery of %u bytes", (unsigned) n);
    return;
  }
  auto &s = st[type];
  s.rx++;
  if (!good) {
    s.bad++;
    ESP_LOGW(TAG, "RXBAD %c seq=%u len=%u crc mismatch", TYPES[type], (unsigned) seq, (unsigned) n);
    return;
  }
  s.ok++;
  if (s.last >= 0) {
    if ((int32_t) seq == s.last) {
      s.dup++;
      return;
    }
    if ((int32_t) seq != s.last + 1) {
      s.gap++;
      ESP_LOGI(TAG, "GAP %c %d -> %u", TYPES[type], (int) s.last, (unsigned) seq);
    }
  }
  s.last = seq;
  g_fresh = true;
  g_fresh_type = type;
  g_fresh_len = n;
}

#ifdef USE_PACKET_INTERFACE_LZSS
static void on_wire(const PacketBuffer &d) {
  uint8_t *buf = rx_buf;
  size_t n = d.size();
  if (n > sizeof(rx_buf) || n == 0)
    return;
  d.copy_to(buf, n);
  if ((buf[0] & 0x7F) != g_check)
    g_wrong_check++;
  if (!g_fresh) {
    g_other_wire++;
    return;
  }
  g_fresh = false;
  auto &s = st[g_fresh_type];
  bool comp = buf[0] & 0x80;
  uint32_t in = g_fresh_len + 1;  // + session header
  if (comp) {
    uint32_t t0 = micros();
    size_t len = g_codec->expand(buf + 1, n - 1, scratch, sizeof(scratch));
    uint32_t dt = micros() - t0;
    s.dec_n++;
    s.dec_sum += dt;
    if (dt > s.dec_max)
      s.dec_max = dt;
    if (len != in)
      ESP_LOGW(TAG, "WIRE %c expand gave %d, expected %u", TYPES[g_fresh_type], (int) len, (unsigned) in);
  } else if (n != in + 1) {
    s.w_raw_bad++;
    ESP_LOGW(TAG, "WIRE %c raw packet %u bytes for %u input", TYPES[g_fresh_type], (unsigned) n, (unsigned) in);
  }
  ESP_LOGD(TAG, "RX %c seq=%d in=%u wire=%u %s", TYPES[g_fresh_type], (int) st[g_fresh_type].last, (unsigned) in,
           (unsigned) n, comp ? "comp" : "raw");
}

#endif

static void attach_top(PacketInterface *link, int node) {
  g_node = node;
  rng_state = 0x9E3779B9u * (node + 1) + micros();
  g_top = link->top();
  g_top->add_packet_interface_listener([](const PacketBuffer &d, const PacketMetaData &) { on_top(d); });
  g_top->add_on_session_state_callback([](bool up) {
    if (up)
      g_sess_up++;
    else
      g_sess_down++;
    ESP_LOGI(TAG, "SESSION %s", up ? "up" : "down");
  });
}

inline void begin_plain(PacketInterface *link, int node) {
  attach_top(link, node);
  ESP_LOGI(TAG, "BEGIN plain node=%d top max=%u", node, (unsigned) g_top->get_max_packet_size());
}

#ifdef USE_PACKET_INTERFACE_LZSS
inline void begin(PacketInterface *link, LzssCompression *cmp, int node, bool oversize) {
  attach_top(link, node);
  g_oversize = oversize;
  g_low = cmp->*get(LowerTag{});
  g_check = cmp->*get(CheckTag{});
  g_codec = &(cmp->*get(CodecTag{}));
  g_low->add_packet_interface_listener([](const PacketBuffer &d, const PacketMetaData &) { on_wire(d); });
  ESP_LOGI(TAG, "BEGIN node=%d dict=%u bytes check=0x%02X top max=%u low max=%u cmp max=%u", node,
           (unsigned) g_codec->dictionary_size(), g_check, (unsigned) g_top->get_max_packet_size(),
           (unsigned) g_low->get_max_packet_size(), (unsigned) cmp->get_max_packet_size());
}
#endif

// Called every second; sends one message every 1-4 s.
inline void tick() {
  static uint32_t next = 0;
  uint32_t now = millis();
  if (next == 0)
    next = now + 5000;
  if ((int32_t) (now - next) < 0)
    return;
  next = now + 1000 + rnd() % 3000;
  uint32_t r = rnd() % 100;
  char t = r < 35 ? 'S' : r < 60 ? 'W' : r < 75 ? 'D' : r < 90 ? 'H' : 'B';
  if (g_oversize && rnd() % 8 == 0)
    t = 'X';
  int ti = type_index(t);
  auto &s = st[ti];
  static uint8_t buf[1200];
  size_t n = build(t, s.seq, buf);
  // Encode timing with the layer's own codec, outside the stack
#ifdef USE_PACKET_INTERFACE_LZSS
  if (g_codec != nullptr) {
    uint32_t t0 = micros();
    g_codec->compress(buf, n, scratch, n - 1, chains);
    uint32_t dt = micros() - t0;
    s.enc_n++;
    s.enc_sum += dt;
    if (dt > s.enc_max)
      s.enc_max = dt;
  }
#endif
  bool ok = g_top->send_to_interface(PacketBuffer(buf, n), {});
  if (ok) {
    s.tx++;
    s.seq++;
  } else {
    s.ref++;
    ESP_LOGI(TAG, "REFUSED %c len=%u", t, (unsigned) n);
  }
}

inline void stat() {
  for (int i = 0; i < 6; i++) {
    auto &s = st[i];
    if (s.tx + s.ref + s.rx == 0)
      continue;
    ESP_LOGI(TAG,
             "STAT %c tx=%u ref=%u rx=%u ok=%u bad=%u gap=%u dup=%u | wire rawbad=%u | enc avg=%u max=%u us | "
             "dec avg=%u max=%u us",
             TYPES[i], s.tx, s.ref, s.rx, s.ok, s.bad, s.gap, s.dup, s.w_raw_bad, s.enc_n ? s.enc_sum / s.enc_n : 0,
             s.enc_max, s.dec_n ? s.dec_sum / s.dec_n : 0, s.dec_max);
  }
  ESP_LOGI(TAG, "STATG foreign=%u other_wire=%u wrong_check=%u sess_up=%u sess_down=%u heap=%u", g_foreign,
           g_other_wire, g_wrong_check, g_sess_up, g_sess_down, (unsigned) free_heap());
}

}  // namespace lztest
