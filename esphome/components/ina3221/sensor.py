import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ID,
    CONF_MODE,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_WATT,
)

DEPENDENCIES = ["i2c"]

CONF_CHANNEL_1 = "channel_1"
CONF_CHANNEL_2 = "channel_2"
CONF_CHANNEL_3 = "channel_3"
CONF_POWER_DOWN_ON_SHUTDOWN = "power_down_on_shutdown"

CONF_AVERAGING = "averaging"
CONF_BUS_CONVERSION_TIME = "bus_conversion_time"
CONF_SHUNT_CONVERSION_TIME = "shunt_conversion_time"
CONF_WARNING_CURRENT_LIMIT = "warning_current_limit"
CONF_CRITICAL_CURRENT_LIMIT = "critical_current_limit"

CONF_SUM_SHUNT_VOLTAGE = "sum_shunt_voltage"
CONF_SUM_CURRENT = "sum_current"
CONF_SUM_POWER = "sum_power"

ina3221_ns = cg.esphome_ns.namespace("ina3221")
INA3221Component = ina3221_ns.class_(
    "INA3221Component", cg.PollingComponent, i2c.I2CDevice
)

INA3221Mode = ina3221_ns.enum("INA3221Mode")
MODE_OPTIONS = {
    "CONTINUOUS": INA3221Mode.INA3221_MODE_CONTINUOUS,
    "SINGLE_SHOT": INA3221Mode.INA3221_MODE_SINGLE_SHOT,
}

INA3221Averaging = ina3221_ns.enum("INA3221Averaging")
AVERAGING_OPTIONS = {
    1: INA3221Averaging.INA3221_AVERAGING_1,
    4: INA3221Averaging.INA3221_AVERAGING_4,
    16: INA3221Averaging.INA3221_AVERAGING_16,
    64: INA3221Averaging.INA3221_AVERAGING_64,
    128: INA3221Averaging.INA3221_AVERAGING_128,
    256: INA3221Averaging.INA3221_AVERAGING_256,
    512: INA3221Averaging.INA3221_AVERAGING_512,
    1024: INA3221Averaging.INA3221_AVERAGING_1024,
}

INA3221ConversionTime = ina3221_ns.enum("INA3221ConversionTime")
CONVERSION_TIME_OPTIONS = {
    "140us": INA3221ConversionTime.INA3221_CONVERSION_TIME_140US,
    "204us": INA3221ConversionTime.INA3221_CONVERSION_TIME_204US,
    "332us": INA3221ConversionTime.INA3221_CONVERSION_TIME_332US,
    "588us": INA3221ConversionTime.INA3221_CONVERSION_TIME_588US,
    "1100us": INA3221ConversionTime.INA3221_CONVERSION_TIME_1100US,
    "2116us": INA3221ConversionTime.INA3221_CONVERSION_TIME_2116US,
    "4156us": INA3221ConversionTime.INA3221_CONVERSION_TIME_4156US,
    "8244us": INA3221ConversionTime.INA3221_CONVERSION_TIME_8244US,
}

INA3221_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SHUNT_RESISTANCE, default=0.1): cv.All(
            cv.resistance, cv.Range(min=0.0, max=32.0)
        ),
        cv.Optional(CONF_WARNING_CURRENT_LIMIT): cv.current,
        cv.Optional(CONF_CRITICAL_CURRENT_LIMIT): cv.current,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(INA3221Component),
            cv.Optional(CONF_CHANNEL_1): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_2): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_3): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_MODE, default="CONTINUOUS"): cv.enum(
                MODE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_POWER_DOWN_ON_SHUTDOWN, default=False): cv.boolean,
            cv.Optional(CONF_AVERAGING, default=1): cv.enum(
                AVERAGING_OPTIONS, int=True
            ),
            cv.Optional(CONF_BUS_CONVERSION_TIME, default="1100us"): cv.enum(
                CONVERSION_TIME_OPTIONS, lower=True
            ),
            cv.Optional(CONF_SHUNT_CONVERSION_TIME, default="1100us"): cv.enum(
                CONVERSION_TIME_OPTIONS, lower=True
            ),
            cv.Optional(CONF_SUM_SHUNT_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SUM_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SUM_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for i, channel in enumerate([CONF_CHANNEL_1, CONF_CHANNEL_2, CONF_CHANNEL_3]):
        if channel not in config:
            continue
        conf = config[channel]
        if CONF_SHUNT_RESISTANCE in conf:
            cg.add(var.set_shunt_resistance(i, conf[CONF_SHUNT_RESISTANCE]))
        if CONF_BUS_VOLTAGE in conf:
            sens = await sensor.new_sensor(conf[CONF_BUS_VOLTAGE])
            cg.add(var.set_bus_voltage_sensor(i, sens))
        if CONF_SHUNT_VOLTAGE in conf:
            sens = await sensor.new_sensor(conf[CONF_SHUNT_VOLTAGE])
            cg.add(var.set_shunt_voltage_sensor(i, sens))
        if CONF_CURRENT in conf:
            sens = await sensor.new_sensor(conf[CONF_CURRENT])
            cg.add(var.set_current_sensor(i, sens))
        if CONF_POWER in conf:
            sens = await sensor.new_sensor(conf[CONF_POWER])
            cg.add(var.set_power_sensor(i, sens))
        if CONF_WARNING_CURRENT_LIMIT in conf:
            cg.add(var.set_warning_current_limit(i, conf[CONF_WARNING_CURRENT_LIMIT]))
        if CONF_CRITICAL_CURRENT_LIMIT in conf:
            cg.add(var.set_critical_current_limit(i, conf[CONF_CRITICAL_CURRENT_LIMIT]))

    if CONF_MODE in config:
        cg.add(var.set_mode(config[CONF_MODE]))

    cg.add(var.set_power_down_on_shutdown(config[CONF_POWER_DOWN_ON_SHUTDOWN]))
    if CONF_AVERAGING in config:
        cg.add(var.set_averaging(config[CONF_AVERAGING]))
    if CONF_BUS_CONVERSION_TIME in config:
        cg.add(var.set_bus_conversion_time(config[CONF_BUS_CONVERSION_TIME]))
    if CONF_SHUNT_CONVERSION_TIME in config:
        cg.add(var.set_shunt_conversion_time(config[CONF_SHUNT_CONVERSION_TIME]))

    if CONF_SUM_SHUNT_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_SUM_SHUNT_VOLTAGE])
        cg.add(var.set_sum_shunt_voltage_sensor(sens))
    if CONF_SUM_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_SUM_CURRENT])
        cg.add(var.set_sum_current_sensor(sens))
    if CONF_SUM_POWER in config:
        sens = await sensor.new_sensor(config[CONF_SUM_POWER])
        cg.add(var.set_sum_power_sensor(sens))
