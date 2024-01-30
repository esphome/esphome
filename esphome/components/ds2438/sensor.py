import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ds248x, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)

DEPENDENCIES = ["ds248x"]

ds2438_ns = cg.esphome_ns.namespace("ds2438")
DS2348BatterySensor = ds2438_ns.class_("DS2438BatterySensor", ds248x.DS248xSensor)

CONF_TEMPERATURE = "temperature"
CONF_VOLTAGE = "voltage"
CONF_CURRENT = "current"

temperature_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

voltage_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)

current_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS2348BatterySensor),
            cv.Optional(CONF_TEMPERATURE): temperature_schema,
            cv.Optional(CONF_VOLTAGE): voltage_schema,
            cv.Optional(CONF_CURRENT): current_schema,
        }
    ).extend(ds248x.ds248x_sensor_schema()),
    cv.has_at_least_one_key(CONF_TEMPERATURE, CONF_VOLTAGE, CONF_CURRENT),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # await cg.register_component(var, config)
    await ds248x.register_ds248x_sensor(var, config)

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_temperature_sensor(sens))
