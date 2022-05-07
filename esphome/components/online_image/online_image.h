#pragma once
#ifdef USE_ARDUINO

#include "esphome/components/display/display_buffer.h"

#ifdef USE_ESP32
#include <HTTPClient.h>
#endif
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#ifdef USE_HTTP_REQUEST_ESP8266_HTTPS
#include <WiFiClientSecure.h>
#endif
#endif

namespace esphome {
namespace online_image {

/**
 * @brief Format that the image is encoded with.
 */
enum ImageFormat {
  /** JPEG format. Not supported yet. */
  JPEG,
  /** PNG format. */
  PNG,
};

/**
 * @brief Image that will be downloaded and decoded in runtime.
 */
class OnlineImage {
 public:
  /**
   * @brief Construct a new Online Image object.
   *
   * @param url URL to download the image from.
   * @param width Desired widh of the target image area.
   * @param height Desired height of the target image area.
   * @param format Format that the image is encoded in (@see ImageFormat).
   */
  OnlineImage(const char *url, uint16_t width, uint16_t height, ImageFormat format)
      : url_(url), width_(width), height_(height), format_(format){};

  /**
   * @brief Draw the image on the display.
   *
   * @param x X coordinate to draw the image on.
   * @param y Y coordinate to draw the image on.
   * @param display the display to draw to.
   */
  void draw(int x, int y, display::DisplayBuffer *display, Color color_on, Color color_off);

 protected:
  const char *url_;
  const uint16_t width_;
  const uint16_t height_;
  const ImageFormat format_;
};

/**
 * @brief Class to abstract decoding different image formats.
 */
class ImageDecoder {
 public:
  /**
   * @brief Construct a new Image Decoder object
   *
   * @param display The display to draw the decoded image to.
   */
  ImageDecoder(display::DisplayBuffer *display) : display_(display){};
  virtual ~ImageDecoder() = default;

  /**
   * @brief Set the offset where the image should be drawn to.
   *
   * @param x leftmost coordinate.
   * @param y topmost coordinate.
   */
  virtual void set_offset(int x, int y) {
    offset_x_ = x;
    offset_y_ = y;
  };

  /**
   * @brief Set the on and off colors for monochrome images.
   *
   * @param color_on The color to use when the original pixel is not black.
   * @param color_off The color to use when the original pixel is black.
   */
  void set_monochrome_colors(Color color_on, Color color_off) {
    color_on_ = color_on;
    color_off_ = color_off;
  }

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
   * @return int the total number of bytes received over HTTP.
   */
  virtual size_t decode(HTTPClient &http, WiFiClient *stream) { return 0; };

  /**
   * @return the leftmost coordinate.
   */
  int x0() { return offset_x_; }
  /**
   * @return the topmost coordinate.
   */
  int y0() { return offset_y_; }
  /**
   * @return the color for on pixels.
   */
  Color color_on() { return color_on_; }
  /**
   * @return the color for off pixels.
   */
  Color color_off() { return color_off_; }

 protected:
  display::DisplayBuffer *display_;
  int offset_x_ = 0;
  int offset_y_ = 0;
  Color color_on_ = display::COLOR_ON;
  Color color_off_ = display::COLOR_OFF;
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
