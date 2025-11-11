#pragma once

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/image/image.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

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
  /** JPEG format. */
  JPEG,
  /** PNG format. */
  PNG,
  /** BMP format. */
  BMP,
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
              image::Transparency transparency, uint32_t buffer_size, bool is_big_endian);

  void draw(int x, int y, display::Display *display, Color color_on, Color color_off) override;

  void update() override;
  void loop() override;
  void map_chroma_key(Color &color);

  /** Set the URL to download the image from. */
  void set_url(const std::string &url) {
    if (this->validate_url_(url)) {
      this->url_ = url;
    }
    this->etag_ = "";
    this->last_modified_ = "";
  }

  /** Add the request header */
  template<typename V> void add_request_header(const std::string &header, V value) {
    this->request_headers_.push_back(std::pair<std::string, TemplatableValue<std::string> >(header, value));
  }

  /**
   * @brief Set the image that needs to be shown as long as the downloaded image
   *  is not available.
   *
   * @param placeholder Pointer to the (@link Image) to show as placeholder.
   */
  void set_placeholder(image::Image *placeholder) { this->placeholder_ = placeholder; }

  /**
   * Release the buffer storing the image. The image will need to be downloaded again
   * to be able to be displayed.
   */
  void release();

  /**
   * Resize the download buffer
   *
   * @param size The new size for the download buffer.
   */
  size_t resize_download_buffer(size_t size) { return this->download_buffer_.resize(size); }

  void add_on_finished_callback(std::function<void(bool)> &&callback);
  void add_on_error_callback(std::function<void()> &&callback);

 protected:
  bool validate_url_(const std::string &url);

  RAMAllocator<uint8_t> allocator_{};

  uint32_t get_buffer_size_() const { return get_buffer_size_(this->buffer_width_, this->buffer_height_); }
  int get_buffer_size_(int width, int height) const { return (this->get_bpp() * width + 7u) / 8u * height; }

  int get_position_(int x, int y) const { return (x + y * this->buffer_width_) * this->get_bpp() / 8; }

  ESPHOME_ALWAYS_INLINE bool is_auto_resize_() const { return this->fixed_width_ == 0 || this->fixed_height_ == 0; }

  /**
   * @brief Resize the image buffer to the requested dimensions.
   *
   * The buffer will be allocated if not existing.
   * If the dimensions have been fixed in the yaml config, the buffer will be created
   * with those dimensions and not resized, even on request.
   * Otherwise, the old buffer will be deallocated and a new buffer with the requested
   * allocated
   *
   * @param width
   * @param height
   * @return 0 if no memory could be allocated, the size of the new buffer otherwise.
   */
  size_t resize_(int width, int height);

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

  CallbackManager<void(bool)> download_finished_callback_{};
  CallbackManager<void()> download_error_callback_{};

  std::shared_ptr<http_request::HttpContainer> downloader_{nullptr};
  std::unique_ptr<ImageDecoder> decoder_{nullptr};

  uint8_t *buffer_;
  DownloadBuffer download_buffer_;
  /**
   * This is the *initial* size of the download buffer, not the current size.
   * The download buffer can be resized at runtime; the download_buffer_initial_size_
   * will *not* change even if the download buffer has been resized.
   */
  size_t download_buffer_initial_size_;

  const ImageFormat format_;
  image::Image *placeholder_{nullptr};

  std::string url_{""};

  std::vector<std::pair<std::string, TemplatableValue<std::string> > > request_headers_;

  /** width requested on configuration, or 0 if non specified. */
  const int fixed_width_;
  /** height requested on configuration, or 0 if non specified. */
  const int fixed_height_;
  /**
   * Whether the image is stored in big-endian format.
   * This is used to determine how to store 16 bit colors in the buffer.
   */
  bool is_big_endian_;
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
  /**
   * The value of the ETag HTTP header provided in the last response.
   */
  std::string etag_ = "";
  /**
   * The value of the Last-Modified HTTP header provided in the last response.
   */
  std::string last_modified_ = "";

  time_t start_time_;

  friend bool ImageDecoder::set_size(int width, int height);
  friend void ImageDecoder::draw(int x, int y, int w, int h, const Color &color);
};

template<typename... Ts> class OnlineImageSetUrlAction : public Action<Ts...> {
 public:
  OnlineImageSetUrlAction(OnlineImage *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(bool, update)
  void play(const Ts &...x) override {
    this->parent_->set_url(this->url_.value(x...));
    if (this->update_.value(x...)) {
      this->parent_->update();
    }
  }

 protected:
  OnlineImage *parent_;
};

template<typename... Ts> class OnlineImageReleaseAction : public Action<Ts...> {
 public:
  OnlineImageReleaseAction(OnlineImage *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->release(); }

 protected:
  OnlineImage *parent_;
};

class DownloadFinishedTrigger : public Trigger<bool> {
 public:
  explicit DownloadFinishedTrigger(OnlineImage *parent) {
    parent->add_on_finished_callback([this](bool cached) { this->trigger(cached); });
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
