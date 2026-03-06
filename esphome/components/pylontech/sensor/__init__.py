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

NUM_CELLS = 15

# Sensors from the "pwr" command
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

# Cell-level sensors from the "bat N" command (cell_0_voltage .. cell_14_voltage, etc.)
CELL_TYPES: dict[str, cv.Schema] = {}
for _i in range(NUM_CELLS):
    CELL_TYPES[f"cell_{_i}_voltage"] = sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
    )
    CELL_TYPES[f"cell_{_i}_temperature"] = sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )

ALL_TYPES = {**TYPES, **CELL_TYPES}

CONFIG_SCHEMA = PYLONTECH_COMPONENT_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(PylontechSensor)}
).extend({cv.Optional(marker): schema for marker, schema in ALL_TYPES.items()})


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PYLONTECH_ID])
    bat = cg.new_Pvariable(config[CONF_ID], config[CONF_BATTERY])

    # Register pwr sensors
    for marker in TYPES:
        if marker_config := config.get(marker):
            sens = await sensor.new_sensor(marker_config)
            cg.add(getattr(bat, f"set_{marker}_sensor")(sens))

    # Register cell sensors and request cell data from the parent component
    has_cells = False
    for i in range(NUM_CELLS):
        v_key = f"cell_{i}_voltage"
        t_key = f"cell_{i}_temperature"
        if v_config := config.get(v_key):
            sens = await sensor.new_sensor(v_config)
            cg.add(bat.set_cell_voltage_sensor(i, sens))
            has_cells = True
        if t_config := config.get(t_key):
            sens = await sensor.new_sensor(t_config)
            cg.add(bat.set_cell_temperature_sensor(i, sens))
            has_cells = True

    if has_cells:
        cg.add(paren.request_cell_data(config[CONF_BATTERY]))

    cg.add(paren.register_listener(bat))
