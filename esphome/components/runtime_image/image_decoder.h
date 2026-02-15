#pragma once
#include "esphome/core/color.h"

namespace esphome::runtime_image {

enum DecodeError : int {
  DECODE_ERROR_INVALID_TYPE = -1,
  DECODE_ERROR_UNSUPPORTED_FORMAT = -2,
  DECODE_ERROR_OUT_OF_MEMORY = -3,
};

class RuntimeImage;

/**
 * @brief Class to abstract decoding different image formats.
 */
class ImageDecoder {
 public:
  /**
   * @brief Construct a new Image Decoder object
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  ImageDecoder(RuntimeImage *image) : image_(image) {}
  virtual ~ImageDecoder() = default;

  /**
   * @brief Initialize the decoder.
   *
   * @param expected_size Hint about the expected data size (0 if unknown).
   * @return int          Returns 0 on success, a {@see DecodeError} value in case of an error.
   */
  virtual int prepare(size_t expected_size) {
    this->expected_size_ = expected_size;
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

 protected:
  RuntimeImage *image_;
  size_t expected_size_ = 0;  // Expected data size (0 if unknown)
  size_t decoded_bytes_ = 0;  // Bytes processed so far
  double x_scale_ = 1.0;
  double y_scale_ = 1.0;
};

}  // namespace esphome::runtime_image
