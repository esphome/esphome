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
from . import st7701s_ns

CONF_OFFSET_HEIGHT = "offset_height"
CONF_OFFSET_WIDTH = "offset_width"
CONF_DATA_PINS = "data_pins"

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["spi"]

ST7701S = st7701s_ns.class_(
    "ST7701S", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)


def validate_st7701s(config):
    if (
        CONF_OFFSET_WIDTH not in config
        or CONF_OFFSET_HEIGHT not in config
        or CONF_HEIGHT not in config
        or CONF_WIDTH not in config
    ):
        raise cv.Invalid(
            f"{CONF_HEIGHT}, {CONF_WIDTH}, {CONF_OFFSET_HEIGHT} and {CONF_OFFSET_WIDTH} must all be specified"
        )
    if CONF_DC_PIN not in config or CONF_RESET_PIN not in config:
        raise cv.Invalid(f"both {CONF_DC_PIN} and {CONF_RESET_PIN} must be specified")

    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST7701S),
            cv.Required(CONF_DATA_PINS): cv.All(
                [pins.internal_gpio_output_pin_number],
                cv.Length(min=16, max=16, msg="Exactly 16 data pins required"),
            ),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BACKLIGHT_PIN): cv.Any(
                cv.boolean,
                pins.gpio_output_pin_schema,
            ),
            cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
            cv.Optional(CONF_HEIGHT): cv.int_,
            cv.Optional(CONF_WIDTH): cv.int_,
            cv.Optional(CONF_OFFSET_HEIGHT): cv.int_,
            cv.Optional(CONF_OFFSET_WIDTH): cv.int_,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    cv.only_with_esp_idf,
    validate_st7701s,
)


async def to_code(config):
    cg.add_library("esp_lcd")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_model_str(config[CONF_MODEL]))

    cg.add(var.set_height(config[CONF_HEIGHT]))
    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_offset_height(config[CONF_OFFSET_HEIGHT]))
    cg.add(var.set_offset_width(config[CONF_OFFSET_WIDTH]))

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))

    if CONF_BACKLIGHT_PIN in config and config[CONF_BACKLIGHT_PIN]:
        bl = await cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
        cg.add(var.set_backlight_pin(bl))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_POWER_SUPPLY in config:
        ps = await cg.get_variable(config[CONF_POWER_SUPPLY])
        cg.add(var.set_power_supply(ps))
