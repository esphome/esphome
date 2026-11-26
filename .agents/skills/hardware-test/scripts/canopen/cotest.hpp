#pragma once
#include <cstring>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/canopen/canopen.h"

// SDO client checks for the canopen hub (esphome#22410): every request carries its expected result, and the
// callback checks result, abort code, value or bytes and the completion order.

namespace cot {
using esphome::micros;
using esphome::millis;
using esphome::canopen::CANopenComponent;
using esphome::canopen::SDOReply;
using esphome::canopen::SDOResult;
static const char *const TAG = "cot";
static constexpr uint32_t ANY = 0xFFFFFFFF;
static constexpr uint8_t NODE = 0x20;

struct Req {
  const char *tag;
  uint32_t t0;
  SDOResult exp;
  uint32_t exp_abort;
  bool check_val;
  uint32_t exp_val;
  const uint8_t *exp_bytes;
  size_t exp_len;
  uint32_t seq;
};
static Req pool[64];
static uint8_t pool_i;
static uint32_t n_pass, n_fail, n_done, next_seq, last_seq, n_soak_sent;
static uint32_t last_done_us;
static bool order_ok = true;
// soak latency per request kind, queued to callback
struct Stat {
  uint32_t n, min_us = UINT32_MAX, max_us, sum_us;
  void add(uint32_t v) {
    n++;
    sum_us += v;
    if (v < min_us)
      min_us = v;
    if (v > max_us)
      max_us = v;
  }
  void log(const char *name) {
    if (n)
      ESP_LOGI(TAG, "LAT %s n=%u min=%u avg=%u max=%u us", name, (unsigned) n, (unsigned) min_us,
               (unsigned) (sum_us / n), (unsigned) max_us);
    *this = Stat{};
  }
};
static constexpr uint8_t NUM_RESULTS = 6;
static uint32_t res_count[NUM_RESULTS];

inline const char *res_s(SDOResult r) {
  static const char *const N[NUM_RESULTS] = {"SUCCESS",       "ABORT",           "TIMEOUT",
                                             "SIZE_MISMATCH", "TRANSPORT_ERROR", "REJECTED"};
  return (uint8_t) r < NUM_RESULTS ? N[(uint8_t) r] : "?";
}
inline void count(const SDOReply &rep) {
  n_done++;
  if ((uint8_t) rep.result < NUM_RESULTS)
    res_count[(uint8_t) rep.result]++;
}

inline Req *mk(const char *tag, SDOResult exp, uint32_t exp_abort = 0) {
  Req *r = &pool[pool_i++ % 64];
  *r = Req{tag, micros(), exp, exp_abort, false, 0, nullptr, 0, ++next_seq};
  return r;
}
inline Req *mkv(const char *tag, uint32_t val) {
  Req *r = mk(tag, SDOResult::SDO_RESULT_SUCCESS);
  r->check_val = true;
  r->exp_val = val;
  return r;
}
inline Req *mks(const char *tag, const void *bytes, size_t len) {
  Req *r = mk(tag, SDOResult::SDO_RESULT_SUCCESS);
  r->exp_bytes = (const uint8_t *) bytes;
  r->exp_len = len;
  return r;
}

inline void cb(void *ctx, const SDOReply &rep) {
  auto *r = static_cast<Req *>(ctx);
  uint32_t now = micros();
  uint32_t dt = now - r->t0;
  uint32_t start = (int32_t) (last_done_us - r->t0) > 0 ? last_done_us : r->t0;
  uint32_t svc = now - start;
  count(rep);
  if (rep.result != SDOResult::SDO_RESULT_REJECTED) {
    if (r->seq <= last_seq) {
      order_ok = false;
      ESP_LOGE(TAG, "ORDER seq %u after %u", (unsigned) r->seq, (unsigned) last_seq);
    }
    last_seq = r->seq;
    last_done_us = now;
  }
  bool ok = rep.result == r->exp && (r->exp_abort == ANY || rep.abort_code == r->exp_abort);
  if (ok && rep.ok() && r->check_val)
    ok = rep.value == r->exp_val;
  if (ok && rep.ok() && r->exp_bytes != nullptr)
    ok = rep.len == r->exp_len && memcmp(rep.data, r->exp_bytes, r->exp_len) == 0;
  ok ? n_pass++ : n_fail++;
  char txt[50] = {};
  if (rep.ok() && rep.data != nullptr) {
    size_t n = rep.len < 48 ? rep.len : 48;
    for (size_t i = 0; i < n; i++)
      txt[i] = (rep.data[i] >= 32 && rep.data[i] < 127) ? rep.data[i] : '.';
  }
  ESP_LOGI(TAG,
           "%s %-26s seq=%u res=%s abort=0x%08x val=0x%08x size=%u len=%u '%s' queued_to_cb=%u.%03u ms svc=%u.%03u ms",
           ok ? "PASS" : "FAIL", r->tag, (unsigned) r->seq, res_s(rep.result), (unsigned) rep.abort_code,
           (unsigned) rep.value, rep.size, (unsigned) rep.len, txt, (unsigned) (dt / 1000), (unsigned) (dt % 1000),
           (unsigned) (svc / 1000), (unsigned) (svc % 1000));
}

inline void summary(const char *when) {
  ESP_LOGI(TAG,
           "SUMMARY %s done=%u outstanding=%d pass=%u fail=%u order_ok=%d | ok %u abort %u timeout %u size %u "
           "transport %u rejected %u",
           when, (unsigned) n_done, (int) (next_seq + n_soak_sent - n_done), (unsigned) n_pass, (unsigned) n_fail,
           order_ok, (unsigned) res_count[0], (unsigned) res_count[1], (unsigned) res_count[2], (unsigned) res_count[3],
           (unsigned) res_count[4], (unsigned) res_count[5]);
}

// ---- phases ----
using R = SDOResult;
inline void phase_values(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE values");
  h->write_sdo<uint8_t>(NODE, 0x2000, 0, 0xA5, cb, mk("lambda write u8", R::SDO_RESULT_SUCCESS));
  h->write_sdo<uint16_t>(NODE, 0x2001, 0, 0xBEEF, cb, mk("lambda write u16", R::SDO_RESULT_SUCCESS));
  h->write_sdo<uint32_t>(NODE, 0x2002, 0, 0xDEADBEEF, cb, mk("lambda write u32", R::SDO_RESULT_SUCCESS));
  h->read_sdo<uint8_t>(NODE, 0x2000, 0, cb, mkv("lambda read u8", 0xA5));
  h->read_sdo<uint16_t>(NODE, 0x2001, 0, cb, mkv("lambda read u16", 0xBEEF));
  h->read_sdo<uint32_t>(NODE, 0x2002, 0, cb, mkv("lambda read u32", 0xDEADBEEF));
  h->read_sdo(NODE, 0x2002, 0, 0, cb, mkv("lambda read any width", 0xDEADBEEF));
  h->read_sdo<uint16_t>(NODE, 0x2000, 0, cb, mk("read u8 obj as u16", R::SDO_RESULT_SIZE_MISMATCH));
  h->write_sdo<uint16_t>(NODE, 0x2000, 0, 0x1234, cb, mk("write u16 to u8 obj", R::SDO_RESULT_ABORT, ANY));
  h->write_sdo<int16_t>(NODE, 0x2001, 0, -2, cb, mk("lambda write int16 -2", R::SDO_RESULT_SUCCESS));
  h->read_sdo<int16_t>(NODE, 0x2001, 0, cb, mkv("lambda read int16", 0xFFFE));
}

static uint8_t rbuf[80];
static uint8_t rbuf2[80];
static const char S_INIT[] = "hello canopen world";
static const char S_LONG[] = "segmented write over five segments!";  // 35 bytes
static const char S_7[] = "1234567";
static const char S_14[] = "ABCDEFGHIJKLMN";
static const char S_4[] = "wxyz";
static const char S_5[] = "short";
inline void phase_strings(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE strings");
  h->read_sdo_data(NODE, 0x2010, 0, rbuf, sizeof(rbuf), cb, mks("read initial string", S_INIT, strlen(S_INIT)));
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_LONG, strlen(S_LONG), cb,
                    mk("write 35 B string", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf2, sizeof(rbuf2), cb, mks("read back 35 B", S_LONG, strlen(S_LONG)));
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_7, 7, cb, mk("write 7 B (1 segment)", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf, sizeof(rbuf), cb, mks("read back 7 B", S_7, 7));
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_14, 14, cb,
                    mk("write 14 B (2 segments)", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf2, sizeof(rbuf2), cb, mks("read back 14 B", S_14, 14));
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_4, 4, cb, mk("write 4 B (expedited)", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf, sizeof(rbuf), cb, mks("read back 4 B", S_4, 4));
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_5, 5, cb, mk("write 5 B", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf2, sizeof(rbuf2), cb, mks("read back 5 B", S_5, 5));
}

static uint8_t LONG_EXP[64];
static uint8_t cap_buf[64];
inline void phase_limits(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE buffer limits");
  for (int i = 0; i < 64; i++)
    LONG_EXP[i] = i;
  h->read_sdo_data(NODE, 0x2020, 0, cap_buf, 64, cb, mks("read 64 B obj, cap 64", LONG_EXP, 64));
  h->read_sdo_data(NODE, 0x2020, 0, cap_buf, 63, cb, mk("read 64 B obj, cap 63", R::SDO_RESULT_ABORT, 0x05040005));
  h->read_sdo_data(NODE, 0x2020, 0, cap_buf, 16, cb, mk("read 64 B obj, cap 16", R::SDO_RESULT_ABORT, 0x05040005));
  h->read_sdo_data(NODE, 0x2020, 0, cap_buf, 4, cb, mk("read 64 B obj, cap 4", R::SDO_RESULT_ABORT, 0x05040005));
  h->read_sdo_data(NODE, 0x2010, 0, cap_buf, 3, cb, mk("read 5 B str, cap 3", R::SDO_RESULT_ABORT, 0x05040005));
  h->read_sdo(NODE, 0x2020, 0, 0, cb, mk("value read of 64 B obj", R::SDO_RESULT_SIZE_MISMATCH));
  h->read_sdo(NODE, 0x2010, 0, 0, cb, mk("value read of 5 B str", R::SDO_RESULT_SIZE_MISMATCH));
  h->read_sdo<uint32_t>(NODE, 0x2002, 0, cb, mk("u32 read after aborts", R::SDO_RESULT_SUCCESS));
}

inline void phase_bad(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE bad objects");
  h->read_sdo<uint32_t>(NODE, 0x2FFF, 0, cb, mk("read missing 0x2FFF", R::SDO_RESULT_ABORT, 0x06020000));
  h->read_sdo(NODE, 0x2000, 5, 0, cb, mk("read missing sub 0x2000:5", R::SDO_RESULT_ABORT, 0x06090011));
  h->write_sdo<uint8_t>(NODE, 0x2020, 0, 1, cb, mk("write read-only 0x2020", R::SDO_RESULT_ABORT, ANY));
  h->read_sdo_data(NODE, 0x2FFF, 0, cap_buf, 64, cb, mk("data read missing 0x2FFF", R::SDO_RESULT_ABORT, 0x06020000));
  h->read_sdo<uint32_t>(0x33, 0x1000, 0, cb, mk("read absent node 0x33", R::SDO_RESULT_TIMEOUT, 0x05040000));
  h->read_sdo<uint32_t>(NODE, 0x2002, 0, cb, mk("read after absent node", R::SDO_RESULT_SUCCESS));
  // argument checks: rejected synchronously
  bool a = h->read_sdo(NODE, 0x2000, 0, 3, cb, mk("read size 3 (invalid)", R::SDO_RESULT_REJECTED));
  bool b = h->write_sdo(NODE, 0x2000, 0, 0x1FF, 1, cb, mk("write 0x1FF size 1", R::SDO_RESULT_REJECTED));
  bool c = h->read_sdo<uint8_t>(0, 0x2000, 0, cb, mk("node id 0", R::SDO_RESULT_REJECTED));
  ESP_LOGI(TAG, "invalid args returned %d %d %d", a, b, c);
}

inline void phase_queue(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE queue: 17 reads in one call");
  int accepted = 0;
  for (int i = 0; i < 17; i++) {
    Req *r = i < 16 ? mk("queue read", R::SDO_RESULT_SUCCESS) : mk("queue read", R::SDO_RESULT_REJECTED);
    bool ok = (i % 2) ? h->read_sdo<uint32_t>(NODE, 0x2002, 0, cb, r) : h->read_sdo<uint8_t>(NODE, 0x2000, 0, cb, r);
    accepted += ok;
    if (!ok)
      ESP_LOGI(TAG, "request %d returned false", i + 1);
  }
  ESP_LOGI(TAG, "queue: %d of 17 accepted", accepted);
}

inline void phase_queue_mixed(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE queue: 16 mixed write/read");
  int accepted = 0;
  for (int i = 0; i < 8; i++) {
    accepted += h->write_sdo<uint16_t>(NODE, 0x2001, 0, 0x100 + i, cb, mk("mixed write", R::SDO_RESULT_SUCCESS));
    accepted += h->read_sdo<uint16_t>(NODE, 0x2001, 0, cb, mkv("mixed read", 0x100 + i));
  }
  ESP_LOGI(TAG, "mixed queue: %d of 16 accepted", accepted);
}

inline void nmt(esphome::canbus::Canbus *bus, uint8_t cmd) {
  uint8_t d[2] = {cmd, NODE};
  auto err = bus->send_data(0x000, false, false, d, 2);
  if (err != esphome::canbus::ERROR_OK) {
    n_fail++;
    ESP_LOGE(TAG, "FAIL NMT 0x%02x to node 0x%02x not sent, error %d", cmd, NODE, (int) err);
    return;
  }
  ESP_LOGI(TAG, "NMT 0x%02x to node 0x%02x sent", cmd, NODE);
}

inline void log_stat(esphome::canbus::Canbus *bus) {
  auto s = bus->get_status();
  ESP_LOGI(TAG, "STAT up %u s | bus_off %d tec %u rec %u bus_err %u arb_lost %u rx_missed %u",
           (unsigned) (millis() / 1000), s.bus_off, s.tx_error_counter, s.rx_error_counter,
           (unsigned) s.bus_error_count, (unsigned) s.arb_lost_count, (unsigned) s.rx_missed_count);
}

// soak: chained requests, one kind after the other
static Stat st_read, st_write, st_str;
static uint32_t soak_until, soak_k;
static Req soak_req;
inline void soak_next(CANopenComponent *h);
inline void soak_cb(void *ctx, const SDOReply &rep) {
  auto *h = static_cast<CANopenComponent *>(ctx);
  uint32_t now = micros();
  uint32_t dt = now - soak_req.t0;
  count(rep);
  bool ok = rep.ok();
  uint32_t k = soak_k % 3;
  if (ok && k == 0)
    ok = rep.value == 0xC0DE0000 + (soak_k / 3 & 0xFFFF);
  if (ok && k == 2)
    ok = rep.len == 14 && memcmp(rep.data, S_14, 14) == 0;
  ok ? n_pass++ : n_fail++;
  if (!ok)
    ESP_LOGW(TAG, "FAIL soak k=%u res=%s abort=0x%08x val=0x%08x", (unsigned) soak_k, res_s(rep.result),
             (unsigned) rep.abort_code, (unsigned) rep.value);
  (k == 0 ? st_read : k == 1 ? st_write : st_str).add(dt);
  soak_k++;
  if (rep.result == SDOResult::SDO_RESULT_REJECTED)  // synchronous: chaining again would recurse
    ESP_LOGE(TAG, "FAIL soak stopped, request %u rejected", (unsigned) soak_k);
  else if ((int32_t) (millis() - soak_until) < 0)
    soak_next(h);
  else
    ESP_LOGI(TAG, "SOAK done, %u requests", (unsigned) soak_k);
}
inline void soak_next(CANopenComponent *h) {
  soak_req.t0 = micros();
  n_soak_sent++;
  uint32_t k = soak_k % 3;
  if (k == 1) {  // write the next value
    h->write_sdo<uint32_t>(NODE, 0x2002, 0, 0xC0DE0000 + ((soak_k / 3 + 1) & 0xFFFF), soak_cb, h);
  } else if (k == 0) {
    h->read_sdo<uint32_t>(NODE, 0x2002, 0, soak_cb, h);
  } else {
    h->read_sdo_data(NODE, 0x2010, 0, rbuf, sizeof(rbuf), soak_cb, h);
  }
}
inline void soak_start(CANopenComponent *h, uint32_t ms) {
  ESP_LOGI(TAG, "PHASE soak %u ms", (unsigned) ms);
  soak_until = millis() + ms;
  soak_k = 0;
  // the soak reads expect 0xC0DE0000 first and the 14 B string
  h->write_sdo<uint32_t>(NODE, 0x2002, 0, 0xC0DE0000, cb, mk("soak preset u32", R::SDO_RESULT_SUCCESS));
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_14, 14, cb, mk("soak preset str", R::SDO_RESULT_SUCCESS));
  soak_next(h);
}
inline void soak_report() {
  st_read.log("soak u32 read (expedited)");
  st_write.log("soak u32 write (expedited)");
  st_str.log("soak 14 B read (segmented, 3 round trips)");
}

// offline: a request every 250 ms, results counted per second
static uint32_t off_win[NUM_RESULTS];
inline void off_cb(void *ctx, const SDOReply &rep) {
  auto *r = static_cast<Req *>(ctx);
  uint32_t dt = micros() - r->t0;
  count(rep);
  if ((uint8_t) rep.result < NUM_RESULTS)
    off_win[(uint8_t) rep.result]++;
  ESP_LOGI(TAG, "OFF %s seq=%u res=%s abort=0x%08x val=0x%08x queued_to_cb=%u ms", r->tag, (unsigned) r->seq,
           res_s(rep.result), (unsigned) rep.abort_code, (unsigned) rep.value, (unsigned) (dt / 1000));
}
inline void off_report() {
  for (uint8_t i = 0; i < NUM_RESULTS; i++)
    ESP_LOGI(TAG, "OFFLINE window %s %u", res_s((SDOResult) i), (unsigned) off_win[i]);
}
inline void off_tick(CANopenComponent *h) {
  static uint32_t i;
  i++;
  Req *r = mk((i % 2) ? "off read u32" : "off write u16", R::SDO_RESULT_SUCCESS);
  bool ok = (i % 2) ? h->read_sdo<uint32_t>(NODE, 0x2002, 0, off_cb, r)
                    : h->write_sdo<uint16_t>(NODE, 0x2001, 0, i & 0xFFFF, off_cb, r);
  if (!ok)
    ESP_LOGW(TAG, "OFF request %u not queued", (unsigned) i);
}

inline void phase_strings_after(CANopenComponent *h) {
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_LONG, strlen(S_LONG), cb,
                    mk("recovery write 35 B", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf2, sizeof(rbuf2), cb, mks("recovery read 35 B", S_LONG, strlen(S_LONG)));
}

inline void phase_wrap(CANopenComponent *h) {
  ESP_LOGI(TAG, "PHASE wrap (lengths over 65535)");
  h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_LONG, strlen(S_LONG), cb,
                    mk("wrap: write 35 B", R::SDO_RESULT_SUCCESS));
  h->read_sdo_data(NODE, 0x2010, 0, rbuf, 65536 + 4, cb, mks("wrap: read 35 B, cap 65540", S_LONG, strlen(S_LONG)));
  bool w = h->write_sdo_data(NODE, 0x2010, 0, (const uint8_t *) S_LONG, 65536 + 5, cb,
                             mk("wrap: write len 65541", R::SDO_RESULT_REJECTED));
  ESP_LOGI(TAG, "wrap: write len 65541 returned %d", w);
  h->read_sdo_data(NODE, 0x2010, 0, rbuf2, sizeof(rbuf2), cb, mks("wrap: string unchanged", S_LONG, strlen(S_LONG)));
  h->read_sdo_data(NODE, 0x2020, 0, cap_buf, 65536 + 64, cb, mk("wrap: 64 B obj, cap 65600", R::SDO_RESULT_SUCCESS));
}

// node B answers 0x2003 writes and 0x2004 reads after 450 ms, past the 300 ms response timeout
static const char *const LW[4] = {"late write ack: write", "late write ack: read", "late read rsp: read",
                                  "late read rsp: write"};
static uint32_t late_n;
inline void phase_late(CANopenComponent *h) {
  uint32_t v = 0xA0000000 + (++late_n);
  ESP_LOGI(TAG, "PHASE late responses, round %u", (unsigned) late_n);
  h->write_sdo<uint32_t>(NODE, 0x2003, 0, v, cb, mk(LW[0], R::SDO_RESULT_TIMEOUT, 0x05040000));
  h->read_sdo<uint32_t>(NODE, 0x2003, 0, cb, mkv(LW[1], v));
}
inline void phase_late_read(CANopenComponent *h) {
  uint32_t v = 0xB0000000 + late_n;
  h->read_sdo<uint32_t>(NODE, 0x2004, 0, cb, mk(LW[2], R::SDO_RESULT_TIMEOUT, 0x05040000));
  h->write_sdo<uint32_t>(NODE, 0x2004, 0, v, cb, mk(LW[3], R::SDO_RESULT_SUCCESS));
}
}  // namespace cot
