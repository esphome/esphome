import re

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, i2c
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.esp32.const import VARIANT_ESP32P4
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
    CONF_PIN,
    CONF_RESET_PIN,
    CONF_RESOLUTION,
    CONF_SENSOR,
    CONF_TRIGGER_ID,
    Framework,
)
from esphome.core.entity_helpers import setup_entity
from esphome.types import ConfigType

from .const import (
    CONF_EXTERNAL_CLOCK,
    CONF_FRAME_BUFFER_COUNT,
    CONF_FRAMERATE,
    CONF_HORIZONTAL_FLIP,
    CONF_IDLE_FRAMERATE,
    CONF_INIT_LDO,
    CONF_JPEG_QUALITY,
    CONF_ON_IMAGE,
    CONF_ON_STREAM_START,
    CONF_ON_STREAM_STOP,
    CONF_PIXEL_FORMAT,
    CONF_POWER_PIN,
    CONF_SCCB_FREQUENCY,
    CONF_VERTICAL_FLIP,
)
from .sensors import SENSOR_FORMATS

CODEOWNERS = ["@Horstexplorer"]

DEPENDENCIES = ["esp32", "i2c", "psram"]
AUTO_LOAD = ["camera"]
MULTI_CONF = False

mipi_csi_ns = cg.esphome_ns.namespace("mipi_csi")
MipiCsiCamera = mipi_csi_ns.class_("MipiCsiCamera", cg.Component, cg.EntityBase)
CameraImageData = mipi_csi_ns.struct("CameraImageData")
MipiCsiImageTrigger = mipi_csi_ns.class_(
    "MipiCsiImageTrigger", automation.Trigger.template()
)
MipiCsiStreamStartTrigger = mipi_csi_ns.class_(
    "MipiCsiStreamStartTrigger", automation.Trigger.template()
)
MipiCsiStreamStopTrigger = mipi_csi_ns.class_(
    "MipiCsiStreamStopTrigger", automation.Trigger.template()
)

PixelFormat = mipi_csi_ns.enum("PixelFormat", is_class=True)
PIXEL_FORMATS = {
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "RGB888": PixelFormat.PIXEL_FORMAT_RGB888,
    "YUV422": PixelFormat.PIXEL_FORMAT_YUV422,
    "GRAYSCALE": PixelFormat.PIXEL_FORMAT_GRAYSCALE,
}

_RESOLUTION_PATTERN = re.compile(r"^\s*(\d+)\s*[xX]\s*(\d+)\s*$")


def validate_resolution(value: str) -> tuple[int, int]:
    """Parses a `WIDTHxHEIGHT` resolution into a pair of pixel counts."""
    value = cv.string(value)
    match = _RESOLUTION_PATTERN.match(value)
    if match is None:
        raise cv.Invalid(f"Resolution must be written as WIDTHxHEIGHT, got '{value}'")
    width, height = int(match.group(1)), int(match.group(2))
    # A zero is what tells the component to keep the sensor's own resolution, and anything above
    # 65535 does not fit the size the component stores, so neither may reach it from here.
    for dimension, name in ((width, "Width"), (height, "Height")):
        if not 1 <= dimension <= 65535:
            raise cv.Invalid(
                f"{name} must be between 1 and 65535, got {dimension} in '{value}'"
            )
    return width, height


CONFIG_SCHEMA = cv.All(
    cv.ENTITY_BASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MipiCsiCamera),
            cv.Required(CONF_SENSOR): cv.one_of(*SENSOR_FORMATS, upper=True),
            cv.Optional(CONF_RESOLUTION): validate_resolution,
            cv.Optional(CONF_PIXEL_FORMAT, default="RGB565"): cv.enum(
                PIXEL_FORMATS, upper=True
            ),
            cv.Optional(CONF_JPEG_QUALITY, default=40): cv.int_range(min=10, max=100),
            cv.Optional(CONF_FRAMERATE, default="10 fps"): cv.All(
                cv.framerate, cv.Range(min=1, max=60)
            ),
            cv.Optional(CONF_IDLE_FRAMERATE, default="0.1 fps"): cv.All(
                cv.framerate, cv.Range(min=0, max=1)
            ),
            cv.Optional(CONF_FRAME_BUFFER_COUNT, default=2): cv.int_range(min=1, max=3),
            cv.Optional(CONF_INIT_LDO, default=True): cv.boolean,
            cv.Optional(CONF_HORIZONTAL_FLIP, default=False): cv.boolean,
            cv.Optional(CONF_VERTICAL_FLIP, default=False): cv.boolean,
            cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.InternalI2CBus),
            cv.Required(CONF_SCCB_FREQUENCY): cv.All(
                cv.frequency, cv.Range(min=10000, max=1000000)
            ),
            cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_POWER_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_EXTERNAL_CLOCK): cv.Schema(
                {
                    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
                    cv.Optional(CONF_FREQUENCY, default="24MHz"): cv.All(
                        cv.frequency, cv.Range(min=6e6, max=40e6)
                    ),
                }
            ),
            cv.Optional(CONF_ON_STREAM_START): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MipiCsiStreamStartTrigger
                    )
                }
            ),
            cv.Optional(CONF_ON_STREAM_STOP): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MipiCsiStreamStopTrigger
                    )
                }
            ),
            cv.Optional(CONF_ON_IMAGE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MipiCsiImageTrigger)}
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ESP_IDF),
    esp32.only_on_variant(supported=[VARIANT_ESP32P4]),
)

SETTERS = {
    CONF_PIXEL_FORMAT: "set_pixel_format",
    CONF_JPEG_QUALITY: "set_jpeg_quality",
    CONF_HORIZONTAL_FLIP: "set_horizontal_flip",
    CONF_VERTICAL_FLIP: "set_vertical_flip",
    CONF_FRAME_BUFFER_COUNT: "set_frame_buffer_count",
    CONF_RESET_PIN: "set_reset_pin",
    CONF_POWER_PIN: "set_power_pin",
}


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA")
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_entity(var, config, "camera")
    await cg.register_component(var, config)

    for key, setter in SETTERS.items():
        if key in config:
            cg.add(getattr(var, setter)(config[key]))

    if (resolution := config.get(CONF_RESOLUTION)) is not None:
        cg.add(var.set_resolution(*resolution))
    cg.add(var.set_sensor_name(config[CONF_SENSOR]))
    cg.add(var.set_init_ldo(config.get(CONF_INIT_LDO, True)))
    cg.add(var.set_framerate(int(config[CONF_FRAMERATE])))
    cg.add(var.set_sccb_frequency(int(config[CONF_SCCB_FREQUENCY])))
    idle_framerate = config[CONF_IDLE_FRAMERATE]
    cg.add(
        var.set_idle_update_interval(
            int(1000 / idle_framerate) if idle_framerate else 0
        )
    )

    i2c_bus = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(i2c_bus))

    if (external_clock := config.get(CONF_EXTERNAL_CLOCK)) is not None:
        cg.add(
            var.set_external_clock(
                external_clock[CONF_PIN], int(external_clock[CONF_FREQUENCY])
            )
        )
        add_idf_sdkconfig_option("CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER", True)

    for conf_key, args in (
        (CONF_ON_STREAM_START, []),
        (CONF_ON_STREAM_STOP, []),
        (CONF_ON_IMAGE, [(CameraImageData, "image")]),
    ):
        for conf in config.get(conf_key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, args, conf)

    _add_idf_config(config)


def _add_idf_config(config: ConfigType) -> None:
    add_idf_component(name="espressif/esp_video", ref="2.5.0")
    add_idf_component(name="espressif/esp_cam_sensor", ref="2.6.0")
    add_idf_component(name="espressif/esp_ipa", ref="2.4.0")

    sensor = config[CONF_SENSOR]
    add_idf_sdkconfig_option(f"CONFIG_CAMERA_{sensor}", True)
    for format_option in SENSOR_FORMATS[sensor]:
        add_idf_sdkconfig_option(f"CONFIG_{format_option}", True)

    for option, value in {
        # The MIPI-CSI capture path is the only one used. DVP defaults to on, so turn it off; SPI
        # is already off unless an SPI sensor selects it, so it needs no entry here.
        "CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE": True,
        "CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE": False,
        # The ISP debayers RAW sensors. Its pipeline controller is what runs the IPA algorithms
        # (auto exposure, auto white balance, denoise) and is off by default.
        "CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE": True,
        "CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER": True,
        # Defaults to y, which saves one frame of memory but in exchange makes the driver re-use the
        # most recent application buffer whenever its free queue runs empty, writing the next frame
        # over the buffer being encoded. Its own spare buffer is worth the memory.
        "CONFIG_ESP_VIDEO_DISABLE_MIPI_CSI_DRIVER_BACKUP_BUFFER": False,
        # The video devices are reached as /dev/videoN, so open(), ioctl() and mmap() all go
        # through VFS. Select support is deliberately not requested: the capture task blocks in
        # VIDIOC_DQBUF and esp_video registers no select handlers, so ESPHome is free to leave
        # CONFIG_VFS_SUPPORT_SELECT off and save the code it would otherwise pull in.
        "CONFIG_VFS_SUPPORT_IO": True,
    }.items():
        add_idf_sdkconfig_option(option, value)
