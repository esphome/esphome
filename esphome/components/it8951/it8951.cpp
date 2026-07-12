#include "it8951.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::it8951 {

static const char *const TAG = "it8951";

// Soft cap for time spent in a single XFER_ROWS Op so we yield back to the
// loop within one tick budget.
static constexpr uint32_t MAX_TRANSFER_TIME_MS = 20;

// --- Loop / scheduling -------------------------------------------------------

void IT8951Display::enqueue_(OpType type, uint16_t a, uint16_t b) {
  if (!this->queue_.push_back(Op{type, a, b})) {
    ESP_LOGE(TAG, "Op queue overflow (cap=%u); dropping op type=%u", static_cast<unsigned>(OP_QUEUE_SIZE),
             static_cast<unsigned>(type));
  }
}

void IT8951Display::prepend_(OpType type, uint16_t a, uint16_t b) {
  if (!this->queue_.push_front(Op{type, a, b})) {
    ESP_LOGE(TAG, "Op queue overflow (cap=%u); dropping op type=%u", static_cast<unsigned>(OP_QUEUE_SIZE),
             static_cast<unsigned>(type));
  }
}

bool IT8951Display::is_busy_() const {
  // IT8951 Hardware Ready (HW_RDY): HIGH = ready, LOW = busy.
  return !this->busy_pin_->digital_read();
}

void IT8951Display::loop() {
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - this->delay_until_) < 0)
    return;

  // Nothing queued — either the current phase has more work to enqueue, or
  // we're done.
  if (this->queue_.empty()) {
    if (this->phase_ == Phase::IDLE) {
      this->disable_loop();
      return;
    }
    this->advance_phase_();
    if (this->queue_.empty())
      return;
  }

  // Gate SPI ops on HW_RDY. GPIO/DELAY ops run unconditionally — they're how
  // we get the controller out of a stuck-busy state in the first place
  // (e.g. during reset, HW_RDY is undefined/low until ROM boot completes).
  Op queued_op = this->queue_.front();
  const bool needs_hardware_ready = queued_op.type != OpType::GPIO_RESET_LOW &&
                                    queued_op.type != OpType::GPIO_RESET_HIGH && queued_op.type != OpType::DELAY_MS;
  if (needs_hardware_ready && this->is_busy_()) {
    // Signed elapsed: any pending DELAY_MS or scheduled work in the near
    // future shows up as <= 0 elapsed and won't trigger a false timeout.
    const int32_t elapsed = static_cast<int32_t>(now - this->phase_started_at_);
    ESP_LOGV(TAG, "HW_RDY is LOW (busy) in phase %u, elapsed=%" PRId32 "ms", static_cast<unsigned>(this->phase_),
             elapsed);
    if (elapsed > static_cast<int32_t>(BUSY_TIMEOUT_MS)) {
      ESP_LOGW(TAG, "Busy timeout (%" PRIu32 "ms) in phase %u, recovering", elapsed,
               static_cast<unsigned>(this->phase_));
      this->recover_();
    }
    return;
  }

  this->queue_.pop_front();
  this->process_op_(queued_op);
}

void IT8951Display::process_op_(const Op &op) {
  ESP_LOGV(TAG, "Processing op type=%u a=0x%04X b=0x%04X", static_cast<unsigned>(op.type), op.a, op.b);
  switch (op.type) {
    case OpType::CMD:
      this->spi_cmd_(op.a);
      break;
    case OpType::WRITE_W:
      this->spi_write_word_(op.a);
      break;
    case OpType::WRITE_REG:
      this->spi_write_reg_(op.a, op.b);
      break;
    case OpType::READ_DEV_INFO:
      this->spi_read_dev_info_();
      break;
    case OpType::READ_WORD:
      this->read_result_ = this->spi_read_word_();
      break;
    case OpType::CHECK_LUT_IDLE:
      this->op_check_lut_idle_();
      break;
    case OpType::SET_1BPP:
      this->op_set_1bpp_();
      break;
    case OpType::XFER_LISAR:
      this->op_xfer_lisar_();
      break;
    case OpType::XFER_AREA_CMD:
      this->spi_cmd_(TCON_LD_IMG_AREA);
      break;
    case OpType::XFER_AREA_ARGS:
      this->op_xfer_area_args_();
      break;
    case OpType::XFER_ROWS:
      // Stream rows into the single open LD_IMG_AREA load. The load stays open
      // across loop iterations (CS toggles between bursts, matching the
      // reference driver), so a partial slice just re-queues another XFER_ROWS
      // pass to resume; only when all rows are sent do we close it with one
      // LD_IMG_END. This avoids an LD_IMG_END / LD_IMG_AREA round-trip per slice.
      if (this->op_xfer_rows_()) {
        this->enqueue_(OpType::XFER_AREA_END);
      } else {
        this->enqueue_(OpType::XFER_ROWS);
      }
      break;
    case OpType::XFER_AREA_END:
      this->op_xfer_area_end_();
      break;
    case OpType::DPY_BUF_CMD:
      // Some panel firmwares (notably Seeed reTerminal E1003) silently drop
      // I80_CMD_DPY_BUF_AREA (0x0037) — the LUT engine never starts and the
      // host eventually times out after ~12s. Fall back to the basic
      // I80_CMD_DPY_AREA (0x0034) for those panels; the buffer address is
      // already programmed via LISAR during the transfer phase.
      this->spi_cmd_(this->use_legacy_dpy_area_ ? I80_CMD_DPY_AREA : I80_CMD_DPY_BUF_AREA);
      break;
    case OpType::DPY_BUF_ARGS:
      this->op_dpy_buf_args_();
      break;
    case OpType::GPIO_RESET_LOW:
      if (this->reset_pin_ != nullptr)
        this->reset_pin_->digital_write(false);
      break;
    case OpType::GPIO_RESET_HIGH:
      if (this->reset_pin_ != nullptr)
        this->reset_pin_->digital_write(true);
      break;
    case OpType::DELAY_MS:
      this->delay_until_ = millis() + op.a;
      break;
  }
}

void IT8951Display::set_phase_(Phase next) {
  ESP_LOGV(TAG, "Phase %u -> %u", static_cast<unsigned>(this->phase_), static_cast<unsigned>(next));
  // Run the loop continuously for the whole active sequence, returning to normal
  // throttling only at IDLE. Each queued op is processed one per loop iteration,
  // so at the default ~16ms loop interval the dozens of small ops in the refresh
  // and restore phases (register polls, 1bpp enable/restore, DPY) would dominate
  // a partial update's latency. The LUT-idle polls are DELAY_MS-paced, so this
  // doesn't hammer SPI — it only spends a little extra CPU during the (short,
  // infrequent) update instead of sleeping between ops. start()/stop() are
  // idempotent, so driving them off the transition is safe.
  if (next == Phase::IDLE) {
    this->high_freq_.stop();
  } else {
    this->high_freq_.start();
  }
  this->phase_ = next;
  this->phase_started_at_ = millis();
}

void IT8951Display::advance_phase_() {
  switch (this->phase_) {
    case Phase::IDLE:
      if (this->initialised_ && this->update_pending_) {
        this->update_pending_ = false;
        this->active_mode_ = this->pending_update_mode_;
        this->active_load_only_ = this->pending_load_only_;
        this->active_refresh_only_ = this->pending_refresh_only_;
        this->update_started_at_ = millis();
        this->set_phase_(Phase::UPDATE_PREPARE);
        this->advance_phase_();
      } else {
        this->disable_loop();
      }
      break;

    case Phase::INIT_RESET:
      this->set_phase_(Phase::INIT_DEV_INFO);
      this->enqueue_init_dev_info_();
      break;

    case Phase::INIT_DEV_INFO:
      if (this->dev_info_.panel_width == 0 || this->dev_info_.panel_width > 2048 || this->dev_info_.panel_height == 0 ||
          this->dev_info_.panel_height > 2048 || this->dev_info_.panel_width == 0xFFFF ||
          this->dev_info_.panel_height == 0xFFFF) {
        if (++this->dev_info_attempts_ < 5) {
          ESP_LOGW(TAG, "DevInfo attempt %u returned invalid data (W=%u H=%u), retrying...", this->dev_info_attempts_,
                   this->dev_info_.panel_width, this->dev_info_.panel_height);
          // Give the controller more time, then re-read.
          this->enqueue_(OpType::DELAY_MS, 100);
          this->enqueue_init_dev_info_();
          return;
        }
        ESP_LOGE(TAG, "DevInfo invalid after %u attempts (W=%u H=%u)", this->dev_info_attempts_,
                 this->dev_info_.panel_width, this->dev_info_.panel_height);
        this->mark_failed(LOG_STR("Failed to read IT8951 device info"));
        this->set_phase_(Phase::IDLE);
        return;
      }

      if (this->dev_info_.panel_width != this->width_ || this->dev_info_.panel_height != this->height_) {
        ESP_LOGE(TAG, "Panel dimension mismatch: configured=%ux%u, DevInfo=%ux%u. Check model/dimensions settings.",
                 this->width_, this->height_, this->dev_info_.panel_width, this->dev_info_.panel_height);
        this->mark_failed(LOG_STR("IT8951 panel dimensions do not match DevInfo"));
        this->set_phase_(Phase::IDLE);
        return;
      }

      this->dev_info_attempts_ = 0;
      this->row_width_ = this->compute_row_width_();
      this->buffer_length_ = static_cast<size_t>(this->row_width_) * static_cast<size_t>(this->height_);
      this->img_buf_addr_l_ = this->dev_info_.img_buf_addr_l;
      this->img_buf_addr_h_ = this->dev_info_.img_buf_addr_h;
      ESP_LOGI(TAG, "DevInfo: %ux%u, ImgBuf 0x%04X%04X", this->width_, this->height_, this->img_buf_addr_h_,
               this->img_buf_addr_l_);
      this->set_phase_(Phase::INIT_VCOM);
      this->enqueue_init_vcom_();
      break;

    case Phase::INIT_VCOM:
      this->set_phase_(Phase::INIT_TEMP);
      if (this->force_temperature_set_) {
        this->enqueue_init_temp_();
      } else {
        this->advance_phase_();
      }
      break;

    case Phase::INIT_TEMP:
      this->set_phase_(Phase::INIT_DONE);
      this->advance_phase_();
      break;

    case Phase::INIT_DONE:
      if (this->configured_data_rate_ != 0 && this->configured_data_rate_ != this->data_rate_) {
        this->spi_teardown();
        this->set_data_rate(this->configured_data_rate_);
        this->spi_setup();
      }
      this->initialised_ = true;
      this->recovery_attempts_ = 0;
      ESP_LOGCONFIG(TAG, "IT8951 setup complete");
      this->set_phase_(Phase::IDLE);
      this->advance_phase_();
      break;

    case Phase::UPDATE_PREPARE: {
      // refresh-only (it8951.refresh): nothing to render or transfer — the
      // image was preloaded into controller RAM. Display the full screen.
      if (this->active_refresh_only_) {
        this->area_x_ = 0;
        this->area_y_ = 0;
        this->area_w_ = this->width_;
        this->area_h_ = this->height_;
        this->set_phase_(Phase::UPDATE_TRANSFER);
        this->advance_phase_();
        break;
      }
      this->do_update_();
      UpdateMode mode = this->active_mode_;
      if (!this->prepare_update_region_(mode)) {
        ESP_LOGD(TAG, "Nothing to update");
        this->active_load_only_ = false;
        this->set_phase_(Phase::IDLE);
        this->advance_phase_();
        return;
      }
      this->active_mode_ = mode;
      // load-only (it8951.load): the transfer overwrites controller RAM that
      // an in-progress waveform may still be reading from (the panel could be
      // mid-GC16 showing the other page). Normal updates skip this gate for
      // latency and immediately re-display anyway; a preload never re-displays,
      // so tearing the visible image would persist — wait for LUT idle first.
      if (this->active_load_only_) {
        this->enqueue_(OpType::CMD, TCON_REG_RD);
        this->enqueue_(OpType::WRITE_W, LUTAFSR);
        this->enqueue_(OpType::READ_WORD);
        this->enqueue_(OpType::CHECK_LUT_IDLE);
      }
      this->set_phase_(Phase::UPDATE_TRANSFER);
      this->enqueue_update_transfer_();
      break;
    }

    case Phase::UPDATE_TRANSFER:
      if (this->active_load_only_) {
        // Preload complete: image is in controller RAM, skip the display.
        this->set_phase_(Phase::UPDATE_SLEEP);
        this->enqueue_update_sleep_();
        break;
      }
      this->set_phase_(Phase::UPDATE_REFRESH);
      this->enqueue_update_refresh_();
      break;

    case Phase::UPDATE_REFRESH:
      // Fire-and-forget: don't block here waiting for the refresh to complete.
      // The next update's pre-display LUT-idle poll (and the HW_RDY-gated
      // TCON_SLEEP) wait as needed, so the refresh time stays off this update's
      // critical path. The 1bpp display mode is left enabled rather than
      // restored after every update: on a monochrome display every update
      // (DU partials and the periodic GC16 cleans) runs in 1bpp mode, so the
      // bit never needs clearing — and clearing it required a full
      // refresh-length LUT-idle wait.
      this->set_phase_(Phase::UPDATE_SLEEP);
      this->enqueue_update_sleep_();
      break;

    case Phase::UPDATE_SLEEP:
      ESP_LOGV(TAG, "Update took %" PRIu32 "ms (mode=%u area=%ux%u@%u,%u%s)", millis() - this->update_started_at_,
               static_cast<unsigned>(this->active_mode_), this->area_w_, this->area_h_, this->area_x_, this->area_y_,
               this->active_load_only_ ? " load-only" : (this->active_refresh_only_ ? " refresh-only" : ""));
      this->active_load_only_ = false;
      this->active_refresh_only_ = false;
      this->set_phase_(Phase::IDLE);
      this->advance_phase_();
      break;
  }
}

// --- Setup -------------------------------------------------------------------

void IT8951Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up IT8951...");
  this->configured_data_rate_ = this->data_rate_;
  this->data_rate_ = SPI_PROBE_FREQUENCY;
  this->spi_setup();

  // Power on the panel before reset and the init handshake.
  for (auto *pin : this->enable_pins_) {
    pin->setup();
    pin->digital_write(true);
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }

  this->update_effective_transform_();
  this->reset_dirty_region_();

  // Allocate the framebuffer now: its size is fixed by the configured pixel
  // format and dimensions, so there's no need to defer to the async controller
  // init. LVGL (and other writers) can push pixels via draw_pixels_at as soon
  // as the component is set up — before init completes — and without a buffer
  // those writes would dereference a null pointer and crash.
  this->row_width_ = this->compute_row_width_();
  this->buffer_length_ = static_cast<size_t>(this->row_width_) * static_cast<size_t>(this->height_);
  RAMAllocator<uint8_t> allocator{};
  this->buffer_ = allocator.allocate(this->buffer_length_);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Failed to allocate IT8951 framebuffer"));
    return;
  }
  // The allocator does not zero memory; start blank (white) so undrawn regions
  // (e.g. with auto_clear disabled) don't show garbage on the first update.
  this->fill(Color::WHITE);

  // Kick off async init via the queue. Reset pulse + boot delay + wake +
  // packed-write enable; everything blocking lives as DELAY_MS Ops gated by
  // the loop scheduler.
  this->set_phase_(Phase::INIT_RESET);
  this->enqueue_init_reset_();
  this->enable_loop();
}

void IT8951Display::on_safe_shutdown() {
  // Best-effort synchronous sleep — runs during shutdown so we don't queue.
  this->spi_cmd_(TCON_SLEEP);
}

// --- Init op enqueuers -------------------------------------------------------

void IT8951Display::enqueue_init_reset_() {
  // A reset (including recovery) re-runs SYS_RUN below, so the controller is
  // awake once this sequence completes.
  this->asleep_ = false;
  // Reset pulse: high -> low (reset_duration) -> high -> wait for ROM boot.
  this->enqueue_(OpType::GPIO_RESET_HIGH);
  this->enqueue_(OpType::GPIO_RESET_LOW);
  this->enqueue_(OpType::DELAY_MS, static_cast<uint16_t>(this->reset_duration_));
  this->enqueue_(OpType::GPIO_RESET_HIGH);
  // SPI ROM boot. HW_RDY gating in loop() handles the actual wait, but a small
  // floor avoids hammering SPI before HW_RDY has settled high. 300ms matches
  // what most IT8951 reference drivers use for safety.
  this->enqueue_(OpType::DELAY_MS, 300);
  this->enqueue_(OpType::CMD, TCON_SYS_RUN);
  this->enqueue_(OpType::DELAY_MS, 10);      // clocks settle after SYS_RUN
  this->enqueue_(OpType::CMD, TCON_REG_WR);  // packed write mode
  this->enqueue_(OpType::WRITE_REG, I80CPCR, 0x0001);
}

void IT8951Display::enqueue_init_dev_info_() {
  // CMD triggers the controller to prepare DevInfo. HW_RDY drops while it works.
  // The loop-level HW_RDY gate non-blockingly waits before dispatching READ_DEV_INFO.
  this->enqueue_(OpType::CMD, I80_CMD_GET_DEV_INFO);
  this->enqueue_(OpType::READ_DEV_INFO);
}

void IT8951Display::enqueue_init_vcom_() {
  // Always write configured VCOM. The IT8951 stores it in OTP-backed RAM;
  // rewriting the same value is harmless. The VCOM SET selector is
  // panel-specific (see I80_CMD_VCOM_WRITE / I80_CMD_VCOM_WRITE_ALT in
  // it8951_defs.h) and is supplied via the model preset.
  this->enqueue_(OpType::CMD, I80_CMD_VCOM);
  this->enqueue_(OpType::WRITE_W, this->vcom_register_);
  this->enqueue_(OpType::WRITE_W, this->vcom_);
}

void IT8951Display::enqueue_init_temp_() {
  // Force panel temperature (in degrees C) so the controller selects the
  // correct waveform LUT. Some panels (e.g. Seeed reTerminal E1003) ship
  // with auto-temperature disabled and rely on the host to declare the
  // operating temperature; without this, grayscale waveforms run against
  // a mismatched LUT and pixels do not visibly change even though the LUT
  // engine completes a full cycle.
  this->enqueue_(OpType::CMD, I80_CMD_FORCE_TEMP);
  this->enqueue_(OpType::WRITE_W, I80_CMD_FORCE_TEMP_WRITE);
  this->enqueue_(OpType::WRITE_W, static_cast<uint16_t>(this->force_temperature_));
}

// --- Update op enqueuers -----------------------------------------------------

void IT8951Display::enqueue_update_transfer_() {
  // If the controller was put to sleep after the previous update, wake it
  // before touching the display engine. TCON_SLEEP gates off all clocks; a
  // register read (e.g. the LUTAFSR poll in UPDATE_REFRESH) returns a frozen
  // value while asleep, so without this the next update stalls forever in
  // op_check_lut_idle_(). SRAM/registers (packed-write mode, VCOM, LUT) are
  // retained across sleep, so SYS_RUN + a short settle is all that's needed.
  if (this->asleep_) {
    this->enqueue_(OpType::CMD, TCON_SYS_RUN);
    this->enqueue_(OpType::DELAY_MS, 10);  // clocks settle after SYS_RUN
    this->asleep_ = false;
  }
  this->transfer_row_ = 0;
  // Open a single LD_IMG_AREA load for the whole region. XFER_ROWS streams into
  // it across as many time-sliced passes as needed and emits the one matching
  // LD_IMG_END when the last row is sent (see the XFER_ROWS handler).
  this->enqueue_(OpType::XFER_LISAR);
  this->enqueue_(OpType::XFER_AREA_CMD);
  this->enqueue_(OpType::XFER_AREA_ARGS);
  this->enqueue_(OpType::XFER_ROWS);
}

void IT8951Display::enqueue_update_refresh_() {
  ESP_LOGV(TAG, "Enqueueing refresh ops: grayscale=%u", this->grayscale_);
  // A refresh-only update (it8951.refresh) skips the transfer stage, which is
  // where a sleeping controller is normally woken — wake it here if needed.
  if (this->asleep_) {
    this->enqueue_(OpType::CMD, TCON_SYS_RUN);
    this->enqueue_(OpType::DELAY_MS, 10);  // clocks settle after SYS_RUN
    this->asleep_ = false;
  }
  // Poll LUT idle: CMD(REG_RD) → WRITE_W(LUTAFSR) → READ_WORD → CHECK_LUT_IDLE
  this->enqueue_(OpType::CMD, TCON_REG_RD);
  this->enqueue_(OpType::WRITE_W, LUTAFSR);
  this->enqueue_(OpType::READ_WORD);
  this->enqueue_(OpType::CHECK_LUT_IDLE);
  if (!this->grayscale_) {
    // Read UP1SR+2: CMD(REG_RD) → WRITE_W(UP1SR+2) → READ_WORD → SET_1BPP
    this->enqueue_(OpType::CMD, TCON_REG_RD);
    this->enqueue_(OpType::WRITE_W, static_cast<uint16_t>(UP1SR + 2));
    this->enqueue_(OpType::READ_WORD);
    this->enqueue_(OpType::SET_1BPP);
  }
  this->enqueue_(OpType::DPY_BUF_CMD);
  this->enqueue_(OpType::DPY_BUF_ARGS);
}

void IT8951Display::enqueue_update_sleep_() {
  if (this->sleep_when_done_) {
    this->enqueue_(OpType::CMD, TCON_SLEEP);
    // Remember that the controller is now asleep so the next update wakes it
    // (see enqueue_update_transfer_) before polling any register.
    this->asleep_ = true;
  }
}

// --- SPI primitives ----------------------------------------------------------
//
// IT8951 SPI protocol: no DC pin. 16-bit preamble word identifies whether
// the transaction is command (0x6000), write-data (0x0000), or read-data
// (0x1000).
//
// All ops are fully non-blocking at the loop level. The loop-level HW_RDY gate
// guarantees the controller is ready before any op is dispatched.
//
// Within a single CS-asserted transaction, the IT8951 requires HW_RDY to be
// checked after the preamble word before sending the first data word. This
// is a hardware protocol requirement — the controller needs a few clock
// cycles to latch the preamble and configure its internal bus direction.
// In practice this completes in <1µs for write ops; we use a short spin
// (max ~50µs) that never triggers under normal operation.

static constexpr uint32_t INTRA_CS_READY_TIMEOUT_US = 50;

static inline void wait_for_hardware_ready(GPIOPin *busy_pin) {
  if (busy_pin == nullptr)
    return;
  uint32_t waited = 0;
  while (!busy_pin->digital_read()) {
    if (waited >= INTRA_CS_READY_TIMEOUT_US)
      return;
    delayMicroseconds(1);
    waited += 1;
  }
}

void IT8951Display::spi_cmd_(uint16_t cmd) {
  this->enable();
  this->write_byte16(PACKET_TYPE_CMD);
  wait_for_hardware_ready(this->busy_pin_);
  this->write_byte16(cmd);
  this->disable();
}

void IT8951Display::spi_write_word_(uint16_t value) {
  this->enable();
  this->write_byte16(PACKET_TYPE_WRITE);
  wait_for_hardware_ready(this->busy_pin_);
  this->write_byte16(value);
  this->disable();
}

void IT8951Display::spi_write_reg_(uint16_t addr, uint16_t value) {
  // Single CS transaction: WRITE preamble + addr + value.
  // Caller must have already sent CMD(TCON_REG_WR) as a prior op.
  this->enable();
  this->write_byte16(PACKET_TYPE_WRITE);
  wait_for_hardware_ready(this->busy_pin_);
  this->write_byte16(addr);
  this->write_byte16(value);
  this->disable();
}

void IT8951Display::spi_write_args_(const uint16_t *args, uint16_t count) {
  // Single CS transaction: WRITE preamble + N data words.
  this->enable();
  this->write_byte16(PACKET_TYPE_WRITE);
  wait_for_hardware_ready(this->busy_pin_);
  for (uint16_t i = 0; i < count; i++)
    this->write_byte16(args[i]);
  this->disable();
}

uint16_t IT8951Display::spi_read_word_() {
  // Single CS read transaction. HW_RDY was confirmed HIGH by the loop gate
  // before this op was dispatched, so data is ready.
  this->enable();
  this->write_byte16(PACKET_TYPE_READ);
  wait_for_hardware_ready(this->busy_pin_);
  this->write_byte16(0x0000);  // dummy — provides clock cycles for controller
  wait_for_hardware_ready(this->busy_pin_);
  // Read byte-by-byte: a 2-byte transfer_array can lose the low byte on
  // ESP-IDF SPI DMA due to 4-byte alignment requirements.
  const uint8_t hi = this->transfer_byte(0);
  const uint8_t lo = this->transfer_byte(0);
  this->disable();
  return encode_uint16(hi, lo);
}

void IT8951Display::spi_read_dev_info_() {
  // Read DevInfo struct. The CMD(GET_DEV_INFO) was already sent as a prior op,
  // and the loop HW_RDY gate waited for the controller to prepare data.
  std::memset(&this->dev_info_, 0, sizeof(this->dev_info_));
  this->enable();
  this->write_byte16(PACKET_TYPE_READ);
  wait_for_hardware_ready(this->busy_pin_);
  this->write_byte16(0x0000);  // dummy
  wait_for_hardware_ready(this->busy_pin_);
  auto *words = reinterpret_cast<uint16_t *>(&this->dev_info_);
  constexpr uint32_t word_count = sizeof(this->dev_info_) / sizeof(uint16_t);
  for (uint32_t i = 0; i < word_count; i++) {
    const uint8_t hi = this->transfer_byte(0);
    const uint8_t lo = this->transfer_byte(0);
    words[i] = encode_uint16(hi, lo);
  }
  this->disable();
}

// --- Compound Ops ------------------------------------------------------------

void IT8951Display::op_xfer_lisar_() {
  // Set image-buffer target address. Two register writes = 4 CS transactions.
  // Push to FRONT in reverse order so they execute before the rest of the queue.
  this->prepend_(OpType::WRITE_REG, LISAR, this->img_buf_addr_l_);
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
  this->prepend_(OpType::WRITE_REG, static_cast<uint16_t>(LISAR + 2), this->img_buf_addr_h_);
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
}

void IT8951Display::op_xfer_area_args_() {
  // Single CS transaction: WRITE preamble + 5 area-parameter words describing
  // the full update region. Sent once when the load is opened (transfer_row_ is
  // 0); XFER_ROWS then streams every row into this one area.
  uint16_t args[5];
  if (this->grayscale_) {
    args[0] = static_cast<uint16_t>((LDIMG_B_ENDIAN << 8) | (PIXEL_4BPP << 4));
    args[1] = this->area_x_;
    args[2] = this->area_y_;
    args[3] = this->area_w_;
    args[4] = this->area_h_;
  } else {
    // Monochrome is loaded via the 8bpp-packed trick: x and width are expressed
    // in bytes (8 pixels each) and the controller unpacks one bit per pixel.
    args[0] = static_cast<uint16_t>((LDIMG_L_ENDIAN << 8) | (PIXEL_8BPP << 4));
    args[1] = static_cast<uint16_t>(this->area_x_ / 8);
    args[2] = this->area_y_;
    args[3] = static_cast<uint16_t>(this->area_w_ / 8);
    args[4] = this->area_h_;
  }
  this->spi_write_args_(args, 5);
}

void IT8951Display::op_xfer_area_end_() { this->spi_cmd_(TCON_LD_IMG_END); }

bool IT8951Display::op_xfer_rows_() {
  const uint32_t start_time = millis();
  const uint16_t area_y = this->area_y_;
  const uint16_t area_h = this->area_h_;

  // Bytes per source row, and the byte offset of area_x within a row, in the
  // framebuffer's native packing. These match the per-row byte count the
  // controller expects from op_xfer_area_args_: area_w/2 for 4bpp grayscale,
  // area_w/8 for the 1bpp-packed monochrome trick. area_x / area_w are
  // 16-pixel aligned (see prepare_update_region_), so both divisions are exact.
  const uint16_t bytes_per_row =
      this->grayscale_ ? static_cast<uint16_t>(this->area_w_ >> 1) : static_cast<uint16_t>(this->area_w_ >> 3);
  const uint16_t row_x_bytes =
      this->grayscale_ ? static_cast<uint16_t>(this->area_x_ >> 1) : static_cast<uint16_t>(this->area_x_ >> 3);

  // Single CS write transaction — HW_RDY was confirmed high by the loop gate.
  this->enable();
  this->write_byte16(PACKET_TYPE_WRITE);
  wait_for_hardware_ready(this->busy_pin_);

  // Each source row is a contiguous slice of the framebuffer in both formats —
  // the buffer already holds the wire bytes — so stream it straight to SPI with
  // no per-pixel packing or temporary buffer.
  while (this->transfer_row_ < area_h) {
    const uint32_t offset = (static_cast<uint32_t>(area_y) + this->transfer_row_) * this->row_width_ + row_x_bytes;
    this->write_array(&this->buffer_[offset], bytes_per_row);
    this->transfer_row_++;
    if (millis() - start_time >= MAX_TRANSFER_TIME_MS)
      break;
  }

  this->disable();
  return this->transfer_row_ >= area_h;
}

void IT8951Display::op_dpy_buf_args_() {
  // I80_CMD_DPY_BUF_AREA (0x0037) takes 7 args (with explicit buffer addr).
  // I80_CMD_DPY_AREA     (0x0034) takes 5 args; the buffer address is taken
  // from LISAR which we program during the transfer phase, so this is safe.
  if (this->use_legacy_dpy_area_) {
    const uint16_t args[5] = {
        this->area_x_, this->area_y_, this->area_w_, this->area_h_, static_cast<uint16_t>(this->active_mode_),
    };
    this->spi_write_args_(args, 5);
    return;
  }
  const uint16_t args[7] = {
      this->area_x_,
      this->area_y_,
      this->area_w_,
      this->area_h_,
      static_cast<uint16_t>(this->active_mode_),
      this->img_buf_addr_l_,
      this->img_buf_addr_h_,
  };
  this->spi_write_args_(args, 7);
}

void IT8951Display::op_check_lut_idle_() {
  ESP_LOGV(TAG, "Checking LUT idle, read_result_=0x%04X", this->read_result_);
  // read_result_ holds LUTAFSR value from the preceding READ_WORD op.
  if (this->read_result_ != 0) {
    // LUT still busy — re-enqueue the full read sequence after a short delay.
    this->prepend_(OpType::CHECK_LUT_IDLE, 0, 0);
    this->prepend_(OpType::READ_WORD, 0, 0);
    this->prepend_(OpType::WRITE_W, LUTAFSR, 0);
    this->prepend_(OpType::CMD, TCON_REG_RD, 0);
    this->prepend_(OpType::DELAY_MS, 5, 0);
  }
}

void IT8951Display::op_set_1bpp_() {
  // read_result_ holds UP1SR+2 value. Set bit 2 and write back, then set BGVR.
  // Push to FRONT in reverse order so they execute before DPY_BUF_CMD/ARGS
  // that are already in the queue.
  const uint16_t modified = static_cast<uint16_t>(this->read_result_ | (1U << 2));
  this->prepend_(OpType::WRITE_REG, BGVR, 0xFF00);
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
  this->prepend_(OpType::WRITE_REG, UP1SR + 2, modified);
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
}

// --- Update prep / public API ------------------------------------------------

bool IT8951Display::prepare_update_region_(UpdateMode &mode) {
  this->partial_update_count_++;
  const bool full_update = this->partial_update_count_ >= this->full_update_every_;
  if (full_update) {
    this->partial_update_count_ = 0;
    mode = UPDATE_MODE_GC16;
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
  } else {
    // Align the partial region's X extent to 32 pixels. The IT8951's partial
    // display refresh snaps the X start/width to a 32-pixel boundary (the panel
    // source driver fetches 32-pixel chunks); refreshing a region whose X is
    // only 16-aligned makes the panel snap it down to the previous boundary,
    // shifting that update ~16px to the left. 32-alignment also satisfies the
    // load constraints (4bpp X must be a multiple of 4; the 8bpp-packed mono
    // load needs x/8 even, i.e. X a multiple of 16).
    this->x_low_ &= 0xFFE0;
    uint16_t temp_max = this->x_high_ > 0 ? static_cast<uint16_t>(this->x_high_ - 1) : 0;
    temp_max = static_cast<uint16_t>(temp_max | 0x001F);
    if (temp_max >= this->width_)
      temp_max = static_cast<uint16_t>(this->width_ - 1);
    this->x_high_ = static_cast<uint16_t>(temp_max + 1);
  }

  if (this->x_high_ <= this->x_low_ || this->y_high_ <= this->y_low_) {
    this->reset_dirty_region_();
    return false;
  }

  const uint16_t x = this->x_low_;
  const uint16_t y = this->y_low_;
  const uint16_t width = static_cast<uint16_t>(this->x_high_ - this->x_low_);
  const uint16_t height = static_cast<uint16_t>(this->y_high_ - this->y_low_);

  if (x >= this->width_ || y >= this->height_ || (x + width) > this->width_ || (y + height) > this->height_) {
    ESP_LOGE(TAG, "Dirty region (%u,%u %ux%u) out of bounds", x, y, width, height);
    this->reset_dirty_region_();
    return false;
  }

  this->area_x_ = x;
  this->area_y_ = y;
  this->area_w_ = width;
  this->area_h_ = height;
  this->transfer_row_ = 0;

  // On non-full updates, downgrade monochrome frames from the full, flashy GC16
  // clear to DU — a fast, low-flash absolute waveform — so full_update_every
  // buys cheaper refreshes between the periodic GC16 cleans that clear
  // accumulated ghosting.
  //
  // Grayscale frames are deliberately left on GC16: every reduced grayscale
  // waveform this controller exposes (the non-flashing GL family GL16/GLR16/
  // GLD16, and the 4-tone DU4) renders incorrectly on the supported panels —
  // a white background is driven to grey rather than staying white. GC16 is the
  // only waveform that reproduces grayscale faithfully, so we keep it.
  //
  // An explicitly configured non-GC16 update_mode is honoured as-is.
  if (!full_update && mode == UPDATE_MODE_GC16 && !this->grayscale_)
    mode = UPDATE_MODE_DU;

  this->reset_dirty_region_();

  ESP_LOGV(TAG, "Update: %ux%u@%u,%u mode=%u (%s)", width, height, x, y, static_cast<unsigned>(mode),
           this->grayscale_ ? "grayscale" : "mono");
  return true;
}

void IT8951Display::reset_dirty_region_() {
  this->x_low_ = this->width_;
  this->x_high_ = 0;
  this->y_low_ = this->height_;
  this->y_high_ = 0;
}

void IT8951Display::start_update_(UpdateMode mode, bool load_only, bool refresh_only) {
  if (this->phase_ == Phase::IDLE && this->initialised_) {
    this->update_started_at_ = millis();
    this->active_mode_ = mode;
    this->active_load_only_ = load_only;
    this->active_refresh_only_ = refresh_only;
    this->set_phase_(Phase::UPDATE_PREPARE);
    this->enable_loop();
    this->advance_phase_();
  } else {
    // Coalesce: latest pending request wins (mode AND load/refresh kind).
    this->update_pending_ = true;
    this->pending_update_mode_ = mode;
    this->pending_load_only_ = load_only;
    this->pending_refresh_only_ = refresh_only;
    this->enable_loop();
  }
}

void IT8951Display::update() {
  if (!this->is_ready())
    return;
  if (this->default_update_mode_ != UPDATE_MODE_NONE) {
    this->start_update_(this->default_update_mode_);
    return;
  }
  this->start_update_(UPDATE_MODE_GC16);
}

void IT8951Display::update_mode(UpdateMode mode) {
  if (!this->is_ready())
    return;
  if (mode == UPDATE_MODE_NONE) {
    ESP_LOGW(TAG, "Unknown update mode");
    return;
  }
  this->start_update_(mode);
}

void IT8951Display::load_buffer() {
  if (!this->is_ready())
    return;
  // Mode is irrelevant for a load (nothing is displayed); GC16 is a placeholder.
  this->start_update_(UPDATE_MODE_GC16, /*load_only=*/true, /*refresh_only=*/false);
}

void IT8951Display::refresh_full(UpdateMode mode) {
  if (!this->is_ready())
    return;
  if (mode == UPDATE_MODE_NONE)
    mode = UPDATE_MODE_GC16;
  this->start_update_(mode, /*load_only=*/false, /*refresh_only=*/true);
}

// --- Recovery ----------------------------------------------------------------

void IT8951Display::recover_() {
  if (++this->recovery_attempts_ > 3) {
    ESP_LOGE(TAG, "Recovery failed after %u attempts; giving up. Check BUSY pin wiring and power.",
             this->recovery_attempts_);
    this->mark_failed(LOG_STR("IT8951 recovery exhausted"));
    this->queue_.clear();
    this->set_phase_(Phase::IDLE);
    this->disable_loop();
    return;
  }
  ESP_LOGW(TAG, "Recovering (attempt %u): hardware-resetting controller (was in phase %u)", this->recovery_attempts_,
           static_cast<unsigned>(this->phase_));
  this->queue_.clear();
  this->update_pending_ = false;
  this->transfer_row_ = 0;
  this->initialised_ = false;
  this->dev_info_attempts_ = 0;

  // Drop SPI clock back to the safe probe rate for the re-init handshake.
  if (this->configured_data_rate_ != 0 && this->data_rate_ != SPI_PROBE_FREQUENCY) {
    this->spi_teardown();
    this->set_data_rate(SPI_PROBE_FREQUENCY);
    this->spi_setup();
  }

  // Force a full redraw on next opportunity.
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;

  this->set_phase_(Phase::INIT_RESET);
  this->enqueue_init_reset_();
  this->update_pending_ = true;
  this->pending_update_mode_ = UPDATE_MODE_GC16;
  this->enable_loop();
}

// --- Coordinate transform ----------------------------------------------------

void IT8951Display::update_effective_transform_() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_90_DEGREES:
      this->effective_transform_ = this->transform_ ^ (TRANSFORM_SWAP_XY | TRANSFORM_MIRROR_X);
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      this->effective_transform_ = this->transform_ ^ (TRANSFORM_MIRROR_Y | TRANSFORM_MIRROR_X);
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      this->effective_transform_ = this->transform_ ^ (TRANSFORM_SWAP_XY | TRANSFORM_MIRROR_Y);
      break;
    default:
      this->effective_transform_ = this->transform_;
      break;
  }
}

void IT8951Display::apply_transform_(int &x, int &y) const {
  if (this->effective_transform_ & TRANSFORM_SWAP_XY)
    std::swap(x, y);
  if (this->effective_transform_ & TRANSFORM_MIRROR_X)
    x = this->width_ - x - 1;
  if (this->effective_transform_ & TRANSFORM_MIRROR_Y)
    y = this->height_ - y - 1;
}

bool IT8951Display::rotate_coordinates_(int &x, int &y) {
  if (!this->get_clipping().inside(x, y))
    return false;
  this->apply_transform_(x, y);
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return false;
  this->x_low_ = clamp_at_most(this->x_low_, x);
  this->x_high_ = clamp_at_least(this->x_high_, x + 1);
  this->y_low_ = clamp_at_most(this->y_low_, y);
  this->y_high_ = clamp_at_least(this->y_high_, y + 1);
  return true;
}

// --- Color / drawing ---------------------------------------------------------

static uint8_t quantize_8bit_to_nibble(uint8_t value) {
  uint8_t nibble = static_cast<uint8_t>((static_cast<uint16_t>(value) + 8) >> 4);
  return nibble > 0x0F ? 0x0F : nibble;
}

static uint8_t color_to_nibble(const Color &color) {
  // Grayscale images are emitted as Color(gray, gray, gray, 0xFF).
  // Handle this shape first so endpoint values don't alias COLOR_ON/OFF.
  if (color.w == 0xFF && color.r == color.g && color.g == color.b)
    return quantize_8bit_to_nibble(color.r);

  if (color.raw_32 == 0)
    return 0x00;  // black
  if (color.raw_32 == 0xFFFFFFFF)
    return 0x0F;  // white

  // Derive luma from RGB using Rec.601 weights (0.299/0.587/0.114, scaled by
  // 256). Rec.601 is the standard for converting SDR images to grayscale and
  // spreads saturated colours across the mid-range; Rec.709 instead crams them
  // against white/black where the 16 panel levels are hard to tell apart.
  auto luma = static_cast<uint8_t>((77u * color.r + 150u * color.g + 29u * color.b + 128u) >> 8);
  return quantize_8bit_to_nibble(luma);
}

// 4x4 ordered (Bayer) dither threshold over the weighted-luma range (0..65535).
// A pixel whose luma is below the threshold renders black, so lighter pixels
// produce progressively sparser black dots instead of vanishing to white. The
// matrix averages to 32768, matching the conventional monochrome cut, while the
// per-pixel variation reproduces intermediate gray levels.
static uint16_t dither_threshold(uint16_t x, uint16_t y) {
  static const uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  return static_cast<uint16_t>(BAYER4[((y & 3) << 2) | (x & 3)] * 4096u + 2048u);
}

void IT8951Display::fill(Color color) {
  if (this->buffer_ == nullptr)
    return;
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  uint8_t packed = color_to_nibble(color);
  if (this->invert_colors_)
    packed = 0x0F - packed;
  uint8_t fill_byte;
  if (this->grayscale_) {
    fill_byte = static_cast<uint8_t>((packed << 4) | packed);
  } else {
    fill_byte = (packed <= 0x07) ? 0xFF : 0x00;
  }
  memset(this->buffer_, fill_byte, this->buffer_length_);
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT IT8951Display::draw_pixel_at(int x, int y, Color color) {
  if (this->buffer_ == nullptr)
    return;
  App.feed_wdt();
  if (!this->rotate_coordinates_(x, y))
    return;
  this->write_pixel_native_(static_cast<uint16_t>(x), static_cast<uint16_t>(y), color);
}

void HOT IT8951Display::write_pixel_native_(uint16_t x, uint16_t y, const Color &color) const {
  if (this->grayscale_) {
    uint8_t nibble = color_to_nibble(color);
    if (this->invert_colors_)
      nibble = static_cast<uint8_t>(0x0F - nibble);
    this->set_gray_pixel_(x, y, nibble);
  } else {
    // Rec.601 luma (see color_to_nibble). Weights sum to 257 so white maps to
    // exactly 65535, using the full 16-bit range without overflow.
    auto lum = static_cast<uint16_t>(77u * color.r + 151u * color.g + 29u * color.b);
    if (this->invert_colors_)
      lum = static_cast<uint16_t>(65535u - lum);
    // Set the bit (foreground/black) when this pixel is darker than its
    // threshold. With dithering the threshold varies per pixel so pale colours
    // render as visible texture; otherwise it's the fixed ~50% cut (r+g+b<32768).
    const uint16_t threshold = this->dithering_ ? dither_threshold(x, y) : 32768;
    this->set_mono_pixel_(x, y, lum < threshold);
  }
}

void HOT IT8951Display::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                                       ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  // A writer (e.g. LVGL) may push pixels before the framebuffer is ready or
  // after an allocation failure; ignore those rather than dereferencing null.
  if (this->buffer_ == nullptr)
    return;
  // A clipping rectangle would need a per-pixel test; that's rare for the bulk
  // blit callers (LVGL, images), so fall back to the base per-pixel path then.
  if (this->get_clipping().is_set()) {
    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  }

  const size_t line_stride = static_cast<size_t>(x_offset) + w + x_pad;  // source line length in pixels
  for (int y = 0; y < h; y++) {
    App.feed_wdt();
    size_t source_idx = (static_cast<size_t>(y_offset) + y) * line_stride + x_offset;
    for (int x = 0; x < w; x++, source_idx++) {
      uint32_t color_value;
      switch (bitness) {
        case COLOR_BITNESS_565: {
          const size_t i = source_idx * 2;
          color_value = big_endian ? (static_cast<uint32_t>(ptr[i]) << 8) | ptr[i + 1]
                                   : ptr[i] | (static_cast<uint32_t>(ptr[i + 1]) << 8);
          break;
        }
        case COLOR_BITNESS_888: {
          const size_t i = source_idx * 3;
          color_value =
              big_endian
                  ? (static_cast<uint32_t>(ptr[i]) << 16) | (static_cast<uint32_t>(ptr[i + 1]) << 8) | ptr[i + 2]
                  : ptr[i] | (static_cast<uint32_t>(ptr[i + 1]) << 8) | (static_cast<uint32_t>(ptr[i + 2]) << 16);
          break;
        }
        default:
          color_value = ptr[source_idx];
          break;
      }
      int nx = x_start + x;
      int ny = y_start + y;
      this->apply_transform_(nx, ny);
      if (nx < 0 || ny < 0 || nx >= this->width_ || ny >= this->height_)
        continue;
      this->write_pixel_native_(static_cast<uint16_t>(nx), static_cast<uint16_t>(ny),
                                ColorUtil::to_color(color_value, order, bitness));
    }
  }

  // Expand the dirty bounding box once from the transformed block corners: the
  // image of an axis-aligned rectangle under swap/mirror is still axis-aligned,
  // so its two opposite corners bound it.
  int x0 = x_start, y0 = y_start;
  int x1 = x_start + w - 1, y1 = y_start + h - 1;
  this->apply_transform_(x0, y0);
  this->apply_transform_(x1, y1);
  const int nx_lo = std::max(0, std::min(x0, x1));
  const int ny_lo = std::max(0, std::min(y0, y1));
  const int nx_hi = std::min(this->width_ - 1, std::max(x0, x1));
  const int ny_hi = std::min(this->height_ - 1, std::max(y0, y1));
  if (nx_hi >= nx_lo && ny_hi >= ny_lo) {
    this->x_low_ = clamp_at_most(this->x_low_, nx_lo);
    this->x_high_ = clamp_at_least(this->x_high_, nx_hi + 1);
    this->y_low_ = clamp_at_most(this->y_low_, ny_lo);
    this->y_high_ = clamp_at_least(this->y_high_, ny_hi + 1);
  }
}

void IT8951Display::set_mono_pixel_(uint16_t x, uint16_t y, bool value) const {
  // The monochrome framebuffer holds the exact bytes streamed to the
  // controller for the 8bpp-load / 1bpp-display trick (L_ENDIAN). Pixels are
  // grouped in 16s; on the wire the high byte (pixels 8..15) precedes the low
  // byte (pixels 0..7), and the bit index within a byte is the pixel's offset
  // (LSB = lowest x). Storing in that order lets op_xfer_rows_ copy rows
  // verbatim with no packing or byte-swapping.
  const uint16_t group = static_cast<uint16_t>(x >> 4);
  const uint8_t sub = static_cast<uint8_t>(x & 0x0F);
  const uint16_t byte_index = static_cast<uint16_t>(group * 2u + (sub < 8u ? 1u : 0u));
  const uint8_t mask = static_cast<uint8_t>(1u << (sub & 0x07));
  const uint32_t index = static_cast<uint32_t>(y) * this->row_width_ + byte_index;
  if (value) {
    this->buffer_[index] |= mask;
  } else {
    this->buffer_[index] &= static_cast<uint8_t>(~mask);
  }
}

void IT8951Display::set_gray_pixel_(uint16_t x, uint16_t y, uint8_t nibble) const {
  const uint32_t index = static_cast<uint32_t>(y) * this->row_width_ + (static_cast<uint32_t>(x) >> 1);
  uint8_t buf = this->buffer_[index];
  if (x & 0x1) {
    buf = (buf & 0xF0) | nibble;
  } else {
    buf = (buf & 0x0F) | static_cast<uint8_t>(nibble << 4);
  }
  this->buffer_[index] = buf;
}

// --- Diagnostics -------------------------------------------------------------

void IT8951Display::dump_config() {
  LOG_DISPLAY("", "IT8951 E-Paper", this);
  char force_temperature[24];
  if (this->force_temperature_set_) {
    snprintf(force_temperature, sizeof(force_temperature), "%d °C", this->force_temperature_);
  } else {
    strncpy(force_temperature, "(controller default)", sizeof(force_temperature));
    force_temperature[sizeof(force_temperature) - 1] = '\0';
  }
  ESP_LOGCONFIG(TAG,
                "  Model preset: %s"
                "\n  Dimensions: %dx%d"
                "\n  Buffer: %u bytes"
                "\n  Image buffer addr: 0x%04X%04X"
                "\n  VCOM: %.02fV (set selector 0x%04X)"
                "\n  Force temperature: %s"
                "\n  Display command: %s"
                "\n  Sleep when done: %s"
                "\n  Full update every: %u"
                "\n  Inverted colors: %s"
                "\n  Pixel format: %s"
                "\n  Reset duration: %" PRIu32 "ms",
                this->name_ != nullptr ? this->name_ : "(unknown)", this->get_width_internal(),
                this->get_height_internal(), static_cast<unsigned>(this->buffer_length_), this->img_buf_addr_h_,
                this->img_buf_addr_l_, static_cast<float>(this->vcom_) / 1000.0f, this->vcom_register_,
                force_temperature, this->use_legacy_dpy_area_ ? "DPY_AREA (0x0034, legacy)" : "DPY_BUF_AREA (0x0037)",
                YESNO(this->sleep_when_done_), this->full_update_every_, YESNO(this->invert_colors_),
                this->grayscale_ ? "4bpp grayscale" : "1bpp monochrome", this->reset_duration_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::it8951
