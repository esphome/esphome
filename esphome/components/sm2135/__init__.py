from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["output"]
CODEOWNERS = ["@BoukeHaarsma23", "@matika77", "@dd32"]
DOMAIN = "sm2135"

sm2135_ns = cg.esphome_ns.namespace("sm2135")
SM2135 = sm2135_ns.class_("SM2135", cg.Component)

CONF_RGB_CURRENT = "rgb_current"
CONF_CW_CURRENT = "cw_current"
CONF_SEPARATE_MODES = "separate_modes"

SM2135Current = sm2135_ns.enum("SM2135Current")

DRIVE_STRENGTHS_CW = SM2135Current.options(
    "10mA",
    "15mA",
    "20mA",
    "25mA",
    "30mA",
    "35mA",
    "40mA",
    "45mA",
    "50mA",
    "55mA",
    "60mA",
    prefix="SM2135_CURRENT_",
)
DRIVE_STRENGTHS_RGB = SM2135Current.options(
    "10mA",
    "15mA",
    "20mA",
    "25mA",
    "30mA",
    "35mA",
    "40mA",
    "45mA",
    prefix="SM2135_CURRENT_",
)


MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM2135),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RGB_CURRENT, "20mA"): cv.enum(DRIVE_STRENGTHS_RGB),
        cv.Optional(CONF_CW_CURRENT, "10mA"): cv.enum(DRIVE_STRENGTHS_CW),
        cv.Optional(CONF_SEPARATE_MODES, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_data_pin, config[CONF_DATA_PIN])
    await cg.add_pin(var.set_clock_pin, config[CONF_CLOCK_PIN])

    cg.add(var.set_rgb_current(config[CONF_RGB_CURRENT]))
    cg.add(var.set_cw_current(config[CONF_CW_CURRENT]))
    cg.add(var.set_separate_modes(config[CONF_SEPARATE_MODES]))
