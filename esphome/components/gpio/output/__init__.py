from esphome import pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.const import CONF_HOLD_STATE
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, CONF_POWER_SUPPLY
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import gpio_ns

GPIOBinaryOutput = gpio_ns.class_("GPIOBinaryOutput", output.BinaryOutput, cg.Component)

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(GPIOBinaryOutput),
        cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


def _require_hold(ps_config: ConfigType) -> ConfigType:
    if not ps_config[CONF_PIN].get(CONF_HOLD_STATE):
        raise cv.Invalid(
            f"The power supply pin must also set {CONF_HOLD_STATE} "
            f"when an output it powers sets it",
            [CONF_PIN],
        )
    return ps_config


def _final_validate(config: ConfigType) -> None:
    if not config[CONF_PIN].get(CONF_HOLD_STATE):
        return
    if (ps_id := config.get(CONF_POWER_SUPPLY)) is None:
        return
    fv.id_declaration_match_schema(_require_hold)(ps_id)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
