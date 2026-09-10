#pragma once

#ifdef USE_RP2

#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/components/light/light_output.h"

#include <hardware/dma.h>
#include <hardware/pio.h>
#include <hardware/structs/pio.h>
#include <pico/stdio.h>
#include <pico/sem.h>
#include <map>

namespace esphome::rp2040_pio_led_strip {

enum Chipset : uint8_t {
  CHIPSET_WS2812,
  CHIPSET_WS2812B,
  CHIPSET_SK6812,
  CHIPSET_SM16703,
  CHIPSET_APA102,
  CHIPSET_CUSTOM = 0xFF,
};

using init_fn = void (*)(PIO pio, uint sm, uint offset, uint pin, float freq);

class RP2040PIOLEDStripLightOutput final : public light::AddressableLight {
 public:
  RP2040PIOLEDStripLightOutput(size_t num_leds, light::ChannelColors channel_colors)
      : buffer_(num_leds, {.channel_colors = channel_colors}, &this->correction_) {}

  light::ESPColorBuffer &buffer() override { return this->buffer_; }

  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    this->buffer_.layout().channel_colors.has_white()
        ? traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE})
        : traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void set_pin(uint8_t pin) { this->pin_ = pin; }

  void set_max_refresh_rate(float interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_pio(int pio_num) { pio_num ? this->pio_ = pio1 : this->pio_ = pio0; }
  void set_program(const pio_program_t *program) { this->program_ = program; }
  void set_init_function(init_fn init) { this->init_ = init; }

  void set_chipset(Chipset chipset) { this->chipset_ = chipset; };

  void dump_config() override;

 protected:
  static void dma_write_complete_handler();

  light::InterleavedColorBuffer buffer_;

  uint8_t pin_;

  pio_hw_t *pio_;
  uint sm_;
  uint dma_chan_;
  dma_channel_config dma_config_;

  Chipset chipset_{CHIPSET_CUSTOM};

  uint32_t last_refresh_{0};
  float max_refresh_rate_;

  const pio_program_t *program_;
  init_fn init_;

 private:
  inline static int num_instance[2];
  inline static std::map<Chipset, bool> conf_count;
  inline static std::map<Chipset, int> chipset_offsets;
  inline static bool dma_chan_active[12];
  inline static struct semaphore dma_write_complete_sem[12];
};

}  // namespace esphome::rp2040_pio_led_strip

#endif  // USE_RP2
