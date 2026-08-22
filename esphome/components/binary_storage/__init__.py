from __future__ import annotations

import logging
import re

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c, spi
from esphome.components.const import CONF_LABEL
from esphome.components.storage import (
    request_path_length,
    request_storage_device,
    request_storage_worker,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CAPACITY,
    CONF_CS_PIN,
    CONF_DATA,
    CONF_DATA_RATE,
    CONF_FORMAT,
    CONF_I2C_ID,
    CONF_ID,
    CONF_LENGTH,
    CONF_MODEL,
    CONF_NUMBER,
    CONF_PIN,
    CONF_SIZE,
    CONF_SPI_ID,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE
import esphome.final_validate as fv

CODEOWNERS = ["@p1ngb4ck"]
DEPENDENCIES = []
AUTO_LOAD = ["storage"]
DOMAIN = "binary_storage"
MULTI_CONF = True

# Namespaces
binary_storage_ns = cg.esphome_ns.namespace("binary_storage")
storage_ns = cg.esphome_ns.namespace("storage")

# Base classes (from storage component)
RawStorage = storage_ns.class_("RawStorage", cg.Component)
KeyValueStorage = storage_ns.class_("KeyValueStorage", cg.Component)

# binary_storage device classes -- extend RawStorage
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


# Key-value storage classes -- extend KeyValueStorage
NVSStore = binary_storage_ns.class_("NVSStore", KeyValueStorage)
InplaceKVStore = binary_storage_ns.class_("InplaceKVStore", KeyValueStorage)

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


CONF_AUTO_FORMAT = "auto_format"
CONF_PARTITION_LABEL = "partition_label"
CONF_PARTITION_SIZE = "partition_size"
CONF_FILESYSTEM = "filesystem"
CONF_STORAGE_DEVICE = "storage_device"
CONF_ERASE_SIZE = "erase_size"
CONF_JEDEC_ID = "jedec_id"
CONF_QUAD_MODE = "quad_mode"
CONF_MOUNT_ID = "mount_id"
CONF_PARTITION_ID = "partition_id"
CONF_STORAGE_NAME = "storage_name"
CONF_NAMESPACE = "namespace"
CONF_ASSUME_EXCLUSIVE_BUS = "assume_exclusive_bus"
CONF_DEVICE_NODE = "device_node"
CONF_DEVICE_NODE_NAME = "device_node_name"

# Storage modes (for external devices)
_LOGGER = logging.getLogger(__name__)

CONF_REGIONS = "regions"
FORMAT_RAW = "raw"
FORMAT_KV = "kv"
FORMAT_NVS = "nvs"
SUBTYPE_DATA_NVS = 0x02
SUBTYPE_DATA_UNDEFINED = 0x06
MAX_PARTITION_LABEL = 16
REGION_REMAINING = "remaining"


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


# Opt-in shared by the bus-attached raw devices (SPI + I2C). The user asserts this device is
# alone on its bus and the bus is a real hardware bus, making its data-plane I/O safe to run
# on the async worker task. This is NOT believed blindly: FINAL_VALIDATE enforces the promise
# (no other device on the same bus; hardware bus on esp32) and errors otherwise -- a safety net
# for the unwary. Only the value lives here; the bus itself is never modified, only inspected.
# See .ai/architecture/task-safe-raw-devices.md.
_ASSUME_EXCLUSIVE_BUS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ASSUME_EXCLUSIVE_BUS): cv.All(cv.boolean, cv.only_on_esp32),
    }
)


def region_size(value):
    """A region size in bytes, or the literal "remaining" (at most one per device)."""
    if isinstance(value, str) and value.strip().lower() == REGION_REMAINING:
        return REGION_REMAINING
    return validate_bytes(value)


# A region places one format on a byte window of the device. Regions are laid out sequentially in
# list order; at most one may use size: remaining (= capacity minus the explicit sizes).
_RAW_REGION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RawStorage),
        cv.Optional(CONF_SIZE, default=REGION_REMAINING): region_size,
        cv.Optional(CONF_LABEL): cv.string,
        cv.Optional(CONF_STORAGE_NAME): cv.string,
    }
)
# NVS region: only valid on esp32/esp-idf SPI flash with an exclusive bus (esp_partition mode),
# where it becomes a real NVS partition with wear leveling.
_NVS_REGION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NVSStore),
        cv.Optional(CONF_SIZE, default=REGION_REMAINING): region_size,
        cv.Optional(CONF_LABEL): cv.string,
        cv.Optional(CONF_NAMESPACE, default="binary_storage"): cv.string,
        cv.Optional(CONF_STORAGE_NAME): cv.string,
    }
)
_KV_REGION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(InplaceKVStore),
        cv.Optional(CONF_SIZE, default=REGION_REMAINING): region_size,
        cv.Optional(CONF_STORAGE_NAME): cv.string,
    }
)
REGION_SCHEMA = cv.typed_schema(
    {
        FORMAT_RAW: _RAW_REGION_SCHEMA,
        FORMAT_KV: _KV_REGION_SCHEMA,
        FORMAT_NVS: _NVS_REGION_SCHEMA,
    },
    key=CONF_FORMAT,
    lower=True,
)


# EEPROM Configuration Schema
EEPROM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(I2CEeprom),
            cv.Optional(CONF_MODEL, default="AT24C256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_PAGE_SIZE): cv.int_range(min=8, max=128),
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(8, 9, 10, 11, 16, int=True),
            cv.Optional(CONF_REGIONS): cv.ensure_list(REGION_SCHEMA),
            cv.Optional(CONF_STORAGE_NAME): cv.string,
            # Whether this device gets its own node in the file browser. Defaults to on when a
            # browser is configured at all.
            cv.Optional(CONF_DEVICE_NODE): cv.boolean,
            # What that node is called. Neither the YAML id (references in lambdas/actions) nor
            # the entity name (Home Assistant / web server): this names the node and nothing
            # else. Defaults to the device type, which is unambiguous until a second device of
            # that type shows up -- then it is required.
            cv.Optional(CONF_DEVICE_NODE_NAME): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x50))
    .extend(_ASSUME_EXCLUSIVE_BUS_SCHEMA)
)

# FRAM Configuration Schema
FRAM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(I2CFram),
            cv.Optional(CONF_MODEL, default="MB85RC256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(9, 11, 16, 32, int=True),
            cv.Optional(CONF_REGIONS): cv.ensure_list(REGION_SCHEMA),
            cv.Optional(CONF_STORAGE_NAME): cv.string,
            # Whether this device gets its own node in the file browser. Defaults to on when a
            # browser is configured at all.
            cv.Optional(CONF_DEVICE_NODE): cv.boolean,
            # What that node is called. Neither the YAML id (references in lambdas/actions) nor
            # the entity name (Home Assistant / web server): this names the node and nothing
            # else. Defaults to the device type, which is unambiguous until a second device of
            # that type shows up -- then it is required.
            cv.Optional(CONF_DEVICE_NODE_NAME): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x50))
    .extend(_ASSUME_EXCLUSIVE_BUS_SCHEMA)
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
            cv.Optional(CONF_REGIONS): cv.ensure_list(REGION_SCHEMA),
            cv.Optional(CONF_STORAGE_NAME): cv.string,
            # Whether this device gets its own node in the file browser. Defaults to on when a
            # browser is configured at all.
            cv.Optional(CONF_DEVICE_NODE): cv.boolean,
            # What that node is called. Neither the YAML id (references in lambdas/actions) nor
            # the entity name (Home Assistant / web server): this names the node and nothing
            # else. Defaults to the device type, which is unambiguous until a second device of
            # that type shows up -- then it is required.
            cv.Optional(CONF_DEVICE_NODE_NAME): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
    .extend(_ASSUME_EXCLUSIVE_BUS_SCHEMA)
)

# SPI FRAM Configuration Schema
SPI_FRAM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIFram),
            cv.Optional(CONF_MODEL, default="FM25V10"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(16, 24, int=True),
            cv.Optional(CONF_REGIONS): cv.ensure_list(REGION_SCHEMA),
            cv.Optional(CONF_STORAGE_NAME): cv.string,
            # Whether this device gets its own node in the file browser. Defaults to on when a
            # browser is configured at all.
            cv.Optional(CONF_DEVICE_NODE): cv.boolean,
            # What that node is called. Neither the YAML id (references in lambdas/actions) nor
            # the entity name (Home Assistant / web server): this names the node and nothing
            # else. Defaults to the device type, which is unambiguous until a second device of
            # that type shows up -- then it is required.
            cv.Optional(CONF_DEVICE_NODE_NAME): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
    .extend(_ASSUME_EXCLUSIVE_BUS_SCHEMA)
)

# SPI MRAM Configuration Schema
SPI_MRAM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIMRAM),
            cv.Optional(CONF_MODEL, default="MR25H256"): cv.string,
            cv.Optional(CONF_CAPACITY): validate_bytes,
            cv.Optional(CONF_ADDRESSING_BITS): cv.one_of(16, 24, int=True),
            cv.Optional(CONF_REGIONS): cv.ensure_list(REGION_SCHEMA),
            cv.Optional(CONF_STORAGE_NAME): cv.string,
            # Whether this device gets its own node in the file browser. Defaults to on when a
            # browser is configured at all.
            cv.Optional(CONF_DEVICE_NODE): cv.boolean,
            # What that node is called. Neither the YAML id (references in lambdas/actions) nor
            # the entity name (Home Assistant / web server): this names the node and nothing
            # else. Defaults to the device type, which is unambiguous until a second device of
            # that type shows up -- then it is required.
            cv.Optional(CONF_DEVICE_NODE_NAME): cv.string_strict,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
    .extend(_ASSUME_EXCLUSIVE_BUS_SCHEMA)
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
        cv.Optional(CONF_REGIONS): cv.ensure_list(REGION_SCHEMA),
        cv.Optional(CONF_STORAGE_NAME): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


# A dedicated NVS partition exposed as a KeyValueStorage. esp32 only (NVS is an ESP-IDF facility).
# It uses its OWN partition label, never the system "nvs" partition, so it cannot collide with the
# preferences store.
NVS_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NVSStore),
            cv.Required(CONF_PARTITION_LABEL): cv.string,
            # NVS needs at least three 4KB sectors. 4KB aligned; casing per validate_bytes.
            cv.Optional(CONF_PARTITION_SIZE, default="64kB"): cv.All(
                cv.validate_bytes, cv.Range(min=0x3000)
            ),
            # NVS namespace (max 15 chars). Defaults to a component-specific namespace so it is
            # isolated from anything else on the same partition.
            cv.Optional(CONF_NAMESPACE, default="binary_storage"): cv.All(
                cv.string, cv.Length(max=15)
            ),
            cv.Optional(CONF_STORAGE_NAME): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
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
        "SPI_MRAM": SPI_MRAM_SCHEMA,
        "MRAM": SPI_MRAM_SCHEMA,
        "ONEWIRE_EEPROM": ONEWIRE_EEPROM_SCHEMA,
        "ONEWIRE": ONEWIRE_EEPROM_SCHEMA,
        "NVS": NVS_SCHEMA,
    },
    key=CONF_TYPE,
    upper=True,
)

# Mapping of device types to their source files, keyed by the type: value above.
DEVICE_SOURCE_FILES = {
    "i2c_fram": ["i2c_fram.cpp"],
    "i2c_eeprom": ["i2c_eeprom.cpp"],
    "spi_flash": ["spi_flash.cpp"],
    "spi_fram": ["spi_fram.cpp"],
    "spi_mram": ["spi_mram.cpp"],
    "onewire_eeprom": ["onewire_eeprom.cpp"],
    "nvs": ["nvs_store.cpp"],
    "inplace_kv": ["inplace_kv.cpp"],
}

# Raw media only (kv/nvs partition consumers are mount points/stores, not raw nodes).
RAW_DEVICE_TYPES = {
    "i2c_eeprom",
    "i2c_fram",
    "spi_flash",
    "spi_fram",
    "spi_mram",
    "onewire_eeprom",
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
    "NVS": "nvs",
}


def _browser_configured() -> bool:
    """True when a web_server in this config has the file browser turned on."""
    web_server = fv.full_config.get().get("web_server")
    if isinstance(web_server, list):
        return any("file_browser" in ws for ws in web_server)
    return isinstance(web_server, dict) and "file_browser" in web_server


def _node_name_of(device: dict) -> str | None:
    """Effective node name of a raw device, or None when it has no node."""
    internal = TYPE_TO_DEVICE.get(device[CONF_TYPE].upper())
    if internal not in RAW_DEVICE_TYPES:
        return None
    regions = device.get(CONF_REGIONS) or [{CONF_FORMAT: FORMAT_RAW}]
    if not any(r[CONF_FORMAT] == FORMAT_RAW for r in regions):
        return None  # no raw side -- nothing a node could address
    if not device.get(CONF_DEVICE_NODE, _browser_configured()):
        return None
    return device.get(CONF_DEVICE_NODE_NAME) or internal


def _esp_partition_active(config):
    """True when a SPI flash uses the esp32/ESP-IDF esp_partition path: regions become real
    esp_partitions driven by esp_flash on an exclusive bus. Purely internal -- derived from the
    exclusive-bus opt-in, no separate config field."""
    return (
        CORE.is_esp32
        and not CORE.using_arduino
        and config[CONF_TYPE].upper() in ("SPI_FLASH", "FLASH")
        and bool(config.get(CONF_ASSUME_EXCLUSIVE_BUS))
    )


def _region_id(region):
    return region.get(CONF_ID) or region.get(CONF_MOUNT_ID)


def _derive_partition_label(region):
    rid = _region_id(region)
    return str(rid) if rid is not None else None


def _validate_regions(config):
    """Lay out and validate the regions on a byte device. Regions are placed sequentially in list
    order; at most one may use size: remaining (= capacity minus the explicit sizes). The resolved
    offset and size are stored back on each region for to_code()."""
    regions = config.get(CONF_REGIONS)
    if not regions:
        return config  # a bare device is whole-device raw (see to_code)
    internal_type = TYPE_TO_DEVICE.get(config[CONF_TYPE].upper())
    capacity = config.get(CONF_CAPACITY)
    if capacity is None:
        raise cv.Invalid(f"'{CONF_REGIONS}' requires an explicit '{CONF_CAPACITY}'")
    remaining = [r for r in regions if r[CONF_SIZE] == REGION_REMAINING]
    if len(remaining) > 1:
        raise cv.Invalid(
            f"at most one region may use '{CONF_SIZE}: {REGION_REMAINING}'"
        )
    explicit = sum(r[CONF_SIZE] for r in regions if r[CONF_SIZE] != REGION_REMAINING)
    if explicit > capacity:
        raise cv.Invalid(
            f"region sizes ({explicit}) exceed the device capacity ({capacity})"
        )
    rem_size = capacity - explicit
    if remaining and rem_size <= 0:
        raise cv.Invalid(
            f"'{CONF_SIZE}: {REGION_REMAINING}' leaves no room after the explicit sizes"
        )
    erase = config.get(CONF_ERASE_SIZE)
    esp_part = _esp_partition_active(config)
    sector = erase or 4096
    seen_labels = set()
    offset = 0
    for region in regions:
        fmt = region[CONF_FORMAT]
        size = rem_size if region[CONF_SIZE] == REGION_REMAINING else region[CONF_SIZE]
        if fmt == FORMAT_NVS and not esp_part:
            raise cv.Invalid(
                "format: nvs needs esp32 with ESP-IDF, a SPI flash device, and an exclusive bus "
                "(assume_exclusive_bus: true)."
            )
        if esp_part:
            if offset % sector != 0 or size % sector != 0:
                raise cv.Invalid(
                    f"in esp_partition mode every region must be aligned to the erase sector "
                    f"({sector} bytes); the region at offset {offset} of size {size} is not"
                )
            label = region.get(CONF_LABEL) or _derive_partition_label(region)
            if label is None:
                raise cv.Invalid(
                    "this region needs an explicit 'label' (none could be derived)"
                )
            if len(label) > MAX_PARTITION_LABEL:
                raise cv.Invalid(
                    f"partition label '{label}' exceeds {MAX_PARTITION_LABEL} characters; set a "
                    f"shorter explicit 'label'"
                )
            if label in seen_labels:
                raise cv.Invalid(
                    f"duplicate partition label '{label}'; set an explicit 'label'"
                )
            seen_labels.add(label)
            region["_label"] = label
        if fmt == FORMAT_KV:
            if internal_type not in ("i2c_fram", "spi_fram", "spi_mram"):
                raise cv.Invalid(
                    "format: kv (in-place key-value store) is only supported on erase-free byte "
                    "devices (FRAM/MRAM)."
                )
            if size < 256:
                raise cv.Invalid(
                    f"format: kv region must be at least 256 bytes (got {size})"
                )
        region[CONF_SIZE] = size  # resolve remaining -> concrete size
        region["_offset"] = offset
        offset += size
    return config


def _validate_device_node(config):
    """The node name defaults to the device type -- fine for one FRAM, ambiguous for two.

    Two nodes called 'spi_flash' would be indistinguishable in the browser and would address
    each other's device, so the second one has to say who it is."""
    if config.get(CONF_DEVICE_NODE_NAME) and not _browser_configured():
        raise cv.Invalid(
            f"'{CONF_DEVICE_NODE_NAME}' needs a web_server with 'file_browser:' -- there is "
            f"nowhere to show the node otherwise"
        )
    if config.get(CONF_DEVICE_NODE) and not _browser_configured():
        raise cv.Invalid(
            f"'{CONF_DEVICE_NODE}' needs a web_server with 'file_browser:' -- there is nowhere "
            f"to show the node otherwise"
        )
    regions = config.get(CONF_REGIONS) or [{CONF_FORMAT: FORMAT_RAW}]
    if config.get(CONF_DEVICE_NODE) and not any(
        r[CONF_FORMAT] == FORMAT_RAW for r in regions
    ):
        raise cv.Invalid(
            f"'{CONF_DEVICE_NODE}' needs a raw region -- a device with no raw format has no raw "
            f"side to hang a node on"
        )

    seen: dict[str, int] = {}
    for device in fv.full_config.get().get(DOMAIN, []):
        name = _node_name_of(device)
        if name is not None:
            seen[name] = seen.get(name, 0) + 1
    duplicates = sorted(name for name, count in seen.items() if count > 1)
    if duplicates:
        raise cv.Invalid(
            f"More than one device node is called {duplicates}. Give each one a "
            f"'{CONF_DEVICE_NODE_NAME}' -- it is what the file browser shows and addresses them by."
        )
    return config


def _count_devices_on_bus(fconf, bus_key, bus_id):
    """Count how many component configs across the whole config reference the same bus id.

    Walks every domain's component list and looks for the bus-id key (spi_id / i2c_id). The
    bus is only READ here -- never modified. Returns (count, names) where names lists the ids of
    the other devices for a helpful error.
    """
    count = 0
    names = []
    root = fconf.get_config_for_path([])
    for domain_conf in root.values():
        entries = domain_conf if isinstance(domain_conf, list) else [domain_conf]
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            ref = entry.get(bus_key)
            if ref is not None and str(ref) == str(bus_id):
                count += 1
                if (other_id := entry.get(CONF_ID)) is not None:
                    names.append(str(other_id))
    return count, names


def _validate_assume_exclusive_bus(config, fconf):
    """Enforce the assume_exclusive_bus promise (see the opt-in schema and .ai/).

    The user asserts the device is alone on a real hardware bus. We do not believe it blindly:
    (A) no other device may reference the same bus, and (B) the bus must be a hardware bus on
    esp32. Either failing is an error. The bus config is only inspected, never changed.
    """
    if not config.get(CONF_ASSUME_EXCLUSIVE_BUS):
        return

    device_type = config[CONF_TYPE].upper()
    is_spi = device_type in ["SPI_FLASH", "FLASH", "SPI_FRAM", "SPI_MRAM", "MRAM"]
    bus_key = CONF_SPI_ID if is_spi else CONF_I2C_ID
    bus_id = config.get(bus_key)
    if bus_id is None:
        # Should not happen for a bus device, but fail loudly rather than silently skip.
        raise cv.Invalid(
            f"'{CONF_ASSUME_EXCLUSIVE_BUS}' is only valid on an SPI or I2C device"
        )

    # --- Check A: this device must be alone on its bus ---
    count, others = _count_devices_on_bus(fconf, bus_key, bus_id)
    if count > 1:
        shared_with = ", ".join(n for n in others if n != str(config.get(CONF_ID))) or (
            f"{count - 1} other device(s)"
        )
        raise cv.Invalid(
            f"'{CONF_ASSUME_EXCLUSIVE_BUS}: true' requires this device to be the only thing on "
            f"bus '{bus_id}', but it is shared with: {shared_with}. A background task driving a "
            f"shared bus would corrupt the other devices' traffic. Give this device its own bus, "
            f"or remove '{CONF_ASSUME_EXCLUSIVE_BUS}'."
        )

    # --- Check B: the bus must be a real hardware bus on esp32 ---
    if not CORE.is_esp32:
        raise cv.Invalid(
            f"'{CONF_ASSUME_EXCLUSIVE_BUS}' needs a hardware bus on ESP32 (the async worker "
            f"task only exists there)."
        )
    if is_spi:
        # A software (bit-banged) SPI bus is driven from the main loop and is not task-safe.
        # The validated bus config carries 'interface' == 'software' for software SPI, and a
        # resolved 'interface_index' for a hardware one.
        bus_path = fconf.get_path_for_id(bus_id)[:-1]
        bus_conf = fconf.get_config_for_path(bus_path)
        if bus_conf.get("interface") == "software" or "interface_index" not in bus_conf:
            raise cv.Invalid(
                f"'{CONF_ASSUME_EXCLUSIVE_BUS}: true' on '{config.get(CONF_ID)}' needs a "
                f"hardware SPI bus, but bus '{bus_id}' is software (bit-banged) -- that is driven "
                f"from the main loop and cannot be task-safe. Use a hardware SPI interface."
            )
    # I2C on esp32 is always a hardware bus (IDFI2CBus); no software-I2C path exists, so
    # CORE.is_esp32 is sufficient for the I2C case.


def _final_validate(config):
    _validate_regions(config)
    _validate_device_node(config)
    _validate_assume_exclusive_bus(config, fv.full_config.get())
    # Resolved here because it depends on another component's config; stored back for to_code().
    if (node_name := _node_name_of(config)) is not None:
        config[CONF_DEVICE_NODE_NAME] = node_name
    device_type = config[CONF_TYPE].upper()
    if (internal_type := TYPE_TO_DEVICE.get(device_type)) is not None:
        CORE.data.setdefault("binary_storage_device_types", set()).add(internal_type)
    regions = config.get(CONF_REGIONS) or []
    if any(r[CONF_FORMAT] == FORMAT_KV for r in regions):
        CORE.data.setdefault("binary_storage_device_types", set()).add("inplace_kv")
    if _esp_partition_active(config):
        # Resolve which hardware SPI host the (exclusive) bus is, for esp_flash. The bus was already
        # validated as hardware SPI on esp32 by _validate_assume_exclusive_bus.
        bus_id = config.get(CONF_SPI_ID)
        fconf = fv.full_config.get()
        bus_conf = fconf.get_config_for_path(fconf.get_path_for_id(bus_id)[:-1])
        config["_esp_flash_host_index"] = bus_conf.get("interface_index", 0)
        # nvs regions still pull in their partition consumer.
        if any(r[CONF_FORMAT] == FORMAT_NVS for r in regions):
            CORE.data.setdefault("binary_storage_device_types", set()).add("nvs")
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

    if device_type == "NVS":
        cg.add_define("USE_BINARY_STORAGE_NVS")
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

        # Append a dedicated NVS data partition so the label exists at runtime. This is the store's
        # own space -- never the system "nvs" partition the preferences use.
        from esphome.components.esp32 import add_partition

        size = config[CONF_PARTITION_SIZE]
        if size % 0x1000 != 0:
            size = (size + 0xFFF) & ~0xFFF
        try:
            add_partition(config[CONF_PARTITION_LABEL], "data", "nvs", size)
        except ValueError as err:
            raise cv.Invalid(str(err)) from err

        cg.add(var.set_partition_label(config[CONF_PARTITION_LABEL]))
        cg.add(var.set_namespace(config[CONF_NAMESPACE]))

        storage_id = str(config[CONF_ID])
        storage_name = config.get(CONF_STORAGE_NAME, config[CONF_PARTITION_LABEL])
        cg.add(var.set_storage_id(storage_id))
        cg.add(var.set_storage_name(storage_name))

        # Key-value device -> registers with the runtime registry. No mount path (not path-based)
        # and no worker (NVS access is fast and synchronous).
        request_storage_device()
        return

    # External memory devices (FRAM, EEPROM, SPI Flash, MRAM, OneWire)
    regions = config.get(CONF_REGIONS) or [
        {CONF_FORMAT: FORMAT_RAW, "_offset": 0, CONF_SIZE: 0}
    ]

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    is_spi = device_type in ["SPI_FLASH", "FLASH", "SPI_FRAM", "SPI_MRAM", "MRAM"]
    is_onewire = device_type in ["ONEWIRE_EEPROM", "ONEWIRE"]

    if is_spi:
        cg.add_define("USE_BINARY_STORAGE_SPI")
        if _esp_partition_active(config):
            # esp_partition mode: esp_flash owns the exclusive bus, so the flash is NOT registered
            # as an ESPHome spi_device (esp_flash and spi_device cannot share a bus).
            _LOGGER.warning(
                "binary_storage '%s': esp_partition mode is experimental (testing stage). A wiring "
                "or hardware fault on the flash can crash or bootloop the device at runtime.",
                config[CONF_ID],
            )
            host = spi.get_spi_interface(config["_esp_flash_host_index"])
            cs_num = config[CONF_CS_PIN][CONF_NUMBER]
            freq_mhz = int(config.get(CONF_DATA_RATE, 40_000_000) // 1_000_000) or 40
            cg.add(
                var.enable_esp_partition_mode(cg.RawExpression(host), cs_num, freq_mhz)
            )
        else:
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

    # The device's identity in the registry is its YAML id -- nothing else to choose.
    storage_id = str(config[CONF_ID])
    storage_name = config.get(CONF_STORAGE_NAME, config.get(CONF_MODEL, device_type))
    cg.add(var.set_storage_id(storage_id))
    cg.add(var.set_storage_name(storage_name))
    if (node_name := config.get(CONF_DEVICE_NODE_NAME)) is not None:
        # The whole device-node notion only exists when something shows it (see storage.h).
        cg.add_define("USE_STORAGE_DEVICE_NODES")
        cg.add(var.set_device_node_name(node_name))

    request_path_length(256)

    # assume_exclusive_bus: FINAL_VALIDATE already enforced the promise (alone on a hardware
    # bus on esp32). Tell the driver it may advertise task-safe I/O, and request the worker
    # with task_safe=True so the background task is actually created for it.
    if config.get(CONF_ASSUME_EXCLUSIVE_BUS):
        cg.add(var.set_assume_exclusive_bus(True))
        request_storage_worker(task_safe=True)

    # Lay out the regions on the device. Offsets/sizes were resolved in FINAL_VALIDATE
    # (_validate_regions); size 0 here means "to the end of the device" (bare whole-device raw).
    has_raw = any(r[CONF_FORMAT] == FORMAT_RAW for r in regions)
    if not has_raw:
        # No raw region: the device is a backing only, it does not register as raw storage.
        cg.add(var.set_raw_enabled(False))

    esp_part = _esp_partition_active(config)

    for region in regions:
        fmt = region[CONF_FORMAT]
        offset = region.get("_offset", 0)
        size = region.get(CONF_SIZE, 0)
        size = 0 if size == REGION_REMAINING else size

        if esp_part:
            # Register the region as a real esp_partition; consumers use it by label.
            label = region["_label"]
            if fmt == FORMAT_RAW:
                cg.add(
                    var.add_partition_region(
                        offset, size, label, SUBTYPE_DATA_UNDEFINED
                    )
                )
                cg.add(var.set_raw_window(offset, size))
                if (rname := region.get(CONF_STORAGE_NAME)) is not None:
                    cg.add(var.set_storage_name(rname))
                request_storage_device()
            elif fmt == FORMAT_NVS:
                cg.add_define("USE_BINARY_STORAGE_NVS")
                cg.add(var.add_partition_region(offset, size, label, SUBTYPE_DATA_NVS))
                nvs_var = cg.new_Pvariable(region[CONF_ID])
                await cg.register_component(nvs_var, {})
                cg.add(nvs_var.set_partition_label(label))
                cg.add(nvs_var.set_namespace(region[CONF_NAMESPACE]))
                request_storage_device()
            continue

        if fmt == FORMAT_RAW:
            # The device itself is the raw storage for this window (addressed by the device id).
            cg.add(var.set_raw_window(offset, size))
            if (rname := region.get(CONF_STORAGE_NAME)) is not None:
                cg.add(var.set_storage_name(rname))
            request_storage_device()

        elif fmt == FORMAT_KV:
            cg.add_define("USE_BINARY_STORAGE_INPLACE_KV")
            kv_var = cg.new_Pvariable(region[CONF_ID])
            await cg.register_component(kv_var, {})
            cg.add(kv_var.set_device(var))
            cg.add(kv_var.set_window(offset, size))
            cg.add(kv_var.set_storage_id(str(region[CONF_ID])))
            cg.add(
                kv_var.set_storage_name(
                    region.get(CONF_STORAGE_NAME) or str(region[CONF_ID])
                )
            )
            request_storage_device()


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
