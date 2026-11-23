#pragma once

// Streaming decoder for the raster formats driverless clients send: PWG raster (IPP Everywhere:
// Android, Windows, Linux) and Apple raster (URF, AirPrint: iOS, macOS). Each page is placed on the
// paper at 600 dpi, scaled from the client's resolution, turned into black and white and handed to the
// QPDL encoder line by line, so no page is ever held in memory.

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "qpdl.h"

namespace esphome::xerox_phaser_3020 {

/// Supplies document data. read() blocks and returns the number of bytes read, 0 at the end of the
/// document or -1 on an error.
class ByteSource {
 public:
  virtual int read(uint8_t *buf, size_t len) = 0;
};

enum class RasterResult : uint8_t {
  RASTER_OK,
  RASTER_NO_DATA,      // the document was empty
  RASTER_BAD_FORMAT,   // not PWG or Apple raster, or broken data
  RASTER_UNSUPPORTED,  // colour space, depth or size not handled
  RASTER_READ_ERROR,   // the connection to the client was lost
  RASTER_WRITE_ERROR,  // the printer stopped taking data
  RASTER_CANCELED,
};

inline constexpr uint8_t MAX_PAGE_RANGES = 8;

struct RasterJob {
  const std::atomic<bool> *cancel{nullptr};
  std::atomic<uint16_t> *pages_done{nullptr};
  uint16_t copies{1};
  uint16_t ranges[MAX_PAGE_RANGES][2];
  uint8_t num_ranges{0};
  uint8_t requested_paper{PAPER_NONE};
  uint8_t default_paper{0};
};

/// Widest output row in pixels
inline constexpr uint16_t MAX_ROW_PX = MAX_BAND_BYTES * 8;
/// One decoded client line: room for a 600 dpi Legal-width line in 24-bit colour
inline constexpr size_t RASTER_LINE_BUFFER_SIZE = size_t{MAX_ROW_PX} * 3;
/// Grey row and the packed output row
inline constexpr size_t RASTER_WORK_BUFFER_SIZE = MAX_ROW_PX + MAX_BAND_BYTES;

class RasterPrinter {
 public:
  /// `line` holds RASTER_LINE_BUFFER_SIZE bytes and `work` RASTER_WORK_BUFFER_SIZE bytes
  void set_buffers(uint8_t *line, uint8_t *work);

  /// Decode a whole document from `in` and print it through `encoder` to `out`. A page cut short by
  /// a read error or by canceling is still finished, so the printer is never left in the middle of one.
  RasterResult print(ByteSource *in, QpdlEncoder *encoder, OutputSink *out, const RasterJob &job);

 protected:
  struct PageFormat {
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_line;
    uint32_t x_dpi;
    uint32_t y_dpi;
    uint32_t width_pt;  // size the client gave, 0 when unknown
    uint32_t height_pt;
    uint8_t bits;
    bool black_is_one;  // K colour spaces, where 0 is white
  };

  RasterResult read_page_header_(bool urf, PageFormat &format);
  RasterResult print_page_(const PageFormat &format, bool selected, QpdlEncoder *encoder, OutputSink *out,
                           const RasterJob &job);
  bool decode_line_(const PageFormat &format);
  void grey_row_(const PageFormat &format);
  /// Turn the grey row into black and white for page row `row`
  void halftone_row_(int32_t row);
  void copy_bits_row_(const PageFormat &format);
  bool emit_white_rows_(QpdlEncoder *encoder, uint32_t count);

  int fill_();
  int next_byte_() {
    if (this->in_pos_ == this->in_len_ && this->fill_() <= 0)
      return -1;
    return this->in_[this->in_pos_++];
  }
  /// Read exactly `len` bytes, or skip them when `dst` is null. Returns the bytes read before the end.
  size_t read_(uint8_t *dst, size_t len);

  ByteSource *source_{nullptr};
  uint8_t *line_{nullptr};
  uint8_t *grey_{nullptr};
  uint8_t *row_{nullptr};
  // Page placement at 600 dpi
  int32_t x_offset_{0};
  int32_t y_offset_{0};
  uint32_t doc_width_{0};
  uint32_t doc_height_{0};
  uint16_t row_px_{0};
  uint16_t in_pos_{0};
  uint16_t in_len_{0};
  bool read_error_{false};
  bool line_white_{false};
  uint8_t in_[2048];
};

}  // namespace esphome::xerox_phaser_3020
