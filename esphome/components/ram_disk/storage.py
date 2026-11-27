import esphome.codegen as cg
from esphome.components import storage
from esphome.components.storage import CONF_BLOCK_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_SIZE
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]

ram_disk_ns = cg.esphome_ns.namespace("ram_disk")
RamDisk = ram_disk_ns.class_("RamDisk", storage.BlockDevice, cg.Component)


def _validate_size(config: ConfigType) -> ConfigType:
    if config[CONF_SIZE] % config[CONF_BLOCK_SIZE] != 0:
        raise cv.Invalid(
            f"'{CONF_SIZE}' must be a multiple of '{CONF_BLOCK_SIZE}' ({config[CONF_BLOCK_SIZE]})"
        )
    return config


CONFIG_SCHEMA = cv.All(
    storage.block_device_schema(RamDisk).extend(
        {
            cv.Required(CONF_SIZE): cv.All(
                storage.storage_size, cv.int_range(max=64 * 1024 * 1024)
            ),
            cv.Optional(CONF_BLOCK_SIZE, default=512): storage.block_size,
        }
    ),
    _validate_size,
)


async def to_code(config: ConfigType) -> None:
    block_size = config[CONF_BLOCK_SIZE]
    await storage.new_block_device(config, block_size, config[CONF_SIZE] // block_size)
