import esphome.codegen as cg
from esphome.components import text_sensor, wifi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BSSID,
    CONF_DNS_ADDRESS,
    CONF_IP_ADDRESS,
    CONF_MAC_ADDRESS,
    CONF_POWER_SAVE_MODE,
    CONF_SCAN_RESULTS,
    CONF_SSID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["wifi"]

wifi_info_ns = cg.esphome_ns.namespace("wifi_info")
IPAddressWiFiInfo = wifi_info_ns.class_(
    "IPAddressWiFiInfo", text_sensor.TextSensor, cg.Component
)
ScanResultsWiFiInfo = wifi_info_ns.class_(
    "ScanResultsWiFiInfo", text_sensor.TextSensor, cg.Component
)
SSIDWiFiInfo = wifi_info_ns.class_("SSIDWiFiInfo", text_sensor.TextSensor, cg.Component)
BSSIDWiFiInfo = wifi_info_ns.class_(
    "BSSIDWiFiInfo", text_sensor.TextSensor, cg.Component
)
MacAddressWifiInfo = wifi_info_ns.class_(
    "MacAddressWifiInfo", text_sensor.TextSensor, cg.Component
)
DNSAddressWifiInfo = wifi_info_ns.class_(
    "DNSAddressWifiInfo", text_sensor.TextSensor, cg.Component
)
PowerSaveModeWiFiInfo = wifi_info_ns.class_(
    "PowerSaveModeWiFiInfo", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IP_ADDRESS): text_sensor.text_sensor_schema(
            IPAddressWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ).extend(
            {
                cv.Optional(f"address_{x}"): text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                )
                for x in range(5)
            }
        ),
        cv.Optional(CONF_SCAN_RESULTS): text_sensor.text_sensor_schema(
            ScanResultsWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_SSID): text_sensor.text_sensor_schema(
            SSIDWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_BSSID): text_sensor.text_sensor_schema(
            BSSIDWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
            MacAddressWifiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_DNS_ADDRESS): text_sensor.text_sensor_schema(
            DNSAddressWifiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_POWER_SAVE_MODE): text_sensor.text_sensor_schema(
            PowerSaveModeWiFiInfo,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

# Keys that require WiFi listeners
_NETWORK_INFO_KEYS = {
    CONF_SSID,
    CONF_BSSID,
    CONF_IP_ADDRESS,
    CONF_DNS_ADDRESS,
    CONF_SCAN_RESULTS,
    CONF_POWER_SAVE_MODE,
}


async def setup_conf(config, key):
    if key in config:
        conf = config[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    # Request WiFi listeners for any sensor that needs them
    if _NETWORK_INFO_KEYS.intersection(config):
        wifi.request_wifi_listeners()

    await setup_conf(config, CONF_SSID)
    await setup_conf(config, CONF_BSSID)
    await setup_conf(config, CONF_MAC_ADDRESS)
    if CONF_SCAN_RESULTS in config:
        await setup_conf(config, CONF_SCAN_RESULTS)
        wifi.request_wifi_scan_results()
    await setup_conf(config, CONF_DNS_ADDRESS)
    await setup_conf(config, CONF_POWER_SAVE_MODE)
    if conf := config.get(CONF_IP_ADDRESS):
        wifi_info = await text_sensor.new_text_sensor(config[CONF_IP_ADDRESS])
        await cg.register_component(wifi_info, config[CONF_IP_ADDRESS])
        for x in range(5):
            if sensor_conf := conf.get(f"address_{x}"):
                sens = await text_sensor.new_text_sensor(sensor_conf)
                cg.add(wifi_info.add_ip_sensors(x, sens))
