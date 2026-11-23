#include "qpdl.h"

#include <cstdio>
#include <cstring>

namespace esphome::xerox_phaser_3020 {

// Compression 0x11 (SpliX algo0x11): a table of 64 back-reference distances, chosen per band from the
// distances at which bytes repeat most often, followed by literal runs and (distance, length) copies.
static constexpr uint32_t SAMPLE_RATE = 0x800;
static constexpr uint8_t TABLE_SIZE = 64;
static constexpr uint32_t MAX_LITERAL = 0x80;
static constexpr uint32_t MAX_COPY = 0x202;
static constexpr uint32_t MIN_COPY = 3;
static constexpr uint8_t COMPRESSION_0X11 = 0x11;
static constexpr uint8_t QPDL_VERSION = 3;
// Band sub-header signature 0x09ABCDEF, sent in the byte order of the machine as SpliX does (little endian)
static constexpr uint8_t BAND_SIGNATURE[4] = {0xEF, 0xCD, 0xAB, 0x09};
static constexpr uint32_t BAND_SIGNATURE_SUM = 0x09 + 0xAB + 0xCD + 0xEF;

uint8_t find_paper_by_points(uint32_t width_pt, uint32_t height_pt) {
  uint8_t best = PAPER_NONE;
  uint32_t best_error = 40;  // per mille, summed over both sides
  for (uint8_t i = 0; i < NUM_PAPERS; i++) {
    for (uint8_t turn = 0; turn < 2; turn++) {
      const uint32_t w = turn ? PAPERS[i].height_pt : PAPERS[i].width_pt;
      const uint32_t h = turn ? PAPERS[i].width_pt : PAPERS[i].height_pt;
      const uint32_t dw = width_pt > w ? width_pt - w : w - width_pt;
      const uint32_t dh = height_pt > h ? height_pt - h : h - height_pt;
      const uint32_t error = dw * 1000 / w + dh * 1000 / h;
      if (error < best_error) {
        best_error = error;
        best = i;
      }
    }
  }
  return best;
}

uint8_t find_paper_by_name(const char *pwg_name) {
  for (uint8_t i = 0; i < NUM_PAPERS; i++) {
    if (strcmp(PAPERS[i].pwg_name, pwg_name) == 0)
      return i;
  }
  return PAPER_NONE;
}

uint8_t find_paper_by_hmm(int32_t width_hmm, int32_t height_hmm) {
  for (uint8_t i = 0; i < NUM_PAPERS; i++) {
    const int32_t dw = width_hmm - PAPERS[i].width_hmm;
    const int32_t dh = height_hmm - PAPERS[i].height_hmm;
    if (dw >= -100 && dw <= 100 && dh >= -100 && dh <= 100)
      return i;
  }
  return PAPER_NONE;
}

// Copy a name into a PJL string value: printable ASCII without quotes, at most 40 characters
static void pjl_clean(char *out, size_t size, const char *in) {
  size_t n = 0;
  for (; in != nullptr && *in != '\0' && n + 1 < size && n < 40; in++) {
    const char c = *in;
    out[n++] = (c >= 0x20 && c < 0x7F && c != '"') ? c : '_';
  }
  out[n] = '\0';
}

bool write_pjl_header(OutputSink *sink, const PjlSettings &settings, const char *user, const char *job_name,
                      uint32_t date) {
  char clean_user[41];
  char clean_job[41];
  pjl_clean(clean_user, sizeof(clean_user), user);
  pjl_clean(clean_job, sizeof(clean_job), job_name);
  // Same lines, in the same order, as SpliX sends for the Phaser 3020 PPD
  char buf[640];
  int n = snprintf(buf, sizeof(buf), "\x1b%%-12345X");
  if (date != 0)
    n += snprintf(buf + n, sizeof(buf) - n, "@PJL DEFAULT SERVICEDATE=%08u\n", static_cast<unsigned>(date));
  n += snprintf(buf + n, sizeof(buf) - n, "@PJL SET USERNAME=\"%s\"\n@PJL SET JOBNAME=\"%s\"\n", clean_user, clean_job);
  if (settings.toner_save != TonerSave::TONER_SAVE_PRINTER_DEFAULT) {
    n += snprintf(buf + n, sizeof(buf) - n, "@PJL SET ECONOMODE=%s\n",
                  settings.toner_save == TonerSave::TONER_SAVE_ON ? "ON" : "OFF");
  }
  if (settings.power_save_minutes != 0) {
    n += snprintf(buf + n, sizeof(buf) - n, "@PJL DEFAULT POWERSAVE=ON\n@PJL DEFAULT POWERSAVETIME=%u\n",
                  settings.power_save_minutes);
  } else {
    n += snprintf(buf + n, sizeof(buf) - n, "@PJL DEFAULT POWERSAVE=OFF\n");
  }
  n += snprintf(buf + n, sizeof(buf) - n,
                "@PJL SET JAMRECOVERY=%s\n@PJL SET DUPLEX=OFF\n@PJL SET PAPERTYPE=OFF\n@PJL SET ALTITUDE=LOW\n"
                "@PJL SET DENSITY=%u\n@PJL SET RET=NORMAL\n@PJL ENTER LANGUAGE = QPDL\n",
                settings.jam_recovery ? "ON" : "OFF", settings.density);
  return sink->write(reinterpret_cast<const uint8_t *>(buf), n);
}

bool write_pjl_footer(OutputSink *sink) {
  static constexpr uint8_t FOOTER[] = {'\t', 0x1B, '%', '-', '1', '2', '3', '4', '5', 'X'};
  return sink->write(FOOTER, sizeof(FOOTER));
}

void QpdlEncoder::begin_page(OutputSink *sink, const Paper &paper, uint16_t copies, bool manual_feed) {
  this->sink_ = sink;
  this->paper_ = &paper;
  this->copies_ = copies;
  this->manual_feed_ = manual_feed;
  this->band_bytes_ = paper.band_bytes;
  const uint16_t usable = page_width_px(paper) / 8 - HARD_MARGIN_X / 8;
  this->row_bytes_ = usable < paper.band_bytes ? usable : paper.band_bytes;
  this->rows_ = page_height_px(paper) - HARD_MARGIN_Y;
  this->row_ = 0;
  this->band_row_ = 0;
  this->band_number_ = 0;
  this->band_dirty_ = false;
  this->page_started_ = false;
  this->ok_ = true;
  this->out_len_ = 0;
  memset(this->band_, 0, size_t{this->band_bytes_} * BAND_HEIGHT);
}

bool QpdlEncoder::add_row(const uint8_t *row) {
  if (this->row_ >= this->rows_ || !this->ok_)
    return this->ok_;
  if (row != nullptr) {
    // The band is stored column by column: each byte column of the band is BAND_HEIGHT bytes long
    uint8_t *dst = this->band_ + this->band_row_;
    uint8_t any = 0;
    for (uint16_t x = 0; x < this->row_bytes_; x++) {
      dst[size_t{x} * BAND_HEIGHT] = row[x];
      any |= row[x];
    }
    this->band_dirty_ |= any != 0;
  }
  this->row_++;
  if (++this->band_row_ == BAND_HEIGHT)
    return this->flush_band_();
  return true;
}

bool QpdlEncoder::end_page() {
  if (this->band_row_ != 0)
    this->flush_band_();
  // A blank page is still sent, with no bands, so it comes out as an empty sheet
  this->start_page_();
  if (this->ok_) {
    const uint8_t footer[3] = {1, static_cast<uint8_t>(this->copies_ >> 8), static_cast<uint8_t>(this->copies_)};
    this->write_(footer, sizeof(footer));
    this->flush_out_();
  }
  return this->ok_;
}

void QpdlEncoder::choose_table_(uint32_t size) {
  const uint8_t *data = this->band_;
  int16_t *counts = this->scratch_;
  memset(counts, 0, QPDL_SCRATCH_SIZE);
  // counts[j - 1]: how often the sampled byte equals the byte j positions before it
  for (uint32_t i = SAMPLE_RATE; i < size; i += SAMPLE_RATE) {
    const uint8_t b = data[i];
    for (uint32_t j = 1; j < SAMPLE_RATE; j++) {
      if (data[i - j] == b)
        counts[j - 1]++;
    }
  }
  // The 64 most frequent distances, ties to the shorter one
  bool has_one = false;
  for (uint8_t t = 0; t < TABLE_SIZE; t++) {
    uint32_t best = 0;
    for (uint32_t j = 1; j < SAMPLE_RATE; j++) {
      if (counts[j] > counts[best])
        best = j;
    }
    counts[best] = -1;
    this->table_[t] = best + 1;
    has_one |= best == 0;
  }
  // Distance 1 compresses runs of the same byte well, so it is always in the table
  if (!has_one)
    this->table_[TABLE_SIZE - 1] = 1;
}

template<typename Out> void QpdlEncoder::compress_(uint32_t size, Out &out) {
  const uint8_t *data = this->band_;
  uint32_t literal = 0;
  for (uint32_t distance : this->table_) {
    if (distance > literal)
      literal = distance;
  }
  if (literal > MAX_LITERAL)
    literal = MAX_LITERAL;
  // Header: length of the leading literal block and the distance table, little endian
  const uint8_t head[4] = {static_cast<uint8_t>(literal), static_cast<uint8_t>(literal >> 8), 0, 0};
  out.bytes(head, sizeof(head));
  for (uint32_t distance : this->table_) {
    const uint8_t d[2] = {static_cast<uint8_t>(distance), static_cast<uint8_t>(distance >> 8)};
    out.bytes(d, sizeof(d));
  }
  out.bytes(data, literal);

  uint32_t r = literal;
  uint32_t run = 0;
  const uint8_t *run_start = data;
  while (r < size) {
    const uint32_t max_copy = size - r > MAX_COPY ? MAX_COPY : size - r;
    if (max_copy >= 2) {
      uint32_t best = 0;
      uint8_t best_index = 0;
      const uint8_t *cur = data + r;
      for (uint8_t i = 0; i < TABLE_SIZE; i++) {
        const uint32_t distance = this->table_[i];
        if (distance > r)
          continue;
        const uint8_t *ref = cur - distance;
        uint32_t n = 0;
        while (n < max_copy && cur[n] == ref[n])
          n++;
        if (n > best) {
          best = n;
          best_index = i;
        }
      }
      if (best >= MIN_COPY) {
        if (run != 0) {
          out.byte(run - 1);
          out.bytes(run_start, run);
          run = 0;
        }
        r += best;
        const uint32_t count = best - MIN_COPY;
        out.byte(0x80 | (count & 0x7F));
        out.byte(((count >> 1) & 0xC0) | best_index);
        continue;
      }
    }
    if (run == 0)
      run_start = data + r;
    r++;
    if (++run == MAX_LITERAL) {
      out.byte(MAX_LITERAL - 1);
      out.bytes(run_start, MAX_LITERAL);
      run = 0;
    }
  }
  if (run != 0) {
    out.byte(run - 1);
    out.bytes(run_start, run);
  }
}

namespace {
struct CountingOut {
  uint32_t size{0};
  uint32_t sum{0};
  void byte(uint8_t b) {
    this->size++;
    this->sum += b;
  }
  void bytes(const uint8_t *data, uint32_t len) {
    this->size += len;
    for (uint32_t i = 0; i < len; i++)
      this->sum += data[i];
  }
};
}  // namespace

struct QpdlWriterOut {
  QpdlEncoder *encoder;
  void byte(uint8_t b) { this->encoder->put_(b); }
  void bytes(const uint8_t *data, uint32_t len) { this->encoder->write_(data, len); }
};

void QpdlEncoder::start_page_() {
  if (this->page_started_)
    return;
  this->page_started_ = true;
  const uint16_t width = (this->paper_->width_pt * 300 + 71) / 72;
  const uint16_t height = (this->paper_->height_pt * 300 + 71) / 72;
  const uint8_t header[17] = {
      0,                                         // signature
      QPDL_DPI / 100,                            // vertical resolution
      static_cast<uint8_t>(this->copies_ >> 8),  // copies
      static_cast<uint8_t>(this->copies_),
      this->paper_->qpdl_type,
      static_cast<uint8_t>(width >> 8),  // page size at 300 dpi
      static_cast<uint8_t>(width),
      static_cast<uint8_t>(height >> 8),
      static_cast<uint8_t>(height),
      static_cast<uint8_t>(this->manual_feed_ ? 2 : 1),  // paper source
      0,
      1,  // "duplex" byte SpliX sends for one-sided pages
      0,  // tumble
      0,
      QPDL_VERSION,
      1,               // colour planes byte from the PPD
      QPDL_DPI / 100,  // horizontal resolution
  };
  this->write_(header, sizeof(header));
}

bool QpdlEncoder::flush_band_() {
  const uint16_t band_number = this->band_number_++;
  const uint32_t size = uint32_t{this->band_bytes_} * BAND_HEIGHT;
  this->band_row_ = 0;
  if (!this->band_dirty_)
    return this->ok_;
  this->band_dirty_ = false;

  this->start_page_();
  // The printer wants 1 for white, so the band is inverted before it is compressed
  auto *words = reinterpret_cast<uint32_t *>(this->band_);
  for (uint32_t i = 0; i < size / 4; i++)
    words[i] = ~words[i];

  this->choose_table_(size);
  CountingOut counter;
  this->compress_(size, counter);
  const uint32_t data_size = counter.size + 8;  // signature and checksum
  const uint32_t checksum = counter.sum + BAND_SIGNATURE_SUM;
  const uint16_t width = this->band_bytes_ * 8;
  const uint8_t header[11] = {
      0x0C,
      static_cast<uint8_t>(band_number),
      static_cast<uint8_t>(width >> 8),
      static_cast<uint8_t>(width),
      static_cast<uint8_t>(BAND_HEIGHT >> 8),
      static_cast<uint8_t>(BAND_HEIGHT),
      COMPRESSION_0X11,
      static_cast<uint8_t>(data_size >> 24),
      static_cast<uint8_t>(data_size >> 16),
      static_cast<uint8_t>(data_size >> 8),
      static_cast<uint8_t>(data_size),
  };
  this->write_(header, sizeof(header));
  this->write_(BAND_SIGNATURE, sizeof(BAND_SIGNATURE));
  QpdlWriterOut writer{this};
  this->compress_(size, writer);
  const uint8_t trailer[4] = {static_cast<uint8_t>(checksum >> 24), static_cast<uint8_t>(checksum >> 16),
                              static_cast<uint8_t>(checksum >> 8), static_cast<uint8_t>(checksum)};
  this->write_(trailer, sizeof(trailer));
  memset(this->band_, 0, size);
  return this->ok_;
}

bool QpdlEncoder::put_(uint8_t byte) {
  this->out_[this->out_len_++] = byte;
  if (this->out_len_ == sizeof(this->out_))
    return this->flush_out_();
  return this->ok_;
}

bool QpdlEncoder::write_(const uint8_t *data, size_t len) {
  while (len > 0) {
    size_t n = sizeof(this->out_) - this->out_len_;
    if (n > len)
      n = len;
    memcpy(this->out_ + this->out_len_, data, n);
    this->out_len_ += n;
    data += n;
    len -= n;
    if (this->out_len_ == sizeof(this->out_) && !this->flush_out_())
      return false;
  }
  return this->ok_;
}

bool QpdlEncoder::flush_out_() {
  if (this->out_len_ != 0 && this->ok_)
    this->ok_ = this->sink_->write(this->out_, this->out_len_);
  this->out_len_ = 0;
  return this->ok_;
}

}  // namespace esphome::xerox_phaser_3020
