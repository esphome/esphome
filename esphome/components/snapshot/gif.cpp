#ifdef USE_HOST
#include "gif.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace esphome::snapshot {

namespace {

constexpr size_t MAX_PALETTE_SIZE = 256;
// GIF needs at least two bits for each colour, even when the picture has fewer than four colours.
constexpr unsigned MIN_PALETTE_BITS = 2;
constexpr unsigned MAX_CODE_BITS = 12;
constexpr uint32_t MAX_CODES = 1u << MAX_CODE_BITS;
// Twice the number of codes there can be, so a lookup rarely has to look at more than one entry.
constexpr size_t DICTIONARY_SIZE = 2 * MAX_CODES;
constexpr uint32_t EMPTY_ENTRY = 0xFFFFFFFF;
constexpr size_t MAX_SUB_BLOCK_SIZE = 255;
// Colours are grouped by their top 5, 6 and 5 bits, which is all an RGB565 display can show.
constexpr size_t COLOR_KEY_COUNT = 1u << 16;

constexpr uint16_t color_key(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}

/// One of the three parts of a colour key: 0 for red, 1 for green, 2 for blue. The value is spread
/// over 0 to 255 whichever part it is, so the parts can be compared with each other.
constexpr unsigned key_channel(uint16_t key, unsigned channel) {
  switch (channel) {
    case 0:
      return (key >> 11) << 3;
    case 1:
      return ((key >> 5) & 0x3F) << 2;
    default:
      return (key & 0x1F) << 3;
  }
}

/// Store a value in two bytes, least significant first.
void put_le16(std::vector<uint8_t> &out, unsigned value) {
  out.push_back(static_cast<uint8_t>(value & 0xFF));
  out.push_back(static_cast<uint8_t>(value >> 8));
}

/// Packs codes of varying length into bytes, least significant bit first.
struct BitWriter {
  std::vector<uint8_t> &out;
  uint32_t buffer{0};
  unsigned count{0};

  void put(uint32_t code, unsigned size) {
    this->buffer |= code << this->count;
    this->count += size;
    while (this->count >= 8) {
      this->out.push_back(static_cast<uint8_t>(this->buffer & 0xFF));
      this->buffer >>= 8;
      this->count -= 8;
    }
  }

  void flush() {
    if (this->count > 0)
      this->out.push_back(static_cast<uint8_t>(this->buffer & 0xFF));
    this->buffer = 0;
    this->count = 0;
  }
};

/// Compress palette indices the way GIF asks for, appending the bytes to `out`.
void lzw_compress(const uint8_t *data, size_t length, unsigned min_code_size, std::vector<uint8_t> &out) {
  struct Entry {
    uint32_t key;  // the code before, shifted up a byte, with the next index in the low byte
    uint16_t code;
  };
  const uint32_t clear_code = 1u << min_code_size;
  const uint32_t end_code = clear_code + 1;
  std::vector<Entry> dictionary(DICTIONARY_SIZE);
  BitWriter bits{out};
  uint32_t next_code = 0;
  unsigned code_size = 0;

  auto reset = [&]() {
    std::fill(dictionary.begin(), dictionary.end(), Entry{EMPTY_ENTRY, 0});
    next_code = end_code + 1;
    code_size = min_code_size + 1;
  };

  reset();
  bits.put(clear_code, code_size);
  uint32_t prefix = data[0];
  for (size_t i = 1; i != length; i++) {
    const uint32_t key = prefix << 8 | data[i];
    size_t slot = (key * 2654435761u >> 16) & (DICTIONARY_SIZE - 1);
    while (dictionary[slot].key != EMPTY_ENTRY && dictionary[slot].key != key)
      slot = (slot + 1) & (DICTIONARY_SIZE - 1);
    if (dictionary[slot].key == key) {
      prefix = dictionary[slot].code;
      continue;
    }
    bits.put(prefix, code_size);
    if (next_code < MAX_CODES) {
      dictionary[slot] = Entry{key, static_cast<uint16_t>(next_code++)};
      // The reader is one entry behind, so it moves to longer codes a step after this does.
      if (next_code > (1u << code_size))
        code_size++;
    } else {
      bits.put(clear_code, code_size);
      reset();
    }
    prefix = data[i];
  }
  bits.put(prefix, code_size);
  // The reader adds an entry for that last code, which may take it to longer codes.
  if (next_code == (1u << code_size) && code_size < MAX_CODE_BITS)
    code_size++;
  bits.put(end_code, code_size);
  bits.flush();
}

}  // namespace

GifWriter::GifWriter(FILE *file, int width, int height)
    : file_(file),
      width_(width),
      height_(height),
      bins_(COLOR_KEY_COUNT),
      palette_index_(COLOR_KEY_COUNT),
      indices_(static_cast<size_t>(width) * height) {}

bool GifWriter::write_header() {
  this->output_.clear();
  static constexpr char SIGNATURE[] = "GIF89a";
  this->output_.insert(this->output_.end(), SIGNATURE, SIGNATURE + 6);
  put_le16(this->output_, this->width_);
  put_le16(this->output_, this->height_);
  this->output_.push_back(0x70);  // no shared colour table; 8 bits per colour
  this->output_.push_back(0);     // background colour, unused
  this->output_.push_back(0);     // pixels are square
  // Ask players to repeat the animation forever.
  static constexpr char LOOP_EXTENSION[] = "\x21\xFF\x0BNETSCAPE2.0\x03\x01";
  this->output_.insert(this->output_.end(), LOOP_EXTENSION, LOOP_EXTENSION + sizeof(LOOP_EXTENSION) - 1);
  put_le16(this->output_, 0);
  this->output_.push_back(0);
  return fwrite(this->output_.data(), 1, this->output_.size(), this->file_) == this->output_.size();
}

size_t GifWriter::build_palette_(uint8_t *palette) {
  struct Box {
    size_t begin;  // range of used_colors_, which the box owns
    size_t end;
    uint64_t pixels;
  };

  this->used_colors_.clear();
  uint64_t total = 0;
  for (size_t key = 0; key != COLOR_KEY_COUNT; key++) {
    if (this->bins_[key].count != 0) {
      this->used_colors_.push_back(static_cast<uint16_t>(key));
      total += this->bins_[key].count;
    }
  }

  // Median cut: keep cutting the box with the most pixels in two, across the colour part that
  // varies most, until there is a box for each palette entry or every box is a single colour.
  std::vector<Box> boxes;
  boxes.push_back({0, this->used_colors_.size(), total});
  while (boxes.size() < MAX_PALETTE_SIZE) {
    size_t chosen = boxes.size();
    for (size_t i = 0; i != boxes.size(); i++) {
      if (boxes[i].end - boxes[i].begin >= 2 && (chosen == boxes.size() || boxes[i].pixels > boxes[chosen].pixels))
        chosen = i;
    }
    if (chosen == boxes.size())
      break;
    const Box box = boxes[chosen];

    unsigned low[3] = {255, 255, 255};
    unsigned high[3] = {0, 0, 0};
    for (size_t i = box.begin; i != box.end; i++) {
      for (unsigned channel = 0; channel != 3; channel++) {
        const unsigned value = key_channel(this->used_colors_[i], channel);
        low[channel] = std::min(low[channel], value);
        high[channel] = std::max(high[channel], value);
      }
    }
    unsigned widest = 0;
    for (unsigned channel = 1; channel != 3; channel++) {
      if (high[channel] - low[channel] > high[widest] - low[widest])
        widest = channel;
    }
    std::sort(this->used_colors_.begin() + box.begin, this->used_colors_.begin() + box.end,
              [widest](uint16_t a, uint16_t b) {
                const unsigned value_a = key_channel(a, widest);
                const unsigned value_b = key_channel(b, widest);
                return value_a != value_b ? value_a < value_b : a < b;
              });

    // Cut where the pixels are half on one side and half on the other, keeping a colour on each.
    size_t cut = box.begin;
    uint64_t below = 0;
    while (cut < box.end - 1 && below * 2 < box.pixels)
      below += this->bins_[this->used_colors_[cut++]].count;
    boxes[chosen] = {box.begin, cut, below};
    boxes.push_back({cut, box.end, box.pixels - below});
  }

  for (size_t i = 0; i != boxes.size(); i++) {
    uint64_t sum[3] = {0, 0, 0};
    for (size_t j = boxes[i].begin; j != boxes[i].end; j++) {
      const ColorBin &bin = this->bins_[this->used_colors_[j]];
      for (unsigned channel = 0; channel != 3; channel++)
        sum[channel] += bin.sum[channel];
      this->palette_index_[this->used_colors_[j]] = static_cast<uint8_t>(i);
    }
    // The average colour of the box, rounded to the nearest.
    for (unsigned channel = 0; channel != 3; channel++)
      palette[i * 3 + channel] = static_cast<uint8_t>((sum[channel] + boxes[i].pixels / 2) / boxes[i].pixels);
  }
  return boxes.size();
}

bool GifWriter::write_frame(const uint8_t *bgr, size_t row_stride, unsigned delay_centiseconds) {
  std::fill(this->bins_.begin(), this->bins_.end(), ColorBin{});
  for (int y = 0; y != this->height_; y++) {
    const uint8_t *in = bgr + y * row_stride;
    for (int x = 0; x != this->width_; x++, in += 3) {
      ColorBin &bin = this->bins_[color_key(in[2], in[1], in[0])];
      bin.count++;
      bin.sum[0] += in[2];
      bin.sum[1] += in[1];
      bin.sum[2] += in[0];
    }
  }

  uint8_t palette[MAX_PALETTE_SIZE * 3] = {};
  const size_t colors = this->build_palette_(palette);
  unsigned palette_bits = MIN_PALETTE_BITS;
  while ((size_t{1} << palette_bits) < colors)
    palette_bits++;

  uint8_t *out_index = this->indices_.data();
  for (int y = 0; y != this->height_; y++) {
    const uint8_t *in = bgr + y * row_stride;
    for (int x = 0; x != this->width_; x++, in += 3)
      *out_index++ = this->palette_index_[color_key(in[2], in[1], in[0])];
  }

  this->output_.clear();
  // Show the frame for the time asked, then leave it in place under the next one.
  static constexpr uint8_t GRAPHIC_CONTROL[] = {0x21, 0xF9, 0x04, 0x04};
  this->output_.insert(this->output_.end(), std::begin(GRAPHIC_CONTROL), std::end(GRAPHIC_CONTROL));
  put_le16(this->output_, delay_centiseconds);
  this->output_.push_back(0);  // no transparent colour
  this->output_.push_back(0);
  this->output_.push_back(0x2C);  // the frame fills the whole picture
  put_le16(this->output_, 0);
  put_le16(this->output_, 0);
  put_le16(this->output_, this->width_);
  put_le16(this->output_, this->height_);
  this->output_.push_back(static_cast<uint8_t>(0x80 | (palette_bits - 1)));  // has its own colour table
  this->output_.insert(this->output_.end(), palette, palette + (size_t{3} << palette_bits));
  this->output_.push_back(static_cast<uint8_t>(palette_bits));

  std::vector<uint8_t> compressed;
  lzw_compress(this->indices_.data(), this->indices_.size(), palette_bits, compressed);
  for (size_t pos = 0; pos < compressed.size(); pos += MAX_SUB_BLOCK_SIZE) {
    const size_t length = std::min(MAX_SUB_BLOCK_SIZE, compressed.size() - pos);
    this->output_.push_back(static_cast<uint8_t>(length));
    this->output_.insert(this->output_.end(), compressed.begin() + pos, compressed.begin() + pos + length);
  }
  this->output_.push_back(0);
  return fwrite(this->output_.data(), 1, this->output_.size(), this->file_) == this->output_.size();
}

bool GifWriter::write_trailer() {
  const uint8_t trailer = 0x3B;
  return fwrite(&trailer, 1, 1, this->file_) == 1;
}

}  // namespace esphome::snapshot
#endif
