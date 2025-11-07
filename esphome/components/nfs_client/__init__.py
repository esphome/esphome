"""NFS v3 Client component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.components import network

# Constants
CONF_SERVER = "server"
CONF_EXPORT = "export"
CONF_MOUNT_PATH = "mount_path"
CONF_UID = "uid"
CONF_GID = "gid"

# Namespace
nfs_client_ns = cg.esphome_ns.namespace("nfs_client")
NFSClient = nfs_client_ns.class_("NFSClient", cg.Component)

# Default values
DEFAULT_PORT = 2049
DEFAULT_UID = 0
DEFAULT_GID = 0

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NFSClient),
        cv.Required(CONF_SERVER): cv.string,
        cv.Required(CONF_EXPORT): cv.string,
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Optional(CONF_MOUNT_PATH): cv.string,
        cv.Optional(CONF_UID, default=DEFAULT_UID): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_GID, default=DEFAULT_GID): cv.int_range(min=0, max=65535),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for NFS client component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add network dependency
    cg.add_define("USE_NETWORK")

    # Configure NFS client
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_export(config[CONF_EXPORT]))
    cg.add(var.set_uid(config[CONF_UID]))
    cg.add(var.set_gid(config[CONF_GID]))

    # Set mount path if specified
    if CONF_MOUNT_PATH in config:
        cg.add(var.set_mount_path(config[CONF_MOUNT_PATH]))
