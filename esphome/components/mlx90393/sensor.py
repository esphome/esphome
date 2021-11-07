import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    UNIT_MICROTESLA,
    STATE_CLASS_MEASUREMENT,
    ICON_MAGNET,
    CONF_GAIN,
)
from esphome import pins

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["i2c"]

mlx90393_ns = cg.esphome_ns.namespace("mlx90393")

MLX90393 = mlx90393_ns.class_("MLX90393_cls", cg.PollingComponent, i2c.I2CDevice)

GAINEnum = mlx90393_ns.enum("GAIN")
GAIN = {
    "GAIN_1X": GAINEnum.GAIN_1X,
    "GAIN_1_33X": GAINEnum.GAIN_1_33X,
    "GAIN_1_67X": GAINEnum.GAIN_1_67X,
    "GAIN_2X": GAINEnum.GAIN_2X,
    "GAIN_2_5X": GAINEnum.GAIN_2_5X,
    "GAIN_3X": GAINEnum.GAIN_3X,
    "GAIN_4X": GAINEnum.GAIN_4X,
    "GAIN_5X": GAINEnum.GAIN_5X,
}

CONF_X_AXIS = "x-axis"
CONF_Y_AXIS = "y-axis"
CONF_Z_AXIS = "z-axis"
CONF_DRDY_PIN = "drdy_pin"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(MLX90393),
            cv.Optional(CONF_GAIN, default="GAIN_2_5X"): cv.enum(
                GAIN, upper=True, space="_"
            ),
            cv.Optional(CONF_DRDY_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_X_AXIS): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROTESLA,
                accuracy_decimals=0,
                icon=ICON_MAGNET,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_Y_AXIS): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROTESLA,
                accuracy_decimals=0,
                icon=ICON_MAGNET,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_Z_AXIS): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROTESLA,
                accuracy_decimals=0,
                icon=ICON_MAGNET,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        },
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x0C))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_X_AXIS in config:
        sens = await sensor.new_sensor(config[CONF_X_AXIS])
        cg.add(var.set_x_sensor(sens))
    if CONF_Y_AXIS in config:
        sens = await sensor.new_sensor(config[CONF_Y_AXIS])
        cg.add(var.set_y_sensor(sens))
    if CONF_Z_AXIS in config:
        sens = await sensor.new_sensor(config[CONF_Z_AXIS])
        cg.add(var.set_z_sensor(sens))
    if CONF_DRDY_PIN in config:
        cg.add(var.set_pin(pin))


cg.add_library("functionpointer/arduino-MLX90393", "^0.0.4")
