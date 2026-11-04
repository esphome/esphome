#pragma once
#if defined(USE_CSI_CAMERA) && defined(USE_ESP32) && defined(USE_ESP32_VARIANT_ESP32P4)

#include "csi_types.h"
#include "i2c_sccb_adapter.h"

#include <cstdint>
#include <optional>

namespace esphome::csi_camera {

struct Ov5647SensorConfig {
  I2CSCCBAdapter *sccb_adapter{nullptr};
  uint8_t i2c_address{0x36};
  int power_down_pin{-1};
  CsiFormat format{1920, 1080, 30, CsiRawFormat::CSI_RAW_FORMAT_RAW10};
  std::optional<bool> horizontal_mirror;
  std::optional<bool> vertical_flip;
  bool test_pattern{false};
  std::optional<int> wb_mode;
  std::optional<bool> aec_enabled;
  std::optional<int> ae_level;
  std::optional<bool> agc_enabled;
  std::optional<int> sharpness;
  std::optional<int> denoise;
  std::optional<bool> dead_pixel_correction;
  std::optional<bool> black_level_correction;
  std::optional<bool> lens_shading_correction;
  std::optional<bool> night_mode;
};

class Ov5647Sensor final {
 public:
  static constexpr bool supports_format(const CsiFormat &format) {
    const bool raw8 = format.raw_format == CsiRawFormat::CSI_RAW_FORMAT_RAW8;
    const bool raw10 = format.raw_format == CsiRawFormat::CSI_RAW_FORMAT_RAW10;
    return (raw8 && format.width == 800 && format.height == 640 && format.fps == 50) ||
           (raw8 && format.width == 800 && format.height == 800 && format.fps == 50) ||
           (raw8 && format.width == 800 && format.height == 1280 && format.fps == 50) ||
           (raw10 && format.width == 1920 && format.height == 1080 && format.fps == 30) ||
           (raw10 && format.width == 1280 && format.height == 960 && format.fps == 45);
  }
  ~Ov5647Sensor();

  bool setup(const Ov5647SensorConfig &config, CsiSensorSetup *setup);
  bool start_stream();
  void stop_stream();
  void reset();
  void set_night_mode(bool enabled);
  const char *name() const { return "OV5647"; }

 protected:
  bool read_register_bit_(uint32_t reg, uint32_t mask, bool *enabled) const;
  bool update_register_bit_(uint32_t reg, uint32_t mask, bool enabled, const char *name);
  bool ensure_stream_stopped_();
  void apply_controls_(const Ov5647SensorConfig &config);
  void apply_night_mode_();

  void *sensor_{nullptr};
  std::optional<bool> night_mode_;
  bool streaming_{false};
};

}  // namespace esphome::csi_camera
#endif
