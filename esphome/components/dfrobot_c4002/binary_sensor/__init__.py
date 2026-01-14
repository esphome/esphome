import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_MOTION, ICON_MOTION_SENSOR

from .. import CONF_C4002_ID, C4002Component, dfrobot_c4002_ns

C4002BinarySensor = dfrobot_c4002_ns.class_(
    "C4002BinarySensor", binary_sensor.BinarySensor, cg.Component
)


CONF_EXIST_STATE = "exist_state"
CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(C4002BinarySensor),
            cv.GenerateID(CONF_C4002_ID): cv.use_id(C4002Component),
            cv.Optional(CONF_EXIST_STATE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_MOTION,
                icon=ICON_MOTION_SENSOR,
            ),
        }
    ),
)


async def to_code(config):
    c4002_binary_sensor = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(c4002_binary_sensor, config)
    if CONF_EXIST_STATE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_EXIST_STATE])
        cg.add(c4002_binary_sensor.set_presence_sensor(sens))
    c4002_component = await cg.get_variable(config[CONF_C4002_ID])
    cg.add(c4002_component.register_listener(c4002_binary_sensor))
