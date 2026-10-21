from ipaddress import IPv4Address, IPv6Address
from typing import Any

from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
from esphome.components import network
from esphome.components.const import CONF_ON_RECEIVE
from esphome.components.packet_transport import (
    CONF_BINARY_SENSORS,
    CONF_ENCRYPTION,
    CONF_PING_PONG_ENABLE,
    CONF_PROVIDERS,
    CONF_ROLLING_CODE_ENABLE,
    CONF_SENSORS,
)
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_PORT, CONF_TRIGGER_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]
DOMAIN = "udp"

MULTI_CONF = True
udp_ns = cg.esphome_ns.namespace("udp")
UDPComponent = udp_ns.class_("UDPComponent", cg.Component)
UDPWriteAction = udp_ns.class_("UDPWriteAction", automation.Action)
trigger_argname = "data"
# Listener callback type (non-owning span from UDP component)
listener_args = cg.std_span.template(cg.uint8.operator("const"))
listener_argtype = [(listener_args, trigger_argname)]
# Automation/trigger type (owned vector, safe for deferred actions like delay)
trigger_args = cg.std_vector.template(cg.uint8)
trigger_argtype = [(trigger_args, trigger_argname)]

CONF_ADDRESSES = "addresses"
CONF_LISTEN_ADDRESS = "listen_address"
CONF_UDP_ID = "udp_id"
CONF_LISTEN_PORT = "listen_port"
CONF_BROADCAST_PORT = "broadcast_port"

UDP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_UDP_ID): cv.use_id(UDPComponent),
    }
)


RELOCATED = {
    cv.Optional(x): cv.invalid(
        f"The '{x}' option should now be configured in the 'packet_transport' component"
    )
    for x in (
        CONF_PROVIDERS,
        CONF_ENCRYPTION,
        CONF_PING_PONG_ENABLE,
        CONF_ROLLING_CODE_ENABLE,
        CONF_SENSORS,
        CONF_BINARY_SENSORS,
    )
}


def _no_scope_id(addr: IPv4Address | IPv6Address) -> IPv4Address | IPv6Address:
    """Python accepts a scope id, which neither inet_pton nor lwIP's inet6_aton takes."""
    if isinstance(addr, IPv6Address) and addr.scope_id is not None:
        raise cv.Invalid(
            f"{addr} carries a scope id, which the device cannot parse; "
            "give the address without the '%' part"
        )
    return addr


def _address(value: Any) -> IPv4Address | IPv6Address:
    addr = _no_scope_id(cv.ipaddress(value))
    # A link-local destination needs a scope id, which the device cannot pick on every platform
    if isinstance(addr, IPv6Address) and addr.is_link_local:
        raise cv.Invalid(
            f"{addr} is link-local; use a multicast, unique local or global address"
        )
    return addr


def _listen_address(value: Any) -> IPv4Address | IPv6Address:
    """Accept an IPv4 multicast/broadcast or an IPv6 multicast address, with a family-specific error."""
    if ":" not in str(value):
        return cv.ipv4address_multi_broadcast(value)
    addr = _no_scope_id(cv.ipv6address(value))
    if not addr.is_multicast:
        raise cv.Invalid(f"{value} is not an IPv6 multicast address")
    return addr


def _final_validate(config: ConfigType) -> None:
    from esphome.components import socket

    full_config = fv.full_config.get()
    raw_lwip = (
        full_config.get(socket.DOMAIN, {}).get(socket.CONF_IMPLEMENTATION)
        == socket.IMPLEMENTATION_LWIP_TCP
    )
    for key, addresses in (
        (CONF_ADDRESSES, config[CONF_ADDRESSES]),
        (CONF_LISTEN_ADDRESS, [config[CONF_LISTEN_ADDRESS]]),
    ):
        for addr in addresses:
            if addr.version != 6:
                continue
            if raw_lwip:
                raise cv.Invalid(
                    f"IPv6 address '{addr}' is not supported on this platform", [key]
                )


FINAL_VALIDATE_SCHEMA = _final_validate


def _ipv6_addresses(config: ConfigType) -> list[IPv6Address]:
    return [addr for addr in config[CONF_ADDRESSES] if addr.version == 6]


def _require_ipv6_for_ipv6_addresses(config: ConfigType) -> ConfigType:
    if _ipv6_addresses(config) or config[CONF_LISTEN_ADDRESS].version == 6:
        network.require_ipv6("udp")(config)
    return config


def _has_ipv4_address(config: ConfigType) -> bool:
    return any(addr.version == 4 for addr in config[CONF_ADDRESSES])


def _consume_udp_sockets(config: ConfigType) -> ConfigType:
    """Reserve the listen socket plus one send socket per address family in use."""
    from esphome.components import socket

    count = 1 + _has_ipv4_address(config) + bool(_ipv6_addresses(config))
    socket.consume_sockets(count, "udp", socket.SocketType.UDP)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(UDPComponent),
            cv.Optional(CONF_PORT, default=18511): cv.Any(
                cv.port,
                cv.Schema(
                    {
                        cv.Required(CONF_LISTEN_PORT): cv.port,
                        cv.Required(CONF_BROADCAST_PORT): cv.port,
                    }
                ),
            ),
            cv.Optional(
                CONF_LISTEN_ADDRESS, default="255.255.255.255"
            ): _listen_address,
            cv.Optional(CONF_ADDRESSES, default=["255.255.255.255"]): cv.ensure_list(
                _address
            ),
            cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        Trigger.template(trigger_args)
                    ),
                }
            ),
        }
    ).extend(RELOCATED),
    _consume_udp_sockets,
    network.require_ipv4("udp"),
    _require_ipv6_for_ipv6_addresses,
)


async def register_udp_client(var: MockObj, config: ConfigType) -> MockObj:
    udp_var = await cg.get_variable(config[CONF_UDP_ID])
    cg.add(var.set_parent(udp_var))
    return udp_var


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_UDP")
    cg.add_global(udp_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    var = await cg.register_component(var, config)
    conf_port = config[CONF_PORT]
    if isinstance(conf_port, int):
        cg.add(var.set_listen_port(conf_port))
        cg.add(var.set_broadcast_port(conf_port))
    else:
        cg.add(var.set_listen_port(conf_port[CONF_LISTEN_PORT]))
        cg.add(var.set_broadcast_port(conf_port[CONF_BROADCAST_PORT]))
    if (listen_address := str(config[CONF_LISTEN_ADDRESS])) != "255.255.255.255":
        cg.add(var.set_listen_address(listen_address))
    if ipv6_addresses := _ipv6_addresses(config):
        cg.add_define("USE_UDP_IPV6_SEND")
        cg.add(var.set_ipv6_send(any(addr.is_multicast for addr in ipv6_addresses)))
        if not _has_ipv4_address(config):
            cg.add(var.set_ipv4_send(False))
    # Shared flash table ended by nullptr, so the component stores only a pointer.
    if addresses := [str(addr) for addr in config[CONF_ADDRESSES]]:
        cg.add(
            var.set_addresses(
                cg.shared_progmem_array(
                    "udp_addresses", cg.const_char_ptr, [*addresses, cg.nullptr]
                )
            )
        )
    for conf in config.get(CONF_ON_RECEIVE, []):
        trigger_id = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        trigger = await automation.build_automation(trigger_id, trigger_argtype, conf)
        trigger_lambda = await cg.process_lambda(
            trigger.trigger(
                cg.std_vector.template(cg.uint8)(
                    MockObj(trigger_argname).begin(),
                    MockObj(trigger_argname).end(),
                )
            ),
            listener_argtype,
        )
        cg.add(var.add_listener(trigger_lambda))
    if config.get(CONF_ON_RECEIVE):
        cg.add(var.set_should_listen())


def validate_raw_data(value: Any) -> bytes | list[int]:
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


@automation.register_action(
    "udp.write",
    UDPWriteAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(UDPComponent),
            cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
        },
        key=CONF_DATA,
    ),
    synchronous=True,
)
async def udp_write_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    udp_var = await cg.get_variable(config[CONF_ID])
    await cg.register_parented(var, udp_var)
    cg.add(udp_var.set_should_broadcast())
    await automation.templatable_bytes(
        config[CONF_DATA], args, var.set_data_template, var.set_data_static, "udp_data"
    )
    return var
