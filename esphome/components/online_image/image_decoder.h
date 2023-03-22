#pragma once
#ifdef USE_ARDUINO

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
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
  ImageDecoder(OnlineImage *image) : image_(image){}
  virtual ~ImageDecoder() = default;

  /**
   * @brief Initialize the decoder.
   *
   * @param stream WiFiClient to read the data from, in case the decoder needs initial data to auto-configure itself.
   * @param download_size The total number of bytes that need to be download for the image.
   */
  virtual void prepare(WiFiClient *stream, uint32_t download_size){ download_size_ = download_size; }

  /**
   * @brief Decode the stream into the image.
   *
   * @param http HTTPClient object, to detect when the image has been fully downloaded.
   * @param stream The WiFiClient stream to read the image from.
   * @param buffer The buffer to use for downloading the image chunks.
   *
   * @return int the total number of bytes received over HTTP.
   */
  virtual size_t decode(HTTPClient &http, WiFiClient *stream, std::vector<uint8_t> &buffer) { return 0; }

  /**
   * @brief Request the image to be resized once the actual dimensions are known.
   *
   * @param width The image's width.
   * @param height The image's height.
   */
  void set_size(uint32_t width, uint32_t height);

  /**
   * @brief Draw a rectangle on the display_buffer using the defined color.
   * Will check the given coordinates for out-of-bounds, and clip the rectangle accordingly.
   * In case of binary displays, the color will be converted to binary as well.
   *
   * @param x The left-most coordinate of the rectangle.
   * @param y The top-most coordinate of the rectangle.
   * @param w The width of the rectangle.
   * @param h The height of the rectangle.
   * @param color The color to draw the rectangle with.
   */
  void draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const Color &color);

 protected:
  OnlineImage *image_;
  uint32_t download_size_ = 0;
  double x_scale_ = 1.0;
  double y_scale_ = 1.0;
};


}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
