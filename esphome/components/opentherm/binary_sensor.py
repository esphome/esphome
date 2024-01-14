import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_EMPTY,
)
from . import OpenThermComponent, CONF_OPENTHERM_ID

CONF_CH_ACTIVE = "ch_active"
CONF_CH_2_ACTIVE = "ch_2_active"
CONF_DHW_ACTIVE = "dhw_active"
CONF_FLAME_ACTIVE = "flame_active"
CONF_COOLING_ACTIVE = "cooling_active"
CONF_FAULT = "fault"
CONF_DIAGNOSTIC = "diagnostic"
CONF_SERVICE_REQUEST = "service_request"
CONF_LOCKOUT_RESET = "lockout_reset"
CONF_WATER_PRESSURE_FAULT = "water_pressure_fault"
CONF_GAS_FLAME_FAULT = "gas_flame_fault"
CONF_AIR_PRESSURE_FAULT = "air_pressure_fault"
CONF_WATER_OVER_TEMPERATURE_FAULT = "water_over_temperature_fault"
CONF_DHW_PRESENT = "dhw_present"
CONF_MODULATING = "modulating"
CONF_COOLING_SUPPORTED = "cooling_supported"
CONF_DHW_STORAGE_TANK = "dhw_storage_tank"
CONF_DEVICE_LOWOFF_PUMP_CONTROL = "device_lowoff_pump_control"
CONF_CH_2_PRESENT = "ch_2_present"

TYPES = [
    CONF_CH_ACTIVE,
    CONF_CH_2_ACTIVE,
    CONF_DHW_ACTIVE,
    CONF_COOLING_ACTIVE,
    CONF_FLAME_ACTIVE,
    CONF_FAULT,
    CONF_DIAGNOSTIC,
    CONF_SERVICE_REQUEST,
    CONF_LOCKOUT_RESET,
    CONF_WATER_PRESSURE_FAULT,
    CONF_GAS_FLAME_FAULT,
    CONF_AIR_PRESSURE_FAULT,
    CONF_WATER_OVER_TEMPERATURE_FAULT,
    CONF_DHW_PRESENT,
    CONF_MODULATING,
    CONF_COOLING_SUPPORTED,
    CONF_DHW_STORAGE_TANK,
    CONF_DEVICE_LOWOFF_PUMP_CONTROL,
    CONF_CH_2_PRESENT,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenThermComponent),
            cv.Optional(CONF_CH_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_CH_2_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_DHW_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_COOLING_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_COLD,
            ),
            cv.Optional(CONF_FLAME_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_FAULT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_DIAGNOSTIC): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_SERVICE_REQUEST): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_LOCKOUT_RESET): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_WATER_PRESSURE_FAULT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_GAS_FLAME_FAULT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_AIR_PRESSURE_FAULT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(
                CONF_WATER_OVER_TEMPERATURE_FAULT
            ): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_DHW_PRESENT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_MODULATING): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_COOLING_SUPPORTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_DHW_STORAGE_TANK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(
                CONF_DEVICE_LOWOFF_PUMP_CONTROL
            ): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_CH_2_PRESENT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if conf := config.get(key):
        var = await binary_sensor.new_binary_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
