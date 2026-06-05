from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]

pcm5122_ns = cg.esphome_ns.namespace("pcm5122")
PCM5122 = pcm5122_ns.class_("PCM5122", AudioDac, cg.Component, i2c.I2CDevice)
CONF_PCM5122 = "pcm5122"


PCMGPIOPin = pcm5122_ns.class_(
    "PCMGPIOPin",
    cg.GPIOPin,
    cg.Parented.template(PCM5122),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PCM5122),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x4D))
)


def _validate_pin_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output, not both")
    return value


def _validate_pin(value):
    if value[CONF_MODE][CONF_INPUT] and value[CONF_NUMBER] == 6:
        raise cv.Invalid("GPIO6 cannot be used as input on the PCM5122")
    return value


PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        PCMGPIOPin,
        cv.int_range(min=3, max=6),
        modes=[CONF_INPUT, CONF_OUTPUT],
        mode_validator=_validate_pin_mode,
    ).extend(
        {
            cv.Required(CONF_PCM5122): cv.use_id(PCM5122),
        }
    ),
    _validate_pin,
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PCM5122, PIN_SCHEMA)
async def pcm5122_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_PCM5122])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
