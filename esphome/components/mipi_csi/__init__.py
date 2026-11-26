import difflib
import logging

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, i2c, logger, network
from esphome.components.const import CONF_PIXEL_FORMAT
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.esp32.const import VARIANT_ESP32P4
from esphome.components.esp_ldo import DOMAIN as ESP_LDO_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
    CONF_PIN,
    CONF_RESET_PIN,
    CONF_RESOLUTION,
    CONF_SENSOR,
    Framework,
)
from esphome.core import CORE
from esphome.core.entity_helpers import setup_entity
from esphome.types import ConfigType

from .const import (
    CONF_EXTERNAL_CLOCK,
    CONF_FRAME_BUFFER_COUNT,
    CONF_FRAMERATE,
    CONF_HORIZONTAL_FLIP,
    CONF_IDLE_FRAMERATE,
    CONF_JPEG_QUALITY,
    CONF_ON_IMAGE,
    CONF_ON_STREAM_START,
    CONF_ON_STREAM_STOP,
    CONF_POWER_PIN,
    CONF_SCCB_FREQUENCY,
    CONF_VERTICAL_FLIP,
)
from .sensors import SENSOR_FORMATS, format_resolution, sensor_formats

DOMAIN = "mipi_csi"
CODEOWNERS = ["@Horstexplorer"]

DEPENDENCIES = ["esp32", "i2c", "psram"]
AUTO_LOAD = ["camera"]

mipi_csi_ns = cg.esphome_ns.namespace("mipi_csi")
MipiCsiCamera = mipi_csi_ns.class_("MipiCsiCamera", cg.Component, cg.EntityBase)
CameraImageData = mipi_csi_ns.struct("CameraImageData")

_LOGGER = logging.getLogger(__name__)

# esp_video powers the MIPI D-PHY from this LDO channel unless another component already does.
CSI_LDO_CHANNEL = 3


def _fourcc(code: str) -> int:
    return int.from_bytes(code.encode(), "little")


# V4L2 fourcc, JPEG encoder input, chroma subsampling, bytes per pixel, matching sensor output
PIXEL_FORMATS = {
    "RGB565": (_fourcc("RGBP"), "RGB565", "YUV422", 2, "RGB565"),
    "RGB888": (_fourcc("RGB3"), "RGB888", "YUV444", 3, "RGB888"),
    "YUV422": (_fourcc("UYVY"), "YUV422", "YUV422", 2, "YUV422"),
    "GRAYSCALE": (_fourcc("GREY"), "GRAY", "GRAY", 1, "GRAYSCALE"),
}
# The prebuilt esp_ipa library logs at DEBUG on every frame, past ESPHome's IDF log level
IPA_LOG_TAGS = (
    "esp_ipa_ian",
    "esp_ipa_agc",
    "esp_ipa_acc",
    "esp_ipa_adn",
    "esp_ipa_atc",
)

FLIP_FLAGS = {CONF_HORIZONTAL_FLIP: 0, CONF_VERTICAL_FLIP: 2}


def validate_sensor(value: str) -> str:
    """A built-in sensor, or one whose driver comes from another IDF component."""
    value = cv.string_strict(value).strip().upper()
    if value not in SENSOR_FORMATS:
        # Warn only: real sensors such as SC2356 are one digit from a built-in name
        close = difflib.get_close_matches(value, SENSOR_FORMATS, n=1, cutoff=0.8)
        _LOGGER.warning(
            "Sensor '%s' is not built into esp_cam_sensor%s. Its driver must come from another "
            "IDF component (for example under esp32: framework: components:).",
            value,
            f" (did you mean '{close[0]}'?)" if close else "",
        )
    return value


def _validate_resolution(config: ConfigType) -> ConfigType:
    if (resolution := config.get(CONF_RESOLUTION)) is None:
        return config
    sensor = config[CONF_SENSOR]
    if sensor in SENSOR_FORMATS and not sensor_formats(sensor, resolution):
        supported = sorted({format_resolution(fmt) for fmt in SENSOR_FORMATS[sensor]})
        raise cv.Invalid(
            f"{sensor} cannot capture {resolution[0]}x{resolution[1]}. Supported "
            f"resolutions are: {', '.join(f'{w}x{h}' for w, h in supported)}",
            path=[CONF_RESOLUTION],
        )
    return config


def _whole_framerate(value: float) -> float:
    """The capture rate is a whole number of frames per second."""
    if value != int(value):
        raise cv.Invalid("framerate must be a whole number of fps")
    return value


def _request_high_performance_networking(config: ConfigType) -> ConfigType:
    """Streams send whole frames; the default lwIP buffers cap them at about a third of the frame rate."""
    network.require_high_performance_networking()
    return config


CONFIG_SCHEMA = cv.All(
    cv.ENTITY_BASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MipiCsiCamera),
            cv.Required(CONF_SENSOR): validate_sensor,
            cv.Optional(CONF_RESOLUTION): cv.dimensions,
            cv.Optional(CONF_PIXEL_FORMAT, default="RGB565"): cv.one_of(
                *PIXEL_FORMATS, upper=True
            ),
            cv.Optional(CONF_JPEG_QUALITY, default=40): cv.int_range(min=10, max=100),
            cv.Optional(CONF_FRAMERATE, default="10 fps"): cv.All(
                cv.framerate, cv.Range(min=1, max=60), _whole_framerate
            ),
            cv.Optional(CONF_IDLE_FRAMERATE, default="0.1 fps"): cv.All(
                cv.framerate, cv.Range(min=0, max=1)
            ),
            cv.Optional(CONF_FRAME_BUFFER_COUNT, default=2): cv.int_range(min=1, max=3),
            cv.Optional(CONF_HORIZONTAL_FLIP): cv.boolean,
            cv.Optional(CONF_VERTICAL_FLIP): cv.boolean,
            cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.InternalI2CBus),
            cv.Required(CONF_SCCB_FREQUENCY): cv.All(
                cv.frequency, cv.Range(min=10000, max=1000000)
            ),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_EXTERNAL_CLOCK): cv.Schema(
                {
                    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
                    cv.Optional(CONF_FREQUENCY, default="24MHz"): cv.All(
                        cv.frequency, cv.Range(min=6e6, max=40e6)
                    ),
                }
            ),
            cv.Optional(CONF_ON_STREAM_START): automation.validate_automation({}),
            cv.Optional(CONF_ON_STREAM_STOP): automation.validate_automation({}),
            cv.Optional(CONF_ON_IMAGE): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_resolution,
    _request_high_performance_networking,
    cv.only_with_framework(Framework.ESP_IDF),
    esp32.only_on_variant(supported=[VARIANT_ESP32P4]),
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_STREAM_START,
        "add_on_stream_callback",
        forwarder=automation.TriggerOnTrueForwarder,
    ),
    automation.CallbackAutomation(
        CONF_ON_STREAM_STOP,
        "add_on_stream_callback",
        forwarder=automation.TriggerOnFalseForwarder,
    ),
    automation.CallbackAutomation(
        CONF_ON_IMAGE, "add_on_image_callback", [(CameraImageData, "image")]
    ),
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA")
    fourcc, jpeg_input, sub_sample, bytes_per_pixel, sensor_format = PIXEL_FORMATS[
        config[CONF_PIXEL_FORMAT]
    ]
    idle_framerate = config[CONF_IDLE_FRAMERATE]
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_SENSOR],
        await cg.get_variable(config[CONF_I2C_ID]),
        int(config[CONF_SCCB_FREQUENCY]),
        cg.RawExpression(f"{fourcc:#010x}"),
        bytes_per_pixel,
        cg.RawExpression(f"JPEG_ENCODE_IN_FORMAT_{jpeg_input}"),
        cg.RawExpression(f"JPEG_DOWN_SAMPLING_{sub_sample}"),
        config[CONF_JPEG_QUALITY],
        config[CONF_FRAME_BUFFER_COUNT],
        int(config[CONF_FRAMERATE]),
        int(1000 / idle_framerate) if idle_framerate else 0,
        cg.RawExpression(f"ESP_CAM_SENSOR_PIXFORMAT_{sensor_format}"),
    )
    # On a UART logger esp_ipa's per-frame logs starve the main loop
    for tag in IPA_LOG_TAGS:
        await logger.request_tag_log_level(tag, "WARN")
    await setup_entity(var, config, "camera")
    await cg.register_component(var, config)

    if (resolution := config.get(CONF_RESOLUTION)) is not None:
        cg.add(var.set_resolution(*resolution))
    # Bit n marks the flip as configured, bit n + 1 holds its value.
    if flip_flags := sum(
        (1 | config[key] << 1) << shift
        for key, shift in FLIP_FLAGS.items()
        if key in config
    ):
        cg.add(var.set_flip_flags(flip_flags))
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
    await cg.add_pin(var.set_power_pin, config.get(CONF_POWER_PIN))

    if (external_clock := config.get(CONF_EXTERNAL_CLOCK)) is not None:
        cg.add(
            var.set_external_clock(
                external_clock[CONF_PIN], int(external_clock[CONF_FREQUENCY])
            )
        )
        add_idf_sdkconfig_option("CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER", True)

    # An esp_ldo entry for the CSI channel already powers the D-PHY; claiming it again fails.
    if any(
        ldo[CONF_CHANNEL] == CSI_LDO_CHANNEL
        for ldo in CORE.config.get(ESP_LDO_DOMAIN, [])
    ):
        cg.add_define("USE_MIPI_CSI_EXTERNAL_LDO")

    if CONF_ON_IMAGE in config:
        cg.add_define("USE_MIPI_CSI_IMAGE_CALLBACK")
    if CONF_ON_STREAM_START in config or CONF_ON_STREAM_STOP in config:
        cg.add_define("USE_MIPI_CSI_STREAM_CALLBACK")
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    _add_idf_config(config)


def _add_idf_config(config: ConfigType) -> None:
    add_idf_component(name="espressif/esp_video", ref="2.6.0")
    add_idf_component(name="espressif/esp_cam_sensor", ref="2.7.0")
    add_idf_component(name="espressif/esp_ipa", ref="2.5.0")

    sensor = config[CONF_SENSOR]
    # A sensor whose driver comes from another IDF component brings its own Kconfig.
    if sensor in SENSOR_FORMATS:
        add_idf_sdkconfig_option(f"CONFIG_CAMERA_{sensor}", True)
        for fmt in sensor_formats(sensor, config.get(CONF_RESOLUTION)):
            add_idf_sdkconfig_option(f"CONFIG_CAMERA_{sensor}_MIPI_{fmt}", True)

    for option, value in {
        "CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE": True,
        "CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE": False,  # on by default
        # Debayers RAW sensors; the controller runs auto exposure and white balance
        "CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE": True,
        "CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER": True,
        # Without its spare buffer the driver writes the next frame over the one being encoded
        "CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER": False,
        "CONFIG_VFS_SUPPORT_IO": True,  # /dev/videoN
        # With mipi_dsi (ISR cache safe GDMA) the CSI controller must be in internal RAM
        "CONFIG_CAM_CTLR_MIPI_CSI_ISR_CACHE_SAFE": True,
    }.items():
        add_idf_sdkconfig_option(option, value)
