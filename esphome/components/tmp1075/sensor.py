import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    ICON_THERMOMETER,
)

DEPENDENCIES = ["i2c"]

tmp1075_ns = cg.esphome_ns.namespace("tmp1075")

TMP1075Sensor = tmp1075_ns.class_(
    "TMP1075Sensor", cg.PollingComponent, sensor.Sensor, i2c.I2CDevice
)

EConversionRate = tmp1075_ns.enum("EConversionRate")
CONVERSION_RATES = {
    "27.5ms": EConversionRate.CONV_RATE_27_5_MS,
    "55ms": EConversionRate.CONV_RATE_55_MS,
    "110ms": EConversionRate.CONV_RATE_110_MS,
    "220ms": EConversionRate.CONV_RATE_220_MS,
}

POLARITY = {
    "ACTIVE_LOW": 0,
    "ACTIVE_HIGH": 1,
}

EAlertFunction = tmp1075_ns.enum("EAlertFunction")
ALERT_FUNCTION = {
    "COMPARATOR": EAlertFunction.ALERT_COMPARATOR,
    "INTERRUPT": EAlertFunction.ALERT_INTERRUPT,
}

CONF_ALERT = "alert"
CONF_LIMIT_LOW = "limit_low"
CONF_LIMIT_HIGH = "limit_high"
CONF_FAULT_COUNT = "fault_count"
CONF_POLARITY = "polarity"
CONF_CONVERSION_RATE = "conversion_rate"
CONF_FUNCTION = "function"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        TMP1075Sensor,
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_CONVERSION_RATE): cv.enum(CONVERSION_RATES, lower=True),
            cv.Optional(CONF_ALERT, default={}): cv.Schema(
                {
                    cv.Optional(CONF_LIMIT_LOW): cv.temperature,
                    cv.Optional(CONF_LIMIT_HIGH): cv.temperature,
                    cv.Optional(CONF_FAULT_COUNT): cv.int_range(min=1, max=4),
                    cv.Optional(CONF_POLARITY): cv.enum(POLARITY, upper=True),
                    cv.Optional(CONF_FUNCTION): cv.enum(ALERT_FUNCTION, upper=True),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_CONVERSION_RATE in config:
        cg.add(var.set_conversion_rate(config[CONF_CONVERSION_RATE]))

    alert = config[CONF_ALERT]
    if CONF_LIMIT_LOW in alert:
        cg.add(var.set_alert_limit_low(alert[CONF_LIMIT_LOW]))
    if CONF_LIMIT_HIGH in alert:
        cg.add(var.set_alert_limit_high(alert[CONF_LIMIT_HIGH]))
    if CONF_FAULT_COUNT in alert:
        cg.add(var.set_fault_count(alert[CONF_FAULT_COUNT]))
    if CONF_POLARITY in alert:
        cg.add(var.set_alert_polarity(alert[CONF_POLARITY]))
    if CONF_FUNCTION in alert:
        cg.add(var.set_alert_function(alert[CONF_FUNCTION]))
