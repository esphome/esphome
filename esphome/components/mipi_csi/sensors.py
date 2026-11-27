"""MIPI-CSI sensors of espressif/esp_cam_sensor and their formats (`CONFIG_CAMERA_<SENSOR>_MIPI_<FORMAT>`)."""

import re

_RESOLUTION = re.compile(r"_(\d+)X(\d+)_")

SENSOR_FORMATS: dict[str, tuple[str, ...]] = {
    "GC2053": (
        "RAW10_1920X1080_30FPS",
        "RAW8_1920X1080_25FPS",
    ),
    "GC2145": (
        "RGB565_640X480_15FPS",
        "RGB565_800X600_30FPS",
        "RGB565_1600X1200_7FPS",
    ),
    "GC2607": ("RAW10_1920X1080_25FPS",),
    "GC4053": ("RAW10_1440X1440_25FPS",),
    "LT6911": (
        "YUV422_1280X720_60FPS",
        "YUV422_1920X1080_48FPS",
    ),
    "MIRA220": ("RAW8_1024X600_15FPS",),
    "MIT245": (
        "RAW8_1920X1080_30FPS",
        "RAW10_1920X1080_30FPS",
    ),
    "OS02N10": (
        "RAW8_960X540_25FPS",
        "RAW8_1280X720_50FPS",
        "RAW8_1920X1080_25FPS",
        "RAW10_1280X720_50FPS",
        "RAW10_1920X1080_25FPS",
    ),
    "OS04C10": (
        "RAW10_810X1080_30FPS",
        "RAW10_960X1280_30FPS",
        "RAW10_1280X720_25FPS",
    ),
    "OV2710": (
        "RAW10_1280X720_25FPS",
        "RAW10_1920X1080_25FPS",
    ),
    "OV5640": ("RGB565_1280X720_14FPS",),
    "OV5645": (
        "RGB565_1280X960_30FPS",
        "YUV420_1280X960_30FPS",
        "YUV422_640X480_24FPS",
        "YUV422_1280X960_30FPS",
        "YUV422_1920X1080_15FPS",
        "YUV422_2592X1944_15FPS",
    ),
    "OV5647": (
        "RAW8_800X640_50FPS",
        "RAW8_800X800_50FPS",
        "RAW8_800X1280_50FPS",
        "RAW10_1920X1080_30FPS",
        "RAW10_1280X960_BINNING_45FPS",
    ),
    "OV9281": (
        "RAW8_640X400_100FPS",
        "RAW8_1280X720_50FPS",
    ),
    "SC035HGS": (
        "RAW8_640X480_50FPS",
        "RAW8_640X480_100FPS",
        "RAW10_640X480_48FPS",
        "RAW10_640X480_120FPS",
    ),
    "SC121AT": (
        "YUV422_1280X800_25FPS",
        "YUV422_1280X960_30FPS",
    ),
    "SC132GS": (
        "RAW10_544X640_60FPS",
        "RAW10_1080X1280_60FPS",
    ),
    "SC1346": (
        "RAW10_1280X720_25FPS",
        "RAW10_1280X720_30FPS",
        "RAW8_1280X720_30FPS",
    ),
    "SC202CS": (
        "RAW8_1280X720_30FPS",
        "RAW8_1600X1200_30FPS",
        "RAW10_1600X900_30FPS",
        "RAW10_1600X1200_30FPS",
    ),
    "SC2331": (
        "RAW8_1280X720_25FPS",
        "RAW8_1280X720_30FPS",
        "RAW8_1920X1080_5FPS",
        "RAW8_1920X1080_25FPS",
        "RAW10_1920X1080_25FPS",
        "RAW8_1920X1080_30FPS",
    ),
    "SC2336": (
        "RAW8_800X800_30FPS",
        "RAW8_1024X600_30FPS",
        "RAW8_1280X720_30FPS",
        "RAW8_1920X1080_30FPS",
        "RAW10_640X480_50FPS",
        "RAW10_800X800_30FPS",
        "RAW10_1280X720_30FPS",
        "RAW10_1280X720_50FPS",
        "RAW10_1280X720_60FPS",
        "RAW10_1920X1080_25FPS_1_LANE",
        "RAW10_1920X1080_25FPS_2_LANE",
        "RAW10_1920X1080_30FPS",
    ),
    "SC2337P": (
        "RAW10_1088X1080_30FPS",
        "RAW10_1920X1080_30FPS",
        "RAW8_1920X1080_30FPS",
    ),
    "STI2250": ("RAW8_800X600_50FPS",),
    "TC358743": (
        "RGB888_1920X1080_30FPS",
        "YUV422_1920X1080_30FPS",
    ),
}


def format_resolution(fmt: str) -> tuple[int, int]:
    """Width and height of a format suffix such as `RAW10_1920X1080_30FPS`."""
    width, height = _RESOLUTION.search(fmt).groups()
    return int(width), int(height)


def sensor_formats(sensor: str, resolution: list[int] | None) -> list[str]:
    """Formats of `sensor` that capture `resolution`, or all of them without one."""
    formats = SENSOR_FORMATS[sensor]
    if resolution is None:
        return list(formats)
    return [fmt for fmt in formats if format_resolution(fmt) == tuple(resolution)]
