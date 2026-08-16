"""Shared support for writing what a display is showing out to an image file.

The component itself has no configuration. It provides the ``snapshot.take`` action and the C++
base class behind it, so any display that can hand over its pixels - the in memory display in this
component, or an SDL window - saves files the same way, under the same directory, with the same
rules about names.
"""

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@clydebarrow"]

CONF_FILENAME = "filename"

snapshot_ns = cg.esphome_ns.namespace("snapshot")
Snapshot = snapshot_ns.class_("Snapshot")
SnapshotAction = snapshot_ns.class_("SnapshotAction", automation.Action)


@automation.register_action(
    "snapshot.take",
    SnapshotAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(Snapshot),
            cv.Optional(CONF_FILENAME): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def snapshot_take_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if (filename := config.get(CONF_FILENAME)) is not None:
        cg.add(var.set_filename(await cg.templatable(filename, args, cg.std_string)))
    return var


async def register_snapshot(var: MockObj, config: ConfigType) -> None:
    """Set up a component so that the snapshot action can write its picture to a file."""
    cg.add_define(
        "ESPHOME_SNAPSHOT_DIR",
        (CORE.data_dir / "snapshots" / CORE.name).as_posix(),
    )
    cg.add(var.set_snapshot_prefix(str(config[CONF_ID])))
