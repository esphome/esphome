#include <benchmark/benchmark.h>

#include <memory>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/addressable_light_effect.h"
#include "esphome/components/light/base_light_effects.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/transformers.h"

extern const esphome::light::GammaTable bench_gamma_2_8;

namespace esphome::benchmarks {

static constexpr int kLoopIterations = 2000;
static constexpr int kStripIterations = 200;
static constexpr int32_t kStripLeds = 150;
// One hour, so a run never reaches the end of a transition or flash
static constexpr uint32_t kLongMs = 3600000;

// Starts half way through, so every loop() sees a transition in progress
class MidwayTransition : public light::LightTransitionTransformer {
 public:
  void start() override {
    LightTransitionTransformer::start();
    this->start_time_ -= this->length_ / 2;
  }
};

// Writes the gamma corrected channels like a PWM driver would
class LoopLightOutput : public light::LightOutput {
 public:
  explicit LoopLightOutput(light::ColorMode mode) { this->traits_.set_supported_color_modes({mode}); }
  light::LightTraits get_traits() override { return this->traits_; }
  std::unique_ptr<light::LightTransformer> create_default_transition() override {
    return std::make_unique<MidwayTransition>();
  }
  void write_state(light::LightState *state) override {
    if (this->traits_.supports_color_mode(light::ColorMode::BRIGHTNESS)) {
      state->current_values_as_brightness(&this->out_[0]);
    } else {
      state->current_values_as_rgbww(&this->out_[0], &this->out_[1], &this->out_[2], &this->out_[3], &this->out_[4]);
    }
    benchmark::DoNotOptimize(this->out_);
  }

 protected:
  light::LightTraits traits_;
  float out_[5]{};
};

// In-memory RGB strip with no hardware behind it
class FakeStrip : public light::AddressableLight {
 public:
  FakeStrip() : accessor_impl_(this->colors_, this->effect_data_, light::ChannelColors{0, 1, 2}, &this->correction_) {
    this->accessor_ = &this->accessor_impl_;
  }
  int32_t size() const override { return kStripLeds; }
  void clear_effect_data() override { this->accessor_impl_.clear_effect_data(kStripLeds); }
  light::LightTraits get_traits() override {
    light::LightTraits traits;
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void write_state(light::LightState * /*state*/) override { benchmark::DoNotOptimize(this->colors_); }

 protected:
  uint8_t colors_[kStripLeds * 3]{};
  uint8_t effect_data_[kStripLeds]{};
  light::CorrectedBufferColorAccessor accessor_impl_;
};

class LoopLightState : public light::LightState {
 public:
  using LightState::LightState;
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }
};

static void setup_light(LoopLightState &light) {
  light.configure("bench_light");
  light.set_default_transition_length(0);
  light.set_gamma_table(&bench_gamma_2_8);
  light.get_output()->setup_state(&light);
}

static void run_loop(benchmark::State &state, LoopLightState &light, int iterations) {
  for (auto _ : state) {
    for (int i = 0; i < iterations; i++) {
      light.loop();
    }
    benchmark::DoNotOptimize(light.current_values);
  }
  state.SetItemsProcessed(state.iterations() * iterations);
}

// --- loop() with an RGBWW transition half way: lerp, gamma and output write ---

static void LightTransition_RGBWW(benchmark::State &state) {
  LoopLightOutput output(light::ColorMode::RGB_COLD_WARM_WHITE);
  LoopLightState light(&output);
  setup_light(light);
  light.make_call()
      .set_state(true)
      .set_brightness(0.2f)
      .set_red(1.0f)
      .set_green(0.0f)
      .set_blue(0.0f)
      .set_cold_white(1.0f)
      .set_warm_white(0.0f)
      .perform();
  light.make_call()
      .set_brightness(1.0f)
      .set_red(0.0f)
      .set_green(0.5f)
      .set_blue(1.0f)
      .set_cold_white(0.0f)
      .set_warm_white(1.0f)
      .set_transition_length(kLongMs)
      .perform();
  run_loop(state, light, kLoopIterations);
}
BENCHMARK(LightTransition_RGBWW);

// --- loop() with a brightness-only transition half way ---

static void LightTransition_Monochromatic(benchmark::State &state) {
  LoopLightOutput output(light::ColorMode::BRIGHTNESS);
  LoopLightState light(&output);
  setup_light(light);
  light.make_call().set_state(true).set_brightness(0.1f).perform();
  light.make_call().set_brightness(1.0f).set_transition_length(kLongMs).perform();
  run_loop(state, light, kLoopIterations);
}
BENCHMARK(LightTransition_Monochromatic);

// --- loop() with an RGBWW flash, inside its fade-in transition ---

static void LightFlash_RGBWW(benchmark::State &state) {
  LoopLightOutput output(light::ColorMode::RGB_COLD_WARM_WHITE);
  LoopLightState light(&output);
  setup_light(light);
  light.set_flash_transition_length(kLongMs);
  light.make_call().set_state(true).set_brightness(0.2f).set_red(1.0f).set_green(0.0f).set_blue(0.0f).perform();
  light.make_call().set_brightness(1.0f).set_red(0.0f).set_blue(1.0f).set_flash_length(kLongMs * 2).perform();
  run_loop(state, light, kLoopIterations);
}
BENCHMARK(LightFlash_RGBWW);

// --- loop() with the flicker effect: one instant LightCall per loop ---

static void LightEffect_Flicker_RGBWW(benchmark::State &state) {
  LoopLightOutput output(light::ColorMode::RGB_COLD_WARM_WHITE);
  LoopLightState light(&output);
  setup_light(light);
  light::FlickerLightEffect flicker("Flicker");
  flicker.set_alpha(0.95f);
  flicker.set_intensity(0.015f);
  light::LightEffect *const effects[] = {&flicker};
  light.add_effects(effects, 1);
  flicker.init_internal(&light);
  light.make_call().set_state(true).set_brightness(0.8f).set_red(1.0f).set_green(0.6f).set_blue(0.2f).perform();
  light.make_call().set_effect(1u).perform();
  run_loop(state, light, kLoopIterations);
}
BENCHMARK(LightEffect_Flicker_RGBWW);

// --- loop() running an addressable effect on a 150 LED strip ---

static void run_strip_effect(benchmark::State &state, light::AddressableLightEffect &effect) {
  FakeStrip strip;
  LoopLightState light(&strip);
  setup_light(light);
  light::LightEffect *const effects[] = {&effect};
  light.add_effects(effects, 1);
  effect.init_internal(&light);
  light.make_call().set_state(true).set_brightness(1.0f).set_red(1.0f).set_green(0.5f).set_blue(0.0f).perform();
  light.make_call().set_effect(1u).perform();
  run_loop(state, light, kStripIterations);
}

static void AddressableEffect_Rainbow_150(benchmark::State &state) {
  light::AddressableRainbowLightEffect effect("Rainbow");
  run_strip_effect(state, effect);
}
BENCHMARK(AddressableEffect_Rainbow_150);

static void AddressableEffect_ColorWipe_150(benchmark::State &state) {
  static constexpr light::AddressableColorWipeEffectColor COLORS[] = {
      {255, 0, 0, 0, light::NO_RANDOM_SLOT, true, 10},
      {0, 0, 0, 0, 0, false, 10},
  };
  Color random_colors[1];
  light::AddressableColorWipeEffect effect("Color Wipe");
  effect.set_colors(COLORS, 2, random_colors);
  effect.set_add_led_interval(0);  // shift every loop
  run_strip_effect(state, effect);
}
BENCHMARK(AddressableEffect_ColorWipe_150);

static void AddressableEffect_Scan_150(benchmark::State &state) {
  light::AddressableScanEffect effect("Scan");
  effect.set_move_interval(0);  // move every loop
  effect.set_scan_width(3);
  run_strip_effect(state, effect);
}
BENCHMARK(AddressableEffect_Scan_150);

static void AddressableEffect_Twinkle_150(benchmark::State &state) {
  light::AddressableTwinkleEffect effect("Twinkle");
  effect.set_progress_interval(4);
  run_strip_effect(state, effect);
}
BENCHMARK(AddressableEffect_Twinkle_150);

static void AddressableEffect_RandomTwinkle_150(benchmark::State &state) {
  light::AddressableRandomTwinkleEffect effect("Random Twinkle");
  effect.set_twinkle_probability(0.05f);
  effect.set_progress_interval(32);
  run_strip_effect(state, effect);
}
BENCHMARK(AddressableEffect_RandomTwinkle_150);

}  // namespace esphome::benchmarks
