#pragma once
#ifdef USE_ARDUINO

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

#include "buffer_helper.h"

namespace esphome {
namespace online_image {

/**
 * @brief Class to abstract decoding different image formats.
 */
class ImageDecoder {
 public:
  /**
   * @brief Construct a new Image Decoder object
   *
   * @param buffer The buffer to draw the decoded image to.
   */
  ImageDecoder(Buffer *buffer) : buffer_(buffer){};
  virtual ~ImageDecoder() = default;

  /**
   * @brief Initialize the decoder.
   *
   * @param stream WiFiClient to read the data from, in case the decoder needs initial data to auto-configure itself.
   */
  virtual void prepare(WiFiClient *stream){};

  /**
   * @brief Decode the image and draw it to the display.
   *
   * @param http HTTPClient object, to detect when the image has been fully downloaded.
   * @param stream The WiFiClient stream to read the image from.
   * @param buffer The buffer to use for downloading the image chunks.
   *
   * @return int the total number of bytes received over HTTP.
   */
  virtual size_t decode(HTTPClient &http, WiFiClient *stream, std::vector<uint8_t> &buffer) { return 0; };

  /**
   * @brief Request the buffer size to be set to the actual size if the image.
   *
   * @param width The image's width.
   * @param height The image's height.
   */
  void set_size(uint16_t width, uint16_t height);

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
  void draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, Color color);

  /**
   * @brief Converts an RGB color to a 1-bit grayscale color.
   *
   * @param color The color to convert from; the alpha channel will be ignored.
   * @return Whether the 8-bit grayscale equivalent color is brighter than average (i.e. brighter than 0x7F).
   */
  bool is_color_on(const Color &color);

  Buffer *buffer_;
 protected:
  double x_scale = 1.0;
  double y_scale = 1.0;
};


}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
