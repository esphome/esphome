#include "raster.h"

#include <cstring>

namespace esphome::xerox_phaser_3020 {

static constexpr uint32_t PWG_HEADER_SIZE = 1796;
// The PWG header fields used here all lie in its first 404 bytes
static constexpr uint32_t PWG_HEADER_USED = 404;
static constexpr uint32_t URF_HEADER_SIZE = 32;
static constexpr uint32_t MIN_DPI = 72;
static constexpr uint32_t MAX_DPI = 2400;
static constexpr uint32_t MAX_LINES = 100000;
// CUPS colour spaces
static constexpr uint32_t CSPACE_W = 0;
static constexpr uint32_t CSPACE_RGB = 1;
static constexpr uint32_t CSPACE_K = 3;
static constexpr uint32_t CSPACE_SW = 18;
static constexpr uint32_t CSPACE_SRGB = 19;
static constexpr uint32_t CSPACE_ADOBERGB = 20;

static uint32_t get_be32(const uint8_t *p) {
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}

void RasterPrinter::set_buffers(uint8_t *line, uint8_t *work) {
  this->line_ = line;
  this->grey_ = work;
  this->errors_ = reinterpret_cast<int16_t *>(work + MAX_ROW_PX);
  this->row_ = work + MAX_ROW_PX + (MAX_ROW_PX + 2) * sizeof(int16_t);
}

int RasterPrinter::fill_() {
  const int n = this->source_->read(this->in_, sizeof(this->in_));
  if (n < 0)
    this->read_error_ = true;
  this->in_pos_ = 0;
  this->in_len_ = n > 0 ? n : 0;
  return n;
}

size_t RasterPrinter::read_(uint8_t *dst, size_t len) {
  size_t done = 0;
  while (done < len) {
    if (this->in_pos_ == this->in_len_ && this->fill_() <= 0)
      break;
    size_t n = this->in_len_ - this->in_pos_;
    if (n > len - done)
      n = len - done;
    if (dst != nullptr)
      memcpy(dst + done, this->in_ + this->in_pos_, n);
    this->in_pos_ += n;
    done += n;
  }
  return done;
}

RasterResult RasterPrinter::print(ByteSource *in, QpdlEncoder *encoder, OutputSink *out, const RasterJob &job) {
  this->source_ = in;
  this->in_pos_ = 0;
  this->in_len_ = 0;
  this->read_error_ = false;

  uint8_t magic[8];
  const size_t n = this->read_(magic, 4);
  if (n < 4) {
    if (this->read_error_)
      return RasterResult::RASTER_READ_ERROR;
    return n == 0 ? RasterResult::RASTER_NO_DATA : RasterResult::RASTER_BAD_FORMAT;
  }
  bool urf;
  if (memcmp(magic, "RaS2", 4) == 0) {
    urf = false;
  } else if (memcmp(magic, "UNIR", 4) == 0) {
    // "UNIRAST\0" and a page count, which is not needed
    if (this->read_(magic, 8) < 8 || memcmp(magic, "AST", 4) != 0)
      return this->read_error_ ? RasterResult::RASTER_READ_ERROR : RasterResult::RASTER_BAD_FORMAT;
    urf = true;
  } else {
    return RasterResult::RASTER_BAD_FORMAT;
  }

  uint16_t page = 0;
  for (;;) {
    PageFormat format{};
    RasterResult result = this->read_page_header_(urf, format);
    if (result == RasterResult::RASTER_NO_DATA)
      break;
    if (result != RasterResult::RASTER_OK)
      return result;
    page++;
    bool selected = job.num_ranges == 0;
    for (uint8_t i = 0; i < job.num_ranges; i++)
      selected |= page >= job.ranges[i][0] && page <= job.ranges[i][1];
    result = this->print_page_(format, selected, encoder, out, job);
    if (result != RasterResult::RASTER_OK)
      return result;
  }
  return page == 0 ? RasterResult::RASTER_BAD_FORMAT : RasterResult::RASTER_OK;
}

RasterResult RasterPrinter::read_page_header_(bool urf, PageFormat &format) {
  uint32_t color_space;
  if (urf) {
    uint8_t h[URF_HEADER_SIZE];
    const size_t n = this->read_(h, sizeof(h));
    if (n == 0 && !this->read_error_)
      return RasterResult::RASTER_NO_DATA;
    if (n < sizeof(h))
      return this->read_error_ ? RasterResult::RASTER_READ_ERROR : RasterResult::RASTER_BAD_FORMAT;
    format.bits = h[0];
    format.width = get_be32(h + 12);
    format.height = get_be32(h + 16);
    format.x_dpi = format.y_dpi = get_be32(h + 20);
    // 8-bit grey and 24-bit sRGB use 255 for white; 1-bit lines, which Ghostscript writes, use 1 for black
    color_space = format.bits == 24 ? CSPACE_SRGB : format.bits == 1 ? CSPACE_K : CSPACE_SW;
  } else {
    uint8_t h[PWG_HEADER_USED];
    size_t n = this->read_(h, 4);
    if (n == 0 && !this->read_error_)
      return RasterResult::RASTER_NO_DATA;
    // Tolerate a repeated sync word between pages
    if (n == 4 && memcmp(h, "RaS2", 4) == 0)
      n = this->read_(h, 4);
    if (n < 4 || this->read_(h + 4, sizeof(h) - 4) < sizeof(h) - 4 ||
        this->read_(nullptr, PWG_HEADER_SIZE - sizeof(h)) < PWG_HEADER_SIZE - sizeof(h))
      return this->read_error_ ? RasterResult::RASTER_READ_ERROR : RasterResult::RASTER_BAD_FORMAT;
    const uint8_t *fields = h + 256;
    format.x_dpi = get_be32(fields + 5 * 4);
    format.y_dpi = get_be32(fields + 6 * 4);
    format.width_pt = get_be32(fields + 24 * 4);
    format.height_pt = get_be32(fields + 25 * 4);
    format.width = get_be32(fields + 29 * 4);
    format.height = get_be32(fields + 30 * 4);
    format.bits = get_be32(fields + 33 * 4);
    color_space = get_be32(fields + 36 * 4);
  }

  if (format.width == 0 || format.height == 0 || format.height > MAX_LINES || format.x_dpi < MIN_DPI ||
      format.x_dpi > MAX_DPI || format.y_dpi < MIN_DPI || format.y_dpi > MAX_DPI)
    return RasterResult::RASTER_BAD_FORMAT;
  switch (format.bits) {
    case 1:
    case 8:
      if (color_space != CSPACE_W && color_space != CSPACE_K && color_space != CSPACE_SW)
        return RasterResult::RASTER_UNSUPPORTED;
      format.black_is_one = color_space == CSPACE_K;
      break;
    case 24:
      if (color_space != CSPACE_RGB && color_space != CSPACE_SRGB && color_space != CSPACE_ADOBERGB)
        return RasterResult::RASTER_UNSUPPORTED;
      break;
    default:
      return RasterResult::RASTER_UNSUPPORTED;
  }
  format.bytes_per_line = (format.width * format.bits + 7) / 8;
  if (format.bytes_per_line > RASTER_LINE_BUFFER_SIZE)
    return RasterResult::RASTER_UNSUPPORTED;
  return RasterResult::RASTER_OK;
}

RasterResult RasterPrinter::print_page_(const PageFormat &format, bool selected, QpdlEncoder *encoder, OutputSink *out,
                                        const RasterJob &job) {
  // The paper is the size the client rendered, or else the one the job asked for, or the loaded one
  const uint32_t width_pt = format.width_pt != 0 ? format.width_pt : format.width * 72 / format.x_dpi;
  const uint32_t height_pt = format.height_pt != 0 ? format.height_pt : format.height * 72 / format.y_dpi;
  uint8_t paper_index = find_paper_by_points(width_pt, height_pt);
  if (paper_index == PAPER_NONE)
    paper_index = job.requested_paper != PAPER_NONE ? job.requested_paper : job.default_paper;
  const Paper &paper = PAPERS[paper_index];

  // Centre the page on the paper at 600 dpi, as SpliX does, in whole bytes across
  const uint32_t page_height = page_height_px(paper);
  this->doc_width_ = format.width * QPDL_DPI / format.x_dpi;
  this->doc_height_ = format.height * QPDL_DPI / format.y_dpi;
  this->x_offset_ =
      (static_cast<int32_t>(page_width_px(paper) / 8) - static_cast<int32_t>((this->doc_width_ + 7) / 8)) / 2 * 8;
  this->y_offset_ = (static_cast<int32_t>(page_height) - static_cast<int32_t>(this->doc_height_)) / 2;
  const bool bit_copy = format.bits == 1 && format.x_dpi == QPDL_DPI;
  if (selected) {
    encoder->begin_page(out, paper, job.copies, false);
    this->row_px_ = encoder->row_bytes() * 8;
    memset(this->errors_, 0, (MAX_ROW_PX + 2) * sizeof(int16_t));
    this->errors_clear_ = true;
  }

  RasterResult result = RasterResult::RASTER_OK;
  int32_t next_row = HARD_MARGIN_Y;
  const int32_t doc_end = static_cast<int32_t>(this->doc_height_) + this->y_offset_;
  const int32_t page_end = doc_end < static_cast<int32_t>(page_height) ? doc_end : page_height;
  uint32_t y = 0;
  while (y < format.height) {
    if (job.cancel != nullptr && job.cancel->load(std::memory_order_relaxed)) {
      result = RasterResult::RASTER_CANCELED;
      break;
    }
    const int repeat = this->next_byte_();
    if (repeat < 0 || !this->decode_line_(format)) {
      result = this->read_error_ ? RasterResult::RASTER_READ_ERROR : RasterResult::RASTER_BAD_FORMAT;
      break;
    }
    uint32_t count = static_cast<uint32_t>(repeat) + 1;
    if (count > format.height - y)
      count = format.height - y;
    if (selected) {
      // Page rows that show client lines y .. y + count - 1
      int32_t start = static_cast<int32_t>((y * QPDL_DPI + format.y_dpi - 1) / format.y_dpi) + this->y_offset_;
      int32_t end = static_cast<int32_t>(((y + count) * QPDL_DPI + format.y_dpi - 1) / format.y_dpi) + this->y_offset_;
      if (start < next_row)
        start = next_row;
      if (end > page_end)
        end = page_end;
      if (start < end) {
        bool ok = this->emit_white_rows_(encoder, start - next_row);
        if (bit_copy) {
          this->copy_bits_row_(format);
        } else {
          this->grey_row_(format);
        }
        for (int32_t row = start; row < end && ok; row++) {
          if (this->line_white_) {
            if (!this->errors_clear_) {
              memset(this->errors_, 0, (MAX_ROW_PX + 2) * sizeof(int16_t));
              this->errors_clear_ = true;
            }
            ok = encoder->add_row(nullptr);
          } else {
            if (!bit_copy)
              this->dither_row_();
            ok = encoder->add_row(this->row_);
          }
        }
        next_row = end;
        if (!ok)
          return RasterResult::RASTER_WRITE_ERROR;
      }
    }
    y += count;
  }
  if (!selected)
    return result;
  if (!encoder->end_page())
    return RasterResult::RASTER_WRITE_ERROR;
  if (result == RasterResult::RASTER_OK && job.pages_done != nullptr)
    job.pages_done->fetch_add(1, std::memory_order_relaxed);
  return result;
}

bool RasterPrinter::decode_line_(const PageFormat &format) {
  // PackBits-style runs of whole pixels (whole bytes below 8 bits per pixel); 128 fills the rest
  // of the line with white
  const uint32_t length = format.bytes_per_line;
  const uint32_t unit = format.bits >= 8 ? format.bits / 8 : 1;
  uint8_t *line = this->line_;
  uint32_t pos = 0;
  while (pos < length) {
    const int code = this->next_byte_();
    if (code < 0)
      return false;
    if (code == 128) {
      memset(line + pos, format.black_is_one ? 0x00 : 0xFF, length - pos);
      return true;
    }
    if (code < 128) {
      uint8_t pixel[3];
      if (this->read_(pixel, unit) < unit)
        return false;
      for (uint32_t i = 0; i <= static_cast<uint32_t>(code) && pos < length; i++) {
        for (uint32_t k = 0; k < unit && pos < length; k++)
          line[pos++] = pixel[k];
      }
    } else {
      const uint32_t size = (257 - code) * unit;
      const uint32_t take = size < length - pos ? size : length - pos;
      if (this->read_(line + pos, take) < take || this->read_(nullptr, size - take) < size - take)
        return false;
      pos += take;
    }
  }
  return true;
}

void RasterPrinter::grey_row_(const PageFormat &format) {
  // Column 0 of the row is page pixel HARD_MARGIN_X; client pixels are picked nearest-neighbour
  int32_t doc_x = HARD_MARGIN_X - this->x_offset_;
  uint32_t src = 0;
  uint32_t remainder = 0;
  bool started = false;
  uint8_t all = 0xFF;
  const uint8_t *line = this->line_;
  const uint8_t flip = format.black_is_one ? 0xFF : 0x00;
  for (uint16_t c = 0; c < this->row_px_; c++, doc_x++) {
    uint8_t grey = 0xFF;
    if (doc_x >= 0 && static_cast<uint32_t>(doc_x) < this->doc_width_) {
      if (!started) {
        src = static_cast<uint32_t>(doc_x) * format.x_dpi / QPDL_DPI;
        remainder = static_cast<uint32_t>(doc_x) * format.x_dpi % QPDL_DPI;
        started = true;
      } else {
        remainder += format.x_dpi;
        while (remainder >= QPDL_DPI) {
          remainder -= QPDL_DPI;
          src++;
        }
      }
      switch (format.bits) {
        case 1:
          grey = ((line[src >> 3] >> (7 - (src & 7))) & 1) ? 0xFF : 0x00;
          grey ^= flip;
          break;
        case 8:
          grey = line[src] ^ flip;
          break;
        default: {
          const uint8_t *p = line + src * 3;
          grey = (p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8;
          break;
        }
      }
    }
    this->grey_[c] = grey;
    all &= grey;
  }
  this->line_white_ = all == 0xFF;
}

void RasterPrinter::dither_row_() {
  // Floyd-Steinberg error diffusion with one error row: errors[x] holds the error carried into pixel x
  // from the row above and is replaced, once pixel x + 1 is done, by the error for the row below
  int16_t *errors = this->errors_ + 1;
  uint8_t *out = this->row_;
  int below_left = 0;  // next row, column x - 1, still missing pixel x's share
  int below = 0;       // next row, column x, still missing pixel x's share
  int right = 0;
  uint8_t bits = 0;
  for (uint16_t x = 0; x < this->row_px_; x++) {
    const int value = this->grey_[x] + errors[x] + right;
    int error = value;
    if (value < 128) {
      bits |= 0x80 >> (x & 7);
    } else {
      error = value - 255;
    }
    right = error * 7 / 16;
    errors[x - 1] = below_left + error * 3 / 16;
    below_left = below + error * 5 / 16;
    below = error / 16;
    if ((x & 7) == 7) {
      *out++ = bits;
      bits = 0;
    }
  }
  errors[this->row_px_ - 1] = below_left;
  this->errors_clear_ = false;
}

void RasterPrinter::copy_bits_row_(const PageFormat &format) {
  // 1-bit lines at 600 dpi: the offset and the margin are whole bytes, so bytes copy across
  const int32_t first = (HARD_MARGIN_X - this->x_offset_) / 8;
  const int32_t whole = format.width / 8;
  const uint8_t tail_mask = format.width % 8 ? static_cast<uint8_t>(0xFF << (8 - format.width % 8)) : 0;
  const uint8_t flip = format.black_is_one ? 0x00 : 0xFF;
  uint8_t any = 0;
  for (int32_t b = 0; b < this->row_px_ / 8; b++) {
    const int32_t s = first + b;
    uint8_t v = 0;
    if (s >= 0 && s < whole) {
      v = this->line_[s] ^ flip;
    } else if (s == whole && tail_mask != 0) {
      v = (this->line_[s] ^ flip) & tail_mask;
    }
    this->row_[b] = v;
    any |= v;
  }
  this->line_white_ = any == 0;
}

bool RasterPrinter::emit_white_rows_(QpdlEncoder *encoder, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    if (!encoder->add_row(nullptr))
      return false;
  }
  return true;
}

}  // namespace esphome::xerox_phaser_3020
