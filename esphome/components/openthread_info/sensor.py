import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_EMPTY,
)

CONF_PARENT_AVERAGE_RSSI = "parent_average_rssi"
CONF_PARENT_LAST_RSSI = "parent_last_rssi"
CONF_PARENT_LINK_QUALITY_IN = "parent_link_quality_in"
CONF_PARENT_LINK_QUALITY_OUT = "parent_link_quality_out"

DEPENDENCIES = ["openthread"]

openthread_info_ns = cg.esphome_ns.namespace("openthread_info")
ParentAverageRssiOpenThreadInfo = openthread_info_ns.class_(
    "ParentAverageRssiOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentLastRssiOpenThreadInfo = openthread_info_ns.class_(
    "ParentLastRssiOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentLinkQualityInOpenThreadInfo = openthread_info_ns.class_(
    "ParentLinkQualityInOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentLinkQualityOutOpenThreadInfo = openthread_info_ns.class_(
    "ParentLinkQualityOutOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PARENT_AVERAGE_RSSI): sensor.sensor_schema(
            ParentAverageRssiOpenThreadInfo,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_PARENT_LAST_RSSI): sensor.sensor_schema(
            ParentLastRssiOpenThreadInfo,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_PARENT_LINK_QUALITY_IN): sensor.sensor_schema(
            ParentLinkQualityInOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_PARENT_LINK_QUALITY_OUT): sensor.sensor_schema(
            ParentLinkQualityOutOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
    }
)


async def setup_conf(config: dict, key: str):
    if conf := config.get(key):
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_PARENT_AVERAGE_RSSI)
    await setup_conf(config, CONF_PARENT_LAST_RSSI)
    await setup_conf(config, CONF_PARENT_LINK_QUALITY_IN)
    await setup_conf(config, CONF_PARENT_LINK_QUALITY_OUT)
