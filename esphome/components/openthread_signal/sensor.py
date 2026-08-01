import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

CONF_PARENT_LQI_IN = "parent_lqi_in"
CONF_PARENT_LQI_OUT = "parent_lqi_out"
CONF_PARENT_PATH_COST = "parent_path_cost"
CONF_RSSI = "rssi"

DEPENDENCIES = ["openthread"]

openthread_signal_ns = cg.esphome_ns.namespace("openthread_signal")
OpenThreadParentLqiInSensor = openthread_signal_ns.class_(
    "OpenThreadParentLqiInSensor", sensor.Sensor, cg.PollingComponent
)
OpenThreadParentLqiOutSensor = openthread_signal_ns.class_(
    "OpenThreadParentLqiOutSensor", sensor.Sensor, cg.PollingComponent
)
OpenThreadParentPathCostSensor = openthread_signal_ns.class_(
    "OpenThreadParentPathCostSensor", sensor.Sensor, cg.PollingComponent
)
OpenThreadRssiSensor = openthread_signal_ns.class_(
    "OpenThreadRssiSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PARENT_LQI_IN): sensor.sensor_schema(
            OpenThreadParentLqiInSensor,
            unit_of_measurement=UNIT_LQI,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_PARENT_LQI_OUT): sensor.sensor_schema(
            OpenThreadParentLqiOutSensor,
            unit_of_measurement=UNIT_LQI,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_PARENT_PATH_COST): sensor.sensor_schema(
            OpenThreadParentPathCostSensor,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            OpenThreadRssiSensor,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
    }
)


async def setup_conf(config: dict, key: str):
    if conf := config.get(key):
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_PARENT_LQI_IN)
    await setup_conf(config, CONF_PARENT_LQI_OUT)
    await setup_conf(config, CONF_PARENT_PATH_COST)
    await setup_conf(config, CONF_RSSI)
