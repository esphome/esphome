import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_PRESSURE,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_TEMPERATURE,
    CONF_TEMPERATURE,
    UNIT_CELSIUS,
    UNIT_PASCAL,
    CONF_UPDATE_INTERVAL,
    CONF_PRESSURE,
    CONF_MODEL,
)

DEPENDENCIES = ["i2c"]
# AUTO_LOAD = ["sensor"]

ams5915_ns = cg.esphome_ns.namespace("ams5915")
TRANSDUCER = ams5915_ns.enum("Ams5915::Transducer")
TRANSDUCER_TYPE = {
    "AMS5915_0005_D": TRANSDUCER.AMS5915_0005_D,
    "AMS5915_0010_D": TRANSDUCER.AMS5915_0010_D,
    "AMS5915_0005_D_B": TRANSDUCER.AMS5915_0005_D_B,
    "AMS5915_0010_D_B": TRANSDUCER.AMS5915_0010_D_B,
    "AMS5915_0020_D": TRANSDUCER.AMS5915_0020_D,
    "AMS5915_0050_D": TRANSDUCER.AMS5915_0050_D,
    "AMS5915_0100_D": TRANSDUCER.AMS5915_0100_D,
    "AMS5915_0020_D_B": TRANSDUCER.AMS5915_0020_D_B,
    "AMS5915_0050_D_B": TRANSDUCER.AMS5915_0050_D_B,
    "AMS5915_0100_D_B": TRANSDUCER.AMS5915_0100_D_B,
    "AMS5915_0200_D": TRANSDUCER.AMS5915_0200_D,
    "AMS5915_0350_D": TRANSDUCER.AMS5915_0350_D,
    "AMS5915_1000_D": TRANSDUCER.AMS5915_1000_D,
    "AMS5915_2000_D": TRANSDUCER.AMS5915_2000_D,
    "AMS5915_4000_D": TRANSDUCER.AMS5915_4000_D,
    "AMS5915_7000_D": TRANSDUCER.AMS5915_7000_D,
    "AMS5915_10000_D": TRANSDUCER.AMS5915_10000_D,
    "AMS5915_0200_D_B": TRANSDUCER.AMS5915_0200_D_B,
    "AMS5915_0350_D_B": TRANSDUCER.AMS5915_0350_D_B,
    "AMS5915_1000_D_B": TRANSDUCER.AMS5915_1000_D_B,
    "AMS5915_1000_A": TRANSDUCER.AMS5915_1000_A,
    "AMS5915_1200_B": TRANSDUCER.AMS5915_1200_B,
}
Ams5915 = ams5915_ns.class_("Ams5915", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ams5915),
            cv.Required(CONF_MODEL): cv.enum(TRANSDUCER_TYPE, upper=True),
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
    .extend(cv.polling_component_schema(CONF_UPDATE_INTERVAL))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_transducer_type(config.get(CONF_MODEL)))

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
