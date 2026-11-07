import esphome.codegen as cg
from esphome.components import ble_client, sensor
from esphome.components.const import CONF_OPERATING_HOURS
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_FLOW,
    CONF_HEAD,
    CONF_ID,
    CONF_POWER,
    CONF_SPEED,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_HOUR,
    UNIT_KILOWATT_HOURS,
    UNIT_METER,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.types import ConfigType

from . import Alpha3
from .const import CONF_ALARM_CODE, CONF_STARTS, CONF_WARNING_CODE

AUTO_LOAD = ["alpha3"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Alpha3),
            cv.Optional(CONF_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HEAD): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OPERATING_HOURS): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_STARTS): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_ALARM_CODE): sensor.sensor_schema(
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_WARNING_CODE): sensor.sensor_schema(
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("15s"))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_FLOW, var.set_flow_sensor)
    await sensors(CONF_HEAD, var.set_head_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_SPEED, var.set_speed_sensor)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_OPERATING_HOURS, var.set_operating_hours_sensor)
    await sensors(CONF_ENERGY, var.set_energy_sensor)
    await sensors(CONF_STARTS, var.set_starts_sensor)
    await sensors(CONF_ALARM_CODE, var.set_alarm_code_sensor)
    await sensors(CONF_WARNING_CODE, var.set_warning_code_sensor)
