import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    ICON_MAGNET,
    ICON_ROTATE_RIGHT,
    CONF_GAIN,
    ENTITY_CATEGORY_DIAGNOSTIC,
    CONF_MAGNITUDE,
    CONF_STATUS,
    CONF_POSITION,
)
from . import as5600_ns, AS5600Component

CODEOWNERS = ["@ammmze"]
DEPENDENCIES = ["as5600"]
AUTO_LOAD = ["as5600"]

AS5600Sensor = as5600_ns.class_("AS5600Sensor", sensor.Sensor, cg.PollingComponent)

CONF_ANGLE = "angle"
CONF_RAW_ANGLE = "raw_angle"
CONF_RAW_POSITION = "raw_position"
CONF_WATCHDOG = "watchdog"
CONF_POWER_MODE = "power_mode"
CONF_SLOW_FILTER = "slow_filter"
CONF_FAST_FILTER = "fast_filter"
CONF_PWM_FREQUENCY = "pwm_frequency"
CONF_BURN_COUNT = "burn_count"
CONF_START_POSITION = "start_position"
CONF_END_POSITION = "end_position"
CONF_OUT_OF_RANGE_MODE = "out_of_range_mode"

OutOfRangeMode = as5600_ns.enum("OutRangeMode")
OUT_OF_RANGE_MODES = {
    "MIN_MAX": OutOfRangeMode.OUT_RANGE_MODE_MIN_MAX,
    "NAN": OutOfRangeMode.OUT_RANGE_MODE_NAN,
}


CONF_AS5600_ID = "as5600_id"
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        AS5600Sensor,
        accuracy_decimals=0,
        icon=ICON_ROTATE_RIGHT,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_AS5600_ID): cv.use_id(AS5600Component),
            cv.Optional(CONF_OUT_OF_RANGE_MODE): cv.enum(
                OUT_OF_RANGE_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_RAW_POSITION): sensor.sensor_schema(
                accuracy_decimals=0,
                icon=ICON_ROTATE_RIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GAIN): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MAGNITUDE): sensor.sensor_schema(
                accuracy_decimals=0,
                icon=ICON_MAGNET,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_STATUS): sensor.sensor_schema(
                accuracy_decimals=0,
                icon=ICON_MAGNET,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_AS5600_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)

    if CONF_OUT_OF_RANGE_MODE in config:
        cg.add(var.set_out_of_range_mode(config[CONF_OUT_OF_RANGE_MODE]))

    if CONF_ANGLE in config:
        sens = await sensor.new_sensor(config[CONF_ANGLE])
        cg.add(var.set_angle_sensor(sens))

    if CONF_RAW_ANGLE in config:
        sens = await sensor.new_sensor(config[CONF_RAW_ANGLE])
        cg.add(var.set_raw_angle_sensor(sens))

    if CONF_POSITION in config:
        sens = await sensor.new_sensor(config[CONF_POSITION])
        cg.add(var.set_position_sensor(sens))

    if CONF_RAW_POSITION in config:
        sens = await sensor.new_sensor(config[CONF_RAW_POSITION])
        cg.add(var.set_raw_position_sensor(sens))

    if CONF_GAIN in config:
        sens = await sensor.new_sensor(config[CONF_GAIN])
        cg.add(var.set_gain_sensor(sens))

    if CONF_MAGNITUDE in config:
        sens = await sensor.new_sensor(config[CONF_MAGNITUDE])
        cg.add(var.set_magnitude_sensor(sens))

    if CONF_STATUS in config:
        sens = await sensor.new_sensor(config[CONF_STATUS])
        cg.add(var.set_status_sensor(sens))
