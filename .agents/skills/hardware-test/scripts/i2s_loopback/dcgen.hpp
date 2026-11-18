#pragma once
// I2S test signal generator on I2S1 (master) and a capture analyzer for the microphone data callback.
#include <atomic>
#include <cmath>
#include <cstring>
#include <climits>
#include "driver/i2s_std.h"
#include "esphome/core/log.h"
#include "esphome/components/microphone/microphone.h"

namespace dcgen {
inline volatile int mode = 0;
inline volatile uint32_t seq = 0;
inline i2s_chan_handle_t tx;
inline uint32_t rate = 16000;

inline void set_mode(int m) {
  mode = m;
  seq = seq + 1;
}

// 50 Hz square phase for sample n
inline bool square_hi(uint32_t n) { return ((uint64_t) n * 100 / rate) % 2; }

inline void gen_task(void *) {
  static int16_t buf[2 * 256];
  uint32_t n = 0;
  for (;;) {
    const int m = mode;
    for (int i = 0; i < 256; i++, n++) {
      int32_t l = 0, r = 0;
      switch (m) {
        case 1:
          l = r = 8000;
          break;  // DC step
        case 2:
          l = 8000;
          r = -8000;
          break;   // opposite DC per channel
        case 3: {  // DC plus 1 kHz tone
          const float s = 8000.0f * sinf(2.0f * (float) M_PI * 1000.0f * (float) (n % rate) / (float) rate);
          l = r = 8000 + (int32_t) s;
          break;
        }
        case 4:
          l = r = square_hi(n) ? 32767 : -32768;
          break;  // full-scale square
        case 5:
          l = r = square_hi(n) ? 16000 : -16000;
          break;  // half-scale square
        default:
          break;
      }
      buf[2 * i] = (int16_t) l;
      buf[2 * i + 1] = (int16_t) r;
    }
    size_t w;
    const esp_err_t err = i2s_channel_write(tx, buf, sizeof(buf), &w, portMAX_DELAY);
    if (err != ESP_OK) {
      ESP_LOGE("dc", "i2s write failed: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

inline void start(uint32_t sr, int bclk, int ws, int dout) {
  rate = sr;
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&cc, &tx, nullptr));
  i2s_std_config_t sc = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sr),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                   .bclk = (gpio_num_t) bclk,
                   .ws = (gpio_num_t) ws,
                   .dout = (gpio_num_t) dout,
                   .din = I2S_GPIO_UNUSED,
                   .invert_flags = {}},
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &sc));
  ESP_ERROR_CHECK(i2s_channel_enable(tx));
  if (xTaskCreate(gen_task, "dcgen", 4096, nullptr, 10, nullptr) != pdPASS)
    ESP_LOGE("dc", "generator task not created");
}

// ---- analyzer (runs on the microphone task) ----
constexpr int TRACE_MS = 600;
struct Ch {
  int64_t sum{0}, sq{0}, tsum{0}, tsq{0};
  int32_t mn{INT32_MAX}, mx{INT32_MIN};
  uint32_t tn{0};
};
inline Ch ch[2], res[2];
inline int32_t trace[TRACE_MS], rtrace[TRACE_MS];
inline uint32_t n = 0, rn = 0, seen = 0, blkn = 0, blki = 0, rblki = 0, chans = 0, rchans = 0, sr = 0;
inline int64_t blk = 0;
inline int seg_mode = 0, rmode = 0;
inline std::atomic<bool> ready{false};  // hands the res snapshot between the two tasks

inline void on_data(const esphome::microphone::AudioCapture &c) {
  chans = c.stream_info.get_channels();
  sr = c.stream_info.get_sample_rate();
  if (dcgen::seq != seen) {
    if (!ready) {
      res[0] = ch[0];
      res[1] = ch[1];
      memcpy(rtrace, trace, sizeof(trace));
      rn = n;
      rmode = seg_mode;
      rchans = chans;
      rblki = blki;
      ready = true;
    }
    ch[0] = Ch{};
    ch[1] = Ch{};
    n = blkn = blki = 0;
    blk = 0;
    seen = dcgen::seq;
    seg_mode = dcgen::mode;
  }
  const auto *s = reinterpret_cast<const int16_t *>(c.data.data());
  const size_t frames = c.data.size() / 2 / chans;
  const uint32_t per_ms = sr / 1000;
  for (size_t f = 0; f < frames; f++, n++) {
    for (uint32_t k = 0; k < chans && k < 2; k++) {
      const int32_t v = s[f * chans + k];
      Ch &x = ch[k];
      x.sum += v;
      x.sq += (int64_t) v * v;
      if (v < x.mn)
        x.mn = v;
      if (v > x.mx)
        x.mx = v;
      if (n >= sr) {  // last 500 ms of a 1.5 s segment
        x.tsum += v;
        x.tsq += (int64_t) v * v;
        x.tn++;
      }
      if (k == 0) {
        blk += v;
        if (++blkn == per_ms) {
          if (blki < TRACE_MS)
            trace[blki++] = (int32_t) (blk / (int64_t) blkn);
          blk = 0;
          blkn = 0;
        }
      }
    }
  }
}

inline void report() {
  if (!ready)
    return;
  if (rn == 0) {
    ready = false;
    return;
  }
  static constexpr const char *NAMES[] = {"silence",   "dc+8000",          "dc L+8000 R-8000",
                                          "dc+tone1k", "square fullscale", "square half"};
  for (uint32_t k = 0; k < rchans && k < 2; k++) {
    const Ch &x = res[k];
    const double mean = (double) x.sum / rn, rms = sqrt((double) x.sq / rn);
    const double tmean = x.tn ? (double) x.tsum / x.tn : 0, trms = x.tn ? sqrt((double) x.tsq / x.tn) : 0;
    const double tac = sqrt(fmax(0.0, trms * trms - tmean * tmean));
    ESP_LOGI("dc", "SEG mode=%d (%s) ch%u n=%u mean=%.0f rms=%.0f min=%d max=%d | tail mean=%.1f rms=%.0f ac=%.0f",
             rmode, NAMES[rmode], (unsigned) k, (unsigned) rn, mean, rms, (int) x.mn, (int) x.mx, tmean, trms, tac);
  }
  // settling on channel 0 after a step: first 1 ms block over 1500, then time until 20 blocks stay under 80
  int t0 = -1;
  for (uint32_t i = 0; i < rblki; i++)
    if (abs(rtrace[i]) > 1500) {
      t0 = i;
      break;
    }
  if (t0 >= 0) {
    int settle = -1;
    for (uint32_t i = t0; i + 20 <= rblki; i++) {
      bool ok = true;
      for (int j = 0; j < 20; j++)
        if (abs(rtrace[i + j]) >= 80) {
          ok = false;
          break;
        }
      if (ok) {
        settle = i - t0;
        break;
      }
    }
    auto at = [&](int ms) { return (t0 + ms < (int) rblki) ? rtrace[t0 + ms] : 99999; };
    ESP_LOGI("dc",
             "SETTLE mode=%d step_at=%d ms settle_1pct=%d ms trace 0:%d 5:%d 10:%d 20:%d 40:%d 80:%d 160:%d 320:%d",
             rmode, t0, settle, at(0), at(5), at(10), at(20), at(40), at(80), at(160), at(320));
  }
  ready = false;
}
}  // namespace dcgen
