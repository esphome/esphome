import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_BSSID,
    CONF_IP_ADDRESS,
    CONF_SCAN_RESULTS,
    CONF_SSID,
    CONF_MAC_ADDRESS,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["wifi"]

wifi_info_ns = cg.esphome_ns.namespace("wifi_info")
IPAddressWiFiInfo = wifi_info_ns.class_(
    "IPAddressWiFiInfo", text_sensor.TextSensor, cg.PollingComponent
)
ScanResultsWiFiInfo = wifi_info_ns.class_(
    "ScanResultsWiFiInfo", text_sensor.TextSensor, cg.PollingComponent
)
SSIDWiFiInfo = wifi_info_ns.class_(
    "SSIDWiFiInfo", text_sensor.TextSensor, cg.PollingComponent
)
BSSIDWiFiInfo = wifi_info_ns.class_(
    "BSSIDWiFiInfo", text_sensor.TextSensor, cg.PollingComponent
)
MacAddressWifiInfo = wifi_info_ns.class_(
    "MacAddressWifiInfo", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IP_ADDRESS): text_sensor.text_sensor_schema(
            IPAddressWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_SCAN_RESULTS): text_sensor.text_sensor_schema(
            ScanResultsWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_SSID): text_sensor.text_sensor_schema(
            SSIDWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_BSSID): text_sensor.text_sensor_schema(
            BSSIDWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(cv.polling_component_schema("1s")),
        cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
            MacAddressWifiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


async def setup_conf(config, key):
    if key in config:
        conf = config[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_IP_ADDRESS)
    await setup_conf(config, CONF_SSID)
    await setup_conf(config, CONF_BSSID)
    await setup_conf(config, CONF_MAC_ADDRESS)
    await setup_conf(config, CONF_SCAN_RESULTS)
