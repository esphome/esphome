import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_TYPE

from . import (
    toptronic,
    CONF_TT_ID,
    TopTronicComponent,
    config_schema_polling,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_DATAPOINT,
    TT_TYPE_OPTIONS,
)

TopTronicSensor = toptronic.class_(
    "TopTronicSensor", sensor.Sensor, cg.PollingComponent,
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_TT_ID): cv.use_id(TopTronicComponent),
    cv.Required(CONF_TYPE): cv.enum(TT_TYPE_OPTIONS),
}).extend(sensor.sensor_schema(
    TopTronicSensor
)).extend(config_schema_polling("30s"))


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TT_ID])
    sens = await sensor.new_sensor(config)
    await cg.register_component(sens, config)

    cg.add(sens.set_function_group(config[CONF_FUNCTION_GROUP]))
    cg.add(sens.set_function_number(config[CONF_FUNCTION_NUMBER]))
    cg.add(sens.set_datapoint(config[CONF_DATAPOINT]))
    cg.add(sens.set_type(config[CONF_TYPE]))

    cg.add(tt.add_sensor(sens))
