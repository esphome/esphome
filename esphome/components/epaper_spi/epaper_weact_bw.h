#pragma once

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

class EPaperWeActBW final : public EPaperMono {
 public:
  EPaperWeActBW(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length) {}

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  bool current_partial_update_{false};
  bool write_old_buffer_pass_{false};
};

}  // namespace esphome::epaper_spi
