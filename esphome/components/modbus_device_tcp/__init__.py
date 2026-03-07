"""Modbus TCP device/responder component (esp-modbus). Protocol: controller/device."""

import logging
import os

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@pintomax"]
DEPENDENCIES = ["wifi"]

# Protocol-specific config keys (unit_id is the device/responder identifier)
CONF_UNIT_ID = "unit_id"
CONF_NUM_OBJECTS = "num_objects"

# Single source for esp-modbus library name (used for add_library and -I paths).
ESP_MODBUS_LIB = "esp-modbus"

# Use workimg version
ESP_MODBUS_REPO = "https://github.com/espressif/esp-modbus#15373b1"

# Default Modbus TCP port (single source; passed to C++ via add_define and used in schema).
DEFAULT_MODBUS_PORT = 1502

# Default Modbus unit ID (single source; passed to C++ via add_define and used in schema).
DEFAULT_MODBUS_UNIT_ID = 1

# Default number of objects (coils, discrete inputs, input/holding registers; single source for C++ and schema).
DEFAULT_MODBUS_NUM_OBJECTS = 5

# Define the namespace (matches integration name modbus_device_tcp)
modbus_device_tcp_ns = cg.esphome_ns.namespace("modbus_device_tcp")
ModbusDeviceTCP = modbus_device_tcp_ns.class_("ModbusDeviceTCP", cg.Component)

# -D flags for esp-modbus (ESP-IDF config). Names are from the library API.
# Note: CONFIG_FMB_CONTROLLER_SLAVE_ID is a fixed library build option; the actual unit ID
# is set at runtime via set_unit_id() from config[CONF_UNIT_ID].
_MODBUS_BUILD_DEFINES = [
    "-DMB_PORT_TCP_IPV4=1",
    "-DCONFIG_FMB_COMM_MODE_TCP_EN=1",
    "-DCONFIG_FMB_COMM_MODE_RTU_EN=0",
    "-DCONFIG_FMB_COMM_MODE_ASCII_EN=0",
    "-DCONFIG_MB_SLAVE_ADDR_TYPE_IPV4=1",
    "-DCONFIG_FMB_CONTROLLER_SLAVE_ID_SUPPORT=1",
    "-DCONFIG_FMB_CONTROLLER_SLAVE_ID=1",
    "-DCONFIG_FMB_CONTROLLER_SLAVE_ID_MAX_SIZE=32",
    "-DCONFIG_MB_MDNS_IP_RESOLVER=1",
    "-DCONFIG_FMB_TCP_CONNECTION_TOUT_SEC=20",
    "-DCONFIG_FMB_FUNC_HANDLERS_MAX=16",
    "-DCONFIG_FMB_CONTROLLER_NOTIFY_TIMEOUT=200",
    "-DCONFIG_FMB_CONTROLLER_STACK_SIZE=4096",
    "-DCONFIG_FMB_PORT_TASK_PRIO=10",
    "-DCONFIG_FMB_PORT_TASK_AFFINITY=1",
    "-DCONFIG_FMB_CONTROLLER_NOTIFY_QUEUE_SIZE=20",
    "-DCONFIG_FMB_TCP_PORT_DEFAULT=1502",
    "-DCONFIG_FMB_MASTER_TIMEOUT_MS_RESPOND=200",
    "-DCONFIG_FMB_BUFFER_SIZE=256",
    "-DCONFIG_FMB_SERIAL_BUF_SIZE=256",
    "-DCONFIG_FMB_SERIAL_TASK_PRIO=10",
    "-DCONFIG_FMB_QUEUE_LENGTH=20",
    "-DCONFIG_FMB_MASTER_DELAY_MS_CONVERT=200",
    "-DCONFIG_FMB_TCP_PORT_MAX_CONN=10",
    "-DCONFIG_FMB_TCP_KEEP_ALIVE_TOUT_SEC=1",
    "-DCONFIG_FMB_PORT_TASK_STACK_SIZE=4096",
]

# Relative paths under .piolibdeps/<env>/<lib>/ for -I
_MODBUS_INCLUDE_SUFFIXES = [
    "modbus/mb_controller/common/include",
    "modbus/mb_controller/common",
    "modbus/mb_controller/tcp",
    "modbus/mb_ports/common",
    "modbus/mb_ports/tcp",
    "modbus/mb_objects/common",
    "modbus/mb_objects/include",
    "modbus/mb_transports/tcp",
    "modbus/mb_transports/ascii",
    "modbus/mb_transports/rtu",
    "modbus/mb_transports",
    "modbus/mb_ports/serial",
    "modbus/mb_controller/serial",
    "modbus/include",
]


def _validate_esp32_esp_idf(config):
    """This component only supports ESP32 with ESP-IDF (esp-modbus requirement)."""
    if not CORE.is_esp32:
        raise cv.Invalid(
            "modbus_device_tcp only supports ESP32. Use an ESP32 board (not ESP8266 or other)."
        )
    if CORE.using_arduino:
        raise cv.Invalid(
            "modbus_device_tcp requires ESP-IDF. Set 'esp32: framework: type: esp-idf' in your YAML."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(ModbusDeviceTCP),
        cv.Optional(CONF_PORT, default=DEFAULT_MODBUS_PORT): cv.port,
        cv.Optional(CONF_UNIT_ID, default=DEFAULT_MODBUS_UNIT_ID): cv.int_range(0, 247),
        cv.Optional(CONF_NUM_OBJECTS, default=DEFAULT_MODBUS_NUM_OBJECTS): cv.int_range(1, 128),
    }).extend(cv.COMPONENT_SCHEMA),
    _validate_esp32_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library(ESP_MODBUS_LIB, None, ESP_MODBUS_REPO)

    for flag in _MODBUS_BUILD_DEFINES:
        cg.add_build_flag(flag)

    piolibdeps = CORE.relative_piolibdeps_path()
    env_name = CORE.name
    for suffix in _MODBUS_INCLUDE_SUFFIXES:
        inc_path = str(piolibdeps / env_name / ESP_MODBUS_LIB / suffix)
        cg.add_build_flag(f"-I{inc_path}")

    cg.add_platformio_option("lib_ignore", ["test_apps", "examples"])

    component_dir = os.path.dirname(os.path.abspath(__file__))
    script_path = os.path.join(component_dir, "filter_esp_modbus.py")
    rel_to_build = os.path.relpath(script_path, CORE.build_path)
    cg.add_platformio_option("extra_scripts", [f"pre:{rel_to_build}"])

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
    cg.add(var.set_num_objects(config[CONF_NUM_OBJECTS]))
    cg.add_define("MODBUS_NUM_OBJECTS", config[CONF_NUM_OBJECTS])
