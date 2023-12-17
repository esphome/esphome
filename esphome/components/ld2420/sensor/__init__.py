import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, DEVICE_CLASS_DISTANCE, UNIT_CENTIMETER
from .. import ld2420_ns, LD2420Component, CONF_LD2420_ID

LD2420Sensor = ld2420_ns.class_("LD2420Sensor", sensor.Sensor, cg.Component)

CONF_MOVING_DISTANCE = "moving_distance"
CONF_GATE_ENERGY = "gate_energy"

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LD2420Sensor),
            cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
            cv.Optional(CONF_MOVING_DISTANCE): sensor.sensor_schema(
                device_class=DEVICE_CLASS_DISTANCE, unit_of_measurement=UNIT_CENTIMETER
            ),
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_MOVING_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_MOVING_DISTANCE])
        cg.add(var.set_distance_sensor(sens))
    if CONF_GATE_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_GATE_ENERGY])
        cg.add(var.set_energy_sensor(sens))
    ld2420 = await cg.get_variable(config[CONF_LD2420_ID])
    cg.add(ld2420.register_listener(var))
