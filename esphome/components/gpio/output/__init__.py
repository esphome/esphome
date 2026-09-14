from esphome import pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.const import CONF_HOLD_DURING_SLEEP
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


def _final_validate(config: ConfigType) -> None:
    if config[CONF_PIN].get(CONF_HOLD_DURING_SLEEP) and config.get(CONF_POWER_SUPPLY):
        power_supply = [
            ps
            for ps in fv.full_config.get()[CONF_POWER_SUPPLY]
            if ps[CONF_ID] == config[CONF_POWER_SUPPLY]
        ][0]
        if not power_supply[CONF_PIN].get(CONF_HOLD_DURING_SLEEP):
            raise cv.Invalid(
                f"{CONF_HOLD_DURING_SLEEP} can only be used with a power supply component if the power supply pin is also configured with {CONF_HOLD_DURING_SLEEP}."
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
