#pragma once

#include "esphome/components/camera/camera.h"
#include "esphome/components/camera_video/encoded_stream.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace esphome::camera_video {

/** Holds the legacy camera image until the last encoded consumer releases it. */
class LegacyJpegFrame final : public EncodedVideoFrame {
 public:
  LegacyJpegFrame(std::shared_ptr<camera::CameraImage> image, uint64_t timestamp_us)
      : image_(std::move(image)), timestamp_us_(timestamp_us) {}

  uint8_t *get_data_buffer() override { return this->image_->get_data_buffer(); }
  size_t get_data_length() override { return this->image_->get_data_length(); }
  uint64_t get_timestamp_us() const override { return this->timestamp_us_; }
  bool is_keyframe() const override { return true; }

 protected:
  void on_last_reference() override {
    this->~LegacyJpegFrame();
    RAMAllocator<LegacyJpegFrame>(RAMAllocator<LegacyJpegFrame>::PREFER_INTERNAL).deallocate(this, 1);
  }

 private:
  std::shared_ptr<camera::CameraImage> image_;
  uint64_t timestamp_us_;
};

/** Adapts the existing JPEG camera to an EncodedVideoStream without modifying camera::Camera.
 *
 * The camera API offers add_listener() but no remove_listener(). This adapter must therefore
 * outlive its camera registration (normally for the lifetime of the ESPHome node).
 * Use the original RTSP_REQUESTER to preserve legacy JPEG camera behavior.
 */
class LegacyJpegStreamAdapter final : public EncodedVideoStream, public camera::CameraListener {
 public:
  LegacyJpegStreamAdapter(camera::Camera *camera, uint16_t width, uint16_t height, uint32_t fps)
      : camera_(camera), format_{VideoCodec::JPEG, width, height, fps, 1} {}

  bool get_format(EncodedVideoFormat *format) const override {
    if (format == nullptr || !this->format_.is_valid())
      return false;
    *format = this->format_;
    return true;
  }

  bool get_codec_config(uint8_t *buffer, size_t capacity, size_t *length) const override {
    (void) buffer;
    (void) capacity;
    if (length == nullptr)
      return false;
    *length = 0;  // No out-of-band JPEG codec configuration.
    return true;
  }

  bool start_stream(EncodedVideoStreamListener *listener) override {
    if (listener == nullptr || this->camera_ == nullptr || !this->format_.is_valid())
      return false;
    bool first = false;
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      if (std::find(this->listeners_.begin(), this->listeners_.end(), listener) != this->listeners_.end())
        return true;
      first = this->listeners_.empty();
      this->listeners_.push_back(listener);
      if (!this->registered_) {
        this->camera_->add_listener(this);
        this->registered_ = true;
      }
    }
    if (first)
      this->camera_->start_stream(camera::RTSP_REQUESTER);
    return true;
  }

  void stop_stream(EncodedVideoStreamListener *listener) override {
    bool last = false;
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      const auto found = std::find(this->listeners_.begin(), this->listeners_.end(), listener);
      if (found == this->listeners_.end())
        return;
      this->listeners_.erase(found);
      last = this->listeners_.empty();
    }
    if (last)
      this->camera_->stop_stream(camera::RTSP_REQUESTER);
  }

  bool is_ready() const override { return this->camera_ != nullptr && this->format_.is_valid(); }

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override {
    if (image == nullptr || !image->was_requested_by(camera::RTSP_REQUESTER))
      return;
    std::vector<EncodedVideoStreamListener *> listeners;
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      if (this->listeners_.empty())
        return;
      listeners = this->listeners_;
    }
    // One shared image and one intrusive reference-counted descriptor for every subscriber.
    RAMAllocator<LegacyJpegFrame> allocator(RAMAllocator<LegacyJpegFrame>::PREFER_INTERNAL);
    auto *storage = allocator.allocate(1);
    if (storage == nullptr)
      return;
    auto *frame = new (storage) LegacyJpegFrame(image, static_cast<uint64_t>(micros()));
    FrameRef<EncodedVideoFrame> reference(frame);
    for (auto *listener : listeners)
      listener->on_encoded_frame(reference);
  }

 private:
  camera::Camera *camera_;
  const EncodedVideoFormat format_;
  mutable std::mutex mutex_;
  std::vector<EncodedVideoStreamListener *> listeners_;
  bool registered_{false};
};

}  // namespace esphome::camera_video
