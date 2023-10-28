from typing import Any, Dict

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from . import const, schema, validate, generate

AUTO_LOAD = [ "binary_sensor", "sensor", "switch", "number", "output" ]
MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(generate.OpenthermHub),
        cv.Optional("in_pin", 4): cv.int_,
        cv.Optional("out_pin", 5): cv.int_,
        cv.Optional("ch_enable", True): cv.boolean,
        cv.Optional("dhw_enable", True): cv.boolean,
        cv.Optional("cooling_enable", False): cv.boolean,
        cv.Optional("otc_active", False): cv.boolean,
        cv.Optional("ch2_active", False): cv.boolean,
        cv.Optional("sync_mode", False): cv.boolean,
    }).extend(validate.create_entities_schema(schema.INPUTS, (lambda _: cv.use_id(sensor.Sensor))))
      .extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)

async def to_code(config: Dict[str, Any]) -> None:
    id = str(config[CONF_ID])
    # Create the hub, passing the two callbacks defined below
    # Since the hub is used in the callbacks, we need to define it first
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(id + "_handle_interrupt"), cg.RawExpression(id + "_process_response"))
    # Define two global callbacks to process responses on interrupt
    cg.add_global(cg.RawStatement("void IRAM_ATTR " + id + "_handle_interrupt() { " + id + "->handle_interrupt(); }"))
    cg.add_global(cg.RawStatement("void " + id + "_process_response(unsigned long response, OpenThermResponseStatus status) { " + id + "->process_response(response, status); }"))
    await cg.register_component(var, config)

    input_sensors = []
    for key, value in config.items():
        if key != CONF_ID:
            if key in schema.INPUTS:
                sensor = await cg.get_variable(value)
                cg.add(getattr(var, f"set_{key}_{const.INPUT_SENSOR.lower()}")(sensor))
                input_sensors.append(key)
            else:
                cg.add(getattr(var, f"set_{key}")(value))

    if len(input_sensors) > 0:
        generate.define_has_component(const.INPUT_SENSOR, input_sensors)
        generate.define_message_handler(const.INPUT_SENSOR, input_sensors, schema.INPUTS)
        generate.define_readers(const.INPUT_SENSOR, input_sensors)
        generate.add_messages(var, input_sensors, schema.INPUTS)

    cg.add_library("ihormelnyk/OpenTherm Library", "1.1.4")
