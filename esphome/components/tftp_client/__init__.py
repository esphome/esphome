"""TFTP Client component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT, CONF_SERVER
from esphome.components import network

# Constants
CONF_MOUNT_PATH = "mount_path"

# Namespace
tftp_client_ns = cg.esphome_ns.namespace("tftp_client")
TFTPClient = tftp_client_ns.class_("TFTPClient", cg.Component)

# Default values
DEFAULT_PORT = 69

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TFTPClient),
        cv.Required(CONF_SERVER): cv.string,
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Optional(CONF_MOUNT_PATH): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for TFTP client component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add network dependency
    cg.add_define("USE_NETWORK")

    # Configure TFTP client
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))

    # Set mount path if specified
    if CONF_MOUNT_PATH in config:
        cg.add(var.set_mount_path(config[CONF_MOUNT_PATH]))
