import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_HAS_TARGET, CONF_ID, DEVICE_CLASS_OCCUPANCY

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

LD2420BinarySensor = ld2420_ns.class_(
    "LD2420BinarySensor", binary_sensor.BinarySensor, cg.Component
)


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LD2420BinarySensor),
            cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
            cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OCCUPANCY
            ),
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_HAS_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_TARGET])
        cg.add(var.set_presence_sensor(sens))
    ld2420 = await cg.get_variable(config[CONF_LD2420_ID])
    cg.add(ld2420.register_listener(var))
