#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "esphome/components/display/display.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

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
    ++this->count_;
    return true;
  }

  bool push_front(const T &value) {
    if (this->count_ >= N)
      return false;
    this->head_ = (this->head_ + N - 1) % N;
    this->data_[this->head_] = value;
    ++this->count_;
    return true;
  }

  void pop_front() {
    if (this->count_ == 0)
      return;
    this->head_ = (this->head_ + 1) % N;
    --this->count_;
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
// step). The loop processes one Op per iteration after gating on HW_RDY, so
// the natural ESPHome loop cadence (~8-16 ms) provides inter-op pacing
// without any blocking waits.
//
// Compound Ops (READ_DEV_INFO, XFER_*, DPY_BUF_AREA, ENABLE_1BPP, ...) are
// short self-contained methods that do all their SPI work inside a single
// CS cycle (or a small handful of cycles) and complete well under 2ms, so
// they don't break the no-blocking budget.
//
// Each write-type op is a SINGLE CS-asserted transaction. The loop-level
// HW_RDY gate ensures the controller is ready before dispatching any op, so
// no blocking waits are needed within write ops.
//
// Read ops are decomposed: the command/address that triggers data preparation
// is sent as write ops (CMD, WRITE_W), then a separate read op runs only
// after the loop confirms HW_RDY is back HIGH (data ready). No blocking.
enum class OpType : uint8_t {
  CMD,              // single CS: CMD preamble + command word (a)
  WRITE_W,          // single CS: WRITE preamble + data word (a)
  WRITE_REG,        // single CS: WRITE preamble + addr(a) + value(b)
                    //   (caller must enqueue CMD(TCON_REG_WR) before this)
  READ_DEV_INFO,    // single CS: READ preamble + dummy + read DevInfo struct
                    //   (caller enqueues CMD(GET_DEV_INFO) first; loop HW_RDY gate
                    //    ensures data is ready before this op runs)
  READ_WORD,        // single CS: READ preamble + dummy + read one 16-bit word
                    //   into read_result_. Loop HW_RDY gate ensures data ready.
  CHECK_LUT_IDLE,   // checks read_result_; if non-zero, re-enqueues read sequence
  SET_1BPP,         // uses read_result_ to set UP1SR bit 2, enqueues writes
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
// the queue drains, advance_phase_() runs the next phase.
// This separation keeps per-Op work tiny and predictable.
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
  UPDATE_TRANSFER,  // one LD_IMG_AREA, time-sliced row streaming, one LD_IMG_END
  UPDATE_REFRESH,   // wait LUT idle, optionally enable 1bpp, send DPY_BUF_AREA
  UPDATE_SLEEP,     // optional deep sleep
};

class IT8951Display : public Display,
                      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                            spi::DATA_RATE_2MHZ> {
 public:
  IT8951Display(const char *name, uint16_t width, uint16_t height) : name_(name), width_(width), height_(height) {
    this->row_width_ = this->compute_row_width_();
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
  void set_enable_pins(std::vector<GPIOPin *> pins) { this->enable_pins_ = std::move(pins); }
  void set_reset_duration(uint32_t ms) { this->reset_duration_ = ms; }
  void set_full_update_every(uint8_t n) {
    this->full_update_every_ = n;
    // Seed the counter so the very first update trips the full-update branch in
    // prepare_update_region_, giving a freshly-booted panel a clean GC16 refresh
    // before any partial (fast-waveform) updates begin.
    this->partial_update_count_ = n;
  }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }
  void set_sleep_when_done(bool s) { this->sleep_when_done_ = s; }
  void set_vcom(uint16_t vcom_mv) { this->vcom_ = vcom_mv; }
  void set_vcom_register(uint16_t selector) { this->vcom_register_ = selector; }
  void set_force_temperature(int16_t celsius) {
    this->force_temperature_ = celsius;
    this->force_temperature_set_ = true;
  }
  void set_use_legacy_dpy_area(bool use) { this->use_legacy_dpy_area_ = use; }
  // Pixel format: true = 4bpp grayscale framebuffer, false = packed 1bpp
  // monochrome framebuffer. Chosen at config time; the framebuffer is stored
  // in this native format and every update uses the matching transfer path.
  void set_grayscale(bool g) { this->grayscale_ = g; }
  // Monochrome only: ordered-dither pale colours (true) vs a hard 50% threshold.
  void set_dithering(bool d) { this->dithering_ = d; }
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
  void update_mode(UpdateMode mode);
  // Double-buffering split (it8951.load / it8951.refresh actions):
  // load_buffer() transfers the framebuffer's dirty region into controller
  // RAM WITHOUT displaying it (gated on LUT idle so an in-progress waveform
  // is never torn); refresh_full() displays the full screen from controller
  // RAM WITHOUT transferring. Together they let a page be preloaded while
  // another is on the panel, making the swap a single ~1s waveform instead
  // of render + transfer + waveform.
  void load_buffer();
  void refresh_full(UpdateMode mode);
  // True when no update is in progress or pending — i.e. buffer_ is not
  // being streamed and may be re-rendered (e.g. to preload another page).
  bool is_idle() const { return this->phase_ == Phase::IDLE && !this->update_pending_; }
  DisplayType get_display_type() override { return this->grayscale_ ? DISPLAY_TYPE_GRAYSCALE : DISPLAY_TYPE_BINARY; }
  void fill(Color color) override;
  void clear() override { this->fill(Color::WHITE); }
  void draw_pixel_at(int x, int y, Color color) override;
  // Bulk pixel blit (used by LVGL and image rendering). Overridden to write
  // straight into the framebuffer, avoiding the base class's per-pixel
  // draw_pixel_at overhead (watchdog feed, clipping test, dirty-box clamps).
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                      ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
  int get_width() override { return (this->effective_transform_ & TRANSFORM_SWAP_XY) ? this->height_ : this->width_; }
  int get_height() override { return (this->effective_transform_ & TRANSFORM_SWAP_XY) ? this->width_ : this->height_; }

 protected:
  int get_height_internal() override { return this->height_; }
  int get_width_internal() override { return this->width_; }

  // --- Coord transform / dirty region ---
  void update_effective_transform_();
  // Map display (logical) coordinates to native framebuffer coordinates by
  // applying effective_transform_ (swap/mirror). Shared by rotate_coordinates_
  // and the bulk draw_pixels_at path.
  void apply_transform_(int &x, int &y) const;
  bool rotate_coordinates_(int &x, int &y);
  void reset_dirty_region_();

  // --- Framebuffer geometry / monochrome packing ---
  // Bytes per row for the configured pixel format: 4bpp grayscale packs two
  // pixels per byte; monochrome packs eight bits per byte, rounded up to a
  // whole 16-pixel group (matching the controller's 8bpp-load / 1bpp trick).
  uint16_t compute_row_width_() const {
    return this->grayscale_ ? static_cast<uint16_t>((static_cast<uint32_t>(this->width_) + 1) / 2)
                            : static_cast<uint16_t>(((static_cast<uint32_t>(this->width_) + 15) / 16) * 2);
  }
  void set_mono_pixel_(uint16_t x, uint16_t y, bool value) const;
  // Write a 4bpp grayscale nibble into the framebuffer (two pixels per byte).
  void set_gray_pixel_(uint16_t x, uint16_t y, uint8_t nibble) const;
  // Convert a color and write it at native framebuffer coordinates: a 4bpp
  // nibble in grayscale mode, or an ordered-dithered bit in monochrome mode.
  void write_pixel_native_(uint16_t x, uint16_t y, const Color &color) const;

  // --- Op queue / loop machinery ---
  void enqueue_(OpType type, uint16_t a = 0, uint16_t b = 0);
  void prepend_(OpType type, uint16_t a = 0, uint16_t b = 0);
  bool is_busy_() const;
  void process_op_(const Op &op);
  void advance_phase_();
  void set_phase_(Phase next);
  void start_update_(UpdateMode mode, bool load_only = false, bool refresh_only = false);

  // --- SPI primitives (each is one CS-asserted burst, fully non-blocking) ---
  void spi_cmd_(uint16_t cmd);
  void spi_write_word_(uint16_t value);
  void spi_write_reg_(uint16_t addr, uint16_t value);
  void spi_write_args_(const uint16_t *args, uint16_t count);
  uint16_t spi_read_word_();  // non-blocking: HW_RDY confirmed by loop gate
  void spi_read_dev_info_();  // non-blocking: HW_RDY confirmed by loop gate

  // --- Compound Ops (small bounded helpers) ---
  void op_xfer_lisar_();
  void op_xfer_area_args_();
  void op_xfer_area_end_();
  bool op_xfer_rows_();  // returns true when current update area fully sent
  void op_dpy_buf_args_();
  void op_check_lut_idle_();
  void op_set_1bpp_();

  // --- Phase enqueuers ---
  void enqueue_init_reset_();
  void enqueue_init_dev_info_();
  void enqueue_init_vcom_();
  void enqueue_init_temp_();
  void enqueue_update_transfer_();
  void enqueue_update_refresh_();
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
  // Requests a continuous (non-throttled) main loop while streaming image data
  // so 20ms transfer slices aren't separated by the ~16ms default loop interval.
  HighFrequencyLoopRequester high_freq_;

  // Pending update bookkeeping. The load/refresh flags ride along with the
  // mode; coalescing keeps only the latest pending request (latest wins).
  bool update_pending_{false};
  UpdateMode pending_update_mode_{UPDATE_MODE_NONE};
  bool pending_load_only_{false};
  bool pending_refresh_only_{false};
  UpdateMode active_mode_{UPDATE_MODE_NONE};
  bool active_load_only_{false};
  bool active_refresh_only_{false};
  uint16_t area_x_{0}, area_y_{0}, area_w_{0}, area_h_{0};
  uint16_t transfer_row_{0};
  bool initialised_{false};
  // True once TCON_SLEEP has been sent and the controller has not been woken
  // since. The next update must issue TCON_SYS_RUN before any SPI op.
  bool asleep_{false};
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
  uint8_t *buffer_{};
  uint8_t transform_{0};
  uint8_t effective_transform_{0};
  uint8_t full_update_every_{1};
  uint32_t reset_duration_{10};
  uint16_t vcom_{2300};
  uint16_t vcom_register_{I80_CMD_VCOM_WRITE};
  int16_t force_temperature_{DEFAULT_FORCE_TEMP_C};
  bool force_temperature_set_{false};
  bool use_legacy_dpy_area_{false};
  bool invert_colors_{false};
  bool sleep_when_done_{false};
  // Pixel format selector (see set_grayscale): true = 4bpp grayscale,
  // false = packed 1bpp monochrome.
  bool grayscale_{true};
  // Monochrome dithering (see set_dithering): true = ordered dither.
  bool dithering_{true};
  UpdateMode default_update_mode_{UPDATE_MODE_NONE};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};
  // GPIOs driven high during setup to power on the panel (empty if unused).
  std::vector<GPIOPin *> enable_pins_;

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
  TEMPLATABLE_VALUE(UpdateMode, mode)

 protected:
  void play(const Ts &...x) override {
    if (!this->display_->is_ready())
      return;
    if (this->mode_.has_value()) {
      this->display_->update_mode(this->mode_.value(x...));
    } else {
      this->display_->update();
    }
  }

  IT8951Display *display_;
};

// it8951.load — transfer the framebuffer to controller RAM without displaying.
template<typename... Ts> class IT8951LoadAction : public Action<Ts...> {
 public:
  explicit IT8951LoadAction(IT8951Display *display) : display_(display) {}

 protected:
  void play(const Ts &...x) override {
    if (!this->display_->is_ready())
      return;
    this->display_->load_buffer();
  }

  IT8951Display *display_;
};

// it8951.refresh — display the full screen from controller RAM without
// transferring (shows whatever was last loaded, e.g. by it8951.load).
template<typename... Ts> class IT8951RefreshAction : public Action<Ts...> {
 public:
  explicit IT8951RefreshAction(IT8951Display *display) : display_(display) {}
  TEMPLATABLE_VALUE(UpdateMode, mode)

 protected:
  void play(const Ts &...x) override {
    if (!this->display_->is_ready())
      return;
    this->display_->refresh_full(this->mode_.has_value() ? this->mode_.value(x...) : UPDATE_MODE_GC16);
  }

  IT8951Display *display_;
};

}  // namespace esphome::it8951
