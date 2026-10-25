#include "esp32_camera_window.h"

#if defined(USE_ESP32_CAMERA_WINDOW) && defined(USE_ESP32)

#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::esp32_camera_window {

ESPHOME_LOG_TAG(TAG, "esp32_camera_window");

// OV2640 DSP-bank crop registers; a register number carries its bank in bit 8.
static constexpr int OV2640_REG_HSIZE = 0x0051;
static constexpr int OV2640_REG_VSIZE = 0x0052;
static constexpr int OV2640_REG_XOFFL = 0x0053;
static constexpr int OV2640_REG_YOFFL = 0x0054;
static constexpr int OV2640_REG_VHYX = 0x0055;
// A size register covers four pixels, so a crop is a multiple of four.
static constexpr int OV2640_CROP_STEP = 4;
// JPEG compresses whole 16x8 blocks; a frame that does not fill them is striped.
static constexpr int OV2640_JPEG_BLOCK_WIDTH = 16;
static constexpr int OV2640_JPEG_BLOCK_HEIGHT = 8;
// Readout the OV2640 crops out of, in the order the driver enumerates it (not exported).
static constexpr int OV2640_MODE_UXGA = 0;
static constexpr int OV2640_MODE_SVGA = 1;
static constexpr int OV2640_MODE_CIF = 2;

// 16-bit registers of the OV3660 and OV5640: x is the low byte, y the high byte, hence two apart.
static constexpr int OV_REG_START = 0x3800;   // X_ADDR_ST, Y_ADDR_ST
static constexpr int OV_REG_END = 0x3804;     // X_ADDR_END, Y_ADDR_END
static constexpr int OV_REG_TOTAL = 0x380C;   // X_TOTAL_SIZE, Y_TOTAL_SIZE
static constexpr int OV_REG_OFFSET = 0x3810;  // X_OFFSET, Y_OFFSET
static constexpr int OV_REG_PAIR_STEP = 2;
static constexpr int OV_REG_MASK = 0xFFFF;

// Paged SC window registers; set_reg() selects the page. Each axis has start, end and high bits.
static constexpr int SC_REG_H_START = 0x0170;  // H start, H end, high bits of both
static constexpr int SC_REG_V_START = 0x0173;  // V start, V end, high bits of both
static constexpr int SC_REG_GROUP = 3;         // start, end and high bits of both
// The readout an SC window is placed in, and the high bits the sensor takes per edge.
struct ScWindowFormat {
  int area_width;
  int area_height;
  int edge_bits;
};
static constexpr ScWindowFormat SC101IOT_FORMAT{1280, 720, 4};
static constexpr ScWindowFormat SC030IOT_FORMAT{640, 480, 2};

/// Restores the full readout the driver wants for the current frame size, so windows do not nest.
static bool restore_full_readout(sensor_t *sensor) {
  const int ret = sensor->set_framesize(sensor, sensor->status.framesize);
  if (ret != 0) {
    ESP_LOGE(TAG, "Restoring the full readout failed with %d", ret);
    return false;
  }
  return true;
}

/// A pair of x and y values as a sensor register holds them.
struct SensorPair {
  int x{0};
  int y{0};
};

/// Reads the 16-bit registers at reg and reg + 2, reporting whether they answered.
static bool read_ov_pair(sensor_t *sensor, int reg, SensorPair *value) {
  const int x = sensor->get_reg(sensor, reg, OV_REG_MASK);
  const int y = sensor->get_reg(sensor, reg + OV_REG_PAIR_STEP, OV_REG_MASK);
  if (x < 0 || y < 0) {
    ESP_LOGW(TAG, "Register 0x%04X could not be read", reg);
    return false;
  }
  value->x = x;
  value->y = y;
  return true;
}

/// The readout area an OV3660 or OV5640 is set to, read back from the driver's registers.
/// Reading it back keeps the frame mapping right without repeating the driver's tables.
struct OvReadout {
  SensorPair start;   // first sensor pixel that is read out
  SensorPair offset;  // margin the driver leaves to its image pipeline on each side
  SensorPair total;   // line and frame timing the driver wrote, kept as it was
  int span_x{0};      // sensor pixels that are read out
  int span_y{0};
  int factor{1};  // sensor pixels that make up one readout pixel

  int visible_x() const { return this->span_x / this->factor - 2 * this->offset.x; }
  int visible_y() const { return this->span_y / this->factor - 2 * this->offset.y; }
};

/// Reads the readout area back from the registers the driver wrote.
static bool read_ov_readout(sensor_t *sensor, OvReadout *readout) {
  SensorPair end;
  if (!read_ov_pair(sensor, OV_REG_START, &readout->start) || !read_ov_pair(sensor, OV_REG_END, &end) ||
      !read_ov_pair(sensor, OV_REG_OFFSET, &readout->offset) || !read_ov_pair(sensor, OV_REG_TOTAL, &readout->total))
    return false;

  readout->span_x = end.x - readout->start.x + 1;
  readout->span_y = end.y - readout->start.y + 1;
  // A binned sensor reads two pixels as one and hands half as many of them to its image pipeline.
  readout->factor = sensor->status.binning ? 2 : 1;
  if (readout->span_x <= 0 || readout->span_y <= 0) {
    ESP_LOGW(TAG, "The sensor reported a readout of %d by %d pixels", readout->span_x, readout->span_y);
    return false;
  }
  return true;
}

/// A rectangle, in the pixels of whatever it is measured against.
struct Rect {
  int offset_x{0};
  int offset_y{0};
  int width{0};
  int height{0};
};

/// Keeps a window inside a frame of the given size.
static Rect clamp_window(const Esp32CameraWindow::Window &window, int frame_width, int frame_height) {
  Rect rect;
  rect.width = std::min(window.width, frame_width);
  rect.height = std::min(window.height, frame_height);
  rect.offset_x = std::clamp(window.offset_x, 0, frame_width - rect.width);
  rect.offset_y = std::clamp(window.offset_y, 0, frame_height - rect.height);
  return rect;
}

/// Maps a window onto a larger area, keeping the same part of the picture and at least one pixel.
static Rect scale_window(const Rect &window, int frame_width, int frame_height, int area_width, int area_height) {
  Rect rect;
  rect.width = std::clamp(window.width * area_width / frame_width, 1, area_width);
  rect.height = std::clamp(window.height * area_height / frame_height, 1, area_height);
  // Rounding down can ask for a pixel past the far edge.
  rect.offset_x = std::clamp(window.offset_x * area_width / frame_width, 0, area_width - rect.width);
  rect.offset_y = std::clamp(window.offset_y * area_height / frame_height, 0, area_height - rect.height);
  return rect;
}

/// Reads out a window on an OV3660 or OV5640.
static bool set_ov_window(sensor_t *sensor, const Esp32CameraWindow::Window &window, int frame_width,
                          int frame_height) {
  const Rect frame_window = clamp_window(window, frame_width, frame_height);
  // A full-frame window is the full readout, which already has the timing and scaling the driver wants.
  if (frame_window.width == frame_width && frame_window.height == frame_height)
    return restore_full_readout(sensor);

  // Start from the full readout, or the new window would nest inside the previous one.
  if (!restore_full_readout(sensor))
    return false;
  OvReadout readout;
  if (!read_ov_readout(sensor, &readout))
    return false;

  const Rect crop = scale_window(frame_window, frame_width, frame_height, readout.visible_x(), readout.visible_y());

  // The margin is part of the readout the sensor crops away again, so a crop includes one on each side.
  const int crop_width = crop.width + 2 * readout.offset.x;
  const int crop_height = crop.height + 2 * readout.offset.y;
  const int start_x = readout.start.x + readout.factor * crop.offset_x;
  const int start_y = readout.start.y + readout.factor * crop.offset_y;
  const int end_x = start_x + readout.factor * crop_width - 1;
  const int end_y = start_y + readout.factor * crop_height - 1;

  // The sensor only scales down (OV3660 datasheet, OV5640 application note), so a crop smaller
  // than the frame cannot fill it.
  if (crop.width < frame_width || crop.height < frame_height) {
    ESP_LOGE(TAG, "A %dx%d window leaves %dx%d pixels to fill the %dx%d frame; this sensor only scales down",
             frame_window.width, frame_window.height, crop.width, crop.height, frame_width, frame_height);
    return false;
  }
  // At the same size there is nothing to scale, and the driver leaves the scaler off then too.
  const bool scale = crop.width > frame_width || crop.height > frame_height;

  const int ret =
      sensor->set_res_raw(sensor, start_x, start_y, end_x, end_y, readout.offset.x, readout.offset.y, readout.total.x,
                          readout.total.y, frame_width, frame_height, scale, sensor->status.binning);
  if (ret != 0) {
    ESP_LOGE(TAG, "Reading out %d,%d to %d,%d failed with %d", start_x, start_y, end_x, end_y, ret);
    return false;
  }

  ESP_LOGI(TAG, "Window at %d,%d of %dx%d reads out %d,%d to %d,%d and fills the %dx%d frame", frame_window.offset_x,
           frame_window.offset_y, frame_window.width, frame_window.height, start_x, start_y, end_x, end_y, frame_width,
           frame_height);
  return true;
}

/// The crop an OV2640 is set to, in the pixels of the readout it is in.
struct Ov2640Crop {
  int offset_x{0};
  int offset_y{0};
  int width{0};
  int height{0};
};

/// Reads the full crop back from the driver's registers; the caller restores the frame size first.
static bool read_ov2640_crop(sensor_t *sensor, Ov2640Crop *crop) {
  const int hsize = sensor->get_reg(sensor, OV2640_REG_HSIZE, 0xFF);
  const int vsize = sensor->get_reg(sensor, OV2640_REG_VSIZE, 0xFF);
  const int xoffl = sensor->get_reg(sensor, OV2640_REG_XOFFL, 0xFF);
  const int yoffl = sensor->get_reg(sensor, OV2640_REG_YOFFL, 0xFF);
  const int vhyx = sensor->get_reg(sensor, OV2640_REG_VHYX, 0xFF);
  if (hsize < 0 || vsize < 0 || xoffl < 0 || yoffl < 0 || vhyx < 0) {
    ESP_LOGW(TAG, "The crop registers of the OV2640 could not be read");
    return false;
  }
  // The top bit of both sizes and both offsets shares VHYX.
  crop->width = (hsize | ((vhyx & 0x08) << 5)) * OV2640_CROP_STEP;
  crop->height = (vsize | ((vhyx & 0x80) << 1)) * OV2640_CROP_STEP;
  crop->offset_x = xoffl | ((vhyx & 0x07) << 8);
  crop->offset_y = yoffl | ((vhyx & 0x70) << 4);
  if (crop->width < OV2640_CROP_STEP || crop->height < OV2640_CROP_STEP) {
    ESP_LOGW(TAG, "The OV2640 reported a crop of %d by %d pixels", crop->width, crop->height);
    return false;
  }
  return true;
}

/// The readout the frame size in use needs, as the driver's set_framesize() chooses it.
static int ov2640_mode_for(framesize_t framesize) {
  if (framesize <= FRAMESIZE_CIF)
    return OV2640_MODE_CIF;
  if (framesize <= FRAMESIZE_SVGA)
    return OV2640_MODE_SVGA;
  return OV2640_MODE_UXGA;
}

/// True when a window may change the size of the published frame; only with JPEG output.
static bool window_output_supported(sensor_t *sensor) {
  return sensor->id.PID != OV2640_PID || sensor->pixformat == PIXFORMAT_JPEG;
}

/// Reads out a window on an OV2640.
static bool set_ov2640_window(sensor_t *sensor, const Esp32CameraWindow::Window &window, int frame_width,
                              int frame_height) {
  Rect frame_window = clamp_window(window, frame_width, frame_height);
  // Round the window itself, not just the size it is scaled to, so crop and frame stay aligned.
  if (sensor->pixformat == PIXFORMAT_JPEG) {
    frame_window.width =
        std::max(frame_window.width - frame_window.width % OV2640_JPEG_BLOCK_WIDTH, OV2640_JPEG_BLOCK_WIDTH);
    frame_window.height =
        std::max(frame_window.height - frame_window.height % OV2640_JPEG_BLOCK_HEIGHT, OV2640_JPEG_BLOCK_HEIGHT);
    // A window smaller than one block grows to it, which can push it past the far edge.
    frame_window.offset_x = std::clamp(frame_window.offset_x, 0, frame_width - frame_window.width);
    frame_window.offset_y = std::clamp(frame_window.offset_y, 0, frame_height - frame_window.height);
  }
  // A full-frame window is the full readout.
  if (frame_window.width == frame_width && frame_window.height == frame_height)
    return restore_full_readout(sensor);

  // Start from the full readout, or the new window would nest inside the previous one.
  if (!restore_full_readout(sensor))
    return false;
  Ov2640Crop crop;
  if (!read_ov2640_crop(sensor, &crop))
    return false;

  const Rect area = scale_window(frame_window, frame_width, frame_height, crop.width, crop.height);

  // The crop is a multiple of four pixels, counted from the corner of the crop in use.
  const int offset_x = std::clamp(area.offset_x - area.offset_x % OV2640_CROP_STEP, 0, crop.width - OV2640_CROP_STEP);
  const int offset_y = std::clamp(area.offset_y - area.offset_y % OV2640_CROP_STEP, 0, crop.height - OV2640_CROP_STEP);
  int width = std::min(area.width, crop.width - offset_x);
  int height = std::min(area.height, crop.height - offset_y);
  width = std::max(width - width % OV2640_CROP_STEP, OV2640_CROP_STEP);
  height = std::max(height - height % OV2640_CROP_STEP, OV2640_CROP_STEP);
  // Rounding the size down can leave it past the far edge when the remainder is small.
  width = std::min(width, crop.width - offset_x);
  height = std::min(height, crop.height - offset_y);

  // The crop is in readout pixels; the pipeline scales it to outputX/outputY (the window size),
  // so a crop larger than the frame cannot overrun the buffers sized for it.
  const int output_x = std::max(frame_window.width - frame_window.width % OV2640_CROP_STEP, OV2640_CROP_STEP);
  const int output_y = std::max(frame_window.height - frame_window.height % OV2640_CROP_STEP, OV2640_CROP_STEP);
  const int ret =
      sensor->set_res_raw(sensor, ov2640_mode_for(sensor->status.framesize), 0, 0, 0, crop.offset_x + offset_x,
                          crop.offset_y + offset_y, width, height, output_x, output_y, false, false);
  if (ret != 0) {
    ESP_LOGE(TAG, "Cropping to offset %d,%d size %dx%d failed with %d", crop.offset_x + offset_x,
             crop.offset_y + offset_y, width, height, ret);
    return false;
  }

  ESP_LOGI(TAG, "Window at %d,%d of %dx%d crops to offset %d,%d size %dx%d of the %dx%d readout and is scaled to %dx%d",
           frame_window.offset_x, frame_window.offset_y, frame_window.width, frame_window.height,
           crop.offset_x + offset_x, crop.offset_y + offset_y, width, height, crop.width, crop.height, output_x,
           output_y);
  return true;
}

/// Writes both edges of one axis of an SC window; the end edge is the first pixel outside it.
///
/// Not named start/end: clang-tidy reads SC_REG_H_START as a swapped argument.
static bool write_sc_axis(sensor_t *sensor, int reg, int first, int last, int edge_bits) {
  const int mask = (1 << edge_bits) - 1;
  // The high bits of both edges share the third register of the group.
  const int values[SC_REG_GROUP] = {first & 0xFF, last & 0xFF, ((first >> 8) & mask) | (((last >> 8) & mask) << 4)};
  for (int offset = 0; offset < SC_REG_GROUP; offset++) {
    const int ret = sensor->set_reg(sensor, reg + offset, 0xFF, values[offset]);
    if (ret != 0) {
      ESP_LOGE(TAG, "Writing register 0x%04X failed with %d", reg + offset, ret);
      return false;
    }
  }
  return true;
}

/// Reads out a window on an SC101IOT or SC030IOT, counted from the corner of the sensor readout.
static bool set_sc_window(sensor_t *sensor, const Esp32CameraWindow::Window &window, const ScWindowFormat &format) {
  // Size first: the offsets clamp against a window that already fits.
  const int width = std::clamp(window.width, 1, format.area_width);
  const int height = std::clamp(window.height, 1, format.area_height);
  const int offset_x = std::clamp(window.offset_x, 0, format.area_width - width);
  const int offset_y = std::clamp(window.offset_y, 0, format.area_height - height);

  if (!write_sc_axis(sensor, SC_REG_H_START, offset_x, offset_x + width, format.edge_bits) ||
      !write_sc_axis(sensor, SC_REG_V_START, offset_y, offset_y + height, format.edge_bits))
    return false;

  ESP_LOGI(TAG, "Window at %d,%d of %dx%d reads out %dx%d at %d,%d of the %dx%d sensor", window.offset_x,
           window.offset_y, window.width, window.height, width, height, offset_x, offset_y, format.area_width,
           format.area_height);
  return true;
}

float Esp32CameraWindow::get_setup_priority() const {
  // The sensor exists only after the camera's setup().
  return setup_priority::PROCESSOR;
}

void Esp32CameraWindow::setup() {
  sensor_t *sensor = this->checked_sensor_("setup");
  if (sensor == nullptr)
    return;
  if (!is_window_supported(sensor)) {
    ESP_LOGW(TAG, "%s does not support windows, so no window is used", sensor_name(sensor));
    return;
  }

  if (this->has_initial_window_) {
    this->set_window(this->initial_window_.offset_x, this->initial_window_.offset_y, this->initial_window_.width,
                     this->initial_window_.height);
  }
}

void Esp32CameraWindow::dump_config() {
  ESP_LOGCONFIG(TAG, "Camera window:");
  if (this->camera_ == nullptr) {
    ESP_LOGCONFIG(TAG, "  No camera");
    return;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    ESP_LOGCONFIG(TAG, "  No sensor");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Sensor: %s", sensor_name(sensor));
  ESP_LOGCONFIG(TAG, "  Window support: %s", YESNO(this->supports_window()));
  if (this->has_initial_window_) {
    ESP_LOGCONFIG(TAG, "  Window: offset %d,%d size %dx%d", this->initial_window_.offset_x,
                  this->initial_window_.offset_y, this->initial_window_.width, this->initial_window_.height);
  }
}

bool Esp32CameraWindow::set_window(int offset_x, int offset_y, int width, int height) {
  sensor_t *sensor = this->checked_sensor_("set_window");
  if (sensor == nullptr)
    return false;
  if (!is_window_supported(sensor)) {
    ESP_LOGE(TAG, "%s does not support windows", sensor_name(sensor));
    return false;
  }

  const Window window{offset_x, offset_y, width, height};
  if (!this->set_sensor_window_(sensor, window))
    return false;

  ESP_LOGI(TAG, "Window set to offset %d,%d size %dx%d", offset_x, offset_y, width, height);
  return true;
}

bool Esp32CameraWindow::reset_window() {
  sensor_t *sensor = this->checked_sensor_("reset_window");
  if (sensor == nullptr)
    return false;
  if (!is_window_supported(sensor)) {
    ESP_LOGE(TAG, "%s does not support windows", sensor_name(sensor));
    return false;
  }

  // set_framesize() programs the full readout again; the next captures drain stale frames on their own.
  if (!restore_full_readout(sensor)) {
    ESP_LOGE(TAG, "Reading out the whole sensor again failed");
    return false;
  }

  ESP_LOGI(TAG, "Window cleared, reading out the whole sensor");
  return true;
}

bool Esp32CameraWindow::supports_window() const {
  if (this->camera_ == nullptr)
    return false;
  sensor_t *sensor = esp_camera_sensor_get();
  return sensor != nullptr && is_window_supported(sensor) && window_output_supported(sensor);
}

bool Esp32CameraWindow::set_sensor_window_(sensor_t *sensor, const Window &window) {
  if (sensor->status.framesize >= FRAMESIZE_INVALID) {
    ESP_LOGE(TAG, "The camera is not set to a valid frame size");
    return false;
  }
  const resolution_info_t frame = resolution[sensor->status.framesize];
  if (!window_output_supported(sensor)) {
    ESP_LOGE(TAG, "%s can only read out a window with JPEG output; set pixel_format to JPEG", sensor_name(sensor));
    return false;
  }
  switch (sensor->id.PID) {
    case OV2640_PID:
      return set_ov2640_window(sensor, window, frame.width, frame.height);
    case OV3660_PID:
    case OV5640_PID:
      return set_ov_window(sensor, window, frame.width, frame.height);
    case SC101IOT_PID:
    case SC030IOT_PID: {
      // These sensors read a window out as it is, so a window of another size would be read out
      // at that size and no longer fill the frame the camera is set to.
      if (window.width != frame.width || window.height != frame.height) {
        ESP_LOGE(TAG,
                 "%s reads out a window as it is and cannot scale it, so a window has to be the size of "
                 "the %dx%d frame; %dx%d was asked for",
                 sensor_name(sensor), frame.width, frame.height, window.width, window.height);
        return false;
      }
      const ScWindowFormat &format = sensor->id.PID == SC101IOT_PID ? SC101IOT_FORMAT : SC030IOT_FORMAT;
      return set_sc_window(sensor, window, format);
    }
    default:
      ESP_LOGE(TAG, "%s does not support windows", sensor_name(sensor));
      return false;
  }
}

bool Esp32CameraWindow::is_window_supported(sensor_t *sensor) {
  // The five sensors that take a window; every other one is read out through registers of its own.
  switch (sensor->id.PID) {
    case OV2640_PID:
    case OV3660_PID:
    case OV5640_PID:
    case SC101IOT_PID:
    case SC030IOT_PID:
      return true;
    default:
      return false;
  }
}

const char *Esp32CameraWindow::sensor_name(sensor_t *sensor) {
  const camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
  return info != nullptr ? info->name : "unknown sensor";
}

sensor_t *Esp32CameraWindow::checked_sensor_(const char *op) const {
  // Code generation always passes a camera; the null check only reports a build that went without one.
  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "No camera is configured (%s)", op);
    return nullptr;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    ESP_LOGE(TAG, "No sensor; the camera has not finished setting up (%s)", op);
    return nullptr;
  }
  return sensor;
}

}  // namespace esphome::esp32_camera_window

#endif  // USE_ESP32_CAMERA_WINDOW && USE_ESP32
