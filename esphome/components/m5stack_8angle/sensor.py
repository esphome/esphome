import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from esphome.const import (
    CONF_CHANNEL,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
)

from . import M5Stack_8AngleComponent, m5stack_8angle_ns, CONF_M5STACK_8ANGLE_ID


M5Stack_8AngleSensorKnob = m5stack_8angle_ns.class_(
    "M5Stack_8AngleSensorKnob",
    sensor.Sensor,
    cg.PollingComponent,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Stack_8AngleSensorKnob),
            cv.GenerateID(CONF_M5STACK_8ANGLE_ID): cv.use_id(M5Stack_8AngleComponent),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=8),
        }
    )
    .extend(
        sensor.sensor_schema(
            M5Stack_8AngleSensorKnob,
            accuracy_decimals=2,
            icon=ICON_ROTATE_RIGHT,
            state_class=STATE_CLASS_MEASUREMENT,
        )
    )
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_M5STACK_8ANGLE_ID])
    sens = await sensor.new_sensor(config)
    cg.add(sens.set_parent(hub, config[CONF_CHANNEL] - 1))
    await cg.register_component(sens, config)
