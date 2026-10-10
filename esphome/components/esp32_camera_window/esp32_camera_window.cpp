#include "esp32_camera_window.h"

#if defined(USE_ESP32_CAMERA_WINDOW) && defined(USE_ESP_IDF)

#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::esp32_camera_window {

ESPHOME_LOG_TAG(TAG, "esp32_camera_window");

// Registers of the OV2640 that hold the crop its image pipeline works on. A register number
// carries its bank in bit 8, and all of these are in the DSP bank, bank 0.
static constexpr int OV2640_REG_HSIZE = 0x0051;
static constexpr int OV2640_REG_VSIZE = 0x0052;
static constexpr int OV2640_REG_XOFFL = 0x0053;
static constexpr int OV2640_REG_YOFFL = 0x0054;
static constexpr int OV2640_REG_VHYX = 0x0055;
// A size register of the OV2640 covers four pixels of its readout, so a crop is a multiple of
// four and is set in steps of four.
static constexpr int OV2640_CROP_STEP = 4;
// The readout the OV2640 crops out of, one per group of frame sizes. set_res_raw() takes it in
// the place of startX; the values are in the order of the driver's own enumeration, which is not
// part of an exported header.
static constexpr int OV2640_MODE_UXGA = 0;
static constexpr int OV2640_MODE_SVGA = 1;
static constexpr int OV2640_MODE_CIF = 2;

// Registers that describe the readout of the OV3660 and OV5640, which share one 16-bit register
// map. A 16-bit register holds its x value in the low byte and its y value in the high byte,
// which is why the two registers of a pair are two apart.
static constexpr int OV_REG_START = 0x3800;   // X_ADDR_ST, Y_ADDR_ST
static constexpr int OV_REG_END = 0x3804;     // X_ADDR_END, Y_ADDR_END
static constexpr int OV_REG_TOTAL = 0x380C;   // X_TOTAL_SIZE, Y_TOTAL_SIZE
static constexpr int OV_REG_OFFSET = 0x3810;  // X_OFFSET, Y_OFFSET
static constexpr int OV_REG_PAIR_STEP = 2;
static constexpr int OV_REG_MASK = 0xFFFF;

// A sensor takes a few frames to read out a window that was just set, so waiting a moment avoids
// handing out frames of the old one.
static constexpr uint32_t WINDOW_SETTLE_MS = 50;

// Registers that hold the readout window of the SC101IOT and SC030IOT. Both sensors page their
// register file, so a register number carries the page it lives in and set_reg() writes that page
// for them. Each axis of a window has a start and an end, whose high bits share the third register
// of the group.
static constexpr int SC_REG_H_START = 0x0170;  // H start, H end, high bits of both
static constexpr int SC_REG_V_START = 0x0173;  // V start, V end, high bits of both
static constexpr int SC_REG_GROUP = 3;         // start, end and high bits of both
// The readout an SC window is placed in, and the high bits the sensor takes per edge of a window.
// Its two sensors pack a different number of them, which is all that sets their registers apart.
struct ScWindowFormat {
  int area_width;
  int area_height;
  int edge_bits;
};
static constexpr ScWindowFormat SC101IOT_FORMAT{1280, 720, 4};
static constexpr ScWindowFormat SC030IOT_FORMAT{640, 480, 2};

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
///
/// Reading the readout back keeps the mapping onto the frame right at any frame size and ratio
/// without repeating the driver's tables here, and it stays right when the frame size is changed
/// at runtime.
struct OvReadout {
  SensorPair start;   // first sensor pixel that is read out
  SensorPair offset;  // margin the driver left to its image pipeline, kept as it was
  SensorPair total;   // line and frame timing the driver wrote, kept as it was
  int span_x{0};      // sensor pixels that are read out
  int span_y{0};
  int factor{1};  // sensor pixels that make up one readout pixel

  int visible_x() const { return this->span_x / this->factor; }
  int visible_y() const { return this->span_y / this->factor; }
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
  OvReadout readout;
  if (!read_ov_readout(sensor, &readout))
    return false;

  const Rect frame_window = clamp_window(window, frame_width, frame_height);
  const Rect crop = scale_window(frame_window, frame_width, frame_height, readout.visible_x(), readout.visible_y());

  const int start_x = readout.start.x + readout.factor * crop.offset_x;
  const int start_y = readout.start.y + readout.factor * crop.offset_y;
  const int end_x = start_x + readout.factor * crop.width - 1;
  const int end_y = start_y + readout.factor * crop.height - 1;
  // The image pipeline scales what it is given to the frame, so a window smaller than the frame
  // fills the frame on its own. A window the size of the frame needs no scaling at all.
  const bool scale = crop.width != frame_width || crop.height != frame_height;

  const int ret =
      sensor->set_res_raw(sensor, start_x, start_y, end_x, end_y, readout.offset.x, readout.offset.y, readout.total.x,
                          readout.total.y, frame_width, frame_height, scale, sensor->status.binning);
  if (ret != 0) {
    ESP_LOGE(TAG, "Reading out %d,%d to %d,%d failed with %d", start_x, start_y, end_x, end_y, ret);
    return false;
  }

  vTaskDelay(WINDOW_SETTLE_MS / portTICK_PERIOD_MS);
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

/// Reads the crop back from the registers set_window() wrote.
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

/// The readout the frame size in use needs, which is the choice the driver's set_framesize()
/// makes for it.
static int ov2640_mode_for(framesize_t framesize) {
  if (framesize <= FRAMESIZE_CIF)
    return OV2640_MODE_CIF;
  if (framesize <= FRAMESIZE_SVGA)
    return OV2640_MODE_SVGA;
  return OV2640_MODE_UXGA;
}

/// Reads out a window on an OV2640.
static bool set_ov2640_window(sensor_t *sensor, const Esp32CameraWindow::Window &window, int frame_width,
                              int frame_height) {
  Ov2640Crop crop;
  if (!read_ov2640_crop(sensor, &crop))
    return false;

  const Rect frame_window = clamp_window(window, frame_width, frame_height);
  const Rect area = scale_window(frame_window, frame_width, frame_height, crop.width, crop.height);

  // The crop is a multiple of four pixels, counted from the corner of the crop in use.
  const int offset_x = area.offset_x - area.offset_x % OV2640_CROP_STEP;
  const int offset_y = area.offset_y - area.offset_y % OV2640_CROP_STEP;
  int width = std::min(area.width, crop.width - offset_x);
  int height = std::min(area.height, crop.height - offset_y);
  width = std::max(width - width % OV2640_CROP_STEP, OV2640_CROP_STEP);
  height = std::max(height - height % OV2640_CROP_STEP, OV2640_CROP_STEP);

  // set_res_raw() of this sensor takes the crop in offsetX/offsetY plus totalX/totalY and the
  // frame it fills in outputX/outputY; start and end are left unused.
  const int ret =
      sensor->set_res_raw(sensor, ov2640_mode_for(sensor->status.framesize), 0, 0, 0, crop.offset_x + offset_x,
                          crop.offset_y + offset_y, width, height, frame_width, frame_height, false, false);
  if (ret != 0) {
    ESP_LOGE(TAG, "Cropping to offset %d,%d size %dx%d failed with %d", crop.offset_x + offset_x,
             crop.offset_y + offset_y, width, height, ret);
    return false;
  }

  vTaskDelay(WINDOW_SETTLE_MS / portTICK_PERIOD_MS);
  ESP_LOGI(
      TAG, "Window at %d,%d of %dx%d crops to offset %d,%d size %dx%d of the %dx%d readout and fills the %dx%d frame",
      frame_window.offset_x, frame_window.offset_y, frame_window.width, frame_window.height, crop.offset_x + offset_x,
      crop.offset_y + offset_y, width, height, crop.width, crop.height, frame_width, frame_height);
  return true;
}

/// Writes the start and the end of one axis of an SC window, reporting whether the sensor took them.
static bool write_sc_axis(sensor_t *sensor, int reg, int start, int end, int edge_bits) {
  const int mask = (1 << edge_bits) - 1;
  // The high bits of both edges share the third register of the group.
  const int values[SC_REG_GROUP] = {start & 0xFF, end & 0xFF, ((start >> 8) & mask) | (((end >> 8) & mask) << 4)};
  for (int offset = 0; offset < SC_REG_GROUP; offset++) {
    const int ret = sensor->set_reg(sensor, reg + offset, 0xFF, values[offset]);
    if (ret != 0) {
      ESP_LOGE(TAG, "Writing register 0x%04X failed with %d", reg + offset, ret);
      return false;
    }
  }
  return true;
}

/// Reads out a window on an SC101IOT or SC030IOT.
///
/// These sensors read the window out as it is, so the frame keeps the size it is published with
/// only while the window is that size; the window then moves which part of the sensor the frame
/// shows. A window on them is counted from the corner of the whole readout of the sensor, because
/// that is where their window registers count from.
static bool set_sc_window(sensor_t *sensor, const Esp32CameraWindow::Window &window, const ScWindowFormat &format) {
  const int offset_x = std::clamp(window.offset_x, 0, format.area_width - window.width);
  const int offset_y = std::clamp(window.offset_y, 0, format.area_height - window.height);

  if (!write_sc_axis(sensor, SC_REG_H_START, offset_x, offset_x + window.width, format.edge_bits) ||
      !write_sc_axis(sensor, SC_REG_V_START, offset_y, offset_y + window.height, format.edge_bits))
    return false;

  vTaskDelay(WINDOW_SETTLE_MS / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Window at %d,%d of %dx%d reads out %d,%d to %d,%d of the %dx%d sensor", window.offset_x,
           window.offset_y, window.width, window.height, offset_x, offset_y, offset_x + window.width - 1,
           offset_y + window.height - 1, format.area_width, format.area_height);
  return true;
}

float Esp32CameraWindow::get_setup_priority() const {
  // The sensor is only there once the camera has run its own setup().
  return setup_priority::PROCESSOR;
}

void Esp32CameraWindow::setup() {
  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "No camera is configured");
    return;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    ESP_LOGE(TAG, "No sensor; the camera has not finished setting up");
    return;
  }
  if (!is_window_supported(sensor)) {
    ESP_LOGW(TAG, "%s does not support windows, so no window is used", sensor_name(sensor));
    return;
  }

  if (this->initial_window_.enabled) {
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
  ESP_LOGCONFIG(TAG, "  Window support: %s", YESNO(is_window_supported(sensor)));
  if (this->initial_window_.enabled) {
    ESP_LOGCONFIG(TAG, "  Window: offset %d,%d size %dx%d", this->initial_window_.offset_x,
                  this->initial_window_.offset_y, this->initial_window_.width, this->initial_window_.height);
  }
}

bool Esp32CameraWindow::set_window(int offset_x, int offset_y, int width, int height) {
  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "No camera is configured");
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    ESP_LOGE(TAG, "No sensor; the camera has not finished setting up");
    return false;
  }
  if (!is_window_supported(sensor)) {
    ESP_LOGE(TAG, "%s does not support windows", sensor_name(sensor));
    return false;
  }

  const Window window{offset_x, offset_y, width, height, true};
  if (!this->set_sensor_window_(sensor, window))
    return false;

  ESP_LOGI(TAG, "Window set to offset %d,%d size %dx%d", offset_x, offset_y, width, height);
  return true;
}

bool Esp32CameraWindow::reset_window() {
  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "No camera is configured");
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    ESP_LOGE(TAG, "No sensor; the camera has not finished setting up");
    return false;
  }
  if (!is_window_supported(sensor)) {
    ESP_LOGE(TAG, "%s does not support windows", sensor_name(sensor));
    return false;
  }

  // Setting the frame size again programs the readout the camera wants for it, which is what
  // puts a window back to the whole sensor. The frame size itself stays the same.
  const int ret = sensor->set_framesize(sensor, sensor->status.framesize);
  if (ret != 0) {
    ESP_LOGE(TAG, "Reading out the whole sensor again failed with %d", ret);
    return false;
  }

  vTaskDelay(WINDOW_SETTLE_MS / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Window cleared, reading out the whole sensor");
  return true;
}

bool Esp32CameraWindow::supports_window() const {
  if (this->camera_ == nullptr)
    return false;
  sensor_t *sensor = esp_camera_sensor_get();
  return sensor != nullptr && is_window_supported(sensor);
}

bool Esp32CameraWindow::set_sensor_window_(sensor_t *sensor, const Window &window) {
  if (sensor->status.framesize >= FRAMESIZE_INVALID) {
    ESP_LOGE(TAG, "The camera is not set to a valid frame size");
    return false;
  }
  const resolution_info_t frame = resolution[sensor->status.framesize];
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
  // The OV2640 crops inside the readout it is already doing, and the OV3660 and OV5640 accept any
  // part of the sensor as their readout. The SC101IOT and SC030IOT take a window over their whole
  // readout, of the size of the frame. Every other sensor the driver knows is read out through a
  // different set of registers, so it cannot be asked for a window.
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

}  // namespace esphome::esp32_camera_window

#endif  // USE_ESP32_CAMERA_WINDOW && USE_ESP_IDF
