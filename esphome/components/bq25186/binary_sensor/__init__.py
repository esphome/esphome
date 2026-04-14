import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_RUNNING,
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
CONF_SAFETY_TIMER_FLAG = "safety_timer_flag"
CONF_WAKE1_FLAG = "wake1_flag"
CONF_WAKE2_FLAG = "wake2_flag"
CONF_TS_FLAG = "ts_flag"
CONF_ILIM_FLAG = "ilim_flag"
CONF_VDPPM_FLAG = "vdppm_flag"
CONF_VINDPM_FLAG = "vindpm_flag"
CONF_THERMREG_ACTIVE = "thermreg_active"
CONF_VIN_OVP_FLAG = "vin_ovp_flag"
CONF_BATTERY_UVLO_FLAG = "battery_uvlo_flag"
CONF_BATTERY_OCP_FLAG = "battery_ocp_flag"

_BINARY_SENSOR_SPECS = (
    # STAT0
    (CONF_TS, "BQ25186TsOpenBinarySensor", DEVICE_CLASS_OPENING),
    (CONF_CHARGING, "BQ25186ChargingBinarySensor", DEVICE_CLASS_BATTERY_CHARGING),
    (CONF_CHARGE_DONE, "BQ25186ChargeDoneBinarySensor", DEVICE_CLASS_BATTERY),
    (CONF_ILIM, "BQ25186IlimStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_VDPPM, "BQ25186VdppmStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_VINDPM, "BQ25186VindpmStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_THERMREG, "BQ25186ThermregStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_POWER_GOOD, "BQ25186PowerGoodBinarySensor", DEVICE_CLASS_PLUG),
    # STAT1
    (CONF_VIN_OVP, "BQ25186VinOvpStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_BATTERY_UVLO, "BQ25186BatteryUvloStatusBinarySensor", DEVICE_CLASS_RUNNING),
    (CONF_SAFETY_TIMER_FLAG, "BQ25186SafetyTimerFlagBinarySensor", None),
    (CONF_WAKE1_FLAG, "BQ25186Wake1FlagBinarySensor", None),
    (CONF_WAKE2_FLAG, "BQ25186Wake2FlagBinarySensor", None),
    # FLAG0
    (CONF_TS_FLAG, "BQ25186TsFlagBinarySensor", None),
    (CONF_ILIM_FLAG, "BQ25186IlimFlagBinarySensor", None),
    (CONF_VDPPM_FLAG, "BQ25186VdppmFlagBinarySensor", None),
    (CONF_VINDPM_FLAG, "BQ25186VindpmFlagBinarySensor", None),
    (CONF_THERMREG_ACTIVE, "BQ25186ThermregActiveBinarySensor", None),
    (CONF_VIN_OVP_FLAG, "BQ25186VinOvpFlagBinarySensor", None),
    (CONF_BATTERY_UVLO_FLAG, "BQ25186BatteryUvloActiveFlagBinarySensor", None),
    (CONF_BATTERY_OCP_FLAG, "BQ25186BatteryOcpFlagBinarySensor", None),
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
