import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PASCAL,
)

DEPENDENCIES = ["i2c"]

ams5915_ns = cg.esphome_ns.namespace("ams5915")
Transducer = ams5915_ns.enum("Transducer")
MODEL_PRESSURE_RANGES = {
    "AMS5915_0005_D": {0, 5},
    "AMS5915_0010_D": {0, 10},
    "AMS5915_0005_D_B": {-5, 5},
    "AMS5915_0010_D_B": {-10, 10},
    "AMS5915_0020_D": {0, 20},
    "AMS5915_0050_D": {0, 50},
    "AMS5915_0100_D": {0, 100},
    "AMS5915_0020_D_B": {-20, 20},
    "AMS5915_0050_D_B": {-50, 50},
    "AMS5915_0100_D_B": {-100, 100},
    "AMS5915_0200_D": {0, 200},
    "AMS5915_0350_D": {0, 350},
    "AMS5915_1000_D": {0, 1000},
    "AMS5915_2000_D": {0, 2000},
    "AMS5915_4000_D": {0, 4000},
    "AMS5915_7000_D": {0, 7000},
    "AMS5915_10000_D": {0, 10000},
    "AMS5915_0200_D_B": {-200, 200},
    "AMS5915_0350_D_B": {-350, 350},
    "AMS5915_1000_D_B": {-1000, 1000},
    "AMS5915_1000_A": {0, 1000},
    "AMS5915_1200_B": {700, 1200},
}

Ams5915 = ams5915_ns.class_("Ams5915", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ams5915),
            cv.Required(CONF_MODEL): cv.enum(MODEL_PRESSURE_RANGES, upper=True),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PASCAL,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    min_pressure, max_pressure = MODEL_PRESSURE_RANGES[config[CONF_MODEL]]
    cg.add(var.set_pressure_range(min_pressure, max_pressure))

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
