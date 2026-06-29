from dataclasses import dataclass

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@p1ngb4ck"]

DOMAIN = "storage"

storage_ns = cg.esphome_ns.namespace("storage")
StorageRegistry = storage_ns.class_("StorageRegistry", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(StorageRegistry)})


@dataclass
class StorageData:
    device_count: int = 0


def _get_data() -> StorageData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = StorageData()
    return CORE.data[DOMAIN]


def request_storage_device() -> None:
    """Called by each storage driver's to_code() to count configured devices.

    The accumulated count is passed to StorageRegistry.set_device_count() so the
    internal FixedVector is sized exactly — no compile-time upper bound needed.
    """
    _get_data().device_count += 1


async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    device_count = _get_data().device_count
    cg.add(var.set_device_count(device_count))

    cg.add(cg.RawExpression(f"{storage_ns}::global_storage_registry = {var}"))
