#pragma once
// Colour correction A/B (#22868): step every rp2040_pio_led_strip strip and partition through a fixed list of cases,
// log what the light passes to the strip (IN/PIN lines) and decode three frames from each strip's own pin.
#include "pioscope.hpp"
#include "esphome/components/light/addressable_light.h"

namespace cccases {
using esphome::Color;
using esphome::light::LightState;
using esphome::light::AddressableLight;

// mode 0: light call only; 1: then pixel 0 set to black; 2: then every pixel set to the raw colour via set()
struct Case {
  const char *name;
  float bri;
  float cb;
  uint8_t r, g, b, w;
  uint8_t mode;
  uint8_t mr{255}, mg{255}, mbl{255}, mw{255};
};
static constexpr Case CASES[] = {
    {"or30_5", 0.05f, 1, 255, 30, 0, 0, 0},
    {"or130_1", 0.01f, 1, 255, 130, 0, 0, 0},
    {"or130_10", 0.10f, 1, 255, 130, 0, 0, 0},
    {"white_1", 0.01f, 1, 255, 255, 255, 0, 0},
    {"white_5", 0.05f, 1, 255, 255, 255, 0, 0},
    {"rgbw_all_5", 0.05f, 1, 255, 255, 255, 255, 0},
    {"wonly_5", 0.05f, 1, 0, 0, 0, 255, 2},
    {"red_1", 0.01f, 1, 255, 0, 0, 0, 0},
    {"green_1", 0.01f, 1, 0, 255, 0, 0, 0},
    {"blue_1", 0.01f, 1, 0, 0, 255, 0, 0},
    {"red_5", 0.05f, 1, 255, 0, 0, 0, 0},
    {"thr200_10", 0.10f, 1, 255, 200, 0, 0, 0},
    {"thr199_10", 0.10f, 1, 255, 199, 0, 0, 0},
    {"thr202_5", 0.05f, 1, 255, 202, 0, 0, 0},
    {"thr201_5", 0.05f, 1, 255, 201, 0, 0, 0},
    {"bw30_5", 0.05f, 1, 0, 0, 255, 30, 0},
    {"black_px0_5", 0.05f, 1, 255, 30, 0, 0, 1},
    {"or30_20", 0.20f, 1, 255, 30, 0, 0, 0},
    {"or30_50", 0.50f, 1, 255, 30, 0, 0, 0},
    {"or30_100", 1.00f, 1, 255, 30, 0, 0, 0},
    {"or130_50", 0.50f, 1, 255, 130, 0, 0, 0},
    {"or130_100", 1.00f, 1, 255, 130, 0, 0, 0},
    {"white_50", 0.50f, 1, 255, 255, 255, 0, 0},
    {"white_100", 1.00f, 1, 255, 255, 255, 0, 0},
    {"red_50", 0.50f, 1, 255, 0, 0, 0, 0},
    {"green_100", 1.00f, 1, 0, 255, 0, 0, 0},
    {"thr200_50", 0.50f, 1, 255, 200, 0, 0, 0},
    {"thr199_100", 1.00f, 1, 255, 199, 0, 0, 0},
    {"bw30_50", 0.50f, 1, 0, 0, 255, 30, 0},
    {"bw30_100", 1.00f, 1, 0, 0, 255, 30, 0},
    {"black_px0_50", 0.50f, 1, 255, 30, 0, 0, 1},
    {"cb50_or30_5", 0.05f, 0.5f, 255, 30, 0, 0, 0},
    {"cb50_or30_20", 0.20f, 0.5f, 255, 30, 0, 0, 0},
    {"cb50_white_5", 0.05f, 0.5f, 255, 255, 255, 0, 0},
    {"cb75_red_1", 0.01f, 0.75f, 255, 0, 0, 0, 0},
    {"cb50_or30_50", 0.50f, 0.5f, 255, 30, 0, 0, 0},
    {"or130_lb1", 0.004f, 1, 255, 130, 0, 0, 0},
    {"white_lb1", 0.004f, 1, 255, 255, 255, 0, 0},
    {"thr230_lb1", 0.004f, 1, 255, 230, 0, 0, 0},
    {"red_lb1", 0.004f, 1, 255, 0, 0, 0, 0},
    {"rgbw_wonly_lb1", 0.004f, 1, 0, 0, 0, 255, 2},
    {"mbg50_or130_10", 0.10f, 1, 255, 130, 0, 0, 0, 255, 128, 255, 255},
    {"mbg50_yel_5", 0.05f, 1, 255, 255, 0, 0, 0, 255, 128, 255, 255},
    {"mbg50_yel_lb1", 0.004f, 1, 255, 255, 0, 0, 0, 255, 128, 255, 255},
    {"mbr50_white_5", 0.05f, 1, 255, 255, 255, 0, 0, 128, 255, 255, 255},
    {"mbw50_redw_5", 0.05f, 1, 255, 0, 0, 255, 0, 255, 255, 255, 128},
    {"mbg50_yel_50", 0.50f, 1, 255, 255, 0, 0, 0, 255, 128, 255, 255},
    {"mbr50_or30_50", 0.50f, 1, 255, 30, 0, 0, 0, 128, 255, 255, 255},
    {"mbg50_yel_100", 1.00f, 1, 255, 255, 0, 0, 0, 255, 128, 255, 255},
    {"dis_g_1x255_100", 1.00f, 1, 1, 255, 0, 0, 0, 255, 0, 255, 255},
    {"dis_g_yel_5", 0.05f, 1, 255, 255, 0, 0, 0, 255, 0, 255, 255},
    {"dis_g_yel_lb1", 0.004f, 1, 255, 255, 0, 0, 0, 255, 0, 255, 255},
    {"dis_w_redw_5", 0.05f, 1, 255, 0, 0, 255, 0, 255, 255, 255, 0},
};
static constexpr size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

struct Strip {
  const char *name;
  LightState *state;
  uint pin;
  bool rgbw;
  bool mb;
};

inline void apply(size_t i, const Strip *strips, size_t n) {
  const Case &c = CASES[i];
  for (size_t s = 0; s < n; s++) {
    auto *out = static_cast<AddressableLight *>(strips[s].state->get_output());
    if (strips[s].mb)
      out->set_correction(c.mr / 255.0f, c.mg / 255.0f, c.mbl / 255.0f, c.mw / 255.0f);
    auto call = strips[s].state->turn_on();
    call.set_transition_length(0);
    call.set_brightness(c.bri);
    call.set_color_brightness(c.cb);
    call.set_rgb(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f);
    if (c.r == 0 && c.g == 0 && c.b == 0)
      call.set_rgb(1, 1, 1);  // the call cannot carry black; mode 2 overwrites the pixels
    if (strips[s].rgbw)
      call.set_white(c.w / 255.0f);
    call.perform();
  }
}

inline void probe(size_t i, const Strip *strips, size_t n, int round) {
  const Case &c = CASES[i];
  for (size_t s = 0; s < n; s++) {
    auto *st = strips[s].state;
    auto *out = static_cast<AddressableLight *>(st->get_output());
    auto v = st->current_values;
    Color in = esphome::light::color_from_light_color_values(v);
    uint8_t lb = esphome::light::to_uint8_scale(v.get_brightness() * v.get_state());
    out->update_state(st);  // recompute the frame from the current values and correction
    if (c.mode == 1)
      out->get(0).set(Color(0, 0, 0, 0));
    if (c.mode == 2)
      for (int k = 0; k < out->size(); k++)
        out->get(k).set(Color(c.r, c.g, c.b, c.w));
    Color px0 = c.mode == 1 ? Color(0, 0, 0, 0) : (c.mode == 2 ? Color(c.r, c.g, c.b, c.w) : in);
    Color px1 = c.mode == 2 ? Color(c.r, c.g, c.b, c.w) : in;
    Color mb = strips[s].mb ? Color(c.mr, c.mg, c.mbl, c.mw) : Color(255, 255, 255, 255);
    ESP_LOGI("hwt", "IN r%d %s %s lb=%u px0=%02x%02x%02x%02x px1=%02x%02x%02x%02x mb=%02x%02x%02x%02x", round, c.name,
             strips[s].name, lb, px0.r, px0.g, px0.b, px0.w, px1.r, px1.g, px1.b, px1.w, mb.r, mb.g, mb.b, mb.w);
    for (int k = 0; k < 3; k++) {
      char label[48];
      snprintf(label, sizeof(label), "r%d %s %s %d", round, c.name, strips[s].name, k);
      pioscope::arm(strips[s].pin);
      out->write_state(st);
      size_t words = pioscope::collect();
      pioscope::decode(label, words, out->size() * (strips[s].rgbw ? 32 : 24));
      busy_wait_us_32(5000);
    }
  }
}
// Partition cases: part 0 = [RGB strip, RGBW strip] (RGB traits), part 1 = [RGBW strip, RGB strip] (RGBW traits).
// mode 0: light call; 2: then every partition pixel set to the raw colour via set()
static constexpr Case PCASES[] = {
    {"p_or30_5", 0.05f, 1, 255, 30, 0, 0, 0},        {"p_white_1", 0.01f, 1, 255, 255, 255, 0, 0},
    {"p_set128w_5", 0.05f, 1, 128, 0, 0, 255, 2},    {"p_set128w_lb1", 0.004f, 1, 128, 0, 0, 255, 2},
    {"p_or30w255_5", 0.05f, 1, 255, 30, 0, 255, 0},  {"p_or30w0_5", 0.05f, 1, 255, 30, 0, 0, 0},
    {"p_bw30_5", 0.05f, 1, 0, 0, 255, 30, 0},        {"p_or30_50", 0.50f, 1, 255, 30, 0, 0, 0},
    {"p_or30w255_50", 0.50f, 1, 255, 30, 0, 255, 0}, {"p_set128w_50", 0.50f, 1, 128, 0, 0, 255, 2},
};
static constexpr size_t NUM_PCASES = sizeof(PCASES) / sizeof(PCASES[0]);

inline void papply(size_t i, LightState *part, bool rgbw) {
  const Case &c = PCASES[i];
  auto call = part->turn_on();
  call.set_transition_length(0);
  call.set_brightness(c.bri);
  call.set_rgb(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f);
  if (rgbw)
    call.set_white(c.w / 255.0f);
  call.perform();
}

// probe every segment strip after the partition light PART was set
inline void pprobe(size_t i, LightState *part, const char *pname, const Strip *segs, size_t ns, int round) {
  const Case &c = PCASES[i];
  auto *out = static_cast<AddressableLight *>(part->get_output());
  auto v = part->current_values;
  Color in = esphome::light::color_from_light_color_values(v);
  uint8_t lb = esphome::light::to_uint8_scale(v.get_brightness() * v.get_state());
  out->update_state(part);
  if (c.mode == 2) {
    in = Color(c.r, c.g, c.b, c.w);
    for (int k = 0; k < out->size(); k++)
      out->get(k).set(in);
  }
  ESP_LOGI("hwt", "PIN r%d %s %s lb=%u px=%02x%02x%02x%02x", round, c.name, pname, lb, in.r, in.g, in.b, in.w);
  for (size_t s = 0; s < ns; s++) {
    auto *so = static_cast<AddressableLight *>(segs[s].state->get_output());
    for (int k = 0; k < 3; k++) {
      char label[64];
      snprintf(label, sizeof(label), "r%d %s@%s %s %d", round, c.name, pname, segs[s].name, k);
      pioscope::arm(segs[s].pin);
      so->write_state(segs[s].state);
      size_t words = pioscope::collect();
      pioscope::decode(label, words, so->size() * (segs[s].rgbw ? 32 : 24));
      busy_wait_us_32(5000);
    }
  }
}
}  // namespace cccases
