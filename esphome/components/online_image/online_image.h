#pragma once
#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/image.h"
#include "esphome/core/helpers.h"

#include "image_decoder.h"

namespace esphome {
namespace online_image {
/**
 * @brief Format that the image is encoded with.
 */
enum ImageFormat {
  /** Automatically detect from MIME type. */
  AUTO,
  /** JPEG format. Not supported yet. */
  JPEG,
  /** PNG format. */
  PNG,
};

class OnlineImage : public PollingComponent, public display::Image {
 public:
  /**
   * @brief Construct a new Online Image object.
   *
   * @param url URL to download the image from.
   * @param width Desired width of the target image area.
   * @param height Desired height of the target image area.
   * @param format Format that the image is encoded in (@see ImageFormat).
   * @param buffer_size Size of the buffer used to download the image.
   */
  OnlineImage(const char *url, int width, int height, ImageFormat format, display::ImageType type,
              uint32_t buffer_size);

  bool get_pixel(int x, int y) const override;
  Color get_rgba_pixel(int x, int y) const override;
  Color get_color_pixel(int x, int y) const override;
  Color get_rgb565_pixel(int x, int y) const override;
  Color get_grayscale_pixel(int x, int y) const override;

  void update() override;

  void set_url(const char *url) { url_ = url; }
  void release();

  void set_follow_redirects(bool follow, int limit) {
    follow_redirects_ = follow;
    redirect_limit_ = limit;
  }

  void set_useragent(const char *useragent) { this->useragent_ = useragent; }
  void set_timeout(uint16_t timeout) { this->timeout_ = timeout; }

 protected:
  using Allocator = ExternalRAMAllocator<uint8_t>;
  Allocator allocator_{Allocator::Flags::ALLOW_FAILURE};

  uint32_t get_buffer_size_() const { return get_buffer_size_(width_, height_); }
  int get_buffer_size_(int width, int height) const { return std::ceil(bits_per_pixel_ * width * height / 8.0); }

  int get_position_(int x, int y) const { return ((x + y * width_) * bits_per_pixel_) / 8; }

  ESPHOME_ALWAYS_INLINE bool auto_resize_() const { return fixed_width_ == 0 || fixed_height_ == 0; }

  bool resize_(int width, int height);
  void draw_pixel_(int x, int y, Color color);

  uint8_t *buffer_;
  const char *url_;
  String etag_ = "";
  const uint32_t download_buffer_size_;
  const ImageFormat format_;
  const uint8_t bits_per_pixel_;
  const int fixed_width_;
  const int fixed_height_;

  bool follow_redirects_;
  int redirect_limit_;
  uint16_t timeout_;
  const char *useragent_;

  friend void ImageDecoder::set_size(int width, int height);
  friend void ImageDecoder::draw(int x, int y, int w, int h, const Color &color);
};

template<typename... Ts> class OnlineImageSetUrlAction : public Action<Ts...> {
 public:
  OnlineImageSetUrlAction(OnlineImage *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(const char *, url)
  void play(Ts... x) override {
    this->parent_->set_url(this->url_.value(x...));
    this->parent_->update();
  }

 protected:
  OnlineImage *parent_;
};

template<typename... Ts> class OnlineImageReleaseAction : public Action<Ts...> {
 public:
  OnlineImageReleaseAction(OnlineImage *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(const char *, url)
  void play(Ts... x) override { this->parent_->release(); }

 protected:
  OnlineImage *parent_;
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
