import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CAPACITANCE,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
)

CODEOWNERS = ["@thegreatco"]

DEPENDENCIES = ["i2c"]

adafruit_soil_sensor_ns = cg.esphome_ns.namespace("adafruit_soil_sensor")
AdafruitSoilSensor = adafruit_soil_sensor_ns.class_(
    "AdafruitSoilSensor", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AdafruitSoilSensor),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CAPACITANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x36)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if (conf_temperature := config.get(CONF_TEMPERATURE)) is not None:
        ct = await sensor.new_sensor(conf_temperature)
        cg.add(var.set_temperature(ct))

    if (conf_capacitance := config.get(CONF_CAPACITANCE)) is not None:
        cmr = await sensor.new_sensor(conf_capacitance)
        cg.add(var.set_capacitance(cmr))
