#pragma once
// canlb: MCP251xFD loopback checker. Stream frames carry a sequence number; other phases check against a queue.
#include <cstring>
#include <driver/pulse_cnt.h>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/components/mcp251xfd/mcp251xfd.h"
namespace canlb {
using esphome::canbus::CanFrame;
static constexpr uint8_t DLC_LEN[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
struct Stats {
  uint32_t sent, send_err, rx, lost, bad_id, bad_len, bad_flags, bad_data, lat_sum, lat_max, lat_n;
};
inline Stats st{};
inline esphome::canbus::Canbus *bus = nullptr;
inline esphome::mcp251xfd::MCP251XFD *chip = nullptr;
inline uint32_t seq = 0, expect = 0, t_sent = 0, inflight = 0;
inline bool classic = false;
enum Phase { STREAM, PAUSED, QUEUE };
inline Phase phase = STREAM;

inline void set_fd(CanFrame &f, bool fd, bool brs) {
#ifdef USE_CANBUS_FD
  f.fd = fd;
  f.bit_rate_switch = brs;
#endif
}
inline bool is_fd(const CanFrame &f) {
#ifdef USE_CANBUS_FD
  return f.fd;
#else
  return false;
#endif
}
inline bool is_brs(const CanFrame &f) {
#ifdef USE_CANBUS_FD
  return f.bit_rate_switch;
#else
  return false;
#endif
}
inline void fill(CanFrame &f, uint32_t s) {
  f = CanFrame{};
  f.use_extended_id = s & 1;
  f.can_id = f.use_extended_id ? (0x1ABC000 | (s & 0xFFF)) : (0x100 | (s & 0xFF));
  const bool fd = !classic && (s % 4) != 3;
  set_fd(f, fd, fd && (s & 2));
  uint8_t dlc = s % (fd ? 16 : 9);
  f.can_data_length_code = dlc;
  uint8_t len = fd ? DLC_LEN[dlc] : dlc;
  for (uint8_t i = 0; i < len; i++)
    f.data[i] = uint8_t(s * 31 + i * 7);
  if (len >= 4) {
    f.data[0] = s >> 24;
    f.data[1] = s >> 16;
    f.data[2] = s >> 8;
    f.data[3] = s;
  }
}
// returns 0 if equal, else 1 id, 2 len, 3 flags, 4 data
inline int compare(const CanFrame &got, const CanFrame &want) {
  if (got.can_id != want.can_id || got.use_extended_id != want.use_extended_id)
    return 1;
  if (got.can_data_length_code != want.can_data_length_code)
    return 2;
  if (is_fd(got) != is_fd(want) || is_brs(got) != is_brs(want) ||
      got.remote_transmission_request != want.remote_transmission_request)
    return 3;
  if (!want.remote_transmission_request && memcmp(got.data, want.data, want.payload_length()) != 0)
    return 4;
  return 0;
}

// expected queue for the non-stream phases
inline CanFrame q[64];
inline uint32_t q_rtr = 0, q_rtr_dirty = 0, q_send_err = 0;
inline uint32_t q_head = 0, q_tail = 0, q_rx = 0, q_bad = 0, q_stray = 0, q_tx_bad = 0, q_tx_ok = 0;
inline void q_reset() {
  q_head = q_tail = q_rx = q_bad = q_stray = q_tx_bad = q_tx_ok = 0;
  q_rtr = q_rtr_dirty = q_send_err = 0;
}

inline void on_frame(const CanFrame &got) {
  if (phase == QUEUE || phase == PAUSED) {
    if (q_head == q_tail) {
      q_stray++;
      return;
    }
    const CanFrame &w = q[q_head++ % 64];
    q_rx++;
    if (got.remote_transmission_request) {
      q_rtr++;
      bool zero = true;
      for (uint8_t k = 0; k < got.can_data_length_code && k < 8; k++)
        zero = zero && got.data[k] == 0;
      if (!zero) {
        q_rtr_dirty++;
        ESP_LOGW("canlb", "RTRDIRTY dlc=%u data=%02X %02X %02X %02X %02X %02X %02X %02X", got.can_data_length_code,
                 got.data[0], got.data[1], got.data[2], got.data[3], got.data[4], got.data[5], got.data[6],
                 got.data[7]);
      }
    }
    if (compare(got, w) != 0) {
      if (q_bad < 5)
        ESP_LOGW("canlb", "QBAD id=%x ext=%d dlc=%u/%u rtr=%d/%d fd=%d/%d code=%d", (unsigned) got.can_id,
                 got.use_extended_id, got.can_data_length_code, w.can_data_length_code, got.remote_transmission_request,
                 w.remote_transmission_request, is_fd(got), is_fd(w), compare(got, w));
      q_bad++;
    }
    return;
  }
  st.rx++;
  CanFrame want;
  fill(want, expect);
  switch (compare(got, want)) {
    case 1:
      st.bad_id++;
      break;
    case 2:
      st.bad_len++;
      break;
    case 3:
      st.bad_flags++;
      break;
    case 4:
      st.bad_data++;
      break;
    default:
      break;
  }
  uint32_t lat = esphome::micros() - t_sent;
  st.lat_sum += lat;
  st.lat_n++;
  if (lat > st.lat_max)
    st.lat_max = lat;
  expect++;
  if (inflight)
    inflight--;
}

// chip select counter on the CS GPIO, which stays an output
inline pcnt_unit_handle_t cs_unit = nullptr;
inline void cs_init(int gpio) {
  pcnt_unit_config_t uc = {};
  uc.low_limit = -30000;
  uc.high_limit = 30000;
  uc.flags.accum_count = true;
  pcnt_chan_config_t cc = {};
  cc.edge_gpio_num = gpio;
  cc.level_gpio_num = -1;
  pcnt_channel_handle_t ch = nullptr;
  esp_err_t e = pcnt_new_unit(&uc, &cs_unit);
  if (e == ESP_OK)
    e = pcnt_new_channel(cs_unit, &cc, &ch);
  if (e == ESP_OK)
    e = pcnt_channel_set_edge_action(ch, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
  if (e == ESP_OK)
    e = pcnt_unit_add_watch_point(cs_unit, 30000);
  if (e == ESP_OK)
    e = pcnt_unit_add_watch_point(cs_unit, -30000);
  if (e == ESP_OK)
    e = pcnt_unit_enable(cs_unit);
  if (e == ESP_OK)
    e = pcnt_unit_clear_count(cs_unit);
  if (e == ESP_OK)
    e = pcnt_unit_start(cs_unit);
  if (e != ESP_OK) {
    ESP_LOGE("canlb", "CS counter setup failed: %s", esp_err_to_name(e));
    cs_unit = nullptr;
  }
}
// False when there is no counter or the read failed
inline bool cs_count(int &c) {
  c = 0;
  if (cs_unit == nullptr)
    return false;
  const esp_err_t e = pcnt_unit_get_count(cs_unit, &c);
  if (e != ESP_OK)
    ESP_LOGW("canlb", "CS counter read failed: %s", esp_err_to_name(e));
  return e == ESP_OK;
}
inline int cs_mark_count = 0;
inline bool cs_mark_ok = false;
inline uint32_t cs_mark_time = 0;
inline void cs_mark() {
  cs_mark_ok = cs_count(cs_mark_count);
  cs_mark_time = esphome::millis();
}
inline void cs_report(const char *tag) {
  int c;
  if (!cs_mark_ok || !cs_count(c)) {
    ESP_LOGW("canlb", "CSRATE %s unavailable (no CS counter or a failed read)", tag);
    return;
  }
  const uint32_t ms = esphome::millis() - cs_mark_time;
  ESP_LOGI("canlb", "CSRATE %s transactions=%d in %u ms = %.2f/s", tag, c - cs_mark_count, (unsigned) ms,
           (c - cs_mark_count) * 1000.0f / ms);
}

inline uint32_t start_ms = 0;
inline void start(esphome::mcp251xfd::MCP251XFD *b) {
  bus = b;
  chip = b;
  start_ms = esphome::millis();
  b->add_frame_callback([](const CanFrame &f) { on_frame(f); });
}
inline void txcheck_step();
inline bool watching = false;
inline uint8_t watch_target = 0;
inline uint32_t watch_t0 = 0;
inline void watch_check();
inline void step() {
  if (bus == nullptr)
    return;
  if (watching)
    watch_check();
  if (phase == QUEUE) {
    txcheck_step();
    return;
  }
  // Hold the stream until the boot dump_config is out, or a frame stalled past the 50 ms timeout shifts the count
  if (phase != STREAM || esphome::millis() - start_ms < 5000)
    return;
  const uint32_t now = esphome::micros();
  if (inflight) {
    if (now - t_sent < 50000)
      return;
    st.lost += inflight;
    expect = seq;
    inflight = 0;
  }
  uint32_t n = (seq % 64 == 0) ? 8 : 1;
  t_sent = now;
  for (uint32_t i = 0; i < n; i++) {
    CanFrame f;
    fill(f, seq);
    if (bus->send_data(f) != esphome::canbus::ERROR_OK) {
      st.send_err++;
      break;
    }
    st.sent++;
    seq++;
    inflight++;
  }
}
inline void resume() {
  phase = STREAM;
  expect = seq;
  inflight = 0;
}
inline void report() {
  ESP_LOGI(
      "canlb",
      "STATS sent=%u send_err=%u rx=%u lost=%u bad_id=%u bad_len=%u bad_flags=%u bad_data=%u lat_avg=%uus lat_max=%uus",
      st.sent, st.send_err, st.rx, st.lost, st.bad_id, st.bad_len, st.bad_flags, st.bad_data,
      st.lat_n ? st.lat_sum / st.lat_n : 0, st.lat_max);
}

// raw register access, bypassing the driver
inline void rdbuf(uint16_t a, uint8_t *out, size_t n) {
  uint8_t b[2 + 80] = {uint8_t(0x30 | (a >> 8)), uint8_t(a)};
  memset(b + 2, 0, n);
  chip->enable();
  chip->transfer_array(b, 2 + n);
  chip->disable();
  memcpy(out, b + 2, n);
}
inline uint32_t rd32(uint16_t a) {
  uint8_t b[4];
  rdbuf(a, b, 4);
  return b[0] | (b[1] << 8) | (b[2] << 16) | (uint32_t(b[3]) << 24);
}
inline void wr8(uint16_t a, uint8_t v) {
  uint8_t b[3] = {uint8_t(0x20 | (a >> 8)), uint8_t(a), v};
  chip->enable();
  chip->write_array(b, 3);
  chip->disable();
}
// TREC TXBO: bus-off, also set while the controller sits in Configuration mode
inline bool busoff() { return (rd32(0x034) & 0x00200000) != 0; }
inline uint8_t opmod() { return (rd32(0x000) >> 21) & 7; }
inline bool wait_opmod(uint8_t m) {
  for (int i = 0; i < 100; i++) {
    if (opmod() == m)
      return true;
    esphome::delay(1);
  }
  return false;
}
inline void regs(const char *tag) {
  ESP_LOGI("canlb", "REGS %s CON=%08X OSC=%08X TXFIFOCON=%08X TXFIFOSTA=%08X RXFIFOCON=%08X RXFIFOSTA=%08X", tag,
           (unsigned) rd32(0x000), (unsigned) rd32(0xE00), (unsigned) rd32(0x05C), (unsigned) rd32(0x060),
           (unsigned) rd32(0x068), (unsigned) rd32(0x06C));
}
inline void force_mode(uint8_t m) {
  wr8(0x003, m);
  ESP_LOGI("canlb", "FORCE REQOP %u, CON now %08X", m, (unsigned) rd32(0x000));
}
// through Configuration, so the chip accepts it
inline void force_via_config(uint8_t m, const char *tag) {
  wr8(0x003, 4);
  const bool c = wait_opmod(4);
  wr8(0x003, m);
  const bool l = wait_opmod(m);
  ESP_LOGI("canlb", "FORCEVIA %s config=%d mode%u=%d CON=%08X", tag, c, m, l, (unsigned) rd32(0x000));
}

// send n frames without letting the loop drain RX
inline void burst(uint32_t n, const char *tag) {
  phase = QUEUE;
  q_reset();
  const uint32_t before = bus->get_status().rx_overrun_count;
  uint32_t sent = 0;
  for (uint32_t i = 0; i < n; i++) {
    CanFrame f = CanFrame{};
    f.use_extended_id = i & 1;
    f.can_id = f.use_extended_id ? (0x1F000000 | i) : (0x600 | i);
    set_fd(f, !classic, false);
    f.can_data_length_code = classic ? 8 : 15;
    for (uint8_t k = 0; k < f.payload_length(); k++)
      f.data[k] = uint8_t(i * 13 + k);
    uint32_t t0 = esphome::micros();
    esphome::canbus::Error e;
    while ((e = bus->send_data(f)) == esphome::canbus::ERROR_ALLTXBUSY && esphome::micros() - t0 < 20000) {
    }
    if (e != esphome::canbus::ERROR_OK) {
      q_send_err++;
      ESP_LOGW("canlb", "BURST %s send error %d at frame %u", tag, (int) e, (unsigned) i);
      break;
    }
    q[q_tail++ % 64] = f;
    sent++;
  }
  esphome::delay(20);
  ESP_LOGI("canlb", "BURST %s sent=%u overrun_before=%u", tag, sent, before);
}
inline void queue_report(const char *tag) {
  ESP_LOGI(
      "canlb",
      "QUEUE %s rx=%u bad=%u stray=%u missing=%u tx_obj_ok=%u tx_obj_bad=%u rtr=%u rtr_dirty=%u send_err=%u overrun=%u",
      tag, q_rx, q_bad, q_stray, q_tail - q_head, q_tx_ok, q_tx_bad, q_rtr, q_rtr_dirty, q_send_err,
      bus->get_status().rx_overrun_count);
}

// send one frame and check the TX object the driver wrote into chip RAM, including the padding
inline void tx_one(const CanFrame &f) {
  const uint32_t ua = rd32(0x064) & 0xFFF;
  if (bus->send_data(f) != esphome::canbus::ERROR_OK) {
    q_tx_bad++;
    ESP_LOGW("canlb", "TXSEND failed");
    return;
  }
  q[q_tail++ % 64] = f;
  esphome::delay(2);
  uint8_t obj[72];
  rdbuf(0x400 + ua, obj, 72);
  uint32_t id = f.use_extended_id ? ((f.can_id >> 18) | ((f.can_id & 0x3FFFF) << 11)) : f.can_id;
  uint32_t flags = f.can_data_length_code | (f.use_extended_id << 4) | (f.remote_transmission_request << 5) |
                   (is_brs(f) << 6) | (is_fd(f) << 7);
  const uint32_t got_id = obj[0] | (obj[1] << 8) | (obj[2] << 16) | (uint32_t(obj[3]) << 24);
  const uint32_t got_flags = obj[4] | (obj[5] << 8);
  const size_t len = f.remote_transmission_request ? 0 : f.payload_length();
  const size_t padded = (len + 3) & ~size_t{3};
  bool ok = got_id == id && (got_flags & 0xFF) == flags && memcmp(obj + 8, f.data, len) == 0;
  for (size_t k = len; k < padded; k++)
    ok = ok && obj[8 + k] == 0;
  if (ok)
    q_tx_ok++;
  else {
    q_tx_bad++;
    // A 64-byte payload fills the object, so there are no pad bytes to show
    auto pad = [&](size_t k) -> unsigned { return 8 + len + k < sizeof(obj) ? obj[8 + len + k] : 0; };
    ESP_LOGW("canlb", "TXOBJ bad id=%08X/%08X flags=%02X/%02X len=%u pad=%02X %02X %02X", (unsigned) got_id,
             (unsigned) id, (unsigned) (got_flags & 0xFF), (unsigned) flags, (unsigned) len, pad(0), pad(1), pad(2));
  }
}
inline void tx_dirty(bool ext) {
  CanFrame d = CanFrame{};
  d.use_extended_id = ext;
  d.can_id = ext ? 0x1EEEEEE : 0x5AA;
  set_fd(d, !classic, false);
  d.can_data_length_code = classic ? 8 : 15;
  memset(d.data, 0xAA, sizeof(d.data));
  tx_one(d);
}
// every DLC, classic and FD, RTR, both ID types; each case right after a frame full of 0xAA. Stepped from step().
inline int tc_ext = 0, tc_kind = 0, tc_dlc = 0;
inline bool tc_active = false;
inline uint32_t tc_cases = 0;
inline void txcheck_start() {
  phase = QUEUE;
  q_reset();
  tc_ext = tc_kind = tc_dlc = 0;
  tc_cases = 0;
  tc_active = true;
}
inline void txcheck_step() {
  if (!tc_active)
    return;
  if (classic && tc_kind >= 2) {
    tc_kind = 0;
    tc_ext++;
  }
  if (tc_ext >= 2) {
    tc_active = false;
    ESP_LOGI("canlb", "TXCHECK cases=%u", tc_cases);
    return;
  }
  tx_dirty(tc_ext);
  CanFrame f = CanFrame{};
  f.use_extended_id = tc_ext;
  f.can_id = tc_ext ? (0x1C000000 | (tc_kind << 8) | tc_dlc) : (0x700 | (tc_kind << 4) | tc_dlc);
  f.remote_transmission_request = tc_kind == 1;
  set_fd(f, tc_kind >= 2, tc_kind == 3);
  f.can_data_length_code = tc_dlc;
  if (!f.remote_transmission_request)
    for (uint8_t k = 0; k < f.payload_length(); k++)
      f.data[k] = uint8_t(0x30 + tc_dlc + k);
  tx_one(f);
  tc_cases++;
  if (++tc_dlc > (tc_kind >= 2 ? 15 : 8)) {
    tc_dlc = 0;
    if (++tc_kind >= 4) {
      tc_kind = 0;
      tc_ext++;
    }
  }
}
inline void watch_start(uint8_t target) {
  watch_target = target;
  watch_t0 = esphome::micros();
  watching = true;
}
inline void watch_check() {
  const uint32_t con = rd32(0x000);
  const uint8_t op = (con >> 21) & 7, req = (con >> 24) & 7;
  const uint32_t ms = (esphome::micros() - watch_t0) / 1000;
  if (op == watch_target && req == watch_target) {
    ESP_LOGI("canlb", "RESTORED to %u after %u ms", watch_target, (unsigned) ms);
    watching = false;
  } else if (ms > 12000) {
    ESP_LOGW("canlb", "NOT RESTORED after %u ms, CON=%08X", (unsigned) ms, (unsigned) con);
    watching = false;
  }
}
// data frame with distinct bytes, then a classic RTR, for DLC 4 to 8
inline void rtr_test() {
  phase = QUEUE;
  q_reset();
  for (uint8_t dlc = 4; dlc <= 8; dlc++) {
    CanFrame d = CanFrame{};
    d.can_id = 0x321;
    d.can_data_length_code = 8;
    for (uint8_t k = 0; k < 8; k++)
      d.data[k] = 0x11 * (k + 1);
    CanFrame r = CanFrame{};
    r.can_id = 0x322;
    r.remote_transmission_request = true;
    r.can_data_length_code = dlc;
    for (const CanFrame &f : {d, r}) {
      const auto e = bus->send_data(f);
      if (e == esphome::canbus::ERROR_OK) {
        q[q_tail++ % 64] = f;
      } else {
        q_send_err++;
        ESP_LOGW("canlb", "RTR test send failed: %d", (int) e);
      }
    }
  }
}
inline void chip_reset() {
  uint8_t b[2] = {0, 0};
  chip->enable();
  chip->write_array(b, 2);
  chip->disable();
  ESP_LOGI("canlb", "CHIPRESET sent, CON now %08X", (unsigned) rd32(0x000));
}
inline void timing(const char *tag) {
  ESP_LOGI("canlb", "TIMING %s NBTCFG=%08X DBTCFG=%08X TDC=%08X heap_free=%u", tag, (unsigned) rd32(0x004),
           (unsigned) rd32(0x008), (unsigned) rd32(0x00C), (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
}  // namespace canlb
