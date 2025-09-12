import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_MOTION, ICON_MOTION_SENSOR

from .. import CONF_C4001_ID, C4001Component, dfrobot_c4001_ns

C4001BinarySensor = dfrobot_c4001_ns.class_(
    "C4001BinarySensor", binary_sensor.BinarySensor, cg.Component
)


CONF_EXIST_STATE = "exist_state"
CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(C4001BinarySensor),
            cv.GenerateID(CONF_C4001_ID): cv.use_id(C4001Component),
            cv.Optional(CONF_EXIST_STATE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_MOTION,
                icon=ICON_MOTION_SENSOR,
            ),
        }
    ),
)


async def to_code(config):
    c4001_binary_sensor = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(c4001_binary_sensor, config)
    if CONF_EXIST_STATE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_EXIST_STATE])
        cg.add(c4001_binary_sensor.set_presence_sensor(sens))
    c4001_component = await cg.get_variable(config[CONF_C4001_ID])
    cg.add(c4001_component.register_listener(c4001_binary_sensor))
