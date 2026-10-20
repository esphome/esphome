#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ssd1306_base/ssd1306_base.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
#include "esphome/core/helpers.h"
#include "ssd1306_partial_update.h"
#endif

namespace esphome::ssd1306_i2c {

class I2CSSD1306 final : public ssd1306_base::SSD1306, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  void update() override;
  void loop() override;
  void set_partial_updates() { this->partial_updates_enabled_ = true; }
#endif

 protected:
  void command(uint8_t value) override;
  void write_display_data() override;

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  enum class ResetPhase : uint8_t { RESET_PHASE_IDLE, RESET_PHASE_HIGH, RESET_PHASE_LOW };
  struct PartialUpdateState {
    RAMUniquePtr<uint8_t[]> previous_buffer;
    PartialUpdatePlanner planner;
    uint32_t last_recovery_attempt{0};
    uint32_t reset_phase_started{0};
    ResetPhase reset_phase{ResetPhase::RESET_PHASE_IDLE};
    bool have_previous_buffer{false};
    bool controller_reinit_required{false};
    bool command_failed{false};
    bool transport_failed{false};
    bool initializing_controller{false};
    bool retry_wait{false};
  };

  void mark_partial_transport_failure_(bool reinitialize = false);
  void commit_shadow_buffer_();
  bool reinitialize_controller_(bool display_off);
  bool write_checked_region_(const Region &region);
  bool write_checked_full_frame_();
  bool finish_recovery_(bool reinitialize);
  void recover_();

  bool partial_updates_enabled_{false};
  RAMUniquePtr<PartialUpdateState> partial_update_state_;
#endif
};

}  // namespace esphome::ssd1306_i2c
