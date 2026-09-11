"""IR/RF Proxy component - provides remote_base backend for infrared platform."""

import esphome.codegen as cg
from esphome.components import remote_base
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]

# Namespace and constants exported for infrared.py platform
ir_rf_proxy_ns = cg.esphome_ns.namespace("ir_rf_proxy")

CONF_REMOTE_RECEIVER_ID = "remote_receiver_id"
CONF_REMOTE_TRANSMITTER_ID = "remote_transmitter_id"


async def attach_receiver(var: MockObj, config: ConfigType) -> None:
    """Wire the configured remote_receiver to a proxy entity and register it as a listener."""
    receiver = await cg.get_variable(config[CONF_REMOTE_RECEIVER_ID])
    cg.add(var.set_receiver(receiver))
    remote_base.add_listener(receiver, var)
