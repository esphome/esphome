#pragma once
#include <cstdint>

#include "esphome/core/color.h"
#include "esphome/core/helpers.h"
#include "image_format.h"

namespace esphome::runtime_image {

enum DecodeError : int {
  DECODE_ERROR_INVALID_TYPE = -1,
  DECODE_ERROR_UNSUPPORTED_FORMAT = -2,
  DECODE_ERROR_OUT_OF_MEMORY = -3,
  DECODE_ERROR_INTERNAL_DECODER_ERROR = -4,
  DECODE_ERROR_READ = -5,
};

/// Results of ImageDecoder::decode_step() other than a DecodeError
enum DecodeStep : int {
  DECODE_STEP_DONE = 0,
  DECODE_STEP_MORE = 1,
  DECODE_STEP_NEED_DATA = 2,  // The source has nothing more yet; call again once more has arrived
};

constexpr const char *decode_error_to_string(int error) {
  switch (error) {
    case DECODE_ERROR_INVALID_TYPE:
      return "Invalid type";
    case DECODE_ERROR_UNSUPPORTED_FORMAT:
      return "Unsupported format";
    case DECODE_ERROR_OUT_OF_MEMORY:
      return "Out of memory";
    case DECODE_ERROR_INTERNAL_DECODER_ERROR:
      return "Internal decoder error";
    case DECODE_ERROR_READ:
      return "Read error";
    default:
      return "Unknown error";
  }
}

// The longest one decode() call works before it returns, leaving the rest of the data for the next call
static constexpr uint32_t DECODE_TIME_LIMIT_MS = 15;
// Pixel loops read the clock once per this many steps
static constexpr uint32_t DECODE_TIME_CHECK_STEPS = 64;

class RuntimeImage;

/// Input that a decoder reads itself as it goes, so the whole image never has to be in memory at once
class DecodeSource {
 public:
  virtual ~DecodeSource() = default;
  /**
   * Read up to len bytes at the current position. Returns the number read, which is less than len, even 0, when
   * no more has arrived yet or at the end of the input, or negative on error.
   */
  virtual int32_t read(uint8_t *buf, uint32_t len) = 0;
  /// Move to a position counted from the start. Returns false on error.
  virtual bool seek(uint32_t position) = 0;
};

/**
 * @brief Class to abstract decoding different image formats.
 */
class ImageDecoder {
 public:
  /**
   * @brief Construct a new Image Decoder object
   *
   * @param image The RuntimeImage to decode the stream into.
   * @param format The image format this decoder handles.
   */
  ImageDecoder(RuntimeImage *image, ImageFormat format) : image_(image), format_(format) {}
  virtual ~ImageDecoder() = default;

  /// @brief Get the image format handled by this decoder.
  ImageFormat get_format() const { return this->format_; }

  /// @brief Check if a decoding session is in progress (prepare() called, reset() not yet).
  bool is_active() const { return this->active_; }

  /**
   * @brief Reset the decoder state, ending any decoding session.
   *        Subclasses should override this method to reset any format-specific state.
   *        Buffers the next decode can reuse should be kept allocated to avoid heap churn.
   */
  virtual void reset() {
    this->active_ = false;
    this->expected_size_ = 0;
    this->decoded_bytes_ = 0;
    this->size_valid_ = true;
    this->draw_width_ = 0;
    this->draw_height_ = 0;
    this->direct_rgb565_ = false;
  }

  /**
   * @brief Initialize the decoder, starting a new decoding session.
   *
   * @param expected_size Hint about the expected data size (0 if unknown).
   * @return int          Returns 0 on success, a {@see DecodeError} value in case of an error.
   */
  virtual int prepare(size_t expected_size) {
    this->reset();
    this->expected_size_ = expected_size;
    this->active_ = true;
    return 0;
  }

  /**
   * @brief Decode a part of the image. It will try reading from the buffer.
   * There is no guarantee that the whole available buffer will be read/decoded;
   * the method will return the amount of bytes actually decoded, so that the
   * unread content can be moved to the beginning.
   *
   * @param buffer The buffer to read from.
   * @param size   The maximum amount of bytes that can be read from the buffer.
   * @return int   The amount of bytes read. It can be 0 if the buffer does not have enough content to meaningfully
   *               decode anything, or negative in case of a decoding error.
   */
  virtual int decode(uint8_t *buffer, size_t size) = 0;

  /// Whether this decoder can read a DecodeSource itself, a step at a time, instead of being fed data.
  virtual bool supports_source() const { return false; }

  /**
   * @brief Start decoding from a source the decoder reads itself. Call after prepare(), in place of decode().
   *
   * The image headers (everything before the image data) must be readable from the source when this is called;
   * only decode_step() can wait for more to arrive. The source must hold expected_size bytes in the end, and stay
   * valid until the session ends.
   * @return int 0 on success, or a {@see DecodeError} value.
   */
  virtual int begin_source(DecodeSource *source) { return DECODE_ERROR_UNSUPPORTED_FORMAT; }

  /**
   * @brief Decode a little more of an image started with begin_source(), keeping each call short.
   *
   * @return int DECODE_STEP_MORE while there is more to do, DECODE_STEP_NEED_DATA when the source has not
   *             delivered enough to go on, DECODE_STEP_DONE when the image is complete, or a {@see DecodeError}
   *             value.
   */
  virtual int decode_step() { return DECODE_ERROR_UNSUPPORTED_FORMAT; }

  /**
   * @brief Request the image to be resized once the actual dimensions are known.
   * Called by the callback functions, to be able to access the parent Image class.
   *
   * @param width The image's width.
   * @param height The image's height.
   * @return true if the image was resized, false otherwise.
   */
  bool set_size(int width, int height);

  /**
   * @brief Fill a rectangle on the display_buffer using the defined color.
   * Will check the given coordinates for out-of-bounds, and clip the rectangle accordingly.
   * In case of binary displays, the color will be converted to binary as well.
   * Called by the callback functions, to be able to access the parent Image class.
   *
   * @param x The left-most coordinate of the rectangle.
   * @param y The top-most coordinate of the rectangle.
   * @param w The width of the rectangle.
   * @param h The height of the rectangle.
   * @param color The fill color
   */
  void draw(int x, int y, int w, int h, const Color &color);

  /// Bytes of the image the decoder has used so far
  size_t get_decoded_bytes() const { return this->decoded_bytes_; }

  /**
   * @brief Check if the decoder has finished processing.
   *
   * This should be overridden by decoders that can detect completion
   * based on format-specific markers rather than byte counts.
   */
  virtual bool is_finished() const {
    if (this->expected_size_ > 0) {
      return this->decoded_bytes_ >= this->expected_size_;
    }
    // If size is unknown, derived classes should override this
    return false;
  }

  /// Whether is_finished() can tell a complete image from a cut-off one
  virtual bool knows_end() const { return this->expected_size_ > 0; }

  /// The image is opaque RGB565, so draw() writes it straight into the buffer
  bool uses_direct_rgb565() const { return this->direct_rgb565_; }

 protected:
  /// Resize the image for a width x height source, without preparing to draw. prepare_draw_() must follow.
  bool resize_image_(int width, int height);
  /**
   * Draw a source of width x height over the whole buffer from now on. For an opaque RGB565 image this builds the
   * tables of where each source column and row lands; without the memory for them it draws pixel by pixel instead.
   */
  void prepare_draw_(int width, int height);
  /// Paint a block of source pixels in one color straight into the buffer. Only while uses_direct_rgb565().
  void draw_rgb565_(int x, int y, int w, int h, uint16_t rgb565);

  /// The image's pixel buffer, for decoders that write their output straight into it
  uint8_t *image_buffer_() const;
  bool image_is_big_endian_() const;

  /// The first buffer column, or row, that source column x, or row y, paints: ceil(x * buffer size / draw size)
  int scaled_x_(int x) const;
  int scaled_y_(int y) const;

  // Size of the image as the decoder draws it, which draw() spreads over the buffer
  int draw_width_ = 0;
  int draw_height_ = 0;
  // Boundary n is the first buffer column, or row, that source column, or row, n paints
  FixedVector<uint16_t> x_boundaries_{};
  FixedVector<uint16_t> y_boundaries_{};
  RuntimeImage *image_;
  size_t expected_size_ = 0;  // Expected data size (0 if unknown)
  size_t decoded_bytes_ = 0;  // Bytes processed so far
  const ImageFormat format_;
  bool active_ = false;     // A decoding session is in progress
  bool size_valid_ = true;  // Last set_size() result; draw() no-ops while false
  bool direct_rgb565_ = false;
};

}  // namespace esphome::runtime_image
