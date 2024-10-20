import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    UNIT_MICROTESLA,
    UNIT_CELSIUS,
    STATE_CLASS_MEASUREMENT,
    ICON_MAGNET,
    ICON_THERMOMETER,
    CONF_GAIN,
    CONF_RESOLUTION,
    CONF_OVERSAMPLING,
    CONF_FILTER,
    CONF_TEMPERATURE,
)
from esphome import pins

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["i2c"]

mlx90393_ns = cg.esphome_ns.namespace("mlx90393")

MLX90393Component = mlx90393_ns.class_(
    "MLX90393Cls", cg.PollingComponent, i2c.I2CDevice
)

GAIN = {
    "1X": 7,
    "1_33X": 6,
    "1_67X": 5,
    "2X": 4,
    "2_5X": 3,
    "3X": 2,
    "4X": 1,
    "5X": 0,
}

RESOLUTION = {
    "16BIT": 0,
    "17BIT": 1,
    "18BIT": 2,
    "19BIT": 3,
}

CONF_X_AXIS = "x_axis"
CONF_Y_AXIS = "y_axis"
CONF_Z_AXIS = "z_axis"
CONF_DRDY_PIN = "drdy_pin"


def mlx90393_axis_schema(default_resolution: str):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_MICROTESLA,
        accuracy_decimals=0,
        icon=ICON_MAGNET,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        cv.Schema(
            {
                cv.Optional(CONF_RESOLUTION, default=default_resolution): cv.enum(
                    RESOLUTION, upper=True, space="_"
                )
            }
        )
    )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90393Component),
            cv.Optional(CONF_GAIN, default="2_5X"): cv.enum(
                GAIN, upper=True, space="_"
            ),
            cv.Optional(CONF_DRDY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_OVERSAMPLING, default=2): cv.int_range(min=0, max=3),
            cv.Optional(CONF_FILTER, default=6): cv.int_range(min=0, max=7),
            cv.Optional(CONF_X_AXIS): mlx90393_axis_schema("19BIT"),
            cv.Optional(CONF_Y_AXIS): mlx90393_axis_schema("19BIT"),
            cv.Optional(CONF_Z_AXIS): mlx90393_axis_schema("16BIT"),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                icon=ICON_THERMOMETER,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                cv.Schema(
                    {
                        cv.Optional(CONF_OVERSAMPLING, default=0): cv.int_range(
                            min=0, max=3
                        ),
                    }
                )
            ),
        },
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x0C))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_DRDY_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_DRDY_PIN])
        cg.add(var.set_drdy_pin(pin))
    cg.add(var.set_gain(GAIN[config[CONF_GAIN]]))
    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_filter(config[CONF_FILTER]))

    if CONF_X_AXIS in config:
        sens = await sensor.new_sensor(config[CONF_X_AXIS])
        cg.add(var.set_x_sensor(sens))
        cg.add(var.set_resolution(0, RESOLUTION[config[CONF_X_AXIS][CONF_RESOLUTION]]))
    if CONF_Y_AXIS in config:
        sens = await sensor.new_sensor(config[CONF_Y_AXIS])
        cg.add(var.set_y_sensor(sens))
        cg.add(var.set_resolution(1, RESOLUTION[config[CONF_Y_AXIS][CONF_RESOLUTION]]))
    if CONF_Z_AXIS in config:
        sens = await sensor.new_sensor(config[CONF_Z_AXIS])
        cg.add(var.set_z_sensor(sens))
        cg.add(var.set_resolution(2, RESOLUTION[config[CONF_Z_AXIS][CONF_RESOLUTION]]))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_t_sensor(sens))
        cg.add(var.set_t_oversampling(config[CONF_TEMPERATURE][CONF_OVERSAMPLING]))
    if CONF_DRDY_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_DRDY_PIN])
        cg.add(var.set_drdy_gpio(pin))

    cg.add_library("functionpointer/arduino-MLX90393", "1.0.2")
