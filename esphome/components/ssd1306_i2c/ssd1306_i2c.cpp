#include "ssd1306_i2c.h"

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
#include <algorithm>
#include <cstring>
#endif

#include "esphome/core/log.h"

namespace esphome::ssd1306_i2c {

static const char *const TAG = "ssd1306_i2c";

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
// Partial-update transport cost model
//
// write_register():
//
//   I2C address + ACK        9 clocks
//   control/register + ACK   9 clocks
//
// Fixed transaction cost:
//
//   18 clocks
//
// Each payload byte:
//
//   9 clocks
//
// SSD1306 address window:
//
//   0x21
//   start column
//   end column
//   0x22
//   start page
//   end page
//
// = 6 payload bytes
//
// Window transaction:
//
//   18 + 6 * 9 = 72 clocks

static constexpr uint32_t I2C_TRANSACTION_FIXED_CLOCKS = 18;

static constexpr uint32_t I2C_PAYLOAD_BYTE_CLOCKS = 9;

static constexpr uint32_t SSD1306_WINDOW_COMMAND_CLOCKS = I2C_TRANSACTION_FIXED_CLOCKS + (6 * I2C_PAYLOAD_BYTE_CLOCKS);

static constexpr uint32_t PARTIAL_DATA_CHUNK_SIZE = 16;

// Splitting across a horizontal unchanged gap adds
// another 72-clock address-window transaction.
//
// For a gap shorter than one 16-byte data chunk,
// splitting can save at most one 18-clock data
// transaction.
//
// Therefore:
//
//   gap * 9 + 18 <= 72
//
// is never cheaper than retaining one region.
//
// gap <= 6 bytes.
//
// A six-byte gap can at best tie, so one region is
// preferred because it requires less driver work.

static constexpr uint32_t MAX_ALWAYS_MERGE_GAP =
    (SSD1306_WINDOW_COMMAND_CLOCKS - I2C_TRANSACTION_FIXED_CLOCKS) / I2C_PAYLOAD_BYTE_CLOCKS;

// Arbitrary two-dimensional all-pairs merging is
// deliberately bounded.
//
// Large repeated patterns are first compacted using
// the inexpensive identical-X-span vertical pass.

static constexpr size_t MAX_GENERAL_MERGE_REGIONS = 32;
#endif

// SETUP

void I2CSSD1306::setup() {
  this->init_reset_();

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (this->partial_updates_enabled_ && !this->partial_updates_supported_()) {
    ESP_LOGE(TAG, "partial_updates is supported only for SSD1306 models");
    this->mark_failed();
    return;
  }
#endif

  SSD1306::setup();

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (!this->partial_updates_enabled_) {
    return;
  }

  this->partial_update_state_ = std::make_unique<PartialUpdateState>();
  this->partial_update_state_->previous_buffer = std::make_unique<uint8_t[]>(this->get_buffer_length_());

  std::memset(this->partial_update_state_->previous_buffer.get(), 0, this->get_buffer_length_());

  this->partial_update_state_->have_previous_buffer = false;
  this->partial_update_state_->controller_reinit_required = false;
#endif
}

// CONFIG DUMP

void I2CSSD1306::dump_config() {
  LOG_DISPLAY("", "I2C SSD1306", this);

  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  External VCC: %s\n"
                "  Flip X: %s\n"
                "  Flip Y: %s\n"
                "  Offset X: %d\n"
                "  Offset Y: %d\n"
                "  Inverted Color: %s",
                LOG_STR_ARG(this->model_str_()), YESNO(this->external_vcc_), YESNO(this->flip_x_), YESNO(this->flip_y_),
                this->offset_x_, this->offset_y_, YESNO(this->invert_));

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  ESP_LOGCONFIG(TAG, "  Partial Updates: %s", YESNO(this->partial_updates_enabled_));
#endif

  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

// NATIVE FULL-FRAME TRANSPORT
//
// This remains the ordinary ESPHome transport.
//
// It is intentionally used unchanged whenever
// partial_updates is disabled.
//
// It also remains the setup-time write path.

void I2CSSD1306::command(uint8_t value) {
#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
  if (this->partial_updates_enabled_) {
    if (this->write_byte(0x00, value) != i2c::ERROR_OK) {
      this->mark_partial_transport_failure_();
    }
    return;
  }
#endif

  this->write_byte(0x00, value);
}

void HOT I2CSSD1306::write_display_data() {
  if (this->is_sh1106_() || this->is_sh1107_()) {
    uint32_t i = 0;
    // Some panels wire their visible columns to a window of the controller RAM
    // that does not start at column 0 (e.g. SH1107 M5Stack Unit OLED needs offset_x: 32).
    // SH1106 keeps its historical 0x02 base column on top of any offset.
    uint8_t start_column = this->offset_x_;
    if (this->is_sh1106_()) {
      start_column += 0x02;
    }
    for (uint8_t page = 0; page < (uint8_t) this->get_height_internal() / 8; page++) {
      this->command(0xB0 + page);                 // row
      this->command(start_column & 0x0F);         // lower column
      this->command(0x10 | (start_column >> 4));  // higher column
      for (uint8_t x = 0; x < (uint8_t) this->get_width_internal() / 16; x++) {
        uint8_t data[16];
        for (uint8_t &j : data)
          j = this->buffer_[i++];
        this->write_bytes(0x40, data, sizeof(data));
      }
    }
  } else {
    size_t block_size = 16;
    if ((this->get_buffer_length_() % 24) == 0) {
      // use 24 byte block size for e.g. 72x40 displays where buffer size is multiple of 24, not 16
      block_size = 24;
    }

    for (uint32_t i = 0; i < this->get_buffer_length_();) {
      uint8_t data[block_size];
      for (uint8_t &j : data)
        j = this->buffer_[i++];
      this->write_bytes(0x40, data, sizeof(data));
    }
  }
}

#ifdef USE_SSD1306_I2C_PARTIAL_UPDATES
// MODEL HELPERS

bool I2CSSD1306::partial_updates_supported_() const {
  switch (this->model_) {
    case ssd1306_base::SSD1306_MODEL_128_32:
    case ssd1306_base::SSD1306_MODEL_128_64:
    case ssd1306_base::SSD1306_MODEL_96_16:
    case ssd1306_base::SSD1306_MODEL_64_48:
    case ssd1306_base::SSD1306_MODEL_64_32:
    case ssd1306_base::SSD1306_MODEL_72_40:
      return true;

    default:
      return false;
  }
}

uint8_t I2CSSD1306::get_column_address_base_() const {
  switch (this->model_) {
    case ssd1306_base::SSD1306_MODEL_64_48:
    case ssd1306_base::SSD1306_MODEL_64_32:
      return 0x20;

    case ssd1306_base::SSD1306_MODEL_72_40:
      return 0x1C;

    default:
      return 0x00;
  }
}

uint8_t I2CSSD1306::get_com_pins_config_() const {
  switch (this->model_) {
    case ssd1306_base::SSD1306_MODEL_128_32:
    case ssd1306_base::SSD1306_MODEL_96_16:
      return 0x02;

    default:
      return 0x12;
  }
}

uint8_t I2CSSD1306::get_vcom_detect_() const {
  if (this->model_ == ssd1306_base::SSD1306_MODEL_72_40) {
    return 0x20;
  }

  return 0x00;
}

// SHADOW / FAILURE STATE

void I2CSSD1306::mark_partial_transport_failure_() {
  if (this->partial_update_state_ == nullptr) {
    return;
  }

  if (!this->partial_update_state_->controller_reinit_required) {
    ESP_LOGW(TAG, "SSD1306 I2C update failed; "
                  "controller reinitialization required");
  }

  this->partial_update_state_->controller_reinit_required = true;

  this->partial_update_state_->have_previous_buffer = false;
}

void I2CSSD1306::commit_shadow_buffer_() {
  std::memcpy(this->partial_update_state_->previous_buffer.get(), this->buffer_, this->get_buffer_length_());

  this->partial_update_state_->have_previous_buffer = true;
}

// CHECKED CONTROLLER REINITIALIZATION
//
// This mirrors the SSD1306-specific portion of
// SSD1306::setup().
//
// It deliberately excludes SH1106, SH1107 and
// SSD1305 because partial updates cannot be enabled
// for those models.

bool I2CSSD1306::reinitialize_controller_() {
  this->init_reset_();

  uint8_t commands[32];

  size_t command_count = 0;

  auto push = [&commands, &command_count](uint8_t value) { commands[command_count++] = value; };

  // Display OFF
  push(0xAE);

  // Oscillator / clock division
  push(0xD5);
  push(0x80);

  // Multiplex ratio
  push(0xA8);

  push(static_cast<uint8_t>(this->get_height_internal() - 1));

  // Display Y offset
  push(0xD3);
  push(this->offset_y_);

  // Start line 0
  push(0x40);

  // SSD1306B / 72x40 IREF
  if (this->model_ == ssd1306_base::SSD1306_MODEL_72_40) {
    push(0xAD);
    push(0x30);
  }

  // Charge pump
  push(0x8D);

  push(this->external_vcc_ ? 0x10 : 0x14);

  // Horizontal addressing mode
  push(0x20);
  push(0x00);

  // Segment remap
  push(static_cast<uint8_t>(0xA0 | this->flip_x_));

  // COM scan direction
  push(static_cast<uint8_t>(0xC0 | (this->flip_y_ << 3)));

  // COM pin configuration
  push(0xDA);
  push(this->get_com_pins_config_());

  // Pre-charge period
  push(0xD9);

  push(this->external_vcc_ ? 0x22 : 0xF1);

  // VCOM
  push(0xDB);
  push(this->get_vcom_detect_());

  // Display output follows RAM
  push(0xA4);

  // Normal / inverse
  push(static_cast<uint8_t>(0xA6 | this->invert_));

  // Disable scrolling
  push(0x2E);

  // Contrast
  push(0x81);

  push(static_cast<uint8_t>(255.0f * this->contrast_));

  const auto result = this->write_register(0x00, commands, command_count);

  return result == i2c::ERROR_OK;
}

// CHECKED REGION TRANSPORT

bool I2CSSD1306::write_checked_region_(const Region &region) {
  const uint16_t width = static_cast<uint16_t>(this->get_width_internal());

  const uint16_t column_base =
      static_cast<uint16_t>(this->get_column_address_base_()) + static_cast<uint16_t>(this->offset_x_);

  const uint8_t commands[6] = {0x21,

                               static_cast<uint8_t>(column_base + region.min_x),

                               static_cast<uint8_t>(column_base + region.max_x),

                               0x22,

                               region.min_page,

                               region.max_page};

  // Address window
  if (this->write_register(0x00, commands, sizeof(commands)) != i2c::ERROR_OK) {
    this->mark_partial_transport_failure_();

    return false;
  }

  // Rectangle data
  //
  // Horizontal SSD1306 addressing automatically
  // advances from the last column of one page to the
  // first column of the next page.
  uint8_t data[PARTIAL_DATA_CHUNK_SIZE];

  uint8_t chunk_size = 0;

  for (uint8_t page = region.min_page; page <= region.max_page; page++) {
    const size_t page_offset = static_cast<size_t>(page) * width;

    for (uint16_t x = region.min_x; x <= region.max_x; x++) {
      data[chunk_size++] = this->buffer_[page_offset + x];

      if (chunk_size == PARTIAL_DATA_CHUNK_SIZE) {
        if (this->write_register(0x40, data, chunk_size) != i2c::ERROR_OK) {
          this->mark_partial_transport_failure_();

          return false;
        }

        chunk_size = 0;
      }
    }
  }

  if (chunk_size > 0) {
    if (this->write_register(0x40, data, chunk_size) != i2c::ERROR_OK) {
      this->mark_partial_transport_failure_();

      return false;
    }
  }

  return true;
}

// FULL CHECKED SYNCHRONIZATION

bool I2CSSD1306::write_checked_full_frame_() {
  Region full;

  full.min_x = 0;

  full.max_x = static_cast<uint8_t>(this->get_width_internal() - 1);

  full.min_page = 0;

  full.max_page = static_cast<uint8_t>((this->get_height_internal() / 8) - 1);

  if (!this->write_checked_region_(full)) {
    return false;
  }

  this->commit_shadow_buffer_();

  return true;
}

// COST MODEL

uint32_t I2CSSD1306::estimate_region_wire_clocks_(const Region &region) const {
  const uint32_t region_width = static_cast<uint32_t>(region.max_x - region.min_x + 1);

  const uint32_t region_pages = static_cast<uint32_t>(region.max_page - region.min_page + 1);

  const uint32_t payload_bytes = region_width * region_pages;

  const uint32_t data_transactions = (payload_bytes + PARTIAL_DATA_CHUNK_SIZE - 1) / PARTIAL_DATA_CHUNK_SIZE;

  return SSD1306_WINDOW_COMMAND_CLOCKS + (data_transactions * I2C_TRANSACTION_FIXED_CLOCKS) +
         (payload_bytes * I2C_PAYLOAD_BYTE_CLOCKS);
}

uint32_t I2CSSD1306::get_region_plan_wire_clocks_() const {
  uint32_t total = 0;

  for (size_t i = 0; i < this->partial_update_state_->region_count; i++) {
    total += this->estimate_region_wire_clocks_(this->partial_update_state_->regions[i]);
  }

  return total;
}

// PAGE-LOCAL OPTIMIZATION

bool I2CSSD1306::add_optimized_page_regions_(uint8_t page, const uint8_t *run_starts, const uint8_t *run_ends,
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
    if (this->partial_update_state_->region_count >= MAX_PLANNED_REGIONS) {
      return false;
    }

    Region region;

    region.min_x = run_starts[0];

    region.max_x = run_ends[run_count - 1];

    region.min_page = page;

    region.max_page = page;

    this->partial_update_state_->regions[this->partial_update_state_->region_count++] = region;

    return true;
  }

  // Dynamic program over raw contiguous runs.
  //
  // best_cost[n] is the cheapest way to cover the
  // first n changed runs.
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

      const uint32_t candidate_cost = best_cost[start] + this->estimate_region_wire_clocks_(candidate);

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

  if (this->partial_update_state_->region_count + reverse_count > MAX_PLANNED_REGIONS) {
    return false;
  }

  while (reverse_count > 0) {
    reverse_count--;

    this->partial_update_state_->regions[this->partial_update_state_->region_count++] = reverse_regions[reverse_count];
  }

  return true;
}

// COMPLETE DIRTY SCAN / PLAN CREATION

bool I2CSSD1306::build_region_plan_(Region &global_bounds, bool &has_changes) {
  auto &state = *this->partial_update_state_;

  state.region_count = 0;

  global_bounds = Region{};
  has_changes = false;

  const uint16_t width = static_cast<uint16_t>(this->get_width_internal());

  const uint8_t pages = static_cast<uint8_t>(this->get_height_internal() / 8);

  // Planning is allowed to become unavailable, but
  // dirty-bound discovery must always continue to the
  // end of the framebuffer. The fallback rectangle is
  // safe only when global_bounds contains every
  // changed byte.
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

      const bool changed = this->buffer_[index] != state.previous_buffer[index];

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

      // Once the fixed planner workspace is no longer
      // usable, keep scanning solely to complete
      // global_bounds.
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
      // Do not return here. Later pages may extend the
      // fallback rectangle.
      plan_buildable = false;
    }
  }

  if (!has_changes) {
    return true;
  }

  if (!plan_buildable) {
    // A failed plan is never consumed by update().
    // Clearing it also makes that invariant explicit.
    state.region_count = 0;

    return false;
  }

  this->optimize_region_merges_();

  return true;
}

// TWO-DIMENSIONAL MERGING

I2CSSD1306::Region I2CSSD1306::merge_regions_(const Region &a, const Region &b) const {
  Region merged;

  merged.min_x = std::min<uint8_t>(a.min_x, b.min_x);

  merged.max_x = std::max<uint8_t>(a.max_x, b.max_x);

  merged.min_page = std::min<uint8_t>(a.min_page, b.min_page);

  merged.max_page = std::max<uint8_t>(a.max_page, b.max_page);

  return merged;
}

void I2CSSD1306::remove_region_(size_t index) {
  auto &state = *this->partial_update_state_;

  if (index >= state.region_count) {
    return;
  }

  for (size_t i = index + 1; i < state.region_count; i++) {
    state.regions[i - 1] = state.regions[i];
  }

  state.region_count--;
}

void I2CSSD1306::optimize_region_merges_() {
  auto &state = *this->partial_update_state_;

  // Stage 1:
  //
  // Quickly merge identical horizontal spans across
  // pages.
  //
  // This turns patterns such as 88 separate grid
  // points into a small set of vertical columns
  // before the more expensive arbitrary search.
  size_t i = 0;

  while (i < state.region_count) {
    size_t j = i + 1;

    while (j < state.region_count) {
      const Region &a = state.regions[i];

      const Region &b = state.regions[j];

      if (a.min_x != b.min_x || a.max_x != b.max_x) {
        j++;

        continue;
      }

      const uint32_t separate_cost = this->estimate_region_wire_clocks_(a) + this->estimate_region_wire_clocks_(b);

      const Region merged = this->merge_regions_(a, b);

      if (this->estimate_region_wire_clocks_(merged) > separate_cost) {
        j++;

        continue;
      }

      state.regions[i] = merged;

      this->remove_region_(j);
    }

    i++;
  }

  // Stage 2:
  //
  // Bound the arbitrary all-pairs search so planner
  // execution time cannot explode on pathological
  // sparse patterns.
  if (state.region_count > MAX_GENERAL_MERGE_REGIONS) {
    return;
  }

  while (state.region_count > 1) {
    bool found_merge = false;

    size_t best_i = 0;

    size_t best_j = 0;

    uint32_t best_savings = 0;

    Region best_region;

    for (size_t first = 0; first < state.region_count; first++) {
      const uint32_t first_cost = this->estimate_region_wire_clocks_(state.regions[first]);

      for (size_t second = first + 1; second < state.region_count; second++) {
        const uint32_t separate_cost = first_cost + this->estimate_region_wire_clocks_(state.regions[second]);

        const Region merged = this->merge_regions_(state.regions[first], state.regions[second]);

        const uint32_t merged_cost = this->estimate_region_wire_clocks_(merged);

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

    state.regions[best_i] = best_region;

    this->remove_region_(best_j);
  }
}

// PLAN TRANSPORT

bool I2CSSD1306::write_planned_regions_() {
  auto &state = *this->partial_update_state_;

  for (size_t i = 0; i < state.region_count; i++) {
    if (!this->write_checked_region_(state.regions[i])) {
      return false;
    }
  }

  this->commit_shadow_buffer_();

  return true;
}

// UPDATE

void I2CSSD1306::update() {
  // Feature disabled:
  //
  // preserve the original ESPHome code path exactly.
  if (!this->partial_updates_enabled_) {
    SSD1306::update();

    return;
  }

  auto &state = *this->partial_update_state_;

  // Render the newest desired framebuffer first.
  this->do_update_();

  // Recovery / initial checked synchronization
  if (!state.have_previous_buffer) {
    const bool recovering_controller = state.controller_reinit_required;

    if (recovering_controller) {
      if (!this->reinitialize_controller_()) {
        return;
      }
    }

    if (!this->write_checked_full_frame_()) {
      return;
    }

    if (recovering_controller) {
      if (this->is_on_) {
        const uint8_t display_on = 0xAF;
        if (this->write_register(0x00, &display_on, sizeof(display_on)) != i2c::ERROR_OK) {
          this->mark_partial_transport_failure_();
          return;
        }
      }

      state.controller_reinit_required = false;

      ESP_LOGI(TAG, "SSD1306 controller reinitialized and "
                    "framebuffer resynchronized");
    }

    return;
  }

  // Build the current cost-minimized partial plan.
  Region global_bounds;
  bool has_changes = false;

  const bool plan_valid = this->build_region_plan_(global_bounds, has_changes);

  // An address-only probe keeps controller trust meaningful even when
  // the framebuffer is static. If a sustained disconnect is observed,
  // the next successful update will reinitialize and fully resynchronize.
  if (!has_changes) {
    if (this->write(nullptr, 0) != i2c::ERROR_OK) {
      this->mark_partial_transport_failure_();
    }
    return;
  }

  // Compare the optimized multi-region plan against
  // one global bounding rectangle.
  //
  // If planning overflowed its fixed workspace, or a
  // single rectangle is equally cheap/cheaper, use
  // the single rectangle.
  bool use_region_plan = false;

  if (plan_valid) {
    const uint32_t plan_cost = this->get_region_plan_wire_clocks_();

    const uint32_t global_cost = this->estimate_region_wire_clocks_(global_bounds);

    use_region_plan = plan_cost < global_cost;
  }

  bool success;

  if (use_region_plan) {
    success = this->write_planned_regions_();

  } else {
    success = this->write_checked_region_(global_bounds);

    if (success) {
      this->commit_shadow_buffer_();
    }
  }

  if (!success) {
    // write_checked_region_() already invalidated the
    // shadow and marked the controller untrusted.
    return;
  }
}
#endif

}  // namespace esphome::ssd1306_i2c
