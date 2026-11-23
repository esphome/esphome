#pragma once
// FLAC decode benchmark with the decoder path of audio_decoder.cpp: micro_flac::FLACDecoder fed from a
// RAMAllocator input buffer into a RAMAllocator output buffer (same allocator as AudioTransferBuffer).
// Logs per run: file, decode time of the decode() calls only, real-time factor, FNV-1a of the PCM bytes.
#include <micro_flac/flac_decoder.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstring>
#ifndef MALLOC_CAP_EXEC
#define FB_IRAM_FREE() 0
#else
#define FB_IRAM_FREE() heap_caps_get_free_size(MALLOC_CAP_EXEC)
#endif
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// FB_MEDIA: absolute directory holding s16/m16/s24/m24.flac, set by the config's build flag
#ifndef FB_MEDIA
#error "set FB_MEDIA to the media directory (see bench-esp32.yaml)"
#endif
#define FB_INCBIN(sym, path) \
  asm(".section .rodata." #sym ",\"a\"\n.balign 4\n.global " #sym "\n" #sym ":\n.incbin \"" path "\"\n" \
      ".global " #sym "_end\n" #sym "_end:\n.byte 0\n.balign 4\n.previous\n")

FB_INCBIN(fb_s16, FB_MEDIA "/s16.flac");
FB_INCBIN(fb_m16, FB_MEDIA "/m16.flac");
FB_INCBIN(fb_s24, FB_MEDIA "/s24.flac");
FB_INCBIN(fb_m24, FB_MEDIA "/m24.flac");
extern "C" const uint8_t fb_s16[], fb_s16_end[], fb_m16[], fb_m16_end[], fb_s24[], fb_s24_end[], fb_m24[], fb_m24_end[];

namespace flacbench {
static const char *const TAG = "flacbench";
struct File {
  const char *name;
  const uint8_t *begin;
  const uint8_t *end;
};
static const File FILES[] = {
    {"s16", fb_s16, fb_s16_end}, {"m16", fb_m16, fb_m16_end}, {"s24", fb_s24, fb_s24_end}, {"m24", fb_m24, fb_m24_end}};
static constexpr size_t IN_CAP = 16384;
static std::atomic<bool> running{false};  // set by the button automation, cleared by the task on core 1
static int rounds_req = 5;

static inline uint32_t fnv(uint32_t h, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

static void run_one(const File &f, int round) {
  esphome::RAMAllocator<uint8_t> alloc;
  uint8_t *in = alloc.allocate(IN_CAP);
  size_t out_cap = 4096;
  uint8_t *out = alloc.allocate(out_cap);
  if (in == nullptr || out == nullptr) {
    ESP_LOGE(TAG, "alloc failed");
    alloc.deallocate(in, IN_CAP);
    alloc.deallocate(out, out_cap);
    return;
  }
  auto dec = std::make_unique<micro_flac::FLACDecoder>();
  const uint8_t *src = f.begin;
  size_t in_len = 0, in_off = 0;
  uint32_t h = 2166136261u, bps = 0, ch = 0, rate = 0;
  uint64_t samples = 0, dec_us = 0, total = 0;
  int64_t wall0 = esp_timer_get_time();
  bool decode_error = false, stalled = false;
  while (true) {
    if (in_off > 0 && (stalled || in_len - in_off < IN_CAP / 2)) {  // refill like a transfer buffer
      memmove(in, in + in_off, in_len - in_off);
      in_len -= in_off;
      in_off = 0;
    }
    if (in_off == 0) {
      size_t n = std::min<size_t>(IN_CAP - in_len, f.end - src);
      memcpy(in + in_len, src, n);
      src += n;
      in_len += n;
    }
    stalled = false;
    size_t consumed = 0, decoded = 0;
    int64_t t0 = esp_timer_get_time();
    auto r = dec->decode(in + in_off, in_len - in_off, out, out_cap, consumed, decoded);
    dec_us += esp_timer_get_time() - t0;
    if (r == micro_flac::FLAC_DECODER_SUCCESS) {
      in_off += consumed;
      size_t bytes = decoded * ((bps + 7) / 8);
      h = fnv(h, out, bytes);
      samples += decoded;
    } else if (r == micro_flac::FLAC_DECODER_HEADER_READY) {
      const auto &info = dec->get_stream_info();
      bps = info.bits_per_sample();
      ch = info.num_channels();
      total = info.total_samples_per_channel();
      rate = info.sample_rate();
      out_cap = dec->get_output_buffer_size_samples() * info.bytes_per_sample();
      uint8_t *grown = alloc.reallocate(out, out_cap);
      if (grown == nullptr) {
        ESP_LOGE(TAG, "out realloc failed");
        decode_error = true;
        break;
      }
      out = grown;
      in_off += consumed;
    } else if (r == micro_flac::FLAC_DECODER_END_OF_STREAM) {
      break;
    } else if (r == micro_flac::FLAC_DECODER_NEED_MORE_DATA) {
      in_off += consumed;
      if (src == f.end && in_off >= in_len)
        break;
      if (consumed == 0) {
        // no progress: a truncated tail or a frame bigger than the input buffer would loop forever
        if (src == f.end || in_len - in_off == IN_CAP) {
          decode_error = true;
          ESP_LOGE(TAG, "%s stuck on NEED_MORE_DATA", f.name);
          break;
        }
        stalled = true;
      }
    } else {
      decode_error = true;
      ESP_LOGE(TAG, "%s decode error %d", f.name, (int) r);
      break;
    }
  }
  // the decoder takes a truncated last frame without an error: input running out is not proof of a full decode
  if (!decode_error && (ch == 0 || (total != 0 && samples / ch != total))) {
    decode_error = true;
    ESP_LOGE(TAG, "%s decoded %llu of %llu samples per channel", f.name, (unsigned long long) (ch ? samples / ch : 0),
             (unsigned long long) total);
  }
  int64_t wall = esp_timer_get_time() - wall0;
  // what freeing the decoder gives back, with both buffers still held: the out buffer growth stays out of it
  const size_t dram1 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t iram1 = FB_IRAM_FREE();
  const size_t ps1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  dec.reset();
  const int d_dram = (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) - (int) dram1;
  const int d_iram = (int) FB_IRAM_FREE() - (int) iram1;
  const int d_ps = (int) heap_caps_get_free_size(MALLOC_CAP_SPIRAM) - (int) ps1;
  alloc.deallocate(out, out_cap);
  alloc.deallocate(in, IN_CAP);
  double audio_s = ch ? (double) samples / ch / rate : 0;
  double rtf = dec_us ? audio_s / (dec_us / 1e6) : 0;
  ESP_LOGI(TAG,
           "RES r%d %s bps %u ch %u frames %llu dec_us %llu wall_us %lld rtf %.2f fnv %08x err %d used_dram %d "
           "used_iram %d used_ps %d",
           round, f.name, (unsigned) bps, (unsigned) ch, (unsigned long long) (ch ? samples / ch : 0),
           (unsigned long long) dec_us, (long long) wall, rtf, (unsigned) h, (int) decode_error, d_dram, d_iram, d_ps);
}

static void task(void *) {
  ESP_LOGI(TAG, "START rounds %d cpu %d MHz int_free %u int_largest %u ps_free %u", rounds_req,
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  for (int r = 0; r < rounds_req; r++) {
    for (const auto &f : FILES) {
      run_one(f, r);
      vTaskDelay(pdMS_TO_TICKS(150));
    }
  }
  ESP_LOGI(TAG, "BENCHDONE");
  running = false;
  vTaskDelete(nullptr);
}

static void start(int rounds) {
  if (running)
    return;
  running = true;
  rounds_req = rounds;
  if (xTaskCreatePinnedToCore(task, "flacbench", 8192, nullptr, 5, nullptr, 1) != pdPASS) {
    ESP_LOGE(TAG, "task create failed");
    running = false;
  }
}
}  // namespace flacbench
