import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    DEVICE_CLASS_SAFETY,
)

from .. import CONF_BQ25186_ID, BQ25186Component, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_TS = "ts"
CONF_CHARGING = "charging"
CONF_CHARGE_DONE = "charge_done"
CONF_ILIM = "ilim"
CONF_VDPPM = "vdppm"
CONF_VINDPM = "vindpm"
CONF_THERMREG = "thermreg"
CONF_POWER_GOOD = "power_good"
CONF_VIN_OVP = "vin_ovp"
CONF_BATTERY_UVLO = "battery_uvlo"
CONF_SAFETY_TIMER_FAULT = "safety_timer_fault"
CONF_WAKE1_FLAG = "wake1_flag"
CONF_WAKE2_FLAG = "wake2_flag"
CONF_TS_FAULT = "ts_fault"
CONF_ILIM_FAULT = "ilim_fault"
CONF_VDPPM_FAULT = "vdppm_fault"
CONF_VINDPM_FAULT = "vindpm_fault"
CONF_THERMREG_ACTIVE = "thermreg_active"
CONF_POWER_GOOD_FAULT = "power_good_fault"
CONF_VIN_OVP_FAULT = "vin_ovp_fault"
CONF_BATTERY_UVLO_FAULT = "battery_uvlo_fault"
CONF_BATTERY_OCP_FAULT = "battery_ocp_fault"

_BINARY_SENSOR_SPECS = (
    # STAT0
    (CONF_TS, "BQ25186TsOpenBinarySensor", DEVICE_CLASS_OPENING),
    (CONF_CHARGING, "BQ25186ChargingBinarySensor", DEVICE_CLASS_BATTERY_CHARGING),
    (CONF_CHARGE_DONE, "BQ25186ChargeDoneBinarySensor", None),
    (CONF_ILIM, "BQ25186IlimStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_VDPPM, "BQ25186VdppmStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_VINDPM, "BQ25186VindpmStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_THERMREG, "BQ25186ThermregStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_POWER_GOOD, "BQ25186PowerGoodBinarySensor", DEVICE_CLASS_PLUG),
    # STAT1
    (CONF_VIN_OVP, "BQ25186VinOvpStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_BATTERY_UVLO, "BQ25186BatteryUvloStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (
        CONF_SAFETY_TIMER_FAULT,
        "BQ25186SafetyTimerFaultBinarySensor",
        DEVICE_CLASS_PROBLEM,
    ),
    (CONF_WAKE1_FLAG, "BQ25186Wake1FlagBinarySensor", None),
    (CONF_WAKE2_FLAG, "BQ25186Wake2FlagBinarySensor", None),
    # FLAG0
    (CONF_TS_FAULT, "BQ25186TsFaultBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_ILIM_FAULT, "BQ25186IlimFaultBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_VDPPM_FAULT, "BQ25186VdppmFaultBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_VINDPM_FAULT, "BQ25186VindpmFaultBinarySensor", DEVICE_CLASS_PROBLEM),
    (CONF_THERMREG_ACTIVE, "BQ25186ThermregActiveBinarySensor", DEVICE_CLASS_SAFETY),
    (CONF_VIN_OVP_FAULT, "BQ25186VinOvpFaultBinarySensor", DEVICE_CLASS_PROBLEM),
    (
        CONF_BATTERY_UVLO_FAULT,
        "BQ25186BatteryUvloActiveFaultBinarySensor",
        DEVICE_CLASS_PROBLEM,
    ),
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
