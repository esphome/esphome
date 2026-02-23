from esphome import core
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DURATION,
    CONF_GAS_RESISTANCE,
    CONF_HEATER,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_GAS_CYLINDER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_OHM,
    UNIT_PERCENT,
)

from . import (
    CONF_BME680_ID,
    BME680Component,
    OVERSAMPLING_OPTIONS,
    IIR_FILTER_OPTIONS,
)

DEPENDENCIES = ["i2c"]


def validate_heater(config):
    """Validate heater configuration."""
    if not config:
        return None
    if not isinstance(config, dict):
        return None
    if CONF_TEMPERATURE not in config and CONF_DURATION not in config:
        raise cv.Invalid("Heater must have at least temperature or duration")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME680_ID): cv.use_id(BME680Component),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                    OVERSAMPLING_OPTIONS, upper=True
                ),
            }
        ),
        cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                    OVERSAMPLING_OPTIONS, upper=True
                ),
            }
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {
                cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                    OVERSAMPLING_OPTIONS, upper=True
                ),
            }
        ),
        cv.Optional(CONF_GAS_RESISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_OHM,
            icon=ICON_GAS_CYLINDER,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
            IIR_FILTER_OPTIONS, upper=True
        ),
        cv.Optional(CONF_HEATER): cv.Any(
            None,
            cv.All(
                cv.Schema(
                    {
                        cv.Optional(CONF_TEMPERATURE, default=320): cv.int_range(
                            min=200, max=400
                        ),
                        cv.Optional(CONF_DURATION, default="150ms"): cv.All(
                            cv.positive_time_period_milliseconds,
                            cv.Range(max=core.TimePeriod(milliseconds=4032)),
                        ),
                    }
                ),
                cv.has_at_least_one_key(CONF_TEMPERATURE, CONF_DURATION),
            ),
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling(temperature_config[CONF_OVERSAMPLING]))

    if pressure_config := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(pressure_config)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling(pressure_config[CONF_OVERSAMPLING]))

    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(var.set_humidity_sensor(sens))
        cg.add(var.set_humidity_oversampling(humidity_config[CONF_OVERSAMPLING]))

    if gas_resistance_config := config.get(CONF_GAS_RESISTANCE):
        sens = await sensor.new_sensor(gas_resistance_config)
        cg.add(var.set_gas_resistance_sensor(sens))

    cg.add(var.set_iir_filter(IIR_FILTER_OPTIONS[config[CONF_IIR_FILTER]]))

    if CONF_HEATER in config:
        conf = config[CONF_HEATER]
        if not conf:
            cg.add(var.set_heater(0, 0))
        else:
            cg.add(var.set_heater(conf[CONF_TEMPERATURE], conf[CONF_DURATION]))
