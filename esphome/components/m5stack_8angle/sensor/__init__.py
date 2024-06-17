import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from esphome.const import (
    CONF_BIT_DEPTH,
    CONF_CHANNEL,
    CONF_RAW,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
)

from .. import (
    AnalogBits,
    M5Stack8AngleComponent,
    m5stack_8angle_ns,
    CONF_M5STACK_8ANGLE_ID,
)


M5Stack8AngleKnobSensor = m5stack_8angle_ns.class_(
    "M5Stack8AngleKnobSensor",
    sensor.Sensor,
    cg.PollingComponent,
)


BIT_DEPTHS = {
    8: AnalogBits.BITS_8,
    12: AnalogBits.BITS_12,
}

_validate_bits = cv.float_with_unit("bits", "bit")


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Stack8AngleKnobSensor),
            cv.GenerateID(CONF_M5STACK_8ANGLE_ID): cv.use_id(M5Stack8AngleComponent),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=8),
            cv.Optional(CONF_BIT_DEPTH, default="8bit"): cv.All(
                _validate_bits, cv.enum(BIT_DEPTHS)
            ),
            cv.Optional(CONF_RAW, default=False): cv.boolean,
        }
    )
    .extend(
        sensor.sensor_schema(
            M5Stack8AngleKnobSensor,
            accuracy_decimals=2,
            icon=ICON_ROTATE_RIGHT,
            state_class=STATE_CLASS_MEASUREMENT,
        )
    )
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5STACK_8ANGLE_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL] - 1))
    cg.add(var.set_bit_depth(BIT_DEPTHS[config[CONF_BIT_DEPTH]]))
    cg.add(var.set_raw(config[CONF_RAW]))
