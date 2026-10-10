from ipaddress import IPv4Address

import esphome.codegen as cg
from esphome.components import esp32, wifi
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
from esphome.components.wifi import DOMAIN as WIFI_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

DOMAIN = "espectre"
CODEOWNERS = ["@francescopace"]
DEPENDENCIES = ["esp32", "wifi"]

CONF_ESPECTRE_ID = "espectre_id"
CONF_DETECTION_ALGORITHM = "detection_algorithm"
CONF_CSI_CAPTURE_PROFILE = "csi_capture_profile"
CONF_TRAFFIC_GENERATOR_MODE = "traffic_generator_mode"
CONF_TRAFFIC_GENERATOR_TARGET_IP = "traffic_generator_target_ip"
CONF_CSI_TRAFFIC_MULTICAST_GROUP = "csi_traffic_multicast_group"
CONF_MOTION_ON_HITS = "motion_on_hits"
CONF_MOTION_OFF_HITS = "motion_off_hits"

# Fully qualified: a bare espectre:: in main.cpp would clash with the SDK namespace.
espectre_ns = cg.global_ns.namespace("esphome").namespace("espectre")
ESPectreComponent = espectre_ns.class_("ESPectreComponent", cg.Component)
sdk_ns = cg.global_ns.namespace("::espectre")
DetectionAlgorithm = sdk_ns.enum("DetectionAlgorithm", is_class=True)
CsiCapturePolicy = sdk_ns.enum("CsiCapturePolicy", is_class=True)
TrafficGeneratorMode = sdk_ns.enum("TrafficGeneratorMode", is_class=True)
WifiBandPolicy = sdk_ns.enum("WifiBandPolicy", is_class=True)

DETECTION_ALGORITHMS = {
    "lightweight": DetectionAlgorithm.LIGHTWEIGHT,
    "high_accuracy": DetectionAlgorithm.HIGH_ACCURACY,
}
CSI_CAPTURE_PROFILES = {
    "auto": CsiCapturePolicy.AUTO,
    "lltf": CsiCapturePolicy.LLTF,
    "ht_vht": CsiCapturePolicy.HT_VHT,
}
TRAFFIC_GENERATOR_MODES = {
    "ping": TrafficGeneratorMode.PING,
    "dns": TrafficGeneratorMode.DNS,
    "dns_tcp": TrafficGeneratorMode.DNS_TCP,
    "wifi_raw": TrafficGeneratorMode.WIFI_RAW,
    "external": TrafficGeneratorMode.EXTERNAL_HOST,
}


def validate_target_ip(value: str) -> str:
    value = str(cv.ipv4address(value))
    first_octet = int(IPv4Address(value)) >> 24
    if first_octet in (0, 127) or first_octet >= 224 or value == "255.255.255.255":
        raise cv.Invalid("ESPectre traffic target must be a unicast IPv4 address")
    return value


def validate_multicast_group(value: str) -> str:
    """An IPv4 multicast group, or an empty string to skip joining one."""
    if not (value := cv.string_strict(value).strip()):
        return value
    value = str(cv.ipv4address(value))
    if not IPv4Address(value).is_multicast:
        raise cv.Invalid("ESPectre multicast group must be an IPv4 multicast address")
    return value


def supported_traffic_generator_modes(config: ConfigType) -> list[str]:
    """Traffic generator modes available with this chip and CSI capture profile."""
    wifi_raw = (
        esp32.get_esp32_variant() != VARIANT_ESP32C6
        and config[CONF_CSI_CAPTURE_PROFILE] != "ht_vht"
    )
    return [mode for mode in TRAFFIC_GENERATOR_MODES if wifi_raw or mode != "wifi_raw"]


def validate_config(config: ConfigType) -> ConfigType:
    mode = config[CONF_TRAFFIC_GENERATOR_MODE]
    if mode not in supported_traffic_generator_modes(config):
        raise cv.Invalid(
            "wifi_raw traffic is not supported on ESP32-C6 "
            "or with the ht_vht CSI capture profile"
        )
    if mode in ("wifi_raw", "external") and CONF_TRAFFIC_GENERATOR_TARGET_IP in config:
        raise cv.Invalid(f"{mode} traffic does not use a target IP address")
    if mode != "external" and CONF_CSI_TRAFFIC_MULTICAST_GROUP in config:
        raise cv.Invalid(
            f"{CONF_CSI_TRAFFIC_MULTICAST_GROUP} requires "
            f"{CONF_TRAFFIC_GENERATOR_MODE}: external"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPectreComponent),
            cv.Optional(CONF_DETECTION_ALGORITHM, default="lightweight"): cv.enum(
                DETECTION_ALGORITHMS, lower=True
            ),
            cv.Optional(CONF_CSI_CAPTURE_PROFILE, default="auto"): cv.enum(
                CSI_CAPTURE_PROFILES, lower=True
            ),
            cv.Optional(CONF_TRAFFIC_GENERATOR_MODE, default="ping"): cv.enum(
                TRAFFIC_GENERATOR_MODES, lower=True
            ),
            cv.Optional(CONF_TRAFFIC_GENERATOR_TARGET_IP): validate_target_ip,
            cv.Optional(CONF_CSI_TRAFFIC_MULTICAST_GROUP): validate_multicast_group,
            cv.Optional(CONF_MOTION_ON_HITS): cv.int_range(min=1, max=20),
            cv.Optional(CONF_MOTION_OFF_HITS): cv.int_range(min=1, max=20),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=[
            VARIANT_ESP32,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
            VARIANT_ESP32C3,
            VARIANT_ESP32C5,
            VARIANT_ESP32C6,
        ],
        msg_prefix="ESPectre",
    ),
    # Arduino 3.3.7 is the first release built on ESP-IDF 5.5.3.
    cv.require_framework_version(
        esp_idf=cv.Version(5, 5, 3), esp32_arduino=cv.Version(3, 3, 7)
    ),
    validate_config,
)


def final_validate(config: ConfigType) -> None:
    wifi.force_power_save_off(
        "ESPectre needs the radio awake to receive a steady flow of CSI packets"
    )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_ESPECTRE")
    cg.add(var.set_detection_algorithm(config[CONF_DETECTION_ALGORITHM]))
    cg.add(var.set_csi_capture_profile(config[CONF_CSI_CAPTURE_PROFILE]))
    cg.add(var.set_traffic_generator_mode(config[CONF_TRAFFIC_GENERATOR_MODE]))
    if (target_ip := config.get(CONF_TRAFFIC_GENERATOR_TARGET_IP)) is not None:
        cg.add(var.set_traffic_generator_target_ip(target_ip))
    if (group := config.get(CONF_CSI_TRAFFIC_MULTICAST_GROUP)) is not None:
        cg.add(var.set_csi_traffic_multicast_group(group))
    if (on_hits := config.get(CONF_MOTION_ON_HITS)) is not None:
        cg.add(var.set_motion_on_hits(on_hits))
    if (off_hits := config.get(CONF_MOTION_OFF_HITS)) is not None:
        cg.add(var.set_motion_off_hits(off_hits))
    if esp32.get_esp32_variant() == VARIANT_ESP32C5:
        band = CORE.config[WIFI_DOMAIN].get(wifi.CONF_BAND_MODE, "AUTO")
        cg.add(
            var.set_wifi_band_policy(
                {
                    "2.4GHZ": WifiBandPolicy.BAND_2G,
                    "5GHZ": WifiBandPolicy.BAND_5G,
                    "AUTO": WifiBandPolicy.AUTO,
                }[band]
            )
        )

    wifi.enable_runtime_roaming_suppression()
    esp32.add_idf_component(name="francescopace/espectre", ref="3.0.0")
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_CSI_ENABLED", True)
    # CSI is reported once per received transmission, so aggregation hides frames from sensing.
    # Disabling TX aggregation also lets the SDK fix the station TX rate (6.5 Mbps on ESP32).
    # Both may lower Wi-Fi throughput for the whole firmware.
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_TX_ENABLED", False)
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_RX_ENABLED", False)
    # Keep the radio awake while disconnected too, matching force_power_save_off().
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE", False)
