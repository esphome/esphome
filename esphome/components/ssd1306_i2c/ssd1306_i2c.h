#pragma once

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#endif

#include "esphome/core/component.h"
#include "esphome/components/ssd1306_base/ssd1306_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ssd1306_i2c {

class I2CSSD1306 final : public ssd1306_base::SSD1306, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  void update() override;
  void set_partial_updates() { this->partial_updates_enabled_ = true; }
#endif

 protected:
  void command(uint8_t value) override;
  void write_display_data() override;

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  struct Region {
    uint8_t min_x{0};
    uint8_t max_x{0};
    uint8_t min_page{0};
    uint8_t max_page{0};
  };

  // At most 16 planned regions/page across eight pages for a 128x64 display.
  static constexpr size_t MAX_PLANNED_REGIONS = 128;
  // At most 64 alternating changed runs in 128 columns.
  static constexpr size_t MAX_RAW_RUNS_PER_PAGE = 64;

  struct PartialUpdateState {
    std::unique_ptr<uint8_t[]> previous_buffer;
    std::array<Region, MAX_PLANNED_REGIONS> regions{};
    size_t region_count{0};
    bool have_previous_buffer{false};
    bool controller_reinit_required{false};
  };

  bool partial_updates_supported_() const;

  uint8_t get_column_address_base_() const;
  uint8_t get_com_pins_config_() const;
  uint8_t get_vcom_detect_() const;

  void mark_partial_transport_failure_();
  void commit_shadow_buffer_();

  bool reinitialize_controller_();

  bool write_checked_region_(const Region &region);
  bool write_checked_full_frame_();
  bool write_planned_regions_();

  bool build_region_plan_(Region &global_bounds, bool &has_changes);

  bool add_optimized_page_regions_(uint8_t page, const uint8_t *run_starts, const uint8_t *run_ends, size_t run_count);

  void optimize_region_merges_();

  Region merge_regions_(const Region &a, const Region &b) const;

  void remove_region_(size_t index);

  uint32_t estimate_region_wire_clocks_(const Region &region) const;
  uint32_t get_region_plan_wire_clocks_() const;

  bool partial_updates_enabled_{false};
  std::unique_ptr<PartialUpdateState> partial_update_state_;
#endif

  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};
};

}  // namespace esphome::ssd1306_i2c
