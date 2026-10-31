import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_BATTERY as DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT as DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE as DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE as DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT as STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE as UNIT_AMPERE,
    UNIT_CELSIUS as UNIT_CELSIUS,
    UNIT_PERCENT as UNIT_PERCENT,
    UNIT_VOLT as UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import CONF_BATTERY, PYLONTECH_COMPONENT_SCHEMA, pylontech_ns, register_listener

PylontechSensor = pylontech_ns.class_("PylontechSensor")

CONF_COULOMB = "coulomb"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_VOLTAGE_LOW = "voltage_low"
CONF_VOLTAGE_HIGH = "voltage_high"
CONF_MOS_TEMPERATURE = "mos_temperature"
CONF_CELLS = "cells"
CONF_CELL = "cell"

# A battery reports cells 0 to NUM_CELLS - 1
NUM_CELLS = 15

_request_cell_slot = cg.slot_counter("PYLONTECH_SENSOR_CELL_COUNT")

TYPES: dict[str, cv.Schema] = {
    CONF_VOLTAGE: sensor.sensor_schema(kind=sensor.VOLTAGE, accuracy_decimals=3),
    CONF_CURRENT: sensor.sensor_schema(kind=sensor.CURRENT, accuracy_decimals=3),
    CONF_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE, accuracy_decimals=1
    ),
    CONF_TEMPERATURE_LOW: sensor.sensor_schema(
        kind=sensor.TEMPERATURE, accuracy_decimals=1
    ),
    CONF_TEMPERATURE_HIGH: sensor.sensor_schema(
        kind=sensor.TEMPERATURE, accuracy_decimals=1
    ),
    CONF_VOLTAGE_LOW: sensor.sensor_schema(kind=sensor.VOLTAGE, accuracy_decimals=3),
    CONF_VOLTAGE_HIGH: sensor.sensor_schema(kind=sensor.VOLTAGE, accuracy_decimals=3),
    CONF_COULOMB: sensor.sensor_schema(kind=sensor.BATTERY, accuracy_decimals=0),
    CONF_MOS_TEMPERATURE: sensor.sensor_schema(
        kind=sensor.TEMPERATURE, accuracy_decimals=1
    ),
}

CELL_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_CELL): cv.int_range(min=0, max=NUM_CELLS - 1),
            cv.Optional(CONF_VOLTAGE): TYPES[CONF_VOLTAGE],
            cv.Optional(CONF_TEMPERATURE): TYPES[CONF_TEMPERATURE],
        }
    ),
    cv.has_at_least_one_key(CONF_VOLTAGE, CONF_TEMPERATURE),
)


def _unique_cells(cells: list[ConfigType]) -> list[ConfigType]:
    seen: set[int] = set()
    for cell in cells:
        if (number := cell[CONF_CELL]) in seen:
            raise cv.Invalid(f"Cell {number} is listed more than once")
        seen.add(number)
    return cells


CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PylontechSensor),
        cv.Optional(CONF_CELLS): cv.All(
            cv.ensure_list(CELL_SCHEMA), cv.Length(min=1), _unique_cells
        ),
    }
).extend({cv.Optional(marker): schema for marker, schema in TYPES.items()})


async def to_code(config: ConfigType) -> None:
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    for marker in TYPES:
        if marker_config := config.get(marker):
            sens = await sensor.new_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_sensor")(sens))

    cells = config.get(CONF_CELLS, [])
    for cell_config in cells:
        _request_cell_slot(str(bat))
        voltage = cg.nullptr
        if voltage_config := cell_config.get(CONF_VOLTAGE):
            voltage = await sensor.new_sensor(voltage_config)
        temperature = cg.nullptr
        if temperature_config := cell_config.get(CONF_TEMPERATURE):
            temperature = await sensor.new_sensor(temperature_config)
        cg.add(bat.add_cell(cell_config[CONF_CELL], voltage, temperature))

    await register_listener(config, bat, cells=bool(cells))
