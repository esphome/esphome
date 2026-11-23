#pragma once

// Encoder for QPDL (also called SPL), the page language of the Xerox Phaser 3020 and the Samsung
// printers it is built on. It is a port of the parts of the SpliX driver (GPL-2.0, OpenPrinting) that
// the Phaser 3020 uses: QPDL version 3, 128-line bands and compression algorithm 0x11.
//
// Pages are encoded band by band as their rows arrive, so only one band (at most 80 kB) is held in
// memory. The compressed size of a band is sent before its data, so each band is compressed twice:
// once to measure it and once while it is written out.

#include <cstddef>
#include <cstdint>

namespace esphome::xerox_phaser_3020 {

/// Receives the printer data stream. write() blocks until the data is taken and returns false when
/// the job has to stop (printer gone, transfer error or job canceled).
class OutputSink {
 public:
  virtual bool write(const uint8_t *data, size_t len) = 0;
};

struct Paper {
  const char *pwg_name;  // PWG media name, e.g. iso_a4_210x297mm
  const char *ppd_name;  // name in the SpliX PPD, shown in logs
  uint16_t width_hmm;    // hundredths of a millimetre, as IPP uses
  uint16_t height_hmm;
  uint16_t width_pt;  // points, as SpliX uses
  uint16_t height_pt;
  uint16_t band_bytes;  // band width at 600 dpi that the Phaser 3020 expects for this size
  uint8_t qpdl_type;    // paper type code in the page header
};

// Paper sizes the Phaser 3020 handles, from the SpliX PPD. Band widths come from SpliX's table
// for the Samsung M2020 series (QPDL "SpecialBandWidth").
inline constexpr Paper PAPERS[] = {
    {"iso_a4_210x297mm", "A4", 21000, 29700, 595, 842, 608, 2},
    {"na_letter_8.5x11in", "Letter", 21590, 27940, 612, 792, 640, 0},
    {"na_legal_8.5x14in", "Legal", 21590, 35560, 612, 1008, 640, 1},
    {"na_executive_7.25x10.5in", "Executive", 18415, 26670, 522, 756, 544, 3},
    {"iso_a5_148x210mm", "A5", 14800, 21000, 420, 595, 416, 16},
    {"iso_a6_105x148mm", "A6", 10500, 14800, 297, 420, 288, 17},
    {"jis_b5_182x257mm", "B5", 18200, 25700, 516, 729, 544, 11},
    {"na_number-10_4.125x9.5in", "Env10", 10477, 24130, 297, 684, 288, 6},
    {"na_monarch_3.875x7.5in", "Monarch", 9842, 19050, 279, 540, 288, 7},
    {"iso_dl_110x220mm", "DL", 11000, 22000, 312, 624, 320, 9},
    {"iso_c5_162x229mm", "C5", 16200, 22900, 459, 649, 480, 8},
};
inline constexpr uint8_t NUM_PAPERS = sizeof(PAPERS) / sizeof(PAPERS[0]);
inline constexpr uint8_t PAPER_NONE = 0xFF;

inline constexpr uint16_t QPDL_DPI = 600;
inline constexpr uint16_t BAND_HEIGHT = 128;
inline constexpr uint16_t MAX_BAND_BYTES = 640;
inline constexpr size_t BAND_BUFFER_SIZE = size_t{MAX_BAND_BYTES} * BAND_HEIGHT;
// SpliX's PPD gives the printer a 12.5 pt hardware margin; rows and columns inside it are not sent.
// The left margin is rounded up to whole bytes.
inline constexpr uint16_t HARD_MARGIN_X = 112;
inline constexpr uint16_t HARD_MARGIN_Y = 105;
// The hardware margin in hundredths of a millimetre, for IPP
inline constexpr uint16_t HARD_MARGIN_HMM = 441;
// Scratch space the encoder needs for choosing its compression table
inline constexpr size_t QPDL_SCRATCH_SIZE = 0x800 * sizeof(int16_t);
// Holds any compressed band: literal runs add one byte in 128, plus the 132-byte table header
inline constexpr size_t QPDL_PACKED_BUFFER_SIZE = BAND_BUFFER_SIZE + BAND_BUFFER_SIZE / 64 + 512;

/// Page width in pixels at 600 dpi, rounded up to whole bytes, as SpliX computes it
inline constexpr uint16_t page_width_px(const Paper &paper) { return ((paper.width_pt * QPDL_DPI + 71) / 72 + 7) & ~7; }
/// Page height in pixels at 600 dpi
inline constexpr uint16_t page_height_px(const Paper &paper) { return (paper.height_pt * QPDL_DPI + 71) / 72; }

/// Index of the paper that matches the given size in points (either way round) within 2%, or PAPER_NONE
uint8_t find_paper_by_points(uint32_t width_pt, uint32_t height_pt);
/// Index of the paper with this PWG name, or PAPER_NONE
uint8_t find_paper_by_name(const char *pwg_name);
/// Index of the paper with this size in hundredths of a millimetre (1 mm tolerance), or PAPER_NONE
uint8_t find_paper_by_hmm(int32_t width_hmm, int32_t height_hmm);

enum class TonerSave : uint8_t {
  TONER_SAVE_PRINTER_DEFAULT,
  TONER_SAVE_ON,
  TONER_SAVE_OFF,
};

/// Job settings that go into the PJL header
struct PjlSettings {
  uint8_t density{3};  // 1 light, 3 medium, 5 dark
  TonerSave toner_save{TonerSave::TONER_SAVE_PRINTER_DEFAULT};
  uint8_t power_save_minutes{5};  // 0 turns power save off
  bool jam_recovery{false};
};

/// Write the PJL job header that switches the printer to QPDL. `date` is YYYYMMDD, or 0 when the time
/// is not known. User and job names are cleaned of characters PJL cannot carry.
bool write_pjl_header(OutputSink *sink, const PjlSettings &settings, const char *user, const char *job_name,
                      uint32_t date);
bool write_pjl_footer(OutputSink *sink);

class QpdlEncoder {
 public:
  /// `band` holds BAND_BUFFER_SIZE bytes and `scratch` QPDL_SCRATCH_SIZE bytes. `packed`, which may be
  /// null, holds QPDL_PACKED_BUFFER_SIZE bytes: with it each band is compressed once and kept until it
  /// is sent, without it each band is compressed twice, once to measure it and once to send it.
  void set_buffers(uint8_t *band, uint8_t *scratch, uint8_t *packed) {
    this->band_ = band;
    this->scratch_ = reinterpret_cast<int16_t *>(scratch);
    this->packed_ = packed;
  }

  /// Start a page. The page header is sent with the first band, or by end_page() for a blank page.
  void begin_page(OutputSink *sink, const Paper &paper, uint16_t copies, bool manual_feed);
  /// Bytes in one row given to add_row(): the part of the page right of the hardware margin that the
  /// printer takes. Row bit 7 of byte 0 is the pixel at HARD_MARGIN_X; a set bit prints black.
  uint16_t row_bytes() const { return this->row_bytes_; }
  /// Rows add_row() expects in total: the page height below the hardware margin
  uint16_t rows() const { return this->rows_; }
  uint16_t rows_added() const { return this->row_; }
  /// Add the next row; nullptr adds a white row. Rows past the end of the page are ignored.
  bool add_row(const uint8_t *row);
  /// Fill the rest of the page with white and send it. Returns false on a write error.
  bool end_page();

 protected:
  friend struct QpdlWriterOut;
  void start_page_();
  bool flush_band_();
  void choose_table_(uint32_t size);
  template<typename Out> void compress_(uint32_t size, Out &out);
  bool write_(const uint8_t *data, size_t len);
  bool put_(uint8_t byte);
  bool flush_out_();

  OutputSink *sink_{nullptr};
  uint8_t *band_{nullptr};
  int16_t *scratch_{nullptr};
  uint8_t *packed_{nullptr};
  const Paper *paper_{nullptr};
  uint32_t table_[64];
  uint16_t copies_{1};
  uint16_t band_bytes_{0};
  uint16_t row_bytes_{0};
  uint16_t rows_{0};
  uint16_t row_{0};
  uint16_t band_row_{0};
  uint16_t out_len_{0};
  uint8_t band_number_{0};
  bool band_dirty_{false};
  bool page_started_{false};
  bool manual_feed_{false};
  bool ok_{true};
  uint8_t out_[256];
};

}  // namespace esphome::xerox_phaser_3020
