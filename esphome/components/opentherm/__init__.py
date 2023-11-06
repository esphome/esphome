from typing import Any, Dict

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from esphome import pins

from . import const, schema, validate, generate

AUTO_LOAD = ["binary_sensor", "sensor", "switch", "number", "output"]
MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(generate.OpenthermHub),
            cv.Required("in_pin"): pins.internal_gpio_input_pin_schema,
            cv.Required("out_pin"): pins.internal_gpio_output_pin_schema,
            cv.Optional("ch_enable", True): cv.boolean,
            cv.Optional("dhw_enable", True): cv.boolean,
            cv.Optional("cooling_enable", False): cv.boolean,
            cv.Optional("otc_active", False): cv.boolean,
            cv.Optional("ch2_active", False): cv.boolean,
        }
    )
    .extend(
        validate.create_entities_schema(
            schema.INPUTS, (lambda _: cv.use_id(sensor.Sensor))
        )
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: Dict[str, Any]) -> None:
    id = str(config[CONF_ID])
    # Create the hub, passing the two callbacks defined below
    # Since the hub is used in the callbacks, we need to define it first
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set pins
    in_pin = await cg.gpio_pin_expression(config["in_pin"])
    cg.add(var.set_in_pin(in_pin))

    out_pin = await cg.gpio_pin_expression(config["out_pin"])
    cg.add(var.set_out_pin(out_pin))

    input_sensors = []
    non_sensors = {CONF_ID, "in_pin", "out_pin"}
    for key, value in config.items():
        if key not in non_sensors:
            if key in schema.INPUTS:
                sensor = await cg.get_variable(value)
                cg.add(getattr(var, f"set_{key}_{const.INPUT_SENSOR.lower()}")(sensor))
                input_sensors.append(key)
            else:
                cg.add(getattr(var, f"set_{key}")(value))

    if len(input_sensors) > 0:
        generate.define_has_component(const.INPUT_SENSOR, input_sensors)
        generate.define_message_handler(
            const.INPUT_SENSOR, input_sensors, schema.INPUTS
        )
        generate.define_readers(const.INPUT_SENSOR, input_sensors)
        generate.add_messages(var, input_sensors, schema.INPUTS)
