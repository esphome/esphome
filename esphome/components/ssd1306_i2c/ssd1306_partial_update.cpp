#include "ssd1306_partial_update.h"

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
#include <algorithm>

namespace esphome::ssd1306_i2c {

// Address and control bytes cost 18 clocks; a six-byte address window costs 72.
static constexpr uint32_t I2C_TRANSACTION_FIXED_CLOCKS = 18;
static constexpr uint32_t I2C_PAYLOAD_BYTE_CLOCKS = 9;
static constexpr uint32_t SSD1306_WINDOW_COMMAND_CLOCKS = 72;
static constexpr size_t MAX_GENERAL_MERGE_REGIONS = 32;

uint32_t PartialUpdatePlanner::estimate_region_wire_clocks(const Region &region) {
  const uint32_t region_width = static_cast<uint32_t>(region.max_x - region.min_x + 1);
  const uint32_t region_pages = static_cast<uint32_t>(region.max_page - region.min_page + 1);
  const uint32_t payload_bytes = region_width * region_pages;
  const uint32_t data_transactions = (payload_bytes + DATA_CHUNK_SIZE - 1) / DATA_CHUNK_SIZE;
  return SSD1306_WINDOW_COMMAND_CLOCKS + (data_transactions * I2C_TRANSACTION_FIXED_CLOCKS) +
         (payload_bytes * I2C_PAYLOAD_BYTE_CLOCKS);
}

uint32_t PartialUpdatePlanner::get_wire_clocks() const {
  uint32_t total = 0;
  for (size_t i = 0; i < this->region_count_; i++) {
    total += PartialUpdatePlanner::estimate_region_wire_clocks(this->regions_[i]);
  }
  return total;
}

bool PartialUpdatePlanner::add_optimized_page_regions_(uint8_t page, const uint8_t *run_starts, const uint8_t *run_ends,
                                                       size_t run_count) {
  if (run_count == 0) {
    return true;
  }
  bool all_gaps_always_merge = true;
  for (size_t i = 1; i < run_count; i++) {
    const uint32_t gap = static_cast<uint32_t>(run_starts[i] - run_ends[i - 1] - 1);
    if (gap > MAX_ALWAYS_MERGE_GAP) {
      all_gaps_always_merge = false;
      break;
    }
  }

  // Dense-page fast path.
  if (all_gaps_always_merge) {
    if (this->region_count_ >= this->capacity_) {
      return false;
    }
    Region region;
    region.min_x = run_starts[0];
    region.max_x = run_ends[run_count - 1];
    region.min_page = page;
    region.max_page = page;
    this->regions_[this->region_count_++] = region;
    return true;
  }

  // best_cost[n] is the cheapest way to cover the first n changed runs.
  uint32_t best_cost[MAX_RAW_RUNS_PER_PAGE + 1];
  uint8_t best_previous[MAX_RAW_RUNS_PER_PAGE + 1];
  uint8_t best_region_count[MAX_RAW_RUNS_PER_PAGE + 1];
  best_cost[0] = 0;
  best_previous[0] = 0;
  best_region_count[0] = 0;
  for (size_t end = 1; end <= run_count; end++) {
    best_cost[end] = 0xFFFFFFFFUL;
    best_previous[end] = 0;
    best_region_count[end] = 0xFF;
    for (size_t start = 0; start < end; start++) {
      Region candidate;
      candidate.min_x = run_starts[start];
      candidate.max_x = run_ends[end - 1];
      candidate.min_page = page;
      candidate.max_page = page;
      const uint32_t candidate_cost = best_cost[start] + PartialUpdatePlanner::estimate_region_wire_clocks(candidate);
      const uint8_t candidate_regions = static_cast<uint8_t>(best_region_count[start] + 1);
      if (candidate_cost < best_cost[end] ||
          (candidate_cost == best_cost[end] && candidate_regions < best_region_count[end])) {
        best_cost[end] = candidate_cost;
        best_previous[end] = static_cast<uint8_t>(start);
        best_region_count[end] = candidate_regions;
      }
    }
  }
  Region reverse_regions[MAX_RAW_RUNS_PER_PAGE];
  size_t reverse_count = 0;
  size_t end = run_count;
  while (end > 0) {
    const size_t start = best_previous[end];
    Region region;
    region.min_x = run_starts[start];
    region.max_x = run_ends[end - 1];
    region.min_page = page;
    region.max_page = page;
    reverse_regions[reverse_count++] = region;
    end = start;
  }
  if (this->region_count_ + reverse_count > this->capacity_) {
    return false;
  }
  while (reverse_count > 0) {
    reverse_count--;
    this->regions_[this->region_count_++] = reverse_regions[reverse_count];
  }
  return true;
}

bool PartialUpdatePlanner::build(const uint8_t *current, const uint8_t *previous, uint16_t width, uint8_t pages,
                                 size_t capacity) {
  this->region_count_ = 0;
  this->used_fallback_ = false;
  this->capacity_ = std::min(capacity, MAX_PLANNED_REGIONS);
  if (current == nullptr || previous == nullptr || width == 0 || width > 128 || pages == 0 || pages > 8) {
    return false;
  }
  Region global_bounds{};
  bool has_changes = false;

  // Keep scanning after workspace exhaustion: the fallback must cover every changed byte.
  bool plan_buildable = true;
  for (uint8_t page = 0; page < pages; page++) {
    uint8_t run_starts[MAX_RAW_RUNS_PER_PAGE];
    uint8_t run_ends[MAX_RAW_RUNS_PER_PAGE];
    size_t run_count = 0;
    bool in_run = false;
    uint8_t current_run_start = 0;
    const size_t page_offset = static_cast<size_t>(page) * width;
    for (uint16_t x = 0; x < width; x++) {
      const size_t index = page_offset + x;
      const bool changed = current[index] != previous[index];
      if (changed) {
        if (!has_changes) {
          has_changes = true;
          global_bounds.min_x = static_cast<uint8_t>(x);
          global_bounds.max_x = static_cast<uint8_t>(x);
          global_bounds.min_page = page;
          global_bounds.max_page = page;
        } else {
          global_bounds.min_x = std::min<uint8_t>(global_bounds.min_x, static_cast<uint8_t>(x));
          global_bounds.max_x = std::max<uint8_t>(global_bounds.max_x, static_cast<uint8_t>(x));
          global_bounds.min_page = std::min<uint8_t>(global_bounds.min_page, page);
          global_bounds.max_page = std::max<uint8_t>(global_bounds.max_page, page);
        }
      }

      // Complete the fallback bounds even after planning has become unavailable.
      if (!plan_buildable) {
        continue;
      }
      if (changed) {
        if (!in_run) {
          current_run_start = static_cast<uint8_t>(x);
          in_run = true;
        }
      } else if (in_run) {
        if (run_count >= MAX_RAW_RUNS_PER_PAGE) {
          plan_buildable = false;
          in_run = false;
          continue;
        }
        run_starts[run_count] = current_run_start;
        run_ends[run_count] = static_cast<uint8_t>(x - 1);
        run_count++;
        in_run = false;
      }
    }
    if (plan_buildable && in_run) {
      if (run_count >= MAX_RAW_RUNS_PER_PAGE) {
        plan_buildable = false;
      } else {
        run_starts[run_count] = current_run_start;
        run_ends[run_count] = static_cast<uint8_t>(width - 1);
        run_count++;
      }
    }
    if (plan_buildable && !this->add_optimized_page_regions_(page, run_starts, run_ends, run_count)) {
      // Later pages may extend the fallback rectangle.
      plan_buildable = false;
    }
  }
  if (!has_changes) {
    return true;
  }
  if (!plan_buildable) {
    this->regions_[0] = global_bounds;
    this->region_count_ = 1;
    this->used_fallback_ = true;
    return true;
  }
  this->optimize_region_merges_();
  if (this->get_wire_clocks() >= PartialUpdatePlanner::estimate_region_wire_clocks(global_bounds)) {
    this->regions_[0] = global_bounds;
    this->region_count_ = 1;
  }
  return true;
}

Region PartialUpdatePlanner::merge_regions_(const Region &a, const Region &b) const {
  Region merged;
  merged.min_x = std::min<uint8_t>(a.min_x, b.min_x);
  merged.max_x = std::max<uint8_t>(a.max_x, b.max_x);
  merged.min_page = std::min<uint8_t>(a.min_page, b.min_page);
  merged.max_page = std::max<uint8_t>(a.max_page, b.max_page);
  return merged;
}

void PartialUpdatePlanner::remove_region_(size_t index) {
  if (index >= this->region_count_) {
    return;
  }
  for (size_t i = index + 1; i < this->region_count_; i++) {
    this->regions_[i - 1] = this->regions_[i];
  }
  this->region_count_--;
}

void PartialUpdatePlanner::optimize_region_merges_() {
  // Compact repeated horizontal spans before the more expensive all-pairs search.
  size_t i = 0;
  while (i < this->region_count_) {
    size_t j = i + 1;
    while (j < this->region_count_) {
      const Region &a = this->regions_[i];
      const Region &b = this->regions_[j];
      if (a.min_x != b.min_x || a.max_x != b.max_x) {
        j++;
        continue;
      }
      const uint32_t separate_cost =
          PartialUpdatePlanner::estimate_region_wire_clocks(a) + PartialUpdatePlanner::estimate_region_wire_clocks(b);
      const Region merged = this->merge_regions_(a, b);
      if (PartialUpdatePlanner::estimate_region_wire_clocks(merged) > separate_cost) {
        j++;
        continue;
      }
      this->regions_[i] = merged;
      this->remove_region_(j);
    }
    i++;
  }

  // Bound the all-pairs search for pathological sparse patterns.
  if (this->region_count_ > MAX_GENERAL_MERGE_REGIONS) {
    return;
  }
  while (this->region_count_ > 1) {
    bool found_merge = false;
    size_t best_i = 0;
    size_t best_j = 0;
    uint32_t best_savings = 0;
    Region best_region;
    for (size_t first = 0; first < this->region_count_; first++) {
      const uint32_t first_cost = PartialUpdatePlanner::estimate_region_wire_clocks(this->regions_[first]);
      for (size_t second = first + 1; second < this->region_count_; second++) {
        const uint32_t separate_cost =
            first_cost + PartialUpdatePlanner::estimate_region_wire_clocks(this->regions_[second]);
        const Region merged = this->merge_regions_(this->regions_[first], this->regions_[second]);
        const uint32_t merged_cost = PartialUpdatePlanner::estimate_region_wire_clocks(merged);
        if (merged_cost > separate_cost) {
          continue;
        }
        const uint32_t savings = separate_cost - merged_cost;
        if (!found_merge || savings > best_savings) {
          found_merge = true;
          best_i = first;
          best_j = second;
          best_savings = savings;
          best_region = merged;
        }
      }
    }
    if (!found_merge) {
      break;
    }
    this->regions_[best_i] = best_region;
    this->remove_region_(best_j);
  }
}

}  // namespace esphome::ssd1306_i2c
#endif
