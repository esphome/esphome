CODEOWNERS = ["@tek79"]

import esphome.codegen as cg
from esphome.components import network
from esphome.components.network import ip_address_literal
import esphome.config_validation as cv
from esphome.const import (
    CONF_DNS1,
    CONF_DNS2,
    CONF_DOMAIN,
    CONF_GATEWAY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_MANUAL_IP,
    CONF_STATIC_IP,
    CONF_SUBNET,
    CONF_USE_ADDRESS,
)
from esphome.core import CORE

usb_ethernet_ns = cg.esphome_ns.namespace("usb_ethernet")
USBEthernetComponent = usb_ethernet_ns.class_(
    "USBEthernetComponent",
    cg.Component,
)

ManualIP = usb_ethernet_ns.struct("ManualIP")

MANUAL_IP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATIC_IP): cv.ipv4address,
        cv.Required(CONF_GATEWAY): cv.ipv4address,
        cv.Required(CONF_SUBNET): cv.ipv4address,
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4address,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4address,
    }
)


def _validate(config):
    """Validate and set use_address if not explicitly provided."""
    if CONF_USE_ADDRESS not in config:
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        else:
            use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(USBEthernetComponent),
            cv.Optional(CONF_MANUAL_IP): MANUAL_IP_SCHEMA,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
        }
    ),
    _validate,
)


def manual_ip(config):
    """Helper function to create ManualIP struct initializer."""
    return cg.StructInitializer(
        ManualIP,
        ("static_ip", ip_address_literal(config[CONF_STATIC_IP])),
        ("gateway", ip_address_literal(config[CONF_GATEWAY])),
        ("subnet", ip_address_literal(config[CONF_SUBNET])),
        ("dns1", ip_address_literal(config[CONF_DNS1])),
        ("dns2", ip_address_literal(config[CONF_DNS2])),
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set use_address (validated and set in _validate)
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    # Set manual IP if configured
    if CONF_MANUAL_IP in config:
        cg.add(var.set_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    # Set MAC address if configured
    if mac_address := config.get(CONF_MAC_ADDRESS):
        cg.add(var.set_fixed_mac(mac_address.parts))

    if hasattr(network, "register_addressable"):
        await network.register_addressable(var, config)

    cg.add_define("USE_USB_ETHERNET")
