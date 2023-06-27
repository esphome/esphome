import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi, power_supply
from esphome.const import (
    CONF_BACKLIGHT_PIN,
    CONF_DC_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_RESET_PIN,
    CONF_WIDTH,
    CONF_POWER_SUPPLY,
)
from . import st7789v_ns

CONF_EIGHTBITCOLOR = "eightbitcolor"
CONF_OFFSET_HEIGHT = "offset_height"
CONF_OFFSET_WIDTH = "offset_width"

CODEOWNERS = ["@kbx81"]

DEPENDENCIES = ["spi"]

ST7789V = st7789v_ns.class_(
    "ST7789V", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
ST7789VRef = ST7789V.operator("ref")
ST7789VModel = st7789v_ns.enum("ST7789VModel")

MODELS = {
    "TTGO_TDISPLAY_135X240": ST7789VModel.ST7789V_MODEL_TTGO_TDISPLAY_135_240,
    "ADAFRUIT_FUNHOUSE_240X240": ST7789VModel.ST7789V_MODEL_ADAFRUIT_FUNHOUSE_240_240,
    "ADAFRUIT_RR_280X240": ST7789VModel.ST7789V_MODEL_ADAFRUIT_RR_280_240,
    "ADAFRUIT_S2_TFT_FEATHER_240X135": ST7789VModel.ST7789V_MODEL_ADAFRUIT_S2_TFT_FEATHER_240_135,
    "CUSTOM": ST7789VModel.ST7789V_MODEL_CUSTOM,
}

ST7789V_MODEL = cv.enum(MODELS, upper=True, space="_")


def validate_st7789v(config):
    if config[CONF_MODEL].upper() == "CUSTOM" and (
        CONF_HEIGHT not in config
        or CONF_WIDTH not in config
        or CONF_OFFSET_HEIGHT not in config
        or CONF_OFFSET_WIDTH not in config
    ):
        raise cv.Invalid(
            f'{CONF_HEIGHT}, {CONF_WIDTH}, {CONF_OFFSET_HEIGHT} and {CONF_OFFSET_WIDTH} must be specified when {CONF_MODEL} is "CUSTOM"'
        )

    if config[CONF_MODEL].upper() != "CUSTOM" and (
        CONF_HEIGHT in config
        or CONF_WIDTH in config
        or CONF_OFFSET_HEIGHT in config
        or CONF_OFFSET_WIDTH in config
    ):
        raise cv.Invalid(
            f'Do not specify {CONF_HEIGHT}, {CONF_WIDTH}, {CONF_OFFSET_HEIGHT} or {CONF_OFFSET_WIDTH} when using {CONF_MODEL} that is not "CUSTOM"'
        )

    if (
        config[CONF_MODEL].upper() == "ADAFRUIT_S2_TFT_FEATHER_240X135"
        and CONF_POWER_SUPPLY not in config
    ):
        raise cv.Invalid(
            f'{CONF_POWER_SUPPLY} must be specified when {CONF_MODEL} is "ADAFRUIT_S2_TFT_FEATHER_240X135"'
        )
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST7789V),
            cv.Required(CONF_MODEL): ST7789V_MODEL,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BACKLIGHT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
            cv.Optional(CONF_EIGHTBITCOLOR, default=False): cv.boolean,
            cv.Optional(CONF_HEIGHT): cv.int_,
            cv.Optional(CONF_WIDTH): cv.int_,
            cv.Optional(CONF_OFFSET_HEIGHT): cv.int_,
            cv.Optional(CONF_OFFSET_WIDTH): cv.int_,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    validate_st7789v,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))

    if config[CONF_MODEL].upper() == "CUSTOM":
        cg.add(var.set_height(config[CONF_HEIGHT]))
        cg.add(var.set_width(config[CONF_WIDTH]))
        cg.add(var.set_offset_height(config[CONF_OFFSET_HEIGHT]))
        cg.add(var.set_offset_width(config[CONF_OFFSET_WIDTH]))

    cg.add(var.set_eightbitcolor(config[CONF_EIGHTBITCOLOR]))

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))

    if CONF_BACKLIGHT_PIN in config:
        bl = await cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
        cg.add(var.set_backlight_pin(bl))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_POWER_SUPPLY in config:
        ps = await cg.get_variable(config[CONF_POWER_SUPPLY])
        cg.add(var.set_power_supply(ps))
