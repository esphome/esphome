"""ESP32-P4 camera on Espressif's esp_video (V4L2): MIPI-CSI sensors through the
hardware JPEG encoder, and USB-UVC cameras."""

import logging
from pathlib import Path

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, network
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    add_extra_build_file,
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
from esphome.components.psram import DOMAIN as PSRAM_DOMAIN
from esphome.components.usb_host import DOMAIN as USB_HOST_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    CONF_FRAMEWORK,
    CONF_I2C_ID,
    CONF_ID,
    CONF_LOG_LEVEL,
    CONF_RESOLUTION,
    PLATFORM_ESP32,
)
from esphome.core import CORE
from esphome.core.entity_helpers import setup_entity
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@youkorr"]
# Not "i2c": a USB camera is not on a bus, and a UVC-only board has no reason to
# declare one. The MIPI-CSI path needs it, and _validate_i2c_bus asks for it.
DEPENDENCIES = ["esp32"]
DOMAIN = "esp_video_camera"
AUTO_LOAD = ["camera"]

esp_video_camera_ns = cg.esphome_ns.namespace("esp_video_camera")
ESPVideoCamera = esp_video_camera_ns.class_(
    "ESPVideoCamera", cg.Component, cg.EntityBase
)
DeviceType = esp_video_camera_ns.enum("DeviceType", is_class=True)

CONF_JPEG_QUALITY = "jpeg_quality"
CONF_MAX_FRAMERATE = "max_framerate"
CONF_SENSOR_MODEL = "sensor_model"
CONF_XCLK_PIN = "xclk_pin"
CONF_XCLK_FREQUENCY = "xclk_frequency"
CONF_USB_PERIPHERAL_MAP = "usb_peripheral_map"

# Output formats each sensor ships in esp_cam_sensor 2.3.0. A MIPI sensor's size
# is fixed at build time, so `resolution:` picks the CAMERA_<SENSOR>_MIPI_DEFAULT_FMT_*
# Kconfig choice; where a size has several variants this is the lowest-bandwidth one.
_SENSOR_FORMATS = {
    "sc202cs": {
        (1280, 720): "RAW8_1280X720_30FPS",
        (1600, 900): "RAW10_1600X900_30FPS",
        (1600, 1200): "RAW8_1600X1200_30FPS",
    },
    "ov5647": {
        (800, 640): "RAW8_800X640_50FPS",
        (800, 800): "RAW8_800X800_50FPS",
        (800, 1280): "RAW8_800X1280_50FPS",
        (1280, 960): "RAW10_1280X960_BINNING_45FPS",
        (1920, 1080): "RAW10_1920X1080_30FPS",
    },
    "sc2336": {
        (640, 480): "RAW10_640X480_50FPS",
        (800, 800): "RAW8_800X800_30FPS",
        (1024, 600): "RAW8_1024X600_30FPS",
        (1280, 720): "RAW8_1280X720_30FPS",
        (1920, 1080): "RAW8_1920X1080_30FPS",
    },
}

# Colour tuning files in cfg/, measured against real boards, that replace the
# sensor's stock JSON. Only applied when sensor_model names that sensor, since
# the file replaces the tuning for the whole build.
_SENSOR_TUNING = {"sc202cs": "sc202cs.json"}

# Also pinned in esphome/idf_component.yml for the clang-tidy builds.
_ESP_VIDEO_VERSION = "2.3.0"

# The SC2356 module (M5Stack Tab5, reTerminal) is SC202CS silicon behind a
# different part number, and is driven by the SC202CS driver.
_SENSOR_ALIASES = {"sc2356": "sc202cs"}

# Convenience names. They are only accepted when the chosen sensor actually has
# that size -- none of these sensors does QVGA, for instance.
_RESOLUTION_ALIASES = {
    "QVGA": (320, 240),
    "VGA": (640, 480),
    "480P": (640, 480),
    "720P": (1280, 720),
    "1080P": (1920, 1080),
}


def _validate_sensor_model(value):
    value = cv.string(value).lower()
    value = _SENSOR_ALIASES.get(value, value)
    if value not in _SENSOR_FORMATS:
        raise cv.Invalid(
            f"sensor_model '{value}' is not one of the MIPI-CSI sensors this component "
            f"compiles in: {', '.join(sorted(_SENSOR_FORMATS))} "
            f"(aliases: {', '.join(sorted(_SENSOR_ALIASES))})."
        )
    return value


# esp_video's fixed device nodes (esp_video_device.h).
_JPEG_DEVICE = "/dev/video10"
_UVC_DEVICE_PREFIX = "/dev/video4"


def _validate_resolution(value):
    """'auto', or a [width, height] list."""
    value = cv.string(value)
    if value.lower() == "auto":
        return "auto"
    if (size := _RESOLUTION_ALIASES.get(value.upper())) is not None:
        return list(size)
    try:
        return cv.dimensions(value)
    except cv.Invalid:
        raise cv.Invalid(
            f"resolution '{value}' is invalid. Use 'auto', an alias "
            f"({'/'.join(_RESOLUTION_ALIASES)}) or 'WIDTHxHEIGHT' (e.g. '1280x720')."
        ) from None


def _validate_device(value):
    """Resolve the device aliases to the /dev/videoN path esp_video registers."""
    value = cv.string(value)
    low = value.lower()
    if low == "jpeg":
        return _JPEG_DEVICE
    if low == "uvc":
        return f"{_UVC_DEVICE_PREFIX}0"
    if low.startswith("uvc") and len(low) == 4 and low[3].isdigit():
        return f"{_UVC_DEVICE_PREFIX}{low[3]}"
    if value.startswith("/dev/video"):
        return value
    # No "csi": that device only produces RGB565/RAW, and a camera platform has to
    # publish JPEG. "jpeg" is the same sensor through the hardware encoder.
    raise cv.Invalid(
        f"device '{value}' is invalid. Use 'jpeg' (hardware encoder, MIPI sensors), "
        "'uvc' / 'uvc0'..'uvc9' (USB-UVC camera), or a '/dev/videoN' path."
    )


def _is_uvc(device: str) -> bool:
    """True for a USB camera: esp_video reserves /dev/video40-49 for USB-UVC."""
    return device.startswith(_UVC_DEVICE_PREFIX)


def _xclk_pin(value):
    """A GPIO number for the sensor XCLK, or -1 / NO_CLOCK for boards that
    already drive it (an on-board oscillator, or a BSP that started it)."""
    if isinstance(value, str) and value.upper() in ("-1", "NO_CLOCK"):
        return -1
    if value == -1:
        return -1
    # Accepts both 36 and GPIO36, and rejects pins the variant does not have.
    return pins.internal_gpio_output_pin_number(value)


def _validate_i2c_bus(config):
    if _is_uvc(config[CONF_DEVICE]) or CONF_I2C_ID in config:
        return config
    raise cv.Invalid(
        "i2c_id: is required to probe a MIPI-CSI sensor. Only a USB camera can go "
        "without it, since it is not on an I2C bus.",
        path=[CONF_I2C_ID],
    )


def _mipi_resolution(config: ConfigType) -> tuple[int, int] | None:
    """The size a MIPI sensor must be built for, or None for 'auto' or a USB
    camera, which is resized at runtime through VIDIOC_S_FMT instead."""
    resolution = config[CONF_RESOLUTION]
    if resolution == "auto" or _is_uvc(config[CONF_DEVICE]):
        return None
    return tuple(resolution)


def _sensor_format_symbol(config: ConfigType) -> str | None:
    """The sensor output format `resolution:` maps to, or None."""
    if (size := _mipi_resolution(config)) is None:
        return None
    return _SENSOR_FORMATS[config[CONF_SENSOR_MODEL]][size]


def _validate_resolution_for_sensor(config: ConfigType) -> ConfigType:
    if (size := _mipi_resolution(config)) is None:
        return config

    if (model := config.get(CONF_SENSOR_MODEL)) is None:
        raise cv.Invalid(
            "resolution: needs sensor_model: to go with it. A MIPI-CSI sensor's "
            "resolution is fixed when the firmware is built, so the sensor has to be "
            "named for the right format to be compiled in. Use resolution: auto to "
            "take whatever the detected sensor comes up in.",
            path=[CONF_RESOLUTION],
        )

    width, height = size
    formats = _SENSOR_FORMATS[model]
    if size not in formats:
        supported = ", ".join(f"{w}x{h}" for w, h in sorted(formats))
        raise cv.Invalid(
            f"the {model} does not have a {width}x{height} output format. "
            f"It supports: {supported}.",
            path=[CONF_RESOLUTION],
        )
    return config


def _request_high_performance_networking(config):
    """A 720p JPEG is 60-100 kB, far past lwip's default send buffer, so ask for
    the streaming network defaults like speaker.media_player does."""
    network.require_high_performance_networking()
    return config


def _warn_about_idf_log_level(config):
    """esp_video's ISP task logs several lines per frame at IDF debug level, on a
    4 KB stack, and one tag cannot be quietened at run time."""
    framework = fv.full_config.get()[PLATFORM_ESP32].get(CONF_FRAMEWORK, {})
    if (level := framework.get(CONF_LOG_LEVEL)) in ("DEBUG", "VERBOSE"):
        _LOGGER.warning(
            "esp32 -> framework -> log_level is %s. At that level esp_video's image "
            "tuning prints several lines for every frame from its own task, which "
            "buries the camera's own log lines and slows it to a crawl. Set "
            "'esp32: framework: log_level: ERROR'. This is not the same setting as "
            "'logger: level:', which controls ESPHome's own logs and can stay at "
            "DEBUG.",
            level,
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPVideoCamera),
            # Only the MIPI-CSI path probes a sensor over I2C; a USB camera is not
            # on any bus. Required for everything else by _validate_i2c_bus below.
            cv.Optional(CONF_I2C_ID): cv.use_id(i2c.InternalI2CBus),
            cv.Optional(CONF_DEVICE, default="jpeg"): _validate_device,
            cv.Optional(CONF_RESOLUTION, default="auto"): _validate_resolution,
            cv.Optional(CONF_SENSOR_MODEL): _validate_sensor_model,
            # V4L2_CID_JPEG_COMPRESSION_QUALITY semantics: 1..100, higher is
            # better. esp_video's hardware encoder defaults to 80.
            cv.Optional(CONF_JPEG_QUALITY, default=80): cv.int_range(min=1, max=100),
            cv.Optional(CONF_MAX_FRAMERATE, default=10): cv.float_range(
                min=0.1, max=60.0
            ),
            # No default: most ESP32-P4 boards already drive the sensor clock.
            cv.Optional(CONF_XCLK_PIN): _xclk_pin,
            cv.Optional(CONF_XCLK_FREQUENCY, default="24MHz"): cv.All(
                cv.frequency, cv.int_range(min=1000000, max=40000000)
            ),
            # 0 is the High-Speed controller; a bit mask names another.
            cv.Optional(CONF_USB_PERIPHERAL_MAP, default=0): cv.hex_int_range(
                min=0, max=0xFF
            ),
        }
    )
    .extend(cv.ENTITY_BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    # Platform first, so a wrong chip is reported before anything else.
    only_on_variant(supported=[VARIANT_ESP32P4], msg_prefix="esp_video_camera"),
    cv.require_framework_version(
        esp_idf=cv.Version(5, 4, 0),
        extra_message="esp_video_camera requires the esp-idf framework.",
    ),
    # A 1280x720 RGB565 capture buffer alone is 1.8 MB.
    cv.requires_component(PSRAM_DOMAIN),
    _validate_i2c_bus,
    _validate_resolution_for_sensor,
    # Last: it changes network settings, which a rejected config must not do.
    _request_high_performance_networking,
)


def _reject_uvc_beside_usb_host(config):
    """USB-UVC and usb_host cannot share one firmware yet: usb_host pumps USB
    events from loop(), which a blocking VIDIOC_STREAMON starves into a
    watchdog reboot. A MIPI-CSI camera never touches the USB host."""
    if _is_uvc(config[CONF_DEVICE]) and USB_HOST_DOMAIN in fv.full_config.get():
        raise cv.Invalid(
            "A USB camera cannot be used in the same configuration as the "
            "usb_host component (which usb_uart also pulls in): both want to own "
            "the USB host library, and the result is a boot loop rather than a "
            "clear failure. Use one or the other for now.",
            path=[CONF_DEVICE],
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(_warn_about_idf_log_level, _reject_uvc_beside_usb_host)


async def to_code(config):
    cg.add_define("USE_CAMERA")

    device = config[CONF_DEVICE]
    uvc = _is_uvc(device)
    if uvc:
        device_type = DeviceType.DEVICE_TYPE_UVC
    elif device == _JPEG_DEVICE:
        device_type = DeviceType.DEVICE_TYPE_HW_JPEG
    else:
        device_type = DeviceType.DEVICE_TYPE_DIRECT
    var = cg.new_Pvariable(config[CONF_ID], device, device_type)
    await setup_entity(var, config, "camera")
    await cg.register_component(var, config)

    if (i2c_id := config.get(CONF_I2C_ID)) is not None:
        i2c_bus = await cg.get_variable(i2c_id)
        cg.add(var.set_i2c_bus(i2c_bus))
    # Setting a pin is what turns the clock on; -1 leaves it to the board.
    if (xclk_pin := config.get(CONF_XCLK_PIN, -1)) >= 0:
        cg.add(var.set_xclk_pin(xclk_pin))
        cg.add(var.set_xclk_freq(config[CONF_XCLK_FREQUENCY]))
    cg.add(var.set_usb_peripheral_map(config[CONF_USB_PERIPHERAL_MAP]))

    if (resolution := config[CONF_RESOLUTION]) != "auto":
        cg.add(var.set_resolution(*resolution))
    cg.add(var.set_jpeg_quality(config[CONF_JPEG_QUALITY]))
    cg.add(var.set_min_frame_interval(int(1000 / config[CONF_MAX_FRAMERATE])))

    add_idf_component(name="espressif/esp_video", ref=_ESP_VIDEO_VERSION)
    if uvc:
        # USB-UVC host driver, aligned with esp_video 2.3.0's own dependency.
        add_idf_component(name="espressif/usb_host_uvc", ref="2.5.*")

    # ENABLE_ISP_PIPELINE_CONTROLLER pulls in esp_ipa and runs the AWB/AE/CCM/gamma
    # automation; without it the image is unprocessed.
    for opt in (
        "CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE",
        "CONFIG_ESP_VIDEO_ENABLE_ISP",
        "CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE",
        "CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER",
        "CONFIG_ESP_VIDEO_ENABLE_JPEG_ENC_VIDEO_DEVICE",
        "CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE",
    ):
        add_idf_sdkconfig_option(opt, True)
    if uvc:
        add_idf_sdkconfig_option("CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE", True)
        # 2048: a UVC configuration descriptor overruns the 256-byte default and
        # the camera never appears. 500 ms: the enumeration wait blocks whichever
        # task opened the device, and it is the Kconfig minimum. 0: size the URBs
        # from the endpoint, since esp_video's fixed 10240 is not a multiple of
        # any real packet size and a torn isochronous transfer loses the rest of
        # the microframe.
        add_idf_sdkconfig_option("CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE", 2048)
        add_idf_sdkconfig_option("CONFIG_USB_UVC_INIT_TIMEOUT_MS", 500)
        add_idf_sdkconfig_option("CONFIG_USB_UVC_VIDEO_DEVICE_URB_SIZE", 0)

    # Auto-detection walks the esp_cam_sensor_detect_fn section, which only holds
    # every driver under dynamic linking. Upstream's default, but depended on
    # here. "MOTOR" is not a typo: the one upstream choice covers sensor and
    # motor detection together.
    add_idf_sdkconfig_option(
        "CONFIG_CAMERA_SENSOR_MOTOR_DETECT_METHOD_DYNAMIC_LINK", True
    )

    # Every driver goes in whatever sensor_model says, so a board that turns out
    # to carry a different sensor still comes up.
    for sensor in _SENSOR_FORMATS:
        add_idf_sdkconfig_option(f"CONFIG_CAMERA_{sensor.upper()}", True)
        add_idf_sdkconfig_option(
            f"CONFIG_CAMERA_{sensor.upper()}_AUTO_DETECT_MIPI_INTERFACE_SENSOR", True
        )

    # A format is only choosable as the boot default once its own
    # CAMERA_<SENSOR>_MIPI_* symbol has put it in the driver's format table.
    if (fmt := _sensor_format_symbol(config)) is not None:
        sensor = config[CONF_SENSOR_MODEL].upper()
        add_idf_sdkconfig_option(f"CONFIG_CAMERA_{sensor}_MIPI_{fmt}", True)
        add_idf_sdkconfig_option(f"CONFIG_CAMERA_{sensor}_MIPI_DEFAULT_FMT_{fmt}", True)

    model = config.get(CONF_SENSOR_MODEL)
    if (tuning_file := _SENSOR_TUNING.get(model)) is not None:
        sensor = model.upper()
        tuning = f"esp_video_camera/{model}_ipa.json"
        add_extra_build_file(tuning, Path(__file__).parent / "cfg" / tuning_file)
        prefix = f"CONFIG_CAMERA_{sensor}"
        add_idf_sdkconfig_option(f"{prefix}_DEFAULT_IPA_JSON_CONFIGURATION_FILE", False)
        add_idf_sdkconfig_option(
            f"{prefix}_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE", True
        )
        add_idf_sdkconfig_option(
            f"{prefix}_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE_PATH",
            str(CORE.relative_build_path(tuning)),
        )
