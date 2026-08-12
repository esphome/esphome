#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Soldered Inkplate 13 Spectra: 1200x1600 6-color (black/white/yellow/red/blue/green)
 * e-paper, dual-chip controller (13.3" E Ink Spectra 6 panel). The panel is split into
 * two halves, each with its own chip-select: CS (primary, left half of every row) and
 * CS1 (secondary, right half). BS0/BS1 select the controllers' interface mode.
 *
 * GPIO power-on bring-up (all-pins-low, then IO setup + PWR_EN, then an RST pulse) runs
 * inside reset(), not initialise(). reset()/RESET_END is the one state EPaperBase never
 * busy-gates (see epaper_spi.h), so this sequence completes before the panel is powered
 * -- and before power-on, BUSY reads the same level as "genuinely busy", so gating on it
 * there would hang forever. initialise() only starts once the panel is actually powered,
 * so its busy-pin checks reflect a real signal.
 *
 * Partial update sends a sub-rectangle instead of the full buffer: each chip gets a PTLW
 * (partial window) command describing its slice of the region, then only those rows/bytes.
 * The panel requires both chips to complete a full CMD66->PTLW->DTM cycle on every partial
 * refresh, so a chip untouched by the requested rectangle still gets a null (4x4, at
 * origin) PTLW. display_partial() does not run the display's lambda -- the caller must
 * already have drawn the updated pixels into the buffer before calling it.
 */
class EPaperInkplate13Spectra final : public EPaperBase {
 public:
  EPaperInkplate13Spectra(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                          size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = (size_t) width * height / 2;  // 2 pixels per byte at 4bpp
  }

  void set_rst_pin(GPIOPin *p) { this->rst_pin_ = p; }
  void set_pwr_en_pin(GPIOPin *p) { this->pwr_en_pin_ = p; }
  void set_cs_pins(GPIOPin *cs_m, GPIOPin *cs_s) {
    this->cs_m_pin_ = cs_m;
    this->cs_s_pin_ = cs_s;
  }
  void set_bs_pins(GPIOPin *bs0, GPIOPin *bs1) {
    this->bs0_pin_ = bs0;
    this->bs1_pin_ = bs1;
  }

  void setup() override;
  void dump_config() override;
  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;
  void update() override;

  // Trigger a partial (subregion) update. x/y/w/h are in logical (rotated) coords.
  // The buffer must already contain the updated pixels before calling this.
  void display_partial(int x, int y, int w, int h);

 protected:
  // Rows sent per transfer_data() tick -- keeps each SPI burst short enough that the
  // main loop stays responsive during a multi-second refresh.
  static constexpr size_t ROWS_PER_CHUNK = 16;

  // GPIO power-on bring-up, run inside reset()/RESET_END (see class-level comment).
  enum ResetSub {
    RST_PINS_LOW,
    RST_PINS_LOW_WAIT,  // 50 ms
    RST_IO_WAIT,        // 100 ms after PWR_EN goes high
    RST_LOW_WAIT,       // 100 ms RST low
    RST_HIGH_WAIT,      // 100 ms RST high
    RST_DONE,
  };

  // Full path:    TRF_PRIMARY -> TRF_WAIT_PRIMARY -> TRF_SECONDARY -> TRF_WAIT_SECONDARY -> TRF_DONE
  // Partial path: TRF_PARTIAL_SETUP_M -> TRF_PARTIAL_DATA_M -> TRF_PARTIAL_WAIT_M
  //               -> TRF_PARTIAL_SETUP_S -> TRF_PARTIAL_DATA_S -> TRF_PARTIAL_WAIT_S -> TRF_DONE
  enum TransferSub {
    TRF_PRIMARY,
    TRF_WAIT_PRIMARY,
    TRF_SECONDARY,
    TRF_WAIT_SECONDARY,
    TRF_DONE,
    TRF_PARTIAL_SETUP_M,
    TRF_PARTIAL_DATA_M,
    TRF_PARTIAL_WAIT_M,
    TRF_PARTIAL_SETUP_S,
    TRF_PARTIAL_DATA_S,
    TRF_PARTIAL_WAIT_S,
  };

  // Per-chip PTLW parameters, computed once per partial cycle in compute_ptlw_params_()
  // so transfer_data() can stream data without recomputing anything mid-transfer.
  struct PartialChipParams {
    bool needed{false};  // false -> chip is outside the update region, send a null PTLW
    uint8_t ptlw[9]{};   // 9-byte PTLW payload: HRST(2), HRED(2), VRST(2), VRED(2), PT(1)
    int mem_col_off{0};  // byte offset from the start of a row in buffer_ for this chip's window
    int bytes_per_row{0};
    int row_start{0};  // first physical row to send (inclusive)
    int row_end{0};    // last physical row to send (inclusive)
  };

  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

  void set_io_pins_();
  void set_all_pins_low_();

  /// Send a command (and optional data) to one or both chips. chip: 1=primary, 2=secondary, 3=both.
  void write_command_to_chip_(uint8_t cmd, const uint8_t *data, size_t len, uint8_t chip);
  void write_command_to_chip_(uint8_t cmd, std::initializer_list<uint8_t> data, uint8_t chip) {
    this->write_command_to_chip_(cmd, data.begin(), data.size(), chip);
  }
  void write_command_to_chip_(uint8_t cmd, uint8_t chip) { this->write_command_to_chip_(cmd, nullptr, 0, chip); }

  /// Replays the panel register init table.
  void send_init_sequence_();

  /// Fills ptlw_primary_/ptlw_secondary_ from partial_x_/y_/w_/h_.
  void compute_ptlw_params_();

  /// Throttled (1/s) debug log of the raw busy pin state, for bring-up diagnostics.
  void log_busy_state_(const char *where);

  /// Convert Color to the 4-bit hardware palette index (0=black,1=white,2=yellow,3=red,5=blue,6=green).
  static uint8_t color_to_index(Color color);

  GPIOPin *rst_pin_{nullptr};
  GPIOPin *pwr_en_pin_{nullptr};
  GPIOPin *cs_m_pin_{nullptr};
  GPIOPin *cs_s_pin_{nullptr};
  GPIOPin *bs0_pin_{nullptr};
  GPIOPin *bs1_pin_{nullptr};

  ResetSub reset_sub_{RST_PINS_LOW};
  TransferSub transfer_sub_{TRF_PRIMARY};
  size_t transfer_row_{0};
  uint32_t wait_log_ms_{0};

  bool partial_update_{false};
  int partial_x_{0};
  int partial_y_{0};
  int partial_w_{0};
  int partial_h_{0};
  PartialChipParams ptlw_primary_;
  PartialChipParams ptlw_secondary_;
};

}  // namespace esphome::epaper_spi
