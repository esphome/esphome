#pragma once

#include "esphome/core/defines.h"

// The raw frames come from esp_video_camera, which is ESP32-P4 silicon only, so
// there is nothing here to build anywhere else.
#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/components/esp_video_camera/esp_video_camera.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::lvgl_camera_display {

/// Shows an esp_video_camera's live frames in an LVGL canvas.
///
/// Nothing is copied and nothing is decoded: the sensor's frames are already
/// RGB565, which is what LVGL draws from, so the canvas is pointed straight at
/// the capture buffer. The camera holds that buffer back from the sensor for as
/// long as the canvas is reading it -- see esp_video_camera::RawFrameConsumer.
///
/// A canvas and not an image widget. A canvas is what LVGL offers for a buffer
/// whose contents keep changing, and lv_canvas_set_draw_buf() is the call that
/// keeps the widget and the buffer it draws from in step.
class LVGLCameraDisplay : public Component, public esp_video_camera::RawFrameConsumer {
 public:
  void setup() override;
  void dump_config() override;
  /// After LVGL, whose setup creates the widget this one points at.
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_camera(esp_video_camera::ESPVideoCamera *camera) { this->camera_ = camera; }
  void set_canvas(lv_obj_t **canvas) { this->canvas_ = canvas; }

  /// Start or stop asking the camera for frames. Stopping lets the camera shut
  /// the sensor down, so a display that is switched off costs nothing.
  void set_enabled(bool enabled);
  bool is_enabled() const { return this->enabled_; }

  // esp_video_camera::RawFrameConsumer -------------------------------------------
  bool on_raw_frame(const esp_video_camera::RawFrame &frame) override;
  void on_raw_frames_stopped() override;

 protected:
  /// Stop the canvas drawing from a buffer the camera is about to take back.
  void release_canvas_();

  esp_video_camera::ESPVideoCamera *camera_{nullptr};
  /// The address of the widget pointer, not the pointer: LVGL fills its widget
  /// variables in during its own setup, and this component may be constructed
  /// before that has happened.
  lv_obj_t **canvas_{nullptr};
  bool enabled_{true};

  /// The descriptor handed to LVGL. Its geometry is fixed on the first frame;
  /// after that only the data pointer changes, which is the whole point.
  lv_draw_buf_t draw_buf_{};
  bool draw_buf_ready_{false};
  bool warned_no_canvas_{false};
};

template<typename... Ts> class StartAction : public Action<Ts...> {
 public:
  explicit StartAction(LVGLCameraDisplay *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->set_enabled(true); }

 protected:
  LVGLCameraDisplay *parent_;
};

template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(LVGLCameraDisplay *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->set_enabled(false); }

 protected:
  LVGLCameraDisplay *parent_;
};

}  // namespace esphome::lvgl_camera_display

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4
