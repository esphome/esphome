import esphome.codegen as cg
from esphome.components import binary_sensor, i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_ID,
    CONF_NAME,
    CONF_UPDATE_INTERVAL,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@RoganDawes"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor", "sensor"]
MULTI_CONF = True

CONF_C_BUTTON = "c_button"
CONF_JOYSTICK_X = "joystick_x"
CONF_JOYSTICK_Y = "joystick_y"
CONF_Z_BUTTON = "z_button"

wii_nunchuk_ns = cg.esphome_ns.namespace("wii_nunchuk")
WiiNunchukComponent = wii_nunchuk_ns.class_(
    "WiiNunchukComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WiiNunchukComponent),
            cv.Optional(
                CONF_UPDATE_INTERVAL, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_JOYSTICK_X): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon="mdi:gamepad-square",
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_JOYSTICK_Y): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon="mdi:gamepad-square",
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ACCELERATION_X): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_ACCELERATION_X,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ACCELERATION_Y): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_ACCELERATION_Y,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ACCELERATION_Z): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_ACCELERATION_Z,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_C_BUTTON): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(icon="mdi:alpha-c-box"),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_Z_BUTTON): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(icon="mdi:alpha-z-box"),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x52))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL].total_milliseconds))

    if joystick_x_config := config.get(CONF_JOYSTICK_X):
        sens = await sensor.new_sensor(joystick_x_config)
        cg.add(var.set_joystick_x_sensor(sens))

    if joystick_y_config := config.get(CONF_JOYSTICK_Y):
        sens = await sensor.new_sensor(joystick_y_config)
        cg.add(var.set_joystick_y_sensor(sens))

    if acceleration_x_config := config.get(CONF_ACCELERATION_X):
        sens = await sensor.new_sensor(acceleration_x_config)
        cg.add(var.set_acceleration_x_sensor(sens))

    if acceleration_y_config := config.get(CONF_ACCELERATION_Y):
        sens = await sensor.new_sensor(acceleration_y_config)
        cg.add(var.set_acceleration_y_sensor(sens))

    if acceleration_z_config := config.get(CONF_ACCELERATION_Z):
        sens = await sensor.new_sensor(acceleration_z_config)
        cg.add(var.set_acceleration_z_sensor(sens))

    if c_button_config := config.get(CONF_C_BUTTON):
        sens = await binary_sensor.new_binary_sensor(c_button_config)
        cg.add(var.set_c_button_binary_sensor(sens))

    if z_button_config := config.get(CONF_Z_BUTTON):
        sens = await binary_sensor.new_binary_sensor(z_button_config)
        cg.add(var.set_z_button_binary_sensor(sens))
