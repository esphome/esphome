// In-chip pulse density probe for a sigma-delta (or any PWM-like) output pin, plus a PCM feeder task.
// Sampler: core 0 task reads the pin's input register with pseudo-random cycle gaps; one window
// (SDP_N samples, about 2 ms) gives a density in 1/10000. Windows are stored with their time for dumping.
// Feeder: core 0 task generating DC / sine / stereo PCM in 8/16/24/32 bit and calling speaker->play().
#pragma once
#include <cmath>
#include <atomic>
#include "driver/gpio.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/core/log.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"

namespace sdp {
static const char *const TAG = "sdp";
static constexpr int SDP_N = 2000;
static constexpr int SDP_MAX = 4096;
static int pin_ = -1;
static volatile int cap_left_ = 0;  // windows still to capture
static int cap_n_ = 0;
static uint16_t cap_d_[SDP_MAX];
static int64_t cap_t_[SDP_MAX];
static volatile uint32_t frames_played_ = 0;

static uint32_t xs_ = 2463534242u;
static inline uint32_t rnd() {
  xs_ ^= xs_ << 13;
  xs_ ^= xs_ >> 17;
  xs_ ^= xs_ << 5;
  return xs_;
}

static uint16_t window() {
  int ones = 0;
  for (int i = 0; i < SDP_N; i++) {
    uint32_t gap = rnd() & 255;  // 0..255 cycles at 360 MHz, decorrelates from the 1 MHz modulator
    uint32_t t0 = esp_cpu_get_cycle_count();
    while (esp_cpu_get_cycle_count() - t0 < gap) {
    }
    ones += gpio_get_level((gpio_num_t) pin_);
  }
  return (uint16_t) (ones * 10000 / SDP_N);
}

static void sampler_task(void *) {
  for (;;) {
    if (cap_left_ > 0) {
      int64_t t = esp_timer_get_time();
      uint16_t d = window();
      if (cap_n_ < SDP_MAX) {
        cap_d_[cap_n_] = d;
        cap_t_[cap_n_] = t;
        cap_n_++;
      }
      cap_left_ = cap_left_ - 1;
      taskYIELD();
      if ((cap_n_ & 3) == 0)
        vTaskDelay(1);
    } else {
      vTaskDelay(2);
    }
  }
}

// ---- feeder ----
enum Mode { M_OFF, M_DC, M_SINE, M_STEREO, M_BURST };
static std::atomic<int> mode_{M_OFF};
static volatile float lvl_ = 0, lvl_r_ = 0, freq_ = 2, amp_ = 0.8f;
static volatile int burst_left_ = 0;  // frames still to write in M_BURST
static esphome::speaker::Speaker *spk_ = nullptr;
static uint64_t phase_frames_ = 0;

static void put(uint8_t *p, int bytes, float v) {
  if (v > 1.0f)
    v = 1.0f;
  if (v < -1.0f)
    v = -1.0f;
  int64_t q = (int64_t) llroundf(v * 2147483647.0f);
  if (q > 2147483647LL)  // the float constant is 2^31, so +1.0 rounds one past INT32_MAX
    q = 2147483647LL;
  esphome::audio::pack_q31_as_audio_sample((int32_t) q, p, bytes);
}

static void feeder_task(void *) {
  static uint8_t buf[512 * 8];
  for (;;) {
    int m = mode_.load();
    if (m == M_OFF || spk_ == nullptr || (m == M_BURST && burst_left_ <= 0)) {
      vTaskDelay(2);
      continue;
    }
    auto info = spk_->get_audio_stream_info();
    int bps = info.samples_to_bytes(1);
    int ch = info.get_channels();
    uint32_t rate = info.get_sample_rate();
    int frames = 256;
    if (m == M_BURST && burst_left_ < frames)
      frames = burst_left_;
    // float per frame (the P4 FPU is single precision only); the chunk start phase is exact
    const float ph = (float) (2 * M_PI * fmod((double) freq_ * phase_frames_ / rate, 1.0));
    const float step = 2 * (float) M_PI * freq_ / rate;
    for (int f = 0; f < frames; f++) {
      float l = lvl_, r = lvl_r_;
      if (m == M_SINE) {
        l = r = amp_ * sinf(ph + f * step);
      } else if (m != M_STEREO) {
        r = l;
      }
      put(buf + f * bps * ch, bps, l);
      if (ch == 2)
        put(buf + f * bps * ch + bps, bps, r);
    }
    size_t len = (size_t) frames * bps * ch, off = 0;
    while (off < len && mode_.load() == m) {
      off += spk_->play(buf + off, len - off, pdMS_TO_TICKS(20));
    }
    uint32_t done = off / (bps * ch);
    phase_frames_ += done;
    if (m == M_BURST)
      burst_left_ = burst_left_ - (int) done;
  }
}

static void begin(esphome::speaker::Speaker *s, int pin) {
  spk_ = s;
  pin_ = pin;
  gpio_input_enable((gpio_num_t) pin);
  s->add_audio_output_callback([](uint32_t frames, int64_t) { frames_played_ += frames; });
  xTaskCreatePinnedToCore(sampler_task, "sdp_smp", 4096, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(feeder_task, "sdp_feed", 4096, nullptr, 6, nullptr, 0);
  ESP_LOGI(TAG, "probe on GPIO%d, loop core %d", pin, xPortGetCoreID());
}

static void set_mode(int m) {
  phase_frames_ = 0;
  mode_.store(m);
}

static void start_capture(int windows) {
  cap_n_ = 0;
  cap_left_ = windows > SDP_MAX ? SDP_MAX : windows;
}
static bool capture_done() { return cap_left_ == 0; }

// Summary line: mean/min/max density in percent.
static void summary(const char *label) {
  if (cap_n_ == 0) {
    ESP_LOGW(TAG, "%s: no windows", label);
    return;
  }
  double sum = 0, sq = 0;
  int mn = 10000, mx = 0;
  for (int i = 0; i < cap_n_; i++) {
    sum += cap_d_[i];
    sq += (double) cap_d_[i] * cap_d_[i];
    if (cap_d_[i] < mn)
      mn = cap_d_[i];
    if (cap_d_[i] > mx)
      mx = cap_d_[i];
  }
  double mean = sum / cap_n_;
  double sd = sqrt(sq / cap_n_ - mean * mean);
  ESP_LOGI(TAG, "RES %s n=%d mean=%.2f%% sd=%.2f min=%.2f%% max=%.2f%% span=%lldms", label, cap_n_, mean / 100,
           sd / 100, mn / 100.0, mx / 100.0, (long long) ((cap_t_[cap_n_ - 1] - cap_t_[0]) / 1000));
}

// Raw dump: "DUMP <label> <index> <t_us>:<density> ..." 16 per line, t_us relative to the first window.
static void dump(const char *label) {
  char line[400];
  for (int i = 0; i < cap_n_; i += 16) {
    int p = snprintf(line, sizeof(line), "DUMP %s %d", label, i);
    for (int j = i; j < cap_n_ && j < i + 16; j++)
      p += snprintf(line + p, sizeof(line) - p, " %lld:%u", (long long) (cap_t_[j] - cap_t_[0]), cap_d_[j]);
    ESP_LOGI(TAG, "%s", line);
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}

// 0 stopped, 1 starting or stopping, 2 running
static int st() { return spk_->is_stopped() ? 0 : (spk_->is_running() ? 2 : 1); }

// CPU left on core 1 for a task above the loop task: spin for ms, return iterations.
static volatile uint32_t burn_iters_ = 0;
static int burn_ms_ = 50;
static void burn_task(void *) {
  uint32_t n = 0;
  int64_t end = esp_timer_get_time() + burn_ms_ * 1000;
  while (esp_timer_get_time() < end)
    n++;
  burn_iters_ = n;
  vTaskDelete(nullptr);
}
static void burn_start(int ms) {
  burn_ms_ = ms;
  xTaskCreatePinnedToCore(burn_task, "sdp_burn", 2048, nullptr, 10, nullptr, 1);
}
}  // namespace sdp
