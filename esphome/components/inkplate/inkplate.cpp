#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "inkplate.h"
#include "inkplate_common.h"

#ifdef USE_ESP32

#include "esp_rom_sys.h"
#include "driver/gpio.h"

namespace esphome::inkplate {

static const char *const TAG = "inkplate";

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void InkplateParallelBase::setup() {
  size_t buf_size = (size_t) this->width_ * this->height_ / 8;

  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(buf_size);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "buffer_ alloc failed (%zu bytes)", buf_size);
    this->mark_failed();
    return;
  }
  memset(this->buffer_, 0x00, buf_size);  // all-black initial state

  this->d_memory_new_ = allocator.allocate(buf_size);
  if (this->d_memory_new_ == nullptr) {
    ESP_LOGE(TAG, "d_memory_new_ alloc failed");
    this->mark_failed();
    return;
  }
  memset(this->d_memory_new_, 0x00, buf_size);

  // Partial update diff buffer.
  size_t p_buf_size = (size_t) this->width_ * this->height_ / 4;
  this->p_buffer_ = allocator.allocate(p_buf_size);
  if (this->p_buffer_ == nullptr) {
    ESP_LOGE(TAG, "p_buffer_ alloc failed (%zu bytes)", p_buf_size);
    this->mark_failed();
    return;
  }
  memset(this->p_buffer_, 0, p_buf_size);

  // Grayscale (4bpp) draw buffer — 2 pixels per byte, init to 0xFF (white).
  size_t gs_buf_size = (size_t) this->width_ * this->height_ / 2;
  this->d_memory_4bit_ = allocator.allocate(gs_buf_size);
  if (this->d_memory_4bit_ == nullptr) {
    ESP_LOGE(TAG, "d_memory_4bit_ alloc failed (%zu bytes)", gs_buf_size);
    this->mark_failed();
    return;
  }
  memset(this->d_memory_4bit_, 0xFF, gs_buf_size);

  // DMA-capable SRAM for I2S line buffer and descriptor.
  size_t line_buf_size = (size_t) this->width_ / 4 + 16;
  this->dma_line_buf_ = (volatile uint8_t *) heap_caps_malloc(line_buf_size, MALLOC_CAP_DMA);
  this->dma_desc_ = (volatile lldesc_t *) heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
  if (this->dma_line_buf_ == nullptr || this->dma_desc_ == nullptr) {
    ESP_LOGE(TAG, "DMA alloc failed");
    this->mark_failed();
    return;
  }
  memset((void *) this->dma_line_buf_, 0, line_buf_size);

  ESP_LOGI(TAG, "setup() done — buf %zu bytes", buf_size);
  this->disable_loop();
}

// ---------------------------------------------------------------------------
// Loop / update
// ---------------------------------------------------------------------------

void InkplateParallelBase::loop() {
  if (this->is_failed())
    return;
  this->process_state_();
}

void InkplateParallelBase::update() {
  if (this->is_failed())
    return;
  if (this->state_ != STATE_IDLE) {
    ESP_LOGW(TAG, "update() skipped — busy (state %d)", (int) this->state_);
    return;
  }
  this->do_update_();
  this->update_count_++;
  this->partial_ = false;
  ESP_LOGD(TAG, "update #%d", this->update_count_);
  this->prepare_for_update_();
  this->enable_loop();
  this->set_state_(STATE_POWER_ON);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void InkplateParallelBase::on_safe_shutdown() {
  if (this->state_ == STATE_IDLE)
    return;
  ESP_LOGW(TAG, "on_safe_shutdown() mid-refresh — emergency off");
  this->state_ = STATE_IDLE;
  this->disable_loop();
  this->do_emergency_off_();
}

// ---------------------------------------------------------------------------
// Pixel drawing — 1bpp LSB-first
// ---------------------------------------------------------------------------

void InkplateParallelBase::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= this->width_ || y >= this->height_)
    return;
  if (this->grayscale_mode_) {
    if (this->d_memory_4bit_ == nullptr)
      return;
    // 3-bit grayscale: pass Color(0,0,0)–Color(7,7,7) for levels 0–7.
    uint8_t v = color.r & 0x07u;
    uint32_t byte_pos = (uint32_t) y * (this->width_ / 2) + (x / 2);
    if (x & 1) {
      this->d_memory_4bit_[byte_pos] = (this->d_memory_4bit_[byte_pos] & 0xF0u) | v;
    } else {
      this->d_memory_4bit_[byte_pos] = (this->d_memory_4bit_[byte_pos] & 0x0Fu) | (uint8_t) (v << 4u);
    }
  } else {
    uint32_t byte_pos = (uint32_t) y * (this->width_ / 8) + (x / 8);
    uint8_t bit_mask = 1u << (x % 8);
    if (color.is_on()) {
      this->buffer_[byte_pos] |= bit_mask;
    } else {
      this->buffer_[byte_pos] &= ~bit_mask;
    }
  }
}

// ---------------------------------------------------------------------------
// Public display trigger methods
// ---------------------------------------------------------------------------

void InkplateParallelBase::display_partial() {
  if (this->state_ != STATE_IDLE) {
    ESP_LOGW(TAG, "display_partial() skipped — busy (state %d)", (int) this->state_);
    return;
  }
  if (this->grayscale_mode_) {
    ESP_LOGW(TAG, "display_partial() blocked — disable grayscale mode first (set_grayscale_mode(false))");
    return;
  }
  if (this->block_partial_) {
    ESP_LOGW(TAG, "display_partial() → forced full (no prior full update)");
    this->partial_ = false;
    this->prepare_for_update_();
    this->enable_loop();
    this->set_state_(STATE_POWER_ON);
    return;
  }
  this->partial_ = true;
  this->prepare_for_update_();
  this->enable_loop();
  this->set_state_(STATE_POWER_ON);
}

void InkplateParallelBase::display_grayscale() {
  if (this->state_ != STATE_IDLE) {
    ESP_LOGW(TAG, "display_grayscale() skipped — busy (state %d)", (int) this->state_);
    return;
  }
  this->grayscale_update_ = true;
  this->partial_ = false;
  this->prepare_for_update_();
  this->enable_loop();
  this->set_state_(STATE_POWER_ON);
}

// ---------------------------------------------------------------------------
// prepare_for_update_ — reset all sub-state counters before each refresh
// ---------------------------------------------------------------------------

void InkplateParallelBase::prepare_for_update_() {
  this->pon_sub_ = PON_SETUP;
  this->poff_sub_ = POFF_VCOM_LOW;
  if (this->grayscale_update_) {
    this->trf_after_clean_ = TRF_GRAYSCALE_SEND;
    this->trf_sub_ = TRF_CLEAN;
    this->grayscale_update_ = false;  // consume — one-shot flag
  } else if (this->partial_) {
    this->trf_sub_ = TRF_PARTIAL_DIFF;
  } else {
    this->trf_sub_ = TRF_COPY_BUF;
  }
  this->sub_start_ms_ = 0;
  this->tps_pwrup_start_ms_ = 0;
  this->trf_k_ = 0;
  this->trf_step_ = 0;
  this->trf_pass_ = 0;
}

// ---------------------------------------------------------------------------
// do_power_on_step_ — non-blocking einkOn() + TPS65186 powerUp()
// ---------------------------------------------------------------------------

bool InkplateParallelBase::do_power_on_step_() {
  uint32_t now = App.get_loop_component_start_time();

  switch (this->pon_sub_) {
    case PON_SETUP:
      // Configure pin modes for direct GPIO control pins.
      this->pin_ckv_->pin_mode(gpio::FLAG_OUTPUT);
      this->pin_sph_->pin_mode(gpio::FLAG_OUTPUT);
      this->pin_le_->pin_mode(gpio::FLAG_OUTPUT);
      // Configure expander control pins.
      this->pin_oe_->pin_mode(gpio::FLAG_OUTPUT);
      this->pin_gmod_->pin_mode(gpio::FLAG_OUTPUT);
      this->pin_spv_->pin_mode(gpio::FLAG_OUTPUT);
      // Route I2S1 to data/clock GPIO pins (GPIO matrix).
      this->i2s_pin_route_();
      // Start I2S clock output (matches pinsAsOutputs() tx_stop_en=1).
      I2S1.conf1.tx_stop_en = 1;
      // Set initial EPD control states (matches Arduino einkOn()).
      INK_LE_CLEAR();
      INK_SPH_SET();
      this->pin_gmod_->digital_write(true);
      this->pin_spv_->digital_write(true);
      INK_CKV_CLEAR();
      this->pin_oe_->digital_write(false);
      this->pon_sub_ = PON_TPS_WAKEUP_SET;
      return false;

    case PON_TPS_WAKEUP_SET:
      this->pin_wakeup_->digital_write(true);
      this->sub_start_ms_ = now;
      this->pon_sub_ = PON_TPS_WAKEUP_WAIT;
      return false;

    case PON_TPS_WAKEUP_WAIT:
      if (now - this->sub_start_ms_ < 5)
        return false;  // 5 ms from Arduino powerUp()
      this->pon_sub_ = PON_TPS_ENABLE;
      return false;

    case PON_TPS_ENABLE:
      this->tps_write_reg_(0x01, 0x20);  // ENABLE — enable rails
      this->tps_write_reg_(0x09, 0xE4);  // UPSEQ0 — power-up sequence
      this->tps_write_reg_(0x0B, 0x1B);  // DWNSEQ0 — power-down sequence
      this->tps_pwrup_start_ms_ = now;
      this->pon_sub_ = PON_TPS_PWRUP_SET;
      return false;

    case PON_TPS_PWRUP_SET:
      this->pin_pwrup_->digital_write(true);
      this->pon_sub_ = PON_TPS_GOOD_POLL;
      return false;

    case PON_TPS_GOOD_POLL: {
      uint8_t pg = 0;
      this->tps_read_reg_(0x0F, &pg);
      if (pg == TPS_PWR_GOOD_OK) {
        this->pon_sub_ = PON_TPS_VCOM_SET;
        return false;
      }
      if (now - this->tps_pwrup_start_ms_ > 250) {
        ESP_LOGW(TAG, "TPS65186 power-good timeout (pg=0x%02X, missing=0x%02X)", pg,
                 (uint8_t) (TPS_PWR_GOOD_OK ^ (pg & TPS_PWR_GOOD_OK)));
        this->pon_sub_ = PON_TPS_VCOM_SET;
      }
      return false;
    }

    case PON_TPS_VCOM_SET:
      this->pin_vcom_->digital_write(true);
      this->pin_oe_->digital_write(true);  // OE_SET
      this->pon_sub_ = PON_DONE;
      return true;

    case PON_DONE:
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// do_power_off_step_ — non-blocking einkOff() + TPS65186 powerDown()
// ---------------------------------------------------------------------------

bool InkplateParallelBase::do_power_off_step_() {
  uint32_t now = App.get_loop_component_start_time();

  switch (this->poff_sub_) {
    case POFF_VCOM_LOW:
      this->pin_vcom_->digital_write(false);
      this->poff_sub_ = POFF_PWRUP_LOW;
      return false;

    case POFF_PWRUP_LOW:
      this->pin_pwrup_->digital_write(false);
      this->sub_start_ms_ = now;
      this->poff_sub_ = POFF_WAIT_RAILS;
      return false;

    case POFF_WAIT_RAILS: {
      uint8_t pg = 0xFF;
      this->tps_read_reg_(0x0F, &pg);
      if (pg == 0 || (now - this->sub_start_ms_ > 250))
        this->poff_sub_ = POFF_WAKEUP_LOW;
      return false;
    }

    case POFF_WAKEUP_LOW:
      this->pin_wakeup_->digital_write(false);
      this->tps_write_reg_(0x01, 0x00);  // ENABLE — disable all rails
      this->poff_sub_ = POFF_OE_GMOD;
      return false;

    case POFF_OE_GMOD:
      this->pin_oe_->digital_write(false);
      this->pin_gmod_->digital_write(false);
      INK_LE_CLEAR();
      INK_CKV_CLEAR();
      INK_SPH_CLEAR();
      this->pin_spv_->digital_write(false);
      this->poff_sub_ = POFF_I2S_STOP;
      return false;

    case POFF_I2S_STOP:
      I2S1.conf1.tx_stop_en = 0;  // stop BCK/CL clock output
      this->i2s_pin_release_();
      this->poff_sub_ = POFF_DONE;
      return true;

    case POFF_DONE:
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// do_emergency_off_ — best-effort power kill on OTA / reboot mid-refresh
// ---------------------------------------------------------------------------

void InkplateParallelBase::do_emergency_off_() {
  I2S1.conf1.tx_stop_en = 0;
  this->pin_oe_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_oe_->digital_write(false);
  this->pin_vcom_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_vcom_->digital_write(false);
  this->pin_pwrup_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_pwrup_->digital_write(false);
  this->pin_wakeup_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_wakeup_->digital_write(false);
  ESP_LOGW(TAG, "emergency off");
}

// ---------------------------------------------------------------------------
// TPS65186 I2C helpers
// ---------------------------------------------------------------------------

bool InkplateParallelBase::tps_write_reg_(uint8_t reg, uint8_t data) {
  if (this->write_register(reg, &data, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "TPS65186 write reg 0x%02X failed", reg);
    return false;
  }
  return true;
}

bool InkplateParallelBase::tps_read_reg_(uint8_t reg, uint8_t *out) {
  return this->read_register(reg, out, 1) == i2c::ERROR_OK;
}

// ---------------------------------------------------------------------------
// tps_begin_ — one-time TPS65186 sequencer register init (called in board setup)
// Mirrors TPS65186::begin() from the Arduino library.
// ---------------------------------------------------------------------------

void InkplateParallelBase::tps_begin_() {
  this->pin_wakeup_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_pwrup_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_pwrup_->digital_write(false);  // PCAL6416A output register defaults HIGH — force LOW before TPS init
  this->pin_vcom_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_vcom_->digital_write(false);  // same
  if (this->pin_gpio0_enable_) {
    this->pin_gpio0_enable_->pin_mode(gpio::FLAG_OUTPUT);
    this->pin_gpio0_enable_->digital_write(!this->gpio0_enable_low_);
  }
  this->pin_wakeup_->digital_write(true);
  delay(1);
  // Write UPSEQ0(0x09)=0x1B, UPSEQ1(0x0A)=0x00, DWNSEQ0(0x0B)=0x1B, DWNSEQ1(0x0C)=0x00
  const uint8_t seq[4] = {0x1B, 0x00, 0x1B, 0x00};
  if (this->write_register(0x09, seq, 4) != i2c::ERROR_OK)
    ESP_LOGE(TAG, "TPS65186 sequencer init failed — I2C error");
  delay(1);
  this->pin_wakeup_->digital_write(false);
}

// ---------------------------------------------------------------------------
// i2s_init_ — one-time I2S1 peripheral init (called in board setup)
// Mirrors UtilI2S::I2SInit() from the Arduino library.
// ---------------------------------------------------------------------------

void InkplateParallelBase::i2s_init_() {
  periph_module_enable(PERIPH_I2S1_MODULE);
  periph_module_reset(PERIPH_I2S1_MODULE);

  I2S1.conf.rx_fifo_reset = 1;
  I2S1.conf.rx_fifo_reset = 0;
  I2S1.conf.tx_fifo_reset = 1;
  I2S1.conf.tx_fifo_reset = 0;
  I2S1.lc_conf.in_rst = 1;
  I2S1.lc_conf.in_rst = 0;
  I2S1.lc_conf.out_rst = 1;
  I2S1.lc_conf.out_rst = 0;
  I2S1.conf.rx_reset = 1;
  I2S1.conf.tx_reset = 1;
  I2S1.conf.rx_reset = 0;
  I2S1.conf.tx_reset = 0;

  // LCD parallel mode — 8-bit data output.
  I2S1.conf2.val = 0;
  I2S1.conf2.lcd_en = 1;
  I2S1.conf2.lcd_tx_wrx2_en = 1;
  I2S1.conf2.lcd_tx_sdx2_en = 0;

  I2S1.sample_rate_conf.val = 0;
  I2S1.sample_rate_conf.rx_bits_mod = 8;
  I2S1.sample_rate_conf.tx_bits_mod = 8;
  I2S1.sample_rate_conf.rx_bck_div_num = 2;
  I2S1.sample_rate_conf.tx_bck_div_num = 2;

  // Clock: 80 MHz APB / 5 ≈ 16 MHz BCK.
  I2S1.clkm_conf.val = 0;
  I2S1.clkm_conf.clka_en = 0;
  I2S1.clkm_conf.clkm_div_b = 0;
  I2S1.clkm_conf.clkm_div_a = 1;
  I2S1.clkm_conf.clkm_div_num = 5;

  I2S1.fifo_conf.val = 0;
  I2S1.fifo_conf.rx_fifo_mod_force_en = 1;
  I2S1.fifo_conf.tx_fifo_mod_force_en = 1;
  I2S1.fifo_conf.tx_fifo_mod = 1;
  I2S1.fifo_conf.rx_data_num = 1;
  I2S1.fifo_conf.tx_data_num = 1;
  I2S1.fifo_conf.dscr_en = 1;

  I2S1.conf1.val = 0;
  I2S1.conf1.tx_stop_en = 0;  // clock off initially; enabled in PON_SETUP
  I2S1.conf1.tx_pcm_bypass = 1;

  I2S1.conf_chan.val = 0;
  I2S1.conf_chan.tx_chan_mod = 1;
  I2S1.conf_chan.rx_chan_mod = 1;

  I2S1.conf.tx_right_first = 0;
  I2S1.conf.rx_right_first = 0;
  I2S1.timing.val = 0;

  // Pre-configure DMA descriptor (size and buf pointer never change).
  size_t line_size = (size_t) this->width_ / 4 + 16;
  this->dma_desc_->size = line_size;
  this->dma_desc_->length = line_size;
  this->dma_desc_->sosf = 1;
  this->dma_desc_->owner = 1;
  this->dma_desc_->qe.stqe_next = nullptr;
  this->dma_desc_->eof = 1;
  this->dma_desc_->buf = (uint8_t *) this->dma_line_buf_;
  this->dma_desc_->offset = 0;
}

// ---------------------------------------------------------------------------
// i2s_pin_route_ — connect I2S1 signals to Inkplate GPIO pins via GPIO matrix
// Mirrors UtilI2S::setI2S1pin() + pinsAsOutputs() from the Arduino library.
// ---------------------------------------------------------------------------

void InkplateParallelBase::i2s_pin_route_() {
  // Set GPIO direction + route I2S1 signal through GPIO matrix.
  auto route = [](uint32_t pin, uint32_t func, uint32_t mux_reg) {
    GPIO.func_out_sel_cfg[pin].func_sel = func;
    GPIO.func_out_sel_cfg[pin].inv_sel = 0;
    GPIO.func_out_sel_cfg[pin].oen_sel = 0;
    if (pin < 32) {
      GPIO.enable_w1ts = (1u << pin);
    } else {
      GPIO.enable1_w1ts.data = (1u << (pin - 32));
    }
    REG_WRITE(mux_reg, (3u << FUN_DRV_S) | (2u << MCU_SEL_S));  // NOLINT(clang-analyzer-core.FixedAddressDereference)
  };

  route(0, I2S1O_BCK_OUT_IDX, IO_MUX_GPIO0_REG);  // CL (clock)
  route(4, I2S1O_DATA_OUT0_IDX, IO_MUX_GPIO4_REG);
  route(5, I2S1O_DATA_OUT1_IDX, IO_MUX_GPIO5_REG);
  route(18, I2S1O_DATA_OUT2_IDX, IO_MUX_GPIO18_REG);
  route(19, I2S1O_DATA_OUT3_IDX, IO_MUX_GPIO19_REG);
  route(23, I2S1O_DATA_OUT4_IDX, IO_MUX_GPIO23_REG);
  route(25, I2S1O_DATA_OUT5_IDX, IO_MUX_GPIO25_REG);
  route(26, I2S1O_DATA_OUT6_IDX, IO_MUX_GPIO26_REG);
  route(27, I2S1O_DATA_OUT7_IDX, IO_MUX_GPIO27_REG);
}

// ---------------------------------------------------------------------------
// i2s_pin_release_ — set I2S data/clock GPIO pins to high-Z
// Mirrors pinsZstate() from the Arduino library.
// ---------------------------------------------------------------------------

void InkplateParallelBase::i2s_pin_release_() {
  static const uint8_t I2S_PINS[] = {0, 4, 5, 18, 19, 23, 25, 26, 27};
  for (uint8_t pin : I2S_PINS) {
    // Disconnect from GPIO matrix by routing to "constant 0" output signal.
    GPIO.func_out_sel_cfg[pin].func_sel = SIG_GPIO_OUT_IDX;
    gpio_set_direction((gpio_num_t) pin, GPIO_MODE_INPUT);
  }
}

// ---------------------------------------------------------------------------
// vscan_start_ — vertical scan start handshake
// ---------------------------------------------------------------------------

void InkplateParallelBase::vscan_start_() {
  INK_CKV_SET();
  esp_rom_delay_us(7);
  this->pin_spv_->digital_write(false);  // SPV_CLEAR
  esp_rom_delay_us(10);
  INK_CKV_CLEAR();
  esp_rom_delay_us(0);
  INK_CKV_SET();
  esp_rom_delay_us(8);
  this->pin_spv_->digital_write(true);  // SPV_SET
  esp_rom_delay_us(10);
  INK_CKV_CLEAR();
  esp_rom_delay_us(0);
  INK_CKV_SET();
  esp_rom_delay_us(18);
  INK_CKV_CLEAR();
  esp_rom_delay_us(0);
  INK_CKV_SET();
  esp_rom_delay_us(18);
  INK_CKV_CLEAR();
  esp_rom_delay_us(0);
  INK_CKV_SET();
}

// ---------------------------------------------------------------------------
// vscan_end_ — latch current row into the EPD shift register
// ---------------------------------------------------------------------------

void InkplateParallelBase::vscan_end_() {
  INK_CKV_CLEAR();
  INK_LE_SET();
  INK_LE_CLEAR();
  esp_rom_delay_us(0);
}

// ---------------------------------------------------------------------------
// send_line_i2s_ — send dma_line_buf_ for one EPD row via I2S DMA
// Mirrors UtilI2S::sendDataI2S() from the Arduino library.
// ---------------------------------------------------------------------------

void InkplateParallelBase::send_line_i2s_() {
  // Stop any leftover transmission.
  I2S1.out_link.stop = 1;
  I2S1.out_link.start = 0;
  I2S1.conf.tx_start = 0;

  // Reset FIFO and DMA before each line.
  I2S1.conf.tx_fifo_reset = 1;
  I2S1.conf.tx_fifo_reset = 0;
  I2S1.lc_conf.out_rst = 1;
  I2S1.lc_conf.out_rst = 0;
  I2S1.conf.tx_reset = 1;
  I2S1.conf.tx_reset = 0;

  I2S1.lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
  I2S1.out_link.addr = (uint32_t) (this->dma_desc_) & 0x000FFFFF;
  I2S1.out_link.start = 1;

  INK_SPH_CLEAR();  // begin row
  INK_CKV_SET();
  I2S1.conf.tx_start = 1;

  // Busy-wait for DMA EOF (~100 µs at 16 MHz). Timeout guards against hardware stall.
  uint32_t deadline = esp_timer_get_time() + 5000;  // 5 ms max per line
  while (!I2S1.int_raw.out_total_eof && esp_timer_get_time() < deadline)
    ;
  if (!I2S1.int_raw.out_total_eof)
    ESP_LOGE(TAG, "I2S DMA timeout — line not sent");

  INK_SPH_SET();
  I2S1.int_clr.val = I2S1.int_raw.val;
  I2S1.out_link.stop = 1;
  I2S1.out_link.start = 0;
}

// ---------------------------------------------------------------------------
// clean_data_byte — default: uses clean_seq_ pointer set by subclass constructor.
// Inkplate10 overrides this to select between two sequences.
// ---------------------------------------------------------------------------

uint8_t InkplateParallelBase::clean_data_byte() const {
  if (!this->clean_seq_)
    return 0;
  switch (this->clean_seq_[this->trf_step_].c) {
    case 0:
      return 0b10101010;
    case 1:
      return 0b01010101;
    case 2:
      return 0b00000000;
    case 3:
      return 0b11111111;
    default:
      return 0;
  }
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

void InkplateParallelBase::set_state_(State s) {
  ESP_LOGD(TAG, "state %d → %d", (int) this->state_, (int) s);
  this->state_ = s;
  this->state_start_ms_ = App.get_loop_component_start_time();
  if (s == STATE_IDLE)
    this->disable_loop();
}

void InkplateParallelBase::process_state_() {
  switch (this->state_) {
    case STATE_IDLE:
      return;

    case STATE_POWER_ON:
      if (!this->do_power_on_step_())
        return;
      this->set_state_(STATE_TRANSFER);
      break;

    case STATE_TRANSFER:
      if (!this->do_transfer_step())
        return;
      this->set_state_(STATE_POWER_OFF);
      break;

    case STATE_POWER_OFF:
      if (!this->do_power_off_step_())
        return;
      this->set_state_(STATE_IDLE);
      break;
  }

  App.feed_wdt();
}

// ---------------------------------------------------------------------------
// do_transfer_step — dispatches common TRF cases; board-specific cases fall
// through to do_board_transfer_step().
// ---------------------------------------------------------------------------

bool InkplateParallelBase::do_transfer_step() {
  switch (this->trf_sub_) {
    case TRF_COPY_BUF:
      memcpy(this->d_memory_new_, this->buffer_, (size_t) this->width_ * this->height_ / 8);
      this->trf_after_clean_ = TRF_DARK;
      this->trf_sub_ = TRF_CLEAN;
      this->trf_step_ = 0;
      this->trf_pass_ = 0;
      return false;

    case TRF_CLEAN: {
      if (!this->clean_seq_ || !this->clean_seq_len_)
        return false;
      vscan_start_();
      uint8_t data = this->clean_data_byte();
      memset((void *) this->dma_line_buf_, data, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_pass_++;
      if (this->trf_pass_ >= this->clean_seq_[this->trf_step_].rep) {
        this->trf_pass_ = 0;
        this->trf_step_++;
      }
      if (this->trf_step_ >= this->clean_seq_len_) {
        this->trf_sub_ = this->trf_after_clean_;
        this->trf_k_ = 0;
      }
      return false;
    }

    case TRF_PARTIAL_DIFF: {
      const size_t buf_bytes = (size_t) this->width_ * this->height_ / 8;
      const uint8_t *old_f = this->d_memory_new_ + buf_bytes - 1;
      const uint8_t *new_f = this->buffer_ + buf_bytes - 1;
      uint8_t *pb = this->p_buffer_ + (size_t) this->width_ * this->height_ / 4 - 1;
      for (size_t pos = 0; pos < buf_bytes; pos++) {
        uint8_t o = *old_f--;
        uint8_t n = *new_f--;
        uint8_t diffw = o & ~n;
        uint8_t diffb = ~o & n;
        *pb-- = INKPLATE_LUTW[diffw >> 4] & INKPLATE_LUTB[diffb >> 4];
        *pb-- = INKPLATE_LUTW[diffw & 0x0F] & INKPLATE_LUTB[diffb & 0x0F];
      }
      this->trf_sub_ = TRF_PARTIAL_SEND;
      this->trf_k_ = 0;
      return false;
    }

    case TRF_PARTIAL_CLEAN_DISC: {
      vscan_start_();
      memset((void *) this->dma_line_buf_, 0x00, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= 2) {
        this->trf_sub_ = TRF_PARTIAL_CLEAN_SKIP;
        this->trf_k_ = 0;
      }
      return false;
    }

    case TRF_GRAYSCALE_FINAL_CLEAN: {
      vscan_start_();
      memset((void *) this->dma_line_buf_, 0xFF, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_sub_ = TRF_FINAL_VSCAN;
      return false;
    }

    case TRF_FINAL_VSCAN:
      vscan_start_();
      if (!this->partial_)
        this->block_partial_ = false;
      this->trf_sub_ = TRF_DONE;
      return true;

    case TRF_DONE:
      return true;

    default:
      return this->do_board_transfer_step();
  }
}

// ---------------------------------------------------------------------------
// do_board_transfer_step — standard (16-aligned width) implementations.
// Inkplate4 overrides all cases; Inkplate5 overrides TRF_PARTIAL_CLEAN_SKIP only.
// ---------------------------------------------------------------------------

bool InkplateParallelBase::do_board_transfer_step() {
  switch (this->trf_sub_) {
    case TRF_DARK: {
      const uint8_t *dmp = this->d_memory_new_ + (size_t) this->width_ * this->height_ / 8 - 1;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int n = 0; n < this->width_ / 4; n += 4) {
          uint8_t dram1 = *dmp;
          uint8_t dram2 = *(dmp - 1);
          this->dma_line_buf_[n] = INKPLATE_LUTB[(dram2 >> 4) & 0x0F];
          this->dma_line_buf_[n + 1] = INKPLATE_LUTB[dram2 & 0x0F];
          this->dma_line_buf_[n + 2] = INKPLATE_LUTB[(dram1 >> 4) & 0x0F];
          this->dma_line_buf_[n + 3] = INKPLATE_LUTB[dram1 & 0x0F];
          dmp -= 2;
        }
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= this->dark_phases_)
        this->trf_sub_ = TRF_LUT2;
      return false;
    }

    case TRF_LUT2: {
      const uint8_t *dmp = this->d_memory_new_ + (size_t) this->width_ * this->height_ / 8 - 1;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int n = 0; n < this->width_ / 4; n += 4) {
          uint8_t dram1 = *dmp;
          uint8_t dram2 = *(dmp - 1);
          this->dma_line_buf_[n] = INKPLATE_LUT2[(dram2 >> 4) & 0x0F];
          this->dma_line_buf_[n + 1] = INKPLATE_LUT2[dram2 & 0x0F];
          this->dma_line_buf_[n + 2] = INKPLATE_LUT2[(dram1 >> 4) & 0x0F];
          this->dma_line_buf_[n + 3] = INKPLATE_LUT2[dram1 & 0x0F];
          dmp -= 2;
        }
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_sub_ = TRF_ZERO;
      return false;
    }

    case TRF_ZERO: {
      vscan_start_();
      memset((void *) this->dma_line_buf_, 0, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_sub_ = TRF_FINAL_VSCAN;
      return false;
    }

    case TRF_PARTIAL_SEND: {
      const uint8_t *pb = this->p_buffer_ + (size_t) this->width_ * this->height_ / 4 - 1;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int j = 0; j < this->width_ / 4; j += 4) {
          this->dma_line_buf_[j + 2] = *pb--;
          this->dma_line_buf_[j + 3] = *pb--;
          this->dma_line_buf_[j] = *pb--;
          this->dma_line_buf_[j + 1] = *pb--;
        }
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= this->partial_phases_) {
        this->trf_sub_ = TRF_PARTIAL_CLEAN_DISC;
        this->trf_k_ = 0;
      }
      return false;
    }

    case TRF_PARTIAL_CLEAN_SKIP: {
      vscan_start_();
      memset((void *) this->dma_line_buf_, 0xFF, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      memcpy(this->d_memory_new_, this->buffer_, (size_t) this->width_ * this->height_ / 8);
      this->trf_sub_ = TRF_FINAL_VSCAN;
      return false;
    }

    case TRF_GRAYSCALE_SEND: {
      const uint8_t *dp = this->d_memory_4bit_ + (size_t) this->width_ * this->height_ / 2;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int j = 0; j < this->width_ / 4; j += 4) {
          uint8_t pix_hi, pix_lo;
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j + 2] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j + 3] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j + 1] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
        }
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= this->grayscale_phases_) {
        this->trf_sub_ = TRF_GRAYSCALE_FINAL_CLEAN;
        this->trf_k_ = 0;
      }
      return false;
    }

    default:
      return false;
  }
}

}  // namespace esphome::inkplate

#endif  // USE_ESP32
