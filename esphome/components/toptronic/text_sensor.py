import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_OPTIONS

from . import (
    toptronic,
    CONF_TT_ID,
    TopTronicComponent,
    config_schema_polling,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_DATAPOINT,
    CONF_VALUES,
    _validate_options_values_lengths,
)

TopTronicTextSensor = toptronic.class_(
    "TopTronicTextSensor", text_sensor.TextSensor, cg.PollingComponent,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(CONF_TT_ID): cv.use_id(TopTronicComponent),
        cv.Required(CONF_OPTIONS): cv.All(
            cv.ensure_list(cv.string_strict), cv.Length(min=1)
        ),
        cv.Required(CONF_VALUES): cv.All(
            cv.ensure_list(cv.int_), cv.Length(min=1)
        ),
    }).extend(text_sensor.text_sensor_schema(
        TopTronicTextSensor
    )).extend(config_schema_polling("30s")),
    _validate_options_values_lengths,
)


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TT_ID])
    sens = await text_sensor.new_text_sensor(config)
    await cg.register_component(sens, config)

    cg.add(sens.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(sens.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(sens.set_datapoint(config[CONF_DATAPOINT]))

    for i in range(len(config[CONF_OPTIONS])):
        value = config[CONF_VALUES][i]
        text = config[CONF_OPTIONS][i]
        cg.add(sens.add_option(value, text))

    cg.add(tt.add_sensor(sens))
