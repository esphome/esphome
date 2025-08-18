from __future__ import annotations

from esphome import helpers
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENABLE_IPV6,
    CONF_HOSTS,
    CONF_HOSTSFILE,
    CONF_IP_ADDRESS,
    CONF_MIN_IPV6_ADDR_COUNT,
    CONF_NAME,
)
from esphome.core import CORE, coroutine

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

network_ns = cg.esphome_ns.namespace("network")
IPAddress = network_ns.class_("IPAddress")
Resolver = network_ns.class_("Resolver")
CONF_NETWORK_ID = "network_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(
            CONF_ENABLE_IPV6,
            esp8266=False,
            esp32=False,
            rp2040=False,
            bk72xx=False,
        ): cv.All(
            cv.boolean,
            cv.Any(
                cv.require_framework_version(
                    esp_idf=cv.Version(0, 0, 0),
                    esp32_arduino=cv.Version(0, 0, 0),
                    esp8266_arduino=cv.Version(0, 0, 0),
                    rp2040_arduino=cv.Version(0, 0, 0),
                    bk72xx_arduino=cv.Version(1, 7, 0),
                ),
                cv.boolean_false,
            ),
        ),
        cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
        cv.GenerateID(CONF_NETWORK_ID): cv.declare_id(Resolver),
        cv.Optional(CONF_HOSTSFILE): cv.file_,
        cv.Optional(CONF_HOSTS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_NAME): cv.string,
                    cv.Required(CONF_IP_ADDRESS): cv.string,
                }
            )
        ),
    }
)


def parse_hosts_file(hosts_contents: str) -> list[tuple[str, IPAddress]]:
    """Parse a hosts file"""
    hosts: list[tuple[str, IPAddress]] = []
    for line in hosts_contents.splitlines():
        if line.startswith("#"):
            continue
        split_line = line.split()
        if len(split_line) < 2:
            continue
        ip_address: IPAddress = IPAddress(split_line[0])
        hosts.extend([(host, ip_address) for host in split_line[1:]])
    return hosts


@coroutine
async def to_code(config):
    cg.add_define("USE_NETWORK")
    if CORE.using_arduino and CORE.is_esp32:
        cg.add_library("Networking", None)
    if (enable_ipv6 := config.get(CONF_ENABLE_IPV6, None)) is not None:
        cg.add_define("USE_NETWORK_IPV6", enable_ipv6)
        if enable_ipv6:
            cg.add_define(
                "USE_NETWORK_MIN_IPV6_ADDR_COUNT", config[CONF_MIN_IPV6_ADDR_COUNT]
            )
        if CORE.using_esp_idf:
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", enable_ipv6)
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", enable_ipv6)
        elif enable_ipv6:
            cg.add_build_flag("-DCONFIG_LWIP_IPV6")
            cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG")
            if CORE.is_rp2040:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
            if CORE.is_esp8266:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
            if CORE.is_bk72xx:
                cg.add_build_flag("-DCONFIG_IPV6")
    hosts = []
    if CONF_HOSTS in config:
        hosts = [
            (host[CONF_NAME], IPAddress(host[CONF_IP_ADDRESS]))
            for host in config[CONF_HOSTS]
        ]
    if CONF_HOSTSFILE in config:
        hosts_contents = helpers.read_file(
            CORE.relative_config_path(config[CONF_HOSTSFILE])
        )
        hosts.extend(parse_hosts_file(hosts_contents))

    map_ = cg.std_ns.class_("map").template(cg.std_string, IPAddress)
    cg.new_Pvariable(config[CONF_NETWORK_ID], map_(hosts))
