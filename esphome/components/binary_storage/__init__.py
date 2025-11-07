"""Binary Storage Component - Unified interface for FRAM, EEPROM, Flash storage devices."""
from __future__ import annotations

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, spi, esp32
from esphome.const import CONF_ID, CONF_MODEL, CONF_TYPE, CONF_VALUE, CONF_ADDRESS, CONF_LENGTH, CONF_DATA, CONF_MODE
from esphome import automation
import re

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = []
AUTO_LOAD = []
MULTI_CONF = True

# Namespace
binary_storage_ns = cg.esphome_ns.namespace("binary_storage")

# Base class
BinaryStorage = binary_storage_ns.class_("BinaryStorage", cg.Component)

# I2C EEPROM class
I2CEeprom = binary_storage_ns.class_("I2CEeprom", BinaryStorage, i2c.I2CDevice)

# I2C FRAM class
I2CFram = binary_storage_ns.class_("I2CFram", BinaryStorage, i2c.I2CDevice)

# SPI Flash class
SPIFlash = binary_storage_ns.class_(
    "SPIFlash",
    BinaryStorage,
    spi.SPIDevice,
    cg.Parented.template(spi.SPIComponent),
)

# SPI FRAM class
SPIFram = binary_storage_ns.class_(
    "SPIFram",
    BinaryStorage,
    spi.SPIDevice,
    cg.Parented.template(spi.SPIComponent),
)

# LittleFS Mount class
LittleFSMount = binary_storage_ns.class_("LittleFSMount", cg.Component)

# Automation classes
ReadAction = binary_storage_ns.class_("ReadAction", automation.Action)
WriteAction = binary_storage_ns.class_("WriteAction", automation.Action)
FillAction = binary_storage_ns.class_("FillAction", automation.Action)
WriteByteAction = binary_storage_ns.class_("WriteByteAction", automation.Action)
WriteStringAction = binary_storage_ns.class_("WriteStringAction", automation.Action)
IsReadyCondition = binary_storage_ns.class_("IsReadyCondition", automation.Condition)

# Configuration keys (additional to CONF_* from const)
CONF_PAGE_SIZE = "page_size"
CONF_CAPACITY = "capacity"
CONF_ADDRESSING_BITS = "addressing_bits"
CONF_MOUNT_PATH = "mount_path"
CONF_AUTO_FORMAT = "auto_format"
CONF_PARTITION_LABEL = "partition_label"
CONF_FILESYSTEM = "filesystem"
CONF_STORAGE_DEVICE = "storage_device"
CONF_ERASE_SIZE = "erase_size"
CONF_JEDEC_ID = "jedec_id"

# Storage modes
MODE_RAW = "raw"
MODE_LITTLEFS = "littlefs"
MODE_BOTH = "both"


def validate_bytes(value):
    """Validate and parse byte size with units (e.g., '32KB', '256KiB')."""
    value = cv.string(value).lower()
    match = re.match(r"^([0-9]+)\s*(\w*)$", value)

    if match is None:
        raise cv.Invalid(f"Expected number with optional unit, got {value}")

    suffixes = {
        "": 1,
        "b": 1,
        "kb": 1000,
        "kib": 1024,
        "mb": 1000 ** 2,
        "mib": 1024 ** 2,
    }

    suffix = match.group(2)
    if suffix and suffix not in suffixes:
        raise cv.Invalid(f"Invalid suffix '{suffix}', valid: B, KB, KiB, MB, MiB")

    return int(int(match.group(1)) * suffixes[suffix])


# EEPROM Configuration Schema
EEPROM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(I2CEeprom),
            # Device configuration
            cv.Optional(CONF_MODEL, default="AT24C256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=8, max=128),
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(8, 9, 10, 11, 16, int=True),
            # Usage mode: how to use this device
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            # LittleFS configuration (only used if mode is littlefs or both)
            cv.Optional(CONF_MOUNT_PATH): cv.string,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_PARTITION_LABEL): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x50))
)

# FRAM Configuration Schema
FRAM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(I2CFram),
            # Device configuration
            cv.Optional(CONF_MODEL, default="MB85RC256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(9, 11, 16, 32, int=True),
            # Usage mode: how to use this device
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            # LittleFS configuration (only used if mode is littlefs or both)
            cv.Optional(CONF_MOUNT_PATH): cv.string,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_PARTITION_LABEL): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x50))
)

# SPI Flash Configuration Schema
SPI_FLASH_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIFlash),
            # Device configuration
            cv.Optional(CONF_MODEL, default="W25Q32"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=256, max=256),  # Standard: 256 bytes
            cv.Optional(CONF_ERASE_SIZE): cv.one_of(4096, 32768, 65536, int=True),  # 4KB, 32KB, 64KB
            cv.Optional(CONF_JEDEC_ID): cv.hex_uint32_t,
            # Usage mode: how to use this device
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            # LittleFS configuration (only used if mode is littlefs or both)
            cv.Optional(CONF_MOUNT_PATH): cv.string,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_PARTITION_LABEL): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

# SPI FRAM Configuration Schema
SPI_FRAM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIFram),
            # Device configuration
            cv.Optional(CONF_MODEL, default="FM25V10"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(16, 24, int=True),
            # Usage mode: how to use this device
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            # LittleFS configuration (only used if mode is littlefs or both)
            cv.Optional(CONF_MOUNT_PATH): cv.string,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_PARTITION_LABEL): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

# Typed schema for device selection
CONFIG_SCHEMA = cv.typed_schema(
    {
        "EEPROM": EEPROM_SCHEMA,
        "I2C_EEPROM": EEPROM_SCHEMA,
        "FRAM": FRAM_SCHEMA,
        "I2C_FRAM": FRAM_SCHEMA,
        "SPI_FLASH": SPI_FLASH_SCHEMA,
        "FLASH": SPI_FLASH_SCHEMA,
        "SPI_FRAM": SPI_FRAM_SCHEMA,
    },
    key=CONF_TYPE,
    default_type="EEPROM",
    upper=True,
)


async def to_code(config):
    """Configure binary storage device."""
    mode = config.get(CONF_MODE, MODE_RAW)
    device_type = config.get(CONF_TYPE, "EEPROM").upper()

    # Add LittleFS ESP-IDF component if filesystem is enabled
    if mode in [MODE_LITTLEFS, MODE_BOTH]:
        esp32.add_idf_component(name="joltwallet/littlefs", ref="1.14.8")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Register as I2C or SPI device
    is_spi = device_type in ["SPI_FLASH", "FLASH", "SPI_FRAM"]
    if is_spi:
        await spi.register_spi_device(var, config)
    else:
        await i2c.register_i2c_device(var, config)

    # Set model name
    if CONF_MODEL in config:
        cg.add(var.set_model(config[CONF_MODEL]))

    # Set capacity if specified (otherwise auto-detected from model)
    if CONF_CAPACITY in config:
        cg.add(var.set_capacity(config[CONF_CAPACITY]))

    # Set page size if specified (EEPROM and Flash)
    if CONF_PAGE_SIZE in config:
        cg.add(var.set_page_size(config[CONF_PAGE_SIZE]))

    # Set erase size if specified (Flash only)
    if CONF_ERASE_SIZE in config:
        cg.add(var.set_erase_size(config[CONF_ERASE_SIZE]))

    # Set JEDEC ID if specified (Flash only)
    if CONF_JEDEC_ID in config:
        cg.add(var.set_jedec_id(config[CONF_JEDEC_ID]))

    # Set addressing bits if specified
    if CONF_ADDRESSING_BITS in config:
        cg.add(var.set_addressing_bits(config[CONF_ADDRESSING_BITS]))

    # Setup LittleFS mount if mode requires it
    if mode in [MODE_LITTLEFS, MODE_BOTH]:
        # Determine mount path (required for littlefs mode)
        mount_path = config.get(CONF_MOUNT_PATH)
        if not mount_path:
            # Auto-generate mount path from device id
            mount_path = f"/{config[CONF_ID]}"

        mount_var = cg.new_Pvariable(
            cg.RawExpression(f"id({config[CONF_ID]}_mount)"), LittleFSMount
        )
        await cg.register_component(mount_var, config)

        cg.add(mount_var.set_storage_device(var))
        cg.add(mount_var.set_mount_path(mount_path))

        if CONF_AUTO_FORMAT in config:
            cg.add(mount_var.set_auto_format(config[CONF_AUTO_FORMAT]))

        if CONF_PARTITION_LABEL in config:
            cg.add(mount_var.set_partition_label(config[CONF_PARTITION_LABEL]))

    # Register with storage_host via CORE.data
    from esphome.core import CORE

    if not hasattr(CORE, "data"):
        CORE.data = {}
    if "binary_storage_devices" not in CORE.data:
        CORE.data["binary_storage_devices"] = []
    CORE.data["binary_storage_devices"].append(var)


# ============================================================================
# Automation Actions and Conditions
# ============================================================================

BINARY_STORAGE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(BinaryStorage),
    }
)


@automation.register_action(
    "binary_storage.read",
    ReadAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BinaryStorage),
            cv.Required(CONF_ADDRESS): cv.templatable(cv.uint32_t),
            cv.Required(CONF_LENGTH): cv.templatable(cv.uint32_t),
        }
    ),
)
async def binary_storage_read_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint32)
    cg.add(var.set_address(template_))

    template_ = await cg.templatable(config[CONF_LENGTH], args, cg.uint32)
    cg.add(var.set_length(template_))

    return var


@automation.register_action(
    "binary_storage.write",
    WriteAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BinaryStorage),
            cv.Required(CONF_ADDRESS): cv.templatable(cv.uint32_t),
            cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        }
    ),
)
async def binary_storage_write_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint32)
    cg.add(var.set_address(template_))

    if cg.is_template(config[CONF_DATA]):
        template_ = await cg.templatable(
            config[CONF_DATA], args, cg.std_vector.template(cg.uint8)
        )
        cg.add(var.set_data_template(template_))
    else:
        cg.add(var.set_data(config[CONF_DATA]))

    return var


@automation.register_action(
    "binary_storage.fill",
    FillAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BinaryStorage),
            cv.Optional(CONF_VALUE, default=0xFF): cv.templatable(cv.hex_uint8_t),
        }
    ),
)
async def binary_storage_fill_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint8)
    cg.add(var.set_value(template_))

    return var


@automation.register_action(
    "binary_storage.write_byte",
    WriteByteAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BinaryStorage),
            cv.Required(CONF_ADDRESS): cv.templatable(cv.uint32_t),
            cv.Required(CONF_VALUE): cv.templatable(cv.hex_uint8_t),
        }
    ),
)
async def binary_storage_write_byte_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint32)
    cg.add(var.set_address(template_))

    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint8)
    cg.add(var.set_value(template_))

    return var


@automation.register_action(
    "binary_storage.write_string",
    WriteStringAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BinaryStorage),
            cv.Required(CONF_ADDRESS): cv.templatable(cv.uint32_t),
            cv.Required(CONF_VALUE): cv.templatable(cv.string),
        }
    ),
)
async def binary_storage_write_string_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_ADDRESS], args, cg.uint32)
    cg.add(var.set_address(template_))

    template_ = await cg.templatable(config[CONF_VALUE], args, cg.std_string)
    cg.add(var.set_value(template_))

    return var


@automation.register_condition(
    "binary_storage.is_ready",
    IsReadyCondition,
    BINARY_STORAGE_ACTION_SCHEMA,
)
async def binary_storage_is_ready_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
