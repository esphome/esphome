#pragma once

#ifdef USE_DISPLAY

#include "camera.h"
#include "esphome/components/display/display.h"

namespace esphome {
namespace camera {

class CameraOverlayDisplay;

using overlay_display_writer_t = std::function<void(CameraOverlayDisplay &, CameraIncrementalContext &)>;

/** Camera overlay. Display interface implemenation.
 */
class CameraOverlayDisplay : public display::Display {
 public:
  void setup() override {
    this->set_auto_clear(false);
    this->disable_loop();
    Camera::instance()->add_overlay_callback(
        [this](camera::Buffer &image, camera::CameraImageSpec spec, camera::CameraIncrementalContext &context) {
          this->context_ = &context;
          this->data_buffer_ = image.get_data_buffer();
          this->spec_ = spec;
          this->bpr_ = spec.bytes_per_row();
          if (spec.format == camera::PIXEL_FORMAT_GRAYSCALE) {
            this->display_type_ = display::DISPLAY_TYPE_GRAYSCALE;
          } else {
            this->display_type_ = display::DISPLAY_TYPE_COLOR;
          }

          do_update_();
        });
  }

  // ---- Display interface ----
  void draw_pixel_at(int x, int y, Color color) override {
    if (x >= this->spec_.width || y >= this->spec_.height)
      return;

    switch (spec_.format) {
      case camera::PIXEL_FORMAT_GRAYSCALE: {
        data_buffer_[y * this->bpr_ + x] = color.w;
      } break;
      case camera::PIXEL_FORMAT_RGB565: {
        int idx = y * this->spec_.width + x;
        reinterpret_cast<uint16_t *>(data_buffer_)[idx] =
            (color.r & 0xF8) << 8 | (color.g & 0xFC) << 3 | (color.b & 0xF8) >> 3;
      } break;
      case camera::PIXEL_FORMAT_BGR888: {
        int idx = (y * this->spec_.width + x) * 3;
        data_buffer_[idx] = color.b;
        data_buffer_[idx + 1] = color.g;
        data_buffer_[idx + 2] = color.r;
      } break;
    }
  }
  display::DisplayType get_display_type() override { return display_type_; }
  int get_height_internal() override { return spec_.width; }
  int get_width_internal() override { return spec_.height; }
  void set_my_writer(overlay_display_writer_t &&writer) {
    this->my_writer_ = writer;
    this->set_writer([this](display::Display &d) { my_writer_(*this, *context_); });
  }
  // -------------------------------
  void update() override {}

 protected:
  camera::CameraImageSpec spec_{};
  int bpr_{0};
  display::DisplayType display_type_{display::DISPLAY_TYPE_GRAYSCALE};
  uint8_t *data_buffer_{nullptr};
  CameraIncrementalContext *context_{nullptr};
  overlay_display_writer_t my_writer_;
};

}  // namespace camera
}  // namespace esphome

#endif
