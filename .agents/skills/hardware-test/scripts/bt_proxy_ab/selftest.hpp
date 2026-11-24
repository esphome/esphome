// On-device version of the drive.py proxy scenarios, for boards whose Wi-Fi cannot carry the API test (classic ESP32).
// Drives three ble_client instances through the proxy connection types (V3 without/with cache) and logs ST lines.
#pragma once
#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include <cstring>
#include <vector>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

namespace selftest {
namespace espbt = esphome::esp32_ble_tracker;
using esphome::ble_client::BLEClient;
using esphome::millis;
using espbt::ClientState;
static const char *const TAG = "selftest";

enum Peer : uint8_t { PL = 0, PM = 1, PE = 2 };
static const char *const PNAME[3] = {"L", "M", "eve"};
static uint64_t ADDR[3];  // from the ble_client mac_address entries (selftest-e32.yaml substitutions)
static uint16_t FF01[3];  // ff01 handles of L and M (handle_l, handle_m substitutions); Eve has none

static BLEClient *C[3];
static espbt::ESP32BLETracker *trk;
static esp_ble_addr_type_t atype[3] = {BLE_ADDR_TYPE_PUBLIC, BLE_ADDR_TYPE_PUBLIC, BLE_ADDR_TYPE_RANDOM};
static bool seen[3];
static uint32_t adv_n;
// notify soak
static uint32_t tk_last, tk_rx, tk_gaps, tk_missing, tk_missing_drop, tk_back, tk_dups, tk_short;
static uint32_t tk_down_since, tk_unheld_ms;  // time without the link, as drive.py's unheld_s
static bool tk_have, tk_count, tk_resumed;

inline void on_adv(const espbt::ESPBTDevice &d) {
  adv_n++;
  uint64_t a = d.address_uint64();
  for (int i = 0; i < 3; i++)
    if (a == ADDR[i]) {
      seen[i] = true;
      atype[i] = d.get_address_type();
    }
}
inline void on_tick(uint32_t v) {
  if (!tk_count)
    return;
  tk_rx++;
  if (tk_have && v < tk_last)
    tk_back++;  // the server restarted or the order broke
  if (tk_have && v == tk_last && !tk_resumed)
    tk_dups++;
  if (tk_have && v > tk_last + 1) {
    if (tk_resumed) {
      tk_missing_drop += v - tk_last - 1;  // sent while the link was down
    } else {
      tk_gaps++;
      tk_missing += v - tk_last - 1;
    }
  }
  tk_last = v;
  tk_have = true;
  tk_resumed = false;
}
// a tick payload under 4 bytes: garbled, not a gap
inline void on_short_tick() {
  if (tk_count)
    tk_short++;
}

// read/write round trips by handle
struct Rtt : public esphome::ble_client::BLEClientNode {
  uint32_t t0{0};
  int pending{0};  // 1 read, 2 write
  uint32_t done_ms{0};
  bool ok{false};
  bool len_bad{false};  // a read of ff01 (one byte) returned another length: the handle points elsewhere
  uint8_t bad{0};       // ops that completed with a GATT error status
  void gattc_event_handler(esp_gattc_cb_event_t e, esp_gatt_if_t, esp_ble_gattc_cb_param_t *p) override {
    // uses fixed handles only, so it must not keep BLEClient from releasing the service table
    if (e == ESP_GATTC_SEARCH_CMPL_EVT)
      this->node_state = ClientState::ESTABLISHED;
    if (e == ESP_GATTC_READ_CHAR_EVT && this->pending == 1) {
      this->done_ms = millis() - this->t0;
      this->ok = p->read.status == ESP_GATT_OK;
      this->bad += !this->ok;
      if (this->ok && p->read.value_len != 1)
        this->len_bad = true;
      this->pending = 0;
    } else if (e == ESP_GATTC_WRITE_CHAR_EVT && this->pending == 2) {
      this->done_ms = millis() - this->t0;
      this->ok = p->write.status == ESP_GATT_OK;
      this->bad += !this->ok;
      this->pending = 0;
    } else if (e == ESP_GATTC_DISCONNECT_EVT && this->pending) {
      this->pending = 0;
      this->ok = false;
    }
  }
};
static Rtt *RT[3];

enum Kind : uint8_t { K_ADV, K_CONN, K_RETRY, K_THIRD_EVE, K_THIRD_PD, K_THIRD_L, K_NOTIFY, K_CHURN, K_EVE, K_DONE };
struct Phase {
  Kind k;
  uint8_t peer;
  uint16_t n;  // rounds, cycles or seconds
  bool cold;
  const char *name;
};
static const Phase PHASES[] = {
    {K_ADV, 0, 120, false, "adv"},      {K_CONN, PL, 15, true, "discL"},         {K_CONN, PL, 10, false, "cacheL"},
    {K_CONN, PM, 15, true, "discM"},    {K_CONN, PM, 10, false, "cacheM"},       {K_RETRY, PL, 15, true, "retryL"},
    {K_RETRY, PM, 15, true, "retryM"},  {K_THIRD_EVE, PE, 10, true, "thirdeve"}, {K_THIRD_PD, PM, 8, true, "thirdpd"},
    {K_THIRD_L, PL, 8, true, "thirdL"}, {K_NOTIFY, PL, 600, false, "notify"},    {K_CHURN, 0, 50, false, "churn"},
    {K_EVE, PE, 10, true, "eve"},       {K_DONE, 0, 0, false, "done"},
};

// per-phase state
static uint8_t ph;
static int st;        // step inside the phase
static uint16_t rnd;  // round or cycle
static uint8_t att;   // retry attempt
static uint32_t t_ph, t0, t_wait, t_open, t_round;
static uint16_t ok_n, first_ok_n, fail_n, stuck_n, adv_mode_n, cc_fail_n, hb_n, hd_n, hf_n, se_n, pt_n, nc_n;
static bool bad_handle[2];  // handle_l/handle_m do not point at this server's ff01: RTTs hit another characteristic
static uint32_t adv0;
static uint8_t rtt_i;
static uint32_t rd_sum, wr_sum;
static uint8_t rd_n, wr_n, rtt_err, rtt_to;  // send errors and timeouts of the round's ops

inline void connect(uint8_t p, bool cold) {
  BLEClient *c = C[p];
  if (cold) {
    esp_bd_addr_t bda;
    for (int i = 0; i < 6; i++)
      bda[i] = (ADDR[p] >> (40 - 8 * i)) & 0xFF;
    esp_err_t err = esp_ble_gattc_cache_clean(bda);
    if (err != ESP_OK) {
      cc_fail_n++;  // in the phase_end line: these cold attempts may have used the cache
      ESP_LOGW(TAG, "ST cache_clean %s r=%u err=%d (this attempt may use the cache)", PNAME[p], rnd, err);
    }
  }
  c->set_connection_type(cold ? espbt::ConnectionType::V3_WITHOUT_CACHE : espbt::ConnectionType::V3_WITH_CACHE);
  c->set_remote_addr_type(atype[p]);
  c->set_auto_connect(false);
  // the proxy path: a DISCOVERED client makes the tracker stop the scan and connect it
  c->set_state(ClientState::DISCOVERED);
  t0 = millis();
  t_open = 0;
  ESP_LOGI(TAG, "ST conn_start %s %s r=%u", PNAME[p], cold ? "cold" : "cached", rnd);
}
inline ClientState s(uint8_t p) { return C[p]->state(); }
inline bool up(uint8_t p) { return s(p) == ClientState::ESTABLISHED; }
inline bool idle(uint8_t p) { return s(p) == ClientState::IDLE; }
inline void drop(uint8_t p) {
  if (!idle(p))
    C[p]->disconnect();
}

inline void next_phase() {
  const Phase &P = PHASES[ph];
  ESP_LOGI(TAG,
           "ST phase_end %s rounds=%u ok=%u first_ok=%u fail=%u stuck=%u cache_clean_fail=%u handle_bad=%u "
           "held_dropped=%u hold_failed=%u send_err=%u partial=%u no_readable_char=%u",
           P.name, rnd, ok_n, first_ok_n, fail_n, stuck_n, cc_fail_n, hb_n, hd_n, hf_n, se_n, pt_n, nc_n);
  ph++;
  st = 0;
  rnd = 0;
  att = 0;
  ok_n = first_ok_n = fail_n = stuck_n = cc_fail_n = hb_n = hd_n = hf_n = se_n = pt_n = nc_n = 0;
  t_ph = t_wait = t_round = millis();
  ESP_LOGI(TAG, "ST phase_start %s", PHASES[ph].name);
}

inline void start(BLEClient *l, BLEClient *m, BLEClient *e, espbt::ESP32BLETracker *t, uint16_t ff01_l,
                  uint16_t ff01_m) {
  FF01[PL] = ff01_l;
  FF01[PM] = ff01_m;
  C[PL] = l;
  C[PM] = m;
  C[PE] = e;
  trk = t;
  for (int i = 0; i < 3; i++) {
    ADDR[i] = C[i]->get_address();
    RT[i] = new Rtt();  // NOLINT: allocated once at boot
    C[i]->register_ble_node(RT[i]);
  }
  ph = 0;
  st = -1;
  t_ph = millis();
}

// round start: a client that never returns to IDLE fails the round instead of stalling the suite
inline bool wait_idle(uint8_t p, uint32_t now) {
  if (idle(p))
    return true;
  if (now - t_wait > 20000) {
    stuck_n++;
    fail_n++;
    ESP_LOGW(TAG, "ST stuck_start %s r=%u state=%d", PNAME[p], rnd, (int) s(p));
    drop(p);
    t_wait = now;
    if (++rnd >= PHASES[ph].n)
      next_phase();
  }
  return false;
}

// after a discovery: the ff01 handle from the substitutions must be the ff01 characteristic of this server
inline void check_handle(uint8_t p) {
  static bool done[2];
  if (p > PM || done[p])
    return;
  // 5ec2ff01-0000-1000-8000-00805f9b34fb, least significant byte first
  esp_bt_uuid_t u{};
  u.len = ESP_UUID_LEN_128;
  static constexpr uint8_t FF01_UUID[16] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                                            0x00, 0x10, 0x00, 0x00, 0x01, 0xff, 0xc2, 0x5e};
  memcpy(u.uuid.uuid128, FF01_UUID, 16);
  esp_gattc_char_elem_t el{};
  uint16_t n = 1;
  esp_gatt_status_t ret =
      esp_ble_gattc_get_char_by_uuid(C[p]->get_gattc_if(), C[p]->get_conn_id(), 1, 0xFFFF, u, &el, &n);
  if (ret != ESP_GATT_OK) {  // a lookup error says nothing about the handle: check again on the next cold connect
    ESP_LOGW(TAG, "ST handle_lookup_failed %s ret=%d", PNAME[p], ret);
    return;
  }
  done[p] = true;
  if (n == 0 || el.char_handle != FF01[p]) {
    bad_handle[p] = true;
    ESP_LOGW(TAG, "ST handle_mismatch %s handle_%s=%u server_ff01=%u (ret=%d): RTTs use another characteristic",
             PNAME[p], p == PL ? "l" : "m", FF01[p], n ? el.char_handle : 0, ret);
  } else {
    ESP_LOGI(TAG, "ST handle_ok %s ff01=%u", PNAME[p], FF01[p]);
  }
}

// one RTT op on peer p; returns true when all 10 are done
inline bool rtt_step(uint8_t p) {
  Rtt *r = RT[p];
  if (r->pending) {
    if (millis() - r->t0 > 10000) {
      ESP_LOGW(TAG, "ST rtt timeout %s", PNAME[p]);
      r->pending = 0;
      rtt_to++;
      return true;
    }
    return false;
  }
  if (rtt_i > 0) {
    if (r->ok) {
      if (rtt_i <= 5) {
        rd_sum += r->done_ms;
        rd_n++;
      } else {
        wr_sum += r->done_ms;
        wr_n++;
      }
    }
  }
  if (rtt_i == 10)
    return true;
  r->t0 = millis();
  esp_err_t err;
  if (rtt_i < 5) {
    r->pending = 1;
    err = esp_ble_gattc_read_char(C[p]->get_gattc_if(), C[p]->get_conn_id(), FF01[p], ESP_GATT_AUTH_REQ_NONE);
  } else {
    r->pending = 2;
    uint8_t v = rtt_i;
    err = esp_ble_gattc_write_char(C[p]->get_gattc_if(), C[p]->get_conn_id(), FF01[p], 1, &v, ESP_GATT_WRITE_TYPE_RSP,
                                   ESP_GATT_AUTH_REQ_NONE);
  }
  if (err != ESP_OK) {
    r->pending = 0;
    r->ok = false;
    rtt_err++;
    ESP_LOGW(TAG, "ST rtt %s %s send err=%d", PNAME[p], rtt_i < 5 ? "read" : "write", err);
  }
  rtt_i++;
  return false;
}

// a connect attempt with a budget: 0 running, 1 up, -1 failed or dropped, -2 timed out
inline int attempt(uint8_t p, uint32_t budget) {
  if (up(p)) {
    ESP_LOGI(TAG, "ST conn_up %s r=%u total=%u", PNAME[p], rnd, (unsigned) (millis() - t0));
    return 1;
  }
  if (idle(p) && millis() - t0 > 200) {
    ESP_LOGI(TAG, "ST conn_fail %s r=%u after=%u", PNAME[p], rnd, (unsigned) (millis() - t0));
    return -1;
  }
  if (millis() - t0 > budget) {
    ESP_LOGI(TAG, "ST conn_timeout %s r=%u", PNAME[p], rnd);
    drop(p);
    return -2;
  }
  return 0;
}

inline void tick() {
  if (st == -1) {
    // wait for the three peers to be seen (the Eve Flare may take a while) and the boot to settle
    static uint32_t t_log = 0;
    if (millis() - t_log > 30000) {
      t_log = millis();
      ESP_LOGI(TAG, "ST waiting seen L=%d M=%d eve=%d", seen[PL], seen[PM], seen[PE]);
    }
    if (millis() - t_ph > 600000 && !(seen[PL] && seen[PM])) {
      // wrong mac_l/mac_m substitutions or a server that is off: nothing can run
      ESP_LOGW(TAG, "ST not_seen L=%d M=%d, nothing run", seen[PL], seen[PM]);
      ph = sizeof(PHASES) / sizeof(PHASES[0]) - 1;
      st = 0;
      ESP_LOGI(TAG, "ST phase_start %s", PHASES[ph].name);
      return;
    }
    if (millis() - t_ph > 20000 && seen[PL] && seen[PM] && (seen[PE] || millis() - t_ph > 180000)) {
      ESP_LOGI(TAG, "ST ready seen L=%d M=%d eve=%d eve_type=%d", seen[PL], seen[PM], seen[PE], atype[PE]);
      if (!seen[PE])  // the Eve phases will fail on the missing peer, not on the proxy
        ESP_LOGW(TAG, "ST eve_not_seen");
      st = 0;
      t_ph = millis();
      ESP_LOGI(TAG, "ST phase_start %s", PHASES[ph].name);
    }
    return;
  }
  const Phase &P = PHASES[ph];
  uint32_t now = millis();
  switch (P.k) {
    case K_ADV: {
      // 10 s buckets: active for n s, then passive for n s (first 5 s after the switch skipped), then active again
      static uint32_t skip_until = 0;
      if (st == 0) {
        adv0 = adv_n;
        t_wait = now;
        adv_mode_n = 0;
        st = 1;
      }
      if (st == 3 || st == 4) {
        if (now - t_wait > 30000) {
          ESP_LOGW(TAG, "ST adv scanner_stuck state=%d, phase abandoned", (int) trk->get_scanner_state());
          trk->set_scan_active(true);  // later phases need the active scan they normally get
          trk->start_scan();
          next_phase();
          break;
        }
        if (trk->get_scanner_state() == espbt::ScannerState::IDLE) {
          trk->start_scan();
          ESP_LOGI(TAG, "ST adv scan restarted %s", st == 3 ? "passive" : "active");
          if (st == 4) {
            next_phase();
            break;
          }
          st = 2;
          skip_until = now + 5000;
        }
        break;
      }
      if ((int32_t) (now - skip_until) < 0) {
        adv0 = adv_n;
        t_wait = now;
        break;
      }
      if (now - t_wait >= 10000) {
        ESP_LOGI(TAG, "ST adv mode=%s n10=%u", st == 1 ? "active" : "passive", (unsigned) (adv_n - adv0));
        adv0 = adv_n;
        t_wait = now;
        adv_mode_n++;
        if (adv_mode_n == P.n / 10) {
          adv_mode_n = 0;
          trk->set_scan_active(st == 2);
          trk->stop_scan();
          st = st == 1 ? 3 : 4;  // restart once the scanner is idle
        }
      }
      break;
    }
    case K_CONN:
    case K_EVE: {
      uint8_t p = P.peer;
      if (st == 0) {
        if (!wait_idle(p, now))
          break;
        connect(p, P.cold);
        st = 1;
      } else if (st == 1) {
        uint32_t budget = P.k == K_EVE ? 40000 : (P.cold ? 60000 : 30000);
        int r = attempt(p, budget);
        if (r == 1 && P.k == K_EVE) {
          st = 7;
        } else if (r == 1) {
          if (P.cold)
            check_handle(p);
          rtt_i = 0;
          rd_sum = wr_sum = 0;
          rd_n = wr_n = rtt_err = rtt_to = 0;
          RT[p]->bad = 0;
          st = 2;
        } else if (r < 0) {
          fail_n++;
          st = 3;
        }
      } else if (st == 2) {
        if (!up(p) || rtt_step(p)) {
          // ok only when the link held and a command went through, as drive.py counts t_cmd; a wrong handle is a
          // harness fault, counted apart like drive.py's stale_handles
          // also checked by the read length, which covers cached phases and failed handle lookups
          if (p <= PM && RT[p]->len_bad && !bad_handle[p]) {
            bad_handle[p] = true;
            ESP_LOGW(TAG, "ST handle_mismatch %s: ff01 read returned another length", PNAME[p]);
          }
          if (p <= PM && bad_handle[p])
            hb_n++;
          else if (up(p) && wr_n && (rtt_err || rtt_to || RT[p]->bad))
            pt_n++;                // the RTT loop broke off: ok by t_cmd but partial, like drive.py's rtt_partial
          else if (up(p) && wr_n)  // a write went through, as drive.py's t_cmd
            ok_n++;
          else
            fail_n++;
          ESP_LOGI(TAG, "ST rtt %s r=%u read_avg=%u write_avg=%u n=%u/%u err=%u timeout=%u bad=%u", PNAME[p], rnd,
                   rd_n ? (unsigned) (rd_sum / rd_n) : 0, wr_n ? (unsigned) (wr_sum / wr_n) : 0, rd_n, wr_n, rtt_err,
                   rtt_to, RT[p]->bad);
          st = 3;
        }
      } else if (st == 3) {
        drop(p);
        t_wait = now;
        st = 4;
      } else if (st == 4) {
        if (idle(p) || now - t_wait > 15000) {
          if (!idle(p)) {
            stuck_n++;
            ESP_LOGW(TAG, "ST stuck %s state=%d", PNAME[p], (int) s(p));
          }
          t_wait = now;
          st = 5;
        }
      } else if (st == 7) {
        // Eve: success needs one read of the first readable characteristic, as in drive.py eve
        esp_gattc_char_elem_t chars[20];
        uint16_t n = 20, hnd = 0;
        esp_gatt_status_t gs =
            esp_ble_gattc_get_all_char(C[p]->get_gattc_if(), C[p]->get_conn_id(), 1, 0xFFFF, chars, &n, 0);
        if (gs != ESP_GATT_OK)
          ESP_LOGW(TAG, "ST eve get_all_char_err=%d r=%u (GATT cache or discovery)", gs, rnd);
        if (gs == ESP_GATT_OK) {
          for (uint16_t i = 0; i < n && hnd == 0; i++)
            if (chars[i].properties & ESP_GATT_CHAR_PROP_BIT_READ)
              hnd = chars[i].char_handle;
        }
        if (hnd == 0) {  // an empty or partial table: a harness fault, as drive.py's no_readable_char
          nc_n++;
          ESP_LOGW(TAG, "ST eve r=%u no_readable_char, not counted", rnd);
          st = 3;
          break;
        }
        Rtt *rt = RT[p];
        rt->ok = false;
        rt->t0 = now;
        rt->done_ms = 0;
        rt->pending = 1;
        esp_err_t err = esp_ble_gattc_read_char(C[p]->get_gattc_if(), C[p]->get_conn_id(), hnd, ESP_GATT_AUTH_REQ_NONE);
        if (err != ESP_OK) {
          rt->pending = 0;
          ESP_LOGI(TAG, "ST eve read r=%u failed: read_err %d", rnd, err);
        }
        st = 8;
      } else if (st == 8) {
        Rtt *rt = RT[p];
        if (rt->pending && up(p) && now - rt->t0 < 10000)
          break;
        if (rt->pending)
          ESP_LOGI(TAG, "ST eve read r=%u failed: %s", rnd, up(p) ? "timeout" : "link_lost");
        rt->pending = 0;
        if (rt->ok)
          ok_n++;
        else
          fail_n++;
        ESP_LOGI(TAG, "ST eve read r=%u ok=%d ms=%u", rnd, rt->ok, (unsigned) rt->done_ms);
        st = 3;
      } else if (st == 5 && now - t_wait > (P.k == K_EVE ? 5000u : 2000u)) {
        rnd++;
        st = 0;
        if (rnd >= P.n)
          next_phase();
      }
      break;
    }
    case K_RETRY: {
      uint8_t p = P.peer;
      if (st == 0) {
        if (!wait_idle(p, now))
          break;
        connect(p, true);
        st = 1;
      } else if (st == 1) {
        int r = attempt(p, 10000);
        if (r == 1) {
          if (p <= PM && bad_handle[p]) {  // the write would hit another characteristic: a harness fault
            hb_n++;
            ESP_LOGW(TAG, "ST retry %s r=%u handle_bad, not counted", PNAME[p], rnd);
            att = 0;
            st = 3;
            break;
          }
          // one ff01 write must go through, as drive.py's retry counts t_cmd
          Rtt *rt = RT[p];
          uint8_t v = 0xC0;
          rt->ok = false;
          rt->t0 = now;
          rt->pending = 2;
          esp_err_t err = esp_ble_gattc_write_char(C[p]->get_gattc_if(), C[p]->get_conn_id(), FF01[p], 1, &v,
                                                   ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
          if (err != ESP_OK) {
            rt->pending = 0;
            se_n++;
            ESP_LOGW(TAG, "ST retry %s r=%u send err=%d", PNAME[p], rnd, err);
          }
          st = 8;
        } else if (r < 0) {
          att++;
          if (att >= 5) {
            fail_n++;
            ESP_LOGI(TAG, "ST retry %s r=%u attempts=5 ok=0", PNAME[p], rnd);
            att = 0;
            st = 3;
          } else {
            st = 6;  // wait idle, then the next attempt of the same round
            t_wait = now;
          }
        }
      } else if (st == 8) {
        // like drive.py: one 10 s budget from the connect start to the command, five attempts per round
        Rtt *rt = RT[p];
        if (rt->pending && up(p) && now - t0 < 10000)
          break;
        rt->pending = 0;
        if (rt->ok) {
          ok_n++;
          if (att == 0)
            first_ok_n++;
          ESP_LOGI(TAG, "ST retry %s r=%u attempts=%u ok=1", PNAME[p], rnd, att + 1);
          att = 0;
          st = 3;
        } else if (++att >= 5) {
          fail_n++;
          ESP_LOGI(TAG, "ST retry %s r=%u attempts=5 ok=0", PNAME[p], rnd);
          att = 0;
          st = 3;
        } else {
          drop(p);  // the command failed on an up link: disconnect and try the next attempt
          st = 6;
          t_wait = now;
        }
      } else if (st == 6) {
        if ((idle(p) && now - t_wait > 1000) || now - t_wait > 15000)
          st = 0;
      } else if (st == 3) {
        drop(p);
        t_wait = now;
        st = 4;
      } else if (st == 4) {
        if (idle(p) || now - t_wait > 15000) {
          if (!idle(p))
            stuck_n++;
          t_wait = now;
          st = 5;
        }
      } else if (st == 5 && now - t_wait > 2000) {
        rnd++;
        st = 0;
        if (rnd >= P.n)
          next_phase();
      }
      break;
    }
    case K_THIRD_EVE:
    case K_THIRD_PD:
    case K_THIRD_L: {
      // hold two (cached; a fresh Eve link as the second for PD and L), then connect the third without cache
      uint8_t third = P.k == K_THIRD_EVE ? PE : (P.k == K_THIRD_PD ? PM : PL);
      uint8_t h1 = P.k == K_THIRD_L ? PM : PL, h2 = P.k == K_THIRD_EVE ? PM : PE;
      if (st == 0) {
        if (!up(h1) && idle(h1)) {
          connect(h1, false);
          t_wait = now;
        }
        st = 1;
      } else if (st == 1 && now - t_round > 90000) {
        // h1 or h2 never settled: end the round instead of retrying forever
        ESP_LOGI(TAG, "ST third hold_failed r=%u h1=%d h2=%d", rnd, (int) s(h1), (int) s(h2));
        hf_n++;  // the held links never came up: a harness fault, not a third-link result
        att = 0;
        t_wait = now;
        st = 5;
      } else if (st == 1) {
        if (up(h1)) {
          if (P.k != K_THIRD_EVE && !idle(h2)) {
            drop(h2);
            break;
          }
          if (idle(h2) || up(h2)) {
            if (!up(h2))
              connect(h2, false);
            t_wait = now;
            st = 2;
          }
        } else if (idle(h1) && now - t_wait > 500) {
          st = 0;  // retry the hold
        }
      } else if (st == 2) {
        if (up(h2) && up(h1)) {
          att = 0;
          t_wait = now;
          st = 3;
        } else if ((idle(h2) && now - t_wait > 500) || now - t_wait > 20000) {
          ESP_LOGI(TAG, "ST third held_failed r=%u", rnd);
          drop(h2);
          if (++att >= 3) {  // an absent peer (Eve may never be seen) ends the round
            att = 0;
            hf_n++;
            t_wait = now;
            st = 5;
          } else {
            st = 1;
          }
        }
      } else if (st == 3 && now - t_wait > 500) {
        if (!idle(third)) {  // a stuck third slot fails the round instead of a confusing connect
          if (now - t_wait > 20000) {
            stuck_n++;
            fail_n++;
            ESP_LOGW(TAG, "ST stuck_start %s r=%u state=%d", PNAME[third], rnd, (int) s(third));
            drop(third);
            t_wait = now;
            st = 5;
          }
          break;
        }
        connect(third, true);
        st = 4;
      } else if (st == 4) {
        int r = attempt(third, 30000);
        if (r != 0) {
          ESP_LOGI(TAG, "ST third %s r=%u result=%d t=%u held=%d%d", PNAME[third], rnd, r, (unsigned) (now - t0),
                   up(h1), up(h2));
          if (!(up(h1) && up(h2)))
            hd_n++;  // a held link dropped: not a third-link result, kept out of ok and fail as report.py does
          else if (r == 1)
            ok_n++;
          else
            fail_n++;
          drop(third);
          if (P.k != K_THIRD_EVE)
            drop(h2);
          t_wait = now;
          st = 5;
        }
      } else if (st == 5) {
        if ((idle(third) && now - t_wait > 3000) || now - t_wait > 15000) {
          if (!idle(third)) {
            stuck_n++;
            ESP_LOGW(TAG, "ST stuck %s state=%d", PNAME[third], (int) s(third));
          }
          rnd++;
          t_round = now;
          st = 0;
          if (rnd >= P.n) {
            drop(PL);
            drop(PM);
            drop(PE);
            next_phase();
          }
        }
      }
      break;
    }
    case K_NOTIFY: {
      // V1 connect so the ble_client sensor finds the tick characteristic and subscribes
      if (st == 0) {
        tk_rx = tk_gaps = tk_missing = tk_missing_drop = tk_back = tk_dups = tk_short = 0;
        tk_have = tk_resumed = false;
        tk_count = true;
        tk_unheld_ms = 0;
        tk_down_since = now;
        st = 1;
        t_wait = now;
      }
      if (st == 1 && idle(PL)) {
        C[PL]->set_connection_type(espbt::ConnectionType::V1);
        C[PL]->set_remote_addr_type(atype[PL]);
        C[PL]->set_state(ClientState::DISCOVERED);
        tk_resumed = true;
        st = 2;
      } else if (st == 2 && up(PL)) {
        tk_unheld_ms += now - tk_down_since;
        st = 3;
      } else if (st == 2 && idle(PL) && now - t_wait > 1000) {
        st = 1;
      } else if (st == 3 && !up(PL)) {
        fail_n++;
        ESP_LOGW(TAG, "ST notify drop state=%d", (int) s(PL));
        tk_down_since = now;
        st = 1;
      }
      if (now - t_ph >= 60000u * (rnd + 1)) {
        rnd++;
        const uint32_t unheld_ms = tk_unheld_ms + (st == 3 ? 0 : now - tk_down_since);
        ESP_LOGI(TAG,
                 "ST notify min=%u rx=%u gaps=%u missing=%u drops=%u missing_drop=%u backwards=%u dups=%u short=%u "
                 "unheld_s=%u",
                 rnd, (unsigned) tk_rx, (unsigned) tk_gaps, (unsigned) tk_missing, fail_n, (unsigned) tk_missing_drop,
                 (unsigned) tk_back, (unsigned) tk_dups, (unsigned) tk_short, (unsigned) (unheld_ms / 1000));
        if (rnd * 60 >= P.n) {
          if (tk_rx == 0) {  // the sensor never subscribed to the tick: the phase measured nothing
            fail_n++;
            ESP_LOGW(TAG, "ST notify no_ticks");
          }
          tk_count = false;
          drop(PL);
          C[PL]->set_connection_type(espbt::ConnectionType::V3_WITH_CACHE);
          next_phase();
        }
      }
      break;
    }
    case K_CHURN: {
      uint8_t p = rnd % 2 ? PM : PL;
      if (st == 0) {
        if (!wait_idle(p, now))
          break;
        connect(p, false);
        st = 1;
      } else if (st == 1) {
        int r = attempt(p, 20000);
        if (r == 1) {
          ok_n++;
          t_wait = now;
          st = 2;
        } else if (r < 0) {
          fail_n++;
          st = 3;
        }
      } else if (st == 2 && now - t_wait > 300) {
        st = 3;
      } else if (st == 3) {
        drop(p);
        t_wait = now;
        st = 4;
      } else if (st == 4) {
        if (idle(p) || now - t_wait > 10000) {
          if (!idle(p)) {
            stuck_n++;
            ESP_LOGW(TAG, "ST churn stuck %s state=%d", PNAME[p], (int) s(p));
          }
          ESP_LOGI(TAG, "ST churn c=%u freed_ms=%u", rnd, (unsigned) (now - t_wait));
          t_wait = now;
          st = 5;
        }
      } else if (st == 5 && now - t_wait > 500) {
        rnd++;
        st = 0;
        if (rnd >= P.n)
          next_phase();
      }
      break;
    }
    case K_DONE:
      if (st == 0) {
        ESP_LOGI(TAG, "ST DONE");
        st = 1;
      }
      break;
  }
}
}  // namespace selftest
