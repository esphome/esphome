import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from .. import CONF_BATTERY, CONF_PYLONTECH_ID, PYLONTECH_COMPONENT_SCHEMA, pylontech_ns

PylontechSensor = pylontech_ns.class_("PylontechSensor", cg.Component)

CONF_COULOMB = "coulomb"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_VOLTAGE_LOW = "voltage_low"
CONF_VOLTAGE_HIGH = "voltage_high"
CONF_MOS_TEMPERATURE = "mos_temperature"

TYPES: dict[str, cv.Schema] = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_VOLTAGE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_VOLTAGE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_COULOMB: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_BATTERY,
    ),
    CONF_MOS_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
}

# Add individual cell voltage sensors
CONF_CELL_VOLTAGES = [f"cell_{i+1}_voltage" for i in range(15)]

# 1. Base Schema
CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PylontechSensor),
    }
)

# 2. Add global sensors
CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {cv.Optional(marker): schema for marker, schema in TYPES.items()}
)

# 3. Add cell sensors
CELL_SCHEMA = {
    cv.Optional(conf): sensor.sensor_schema(
        unit_of_measurement="V",
        accuracy_decimals=3,
        device_class="voltage",
        state_class="measurement",
    ) for conf in CONF_CELL_VOLTAGES
}

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(CELL_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    for marker in TYPES:
        if marker_config := config.get(marker):
            sens = await sensor.new_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_sensor")(sens))

    # Generate cell sensors
    for i, conf in enumerate(CONF_CELL_VOLTAGES):
        if conf in config:
            sens = await sensor.new_sensor(config[conf])
            cg.add(bat.set_cell_voltage_sensor(i, sens))
    
    # Notify main component about highest configured battery ID
    cg.add(paren.set_max_battery(config[CONF_BATTERY]))

    cg.add(paren.register_listener(bat))
