from __future__ import annotations

import re

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c, spi
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    require_vfs_dir,
)
from esphome.components.storage import (
    request_storage_device,
    request_storage_worker,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CAPACITY,
    CONF_DATA,
    CONF_ID,
    CONF_LENGTH,
    CONF_MODE,
    CONF_MODEL,
    CONF_PIN,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE

CODEOWNERS = ["@p1ngb4ck"]
DEPENDENCIES = []
AUTO_LOAD = ["storage"]
MULTI_CONF = True

# Namespaces
binary_storage_ns = cg.esphome_ns.namespace("binary_storage")
storage_ns = cg.esphome_ns.namespace("storage")

# Base classes (from storage component)
RawStorage = storage_ns.class_("RawStorage", cg.Component)
FilesystemStorage = storage_ns.class_("FilesystemStorage", cg.Component)

# binary_storage device classes — extend RawStorage
BinaryStorage = binary_storage_ns.class_("BinaryStorage", RawStorage)
I2CEeprom = binary_storage_ns.class_("I2CEeprom", BinaryStorage, i2c.I2CDevice)
I2CFram = binary_storage_ns.class_("I2CFram", BinaryStorage, i2c.I2CDevice)
SPIFlash = binary_storage_ns.class_(
    "SPIFlash",
    BinaryStorage,
    spi.SPIDevice,
    cg.Parented.template(spi.SPIComponent),
)
SPIFram = binary_storage_ns.class_(
    "SPIFram",
    BinaryStorage,
    spi.SPIDevice,
    cg.Parented.template(spi.SPIComponent),
)
SPIMRAM = binary_storage_ns.class_(
    "SPIMRAM",
    BinaryStorage,
    spi.SPIDevice,
    cg.Parented.template(spi.SPIComponent),
)
OneWireEEPROM = binary_storage_ns.class_("OneWireEEPROM", BinaryStorage)

# Filesystem storage classes — extend FilesystemStorage
FlashPartition = binary_storage_ns.class_("FlashPartition", FilesystemStorage)
LittleFSMount = binary_storage_ns.class_("LittleFSMount", FilesystemStorage)

# Automation classes
ReadAction = binary_storage_ns.class_("ReadAction", automation.Action)
WriteAction = binary_storage_ns.class_("WriteAction", automation.Action)
FillAction = binary_storage_ns.class_("FillAction", automation.Action)
WriteByteAction = binary_storage_ns.class_("WriteByteAction", automation.Action)
WriteStringAction = binary_storage_ns.class_("WriteStringAction", automation.Action)
IsReadyCondition = binary_storage_ns.class_("IsReadyCondition", automation.Condition)

# Configuration keys
CONF_PAGE_SIZE = "page_size"
CONF_ADDRESSING_BITS = "addressing_bits"
CONF_MOUNT_PATH = "mount_path"


def _validate_mount_path(value):
    # PathStorage contract: must start with '/', must not end with '/', not '' or '/'
    value = cv.string_strict(value)
    if not value.startswith("/") or (len(value) > 1 and value.endswith("/")) or value == "/":
        raise cv.Invalid(
            "mount_path must be absolute (start with '/'), must not end with '/', "
            "and must not be just '/'"
        )
    return value
CONF_AUTO_FORMAT = "auto_format"
CONF_PARTITION_LABEL = "partition_label"
CONF_FILESYSTEM = "filesystem"
CONF_STORAGE_DEVICE = "storage_device"
CONF_ERASE_SIZE = "erase_size"
CONF_JEDEC_ID = "jedec_id"
CONF_QUAD_MODE = "quad_mode"
CONF_MOUNT_ID = "mount_id"
CONF_STORAGE_ID = "storage_id"
CONF_STORAGE_NAME = "storage_name"

# Storage modes (for external devices)
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
        "mb": 1000**2,
        "mib": 1024**2,
    }

    suffix = match.group(2)
    if suffix and suffix not in suffixes:
        raise cv.Invalid(f"Invalid suffix '{suffix}', valid: B, KB, KiB, MB, MiB")

    return int(int(match.group(1)) * suffixes[suffix])


def _add_littlefs_sdkconfig():
    """Set LittleFS sdkconfig options via the IDF build system."""
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_CACHE_SIZE", 512)
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_MAX_PARTITIONS", 3)
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_READ_SIZE", 128)
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_WRITE_SIZE", 128)
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_LOOKAHEAD_SIZE", 128)
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_BLOCK_CYCLES", 512)
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_PAGE_SIZE", 256)


# EEPROM Configuration Schema
EEPROM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(I2CEeprom),
            cv.Optional(CONF_MODEL, default="AT24C256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=8, max=128),
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(8, 9, 10, 11, 16, int=True),
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            cv.Optional(CONF_MOUNT_PATH): _validate_mount_path,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_MOUNT_ID): cv.declare_id(LittleFSMount),
            cv.Optional(CONF_STORAGE_ID): cv.string,
            cv.Optional(CONF_STORAGE_NAME): cv.string,
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
            cv.Optional(CONF_MODEL, default="MB85RC256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(9, 11, 16, 32, int=True),
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            cv.Optional(CONF_MOUNT_PATH): _validate_mount_path,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_MOUNT_ID): cv.declare_id(LittleFSMount),
            cv.Optional(CONF_STORAGE_ID): cv.string,
            cv.Optional(CONF_STORAGE_NAME): cv.string,
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
            cv.Optional(CONF_MODEL, default="W25Q32"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=256, max=256),
            cv.Optional(CONF_ERASE_SIZE): cv.one_of(4096, 32768, 65536, int=True),
            cv.Optional(CONF_JEDEC_ID): cv.hex_uint32_t,
            cv.Optional(CONF_QUAD_MODE, default=False): cv.boolean,
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            cv.Optional(CONF_MOUNT_PATH): _validate_mount_path,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_MOUNT_ID): cv.declare_id(LittleFSMount),
            cv.Optional(CONF_STORAGE_ID): cv.string,
            cv.Optional(CONF_STORAGE_NAME): cv.string,
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
            cv.Optional(CONF_MODEL, default="FM25V10"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(16, 24, int=True),
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            cv.Optional(CONF_MOUNT_PATH): _validate_mount_path,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_MOUNT_ID): cv.declare_id(LittleFSMount),
            cv.Optional(CONF_STORAGE_ID): cv.string,
            cv.Optional(CONF_STORAGE_NAME): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

# SPI MRAM Configuration Schema
SPI_MRAM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIMRAM),
            cv.Optional(CONF_MODEL, default="MR25H256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(16, 24, int=True),
            cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
                MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
            ),
            cv.Optional(CONF_MOUNT_PATH): _validate_mount_path,
            cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
            cv.Optional(CONF_MOUNT_ID): cv.declare_id(LittleFSMount),
            cv.Optional(CONF_STORAGE_ID): cv.string,
            cv.Optional(CONF_STORAGE_NAME): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

# OneWire EEPROM Configuration Schema
ONEWIRE_EEPROM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OneWireEEPROM),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_MODEL, default="DS2431"): cv.string,
        cv.Optional(CONF_CAPACITY): validate_bytes,
        cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=8, max=32),
        cv.Optional(CONF_ADDRESS): cv.hex_uint64_t,
        cv.Optional(CONF_MODE, default=MODE_RAW): cv.one_of(
            MODE_RAW, MODE_LITTLEFS, MODE_BOTH, lower=True
        ),
        cv.Optional(CONF_MOUNT_PATH): _validate_mount_path,
        cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
        cv.Optional(CONF_MOUNT_ID): cv.declare_id(LittleFSMount),
        cv.Optional(CONF_STORAGE_ID): cv.string,
        cv.Optional(CONF_STORAGE_NAME): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

# Flash Partition Configuration Schema
FLASH_PARTITION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FlashPartition),
        cv.Required(CONF_PARTITION_LABEL): cv.string,
        cv.Optional(CONF_MOUNT_PATH, default="/littlefs"): _validate_mount_path,
        cv.Optional(CONF_AUTO_FORMAT, default=True): cv.boolean,
        cv.Optional(CONF_STORAGE_ID): cv.string,
        cv.Optional(CONF_STORAGE_NAME): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

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
        "SPI_MRAM": SPI_MRAM_SCHEMA,
        "MRAM": SPI_MRAM_SCHEMA,
        "ONEWIRE_EEPROM": ONEWIRE_EEPROM_SCHEMA,
        "ONEWIRE": ONEWIRE_EEPROM_SCHEMA,
        "FLASH_PARTITION": FLASH_PARTITION_SCHEMA,
        "PARTITION": FLASH_PARTITION_SCHEMA,
    },
    key=CONF_TYPE,
    upper=True,
)

# Mapping of device types to their source files
DEVICE_SOURCE_FILES = {
    "i2c_fram": ["i2c_fram.cpp"],
    "i2c_eeprom": ["i2c_eeprom.cpp"],
    "spi_flash": ["spi_flash.cpp"],
    "spi_fram": ["spi_fram.cpp"],
    "spi_mram": ["spi_mram.cpp"],
    "onewire_eeprom": ["onewire_eeprom.cpp"],
    "flash_partition": ["flash_partition.cpp"],
}

TYPE_TO_DEVICE = {
    "EEPROM": "i2c_eeprom",
    "I2C_EEPROM": "i2c_eeprom",
    "FRAM": "i2c_fram",
    "I2C_FRAM": "i2c_fram",
    "SPI_FLASH": "spi_flash",
    "FLASH": "spi_flash",
    "SPI_FRAM": "spi_fram",
    "SPI_MRAM": "spi_mram",
    "MRAM": "spi_mram",
    "ONEWIRE_EEPROM": "onewire_eeprom",
    "ONEWIRE": "onewire_eeprom",
    "FLASH_PARTITION": "flash_partition",
    "PARTITION": "flash_partition",
}


def _final_validate(config):
    device_type = config[CONF_TYPE].upper()
    if (internal_type := TYPE_TO_DEVICE.get(device_type)) is not None:
        CORE.data.setdefault("binary_storage_device_types", set()).add(internal_type)
    mode = config.get(CONF_MODE, MODE_RAW)
    needs_littlefs = mode in [MODE_LITTLEFS, MODE_BOTH] or device_type in [
        "FLASH_PARTITION",
        "PARTITION",
    ]
    if needs_littlefs:
        if CORE.using_arduino:
            raise cv.Invalid(
                "LittleFS and flash partition support requires ESP32 with ESP-IDF framework, "
                "not Arduino. Use mode: raw or switch to esp-idf."
            )
        if not CORE.is_esp32:
            raise cv.Invalid(
                "LittleFS and flash partition support is only available on ESP32."
            )
        require_vfs_dir()
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def FILTER_SOURCE_FILES():
    configured = CORE.data.get("binary_storage_device_types", set())
    exclude = []
    for device_type, files in DEVICE_SOURCE_FILES.items():
        if device_type not in configured:
            exclude.extend(files)
    return exclude


async def to_code(config):
    device_type = config[CONF_TYPE].upper()

    if CORE.is_esp32 and not CORE.using_arduino:
        # LittleFS sdkconfig options and library only needed on ESP32-IDF
        _add_littlefs_sdkconfig()
        add_idf_component(
            name="p1ngb4ck/esphome_esp_littlefs",
            repo="https://github.com/p1ngb4ck/esphome_esp_littlefs.git",
            ref="main",
        )

    # Handle FLASH_PARTITION
    if device_type in ["FLASH_PARTITION", "PARTITION"]:
        require_vfs_dir()
        cg.add_define("USE_BINARY_STORAGE_LITTLEFS")
        _add_littlefs_sdkconfig()
        add_idf_component(
            name="p1ngb4ck/esphome_esp_littlefs",
            repo="https://github.com/p1ngb4ck/esphome_esp_littlefs.git",
            ref="main",
        )

        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

        cg.add(var.set_partition_label(config[CONF_PARTITION_LABEL]))
        cg.add(var.set_mount_path(config[CONF_MOUNT_PATH]))
        cg.add(var.set_auto_format(config[CONF_AUTO_FORMAT]))

        storage_id = config.get(CONF_STORAGE_ID, str(config[CONF_ID]))
        storage_name = config.get(CONF_STORAGE_NAME, config[CONF_PARTITION_LABEL])
        cg.add(var.set_storage_id(storage_id))
        cg.add(var.set_storage_name(storage_name))

        request_storage_device()
        # Path-based driver -> async worker. task_safe: esp_littlefs serializes internally
        # and esp_partition flash I/O is task-safe in IDF for every instance of this driver
        # (see FlashPartition::get_capabilities()).
        request_storage_worker(task_safe=True)
        return

    # External memory devices (FRAM, EEPROM, SPI Flash, MRAM, OneWire)
    mode = config.get(CONF_MODE, MODE_RAW)

    # Custom block device support required for external memory LittleFS
    if mode in [MODE_LITTLEFS, MODE_BOTH]:
        add_idf_sdkconfig_option("CONFIG_LITTLEFS_CUSTOM_BLOCK_DEVICE", True)
        require_vfs_dir()
        cg.add_define("USE_BINARY_STORAGE_LITTLEFS")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    is_spi = device_type in ["SPI_FLASH", "FLASH", "SPI_FRAM", "SPI_MRAM", "MRAM"]
    is_onewire = device_type in ["ONEWIRE_EEPROM", "ONEWIRE"]

    if is_spi:
        cg.add_define("USE_BINARY_STORAGE_SPI")
        await spi.register_spi_device(var, config)
        if device_type in ["SPI_FLASH", "FLASH"]:
            cg.add_define("USE_BINARY_STORAGE_SPI_FLASH")
        elif device_type == "SPI_FRAM":
            cg.add_define("USE_BINARY_STORAGE_SPI_FRAM")
        elif device_type in ["SPI_MRAM", "MRAM"]:
            cg.add_define("USE_BINARY_STORAGE_SPI_MRAM")
    elif is_onewire:
        cg.add_define("USE_BINARY_STORAGE_ONEWIRE")
        cg.add_define("USE_BINARY_STORAGE_ONEWIRE_EEPROM")
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
        if (addr := config.get(CONF_ADDRESS)) is not None:
            cg.add(var.set_address(addr))
    else:
        cg.add_define("USE_BINARY_STORAGE_I2C")
        await i2c.register_i2c_device(var, config)
        if device_type in ["FRAM", "I2C_FRAM"]:
            cg.add_define("USE_BINARY_STORAGE_I2C_FRAM")
        elif device_type in ["EEPROM", "I2C_EEPROM"]:
            cg.add_define("USE_BINARY_STORAGE_I2C_EEPROM")

    if (model := config.get(CONF_MODEL)) is not None:
        cg.add(var.set_model(model))
    if (capacity := config.get(CONF_CAPACITY)) is not None:
        cg.add(var.set_capacity(capacity))
    if (page_size := config.get(CONF_PAGE_SIZE)) is not None:
        cg.add(var.set_page_size(page_size))
    if (erase_size := config.get(CONF_ERASE_SIZE)) is not None:
        cg.add(var.set_erase_size(erase_size))
    if (jedec_id := config.get(CONF_JEDEC_ID)) is not None:
        cg.add(var.set_jedec_id(jedec_id))
    if (quad_mode := config.get(CONF_QUAD_MODE)) is not None:
        cg.add(var.set_quad_mode(quad_mode))
    if (addr_bits := config.get(CONF_ADDRESSING_BITS)) is not None:
        cg.add(var.set_addressing_bits(addr_bits))

    storage_id = config.get(CONF_STORAGE_ID, str(config[CONF_ID]))
    storage_name = config.get(CONF_STORAGE_NAME, config.get(CONF_MODEL, device_type))
    cg.add(var.set_storage_id(storage_id))
    cg.add(var.set_storage_name(storage_name))

    # Raw device always registers itself
    request_storage_device()

    # Create LittleFSMount if mode requires filesystem access
    if mode in [MODE_LITTLEFS, MODE_BOTH]:
        mount_path = config.get(CONF_MOUNT_PATH) or f"/{config[CONF_ID]}"

        from esphome.core import ID

        if (mount_id := config.get(CONF_MOUNT_ID)) is not None:
            pass  # user provided a declared id
        else:
            mount_id = ID(
                f"{config[CONF_ID]}_mount", is_declaration=True, type=LittleFSMount
            )
            CORE.component_ids.add(str(mount_id))

        mount_var = cg.new_Pvariable(mount_id)
        await cg.register_component(mount_var, {})

        cg.add(mount_var.set_storage_device(var))
        cg.add(mount_var.set_mount_path(mount_path))
        if (auto_format := config.get(CONF_AUTO_FORMAT)) is not None:
            cg.add(mount_var.set_auto_format(auto_format))

        # LittleFSMount registers as a separate filesystem storage device
        request_storage_device()
        # Path-based driver -> async worker; never task-safe (bus-attached backing device,
        # see LittleFSMount::get_capabilities()).
        request_storage_worker()


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
    synchronous=True,
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
    synchronous=True,
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
    synchronous=True,
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
    synchronous=True,
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
    synchronous=True,
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
