#include "lvgl_camera_display.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/core/log.h"

namespace esphome::lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  if (this->camera_ == nullptr) {
    this->mark_failed(LOG_STR("No camera"));
    return;
  }
  if (!this->camera_->has_raw_frames()) {
    // A USB-UVC camera, or any device node already producing JPEG. There are no
    // pixels to point the widget at, only a compressed image, and decoding it
    // every frame is not something this component does.
    this->mark_failed(LOG_STR("The camera delivers JPEG, not pixels; only a MIPI-CSI sensor can drive a widget"));
    return;
  }
  this->camera_->set_raw_frame_consumer(this);
  this->camera_->request_raw_frames(this->enabled_);
}

void LVGLCameraDisplay::set_enabled(bool enabled) {
  this->enabled_ = enabled;
  if (this->camera_ != nullptr)
    this->camera_->request_raw_frames(enabled);
  // Switching off gives the camera its buffer back, so the widget must stop
  // drawing from it before the sensor starts writing over it again.
  if (!enabled)
    this->release_widget_();
}

void LVGLCameraDisplay::release_widget_() {
  lv_obj_t *widget = (this->widget_ == nullptr) ? nullptr : *this->widget_;
  if (widget != nullptr) {
    ::lv_image_set_src(widget, nullptr);
    lv_obj_invalidate(widget);
  }
  // Rebuilt from the first frame that arrives next, which may have a different
  // geometry anyway.
  this->draw_buf_ready_ = false;
  this->draw_buf_.data = nullptr;
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LVGL Camera Display:\n"
                "  Camera: %s\n"
                "  Enabled: %s",
                this->camera_ == nullptr ? "none" : this->camera_->get_name().c_str(), YESNO(this->enabled_));
}

bool LVGLCameraDisplay::on_raw_frame(const esp_video_camera::RawFrame &frame) {
  lv_obj_t *widget = (this->widget_ == nullptr) ? nullptr : *this->widget_;
  if (widget == nullptr) {
    if (!this->warned_no_widget_) {
      this->warned_no_widget_ = true;
      ESP_LOGW(TAG, "The widget does not exist yet; frames are being dropped");
    }
    return false;
  }

  if (!this->draw_buf_ready_) {
    // Which widget this is decides how the buffer reaches it, and LVGL is the
    // authority on that -- not the configuration, which only had an ID.
    this->widget_is_canvas_ = lv_obj_check_type(widget, &lv_canvas_class);
    lv_draw_buf_init(&this->draw_buf_, frame.width, frame.height, LV_COLOR_FORMAT_RGB565, frame.stride,
                     (void *) frame.data, frame.stride * frame.height);
    // Without this LVGL treats the buffer as constant image data and may skip
    // the redraw when the pointer it is given has not changed.
    lv_draw_buf_set_flag(&this->draw_buf_, LV_IMAGE_FLAGS_MODIFIABLE);
    this->draw_buf_ready_ = true;
    ESP_LOGD(TAG, "Showing %ux%u frames in an LVGL %s", (unsigned) frame.width, (unsigned) frame.height,
             this->widget_is_canvas_ ? "canvas" : "image");
  } else {
    this->draw_buf_.data = (uint8_t *) frame.data;
  }

  if (this->widget_is_canvas_) {
    // Sets the canvas's own draw buffer as well as the image source behind it;
    // lv_image_set_src() alone would leave the two disagreeing.
    lv_canvas_set_draw_buf(widget, &this->draw_buf_);
  } else {
    ::lv_image_set_src(widget, &this->draw_buf_);
  }
  lv_obj_invalidate(widget);

  // Keep this frame. LVGL renders from it during its own loop, which runs in
  // this same task, so it is finished with it well before the next frame
  // arrives -- and that is when the camera takes this buffer back.
  return true;
}

void LVGLCameraDisplay::on_raw_frames_stopped() {
  // The buffers are about to be unmapped, so nothing may still be reading from
  // them: neither the descriptor nor the widget LVGL would redraw from it.
  this->release_widget_();
}

}  // namespace esphome::lvgl_camera_display

#endif  // USE_ESP_IDF && USE_ESP32_VARIANT_ESP32P4
