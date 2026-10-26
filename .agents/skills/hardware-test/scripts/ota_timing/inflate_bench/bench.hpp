#pragma once
// On-device inflate speed for the ESPHome OTA decoder. Targets the decoder interface with dest_limit (dev as of
// this writing); once the interface without dest_limit lands (#21641), drop the dest_limit line.
#include "esphome/components/esphome/ota/ota_esphome_inflate.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "bench_streams.hpp"

static uint8_t bench_ring[32768];
static OtaInflateState bench_state;

// LibreTiny defines micros as a macro; the parentheses call the ESPHome function.
inline uint32_t bench_now() { return (esphome::micros) (); }

inline void run_inflate_bench() {
  for (const auto &c : BENCH_CASES) {
    for (unsigned ring : {4096u, 8192u, 16384u, 32768u}) {
      if (ring < c.enc_window)
        continue;
      uint32_t best = UINT32_MAX;
      size_t out = 0;
      int res = 0;
      for (int rep = 0; rep < 3; rep++) {
        ota_inflate_init(&bench_state, bench_ring, ring);
        bench_state.source = c.data;
        bench_state.source_limit = c.data + c.len;
        bench_state.source_read_cb = [](OtaInflateState *) -> int { return -1; };
        out = 0;
        uint32_t t0 = bench_now();
        do {
          bench_state.dest = bench_ring;
          bench_state.dest_limit = bench_ring + ring;
          res = ota_inflate(&bench_state);
          out += bench_state.dest - bench_ring;
          esphome::App.feed_wdt();
        } while (res == OTA_INFLATE_OK);
        uint32_t dt = bench_now() - t0;
        if (dt < best)
          best = dt;
      }
      ESP_LOGI("bench", "%s enc=%u ring=%u in=%u out=%u/%u res=%d best_us=%u out_kBps=%u in_kBps=%u", c.name,
               c.enc_window, ring, (unsigned) c.len, (unsigned) out, (unsigned) c.raw, res, (unsigned) best,
               (unsigned) ((uint64_t) out * 1000 / best), (unsigned) ((uint64_t) c.len * 1000 / best));
    }
  }
  ESP_LOGI("bench", "BENCHDONE");
}
