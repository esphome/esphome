import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MOVING_DISTANCE,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
)
from esphome.types import ConfigType

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

LD2420Sensor = ld2420_ns.class_("LD2420Sensor", sensor.Sensor, cg.Component)

CONF_GATE_ENERGY = "gate_energy"

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LD2420Sensor),
            cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
            cv.Optional(CONF_MOVING_DISTANCE): sensor.sensor_schema(
                device_class=DEVICE_CLASS_DISTANCE,
                unit_of_measurement=UNIT_CENTIMETER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_MOVING_DISTANCE, var.set_distance_sensor)
    await sensors(CONF_GATE_ENERGY, var.set_energy_sensor)
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    cg.add(hub.register_listener(var))
