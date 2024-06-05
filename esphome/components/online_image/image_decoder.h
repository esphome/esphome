#pragma once
#ifdef USE_ARDUINO

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_ONLINE_IMAGE_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

#include "esphome/core/color.h"

namespace esphome {
namespace online_image {

class OnlineImage;

/**
 * @brief Class to abstract decoding different image formats.
 */
class ImageDecoder {
 public:
  /**
   * @brief Construct a new Image Decoder object
   *
   * @param image The image to decode the stream into.
   */
  ImageDecoder(OnlineImage *image) : image_(image) {}
  virtual ~ImageDecoder() = default;

  /**
   * @brief Initialize the decoder.
   *
   * @param stream WiFiClient to read the data from, in case the decoder needs initial data to auto-configure itself.
   * @param download_size The total number of bytes that need to be download for the image.
   */
  virtual void prepare(uint32_t download_size) { download_size_ = download_size; }

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
  virtual int decode(uint8_t *buffer, size_t size);

  /**
   * @brief Request the image to be resized once the actual dimensions are known.
   * Called by the callback functions, to be able to access the parent Image class.
   *
   * @param width The image's width.
   * @param height The image's height.
   */
  void set_size(int width, int height);

  /**
   * @brief Draw a rectangle on the display_buffer using the defined color.
   * Will check the given coordinates for out-of-bounds, and clip the rectangle accordingly.
   * In case of binary displays, the color will be converted to binary as well.
   * Called by the callback functions, to be able to access the parent Image class.
   *
   * @param x The left-most coordinate of the rectangle.
   * @param y The top-most coordinate of the rectangle.
   * @param w The width of the rectangle.
   * @param h The height of the rectangle.
   * @param color The color to draw the rectangle with.
   */
  void draw(int x, int y, int w, int h, const Color &color);

  bool is_finished() const { return decoded_bytes_ == download_size_; }

 protected:
  OnlineImage *image_;
  // Initializing to 1, to ensure it is different than initial "decoded_bytes_".
  // Will be overwritten anyway once the download size is known.
  uint32_t download_size_ = 1;
  uint32_t decoded_bytes_ = 0;
  double x_scale_ = 1.0;
  double y_scale_ = 1.0;
};

class DownloadBuffer {
 public:
  DownloadBuffer(size_t size) : buffer_(size) { reset(); }

  uint8_t *data(size_t offset = 0);

  uint8_t *append() { return data(unread_); }

  size_t unread() const { return unread_; }
  size_t size() const { return buffer_.size(); }
  size_t free_capacity() const { return buffer_.size() - unread_; }

  size_t read(size_t len);
  size_t write(size_t len) {
    unread_ += len;
    return unread_;
  }

  void reset() { unread_ = 0; }

 private:
  std::vector<uint8_t> buffer_;
  /** Total number of downloaded bytes not yet read. */
  size_t unread_;
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
