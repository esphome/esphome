from esphome import core
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DURATION,
    CONF_GAS_RESISTANCE,
    CONF_HEATER,
    CONF_HUMIDITY,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    ICON_GAS_CYLINDER,
    STATE_CLASS_MEASUREMENT,
    UNIT_HECTOPASCAL,
    UNIT_OHM,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

bme680_ns = cg.esphome_ns.namespace("bme680")
BME680Oversampling = bme680_ns.enum("BME680Oversampling")
OVERSAMPLING_OPTIONS = BME680Oversampling.options(
    "NONE", "1X", "2X", "4X", "8X", "16X", prefix="BME680_OVERSAMPLING_"
)

BME680IIRFilter = bme680_ns.enum("BME680IIRFilter")
IIR_FILTER_OPTIONS = BME680IIRFilter.options(
    "OFF", "1X", "3X", "7X", "15X", "31X", "63X", "127X", prefix="BME680_IIR_FILTER_"
)

BME680Component = bme680_ns.class_(
    "BME680Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME680Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
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
                kind=sensor.HUMIDITY
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
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x76))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    if await sensors(CONF_TEMPERATURE, var.set_temperature_sensor):
        oversampling = config[CONF_TEMPERATURE][CONF_OVERSAMPLING]
        cg.add(var.set_temperature_oversampling(oversampling))
    if await sensors(CONF_PRESSURE, var.set_pressure_sensor):
        cg.add(var.set_pressure_oversampling(config[CONF_PRESSURE][CONF_OVERSAMPLING]))
    if await sensors(CONF_HUMIDITY, var.set_humidity_sensor):
        cg.add(var.set_humidity_oversampling(config[CONF_HUMIDITY][CONF_OVERSAMPLING]))
    await sensors(CONF_GAS_RESISTANCE, var.set_gas_resistance_sensor)

    cg.add(var.set_iir_filter(IIR_FILTER_OPTIONS[config[CONF_IIR_FILTER]]))

    if CONF_HEATER in config:
        conf = config[CONF_HEATER]
        if not conf:
            cg.add(var.set_heater(0, 0))
        else:
            cg.add(var.set_heater(conf[CONF_TEMPERATURE], conf[CONF_DURATION]))
