import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HEATER,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_POWER_MODE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

hdc302x_ns = cg.esphome_ns.namespace("hdc302x")
HDC302XComponent = hdc302x_ns.class_(
    "HDC302XComponent", cg.PollingComponent, i2c.I2CDevice
)

HDC302XPowerMode = hdc302x_ns.enum("HDC302XPowerMode")
POWER_MODE_OPTIONS = {
    "HIGH_ACCURACY": HDC302XPowerMode.HIGH_ACCURACY,
    "BALANCED": HDC302XPowerMode.BALANCED,
    "LOW_POWER": HDC302XPowerMode.LOW_POWER,
    "ULTRA_LOW_POWER": HDC302XPowerMode.ULTRA_LOW_POWER,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HDC302XComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_MODE, default="HIGH_ACCURACY"): cv.enum(
                POWER_MODE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_HEATER, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x44))  # Default address per datasheet, Table 7-2.
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if temp_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temp_config)
        cg.add(var.set_temp_sensor(sens))

    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(var.set_humidity_sensor(sens))

    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_heater_enabled(config[CONF_HEATER]))
