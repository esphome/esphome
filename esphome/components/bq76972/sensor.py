import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_BQ76972_ID, BQ76972Component

DEPENDENCIES = ["bq76972"]

CONF_STACK_VOLTAGE = "stack_voltage"

# Ordered as the chip reports them, starting at command 0x68
TEMPERATURE_KEYS = [
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

CELL_VOLTAGE_KEYS = [f"cell_{i}_voltage" for i in range(1, 17)]

voltage_sensor_schema = sensor.sensor_schema(kind=sensor.VOLTAGE, accuracy_decimals=3)

temp_sensor_schema = sensor.sensor_schema(kind=sensor.TEMPERATURE)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ76972_ID): cv.use_id(BQ76972Component),
        cv.Optional(CONF_STACK_VOLTAGE): voltage_sensor_schema,
        **{cv.Optional(key): voltage_sensor_schema for key in CELL_VOLTAGE_KEYS},
        **{cv.Optional(key): temp_sensor_schema for key in TEMPERATURE_KEYS},
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ76972_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_STACK_VOLTAGE, hub.set_stack_voltage_sensor)

    for idx, key in enumerate(TEMPERATURE_KEYS):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_temperature_sensor(sens, idx))

    for idx, key in enumerate(CELL_VOLTAGE_KEYS):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_cell_sensor(sens, idx))
