#pragma once

#include <cstddef>
#include <string>

#include "esphome/components/display/display.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "it8951_defs.h"

namespace esphome::it8951 {

using namespace display;

// --- Bounded op queue --------------------------------------------------------
// Fixed-capacity ring buffer used by the loop scheduler. Replaces std::deque
// to comply with ESPHome's STL container guidelines (std::deque allocates in
// 512-byte blocks regardless of element size). Size analysis: the deepest
// observed scenario is UPDATE_REFRESH (10 enqueued ops) + CHECK_LUT_IDLE's
// 5 push_front rescheduling = 14 simultaneous entries. We use 32 for a
// comfortable margin while keeping RAM cost low (~192 bytes per instance vs
// 512+ bytes for std::deque).
template<typename T, size_t N> class StaticOpQueue {
 public:
  bool empty() const { return this->count_ == 0; }
  size_t size() const { return this->count_; }
  static constexpr size_t capacity() { return N; }

  bool push_back(const T &value) {
    if (this->count_ >= N)
      return false;
    this->data_[(this->head_ + this->count_) % N] = value;
    this->count_++;
    return true;
  }

  bool push_front(const T &value) {
    if (this->count_ >= N)
      return false;
    this->head_ = (this->head_ + N - 1) % N;
    this->data_[this->head_] = value;
    this->count_++;
    return true;
  }

  void pop_front() {
    if (this->count_ == 0)
      return;
    this->head_ = (this->head_ + 1) % N;
    this->count_--;
  }

  const T &front() const { return this->data_[this->head_]; }
  T &front() { return this->data_[this->head_]; }

  void clear() {
    this->head_ = 0;
    this->count_ = 0;
  }

 private:
  T data_[N]{};
  size_t head_{0};
  size_t count_{0};
};

// Op queue capacity. See StaticOpQueue comment for sizing analysis.
static constexpr size_t OP_QUEUE_SIZE = 32;

// --- Op queue ---------------------------------------------------------------
// Each Op is a single CS-asserted SPI transaction (or a tiny bookkeeping
// step). The loop processes one Op per iteration after gating on HRDY, so
// the natural ESPHome loop cadence (~8-16 ms) provides inter-op pacing
// without any blocking waits.
//
// Compound Ops (READ_DEV_INFO, XFER_*, DPY_BUF_AREA, ENABLE_1BPP, ...) are
// short self-contained methods that do all their SPI work inside a single
// CS cycle (or a small handful of cycles) and complete well under 2ms, so
// they don't break the no-blocking budget.
//
// Each write-type op is a SINGLE CS-asserted transaction. The loop-level
// HRDY gate ensures the controller is ready before dispatching any op, so
// no blocking waits are needed within write ops.
//
// Read ops are decomposed: the command/address that triggers data preparation
// is sent as write ops (CMD, WRITE_W), then a separate read op runs only
// after the loop confirms HRDY is back HIGH (data ready). No blocking.
enum class OpType : uint8_t {
  CMD,              // single CS: CMD preamble + command word (a)
  WRITE_W,          // single CS: WRITE preamble + data word (a)
  WRITE_REG,        // single CS: WRITE preamble + addr(a) + value(b)
                    //   (caller must enqueue CMD(TCON_REG_WR) before this)
  READ_DEV_INFO,    // single CS: READ preamble + dummy + read DevInfo struct
                    //   (caller enqueues CMD(GET_DEV_INFO) first; loop HRDY gate
                    //    ensures data is ready before this op runs)
  READ_WORD,        // single CS: READ preamble + dummy + read one 16-bit word
                    //   into read_result_. Loop HRDY gate ensures data ready.
  CHECK_LUT_IDLE,   // checks read_result_; if non-zero, re-enqueues read sequence
  SET_1BPP,         // uses read_result_ to set UP1SR bit 2, enqueues writes
  CLEAR_1BPP,       // uses read_result_ to clear UP1SR bit 2, enqueues writes
  XFER_LISAR,       // set image-buffer target address (2× reg write: 4 CS transactions)
  XFER_AREA_CMD,    // single CS: CMD preamble + TCON_LD_IMG_AREA
  XFER_AREA_ARGS,   // single CS: WRITE preamble + 5 area-parameter words
  XFER_ROWS,        // single CS: WRITE preamble + row pixel data (time-sliced)
  XFER_AREA_END,    // single CS: CMD preamble + TCON_LD_IMG_END
  DPY_BUF_CMD,      // single CS: CMD preamble + I80_CMD_DPY_BUF_AREA
  DPY_BUF_ARGS,     // single CS: WRITE preamble + 7 display-area words
  GPIO_RESET_LOW,   // drive RESET pin low
  GPIO_RESET_HIGH,  // drive RESET pin high
  DELAY_MS,         // park `delay_until_` for a few ms (no SPI)
};

struct Op {
  OpType type;
  uint16_t a{0};
  uint16_t b{0};
};

// High-level controller phases. Each phase enqueues a sequence of Ops; when
// the queue drains and a PHASE_DONE Op is reached, advance_phase_() runs the
// next phase. This separation keeps per-Op work tiny and predictable.
enum class Phase : uint8_t {
  IDLE,
  // Initialisation
  INIT_RESET,     // reset pulse + wake controller + packed-write enable
  INIT_DEV_INFO,  // GET_DEV_INFO and validate
  INIT_VCOM,      // write configured VCOM
  INIT_TEMP,      // force temperature for waveform LUT selection
  INIT_DONE,      // allocate framebuffer; transition to IDLE
  // Update flow
  UPDATE_PREPARE,   // do_update_, compute dirty region, decide 4bpp/1bpp
  UPDATE_TRANSFER,  // chunked LD_IMG_AREA / LD_IMG_END loop
  UPDATE_REFRESH,   // wait LUT idle, optionally enable 1bpp, send DPY_BUF_AREA
  UPDATE_RESTORE,   // wait LUT idle, clear UP1SR bit 2 (1bpp only)
  UPDATE_SLEEP,     // optional deep sleep
};

class IT8951Display : public Display,
                      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                            spi::DATA_RATE_2MHZ> {
 public:
  IT8951Display(const char *name, uint16_t width, uint16_t height) : name_(name), width_(width), height_(height) {
    this->row_width_ = static_cast<uint16_t>((static_cast<uint32_t>(width) + 1) / 2);
    this->buffer_length_ = static_cast<size_t>(this->row_width_) * static_cast<size_t>(height);
  }

  // --- Component lifecycle ---
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_safe_shutdown() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // --- Config setters (called from generated code) ---
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_busy_pin(GPIOPin *pin) { this->busy_pin_ = pin; }
  void set_reset_duration(uint32_t ms) { this->reset_duration_ = ms; }
  void set_full_update_every(uint8_t n) { this->full_update_every_ = n; }
  void set_reversed(bool reversed) { this->reversed_ = reversed; }
  void set_sleep_when_done(bool s) { this->sleep_when_done_ = s; }
  void set_vcom(uint16_t vcom_mv) { this->vcom_ = vcom_mv; }
  void set_vcom_register(uint16_t selector) { this->vcom_register_ = selector; }
  void set_force_temperature(int16_t celsius) {
    this->force_temperature_ = celsius;
    this->force_temperature_set_ = true;
  }
  void set_use_legacy_dpy_area(bool use) { this->use_legacy_dpy_area_ = use; }
  void set_force_1bpp(bool f) { this->force_1bpp_ = f; }
  void set_update_mode(uint16_t m) { this->default_update_mode_ = static_cast<UpdateMode>(m); }
  void set_transform(uint8_t t) {
    this->transform_ = t;
    this->update_effective_transform_();
  }
  void set_rotation(DisplayRotation rotation) override {
    Display::set_rotation(rotation);
    this->update_effective_transform_();
  }

  // --- Display API ---
  void update() override;
  void update_mode(const std::string &mode);
  DisplayType get_display_type() override { return DISPLAY_TYPE_GRAYSCALE; }
  void fill(Color color) override;
  void clear() override { this->fill(COLOR_OFF); }
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  int get_height_internal() override { return this->height_; }
  int get_width_internal() override { return this->width_; }
  int get_width() override { return (this->effective_transform_ & TRANSFORM_SWAP_XY) ? this->height_ : this->width_; }
  int get_height() override { return (this->effective_transform_ & TRANSFORM_SWAP_XY) ? this->width_ : this->height_; }

  // --- Coord transform / dirty region ---
  void update_effective_transform_();
  bool rotate_coordinates_(int &x, int &y);
  void reset_dirty_region_();

  // --- Color helpers ---
  uint8_t color_to_nibble_(const Color &color) const;

  // --- 1bpp helpers ---
  uint8_t get_pixel_nibble_(uint16_t x, uint16_t y);

  // --- Op queue / loop machinery ---
  void enqueue_(OpType type, uint16_t a = 0, uint16_t b = 0);
  bool busy_pin_low_() const;
  void process_op_(const Op &op);
  void advance_phase_();
  void set_phase_(Phase next);
  void start_update_(UpdateMode mode);

  // --- SPI primitives (each is one CS-asserted burst, fully non-blocking) ---
  void spi_cmd_(uint16_t cmd);
  void spi_write_word_(uint16_t value);
  void spi_write_reg_(uint16_t addr, uint16_t value);
  void spi_write_args_(const uint16_t *args, uint16_t count);
  uint16_t spi_read_word_();  // non-blocking: HRDY confirmed by loop gate
  void spi_read_dev_info_();  // non-blocking: HRDY confirmed by loop gate

  // --- Compound Ops (small bounded helpers) ---
  void op_xfer_lisar_();
  void op_xfer_area_args_();
  void op_xfer_area_end_();
  bool op_xfer_rows_();  // returns true when current update area fully sent
  void op_dpy_buf_args_();
  void op_check_lut_idle_();
  void op_set_1bpp_();
  void op_clear_1bpp_();

  // --- Phase enqueuers ---
  void enqueue_init_reset_();
  void enqueue_init_dev_info_();
  void enqueue_init_vcom_();
  void enqueue_init_temp_();
  void enqueue_update_transfer_();
  void enqueue_update_refresh_();
  void enqueue_update_restore_();
  void enqueue_update_sleep_();

  bool prepare_update_region_(UpdateMode &mode);

  // --- Recovery ---
  void recover_();

  // --- State ---
  static constexpr uint32_t BUSY_TIMEOUT_MS = 5000;

  StaticOpQueue<Op, OP_QUEUE_SIZE> queue_;
  Phase phase_{Phase::IDLE};
  uint32_t delay_until_{0};
  uint32_t phase_started_at_{0};

  // Pending update bookkeeping
  bool update_pending_{false};
  UpdateMode pending_update_mode_{UPDATE_MODE_NONE};
  UpdateMode active_mode_{UPDATE_MODE_NONE};
  uint16_t area_x_{0}, area_y_{0}, area_w_{0}, area_h_{0};
  uint16_t transfer_row_{0};
  bool use_1bpp_{false};
  bool has_grayscale_{false};
  bool initialised_{false};
  uint32_t partial_update_count_{0};
  uint32_t update_started_at_{0};

  // Read result storage for decomposed read-modify-write op sequences
  uint16_t read_result_{0};

  // Device info
  DevInfo dev_info_{};
  uint16_t img_buf_addr_l_{0};
  uint16_t img_buf_addr_h_{0};

  // Configured properties
  const char *name_;
  uint16_t width_;
  uint16_t height_;
  uint16_t row_width_;
  size_t buffer_length_{};
  split_buffer::SplitBuffer buffer_{};
  uint8_t transform_{0};
  uint8_t effective_transform_{0};
  uint8_t full_update_every_{1};
  uint32_t reset_duration_{10};
  uint16_t vcom_{2300};
  uint16_t vcom_register_{I80_CMD_VCOM_WRITE};
  int16_t force_temperature_{DEFAULT_FORCE_TEMP_C};
  bool force_temperature_set_{false};
  bool use_legacy_dpy_area_{false};
  bool reversed_{false};
  bool sleep_when_done_{false};
  bool force_1bpp_{false};
  UpdateMode default_update_mode_{UPDATE_MODE_NONE};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};

  // Dirty region (pixel coordinates of bounding box of changes since last update)
  uint16_t x_low_{0}, y_low_{0}, x_high_{0}, y_high_{0};

  // Saved data rate so we can probe slow then run fast
  uint32_t configured_data_rate_{0};

  // Consecutive recovery attempts; used to give up rather than infinite-loop
  // when the controller is unresponsive (e.g. wiring issue).
  uint8_t recovery_attempts_{0};

  // DevInfo read retry counter (controller often returns garbage on the first
  // read after reset; the original driver retried up to 3 times with 100ms
  // between attempts).
  uint8_t dev_info_attempts_{0};
};

// --- Automation action ---
template<typename... Ts> class IT8951UpdateAction : public Action<Ts...> {
 public:
  explicit IT8951UpdateAction(IT8951Display *display) : display_(display) {}
  TEMPLATABLE_VALUE(std::string, mode)

  void play(const Ts &...x) override {
    if (!this->display_->is_ready())
      return;
    if (this->mode_.has_value()) {
      this->display_->update_mode(this->mode_.value(x...));
    } else {
      this->display_->update();
    }
  }

 protected:
  IT8951Display *display_;
};

}  // namespace esphome::it8951
