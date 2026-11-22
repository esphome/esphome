#pragma once
// Scenario runner for two radios joined by sxair.hpp, with packet_interface radio links on them (esphome#21110):
// raw links (no layers) for sizes, metadata and listen before talk, reliability links and session plus rolling code
// links carrying entity_transport sensors both ways. Called every 20 ms; logs "RES" lines and "DONE fails=N".
// Define SXL_FSK for a FSK pair (63 byte packets).
#include <cmath>
#include "sxair.hpp"
#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/sensor/sensor.h"

namespace sxlink {
using namespace esphome;
using packet_interface::PacketBuffer;
using packet_interface::PacketInterface;
using packet_interface::PacketMetaData;
static const char *const TAG = "ev";

#ifdef SXL_FSK
static constexpr uint16_t MAXP = 63;
static const uint16_t SIZES[] = {1, 2, 16, 32, 62, 63, 64};
#else
static constexpr uint16_t MAXP = 255;
static const uint16_t SIZES[] = {1, 2, 16, 64, 128, 200, 254, 255, 256};
#endif
static constexpr int NSIZES = sizeof(SIZES) / sizeof(SIZES[0]);

inline void fill(uint8_t *d, uint16_t len) {
  d[0] = 0xEE;
  for (uint16_t i = 1; i < len; i++)
    d[i] = (uint8_t) (i * 7 + len);
}
inline bool check(const uint8_t *d, size_t len) {
  if (len == 0 || d[0] != 0xEE)
    return false;
  for (size_t i = 1; i < len; i++)
    if (d[i] != (uint8_t) (i * 7 + len))
      return false;
  return true;
}

struct RawRx {
  const char *name;
  uint32_t n{0}, bad{0};
  uint16_t last_len{0};
  float rssi{NAN}, snr{NAN};
  void attach(PacketInterface *link) {
    link->add_packet_interface_listener([this](const PacketBuffer &data, const PacketMetaData &meta) {
      uint8_t buf[256];
      size_t len = data.size();
      if (len == 0 || len > sizeof(buf)) {
        this->bad++;
        ESP_LOGW(TAG, "EV raw %s unexpected len=%u", this->name, (unsigned) len);
        return;
      }
      data.copy_to(buf, len);
      if (buf[0] != 0xEE)
        return;  // traffic of the other links on the same radio
      if (!check(buf, len)) {
        this->bad++;
        return;
      }
      this->n++;
      this->last_len = len;
      this->rssi = meta.rssi;
      this->snr = meta.snr;
      ESP_LOGI(TAG, "EV raw %s len=%u rssi=%.1f snr=%.2f", this->name, (unsigned) len, meta.rssi, meta.snr);
    });
  }
};
static RawRx RXA{"on_a"}, RXB{"on_b"};
static uint32_t DRV[2]{};  // packets each radio driver delivered to on_packet

inline bool raw_send(PacketInterface *link, uint16_t len) {
  static uint8_t buf[260];
  fill(buf, len);
  return link->send_to_interface(PacketBuffer(buf, len), {});
}

struct Ctx {
  PacketInterface *raw_a, *raw_b, *links[6];
  int nlinks;
  sxair::Radio *ma, *mb;
  sensor::Sensor *sa, *sb, *sa2, *sb2;                      // local sensors
  sensor::Sensor *ra_on_b, *rb_on_a, *ra2_on_b, *rb2_on_a;  // remote copies
  // start an async send of len raw bytes on radio B
  bool (*b_async)(const uint8_t *, size_t);
} C;

static int step = 0, sub = 0;
static uint32_t next_ms = 0, t0 = 0, cnt = 0, cnt2 = 0, first_ok = 0, over0 = 0;
static float want = NAN;
static uint32_t raw_base = 0;  // the receiver's raw count before the last accepted raw send
static bool raw_sent = false;
static uint32_t fails = 0;  // values that never arrived, wrong send results, held sends never released
static sensor::Sensor *watch_s = nullptr;
static const char *watch_name = "";

inline void drv_report(const char *label) {
  ESP_LOGI(TAG, "RES driver %s delivered A=%u B=%u (air delivered A=%u B=%u)", label, (unsigned) DRV[0],
           (unsigned) DRV[1], (unsigned) C.ma->rx_ok, (unsigned) C.mb->rx_ok);
}
inline void idle_report(const char *label) {
  drv_report(label);
  char s[80];
  size_t o = 0;
  for (int i = 0; i < C.nlinks; i++)
    o += snprintf(s + o, sizeof(s) - o, "%s=%d ", C.links[i]->get_name(), C.links[i]->is_idle());
  ESP_LOGI(TAG, "RES idle %s %s", label, s);
}

// Waits for a remote sensor to show the value; true when done (arrived or 15 s)
inline bool wait_value() {
  if (watch_s->has_state() && watch_s->state == want) {
    ESP_LOGI(TAG, "RES %s arrived value=%.0f after=%ums", watch_name, want, (unsigned) (millis() - t0));
    return true;
  }
  if (millis() - t0 > 15000) {
    fails++;
    ESP_LOGI(TAG, "RES %s NOT arrived (state=%.1f) after 15 s", watch_name, watch_s->state);
    return true;
  }
  return false;
}
inline void publish_and_watch(sensor::Sensor *local, float v, sensor::Sensor *remote, const char *name) {
  want = v;
  watch_s = remote;
  watch_name = name;
  t0 = millis();
  local->publish_state(v);
}

#define WAIT(ms) next_ms = millis() + (ms)
#define NEXT() (step++, sub = 0)

inline void run() {
  if ((int32_t) (millis() - next_ms) < 0)
    return;
  switch (step) {
    case 0:
      sxair::log_radio("boot", C.ma);
      sxair::log_radio("boot", C.mb);
      idle_report("boot");
      ESP_LOGI(TAG, "PHASE entities");
      publish_and_watch(C.sa, 11, C.ra_on_b, "rel A->B");
      NEXT();
      return;
    case 1:
      if (!wait_value())
        return;
      publish_and_watch(C.sb, 22, C.rb_on_a, "rel B->A");
      NEXT();
      return;
    case 2:
      if (!wait_value())
        return;
      publish_and_watch(C.sa2, 33, C.ra2_on_b, "session A->B");
      NEXT();
      return;
    case 3:
      if (!wait_value())
        return;
      publish_and_watch(C.sb2, 44, C.rb2_on_a, "session B->A");
      NEXT();
      return;
    case 4:
      if (!wait_value())
        return;
      ESP_LOGI(TAG, "PHASE raw_sizes");
      NEXT();
      WAIT(1500);
      return;
    case 5: {  // one size per pass, A to B
      if (sub > 0) {
        const uint16_t prev = SIZES[sub - 1];
        ESP_LOGI(TAG, "RES size %u A->B rx_on_b=%u last_len=%u rssi=%.1f snr=%.2f bad=%u air_us=%u", prev,
                 (unsigned) RXB.n, RXB.last_len, RXB.rssi, RXB.snr, (unsigned) RXB.bad,
                 (unsigned) C.ma->last_tx_air_us);
        if (raw_sent && (RXB.n != raw_base + 1 || RXB.last_len != prev))
          fails++;  // an accepted packet that did not arrive once and whole
      }
      if (sub == NSIZES) {
        NEXT();
        return;
      }
      const uint16_t len = SIZES[sub];
      if (cnt == 0)
        t0 = millis();
      const bool ok = raw_send(C.raw_a, len);
      if (!ok && len <= MAXP && millis() - t0 < 3000) {
        cnt++;  // refused while the other links' traffic is on air; try again
        WAIT(30);
        return;
      }
      if (ok != (len <= MAXP))
        fails++;  // a valid size refused, or an oversize one accepted
      raw_sent = ok;
      raw_base = RXB.n;
      ESP_LOGI(TAG, "RES send size %u -> %s after %u refusals", len, ok ? "sent" : "refused", (unsigned) cnt);
      cnt = 0;
      sub++;
      WAIT(900);
      return;
    }
    case 6: {
      const bool ok = raw_send(C.raw_b, MAXP);
      if (!ok && cnt++ < 100) {
        WAIT(30);
        return;
      }
      cnt = 0;
      if (!ok)
        fails++;
      raw_sent = ok;
      raw_base = RXA.n;
      ESP_LOGI(TAG, "RES send size %u B->A -> %s", MAXP, ok ? "sent" : "refused");
      NEXT();
      WAIT(900);
      return;
    }
    case 7:
      ESP_LOGI(TAG, "RES size %u B->A rx_on_a=%u last_len=%u rssi=%.1f snr=%.2f bad=%u", MAXP, (unsigned) RXA.n,
               RXA.last_len, RXA.rssi, RXA.snr, (unsigned) RXA.bad);
      if (raw_sent && (RXA.n != raw_base + 1 || RXA.last_len != MAXP))
        fails++;
      idle_report("after_sizes");
      ESP_LOGI(TAG, "PHASE lbt_peer_sending");
      over0 = C.ma->tx_over_signal;
      {
        static uint8_t big[256];
        fill(big, MAXP);
        const bool ok = C.b_async(big, MAXP);
        if (!ok)
          fails++;  // nothing on air, so the listen before talk check below proves nothing
        ESP_LOGI(TAG, "RES peer async %u bytes -> %d airtime=%uus", MAXP, ok, (unsigned) C.mb->last_tx_air_us);
      }
      t0 = millis();
      cnt = cnt2 = 0;
      NEXT();
      return;
    case 8: {  // try from A every pass until accepted
      const bool idle_before = C.raw_a->is_idle();
      const bool ok = raw_send(C.raw_a, 10);
      if (!ok) {
        cnt++;
        if (!idle_before || !C.raw_a->is_idle())
          cnt2++;  // the link's loop ran while the send was held
        if (millis() - t0 > 6000) {
          fails++;
          ESP_LOGI(TAG, "RES lbt never released, refused=%u", (unsigned) cnt);
          NEXT();
        }
        return;
      }
      const uint32_t end = C.mb->last_tx_end_ms;
      if (cnt == 0 || C.ma->tx_over_signal != over0)
        fails++;  // sent at once over the peer's packet, or over a signal: listen before talk did not hold it
      ESP_LOGI(TAG,
               "RES lbt refused=%u loop_active_while_held=%u accepted after=%ums, peer packet ended at %dms, "
               "a_received_peer=%u tx_over_signal_delta=%u",
               (unsigned) cnt, (unsigned) cnt2, (unsigned) (millis() - t0), (int) (end - t0), (unsigned) C.ma->rx_ok,
               (unsigned) (C.ma->tx_over_signal - over0));
      NEXT();
      WAIT(400);
      return;
    }
    case 9:
      idle_report("after_lbt");
      ESP_LOGI(TAG, "PHASE hold_cap");
      sxair::jam(C.ma, UINT32_MAX);
      t0 = millis();
      cnt = cnt2 = first_ok = 0;
      NEXT();
      return;
    case 10: {  // a radio busy for ever: sends are held for max_busy, then go
      const bool ok = raw_send(C.raw_a, 8);
      const uint32_t el = millis() - t0;
      if (ok) {
        cnt2++;
        ESP_LOGI(TAG, "RES hold release %u at %ums (refused before it %u) loop_idle=%d", (unsigned) cnt2, (unsigned) el,
                 (unsigned) cnt, C.raw_a->is_idle());
        cnt = 0;
      } else {
        cnt++;
      }
      if (el > 5000) {
        if (cnt2 == 0)
          fails++;
        ESP_LOGI(TAG, "RES hold end releases=%u loop_idle_while_jammed=%d", (unsigned) cnt2, C.raw_a->is_idle());
        sxair::unjam(C.ma);
        NEXT();
        WAIT(300);
      } else {
        WAIT(50);
      }
      return;
    }
    case 11:
      idle_report("after_unjam");
      ESP_LOGI(TAG, "PHASE reliability_after_refused");
      over0 = C.ma->tx_over_signal;
      sxair::jam(C.ma, 800);
      publish_and_watch(C.sa, 55, C.ra_on_b, "rel A->B while A busy 800ms");
      NEXT();
      return;
    case 12:
      if (!wait_value())
        return;
      if (C.ma->tx_over_signal != over0)
        fails++;  // outside the hold cap phase no send may start over a signal
      ESP_LOGI(TAG, "RES tx_over_signal_delta=%u", (unsigned) (C.ma->tx_over_signal - over0));
      ESP_LOGI(TAG, "PHASE reliability_lost");
      cnt = C.ma->tx;
      sxair::AIR.drop_next = 1;
      publish_and_watch(C.sa, 66, C.ra_on_b, "rel A->B first packet lost");
      NEXT();
      return;
    case 13:
      if (!wait_value())
        return;
      ESP_LOGI(TAG, "RES lost: A sent %u packets for it, dropped=%u", (unsigned) (C.ma->tx - cnt),
               (unsigned) sxair::AIR.dropped);
      cnt = C.mb->tx;
      sxair::AIR.corrupt_next = 1;
      publish_and_watch(C.sb, 77, C.rb_on_a, "rel B->A first packet corrupted");
      NEXT();
      return;
    case 14:
      if (!wait_value())
        return;
      ESP_LOGI(TAG, "RES corrupt: B sent %u packets for it, corrupted=%u a_rx_crc=%u", (unsigned) (C.mb->tx - cnt),
               (unsigned) sxair::AIR.corrupted, (unsigned) C.ma->rx_crc);
      sxair::AIR.drop_next = 2;
      publish_and_watch(C.sa2, 88, C.ra2_on_b, "session A->B two packets lost");
      NEXT();
      return;
    case 15:
      if (!wait_value())
        return;
      publish_and_watch(C.sb2, 99, C.rb2_on_a, "session B->A");
      NEXT();
      return;
    case 16:
      if (!wait_value())
        return;
      NEXT();
      WAIT(3000);
      return;
    case 17:
      sxair::log_radio("end", C.ma);
      sxair::log_radio("end", C.mb);
      idle_report("end");
      // raw packets with a bad pattern count too
      ESP_LOGI(TAG, "DONE fails=%u", (unsigned) (fails + RXA.bad + RXB.bad));
      NEXT();
      return;
    default:
      return;
  }
}
}  // namespace sxlink
