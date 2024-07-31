#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/image/image.h"

#include "image_decoder.h"

namespace esphome {
namespace online_image {

using t_http_codes = enum {
  HTTP_CODE_OK = 200,
  HTTP_CODE_NOT_MODIFIED = 304,
  HTTP_CODE_NOT_FOUND = 404,
};

/**
 * @brief Format that the image is encoded with.
 */
enum ImageFormat {
  /** Automatically detect from MIME type. Not supported yet. */
  AUTO,
  /** JPEG format. Not supported yet. */
  JPEG,
  /** PNG format. */
  PNG,
};

/**
 * @brief Download an image from a given URL, and decode it using the specified decoder.
 * The image will then be stored in a buffer, so that it can be re-displayed without the
 * need to re-download or re-decode.
 */
class OnlineImage : public PollingComponent,
                    public image::Image,
                    public Parented<esphome::http_request::HttpRequestComponent> {
 public:
  /**
   * @brief Construct a new OnlineImage object.
   *
   * @param url URL to download the image from.
   * @param width Desired width of the target image area.
   * @param height Desired height of the target image area.
   * @param format Format that the image is encoded in (@see ImageFormat).
   * @param buffer_size Size of the buffer used to download the image.
   */
  OnlineImage(const std::string &url, int width, int height, ImageFormat format, image::ImageType type,
              uint32_t buffer_size);

  void update() override;
  void loop() override;

  /** Set the URL to download the image from. */
  void set_url(const std::string &url) {
    if (this->validate_url_(url)) {
      this->url_ = url;
    }
  }

  /**
   * Release the buffer storing the image. The image will need to be downloaded again
   * to be able to be displayed.
   */
  void release();

  void add_on_finished_callback(std::function<void()> &&callback);
  void add_on_error_callback(std::function<void()> &&callback);

 protected:
  bool validate_url_(const std::string &url);

  using Allocator = ExternalRAMAllocator<uint8_t>;
  Allocator allocator_{Allocator::Flags::ALLOW_FAILURE};

  uint32_t get_buffer_size_() const { return get_buffer_size_(this->buffer_width_, this->buffer_height_); }
  int get_buffer_size_(int width, int height) const {
    return std::ceil(image::image_type_to_bpp(this->type_) * width * height / 8.0);
  }

  int get_position_(int x, int y) const {
    return ((x + y * this->buffer_width_) * image::image_type_to_bpp(this->type_)) / 8;
  }

  ESPHOME_ALWAYS_INLINE bool auto_resize_() const { return this->fixed_width_ == 0 || this->fixed_height_ == 0; }

  bool resize_(int width, int height);

  /**
   * @brief Draw a pixel into the buffer.
   *
   * This is used by the decoder to fill the buffer that will later be displayed
   * by the `draw` method. This will internally convert the supplied 32 bit RGBA
   * color into the requested image storage format.
   *
   * @param x Horizontal pixel position.
   * @param y Vertical pixel position.
   * @param color 32 bit color to put into the pixel.
   */
  void draw_pixel_(int x, int y, Color color);

  void end_connection_();

  CallbackManager<void()> download_finished_callback_{};
  CallbackManager<void()> download_error_callback_{};

  std::shared_ptr<http_request::HttpContainer> downloader_{nullptr};
  std::unique_ptr<ImageDecoder> decoder_{nullptr};

  uint8_t *buffer_;
  DownloadBuffer download_buffer_;

  const ImageFormat format_;

  std::string url_{""};

  /** width requested on configuration, or 0 if non specified. */
  const int fixed_width_;
  /** height requested on configuration, or 0 if non specified. */
  const int fixed_height_;
  /**
   * Actual width of the current image. If fixed_width_ is specified,
   * this will be equal to it; otherwise it will be set once the decoding
   * starts and the original size is known.
   * This needs to be separate from "BaseImage::get_width()" because the latter
   * must return 0 until the image has been decoded (to avoid showing partially
   * decoded images).
   */
  int buffer_width_;
  /**
   * Actual height of the current image. If fixed_height_ is specified,
   * this will be equal to it; otherwise it will be set once the decoding
   * starts and the original size is known.
   * This needs to be separate from "BaseImage::get_height()" because the latter
   * must return 0 until the image has been decoded (to avoid showing partially
   * decoded images).
   */
  int buffer_height_;

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

class DownloadFinishedTrigger : public Trigger<> {
 public:
  explicit DownloadFinishedTrigger(OnlineImage *parent) {
    parent->add_on_finished_callback([this]() { this->trigger(); });
  }
};

class DownloadErrorTrigger : public Trigger<> {
 public:
  explicit DownloadErrorTrigger(OnlineImage *parent) {
    parent->add_on_error_callback([this]() { this->trigger(); });
  }
};

}  // namespace online_image
}  // namespace esphome
