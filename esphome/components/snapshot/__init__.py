"""Shared support for writing what a display is showing out to an image file.

The component itself has no configuration. It provides the ``snapshot.take`` action and the C++
base class behind it, so any display that can hand over its pixels - the in memory display in this
component, or an SDL window - saves files the same way, under the same directory, with the same
rules about names.
"""

from dataclasses import dataclass

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]

DOMAIN = "snapshot"

CONF_FILENAME = "filename"

snapshot_ns = cg.esphome_ns.namespace("snapshot")
Snapshot = snapshot_ns.class_("Snapshot")


automation.register_apply_action(
    "snapshot.take",
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(Snapshot),
            cv.Optional(CONF_FILENAME, default=""): cv.templatable(cv.string),
        }
    ),
    automation.ApplyField(CONF_FILENAME, "take_snapshot_or_log", cg.std_string),
)


@dataclass
class SnapshotData:
    directory_defined: bool = False


def _get_data() -> SnapshotData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SnapshotData()
    return CORE.data[DOMAIN]


async def register_snapshot(var: MockObj, config: ConfigType) -> None:
    """Set up a component so that the snapshot action can write its picture to a file."""
    data = _get_data()
    # Only once, however many displays there are: two defines that say the same thing do not
    # compare equal, so asking for this per display repeats the line in defines.h.
    if not data.directory_defined:
        data.directory_defined = True
        cg.add_define(
            "ESPHOME_SNAPSHOT_DIR",
            (CORE.data_dir / "snapshots" / CORE.name).as_posix(),
        )
    cg.add(var.set_snapshot_prefix(str(config[CONF_ID])))
