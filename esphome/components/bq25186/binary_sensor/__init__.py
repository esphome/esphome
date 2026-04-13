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

BQ25186StatusBinarySensor = bq25186_ns.class_(
    "BQ25186StatusBinarySensor", binary_sensor.BinarySensor
)
BQ25186ChargingActiveBinarySensor = bq25186_ns.class_(
    "BQ25186ChargingActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186ChargeDoneBinarySensor = bq25186_ns.class_(
    "BQ25186ChargeDoneBinarySensor", binary_sensor.BinarySensor
)
BQ25186VinPowerGoodBinarySensor = bq25186_ns.class_(
    "BQ25186VinPowerGoodBinarySensor", binary_sensor.BinarySensor
)
BQ25186IlimActiveBinarySensor = bq25186_ns.class_(
    "BQ25186IlimActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186VindpmActiveBinarySensor = bq25186_ns.class_(
    "BQ25186VindpmActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186VdppmActiveBinarySensor = bq25186_ns.class_(
    "BQ25186VdppmActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186ThermregActiveBinarySensor = bq25186_ns.class_(
    "BQ25186ThermregActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186VinOvpActiveBinarySensor = bq25186_ns.class_(
    "BQ25186VinOvpActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186BatteryUvloActiveBinarySensor = bq25186_ns.class_(
    "BQ25186BatteryUvloActiveBinarySensor", binary_sensor.BinarySensor
)
BQ25186SafetyTimerFaultBinarySensor = bq25186_ns.class_(
    "BQ25186SafetyTimerFaultBinarySensor", binary_sensor.BinarySensor
)
BQ25186Wake1FlagBinarySensor = bq25186_ns.class_(
    "BQ25186Wake1FlagBinarySensor", binary_sensor.BinarySensor
)
BQ25186Wake2FlagBinarySensor = bq25186_ns.class_(
    "BQ25186Wake2FlagBinarySensor", binary_sensor.BinarySensor
)
BQ25186TsOpenBinarySensor = bq25186_ns.class_(
    "BQ25186TsOpenBinarySensor", binary_sensor.BinarySensor
)
BQ25186TsFaultBinarySensor = bq25186_ns.class_(
    "BQ25186TsFaultBinarySensor", binary_sensor.BinarySensor
)
BQ25186BatteryOcpFaultBinarySensor = bq25186_ns.class_(
    "BQ25186BatteryOcpFaultBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component),
        cv.Optional(CONF_VIN_POWER_GOOD): binary_sensor.binary_sensor_schema(
            BQ25186VinPowerGoodBinarySensor,
            device_class=DEVICE_CLASS_CONNECTIVITY,
        ),
        cv.Optional(CONF_CHARGING_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186ChargingActiveBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGE_DONE): binary_sensor.binary_sensor_schema(
            BQ25186ChargeDoneBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_ILIM_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186IlimActiveBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_VINDPM_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186VindpmActiveBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_VDPPM_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186VdppmActiveBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_THERMREG_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186ThermregActiveBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_VIN_OVP_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186VinOvpActiveBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_BATTERY_UVLO_ACTIVE): binary_sensor.binary_sensor_schema(
            BQ25186BatteryUvloActiveBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_SAFETY_TIMER_FAULT): binary_sensor.binary_sensor_schema(
            BQ25186SafetyTimerFaultBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_WAKE1_FLAG): binary_sensor.binary_sensor_schema(
            BQ25186Wake1FlagBinarySensor
        ),
        cv.Optional(CONF_WAKE2_FLAG): binary_sensor.binary_sensor_schema(
            BQ25186Wake2FlagBinarySensor
        ),
        cv.Optional(CONF_TS_OPEN): binary_sensor.binary_sensor_schema(
            BQ25186TsOpenBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_TS_FAULT): binary_sensor.binary_sensor_schema(
            BQ25186TsFaultBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_BATTERY_OCP_FAULT): binary_sensor.binary_sensor_schema(
            BQ25186BatteryOcpFaultBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25186_ID])

    if conf := config.get(CONF_VIN_POWER_GOOD):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))

    if conf := config.get(CONF_CHARGING_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))

    if conf := config.get(CONF_CHARGE_DONE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))

    if conf := config.get(CONF_ILIM_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_VINDPM_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_VDPPM_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_THERMREG_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_VIN_OVP_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_BATTERY_UVLO_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_SAFETY_TIMER_FAULT):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_WAKE1_FLAG):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_WAKE2_FLAG):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_TS_OPEN):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_TS_FAULT):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
    if conf := config.get(CONF_BATTERY_OCP_FAULT):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(parent.add_listener(sens))
