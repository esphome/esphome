#pragma once

#ifdef USE_ESP_IDF
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#include "esp_heap_caps.h"

namespace esphome::mipi_dsi::testing {
// Compile the public caller contract without a renderer, network or borrowed output buffer.
inline void exercise_owned_api(MipiDsi &panel) {
  alignas(64) static uint16_t rgb[16 * 16]{};
  alignas(64) static uint8_t yuv[16 * 16 * 3 / 2]{};
  ppa_srm_oper_config_t operation{};
  operation.in.buffer = rgb;
  operation.in.pic_w = operation.in.block_w = 16;
  operation.in.pic_h = operation.in.block_h = 16;
  operation.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  operation.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  operation.scale_x = operation.scale_y = 1.0f;
  bool current = true;
  const auto admitted = [](void *context) { return *static_cast<const bool *>(context); };
  if (panel.supports_rgb565_transform()) {
    panel.draw_rgb565_transformed(operation, sizeof(rgb), 0, 0, 16, 16, admitted, &current);
    operation.in.buffer = yuv;
    operation.in.srm_cm = PPA_SRM_COLOR_MODE_YUV420;
    panel.draw_rgb565_transformed(operation, sizeof(yuv), 16, 0, 16, 16);
  }
  const size_t bytes = size_t(panel.get_width()) * panel.get_height() * sizeof(uint16_t);
  auto *snapshot = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (snapshot != nullptr) {
    panel.copy_frame_buffer_rgb565(snapshot, bytes);
    panel.copy_frame_buffer_rgb565(snapshot, bytes, display::DISPLAY_ROTATION_90_DEGREES);
    heap_caps_free(snapshot);
  }
}
}  // namespace esphome::mipi_dsi::testing
#endif
