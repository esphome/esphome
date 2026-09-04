#pragma once

#include "epaper_spi_4bpp.h"

namespace esphome::epaper_spi {

/**
 * Intermediate base for 4bpp panels split across two chip-selects, each driving half of
 * every row (CS: left half, CS1: right half). Shared by controllers in the GDEP133C02
 * family (T133A01, Inkplate 13 Spectra).
 *
 * Provides write_command_to_chip_() chip-select routing plus transfer_data(): a full-refresh
 * path (transfer_full_()) and a PTLW-based partial-update path. The full-refresh algorithm
 * is T133A01's -- CS held low across the entire DTM stream for each chip in turn, no
 * busy-wait between the two chips' phases, re-selecting the color set (CCSET) on every
 * refresh -- carried over largely unchanged, including its yield/CS-holding regression
 * tests (tests/components/epaper_spi/display/test_t133a01_transfer.cpp) covering a real
 * deadlock (issue #17668). It's also confirmed working on Inkplate 13 Spectra hardware.
 * Concrete subclasses supply color_to_native(), reset()/initialise() (board-specific GPIO
 * bring-up and register tables differ enough to keep these separate), and
 * power_on()/power_off()/refresh_screen()/deep_sleep().
 *
 * Partial update (start_partial_update_()) is protected: it's only been verified working on
 * the Inkplate 13 Spectra so far. A subclass exposes it publicly only once verified on its
 * own hardware -- see EPaperInkplate13Spectra::display_partial(); EPaperT133A01 does not
 * expose it, though nothing stops it from working there once tested.
 *
 * Not a generic dual-CS base -- it bakes in this controller family's specifics:
 *   - 4bpp only (via EPaper4bpp)
 *   - an exact 50/50 column split between the two chips (compute_ptlw_params_() hardcodes
 *     panel_w/2)
 *   - this family's fixed register addresses (REG_DTM/DRF/PON/POF/PTLW/CMD66/CCSET) and
 *     PTLW's column/row alignment rules
 *   - one toggle_dc_ flag per board (always-on or always-off, not variable per command)
 * A dual-CS panel that breaks any of these needs real changes here, not just a new
 * subclass.
 */
class EPaperDualCS : public EPaper4bpp {
 public:
  EPaperDualCS(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length, bool toggle_dc)
      : EPaper4bpp(name, width, height, init_sequence, init_sequence_length), toggle_dc_(toggle_dc) {}

  void set_cs_pins(GPIOPin *cs, GPIOPin *cs1) {
    this->cs_pin_ = cs;
    this->cs1_pin_ = cs1;
  }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  // Rows sent per transfer_data() tick -- keeps each SPI burst short enough that the main
  // loop stays responsive during a multi-second refresh.
  static constexpr size_t ROWS_PER_CHUNK = 16;

  static constexpr uint8_t REG_DTM = 0x10;
  static constexpr uint8_t REG_DRF = 0x12;
  static constexpr uint8_t REG_PON = 0x04;
  static constexpr uint8_t REG_POF = 0x02;
  static constexpr uint8_t REG_PTLW = 0x83;   // partial window
  static constexpr uint8_t REG_CMD66 = 0xF0;  // waveform select, required again before PTLW
  static constexpr uint8_t REG_CCSET = 0xE0;  // color set select, resent before every full transfer

  static constexpr uint8_t CHIP_PRIMARY = 1;
  static constexpr uint8_t CHIP_SECONDARY = 2;
  static constexpr uint8_t CHIP_BOTH = 3;

  static constexpr uint8_t CMD66_V[6] = {0x49, 0x55, 0x13, 0x5D, 0x05, 0x10};
  // 4x4 window at physical origin -- sent to whichever chip the requested rectangle doesn't
  // touch, so both chips still complete a full CMD66->PTLW->DTM cycle.
  static constexpr uint8_t NULL_PTLW[9] = {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x01};

  // Full path: driven entirely by current_data_index_ inside transfer_full_(), no sub-states
  // needed.
  // Partial path: TRF_PARTIAL_SETUP_M -> TRF_PARTIAL_DATA_M -> TRF_PARTIAL_WAIT_M
  //               -> TRF_PARTIAL_SETUP_S -> TRF_PARTIAL_DATA_S -> TRF_PARTIAL_WAIT_S -> TRF_DONE
  enum TransferSub {
    TRF_FULL,
    TRF_DONE,
    TRF_PARTIAL_SETUP_M,
    TRF_PARTIAL_DATA_M,
    TRF_PARTIAL_WAIT_M,
    TRF_PARTIAL_SETUP_S,
    TRF_PARTIAL_DATA_S,
    TRF_PARTIAL_WAIT_S,
  };

  // Per-chip PTLW parameters, computed once per partial cycle in compute_ptlw_params_() so
  // transfer_data() can stream data without recomputing anything mid-transfer.
  struct PartialChipParams {
    bool needed{false};  // false -> chip is outside the update region, send a null PTLW
    uint8_t ptlw[9]{};   // 9-byte PTLW payload: HRST(2), HRED(2), VRST(2), VRED(2), PT(1)
    int mem_col_off{0};  // byte offset from the start of a row in buffer_ for this chip's window
    int bytes_per_row{0};
    int row_start{0};  // first physical row to send (inclusive)
    int row_end{0};    // last physical row to send (inclusive)
  };

  bool transfer_data() override;

  /// Full-refresh transfer: CS phase then CS1 phase, each streaming the whole half-buffer
  /// with CS held low across yields. See class comment for provenance.
  bool transfer_full_();

  /// Send a command (and optional data) to one or both chips. chip: CHIP_PRIMARY/SECONDARY/BOTH.
  void write_command_to_chip_(uint8_t cmd, const uint8_t *data, size_t len, uint8_t chip);
  void write_command_to_chip_(uint8_t cmd, std::initializer_list<uint8_t> data, uint8_t chip) {
    this->write_command_to_chip_(cmd, data.begin(), data.size(), chip);
  }
  void write_command_to_chip_(uint8_t cmd, uint8_t chip) { this->write_command_to_chip_(cmd, nullptr, 0, chip); }

  /// Trigger a partial (subregion) update. x/y/w/h are in logical (rotated) coords. Protected
  /// -- see class comment. The buffer must already contain the updated pixels.
  void start_partial_update_(int x, int y, int w, int h);

  /// Fills ptlw_primary_/ptlw_secondary_ from partial_x_/y_/w_/h_.
  void compute_ptlw_params_();

  /// Throttled (1/s) debug log of the raw busy pin state, for bring-up diagnostics.
  void log_busy_state_(const char *where);

  GPIOPin *cs_pin_{nullptr};
  GPIOPin *cs1_pin_{nullptr};

  // Whether write_command_to_chip_() toggles dc_pin_ around the command byte (4-wire SPI
  // semantics) or leaves it alone (3-wire SPI, where cmd/data is inferred by byte position).
  bool toggle_dc_;

  TransferSub transfer_sub_{TRF_FULL};
  size_t transfer_row_{0};  // used only by the partial path -- the full path uses current_data_index_
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
