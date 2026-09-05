#pragma once
#ifdef USE_CSI_CAMERA

#include <cstdint>

namespace esphome::csi_camera {

enum class CsiRawFormat : uint8_t {
  CSI_RAW_FORMAT_RAW8,
  CSI_RAW_FORMAT_RAW10,
};

inline const char *csi_raw_format_to_string(CsiRawFormat format) {
  switch (format) {
    case CsiRawFormat::CSI_RAW_FORMAT_RAW8:
      return "raw8";
    case CsiRawFormat::CSI_RAW_FORMAT_RAW10:
      return "raw10";
  }
  return "unknown";
}

/** Sensor-independent requested CSI format. */
struct CsiFormat {
  uint16_t width{0};
  uint16_t height{0};
  uint16_t fps{0};
  CsiRawFormat raw_format{CsiRawFormat::CSI_RAW_FORMAT_RAW10};
};

enum class CsiBayerOrder : uint8_t {
  CSI_BAYER_ORDER_RGGB = 0b00,
  CSI_BAYER_ORDER_GRBG = 0b01,
  CSI_BAYER_ORDER_GBRG = 0b10,
  CSI_BAYER_ORDER_BGGR = 0b11,
};

static constexpr uint8_t CSI_BAYER_ORDER_HORIZONTAL_MIRROR_MASK = 0b01;
static constexpr uint8_t CSI_BAYER_ORDER_VERTICAL_FLIP_MASK = 0b10;

inline const char *csi_bayer_order_to_string(CsiBayerOrder order) {
  switch (order) {
    case CsiBayerOrder::CSI_BAYER_ORDER_RGGB:
      return "rggb";
    case CsiBayerOrder::CSI_BAYER_ORDER_GRBG:
      return "grbg";
    case CsiBayerOrder::CSI_BAYER_ORDER_GBRG:
      return "gbrg";
    case CsiBayerOrder::CSI_BAYER_ORDER_BGGR:
      return "bggr";
  }
  return "rggb";
}

inline CsiBayerOrder csi_bayer_order_mirrored(CsiBayerOrder order) {
  return static_cast<CsiBayerOrder>(static_cast<uint8_t>(order) ^ CSI_BAYER_ORDER_HORIZONTAL_MIRROR_MASK);
}

inline CsiBayerOrder csi_bayer_order_flipped(CsiBayerOrder order) {
  return static_cast<CsiBayerOrder>(static_cast<uint8_t>(order) ^ CSI_BAYER_ORDER_VERTICAL_FLIP_MASK);
}

inline CsiBayerOrder csi_bayer_order_after_orientation(CsiBayerOrder order, bool horizontal_mirror,
                                                       bool vertical_flip) {
  if (horizontal_mirror)
    order = csi_bayer_order_mirrored(order);
  if (vertical_flip)
    order = csi_bayer_order_flipped(order);
  return order;
}

/** Negotiated sensor output consumed by the CSI controller and ISP. */
struct CsiSensorSetup {
  CsiFormat format{};
  uint32_t pixel_clock_hz{0};
  uint32_t lane_bit_rate_mbps{0};
  bool uses_line_sync{false};
  uint8_t lane_count{2};
  CsiBayerOrder bayer_order{CsiBayerOrder::CSI_BAYER_ORDER_GBRG};
  bool orientation_state_valid{false};
  bool default_horizontal_mirror{false};
  bool default_vertical_flip{false};
  bool final_horizontal_mirror{false};
  bool final_vertical_flip{false};
};

}  // namespace esphome::csi_camera
#endif
