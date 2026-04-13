import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
)

from .. import CONF_BQ25186_ID, BQ25186Component, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_VIN_POWER_GOOD = "vin_power_good"
CONF_CHARGING_ACTIVE = "charging_active"
CONF_CHARGE_DONE = "charge_done"
CONF_ILIM_ACTIVE = "ilim_active"
CONF_VINDPM_ACTIVE = "vindpm_active"
CONF_VDPPM_ACTIVE = "vdppm_active"
CONF_THERMREG_ACTIVE = "thermreg_active"
CONF_VIN_OVP_ACTIVE = "vin_ovp_active"
CONF_BATTERY_UVLO_ACTIVE = "battery_uvlo_active"
CONF_SAFETY_TIMER_FAULT = "safety_timer_fault"
CONF_WAKE1_FLAG = "wake1_flag"
CONF_WAKE2_FLAG = "wake2_flag"
CONF_TS_OPEN = "ts_open"
CONF_TS_FAULT = "ts_fault"
CONF_BATTERY_OCP_FAULT = "battery_ocp_fault"

_BINARY_SENSOR_SPECS = (
    (CONF_VIN_POWER_GOOD, "BQ25186VinPowerGoodBinarySensor", DEVICE_CLASS_CONNECTIVITY),
    (CONF_CHARGING_ACTIVE, "BQ25186ChargingActiveBinarySensor", DEVICE_CLASS_POWER),
    (CONF_CHARGE_DONE, "BQ25186ChargeDoneBinarySensor", DEVICE_CLASS_POWER),
    (CONF_ILIM_ACTIVE, "BQ25186IlimActiveBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_VINDPM_ACTIVE, "BQ25186VindpmActiveBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_VDPPM_ACTIVE, "BQ25186VdppmActiveBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_THERMREG_ACTIVE, "BQ25186ThermregActiveBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_VIN_OVP_ACTIVE, "BQ25186VinOvpActiveBinarySensor", DEVICE_CLASS_PROBLEM),
    (
        CONF_BATTERY_UVLO_ACTIVE,
        "BQ25186BatteryUvloActiveBinarySensor",
        DEVICE_CLASS_PROBLEM,
    ),
    (
        CONF_SAFETY_TIMER_FAULT,
        "BQ25186SafetyTimerFaultBinarySensor",
        DEVICE_CLASS_PROBLEM,
    ),
    (CONF_WAKE1_FLAG, "BQ25186Wake1FlagBinarySensor", None),
    (CONF_WAKE2_FLAG, "BQ25186Wake2FlagBinarySensor", None),
    (CONF_TS_OPEN, "BQ25186TsOpenBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_TS_FAULT, "BQ25186TsFaultBinarySensor", DEVICE_CLASS_PROBLEM),
    (
        CONF_BATTERY_OCP_FAULT,
        "BQ25186BatteryOcpFaultBinarySensor",
        DEVICE_CLASS_PROBLEM,
    ),
)

_BINARY_SENSOR_TYPES = {
    conf_key: bq25186_ns.class_(class_name, binary_sensor.BinarySensor)
    for conf_key, class_name, _ in _BINARY_SENSOR_SPECS
}


def _binary_sensor_schema(sensor_type, device_class):
    if device_class is None:
        return binary_sensor.binary_sensor_schema(sensor_type)
    return binary_sensor.binary_sensor_schema(sensor_type, device_class=device_class)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component),
        **{
            cv.Optional(conf_key): _binary_sensor_schema(
                _BINARY_SENSOR_TYPES[conf_key], device_class
            )
            for conf_key, _, device_class in _BINARY_SENSOR_SPECS
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25186_ID])

    for conf_key, _, _ in _BINARY_SENSOR_SPECS:
        if conf := config.get(conf_key):
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(parent.add_listener(sens))
