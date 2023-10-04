import re
import ipaddress
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TIME_ID,
    CONF_ADDRESS,
    CONF_REBOOT_TIMEOUT,
)
from esphome.components import time
from esphome.core import TimePeriod

CONF_NETMASK = "netmask"
CONF_PRIVATE_KEY = "private_key"
CONF_PEER_ENDPOINT = "peer_endpoint"
CONF_PEER_PUBLIC_KEY = "peer_public_key"
CONF_PEER_PORT = "peer_port"
CONF_PEER_PRESHARED_KEY = "peer_preshared_key"
CONF_PEER_ALLOWED_IPS = "peer_allowed_ips"
CONF_PEER_PERSISTENT_KEEPALIVE = "peer_persistent_keepalive"
CONF_REQUIRE_CONNECTION_TO_PROCEED = "require_connection_to_proceed"

DEPENDENCIES = ["time", "esp32"]
CODEOWNERS = ["@lhoracek", "@droscy", "@thomas0bernard"]

# The key validation regex has been described by Jason Donenfeld himself
# url: https://lists.zx2c4.com/pipermail/wireguard/2020-December/006222.html
_WG_KEY_REGEX = re.compile(r"^[A-Za-z0-9+/]{42}[AEIMQUYcgkosw480]=$")

wireguard_ns = cg.esphome_ns.namespace("wireguard")
Wireguard = wireguard_ns.class_("Wireguard", cg.Component, cg.PollingComponent)


def _wireguard_key(value):
    if _WG_KEY_REGEX.match(cv.string(value)) is not None:
        return value
    raise cv.Invalid(f"Invalid WireGuard key: {value}")


def _cidr_network(value):
    try:
        ipaddress.ip_network(value, strict=False)
    except ValueError as err:
        raise cv.Invalid(f"Invalid network in CIDR notation: {err}")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Wireguard),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_ADDRESS): cv.ipv4,
        cv.Optional(CONF_NETMASK, default="255.255.255.255"): cv.ipv4,
        cv.Required(CONF_PRIVATE_KEY): _wireguard_key,
        cv.Required(CONF_PEER_ENDPOINT): cv.string,
        cv.Required(CONF_PEER_PUBLIC_KEY): _wireguard_key,
        cv.Optional(CONF_PEER_PORT, default=51820): cv.port,
        cv.Optional(CONF_PEER_PRESHARED_KEY): _wireguard_key,
        cv.Optional(CONF_PEER_ALLOWED_IPS, default=["0.0.0.0/0"]): cv.ensure_list(
            _cidr_network
        ),
        cv.Optional(CONF_PEER_PERSISTENT_KEEPALIVE, default="0s"): cv.All(
            cv.positive_time_period_seconds,
            cv.Range(max=TimePeriod(seconds=65535)),
        ),
        cv.Optional(
            CONF_REBOOT_TIMEOUT, default="15min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_REQUIRE_CONNECTION_TO_PROCEED, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema("10s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_address(str(config[CONF_ADDRESS])))
    cg.add(var.set_netmask(str(config[CONF_NETMASK])))
    cg.add(var.set_private_key(config[CONF_PRIVATE_KEY]))
    cg.add(var.set_peer_endpoint(config[CONF_PEER_ENDPOINT]))
    cg.add(var.set_peer_public_key(config[CONF_PEER_PUBLIC_KEY]))
    cg.add(var.set_peer_port(config[CONF_PEER_PORT]))
    cg.add(var.set_keepalive(config[CONF_PEER_PERSISTENT_KEEPALIVE]))
    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    if CONF_PEER_PRESHARED_KEY in config:
        cg.add(var.set_preshared_key(config[CONF_PEER_PRESHARED_KEY]))

    allowed_ips = list(
        ipaddress.collapse_addresses(
            [
                ipaddress.ip_network(ip, strict=False)
                for ip in config[CONF_PEER_ALLOWED_IPS]
            ]
        )
    )

    for ip in allowed_ips:
        cg.add(var.add_allowed_ip(str(ip.network_address), str(ip.netmask)))

    cg.add(var.set_srctime(await cg.get_variable(config[CONF_TIME_ID])))

    if config[CONF_REQUIRE_CONNECTION_TO_PROCEED]:
        cg.add(var.disable_auto_proceed())

    # This flag is added here because the esp_wireguard library statically
    # set the size of its allowed_ips list at compile time using this value;
    # the '+1' modifier is relative to the device's own address that will
    # be automatically added to the provided list.
    cg.add_build_flag(f"-DCONFIG_WIREGUARD_MAX_SRC_IPS={len(allowed_ips) + 1}")
    cg.add_library("droscy/esp_wireguard", "0.3.2")

    await cg.register_component(var, config)
