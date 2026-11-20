#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::ssd1306_i2c {

struct Region {
  uint8_t min_x;
  uint8_t max_x;
  uint8_t min_page;
  uint8_t max_page;
};

// Allocation-free planning over two page-major framebuffers. No device or bus state is used.
class PartialUpdatePlanner {
 public:
  static constexpr size_t MAX_PLANNED_REGIONS = 128;
  static constexpr size_t MAX_RAW_RUNS_PER_PAGE = 64;
  static constexpr uint32_t DATA_CHUNK_SIZE = 16;
  // A gap of at most six bytes cannot make splitting cheaper, including chunk rounding.
  static constexpr uint32_t MAX_ALWAYS_MERGE_GAP = 6;

  // False means invalid inputs. Workspace exhaustion instead returns one complete bounding region.
  // capacity limits the planning workspace (also used to exercise fallback in tests), not output storage.
  bool build(const uint8_t *current, const uint8_t *previous, uint16_t width, uint8_t pages,
             size_t capacity = MAX_PLANNED_REGIONS);
  size_t get_region_count() const { return this->region_count_; }
  const Region &get_region(size_t index) const { return this->regions_[index]; }
  bool used_fallback() const { return this->used_fallback_; }
  uint32_t get_wire_clocks() const;
  static uint32_t estimate_region_wire_clocks(const Region &region);

 protected:
  bool add_optimized_page_regions_(uint8_t page, const uint8_t *run_starts, const uint8_t *run_ends, size_t run_count);
  void optimize_region_merges_();
  Region merge_regions_(const Region &a, const Region &b) const;
  void remove_region_(size_t index);

  std::array<Region, MAX_PLANNED_REGIONS> regions_;
  size_t region_count_{0};
  size_t capacity_{MAX_PLANNED_REGIONS};
  bool used_fallback_{false};
};

}  // namespace esphome::ssd1306_i2c
#endif
