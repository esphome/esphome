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
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from .. import CONF_BATTERY, CONF_PYLONTECH_ID, PYLONTECH_COMPONENT_SCHEMA, pylontech_ns

PylontechSensor = pylontech_ns.class_("PylontechSensor", cg.Component)
PylontechCellSensor = pylontech_ns.class_("PylontechCellSensor")

CONF_COULOMB = "coulomb"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_VOLTAGE_LOW = "voltage_low"
CONF_VOLTAGE_HIGH = "voltage_high"
CONF_MOS_TEMPERATURE = "mos_temperature"
CONF_CELLS = "cells"
CONF_CELL = "cell"

NUM_CELLS = 15

# Sensors from the "pwr" command
TYPES: dict[str, cv.Schema] = {
    CONF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_TEMPERATURE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_TEMPERATURE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_VOLTAGE_LOW: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_VOLTAGE_HIGH: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_COULOMB: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_MOS_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

# Cell-level sensor schema (nested under "cells:" list)
CELL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PylontechCellSensor),
        cv.Required(CONF_CELL): cv.int_range(min=0, max=NUM_CELLS - 1),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
    }
)

CONFIG_SCHEMA = (
    PYLONTECH_COMPONENT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(PylontechSensor)})
    .extend({cv.Optional(marker): schema for marker, schema in TYPES.items()})
    .extend(
        {
            cv.Optional(CONF_CELLS): cv.ensure_list(CELL_SCHEMA),
        }
    )
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    # Register pwr sensors
    for marker in TYPES:
        if marker_config := config.get(marker):
            sens = await sensor.new_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_sensor")(sens))

    cg.add(paren.register_listener(bat))

    # Register cell sensors as individual listeners
    if CONF_CELLS in config:
        cg.add(paren.set_cell_polling_enabled(True))
        cg.add(paren.request_cell_data(config[CONF_BATTERY]))
        for cell_config in config[CONF_CELLS]:
            cell = cg.new_Pvariable(
                cell_config[CONF_ID], config[CONF_BATTERY], cell_config[CONF_CELL]
            )
            if v_config := cell_config.get(CONF_VOLTAGE):
                sens = await sensor.new_sensor(v_config)
                cg.add(cell.set_voltage_sensor(sens))
            if t_config := cell_config.get(CONF_TEMPERATURE):
                sens = await sensor.new_sensor(t_config)
                cg.add(cell.set_temperature_sensor(sens))
            cg.add(paren.register_listener(cell))
