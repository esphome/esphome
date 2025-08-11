import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_GAIN,
    CONF_ID,
    CONF_MAGNITUDE,
    CONF_POSITION,
    CONF_STATUS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_MAGNET,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
)

from .. import AS5600Component, as5600_ns

CODEOWNERS = ["@ammmze"]
DEPENDENCIES = ["as5600"]

AS5600Sensor = as5600_ns.class_("AS5600Sensor", sensor.Sensor, cg.PollingComponent)

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
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_AS5600_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    if out_of_range_mode_config := config.get(CONF_OUT_OF_RANGE_MODE):
        cg.add(var.set_out_of_range_mode(out_of_range_mode_config))

    if angle_config := config.get(CONF_ANGLE):
        sens = await sensor.new_sensor(angle_config)
        cg.add(var.set_angle_sensor(sens))

    if raw_angle_config := config.get(CONF_RAW_ANGLE):
        sens = await sensor.new_sensor(raw_angle_config)
        cg.add(var.set_raw_angle_sensor(sens))

    if position_config := config.get(CONF_POSITION):
        sens = await sensor.new_sensor(position_config)
        cg.add(var.set_position_sensor(sens))

    if raw_position_config := config.get(CONF_RAW_POSITION):
        sens = await sensor.new_sensor(raw_position_config)
        cg.add(var.set_raw_position_sensor(sens))

    if gain_config := config.get(CONF_GAIN):
        sens = await sensor.new_sensor(gain_config)
        cg.add(var.set_gain_sensor(sens))

    if magnitude_config := config.get(CONF_MAGNITUDE):
        sens = await sensor.new_sensor(magnitude_config)
        cg.add(var.set_magnitude_sensor(sens))

    if status_config := config.get(CONF_STATUS):
        sens = await sensor.new_sensor(status_config)
        cg.add(var.set_status_sensor(sens))
