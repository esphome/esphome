#include "waveshare_epaper.h"

namespace esphome {
namespace waveshare_epaper {

class WaveshareEPaper4P2InV2 : public WaveshareEPaper {
 public:
  void display() override;
  void initialize() override;
  void deep_sleep() override;

  void set_full_update_every(uint32_t full_update_every);

 protected:
  void fast_initialize_();
  void full_update_();

  void reset_();
  void set_window_(uint16_t x, uint16_t y, uint16_t x1, uint16_t y2);
  void set_cursor_(uint16_t x, uint16_t y);
  void turn_on_display_full_();
  void clear_();

  int get_width_internal() override;
  int get_height_internal() override;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
};

}  // namespace waveshare_epaper
}  // namespace esphome