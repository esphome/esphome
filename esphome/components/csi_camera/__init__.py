from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import camera_video, esp_ldo, i2c, psram
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
from esphome.components.esp32.const import VARIANT_ESP32P4
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_CONTRAST,
    CONF_HEIGHT,
    CONF_ID,
    CONF_POWER_SUPPLY,
    CONF_WIDTH,
)
from esphome.core import ID
from esphome.types import ConfigType, TemplateArgsType

AUTO_LOAD = ["camera_video", "esp_ldo"]
DEPENDENCIES = ["esp32", "i2c", "psram"]
CODEOWNERS = ["@jvgelder"]

csi_camera_ns = cg.esphome_ns.namespace("csi_camera")
CsiCamera = csi_camera_ns.class_(
    "CsiCamera", cg.Component, i2c.I2CDevice, camera_video.CameraVideoSource
)
CsiRawFormat = csi_camera_ns.enum("CsiRawFormat", is_class=True)
CsiBayerOrder = csi_camera_ns.enum("CsiBayerOrder", is_class=True)
CsiCameraSetNightModeAction = csi_camera_ns.class_(
    "CsiCameraSetNightModeAction", automation.Action
)

CONF_FRAME_BUFFER_COUNT = "frame_buffer_count"
CONF_POWER_DOWN_PIN = "power_down_pin"
CONF_SATURATION = "saturation"
CONF_VERTICAL_FLIP = "vertical_flip"
CONF_HORIZONTAL_MIRROR = "horizontal_mirror"
CONF_CCM = "ccm"
CONF_HUE = "hue"
CONF_TEST_PATTERN = "test_pattern"
CONF_WB_MODE = "wb_mode"
CONF_AEC_MODE = "aec_mode"
CONF_AE_LEVEL = "ae_level"
CONF_AGC_MODE = "agc_mode"
CONF_SHARPNESS = "sharpness"
CONF_DENOISE = "denoise"
CONF_DEAD_PIXEL_CORRECTION = "dead_pixel_correction"
CONF_BLACK_LEVEL_CORRECTION = "black_level_correction"
CONF_LENS_SHADING_CORRECTION = "lens_shading_correction"
CONF_NIGHT_MODE = "night_mode"
CONF_BAYER_ORDER = "bayer_order"
CONF_FPS = "fps"
CONF_RAW_FORMAT = "raw_format"

OV5647_I2C_ADDRESS = 0x36
ESP_CAM_SENSOR_COMPONENT_VERSION = "2.3.0"
ESP_SCCB_INTF_COMPONENT_VERSION = "0.0.8"
ESP32P4_ISP_MAX_WIDTH = 1920
ESP32P4_ISP_MAX_HEIGHT = 1080

RAW_FORMATS = {
    "raw8": CsiRawFormat.CSI_RAW_FORMAT_RAW8,
    "raw10": CsiRawFormat.CSI_RAW_FORMAT_RAW10,
}

# OV5647 formats provided by the pinned esp_cam_sensor component. The CSI
# request itself remains sensor-independent; this table only owns OV5647
# capability validation and build-time Kconfig selection.
OV5647_FORMAT_KCONFIG = {
    (800, 640, 50, "raw8"): "CONFIG_CAMERA_OV5647_MIPI_RAW8_800X640_50FPS",
    (800, 800, 50, "raw8"): "CONFIG_CAMERA_OV5647_MIPI_RAW8_800X800_50FPS",
    (800, 1280, 50, "raw8"): "CONFIG_CAMERA_OV5647_MIPI_RAW8_800X1280_50FPS",
    (1920, 1080, 30, "raw10"): "CONFIG_CAMERA_OV5647_MIPI_RAW10_1920X1080_30FPS",
    (1280, 960, 45, "raw10"): "CONFIG_CAMERA_OV5647_MIPI_RAW10_1280X960_BINNING_45FPS",
}


def _requested_format(config: ConfigType) -> tuple[int, int, int, str]:
    return (
        config[CONF_WIDTH],
        config[CONF_HEIGHT],
        config[CONF_FPS],
        config[CONF_RAW_FORMAT],
    )


def _validate_ov5647_format(config: ConfigType) -> ConfigType:
    requested = _requested_format(config)
    if requested not in OV5647_FORMAT_KCONFIG:
        width, height, fps, raw_format = requested
        raise cv.Invalid(
            f"OV5647 does not support CSI format {width}x{height}@{fps}fps {raw_format}"
        )
    return config


def _validate_isp_format(config: ConfigType) -> ConfigType:
    width = config[CONF_WIDTH]
    height = config[CONF_HEIGHT]
    if width > ESP32P4_ISP_MAX_WIDTH or height > ESP32P4_ISP_MAX_HEIGHT:
        raise cv.Invalid(
            f"ESP32-P4 ISP supports at most {ESP32P4_ISP_MAX_WIDTH}x{ESP32P4_ISP_MAX_HEIGHT}; "
            f"requested {width}x{height}"
        )
    return config


WB_MODES = {"auto": 0, "sunny": 1, "cloudy": 2, "office": 3, "home": 4}
GAIN_CONTROL_MODES = {"auto": True, "manual": False}
BAYER_ORDERS = {
    "rggb": CsiBayerOrder.CSI_BAYER_ORDER_RGGB,
    "grbg": CsiBayerOrder.CSI_BAYER_ORDER_GRBG,
    "gbrg": CsiBayerOrder.CSI_BAYER_ORDER_GBRG,
    "bggr": CsiBayerOrder.CSI_BAYER_ORDER_BGGR,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CsiCamera),
            cv.Optional(CONF_WIDTH, default=1920): cv.int_range(min=1, max=65535),
            cv.Optional(CONF_HEIGHT, default=1080): cv.int_range(min=1, max=65535),
            cv.Optional(CONF_FPS, default=30): cv.int_range(min=1, max=65535),
            cv.Optional(CONF_RAW_FORMAT, default="raw10"): cv.one_of(
                *RAW_FORMATS, lower=True
            ),
            cv.Optional(CONF_POWER_DOWN_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_POWER_SUPPLY): cv.use_id(esp_ldo.EspLdo),
            cv.Optional(CONF_FRAME_BUFFER_COUNT, default=4): cv.int_range(2, 4),
            cv.Optional(CONF_BRIGHTNESS, default=0): cv.int_range(-128, 127),
            cv.Optional(CONF_CONTRAST, default=1.0): cv.float_range(min=0.0, max=1.99),
            cv.Optional(CONF_SATURATION, default=1.5): cv.float_range(
                min=0.0, max=1.99
            ),
            cv.Optional(CONF_VERTICAL_FLIP): cv.boolean,
            cv.Optional(CONF_HORIZONTAL_MIRROR): cv.boolean,
            cv.Optional(CONF_BAYER_ORDER, default="auto"): cv.one_of(
                "auto", *BAYER_ORDERS, lower=True
            ),
            cv.Optional(CONF_CCM): cv.All([cv.float_], cv.Length(min=9, max=9)),
            cv.Optional(CONF_HUE, default=0): cv.int_range(0, 359),
            cv.Optional(CONF_TEST_PATTERN, default=False): cv.boolean,
            cv.Optional(CONF_WB_MODE): cv.enum(WB_MODES, lower=True),
            cv.Optional(CONF_AEC_MODE): cv.enum(GAIN_CONTROL_MODES, lower=True),
            cv.Optional(CONF_AE_LEVEL): cv.int_range(-2, 2),
            cv.Optional(CONF_AGC_MODE): cv.enum(GAIN_CONTROL_MODES, lower=True),
            cv.Optional(CONF_SHARPNESS): cv.int_range(-2, 2),
            cv.Optional(CONF_DENOISE): cv.int_range(-2, 2),
            cv.Optional(CONF_DEAD_PIXEL_CORRECTION): cv.boolean,
            cv.Optional(CONF_BLACK_LEVEL_CORRECTION): cv.boolean,
            cv.Optional(CONF_LENS_SHADING_CORRECTION): cv.boolean,
            cv.Optional(CONF_NIGHT_MODE): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(OV5647_I2C_ADDRESS)),
    _validate_ov5647_format,
    _validate_isp_format,
    only_on_variant(supported=[VARIANT_ESP32P4]),
)


def _validate_psram(config: ConfigType) -> ConfigType:
    if not psram.is_guaranteed():
        raise cv.Invalid(
            "csi_camera requires guaranteed PSRAM; configure psram with "
            "ignore_not_found: false"
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_psram


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CSI_CAMERA")
    add_idf_component(
        name="espressif/esp_cam_sensor", ref=ESP_CAM_SENSOR_COMPONENT_VERSION
    )
    add_idf_component(
        name="espressif/esp_sccb_intf", ref=ESP_SCCB_INTF_COMPONENT_VERSION
    )
    add_idf_sdkconfig_option("CONFIG_CAMERA_OV5647", True)
    add_idf_sdkconfig_option(
        "CONFIG_CAMERA_OV5647_AUTO_DETECT_MIPI_INTERFACE_SENSOR", True
    )
    requested_format = _requested_format(config)
    add_idf_sdkconfig_option(OV5647_FORMAT_KCONFIG[requested_format], True)
    cg.add_build_flag("-Wl,-u,ov5647_detect")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(
        var.set_format(
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            config[CONF_FPS],
            RAW_FORMATS[config[CONF_RAW_FORMAT]],
        )
    )
    if (power_down_pin := config.get(CONF_POWER_DOWN_PIN)) is not None:
        cg.add(var.set_power_down_pin(power_down_pin))
    if (power_supply_id := config.get(CONF_POWER_SUPPLY)) is not None:
        power_supply = await cg.get_variable(power_supply_id)
        cg.add(var.set_power_supply(power_supply))
    cg.add(var.set_frame_buffer_count(config[CONF_FRAME_BUFFER_COUNT]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_contrast(config[CONF_CONTRAST]))
    cg.add(var.set_saturation(config[CONF_SATURATION]))
    if (vertical_flip := config.get(CONF_VERTICAL_FLIP)) is not None:
        cg.add(var.set_vertical_flip(vertical_flip))
    if (horizontal_mirror := config.get(CONF_HORIZONTAL_MIRROR)) is not None:
        cg.add(var.set_horizontal_mirror(horizontal_mirror))
    if config[CONF_BAYER_ORDER] == "auto":
        cg.add(var.set_bayer_order_auto())
    else:
        cg.add(var.set_bayer_order(BAYER_ORDERS[config[CONF_BAYER_ORDER]]))
    if (ccm := config.get(CONF_CCM)) is not None:
        cg.add(var.set_ccm(*ccm))
    cg.add(var.set_hue(config[CONF_HUE]))
    cg.add(var.set_test_pattern(config[CONF_TEST_PATTERN]))
    if (wb_mode := config.get(CONF_WB_MODE)) is not None:
        cg.add(var.set_wb_mode(wb_mode))
    if (aec_mode := config.get(CONF_AEC_MODE)) is not None:
        cg.add(var.set_aec_mode(aec_mode))
    if (ae_level := config.get(CONF_AE_LEVEL)) is not None:
        cg.add(var.set_ae_level(ae_level))
    if (agc_mode := config.get(CONF_AGC_MODE)) is not None:
        cg.add(var.set_agc_mode(agc_mode))
    if (sharpness := config.get(CONF_SHARPNESS)) is not None:
        cg.add(var.set_sharpness(sharpness))
    if (denoise := config.get(CONF_DENOISE)) is not None:
        cg.add(var.set_denoise(denoise))
    if (dead_pixel_correction := config.get(CONF_DEAD_PIXEL_CORRECTION)) is not None:
        cg.add(var.set_dead_pixel_correction(dead_pixel_correction))
    if (black_level_correction := config.get(CONF_BLACK_LEVEL_CORRECTION)) is not None:
        cg.add(var.set_black_level_correction(black_level_correction))
    if (
        lens_shading_correction := config.get(CONF_LENS_SHADING_CORRECTION)
    ) is not None:
        cg.add(var.set_lens_shading_correction(lens_shading_correction))
    if (night_mode := config.get(CONF_NIGHT_MODE)) is not None:
        cg.add(var.set_night_mode(night_mode))


@automation.register_action(
    "csi_camera.set_night_mode",
    CsiCameraSetNightModeAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(CsiCamera),
            cv.Required(CONF_NIGHT_MODE): cv.templatable(cv.boolean),
        },
        key=CONF_NIGHT_MODE,
    ),
    synchronous=True,
)
async def csi_camera_set_night_mode_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> cg.MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_NIGHT_MODE], args, bool)
    cg.add(var.set_night_mode(template_))
    return var
