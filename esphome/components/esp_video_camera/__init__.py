"""ESP-Video camera platform for ESPHome (ESP32-P4).

Publishes the Espressif esp_video (V4L2) stream to Home Assistant as a native
``camera`` entity. Works with any auto-detected MIPI-CSI sensor through the
hardware JPEG encoder, and with USB-UVC cameras.

All Espressif sources are pulled through the IDF component manager (managed
components) — nothing is vendored.
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
from esphome.components.psram import DOMAIN as PSRAM_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE, CONF_I2C_ID, CONF_ID, CONF_RESOLUTION
from esphome.core.entity_helpers import setup_entity

CODEOWNERS = ["@youkorr"]
# Not "i2c": a USB camera is not on a bus, and a UVC-only board has no reason to
# declare one. The MIPI-CSI path needs it, and _validate_i2c_bus asks for it.
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["camera"]

esp_video_camera_ns = cg.esphome_ns.namespace("esp_video_camera")
ESPVideoCamera = esp_video_camera_ns.class_(
    "ESPVideoCamera", cg.Component, cg.EntityBase
)

CONF_JPEG_QUALITY = "jpeg_quality"
CONF_MAX_FRAMERATE = "max_framerate"
CONF_SENSOR_MODEL = "sensor_model"
CONF_XCLK_PIN = "xclk_pin"
CONF_XCLK_FREQUENCY = "xclk_frequency"
CONF_ENABLE_XCLK = "enable_xclk"
CONF_ENABLE_UVC = "enable_uvc"
CONF_USB_PERIPHERAL_MAP = "usb_peripheral_map"

# Output formats each supported sensor ships in esp_cam_sensor 2.3.0.
#
# A MIPI sensor's resolution is not negotiable at runtime, so `resolution:` is
# resolved here into the CAMERA_<SENSOR>_MIPI_DEFAULT_FMT_* Kconfig choice that
# picks the sensor's boot format. Where one size exists in several variants the
# entry below is the lowest-bandwidth one; the rest stay reachable through
# esp32 -> framework -> sdkconfig_options.
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


def _validate_resolution(value):
    """Normalise to 'auto' or 'WIDTHxHEIGHT'; the C++ side parses nothing else."""
    value = cv.string(value)
    if value.lower() == "auto":
        return "auto"
    if (size := _RESOLUTION_ALIASES.get(value.upper())) is not None:
        return f"{size[0]}x{size[1]}"
    parts = value.lower().split("x")
    if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
        return f"{int(parts[0])}x{int(parts[1])}"
    raise cv.Invalid(
        f"resolution '{value}' is invalid. Use 'auto', an alias "
        f"({'/'.join(_RESOLUTION_ALIASES)}) or 'WIDTHxHEIGHT' (e.g. '1280x720')."
    )


def _validate_device(value):
    value = cv.string(value)
    low = value.lower()
    if low in ("jpeg", "uvc"):
        return low
    if low.startswith("uvc") and len(low) == 4 and low[3].isdigit():
        return low
    if value.startswith("/dev/video"):
        return value
    # No "csi": that device only produces RGB565/RAW, and a camera platform has to
    # publish JPEG. "jpeg" is the same sensor through the hardware encoder.
    raise cv.Invalid(
        f"device '{value}' is invalid. Use 'jpeg' (hardware encoder, MIPI sensors), "
        "'uvc' / 'uvc0'..'uvc9' (USB-UVC camera), or a '/dev/videoN' path."
    )


def _is_uvc(device):
    """True for a USB camera: the 'uvc'/'uvcN' aliases and the /dev/video4N
    paths they resolve to (esp_video reserves 40-49 for USB-UVC)."""
    return device.startswith(("uvc", "/dev/video4"))


def _xclk_pin(value):
    """A GPIO number for the sensor XCLK, or -1 / NO_CLOCK for boards that
    already drive it (an on-board oscillator, or a BSP that started it)."""
    if isinstance(value, str) and value.upper() in ("-1", "NO_CLOCK"):
        return -1
    if value == -1:
        return -1
    # Accepts both 36 and GPIO36, and rejects pins the variant does not have.
    return pins.internal_gpio_output_pin_number(value)


def _validate_xclk(config):
    if config[CONF_ENABLE_XCLK] and config.get(CONF_XCLK_PIN, -1) == -1:
        raise cv.Invalid(
            "enable_xclk: true needs an xclk_pin: to generate the clock on.",
            path=[CONF_XCLK_PIN],
        )
    return config


def _validate_i2c_bus(config):
    if _is_uvc(config[CONF_DEVICE]) or CONF_I2C_ID in config:
        return config
    raise cv.Invalid(
        "i2c_id: is required to probe a MIPI-CSI sensor. Only a USB camera can go "
        "without it, since it is not on an I2C bus.",
        path=[CONF_I2C_ID],
    )


def _validate_uvc_device(config):
    if _is_uvc(config[CONF_DEVICE]) and not config[CONF_ENABLE_UVC]:
        raise cv.Invalid(
            f"device: {config[CONF_DEVICE]} needs enable_uvc: true, otherwise the "
            "USB-UVC host driver is not compiled in and the device never appears.",
            path=[CONF_DEVICE],
        )
    return config


def _sensor_format_symbol(config):
    """The sensor output format `resolution:` maps to, or None for 'auto'.

    Only meaningful for MIPI-CSI sources. A USB-UVC camera carries a real format
    list and is resized at runtime through VIDIOC_S_FMT, so it needs nothing
    here.
    """
    resolution = config[CONF_RESOLUTION]
    if resolution == "auto" or _is_uvc(config[CONF_DEVICE]):
        return None
    width, height = (int(part) for part in resolution.split("x"))
    return _SENSOR_FORMATS[config[CONF_SENSOR_MODEL]][width, height]


def _validate_resolution_for_sensor(config):
    resolution = config[CONF_RESOLUTION]
    if resolution == "auto" or _is_uvc(config[CONF_DEVICE]):
        return config

    if (model := config.get(CONF_SENSOR_MODEL)) is None:
        raise cv.Invalid(
            "resolution: needs sensor_model: to go with it. A MIPI-CSI sensor's "
            "resolution is fixed when the firmware is built, so the sensor has to be "
            "named for the right format to be compiled in. Use resolution: auto to "
            "take whatever the detected sensor comes up in.",
            path=[CONF_RESOLUTION],
        )

    width, height = (int(part) for part in resolution.split("x"))
    formats = _SENSOR_FORMATS[model]
    if (width, height) not in formats:
        supported = ", ".join(f"{w}x{h}" for w, h in sorted(formats))
        raise cv.Invalid(
            f"the {model} does not have a {width}x{height} output format. "
            f"It supports: {supported}.",
            path=[CONF_RESOLUTION],
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
            # No default: most ESP32-P4 boards already drive the sensor clock,
            # and defaulting to a pin means every one of those configs is told
            # off for naming a strapping pin it never touches.
            cv.Optional(CONF_XCLK_PIN): _xclk_pin,
            cv.Optional(CONF_XCLK_FREQUENCY, default=24000000): cv.int_range(
                min=1000000, max=40000000
            ),
            cv.Optional(CONF_ENABLE_XCLK, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_UVC, default=False): cv.boolean,
            # The ESP32-P4 has two USB controllers, and a board wires its host
            # connector to one of them. 0 is the target default -- the
            # High-Speed one -- which is right for most boards; a bit mask
            # (0x1, 0x2, ...) names a specific controller for those it is not.
            cv.Optional(CONF_USB_PERIPHERAL_MAP, default=0): cv.hex_int_range(
                min=0, max=0xFF
            ),
        }
    )
    .extend(cv.ENTITY_BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    # The camera pipeline (MIPI-CSI, ISP, hardware JPEG) is ESP32-P4 silicon,
    # and esp_video 2.3.0 requires ESP-IDF 5.4 or newer. Reject both at
    # validation time rather than at code generation.
    _validate_uvc_device,
    _validate_i2c_bus,
    _validate_xclk,
    # Frames are copied into PSRAM and the V4L2 buffers themselves are sized for
    # it: a 1280x720 RGB565 capture buffer alone is 1.8 MB, well past what
    # internal RAM can hold.
    cv.requires_component(PSRAM_DOMAIN),
    _validate_resolution_for_sensor,
    only_on_variant(supported=[VARIANT_ESP32P4], msg_prefix="esp_video_camera"),
    cv.require_framework_version(
        esp_idf=cv.Version(5, 4, 0),
        extra_message="esp_video_camera requires the esp-idf framework.",
    ),
)


async def to_code(config):
    cg.add_define("USE_CAMERA")

    var = cg.new_Pvariable(config[CONF_ID])
    await setup_entity(var, config, "camera")
    await cg.register_component(var, config)

    if (i2c_id := config.get(CONF_I2C_ID)) is not None:
        i2c_bus = await cg.get_variable(i2c_id)
        cg.add(var.set_i2c_bus(i2c_bus))
    cg.add(
        var.set_xclk_pin(
            cg.RawExpression(
                f"static_cast<gpio_num_t>({config.get(CONF_XCLK_PIN, -1)})"
            )
        )
    )
    cg.add(var.set_xclk_freq(config[CONF_XCLK_FREQUENCY]))
    cg.add(var.set_enable_xclk_init(config[CONF_ENABLE_XCLK]))
    cg.add(var.set_enable_uvc(config[CONF_ENABLE_UVC]))
    cg.add(var.set_usb_peripheral_map(config[CONF_USB_PERIPHERAL_MAP]))

    cg.add(var.set_device(config[CONF_DEVICE]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_jpeg_quality(config[CONF_JPEG_QUALITY]))
    cg.add(var.set_max_framerate(config[CONF_MAX_FRAMERATE]))

    # Also in esphome/idf_component.yml (keep the versions in step): that entry
    # covers the clang-tidy builds, this call is what adds it to "src" REQUIRES.
    add_idf_component(name="espressif/esp_video", ref="2.3.0")
    if config[CONF_ENABLE_UVC]:
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
    if config[CONF_ENABLE_UVC]:
        add_idf_sdkconfig_option("CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE", True)
        # A UVC configuration descriptor runs to several hundred bytes, and the
        # 256-byte default truncates it into "Configuration descriptor larger
        # than control transfer max length" -- the camera never appears.
        add_idf_sdkconfig_option("CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE", 2048)
        # esp_video waits for enumeration on whichever task opened the device.
        # Upstream's 10 s is 10 s of stalled main loop whenever no camera is
        # plugged in; an already-enumerated one costs nothing either way. 500 is
        # the Kconfig minimum.
        add_idf_sdkconfig_option("CONFIG_USB_UVC_INIT_TIMEOUT_MS", 500)
        # 0 means "four times the endpoint's maximum packet size". esp_video
        # hardcodes 10240, which is not a multiple of that for any camera in
        # particular, and an isochronous transfer off a packet boundary loses
        # the rest of the microframe -- a torn picture, or none at all.
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
