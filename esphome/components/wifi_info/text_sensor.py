import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_BSSID,
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_IP_ADDRESS,
    CONF_SCAN_RESULTS,
    CONF_SSID,
    CONF_MAC_ADDRESS,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["wifi"]

wifi_info_ns = cg.esphome_ns.namespace("wifi_info")
IPAddressWiFiInfo = wifi_info_ns.class_(
    "IPAddressWiFiInfo", text_sensor.TextSensor, cg.Component
)
ScanResultsWiFiInfo = wifi_info_ns.class_(
    "ScanResultsWiFiInfo", text_sensor.TextSensor, cg.PollingComponent
)
SSIDWiFiInfo = wifi_info_ns.class_("SSIDWiFiInfo", text_sensor.TextSensor, cg.Component)
BSSIDWiFiInfo = wifi_info_ns.class_(
    "BSSIDWiFiInfo", text_sensor.TextSensor, cg.Component
)
MacAddressWifiInfo = wifi_info_ns.class_(
    "MacAddressWifiInfo", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IP_ADDRESS): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(IPAddressWiFiInfo),
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
                ): cv.entity_category,
            }
        ),
        cv.Optional(CONF_SCAN_RESULTS): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(ScanResultsWiFiInfo),
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
                ): cv.entity_category,
            }
        ).extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_SSID): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(SSIDWiFiInfo),
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
                ): cv.entity_category,
            }
        ),
        cv.Optional(CONF_BSSID): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(BSSIDWiFiInfo),
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
                ): cv.entity_category,
            }
        ),
        cv.Optional(CONF_MAC_ADDRESS): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(MacAddressWifiInfo),
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
                ): cv.entity_category,
            }
        ),
    }
)


async def setup_conf(config, key):
    if key in config:
        conf = config[key]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await text_sensor.register_text_sensor(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_IP_ADDRESS)
    await setup_conf(config, CONF_SSID)
    await setup_conf(config, CONF_BSSID)
    await setup_conf(config, CONF_MAC_ADDRESS)
    await setup_conf(config, CONF_SCAN_RESULTS)
