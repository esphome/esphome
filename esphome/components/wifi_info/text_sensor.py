import esphome.codegen as cg
from esphome.components import text_sensor, wifi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BSSID,
    CONF_CHANNEL,
    CONF_DNS_ADDRESS,
    CONF_IP_ADDRESS,
    CONF_MAC_ADDRESS,
    CONF_POWER_SAVE_MODE,
    CONF_SCAN_RESULTS,
    CONF_SSID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

DEPENDENCIES = ["wifi"]

wifi_info_ns = cg.esphome_ns.namespace("wifi_info")
_request_ip_sensor_slot = cg.slot_counter("WIFI_INFO_IP_SENSOR_COUNT")
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
ChannelWiFiInfo = wifi_info_ns.class_(
    "ChannelWiFiInfo", text_sensor.TextSensor, cg.Component
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
        cv.Optional(CONF_CHANNEL): text_sensor.text_sensor_schema(
            ChannelWiFiInfo, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
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


async def setup_conf(config: ConfigType, key: str) -> None:
    if key in config:
        conf = config[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config: ConfigType) -> None:
    # Request specific WiFi listeners based on which sensors are configured
    # Each sensor needs its own listener slot - call request for EACH sensor

    # SSID, BSSID and channel use WiFiConnectStateListener
    for key in (CONF_SSID, CONF_BSSID, CONF_CHANNEL):
        if key in config:
            wifi.request_wifi_connect_state_listener()

    # IP address and DNS use WiFiIPStateListener
    for key in (CONF_IP_ADDRESS, CONF_DNS_ADDRESS):
        if key in config:
            wifi.request_wifi_ip_state_listener()

    # Scan results use WiFiScanResultsListener
    if CONF_SCAN_RESULTS in config:
        wifi.request_wifi_scan_results_listener()
        wifi.request_wifi_scan_results()

    # Power save mode uses WiFiPowerSaveListener
    if CONF_POWER_SAVE_MODE in config:
        wifi.request_wifi_power_save_listener()

    await setup_conf(config, CONF_SSID)
    await setup_conf(config, CONF_BSSID)
    await setup_conf(config, CONF_CHANNEL)
    await setup_conf(config, CONF_MAC_ADDRESS)
    await setup_conf(config, CONF_SCAN_RESULTS)
    await setup_conf(config, CONF_DNS_ADDRESS)
    await setup_conf(config, CONF_POWER_SAVE_MODE)
    if conf := config.get(CONF_IP_ADDRESS):
        wifi_info = await text_sensor.new_text_sensor(config[CONF_IP_ADDRESS])
        await cg.register_component(wifi_info, config[CONF_IP_ADDRESS])
        indexes = [x for x in range(5) if f"address_{x}" in conf]
        # One shared array size: the largest highest index of any entry, so add_ip_sensors(x) stays in range
        for _ in range(max(indexes, default=-1) + 1):
            _request_ip_sensor_slot(str(wifi_info))
        for x in indexes:
            sens = await text_sensor.new_text_sensor(conf[f"address_{x}"])
            cg.add(wifi_info.add_ip_sensors(x, sens))
