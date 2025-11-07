"""TFTP Server component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.components import network

# Constants
CONF_ROOT_DIR = "root_dir"
CONF_ACCESS_MODE = "access_mode"
CONF_MAX_FILE_SIZE = "max_file_size"
CONF_MAX_SESSIONS = "max_sessions"

# Namespace
tftp_server_ns = cg.esphome_ns.namespace("tftp_server")
TFTPServer = tftp_server_ns.class_("TFTPServer", cg.Component)

# Enums
AccessMode = tftp_server_ns.enum("AccessMode")
ACCESS_MODES = {
    "READ_ONLY": AccessMode.ACCESS_READ_ONLY,
    "WRITE_ONLY": AccessMode.ACCESS_WRITE_ONLY,
    "READ_WRITE": AccessMode.ACCESS_READ_WRITE,
}

# Default values
DEFAULT_PORT = 69
DEFAULT_ROOT_DIR = "/"
DEFAULT_ACCESS_MODE = "READ_ONLY"
DEFAULT_MAX_FILE_SIZE = 1048576  # 1MB
DEFAULT_MAX_SESSIONS = 4

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TFTPServer),
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Optional(CONF_ROOT_DIR, default=DEFAULT_ROOT_DIR): cv.string,
        cv.Optional(CONF_ACCESS_MODE, default=DEFAULT_ACCESS_MODE): cv.enum(
            ACCESS_MODES, upper=True
        ),
        cv.Optional(CONF_MAX_FILE_SIZE, default=DEFAULT_MAX_FILE_SIZE): cv.int_range(
            min=1024, max=10485760  # 1KB - 10MB
        ),
        cv.Optional(CONF_MAX_SESSIONS, default=DEFAULT_MAX_SESSIONS): cv.int_range(
            min=1, max=16
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for TFTP server component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add network dependency
    cg.add_define("USE_NETWORK")

    # Configure TFTP server
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_root_dir(config[CONF_ROOT_DIR]))
    cg.add(var.set_access_mode(config[CONF_ACCESS_MODE]))
    cg.add(var.set_max_file_size(config[CONF_MAX_FILE_SIZE]))
    cg.add(var.set_max_sessions(config[CONF_MAX_SESSIONS]))
