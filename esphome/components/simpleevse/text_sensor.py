import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, simpleevse
from esphome.const import CONF_ID
from . import simpleevse_ns, CONF_SIMPLEEVSE_ID

DEPENDENCIES = ["simpleevse"]

CONF_VEHICLE_STATE = "vehicle_state"
CONF_EVSE_STATE = "evse_state"

SimpleEvseTextSensors = simpleevse_ns.class_("SimpleEvseTextSensors")

# Schema
vehicle_state_schema = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
    }
)
evse_state_schema = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SimpleEvseTextSensors),
        cv.GenerateID(CONF_SIMPLEEVSE_ID): cv.use_id(simpleevse.SimpleEvseComponent),
        cv.Optional(CONF_VEHICLE_STATE): vehicle_state_schema,
        cv.Optional(CONF_EVSE_STATE): evse_state_schema,
    }
)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_SIMPLEEVSE_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)

    if CONF_VEHICLE_STATE in config:
        local_config = config[CONF_VEHICLE_STATE]
        sens_var = cg.new_Pvariable(local_config[CONF_ID])
        yield text_sensor.register_text_sensor(sens_var, local_config)
        yield sens_var
        cg.add(var.set_vehicle_state_sensor(sens_var))

    if CONF_EVSE_STATE in config:
        local_config = config[CONF_EVSE_STATE]
        sens_var = cg.new_Pvariable(local_config[CONF_ID])
        yield text_sensor.register_text_sensor(sens_var, local_config)
        yield sens_var
        cg.add(var.set_evse_state_sensor(sens_var))
