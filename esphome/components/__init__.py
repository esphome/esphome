import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, text_sensor, time
from esphome.const import (
    CONF_ID, CONF_TIME_ID, CONF_TEMPERATURE, CONF_ADDRESS, CONF_I2C_ID,
    DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS, ICON_THERMOMETER, CONF_TIME
)

CODEOWNERS = ["@your_github_username"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor", "time"]

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231Component = ds3231_ns.class_("DS3231Component", cg.PollingComponent, i2c.I2CDevice)

DEFAULT_ADDRESS = 0x68

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS3231Component),
            cv.Optional(CONF_I2C_ID): cv.use_id(i2c.I2CBus),
            cv.Optional(CONF_ADDRESS, default=DEFAULT_ADDRESS): cv.i2c_address,
            cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
            ),
            cv.Optional(CONF_TIME): text_sensor.text_sensor_schema(
                icon="mdi:clock",
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Set the I2C address
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    
    # Set the I2C bus if specified
    if CONF_I2C_ID in config:
        bus = await cg.get_variable(config[CONF_I2C_ID])
        cg.add(var.set_i2c_bus(bus))
    
    await i2c.register_i2c_device(var, config)

    # Time ID is required
    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time_id(time_))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_TIME in config:
        time_sens = await text_sensor.new_text_sensor(config[CONF_TIME])
        cg.add(var.set_time_text_sensor(time_sens))
