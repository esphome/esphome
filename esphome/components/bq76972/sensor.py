import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from . import CONF_BQ76972_ID, BQ76972Component

DEPENDENCIES = ["bq76972"]

CONF_STACK_VOLTAGE = "stack_voltage"

# Ordered as the chip reports them, starting at command 0x68
CONF_TEMPERATURES = [
    "internal_temperature",
    "cfetoff_temperature",
    "dfetoff_temperature",
    "alert_temperature",
    "ts1_temperature",
    "ts2_temperature",
    "ts3_temperature",
    "hdq_temperature",
    "dchg_temperature",
    "ddsg_temperature",
]

CONF_CELL_VOLTAGES = [f"cell_{i}_voltage" for i in range(1, 17)]

voltage_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)

temp_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ76972_ID): cv.use_id(BQ76972Component),
        cv.Optional(CONF_STACK_VOLTAGE): voltage_sensor_schema,
        **{cv.Optional(key): voltage_sensor_schema for key in CONF_CELL_VOLTAGES},
        **{cv.Optional(key): temp_sensor_schema for key in CONF_TEMPERATURES},
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ76972_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_STACK_VOLTAGE, hub.set_stack_voltage_sensor)

    for idx, key in enumerate(CONF_TEMPERATURES):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_temperature_sensor(sens, idx))

    for idx, key in enumerate(CONF_CELL_VOLTAGES):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_cell_sensor(sens, idx))
