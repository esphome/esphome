"""ESP-Video camera platform for ESPHome (ESP32-P4).

Publishes the Espressif esp_video (V4L2) stream to Home Assistant as a native
``camera`` entity. Works with any auto-detected MIPI-CSI sensor through the
hardware JPEG encoder, and with USB-UVC cameras.

All Espressif sources are pulled through the IDF component manager (managed
components) — nothing is vendored.
"""

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

_LOGGER = logging.getLogger(__name__)

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
    if config[CONF_ENABLE_UVC] and not _is_uvc(config[CONF_DEVICE]):
        # esp_video brings up every device it was configured for as one unit, so
        # a USB camera that is asked for and never plugged in fails the whole
        # pipeline -- and the retry that exists for a late-arriving camera then
        # keeps a perfectly good MIPI sensor waiting forever.
        raise cv.Invalid(
            f"enable_uvc: true is for a USB camera, but device: is "
            f"{config[CONF_DEVICE]}. Remove enable_uvc, or set device: uvc.",
            path=[CONF_ENABLE_UVC],
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


def _request_high_performance_networking(config):
    """A camera is a streaming component, so ask for the streaming defaults.

    A 720p JPEG is 60-100 kB against lwip's default 5744-byte send buffer, and
    people were writing the lwip and Wi-Fi sizes out by hand in
    sdkconfig_options to compensate. This is the supported way to ask, and what
    speaker.media_player and sendspin do for the same reason.

    Headroom rather than a measured fix, though: on an M5Stack Tab5 and a
    Waveshare board the frame rate is the same with and without it, both
    sustaining 7-14 Mbit/s. What limits those two is the pipeline, not the link.
    """
    network.require_high_performance_networking()
    return config


def _warn_about_idf_log_level(config):
    """esp_video's ISP task logs per frame at IDF debug level.

    It prints several lines of auto-exposure and denoise statistics for every
    frame, each formatted on its own 4 KB stack from outside the main task. The
    frame rate collapses, and the stack can overflow outright. There is nothing
    the component can do about it at run time -- ESPHome builds with
    CONFIG_LOG_TAG_LEVEL_IMPL_NONE, so a single tag cannot be quietened -- so
    say it here, where it can still be changed.
    """
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
    # Platform first. The camera pipeline (MIPI-CSI, ISP, hardware JPEG) is
    # ESP32-P4 silicon and esp_video 2.3.0 needs ESP-IDF 5.4, so on anything
    # else every later check is beside the point -- being told to add PSRAM is
    # a poor way to learn the chip is wrong.
    only_on_variant(supported=[VARIANT_ESP32P4], msg_prefix="esp_video_camera"),
    cv.require_framework_version(
        esp_idf=cv.Version(5, 4, 0),
        extra_message="esp_video_camera requires the esp-idf framework.",
    ),
    # Frames are copied into PSRAM and the V4L2 buffers themselves are sized for
    # it: a 1280x720 RGB565 capture buffer alone is 1.8 MB, well past what
    # internal RAM can hold.
    cv.requires_component(PSRAM_DOMAIN),
    _validate_uvc_device,
    _validate_i2c_bus,
    _validate_xclk,
    _validate_resolution_for_sensor,
    # Last, because unlike the others it changes state outside this component:
    # a configuration that is going to be rejected must not have moved the
    # network settings on its way out.
    _request_high_performance_networking,
)


def _reject_uvc_beside_usb_host(config):
    """USB-UVC and the usb_host component cannot both be in one firmware yet.

    esp_video's UVC driver needs usb_host_lib_handle_events() called
    continuously. On its own this component installs the USB host library and
    runs a dedicated task for that. When usb_host is present it installs first,
    so this component steps aside and does not start its task -- and the only
    remaining pump is usb_host's loop(). But VIDIOC_STREAMON on the UVC node
    blocks the main loop, so that pump never runs: the device deadlocks and the
    task watchdog reboots it five seconds later.

    Letting usb_host own the interface is not enough on its own to fix this; a
    pump that runs in the main loop cannot service a blocking call made from
    the main loop. Sharing it properly is being worked out separately.

    Only enable_uvc builds are affected -- a MIPI-CSI camera never touches the
    USB host, so it is free to sit beside usb_uart and the rest.
    """
    if config[CONF_ENABLE_UVC] and USB_HOST_DOMAIN in fv.full_config.get():
        raise cv.Invalid(
            "enable_uvc: true cannot be used in the same configuration as the "
            "usb_host component (which usb_uart also pulls in): both want to own "
            "the USB host library, and the result is a boot loop rather than a "
            "clear failure. Use one or the other for now.",
            path=[CONF_ENABLE_UVC],
        )
    return config


# Both of these read other components' configuration, so they can only run once
# everything has been validated.
FINAL_VALIDATE_SCHEMA = cv.All(_warn_about_idf_log_level, _reject_uvc_beside_usb_host)


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

    # Colour tuning for the SC202CS, which the SC2356 module (M5Stack Tab5,
    # reTerminal) is the same silicon as. The image processing algorithms read
    # their gamma, colour matrix and white balance from a JSON file, and the
    # sensor's stock one renders noticeably less faithful colour than the file
    # below, which was measured against these boards.
    #
    # Only for a configuration that names this sensor. It replaces the tuning
    # for the whole build, so a board carrying something else must not silently
    # get an SC202CS colour matrix, and neither should a USB camera.
    if config.get(CONF_SENSOR_MODEL) == "sc202cs":
        tuning = "esp_video_camera/sc202cs_ipa.json"
        add_extra_build_file(tuning, Path(__file__).parent / "cfg" / "sc202cs.json")
        add_idf_sdkconfig_option(
            "CONFIG_CAMERA_SC202CS_DEFAULT_IPA_JSON_CONFIGURATION_FILE", False
        )
        add_idf_sdkconfig_option(
            "CONFIG_CAMERA_SC202CS_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE", True
        )
        add_idf_sdkconfig_option(
            "CONFIG_CAMERA_SC202CS_CUSTOMIZED_IPA_JSON_CONFIGURATION_FILE_PATH",
            str(CORE.relative_build_path(tuning)),
        )
