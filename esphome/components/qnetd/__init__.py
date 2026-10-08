"""Corosync qnetd quorum arbiter for two-node clusters.

The device is the qnetd *server*; the cluster nodes run stock corosync-qdevice
pointed at it. Only the ffsplit algorithm is implemented and the connection is
plaintext (the server advertises TLS as unsupported).
"""

import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT, CONF_REBOOT_TIMEOUT
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@fhedberg"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]

CONF_QNETD_ID = "qnetd_id"
DEFAULT_PORT = 5403

qnetd_ns = cg.esphome_ns.namespace("qnetd")
Qnetd = qnetd_ns.class_("Qnetd", cg.Component)


def _consume_sockets(config: ConfigType) -> ConfigType:
    from esphome.components import socket

    # one listening socket plus one connection per served client; must match
    # MAX_CLIENTS in qnetd_server.h
    socket.consume_sockets(4, "qnetd")(config)
    socket.consume_sockets(1, "qnetd", socket.SocketType.TCP_LISTEN)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Qnetd),
            cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _consume_sockets,
)


def _warn_on_reboot_timeouts(config: ConfigType) -> None:
    # An arbiter must keep running through the outage it exists for, when the
    # Wi-Fi access point or Home Assistant may well be guests on the very
    # cluster it arbitrates. Rebooting then drops both nodes' qdevice vote.
    full_config = fv.full_config.get()
    for component in ("api", "wifi", "ethernet"):
        if (
            (conf := full_config.get(component))
            and isinstance(conf, dict)
            and (timeout := conf.get(CONF_REBOOT_TIMEOUT)) is not None
            and timeout.total_milliseconds != 0
        ):
            _LOGGER.warning(
                "qnetd: '%s: reboot_timeout' is %s; set it to 0s so the arbiter "
                "never reboots while the cluster it serves is unreachable",
                component,
                timeout,
            )


FINAL_VALIDATE_SCHEMA = _warn_on_reboot_timeouts


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PORT])
    await cg.register_component(var, config)
