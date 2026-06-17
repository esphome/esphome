#pragma once

#include "download_buffer.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::online_image {

using t_http_codes = enum {
  HTTP_CODE_OK = 200,
  HTTP_CODE_NOT_MODIFIED = 304,
  HTTP_CODE_NOT_FOUND = 404,
};

/**
 * @brief Download an image from a given URL, and decode it using the specified decoder.
 * The image will then be stored in a buffer, so that it can be re-displayed without the
 * need to re-download or re-decode.
 */
class OnlineImage : public PollingComponent,
                    public runtime_image::RuntimeImage,
                    public Parented<esphome::http_request::HttpRequestComponent> {
 public:
  /**
   * @brief Construct a new OnlineImage object.
   *
   * @param url URL to download the image from.
   * @param width Desired width of the target image area.
   * @param height Desired height of the target image area.
   * @param format Format that the image is encoded in (@see runtime_image::ImageFormat).
   * @param type The pixel format for the image.
   * @param transparency The transparency type for the image.
   * @param placeholder Optional placeholder image to show while loading.
   * @param buffer_size Size of the buffer used to download the image.
   * @param is_big_endian Whether the image is stored in big-endian format.
   */
  OnlineImage(const std::string &url, int width, int height, runtime_image::ImageFormat format, image::ImageType type,
              image::Transparency transparency, image::Image *placeholder, uint32_t buffer_size,
              bool is_big_endian = false);

  void update() override;
  void loop() override;

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
    this->request_headers_.push_back(std::pair<std::string, TemplatableValue<std::string>>(header, value));
  }

  /**
   * Release the buffer storing the image. The image will need to be downloaded again
   * to be able to be displayed.
   */
  void release();

  template<typename F> void add_on_finished_callback(F &&callback) {
    this->download_finished_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_error_callback(F &&callback) {
    this->download_error_callback_.add(std::forward<F>(callback));
  }

 protected:
  bool validate_url_(const std::string &url);
  void end_connection_();

  CallbackManager<void(bool)> download_finished_callback_{};
  CallbackManager<void()> download_error_callback_{};

  std::shared_ptr<http_request::HttpContainer> downloader_{nullptr};
  DownloadBuffer download_buffer_;
  /**
   * This is the *initial* size of the download buffer, not the current size.
   * The download buffer can be resized at runtime; the download_buffer_initial_size_
   * will *not* change even if the download buffer has been resized.
   */
  size_t download_buffer_initial_size_;

  std::string url_;

  std::vector<std::pair<std::string, TemplatableValue<std::string>>> request_headers_;

  /**
   * The value of the ETag HTTP header provided in the last response.
   */
  std::string etag_;
  /**
   * The value of the Last-Modified HTTP header provided in the last response.
   */
  std::string last_modified_;

  uint32_t start_time_{0};
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

}  // namespace esphome::online_image
