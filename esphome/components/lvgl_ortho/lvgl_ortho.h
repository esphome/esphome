// SPDX-License-Identifier: MIT
// Copyright (c) 2026 youkorr
// Part of ppa-alternative: https://github.com/youkorr/ppa-alternative-
#pragma once

// lv_draw_ortho on an ESPHome board.
//
// ESPHome's lvgl renders a turned screen upright in small bands and turns
// every band in its flush, through the PPA on an ESP32-P4. This component
// takes LVGL's display over once lvgl has set it up:
//
//  - LVGL renders into ONE buffer of the panel's own size and orientation
//    (LV_DISPLAY_RENDER_MODE_DIRECT), through lv_draw_ortho, so nothing is
//    ever turned after it is drawn;
//  - the flush copies the rows that changed to the display as they are;
//  - a video (the P4's camera) is turned once by the PPA straight into a
//    window of that buffer that LVGL keeps out of (show_video).
//
// The lvgl: block must carry NO rotation: -- the turn is this component's,
// and LVGL's resolution must still be the panel's when it takes over.

#include "esphome/core/component.h"
#include "esphome/components/display/display.h"

#include <lvgl.h>

#include "lv_draw_ortho.h"
#include "lv_draw_ortho_video.h"

#ifdef USE_LVGL_ORTHO_NATIVE_DSI
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#endif

// youkorr/lvgl_9.5's mipi_dsi publishes the panel's own scan-out buffers
// here when it is given `framebuffers: 2` or `3`. ESPHome's own mipi_dsi has
// no such header, and then this component copies into the display instead.
#if __has_include("esphome/components/mipi_dsi/mipi_dsi_fast_present.h")
#include "esphome/components/mipi_dsi/mipi_dsi_fast_present.h"
#define LVGL_ORTHO_FAST_PRESENT 1
#else
#define LVGL_ORTHO_FAST_PRESENT 0
#endif

namespace esphome {
namespace lvgl_ortho {

class LvglOrtho : public Component {
 public:
  void set_display(display::Display *display) { this->display_ = display; }
#ifdef USE_LVGL_ORTHO_NATIVE_DSI
  void set_native_dsi(mipi_dsi::MipiDsi *panel) { this->native_dsi_ = panel; }
#endif
  // ESPHome's degrees, clockwise -- the number the lvgl: block would have.
  void set_rotation(int degrees) { this->degrees_ = degrees; }
  // Turn tiles with the PPA, the CPU drawing the next one meanwhile; false
  // turns them on the CPU, which is what to compare the PPA against.
  void set_ppa(bool ppa) { this->ppa_ = ppa; }
  void set_cores(int cores) { this->cores_ = cores; }
  // Bytes of PSRAM for pictures turned once (lv_draw_ortho_set_image_cache).
  void set_image_cache(size_t bytes) { this->image_cache_ = bytes; }
  // Bytes of internal RAM, both bands together, for turning a big picture in
  // PSRAM through it (lv_draw_ortho_ppa_set_stage); 0 turns it straight.
  void set_camera_ram(size_t bytes) { this->camera_ram_ = bytes; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After lvgl, which builds the display in its own setup().
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Called from other components: youkorr/esphome_esp-video's
  // lvgl_camera_display (ortho_id:), and possibly Portall's canvas mode one
  // day. Keep show_video(), clear_video() and is_ready() as they are --
  // same signatures, callable from the loop -- or change their callers too.

  /// Show one video frame (RGB565, `width` x `height`) in `obj`'s place,
  /// turned once by the PPA. The frame is centred in the object the way an
  /// lv_image centres its source; whatever does not fit is cropped. Call it
  /// from the loop (an interval, an lv_timer) -- never from another task.
  /// False if the frame could not be shown; the reason is logged once.
  bool show_video(lv_obj_t *obj, const uint8_t *frame, uint16_t width, uint16_t height, bool mirror_x = false,
                  bool mirror_y = false);
  /// Give the window back to LVGL, which redraws what is under it.
  void clear_video();

  bool is_ready() const { return this->ready_; }
  /// True when LVGL draws straight into the panel's own frame buffers.
  bool on_panel_buffers() const { return this->panel_fbs_ > 0; }
  // LVGL's copies between the panel's buffers made by lv_draw_ortho instead
  // (lv_draw_ortho_take_sync): only what the next frame does not draw, by
  // the PPA.
  bool copies_taken() const { return this->sync_on_; }

 protected:
  static void flush_cb_(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
  static void render_start_cb_(lv_event_t *e);
  void copy_native_(const lv_area_t &native);
  bool on_screen_(lv_obj_t *obj) const;

  display::Display *display_{nullptr};
  int degrees_{0};
  bool ppa_{true};
  int cores_{1};
  bool cores_on_{false};
  bool sync_on_{false};
  size_t image_cache_{0};
  size_t camera_ram_{0};
  bool ppa_on_{false};
  bool ppa_lost_{false};  // said once, and the screen drawn again
  lv_display_t *disp_{nullptr};
  uint8_t *buf_{nullptr};
  uint32_t stride_{0};
  int32_t width_{0}, height_{0};  // the panel's
  uint8_t *scratch_{nullptr};     // a narrow area packed into rows before it goes out
  size_t scratch_len_{0};
  bool ready_{false};
  // How many of the panel's own frame buffers LVGL draws into: 0 (copy mode),
  // 2 or 3. Shown by swapping the scan-out pointer, so nothing is copied.
  int panel_fbs_{0};
#ifdef USE_LVGL_ORTHO_NATIVE_DSI
  mipi_dsi::MipiDsi *native_dsi_{nullptr};
  bool native_present_failed_{false};
  uint32_t native_present_us_{0}, native_present_count_{0};
#endif
  lv_draw_buf_t third_{};
  uint32_t presents_{0};
  uint32_t renders_{0};  // LVGL refreshes that drew something, shown or not

  lv_draw_ortho_video_t video_{};
  bool video_init_{false};
  lv_obj_t *video_obj_{nullptr};
  lv_area_t video_ui_{};      // the part of the object the frame covers
  lv_area_t video_crop_{};    // the part of the frame shown
  bool video_failed_{false};  // said once

  // Time spent, summed between two dump lines (see loop()).
  uint32_t flush_us_{0}, flush_rows_{0}, video_us_{0}, video_frames_{0}, last_report_{0};
  bool use_panel_buffers_();
};

}  // namespace lvgl_ortho
}  // namespace esphome
