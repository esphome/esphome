"""SMB2/CIFS Client component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT, CONF_USERNAME, CONF_PASSWORD
from esphome.components import network

# Constants
CONF_SERVER = "server"
CONF_SHARE = "share"
CONF_DOMAIN = "domain"
CONF_MOUNT_PATH = "mount_path"

# Namespace
smb_client_ns = cg.esphome_ns.namespace("smb_client")
SMBClient = smb_client_ns.class_("SMBClient", cg.Component)

# Default values
DEFAULT_PORT = 445
DEFAULT_DOMAIN = "WORKGROUP"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SMBClient),
        cv.Required(CONF_SERVER): cv.string,
        cv.Required(CONF_SHARE): cv.string,
        cv.Required(CONF_USERNAME): cv.string,
        cv.Required(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Optional(CONF_DOMAIN, default=DEFAULT_DOMAIN): cv.string,
        cv.Optional(CONF_MOUNT_PATH): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for SMB client component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add network dependency
    cg.add_define("USE_NETWORK")

    # Configure SMB client
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_share(config[CONF_SHARE]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_domain(config[CONF_DOMAIN]))

    # Set mount path if specified
    if CONF_MOUNT_PATH in config:
        cg.add(var.set_mount_path(config[CONF_MOUNT_PATH]))
