import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]

CONF_AMBIENT = "ambient"
CONF_EMISSIVITY = "emissivity"
CONF_OBJECT = "object"

mlx90614_ns = cg.esphome_ns.namespace("mlx90614")
MLX90614Component = mlx90614_ns.class_(
    "MLX90614Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90614Component),
            cv.Optional(CONF_AMBIENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OBJECT): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_EMISSIVITY, default=1.0): cv.percentage,
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_AMBIENT in config:
        sens = await sensor.new_sensor(config[CONF_AMBIENT])
        cg.add(var.set_ambient_sensor(sens))

    if CONF_OBJECT in config:
        sens = await sensor.new_sensor(config[CONF_OBJECT])
        cg.add(var.set_object_sensor(sens))

        cg.add(var.set_emissivity(config[CONF_OBJECT][CONF_EMISSIVITY]))
