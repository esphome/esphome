import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, sensirion_common
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_HUMIDITY,
    CONF_UPDATE_INTERVAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
)

CODEOWNERS = ["@sjtrny"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sht4x_ns = cg.esphome_ns.namespace("sht4x")

SHT4XComponent = sht4x_ns.class_(
    "SHT4XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

CONF_PRECISION = "precision"
SHT4XPRECISION = sht4x_ns.enum("SHT4XPRECISION")
PRECISION_OPTIONS = {
    "High": SHT4XPRECISION.SHT4X_PRECISION_HIGH,
    "Med": SHT4XPRECISION.SHT4X_PRECISION_MED,
    "Low": SHT4XPRECISION.SHT4X_PRECISION_LOW,
}

CONF_HEATER_POWER = "heater_power"
SHT4XHEATERPOWER = sht4x_ns.enum("SHT4XHEATERPOWER")
HEATER_POWER_OPTIONS = {
    "High": SHT4XHEATERPOWER.SHT4X_HEATERPOWER_HIGH,
    "Med": SHT4XHEATERPOWER.SHT4X_HEATERPOWER_MED,
    "Low": SHT4XHEATERPOWER.SHT4X_HEATERPOWER_LOW,
}

CONF_HEATER_TIME = "heater_time"
SHT4XHEATERTIME = sht4x_ns.enum("SHT4XHEATERTIME")
HEATER_TIME_OPTIONS = {
    "Long": SHT4XHEATERTIME.SHT4X_HEATERTIME_LONG,
    "Short": SHT4XHEATERTIME.SHT4X_HEATERTIME_SHORT,
}

CONF_HEATER_PERIOD = "heater_period"


def _validate(config):
    if config[CONF_HEATER_PERIOD] > 0:
        HEATER_TIME_VALUE = {"Long": 1000, "Short": 100}
        update_interval = config[CONF_UPDATE_INTERVAL].total_milliseconds
        heater_time = HEATER_TIME_VALUE[config[CONF_HEATER_TIME]]
        heater_period = config[CONF_HEATER_PERIOD]
        duty_cycle = 100 * heater_time / (update_interval * heater_period)
        if duty_cycle > 10:
            raise cv.Invalid(
                f"Heater duty cycle {duty_cycle:.2f}% is greater than maximum allowed 10%. You can increase update_interval or heater_period or decrease heater_time"
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SHT4XComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRECISION, default="High"): cv.enum(PRECISION_OPTIONS),
            cv.Optional(CONF_HEATER_POWER, default="High"): cv.enum(
                HEATER_POWER_OPTIONS
            ),
            cv.Optional(CONF_HEATER_TIME, default="Long"): cv.enum(HEATER_TIME_OPTIONS),
            cv.Optional(CONF_HEATER_PERIOD, default=0): cv.uint8_t,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x44)),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_precision(config[CONF_PRECISION]))
    cg.add(var.set_heater_power(config[CONF_HEATER_POWER]))
    cg.add(var.set_heater_time(config[CONF_HEATER_TIME]))
    cg.add(var.set_heater_period(config[CONF_HEATER_PERIOD]))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temp_sensor(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
