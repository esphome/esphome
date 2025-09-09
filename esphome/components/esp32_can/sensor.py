import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.canbus import CONF_CANBUS_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    ICON_BUG,
    ICON_CHIP,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
)

from . import esp32_can

DEPENDENCIES = ["canbus"]

CANBUS_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(esp32_can),
    }
)


CONFIG_SCHEMA = cv.typed_schema(
    {
        "msgs_to_tx": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_CHIP,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "msgs_to_rx": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_CHIP,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "tx_error_counter": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "rx_error_counter": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "tx_failed_count": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "rx_missed_count": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "rx_overrun_count": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "arb_lost_count": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
        "bus_error_count": sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            icon=ICON_BUG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(CANBUS_SENSOR_SCHEMA),
    },
    lower=True,
)


async def to_code(config):
    esp32can = await cg.get_variable(config[CONF_CANBUS_ID])
    var = await sensor.new_sensor(config)
    func = getattr(esp32can, f"set_{config[CONF_TYPE]}_sensor")
    cg.add(func(var))
