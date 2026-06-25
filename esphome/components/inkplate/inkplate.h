#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/display/display_buffer.h"

#ifdef USE_ESP32

// ESP32 I2S / DMA / GPIO matrix headers
#include "esp_private/periph_ctrl.h"
#include "rom/lldesc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"

#include <cinttypes>

namespace esphome::inkplate {

// InkplateParallelBase — non-blocking parallel e-paper driver base class.
//
// Owns two 1bpp framebuffers:
//   buffer_      (DisplayBuffer member) — draw buffer; users draw here via lambda.
//   d_memory_new_ — shadow of the last frame pushed to the panel (reserved for
//                   partial update; currently always mirrors buffer_ on each refresh).
//
// Two additional framebuffers allocated by base setup():
//   p_buffer_       — partial-update diff buffer (LUTW & LUTB pre-mixed)
//   d_memory_4bit_  — 4bpp grayscale draw buffer (2 pixels per byte, init white)
//
// Inherits i2c::I2CDevice for direct TPS65186 PMIC register access (address 0x48).
//
// Refresh state machine:
//   POWER_ON → TRANSFER → POWER_OFF → IDLE
//
// Parallel Inkplates have no panel BUSY line and no SPI controller; timing is
// driven by the ESP32 I2S DMA peripheral.
class InkplateParallelBase : public display::DisplayBuffer, public i2c::I2CDevice {
 public:
  InkplateParallelBase(int width, int height, int dark_phases, int partial_phases, int grayscale_phases)
      : width_(width),
        height_(height),
        dark_phases_(dark_phases),
        partial_phases_(partial_phases),
        grayscale_phases_(grayscale_phases) {}

  void setup() override;
  void loop() override;
  void update() override;
  void on_safe_shutdown() override;

  void set_full_update_every(int n) { this->full_update_every_ = n; }

  // Switch between 1-bit (buffer_) and grayscale (d_memory_4bit_) draw routing.
  void set_grayscale_mode(bool enable) { this->grayscale_mode_ = enable; }

  void set_pin_ckv(GPIOPin *p) { this->pin_ckv_ = p; }
  void set_pin_sph(GPIOPin *p) { this->pin_sph_ = p; }
  void set_pin_le(GPIOPin *p) { this->pin_le_ = p; }
  void set_pin_oe(GPIOPin *p) { this->pin_oe_ = p; }
  void set_pin_gmod(GPIOPin *p) { this->pin_gmod_ = p; }
  void set_pin_spv(GPIOPin *p) { this->pin_spv_ = p; }
  void set_pin_wakeup(GPIOPin *p) { this->pin_wakeup_ = p; }
  void set_pin_pwrup(GPIOPin *p) { this->pin_pwrup_ = p; }
  void set_pin_vcom(GPIOPin *p) { this->pin_vcom_ = p; }
  void set_pin_gpio0_enable(GPIOPin *p) { this->pin_gpio0_enable_ = p; }
  void set_gpio0_enable_low(bool v) { this->gpio0_enable_low_ = v; }

  display::DisplayType get_display_type() override {
    return this->grayscale_mode_ ? display::DisplayType::DISPLAY_TYPE_GRAYSCALE
                                 : display::DisplayType::DISPLAY_TYPE_BINARY;
  }

  bool is_refreshing() const { return this->state_ != STATE_IDLE; }

  void display_partial();
  void display_grayscale();

 protected:
  struct CleanStep {
    uint8_t c;
    uint8_t rep;
  };

  enum State {
    STATE_IDLE,
    STATE_POWER_ON,
    STATE_TRANSFER,
    STATE_POWER_OFF,
  };

  enum PowerOnSub {
    PON_SETUP,
    PON_TPS_WAKEUP_SET,
    PON_TPS_WAKEUP_WAIT,
    PON_TPS_ENABLE,
    PON_TPS_PWRUP_SET,
    PON_TPS_GOOD_POLL,
    PON_TPS_VCOM_SET,
    PON_DONE,
  };

  enum PowerOffSub {
    POFF_VCOM_LOW,
    POFF_PWRUP_LOW,
    POFF_WAIT_RAILS,
    POFF_WAKEUP_LOW,
    POFF_OE_GMOD,
    POFF_I2S_STOP,
    POFF_DONE,
  };

  // TRF_FINAL_SKIP is used by Inkplate4 only; other boards never enter that case.
  enum TransferSub {
    TRF_COPY_BUF,
    TRF_CLEAN,
    TRF_DARK,
    TRF_LUT2,
    TRF_ZERO,
    TRF_FINAL_SKIP,
    TRF_PARTIAL_DIFF,
    TRF_PARTIAL_SEND,
    TRF_PARTIAL_CLEAN_DISC,
    TRF_PARTIAL_CLEAN_SKIP,
    TRF_GRAYSCALE_SEND,
    TRF_GRAYSCALE_FINAL_CLEAN,
    TRF_FINAL_VSCAN,
    TRF_DONE,
  };

  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }

  // 1bpp draw: pixel (x,y) → buffer_[y*(width/8) + x/8] bit (x%8), LSB-first.
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  // --- virtual hardware interface ---

  // Called once before each refresh cycle to reset sub-state counters.
  void prepare_for_update_();

  // GPIO init + I2S pin routing + TPS65186 power-up sequence.
  // Called every loop() tick in STATE_POWER_ON; return true when done.
  bool do_power_on_step_();

  // Send pixel data via I2S DMA (clean + waveform phases).
  // Called every loop() tick in STATE_TRANSFER; return true when done.
  // Base handles common cases; board-specific cases dispatched to do_board_transfer_step().
  virtual bool do_transfer_step();

  // Board-specific transfer cases (TRF_DARK, TRF_LUT2, TRF_ZERO, TRF_PARTIAL_SEND,
  // TRF_PARTIAL_CLEAN_SKIP, TRF_GRAYSCALE_SEND). Base provides standard (16-aligned width)
  // implementation; boards override only for quirks (remainder bytes, alternate sequencing).
  virtual bool do_board_transfer_step();

  // TPS65186 power-down + I2S clock/GPIO release.
  // Called every loop() tick in STATE_POWER_OFF; return true when done.
  bool do_power_off_step_();

  // Emergency hardware power-off on OTA / reboot mid-refresh.
  void do_emergency_off_();

  // clean_data_byte() default: reads from clean_seq_ pointer set by subclass constructor.
  // Inkplate10 overrides to select between two sequences based on update path.
  virtual uint8_t clean_data_byte() const;

  // TPS65186 I2C helpers (via i2c::I2CDevice registers at address 0x48).
  bool tps_write_reg_(uint8_t reg, uint8_t data);
  bool tps_read_reg_(uint8_t reg, uint8_t *out);

  // One-time hardware init — called by each board's setup().
  void tps_begin_();
  void i2s_init_();
  void i2s_pin_route_();
  void i2s_pin_release_();

  // Per-line EPD timing — called from board do_transfer_step() implementations.
  void vscan_start_();
  void vscan_end_();
  void send_line_i2s_();

  void set_state_(State s);
  void process_state_();

  // --- shared data ---
  int width_{0}, height_{0};

  // Control pins — direct ESP32 GPIO
  GPIOPin *pin_ckv_{nullptr};
  GPIOPin *pin_sph_{nullptr};
  GPIOPin *pin_le_{nullptr};

  // Control pins — PCAL6416A expander
  GPIOPin *pin_oe_{nullptr};
  GPIOPin *pin_gmod_{nullptr};
  GPIOPin *pin_spv_{nullptr};
  GPIOPin *pin_wakeup_{nullptr};
  GPIOPin *pin_pwrup_{nullptr};
  GPIOPin *pin_vcom_{nullptr};
  GPIOPin *pin_gpio0_enable_{nullptr};
  bool gpio0_enable_low_{false};

  int full_update_every_{1};
  int update_count_{0};
  bool partial_{false};
  bool grayscale_mode_{false};

  int dark_phases_{0};
  int partial_phases_{0};
  int grayscale_phases_{0};

  // Framebuffers
  // buffer_         — 1bpp draw buffer (DisplayBuffer base member)
  // d_memory_new_   — 1bpp shadow of last displayed frame
  // p_buffer_       — partial update diff buffer
  // d_memory_4bit_  — 4bpp grayscale draw buffer
  uint8_t *d_memory_new_{nullptr};
  uint8_t *p_buffer_{nullptr};
  uint8_t *d_memory_4bit_{nullptr};

  // Grayscale LUTs — allocated by board setup()
  uint8_t *glut_{nullptr};
  uint8_t *glut2_{nullptr};

  // I2S DMA line buffer and descriptor (DMA-capable SRAM)
  volatile uint8_t *dma_line_buf_{nullptr};
  volatile lldesc_t *dma_desc_{nullptr};

  State state_{STATE_IDLE};
  uint32_t state_start_ms_{0};

  // Sub-state for power-on / power-off / transfer state machines
  PowerOnSub pon_sub_{PON_SETUP};
  PowerOffSub poff_sub_{POFF_VCOM_LOW};
  TransferSub trf_sub_{TRF_COPY_BUF};
  TransferSub trf_after_clean_{TRF_DARK};
  uint32_t sub_start_ms_{0};
  uint32_t tps_pwrup_start_ms_{0};
  bool block_partial_{true};
  bool grayscale_update_{false};
  int trf_k_{0};
  size_t trf_step_{0};
  size_t trf_pass_{0};

  // Pointer to board-specific clean sequence — set in subclass constructor.
  const CleanStep *clean_seq_{nullptr};
  size_t clean_seq_len_{0};
};

}  // namespace esphome::inkplate

#endif  // USE_ESP32
