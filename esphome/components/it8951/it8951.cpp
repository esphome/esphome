#include "it8951.h"

#include <array>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::it8951 {

static const char *const TAG = "it8951";

// Maximum row buffer sizes for supported panel widths (up to 2048 pixels).
static constexpr size_t MAX_4BPP_ROW_BYTES = 2048 / 2;
static constexpr size_t MAX_1BPP_ROW_BYTES = 2048 / 8;

// Soft cap for time spent in a single XFER_ROWS Op so we yield back to the
// loop within one tick budget.
static constexpr uint32_t MAX_TRANSFER_TIME_MS = 10;

static uint16_t encode_uint16(uint8_t high_byte, uint8_t low_byte) {
  return static_cast<uint16_t>(high_byte) << 8 | low_byte;
}

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

bool IT8951Display::busy_pin_low_() const {
  // IT8951 Hardware Ready (HW_RDY): HIGH = ready, LOW = busy.
  return this->busy_pin_ != nullptr && !this->busy_pin_->digital_read();
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
  if (needs_hardware_ready && this->busy_pin_low_()) {
    // Signed elapsed: any pending DELAY_MS or scheduled work in the near
    // future shows up as <= 0 elapsed and won't trigger a false timeout.
    const int32_t elapsed = static_cast<int32_t>(now - this->phase_started_at_);
    if (elapsed > static_cast<int32_t>(BUSY_TIMEOUT_MS)) {
      ESP_LOGW(TAG, "Busy timeout (%dms) in phase %u, recovering", elapsed, static_cast<unsigned>(this->phase_));
      this->recover_();
    }
    return;
  }

  this->queue_.pop_front();
  this->process_op_(queued_op);
}

void IT8951Display::process_op_(const Op &op) {
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
    case OpType::CLEAR_1BPP:
      this->op_clear_1bpp_();
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
      // Always commit the chunk we just streamed bytes into. If more rows
      // remain, also queue the next chunk and another XFER_ROWS pass.
      this->enqueue_(OpType::XFER_AREA_END);
      if (!this->op_xfer_rows_()) {
        this->enqueue_(OpType::XFER_AREA_CMD);
        this->enqueue_(OpType::XFER_AREA_ARGS);
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
  this->phase_ = next;
  this->phase_started_at_ = millis();
}

void IT8951Display::advance_phase_() {
  switch (this->phase_) {
    case Phase::IDLE:
      if (this->initialised_ && this->update_pending_) {
        this->update_pending_ = false;
        this->active_mode_ = this->pending_update_mode_;
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
      this->dev_info_attempts_ = 0;
      this->width_ = this->dev_info_.panel_width;
      this->height_ = this->dev_info_.panel_height;
      this->row_width_ = static_cast<uint16_t>((static_cast<uint32_t>(this->width_) + 1) / 2);
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
      if (!this->buffer_.is_valid()) {
        if (!this->buffer_.init(this->buffer_length_)) {
          this->mark_failed(LOG_STR("Failed to allocate IT8951 framebuffer"));
          this->set_phase_(Phase::IDLE);
          return;
        }
        // Fresh buffer is zero-filled by SplitBuffer, which maps to all-black.
        // Fill with white so the first update doesn't flash a black background.
        this->buffer_.fill(this->reversed_ ? 0x00 : 0xFF);
      }
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
      this->has_grayscale_ = false;
      this->do_update_();
      UpdateMode mode = this->active_mode_;
      if (!this->prepare_update_region_(mode)) {
        ESP_LOGD(TAG, "Nothing to update");
        this->set_phase_(Phase::IDLE);
        this->advance_phase_();
        return;
      }
      this->active_mode_ = mode;
      this->set_phase_(Phase::UPDATE_TRANSFER);
      this->enqueue_update_transfer_();
      break;
    }

    case Phase::UPDATE_TRANSFER:
      this->set_phase_(Phase::UPDATE_REFRESH);
      this->enqueue_update_refresh_();
      break;

    case Phase::UPDATE_REFRESH:
      if (this->use_1bpp_) {
        this->set_phase_(Phase::UPDATE_RESTORE);
        this->enqueue_update_restore_();
      } else {
        this->set_phase_(Phase::UPDATE_SLEEP);
        this->enqueue_update_sleep_();
      }
      break;

    case Phase::UPDATE_RESTORE:
      this->set_phase_(Phase::UPDATE_SLEEP);
      this->enqueue_update_sleep_();
      break;

    case Phase::UPDATE_SLEEP:
      ESP_LOGD(TAG, "Update took %ums (mode=%u area=%ux%u@%u,%u)", millis() - this->update_started_at_,
               static_cast<unsigned>(this->active_mode_), this->area_w_, this->area_h_, this->area_x_, this->area_y_);
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

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }

  this->update_effective_transform_();
  this->reset_dirty_region_();

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
  this->transfer_row_ = 0;
  this->enqueue_(OpType::XFER_LISAR);
  this->enqueue_(OpType::XFER_AREA_CMD);
  this->enqueue_(OpType::XFER_AREA_ARGS);
  this->enqueue_(OpType::XFER_ROWS);
  // No trailing XFER_AREA_END here — XFER_ROWS handler always enqueues an
  // END for the chunk it just streamed (and queues another CMD+ARGS+ROWS
  // sequence if more rows remain).
}

void IT8951Display::enqueue_update_refresh_() {
  // Poll LUT idle: CMD(REG_RD) → WRITE_W(LUTAFSR) → READ_WORD → CHECK_LUT_IDLE
  this->enqueue_(OpType::CMD, TCON_REG_RD);
  this->enqueue_(OpType::WRITE_W, LUTAFSR);
  this->enqueue_(OpType::READ_WORD);
  this->enqueue_(OpType::CHECK_LUT_IDLE);
  if (this->use_1bpp_) {
    // Read UP1SR+2: CMD(REG_RD) → WRITE_W(UP1SR+2) → READ_WORD → SET_1BPP
    this->enqueue_(OpType::CMD, TCON_REG_RD);
    this->enqueue_(OpType::WRITE_W, static_cast<uint16_t>(UP1SR + 2));
    this->enqueue_(OpType::READ_WORD);
    this->enqueue_(OpType::SET_1BPP);
  }
  this->enqueue_(OpType::DPY_BUF_CMD);
  this->enqueue_(OpType::DPY_BUF_ARGS);
}

void IT8951Display::enqueue_update_restore_() {
  // After a 1bpp refresh, wait for LUT idle then clear UP1SR bit 2.
  // CMD(REG_RD) → WRITE_W(LUTAFSR) → READ_WORD → CHECK_LUT_IDLE
  this->enqueue_(OpType::CMD, TCON_REG_RD);
  this->enqueue_(OpType::WRITE_W, LUTAFSR);
  this->enqueue_(OpType::READ_WORD);
  this->enqueue_(OpType::CHECK_LUT_IDLE);
  // CMD(REG_RD) → WRITE_W(UP1SR+2) → READ_WORD → CLEAR_1BPP
  this->enqueue_(OpType::CMD, TCON_REG_RD);
  this->enqueue_(OpType::WRITE_W, static_cast<uint16_t>(UP1SR + 2));
  this->enqueue_(OpType::READ_WORD);
  this->enqueue_(OpType::CLEAR_1BPP);
}

void IT8951Display::enqueue_update_sleep_() {
  if (this->sleep_when_done_)
    this->enqueue_(OpType::CMD, TCON_SLEEP);
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
  const uint32_t word_count = sizeof(this->dev_info_) / sizeof(uint16_t);
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
  // Single CS transaction: WRITE preamble + 5 area-parameter words.
  uint16_t args[5];
  if (this->use_1bpp_) {
    args[0] = static_cast<uint16_t>((LDIMG_L_ENDIAN << 8) | (PIXEL_8BPP << 4));
    args[1] = static_cast<uint16_t>(this->area_x_ / 8);
    args[2] = static_cast<uint16_t>(this->area_y_ + this->transfer_row_);
    args[3] = static_cast<uint16_t>(this->area_w_ / 8);
    args[4] = static_cast<uint16_t>(this->area_h_ - this->transfer_row_);
  } else {
    args[0] = static_cast<uint16_t>((LDIMG_B_ENDIAN << 8) | (PIXEL_4BPP << 4));
    args[1] = this->area_x_;
    args[2] = static_cast<uint16_t>(this->area_y_ + this->transfer_row_);
    args[3] = this->area_w_;
    args[4] = static_cast<uint16_t>(this->area_h_ - this->transfer_row_);
  }
  this->spi_write_args_(args, 5);
}

void IT8951Display::op_xfer_area_end_() { this->spi_cmd_(TCON_LD_IMG_END); }

bool IT8951Display::op_xfer_rows_() {
  const uint32_t start_time = millis();
  const uint16_t area_x = this->area_x_;
  const uint16_t area_y = this->area_y_;
  const uint16_t area_w = this->area_w_;
  const uint16_t area_h = this->area_h_;

  // Single CS write transaction — HW_RDY was confirmed high by the loop gate.
  this->enable();
  this->write_byte16(PACKET_TYPE_WRITE);
  wait_for_hardware_ready(this->busy_pin_);

  if (this->use_1bpp_) {
    // Pack the row into 16-bit words, LSB-first to match the IT8951's
    // L_ENDIAN + MSB_FIRST SPI byte-swap behavior.
    const uint16_t words_per_row = static_cast<uint16_t>((area_w + 15) / 16);
    if (words_per_row * 2 > MAX_1BPP_ROW_BYTES) {
      ESP_LOGE(TAG, "1bpp row buffer too small (%u words)", words_per_row);
      this->disable();
      return true;
    }
    std::array<uint16_t, MAX_1BPP_ROW_BYTES / 2> row_words{};
    while (this->transfer_row_ < area_h) {
      row_words.fill(0);
      const uint16_t row_y = static_cast<uint16_t>(area_y + this->transfer_row_);
      for (uint16_t x = 0; x < area_w; x++) {
        const uint8_t nibble = this->get_pixel_nibble_(static_cast<uint16_t>(area_x + x), row_y);
        if (nibble <= 0x07)
          row_words[x / 16] |= static_cast<uint16_t>(1U << (x & 0x0F));
      }
      for (uint16_t i = 0; i < words_per_row; i++)
        row_words[i] = byteswap(row_words[i]);
      this->write_array(reinterpret_cast<const uint8_t *>(row_words.data()), words_per_row * 2);
      this->transfer_row_++;
      if (millis() - start_time >= MAX_TRANSFER_TIME_MS)
        break;
    }
  } else {
    const bool full_width = (area_x == 0 && area_w == this->width_);
    const uint16_t bytes_per_row = full_width ? this->row_width_ : static_cast<uint16_t>(area_w >> 1);
    if (bytes_per_row > MAX_4BPP_ROW_BYTES) {
      ESP_LOGE(TAG, "4bpp row buffer too small (%u bytes)", bytes_per_row);
      this->disable();
      return true;
    }
    std::array<uint8_t, MAX_4BPP_ROW_BYTES> row_buffer{};
    while (this->transfer_row_ < area_h) {
      const uint32_t row_y = area_y + this->transfer_row_;
      const uint32_t offset = row_y * this->row_width_ + (full_width ? 0 : (area_x >> 1));
      for (uint16_t i = 0; i < bytes_per_row; i++)
        row_buffer[i] = this->buffer_[offset + i];
      this->write_array(row_buffer.data(), bytes_per_row);
      this->transfer_row_++;
      if (millis() - start_time >= MAX_TRANSFER_TIME_MS)
        break;
    }
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
  this->prepend_(OpType::WRITE_REG, BGVR, static_cast<uint16_t>((uint16_t(0xFF) << 8) | 0x00));
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
  this->prepend_(OpType::WRITE_REG, static_cast<uint16_t>(UP1SR + 2), modified);
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
}

void IT8951Display::op_clear_1bpp_() {
  // read_result_ holds UP1SR+2 value. Clear bit 2 and write back.
  const uint16_t modified = static_cast<uint16_t>(this->read_result_ & ~(1U << 2));
  this->prepend_(OpType::WRITE_REG, static_cast<uint16_t>(UP1SR + 2), modified);
  this->prepend_(OpType::CMD, TCON_REG_WR, 0);
}

// --- Update prep / public API ------------------------------------------------

bool IT8951Display::prepare_update_region_(UpdateMode &mode) {
  this->partial_update_count_++;
  if (this->full_update_every_ > 0 && this->partial_update_count_ >= this->full_update_every_) {
    this->partial_update_count_ = 0;
    mode = UPDATE_MODE_GC16;
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
  } else {
    // IT8951 requires x and width to be multiples of 4 pixels (4bpp).
    // For the 1bpp trick (8BPP with area_w/8), we need area_w to be a
    // multiple of 16 so that the byte count sent (word-padded) matches
    // what the IT8951 expects.  16-alignment satisfies both constraints.
    this->x_low_ &= 0xFFF0;
    uint16_t temp_max = this->x_high_ > 0 ? static_cast<uint16_t>(this->x_high_ - 1) : 0;
    temp_max = static_cast<uint16_t>(temp_max | 0x000F);
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
  this->use_1bpp_ = this->force_1bpp_ || !this->has_grayscale_;

  this->reset_dirty_region_();

  ESP_LOGD(TAG, "Update: %ux%u@%u,%u mode=%u (%s)", width, height, x, y, static_cast<unsigned>(mode),
           this->use_1bpp_ ? "1bpp" : "4bpp");
  return true;
}

void IT8951Display::reset_dirty_region_() {
  this->x_low_ = this->width_;
  this->x_high_ = 0;
  this->y_low_ = this->height_;
  this->y_high_ = 0;
}

void IT8951Display::start_update_(UpdateMode mode) {
  if (this->phase_ == Phase::IDLE && this->initialised_) {
    this->update_started_at_ = millis();
    this->active_mode_ = mode;
    this->set_phase_(Phase::UPDATE_PREPARE);
    this->enable_loop();
    this->advance_phase_();
  } else {
    // Coalesce: latest pending mode wins.
    this->update_pending_ = true;
    this->pending_update_mode_ = mode;
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

bool IT8951Display::rotate_coordinates_(int &x, int &y) {
  if (!this->get_clipping().inside(x, y))
    return false;
  if (this->effective_transform_ & TRANSFORM_SWAP_XY)
    std::swap(x, y);
  if (this->effective_transform_ & TRANSFORM_MIRROR_X)
    x = this->width_ - x - 1;
  if (this->effective_transform_ & TRANSFORM_MIRROR_Y)
    y = this->height_ - y - 1;
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return false;
  this->x_low_ = clamp_at_most(this->x_low_, x);
  this->x_high_ = clamp_at_least(this->x_high_, x + 1);
  this->y_low_ = clamp_at_most(this->y_low_, y);
  this->y_high_ = clamp_at_least(this->y_high_, y + 1);
  return true;
}

// --- Color / drawing ---------------------------------------------------------

uint8_t IT8951Display::color_to_nibble_(const Color &color) const {
  if (color.raw_32 == 0)
    return 0x00;  // COLOR_OFF -> white (lightest)
  if (color.raw_32 == 0xFFFFFFFF)
    return 0x0F;  // COLOR_ON -> black (darkest)
  if (color.g == 0 && color.b == 0 && color.w == 0 && color.r <= 0x0F)
    return color.r;
  uint16_t gray = static_cast<uint16_t>(color.r) + color.g + color.b;
  gray /= 3;
  if (color.w > gray)
    gray = color.w;
  uint8_t nibble = static_cast<uint8_t>((gray + 8) >> 4);
  if (nibble > 0x0F)
    nibble = 0x0F;
  return nibble;
}

void IT8951Display::fill(Color color) {
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  uint8_t packed = this->color_to_nibble_(color);
  if (!this->reversed_)
    packed = 0x0F - packed;
  if (packed != 0x00 && packed != 0x0F)
    this->has_grayscale_ = true;
  const uint8_t fill_byte = static_cast<uint8_t>((packed << 4) | packed);
  this->buffer_.fill(fill_byte);
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT IT8951Display::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  uint8_t internal_color = this->color_to_nibble_(color) & 0x0F;
  if (!this->reversed_)
    internal_color = 0x0F - internal_color;
  if (internal_color != 0x00 && internal_color != 0x0F)
    this->has_grayscale_ = true;
  const uint32_t index = static_cast<uint32_t>(y) * this->row_width_ + (static_cast<uint32_t>(x) >> 1);
  uint8_t buf = this->buffer_[index];
  if (x & 0x1) {
    buf = (buf & 0xF0) | internal_color;
  } else {
    buf = (buf & 0x0F) | (internal_color << 4);
  }
  this->buffer_[index] = buf;
}

uint8_t IT8951Display::get_pixel_nibble_(uint16_t x, uint16_t y) {
  const uint32_t index = static_cast<uint32_t>(y) * this->row_width_ + (static_cast<uint32_t>(x) >> 1);
  const uint8_t byte = this->buffer_[index];
  return (x & 1U) == 0 ? (byte >> 4) : (byte & 0x0F);
}

// --- Diagnostics -------------------------------------------------------------

void IT8951Display::dump_config() {
  LOG_DISPLAY("", "IT8951 E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model preset: %s", this->name_ != nullptr ? this->name_ : "(unknown)");
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", this->get_width_internal(), this->get_height_internal());
  ESP_LOGCONFIG(TAG, "  Buffer: %u bytes in %u segment(s)", static_cast<unsigned>(this->buffer_length_),
                static_cast<unsigned>(this->buffer_.get_buffer_count()));
  ESP_LOGCONFIG(TAG, "  Image buffer addr: 0x%04X%04X", this->img_buf_addr_h_, this->img_buf_addr_l_);
  ESP_LOGCONFIG(TAG, "  VCOM: %.02fV (set selector 0x%04X)", static_cast<float>(this->vcom_) / 1000.0f,
                this->vcom_register_);
  if (this->force_temperature_set_) {
    ESP_LOGCONFIG(TAG, "  Force temperature: %d °C", this->force_temperature_);
  } else {
    ESP_LOGCONFIG(TAG, "  Force temperature: (controller default)");
  }
  ESP_LOGCONFIG(TAG, "  Display command: %s",
                this->use_legacy_dpy_area_ ? "DPY_AREA (0x0034, legacy)" : "DPY_BUF_AREA (0x0037)");
  ESP_LOGCONFIG(TAG, "  Sleep when done: %s", YESNO(this->sleep_when_done_));
  ESP_LOGCONFIG(TAG, "  Full update every: %u", this->full_update_every_);
  ESP_LOGCONFIG(TAG, "  Reversed colors: %s", YESNO(this->reversed_));
  ESP_LOGCONFIG(TAG, "  Force 1bpp: %s", YESNO(this->force_1bpp_));
  ESP_LOGCONFIG(TAG, "  Reset duration: %ums", this->reset_duration_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::it8951
