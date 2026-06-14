#pragma once

#include "epaper_spi_spectra_e6.h"

namespace esphome::epaper_spi {

class EPaperSpectraE6DualCS : public EPaperSpectraE6 {
 public:
  EPaperSpectraE6DualCS(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                         size_t init_sequence_length)
      : EPaperSpectraE6(name, width, height, init_sequence, init_sequence_length) {}

  void set_cs_slave_pin(GPIOPin *pin) { this->cs_slave_ = pin; }

  void setup() override;
  void dump_config() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

 protected:
  void both_command_(uint8_t cmd);
  void both_cmd_data_(uint8_t cmd, const uint8_t *data, size_t len);
  void slave_command_(uint8_t cmd);
  void slave_start_data_();
  void slave_stop_data_();

  GPIOPin *cs_slave_{nullptr};
  bool transfer_to_slave_{false};
};

}  // namespace esphome::epaper_spi
