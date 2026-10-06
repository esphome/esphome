#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_ARTWORK)

#include "esphome/components/image/image.h"
#include "esphome/components/runtime_image/runtime_image.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include "esphome/core/helpers.h"

#include <sendspin/artwork_role.h>

#include <array>
#include <cstdint>

namespace esphome::sendspin_ {

/// @brief Decode-only RuntimeImage that decodes into a buffer owned by SendspinImageSlot.
///
/// Runs exclusively on the sendspin library's artwork decode thread. RuntimeImage's decode path
/// overwrites the fields the display reads (data_start_/width_/height_), so it must never be the
/// object shown on screen.
class ArtworkDecodeSink : public runtime_image::RuntimeImage {
 public:
  using runtime_image::RuntimeImage::RuntimeImage;

  /// @brief True when the decode ended with the given buffer still in place.
  ///
  /// An external buffer is dropped rather than resized, so a decode that wanted other dimensions
  /// leaves the sink holding nothing. The JPEG and BMP decoders report that as a decode error, but
  /// the PNG decoder ignores it and reports success, so the outcome is checked here as well.
  bool decoded_into(const uint8_t *buffer) const { return this->buffer_ == buffer; }
};

/// @brief A non-owning image::Image view over a buffer owned by SendspinImageSlot.
///
/// Each slot publishes its frames through these: one for the artwork on screen, and optionally a
/// second for the outgoing frame during a cross-fade. A view always names a frame, black to begin
/// with, so LVGL can be given it as a widget source before any artwork exists. Main loop only.
class ArtworkImageView : public image::Image {
 public:
  using image::Image::Image;

  void set_frame(const uint8_t *data, int width, int height) {
    this->data_start_ = data;
    this->width_ = width;
    this->height_ = height;
#ifdef USE_LVGL
    // Keep the descriptor LVGL is handed in step with the frame. This does not redraw anything:
    // only setting a widget's source invalidates it.
    this->get_lv_image_dsc();
#endif
  }

  /// @brief Records whether the frame on show is real artwork rather than the black it starts as.
  ///
  /// Only changes what the display lambda path draws. The frame itself is left alone, so anything
  /// reading the pixels directly (an LVGL widget) keeps drawing the last artwork until it is
  /// pointed elsewhere.
  void set_showing_artwork(bool showing_artwork) { this->showing_artwork_ = showing_artwork; }

  void set_placeholder(image::Image *placeholder) { this->placeholder_ = placeholder; }

  void draw(int x, int y, display::Display *display, Color color_on, Color color_off) override {
    if (!this->showing_artwork_) {
      // Nothing worth showing yet: the placeholder if there is one, otherwise leave the area be
      // rather than paint a blank frame over it.
      if (this->placeholder_ != nullptr) {
        this->placeholder_->draw(x, y, display, color_on, color_off);
      }
      return;
    }
    image::Image::draw(x, y, display, color_on, color_off);
  }

 protected:
  image::Image *placeholder_{nullptr};
  bool showing_artwork_{false};
};

/// @brief A single artwork slot: owns the frame buffers and publishes them to its image views.
///
/// BUFFERS: two buffers, allocated zeroed at setup and never freed. One holds the frame the current
/// image shows; the other holds the outgoing frame a transition shows, and is where the next
/// artwork is decoded. Each display swaps their roles.
///
/// THREADING: the sendspin library decodes on a dedicated thread and fires display/clear on the
/// main loop. Decoding runs into decode_sink_, which writes into the buffer the current image is
/// not showing; the swap that puts it on screen happens on the main loop. Every slot enables the
/// library's require_frame_done gate, which withholds further deliveries for the slot (buffering
/// the newest payload, latest wins) until the hub's artwork_frame_done() runs. That gate is what
/// makes two buffers enough: no decode starts while the main loop still needs the outgoing frame.
///
/// LVGL: publishing a frame to a view updates the descriptor LVGL was handed but does not
/// invalidate the widget, so every widget's source must be set again on each display.
class SendspinImageSlot : public SendspinChild {
 public:
  SendspinImageSlot(uint8_t slot, ArtworkImageView *current_image, int width, int height,
                    runtime_image::ImageFormat format, image::ImageType type, image::Transparency transparency,
                    bool is_big_endian)
      : decode_sink_(format, type, transparency, nullptr, is_big_endian, width, height),
        current_image_(current_image),
        width_(width),
        height_(height),
        slot_(slot) {}

  void setup() override;
  void dump_config() override;

  template<typename F> void add_on_image_display_callback(F &&callback) {
    this->image_display_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_image_clear_callback(F &&callback) {
    this->image_clear_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_image_error_callback(F &&callback) {
    this->image_error_callback_.add(std::forward<F>(callback));
  }

  /// @brief Sets the optional view a transition draws the outgoing artwork from.
  ///
  /// It holds the outgoing frame while a transition is running and the current frame at any other
  /// time, so it always names a picture and never the frame being decoded.
  ///
  /// Setting it is also what defers the library ack to transition_finished(): the ack releases the
  /// outgoing frame to be decoded over, and this view is the only thing that still names it.
  void set_transition_image(ArtworkImageView *transition_image) { this->transition_image_ = transition_image; }

  /// @brief Signals that the display transition for the last frame has finished.
  ///
  /// Acks the library so the next artwork can be delivered, which also hands the outgoing frame's
  /// buffer over to be decoded into. Safe no-op when no transition is pending (e.g. no transition
  /// image is configured, a clear already ended the transition, or the call is a duplicate). Must
  /// run on the main loop thread; exposed as the sendspin.image.transition_finished action.
  void transition_finished();

 protected:
  void on_decode_(const uint8_t *data, size_t length);
  bool decode_frame_(const uint8_t *data, size_t length, const uint8_t *target);
  void on_display_(uint32_t lateness_ms);
  void on_clear_();
  void finish_transition_();
  void apply_frames_(bool transition_is_artwork);
  void report_error_();

  ArtworkDecodeSink decode_sink_;

  // The two frame buffers, allocated in setup() and never freed. Their contents are written on the
  // decode thread and read by whatever draws the views, so only their roles are swapped, never the
  // pointers themselves.
  std::array<uint8_t *, 2> buffers_{};

  // pending_mutex_ guards the two fields below, the only state shared across threads. Everything
  // after them is touched on the main loop only.
  Mutex pending_mutex_;
  // Index into buffers_ of the frame the current image shows. buffers_[current_index_ ^ 1] holds
  // the outgoing frame and is the next decode target. Written on the main loop, read on the
  // decode thread.
  uint8_t current_index_{0};
  // Set on the decode thread once a frame is waiting in buffers_[current_index_ ^ 1].
  bool frame_pending_{false};

  // True once artwork has been displayed, until the next clear; decides whether the outgoing frame
  // is real artwork or the black the buffers start as. Main loop only.
  bool showing_artwork_{false};
  // True while a displayed frame awaits transition_finished(); gates duplicate or stray calls
  // so exactly one ack reaches the library per delivery. Main loop only.
  bool transition_pending_{false};

  ArtworkImageView *current_image_;
  ArtworkImageView *transition_image_{nullptr};
  int width_;
  int height_;
  uint8_t slot_;

  LazyCallbackManager<void(uint32_t)> image_display_callback_{};
  LazyCallbackManager<void()> image_clear_callback_{};
  LazyCallbackManager<void()> image_error_callback_{};
};

}  // namespace esphome::sendspin_

#endif
